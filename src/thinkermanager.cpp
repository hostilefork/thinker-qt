//
// thinkermanager.cpp
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

#include <QThreadPool>
#include <QMutexLocker>

#include "thinkerqt/thinkerrunner.h"
#include "thinkerqt/thinkermanager.h"


#ifndef THINKERQT_EXPLICIT_MANAGER
    ThinkerManager & ThinkerManager::getGlobalManager () {
        //
        // This may depend on C++11 if you call this from more than one thread
        // http://stackoverflow.com/questions/9608257/
        //
        static ThinkerManager globalInstance;
        return globalInstance;
    }
#endif


ThinkerManager::ThinkerManager () :
    QObject (),

    // Hardcoded value; should probably be configurable.  Optional parameter
    // to the constructor?
    //
    _anyThinkerWrittenThrottler (400),

    // We create this Mutex recursively because the checks for whether a
    // thread is in the thread pool or not require the map at the moment.
    // Those checks may be run when a higher level routine has locked the
    // mutex.  It may be desirable to have those assertions use a different
    // (and possibly faster) method for the test so we wouldn't have to
    // make this allow nested locks.
    //
    _mapsMutex (QMutex::Recursive)
{
    hopefullyCurrentThreadIsManager(HERE);

    connect(
        &_anyThinkerWrittenThrottler, &SignalThrottler::throttled,
        this, &ThinkerManager::anyThinkerWritten,
        Qt::DirectConnection
    );

    connect(
        this, &ThinkerManager::pushToThreadMayBeNeeded,
        this, &ThinkerManager::doThreadPushesIfNecessary,
        Qt::QueuedConnection
    );
}


bool ThinkerManager::hopefullyThreadIsManager(
    const QThread & thread,
    codeplace const & cp
){
    return hopefully(&thread == this->thread(), cp);
}


bool ThinkerManager::hopefullyCurrentThreadIsManager(
    codeplace const & cp
){
    return hopefullyThreadIsManager(*QThread::currentThread(), cp);
}


bool ThinkerManager::hopefullyThreadIsNotManager(
    const QThread & thread,
    codeplace const & cp
){
    return hopefully(&thread != this->thread(), cp);
}


bool ThinkerManager::hopefullyCurrentThreadIsNotManager(
    codeplace const & cp
){
    return hopefullyThreadIsNotManager(*QThread::currentThread(), cp);
}


bool ThinkerManager::hopefullyThreadIsNotThinker(
    const QThread & thread,
    codeplace const & cp
){
    return hopefully(maybeGetRunnerForThread(thread) == nullptr, cp);
}


bool ThinkerManager::hopefullyCurrentThreadIsNotThinker(
    codeplace const & cp
){
    return hopefullyThreadIsNotThinker(*QThread::currentThread(), cp);
}


bool ThinkerManager::hopefullyThreadIsThinker(
    const QThread & thread,
    codeplace const & cp
){
    return hopefully(maybeGetRunnerForThread(thread) != nullptr, cp);
}


bool ThinkerManager::hopefullyCurrentThreadIsThinker(
    codeplace const & cp
){
    return hopefullyThreadIsThinker(*QThread::currentThread(), cp);
}


void ThinkerManager::createRunnerForThinker(
    shared_ptr<ThinkerBase> holder,
    codeplace const & cp
){
    hopefullyCurrentThreadIsManager(cp);
    hopefully(holder != nullptr, cp);

    auto runner = make_shared<ThinkerRunner>(holder);

    ThinkerRunnerProxy * proxy = new ThinkerRunnerProxy (runner);

    // this may look like a bad idea because we are dynamically allocating
    // and not hanging onto the result so we can free it.  but it's okay
    // because ThinkerRunners maintain a global table which they insert
    // themselves into on construction and delete themselves from during
    // destruction.  Also, when the thread emits the finished signal, we clean
    // it up using deleteLater (it is freed by the event loop when all events
    // have been processed)
    //
    proxy->setAutoDelete(true);

    // QtConcurrent defines one global thread pool instance.  But maybe I'll
    // let you specify your own, not sure if that's useful.  They make a lot
    // of global assumptions, perhaps I should just piggy back on them.

    // Queue this runnable thing to the thread pool.  It may take a while
    // before a thread gets allocated to it.
    //
    static_cast<void>(QThreadPool::globalInstance()->start(proxy));
}


void ThinkerManager::ensureThinkersPaused(codeplace const & cp)
{
    hopefullyCurrentThreadIsNotThinker(HERE);

    // we have to make a copy of the map
    //
    QMutexLocker lock (&_mapsMutex);
    auto mapCopy = _thinkerMap;
    lock.unlock();

    // for loops on Qt maps iterate just the values:
    // http://stackoverflow.com/questions/8517853/

    // First pass: request all thinkers to pause (accept it if they are
    // aborting, as they may be freed by the ThinkerPresent, not yet returned).

    for (auto & runner : mapCopy) {
        runner->requestPauseButPausedOrCanceledIsOkay(cp);
    }

    // Second pass: wait for all the thinkers to get their code off the stack.

    for (auto & runner : mapCopy) {
        runner->waitForPauseButCanceledIsOkay();
    }
}


void ThinkerManager::ensureThinkersResumed(codeplace const & cp)
{
    hopefullyCurrentThreadIsNotThinker(HERE);

    QMutexLocker lock (&_mapsMutex);

    // any thinkers that have not been aborted can be resumed

    for (auto & runner : _thinkerMap) {
        if (runner->isPaused())
            runner->requestResumeButCanceledIsOkay(cp);
    }
}


shared_ptr<ThinkerRunner> ThinkerManager::maybeGetRunnerForThread(
    const QThread & thread
){
    QMutexLocker lock (&_mapsMutex);

    shared_ptr<ThinkerRunner> result = _threadMap.value(&thread, nullptr);

    return result;
}


shared_ptr<ThinkerRunner> ThinkerManager::maybeGetRunnerForThinker(
    ThinkerBase const & thinker
){
    using State = ThinkerBase::State;

    QMutexLocker lock (&_mapsMutex);

    shared_ptr<ThinkerRunner> result = _thinkerMap.value(&thinker, nullptr);
    if (not result) {
        hopefully(
            (thinker._state == State::ThinkerCanceled)
            or (thinker._state == State::ThinkerFinished),
            HERE
        );
    }

    return result;
}


const ThinkerBase * ThinkerManager::getThinkerForThreadMaybeNull(
    const QThread & thread
){
    shared_ptr<ThinkerRunner> runner = maybeGetRunnerForThread(thread);
    if (not runner)
        return nullptr;

    return &(runner->getThinker());
}


void ThinkerManager::requestAndWaitForCancelButAlreadyCanceledIsOkay(
    ThinkerBase & thinker
){
    using State = ThinkerBase::State;

    shared_ptr<ThinkerRunner> runner = maybeGetRunnerForThinker(thinker);
    if (not runner)
        thinker._state = State::ThinkerCanceled;
    else {
        // thread should be paused or finished... or possibly aborted
        //
        runner->requestCancelButAlreadyCanceledIsOkay(HERE);
        runner->waitForFinished(HERE);
    }
    hopefully(thinker._state == State::ThinkerCanceled, HERE);
}


void ThinkerManager::ensureThinkerFinished(ThinkerBase & thinker)
{
    using State = ThinkerBase::State;

    hopefullyCurrentThreadIsNotThinker(HERE);

    shared_ptr<ThinkerRunner> runner = maybeGetRunnerForThinker(thinker);
    if (runner != nullptr) {
        //
        // can't finish if it's aborted or invalid!
        //
        hopefully(not runner->isCanceled(), HERE); 

        // we need to watch the state changes and ensure that it completes...
        // note user cancellation would mean that it would not so we have to
        // allow for that case!
        //
        if (runner->isPaused()) {
            runner->requestResume(HERE);
            runner->waitForResume(HERE);
        }

        runner->waitForFinished(HERE);
        hopefully(runner->isFinished(), HERE);
        thinker._state = State::ThinkerFinished;
    }

    hopefully(thinker._state == State::ThinkerFinished, HERE);
}


void ThinkerManager::unlockThinker(ThinkerBase & thinker)
{
    // do throttled emit to all the ThinkerPresentWatchers
    {
        QReadLocker lock (&thinker._watchersLock);

        for (ThinkerPresentWatcherBase * watcher : thinker._watchers) {
            watcher->_notificationThrottler->emitThrottled();
        }
    }

    // there is a notification throttler for all thinkers.  Review: should it
    // be possible to have a separate notification for groups?
    //
    _anyThinkerWrittenThrottler.emitThrottled();
}


void ThinkerManager::addToThinkerMap(shared_ptr<ThinkerRunner> runner)
{
    // We use a mutex to guard the addition and removal of Runners to the maps
    // If a Runner exists, then we look to its state information for
    // cancellation--not the Thinker.

    QMutexLocker lock (&_mapsMutex);

    ThinkerBase & thinker = runner->getThinker();
    hopefully(not _thinkerMap.contains(&thinker), HERE);
    _thinkerMap.insert(&thinker, runner);
}


void ThinkerManager::removeFromThinkerMap(
    shared_ptr<ThinkerRunner> runner,
    bool wasCanceled
){
    using State = ThinkerBase::State;

    QMutexLocker lock (&_mapsMutex);

    ThinkerBase & thinker = runner->getThinker();
    hopefully(_thinkerMap.remove(&thinker) == 1, HERE);

    hopefully(thinker._state == State::ThinkerOwnedByRunner, HERE);
    thinker._state = wasCanceled
        ? State::ThinkerCanceled
        : State::ThinkerFinished;
}


void ThinkerManager::addToThreadMap(
    shared_ptr<ThinkerRunner> runner,
    QThread & thread
){
    QMutexLocker lock (&_mapsMutex);

    hopefully(not _threadMap.contains(&thread), HERE);
    _threadMap.insert(&thread, runner);
}


void ThinkerManager::removeFromThreadMap(
    shared_ptr<ThinkerRunner> runner,
    QThread & thread
){
    Q_UNUSED(runner)

    QMutexLocker lock (&_mapsMutex);

    hopefully(_threadMap.remove(&thread) == 1, HERE);
}


void ThinkerManager::waitForPushToThread(ThinkerRunner * runner)
{
    hopefullyCurrentThreadIsThinker(HERE);

    QMutexLocker lock (&_pushThreadMutex);

    _runnerSetToPush.insert(runner);
    _threadsNeedPushing.wakeOne();
    emit pushToThreadMayBeNeeded();
    _threadsWerePushed.wait(&_pushThreadMutex);
}


void ThinkerManager::processThreadPushesUntil(ThinkerRunner * runner)
{
    hopefullyCurrentThreadIsNotThinker(HERE);

    QMutexLocker lock (&_pushThreadMutex);
    forever {
        bool found = false;
        for (ThinkerRunner * runnerToPush : _runnerSetToPush) {
            runnerToPush->doThreadPushIfNecessary();
            if (runner and (runnerToPush == runner))
                found = true;
        }
        _runnerSetToPush.clear();
        _threadsWerePushed.wakeAll();
        if (found or not runner)
            return;
        _threadsNeedPushing.wait(&_pushThreadMutex);
    }
}


void ThinkerManager::doThreadPushesIfNecessary()
{
    processThreadPushesUntil(nullptr);
}


ThinkerManager::~ThinkerManager ()
{
    hopefullyCurrentThreadIsManager(HERE);

    // We catch you with an assertion if you do not make sure all your
    // Presents have been either canceled or completed
    //
    bool anyRunners = false;

    {
        QMutexLocker lock (&_mapsMutex);

        for (shared_ptr<ThinkerRunner> runner : _thinkerMap) {
            hopefully(runner->isCanceled() or runner->isFinished(), HERE);
            anyRunners = true;
        }
    }

    if (anyRunners)
        QThreadPool::globalInstance()->waitForDone();
}
