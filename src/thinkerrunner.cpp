//
// thinkerrunner.cpp
// This file is part of Thinker-Qt
// Copyright (C) 2010-2014 HostileFork.com
//
// Thinker-Qt is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Thinker-Qt is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Thinker-Qt.  If not, see <http://www.gnu.org/licenses/>.
//
// See http://hostilefork.com/thinker-qt/ for more information on this project
//

#include <QMutexLocker>
#include <QDebug>

#include "thinkerqt/thinkerrunner.h"
#include "thinkerqt/thinkermanager.h"


// operator<< for ThinkerRunner::State
//
// Required by tracked<T> because it must be able to present a proper debug
// message about the tracked value.

inline QTextStream & operator<< (
    QTextStream & o,
    ThinkerRunner::State const & state
) {
    o << "ThinkerRunner::";
    switch (state) {
    case ThinkerRunner::RunnerQueued:
        o << "RunnerQueued";
        break;
    case ThinkerRunner::RunnerQueuedButPaused:
        o << "RunnerQueuedButPaused";
        break;
    case ThinkerRunner::RunnerThreadPush:
        o << "RunnerThreadPush";
        break;
    case ThinkerRunner::RunnerThinking:
        o << "RunnerThinking";
        break;
    case ThinkerRunner::RunnerPausing:
        o << "RunnerPausing";
        break;
    case ThinkerRunner::RunnerPaused:
        o << "RunnerPaused";
        break;
    case ThinkerRunner::RunnerResuming:
        o << "RunnerResuming";
        break;
    case ThinkerRunner::RunnerFinished:
        o << "RunnerFinished";
        break;
    case ThinkerRunner::RunnerCanceling:
        o << "RunnerCanceling";
        break;
    case ThinkerRunner::RunnerCanceled:
        o << "RunnerCanceled";
        break;
    default:
        hopefullyNotReached(HERE);
    }
    return o;
}



//
// ThinkerRunnerHelper
//

ThinkerRunnerHelper::ThinkerRunnerHelper (ThinkerRunner & runner) :
    QObject (), // affinity from current thread, no parent
    _runner (runner)
{
    _runner.getManager().hopefullyCurrentThreadIsNotManager(HERE);
}


void ThinkerRunnerHelper::queuedQuit () {
    hopefullyCurrentThreadIsRun(HERE);
    _runner.quit();
}


void ThinkerRunnerHelper::markFinished () {
    hopefullyCurrentThreadIsRun(HERE);

    QMutexLocker lock (&_runner._stateMutex);

    if (_runner._state == ThinkerRunner::RunnerCanceling) {
        // we don't let it transition to finished if abort is requested
    } else {
        _runner._state.hopefullyInSet(
            ThinkerRunner::RunnerThinking, ThinkerRunner::RunnerPausing,
            HERE
        );
        _runner._state.hopefullyAlter(ThinkerRunner::RunnerFinished, HERE);
        _runner._stateWasChanged.wakeOne();
        _runner.quit();
    }
}


ThinkerRunnerHelper::~ThinkerRunnerHelper () {
    hopefullyCurrentThreadIsRun(HERE);
}



//
// ThinkerRunner
//

ThinkerRunner::ThinkerRunner (shared_ptr<ThinkerBase> holder) :
    QEventLoop (),
    _state (RunnerQueued, HERE),
    _holder (holder),
    _helper ()
{
    hopefully(_holder != nullptr, HERE);

    // need to check this, because we will later ask the manager to move the
    // Thinker to the thread of the QRunnable (when we find out what that
    // thread is; we don't know until the Thread Pool decides to run this).
    //
    // Must ask the manager thread because we can only push from a thread,
    // not pull onto an arbitrary one...
    
    hopefullyCurrentThreadIsManager(HERE);
    hopefully(getThinker().thread() == QThread::currentThread(), HERE);

    // Formerly this code would use a queued connection to get a paused thinker
    // whose run loop had been taken off the stack to restart.  When the
    // approach changed to using pooled threads rather than giving each
    // thinker its own, they wait by pausing in the run loop and getting
    // an event signaled.  However if queued connections were needed to speak
    // with the thinker for some reason this would be the place to do:
    //
    //    connect(
    //        this, SIGNAL(...),
    //        &getThinker(), SLOT(...),
    //        Qt::QueuedConnection
    //    );
    //
    // That would also maintain Qt4 compatibility using string-based connect
}


bool ThinkerRunner::hopefullyCurrentThreadIsNotThinker (
    codeplace const & cp
) const
{
    return getManager().hopefullyCurrentThreadIsNotThinker(cp);
}


bool ThinkerRunner::hopefullyCurrentThreadIsManager (
    codeplace const & cp
)
    const
{
    return getManager().hopefullyCurrentThreadIsManager(cp);
}



bool ThinkerRunner::hopefullyCurrentThreadIsRun (
    codeplace const & cp
)
    const
{
    hopefully(_helper, cp);
    return _helper->hopefullyCurrentThreadIsRun(cp);
}


ThinkerManager & ThinkerRunner::getManager () const
{
    return getThinker().getManager();
}


ThinkerBase const & ThinkerRunner::getThinker () const
{
    return *_holder;
}


ThinkerBase & ThinkerRunner::getThinker ()
{
    return *_holder;
}


void ThinkerRunner::doThreadPushIfNecessary ()
{
    hopefullyCurrentThreadIsManager(HERE);

    QMutexLocker lock (&_stateMutex);

    if (_state == RunnerThreadPush) {
        hopefully(_helper, HERE);
        getThinker().moveToThread(_helper->thread());
        _state.hopefullyAlter(RunnerThinking, HERE);
        _stateWasChanged.wakeOne();
    }
}


bool ThinkerRunner::runThinker ()
{
    _stateMutex.lock();

    if (_state == RunnerQueuedButPaused) {
        _stateWasChanged.wait(&_stateMutex);
    }
    _state.hopefullyInSet(RunnerQueued, RunnerCanceled, HERE);

    if (_state == RunnerQueued) {
        // Create from within the thread's run() in order to make sure that
        // our helper object has the thread affinity of the new executing
        // thread, not of the QThread object that spawned the execution
        _helper = QSharedPointer<ThinkerRunnerHelper> (
            new ThinkerRunnerHelper(*this)
        );

        // Maintain Qt4 compatibility using string-based connect
        connect(
            this, SIGNAL(breakEventLoop()),
            _helper.data(), SLOT(queuedQuit()),
            Qt::QueuedConnection
        );
        connect(
            &getThinker(), SIGNAL(done()),
            _helper.data(), SLOT(markFinished()),
            Qt::DirectConnection
        );

        QThread * originalThinkerThread = getThinker().thread();
        // Now that we know what thread the Thinker will be running on, we ask
        // the main thread to push it onto our current thread allocated to us
        // by the pool
        _state.hopefullyAlter(RunnerThreadPush, HERE);
        _stateWasChanged.wakeOne();
        _stateMutex.unlock();

        getManager().waitForPushToThread(this);

        // There are two places where the object will be pushed.  One is from
        // the event loop if the signal happens.  But if before that can happen
        // any of our code gets called from the manager thread then we'll
        // preempt that.
        _stateMutex.lock();
        _state.hopefullyInSet(RunnerThinking, RunnerCanceling, HERE);
        bool didCancelOrFinish = (_state == RunnerCanceling);
        _stateMutex.unlock();

        hopefully(getThinker().thread() == QThread::currentThread(), HERE);

        // TODO: will it be possible to use resumable coroutines so that an
        // idle thinker which is waiting for a message could delegate some time
        // to another thinker?
        getThinker().afterThreadAttach();

        // The thinker thread needs to run until either it has finished (which
        // it indicates by emitting the done() signal)... or until it is
        // canceled by the system.

        bool firstRun = true;

#ifndef Q_NO_EXCEPTIONS
        bool possiblyAbleToContinue = true;
#endif

        while (not didCancelOrFinish) {

#ifndef Q_NO_EXCEPTIONS
            try {
#endif
                if (firstRun)
                    getThinker().startMaybeEmitDone();
                else
                    // returns 0 if quit() or exit(), N if exit(N)
                    static_cast<void>(exec());  

#ifndef Q_NO_EXCEPTIONS
            } catch (const StopException& e) {
                possiblyAbleToContinue = false;
            }
#endif

            // we can get here if either the thinker itself announces being
            // finished or if we get a call to breakEventLoop()

            // the two things that call breakEventLoop are external requests to
            // pause or fully stop the thinker.

            // even if the thinker has finished, however, we can overwrite that
            // with a "Canceled" transition if the work the thinker has done
            // was invalidated

            _stateMutex.lock();

            if (_state == RunnerFinished) {

                didCancelOrFinish = true;

            } else if (_state == RunnerCanceling) {

                _state.hopefullyTransition(
                    RunnerCanceling, RunnerCanceled, HERE
                );

                _stateWasChanged.wakeOne();
                didCancelOrFinish = true;

            } else {

                _state.hopefullyTransition(RunnerPausing, RunnerPaused, HERE);
                _stateWasChanged.wakeOne();
                _stateWasChanged.wait(&_stateMutex);

                // Once we are paused, we just wait for a signal that we are to
                // either be aborted or continue.  (Because we are paused
                // there is no need to pass through a "Canceling" state while
                // the event loop is still running.)

                if (_state == RunnerCanceled) {
                    didCancelOrFinish = true;
                } else {
#ifndef Q_NO_EXCEPTIONS
                    // the Thinker may not have resume() implemented, even if
                    // it returned cleanly from an isPauseRequested (instead of
                    // using the exception variation)
                    hopefully(possiblyAbleToContinue, HERE);
#endif
                    _state.hopefullyTransition(
                        RunnerResuming, RunnerThinking,
                        HERE
                    );
                    _stateWasChanged.wakeOne();
                }
            }

            _stateMutex.unlock();
        }

        getThinker().beforeThreadDetach();

        // We no longer need the helper object
        _helper.clear();

        // For symmetry in constructor/destructor threading, we push the
        // Thinker back to the thread it was initially defined on.  This time
        // we can do it directly instead of asking that thread to do it for us.
        getThinker().moveToThread(originalThinkerThread);
        hopefully(getThinker().thread() == originalThinkerThread, HERE);

        _stateMutex.lock();
    }

    _state.hopefullyInSet(
        RunnerCanceled, RunnerCanceling, RunnerFinished, HERE
    );

    bool wasCanceled = (_state != RunnerFinished);
    _stateMutex.unlock();

    return wasCanceled;
}


void ThinkerRunner::requestPauseCore (
    bool isCanceledOkay,
    codeplace const & cp
) {
    hopefullyCurrentThreadIsNotThinker(HERE);
    getManager().processThreadPushes();

    QMutexLocker lock (&_stateMutex);

    if (_state == RunnerQueued) {
        _state.hopefullyTransition(RunnerQueued, RunnerQueuedButPaused, HERE);
        _stateWasChanged.wakeOne();
    } else if (_state == RunnerFinished) {
        // do nothing
    } else if (
        isCanceledOkay and (
            (_state == RunnerCanceling) or (_state == RunnerCanceled)
        )
    ) {
        // do nothing
    } else {
        _state.hopefullyTransition(RunnerThinking, RunnerPausing, cp);
        _stateWasChanged.wakeOne();

        emit breakEventLoop();
    }
}


void ThinkerRunner::waitForPauseCore (bool isCanceledOkay)
{
    hopefullyCurrentThreadIsNotThinker(HERE);
    getManager().processThreadPushes();

    QMutexLocker lock (&_stateMutex);

    if (
        (_state == RunnerFinished)
        or (_state == RunnerPaused)
        or (_state == RunnerQueuedButPaused)
    ) {
        // do nothing
    } else if (isCanceledOkay and (_state == RunnerCanceled)) {
        // do nothing
    } else if (isCanceledOkay and (_state == RunnerCanceling)) {
        _stateWasChanged.wait(&_stateMutex);
        _state.hopefullyEqualTo(RunnerCanceled, HERE);
    } else {
        _state.hopefullyEqualTo(RunnerPausing, HERE);
        _stateWasChanged.wait(&_stateMutex);
        _state.hopefullyInSet(RunnerPaused, RunnerFinished, HERE);
    }
}


void ThinkerRunner::requestCancelCore (
    bool isCanceledOkay,
    codeplace const & cp
) {
    hopefullyCurrentThreadIsNotThinker(cp);
    getManager().processThreadPushes();

    QMutexLocker lock (&_stateMutex);

    if (
        (_state == RunnerQueued)
        or (_state == RunnerFinished)
        or (_state == RunnerPaused)
        or (_state == RunnerQueuedButPaused)
    ) {
        _state.hopefullyAlter(RunnerCanceled, cp);
        _stateWasChanged.wakeOne();
    } else if (
        isCanceledOkay and (
            (_state == RunnerCanceled) or (_state == RunnerCanceling)
        )
    ) {
        // do nothing
    } else {
        // No one can request a pause or stop besides the worker
        // We should not multiply request stops and pauses...
        // so if it's not initializing and not finished it must be thinking!
        _state.hopefullyTransition(RunnerThinking, RunnerCanceling, cp);
        _stateWasChanged.wakeOne();

        emit breakEventLoop();
    }
}


void ThinkerRunner::requestResumeCore (
    bool isCanceledOkay,
    codeplace const & cp
) {
    hopefullyCurrentThreadIsNotThinker(cp);
    getManager().processThreadPushes();

    waitForPauseCore(isCanceledOkay);

    QMutexLocker lock (&_stateMutex);

    if (_state == RunnerQueuedButPaused) {
        _state.hopefullyAlter(RunnerQueued, HERE);
        _stateWasChanged.wakeOne();
    } else if (_state == RunnerFinished) {
        // do nothing
    } else if (isCanceledOkay and (_state == RunnerCanceled)) {
        // do nothing
    } else {
        _state.hopefullyTransition(RunnerPaused, RunnerResuming, cp);

        // only one should be waiting, max...
        _stateWasChanged.wakeOne();
    }
}


void ThinkerRunner::waitForResume (codeplace const & cp) {
    hopefullyCurrentThreadIsNotThinker(cp);
    getManager().processThreadPushes();

    QMutexLocker lock (&_stateMutex);

    if (
        (_state == RunnerThinking)
        or (_state == RunnerFinished)
        or (_state == RunnerQueued)
    ) {
        // do nothing
    } else {
        _state.hopefullyEqualTo(RunnerResuming, HERE);
        _stateWasChanged.wait(&_stateMutex);
        _state.hopefullyInSet(
            RunnerResuming, RunnerThinking, RunnerFinished,
            HERE
        );
    }
}


void ThinkerRunner::waitForFinished (codeplace const & cp) {
    hopefullyCurrentThreadIsNotThinker(cp);

    QMutexLocker lock (&_stateMutex);

    if ((_state == RunnerQueued) || (_state == RunnerThreadPush)) {
        lock.unlock();
        getManager().processThreadPushesUntil(this);
        lock.relock();
    }

    // Caller should know if they paused the thinker, and resume it before
    // calling this routine!
    if (_state == RunnerThinking)
        _stateWasChanged.wait(&_stateMutex);

    _state.hopefullyInSet(RunnerCanceled, RunnerFinished, HERE);
}


bool ThinkerRunner::isFinished () const {
    hopefullyCurrentThreadIsNotThinker(HERE);

    QMutexLocker lock (&_stateMutex);

    switch (_state) {
        case RunnerQueued:
        case RunnerThinking:
        case RunnerPausing:
        case RunnerPaused:
        case RunnerResuming:
            return false;
        case RunnerFinished:
            return true;
        case RunnerCanceled:
            // used to return indeterminate but removed tribool dependency
            return true;
        default:
            break;
    }

    throw hopefullyNotReached(HERE);
}


bool ThinkerRunner::isCanceled () const {
    hopefullyCurrentThreadIsNotThinker(HERE);

    QMutexLocker lock (&_stateMutex);

    return (_state == RunnerCanceled)
        or (_state == RunnerCanceling);
}


bool ThinkerRunner::isPaused () const {
    hopefullyCurrentThreadIsNotThinker(HERE);

    QMutexLocker lock (&_stateMutex);

    return (_state == RunnerPaused)
        or (_state == RunnerPausing)
        or (_state == RunnerQueuedButPaused);
}


bool ThinkerRunner::wasPauseRequested (unsigned long time) const {
    hopefullyCurrentThreadIsRun(HERE);

    QMutexLocker lock (&_stateMutex);

    if ((_state == RunnerPausing) or (_state == RunnerCanceling))
        return true;

    _state.hopefullyEqualTo(RunnerThinking, HERE);
    if (time == 0)
        return false;

    bool didStateChange = _stateWasChanged.wait(&_stateMutex, time);
    if (didStateChange) {
        _state.hopefullyInSet(RunnerPausing, RunnerCanceling, HERE);
    } else {
        // should not have changed
        _state.hopefullyEqualTo(RunnerThinking, HERE);
    }

    return didStateChange;
}


#ifndef Q_NO_EXCEPTIONS
void ThinkerRunner::pollForStopException (unsigned long time) const {
    if (wasPauseRequested(time))
        throw ThinkerRunner::StopException ();
}
#endif


ThinkerRunner::~ThinkerRunner () {
    // The thread this is deleted on may be either the thread pool thread
    // or the manager thread... it's controlled by a QSharedPointer

    _state.hopefullyInSet(
        RunnerCanceled, RunnerCanceling, RunnerFinished, HERE
    );
}



//
// ThinkerRunnerProxy
//

ThinkerRunnerProxy::ThinkerRunnerProxy (shared_ptr<ThinkerRunner> runner) :
    _runner (runner)
{
    getManager().addToThinkerMap(_runner);
}


ThinkerManager & ThinkerRunnerProxy::getManager ()
{
    return _runner->getManager();
}


void ThinkerRunnerProxy::run ()
{
    getManager().addToThreadMap(_runner, *QThread::currentThread());

    bool wasCanceled = _runner->runThinker();
    getManager().removeFromThreadMap(_runner, *QThread::currentThread());

    getManager().removeFromThinkerMap(_runner, wasCanceled);
    // We should be cleaning up this object using auto-delete.
    hopefully(autoDelete(), HERE);
}


ThinkerRunnerProxy::~ThinkerRunnerProxy ()
{
}
