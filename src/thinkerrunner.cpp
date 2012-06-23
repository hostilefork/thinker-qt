//
// ThinkerRunner.cpp
// This file is part of Thinker-Qt
// Copyright (C) 2010 HostileFork.com
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

#include "thinkerrunner.h"
#include "thinkerqt/thinkermanager.h"

// operator<< for ThinkerRunner::State
//
// Required by tracked< > because it must be able to present a proper debug
// message about the tracked value.

inline QTextStream& operator<< (QTextStream& o, const ThinkerRunner::State& state)
{
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

ThinkerRunnerHelper::ThinkerRunnerHelper(ThinkerRunner& runner) :
	QObject (), // affinity from current thread, no parent
	runner (runner)
{
	runner.getManager().hopefullyCurrentThreadIsNotManager(HERE);
}

void ThinkerRunnerHelper::queuedQuit()
{
	hopefullyCurrentThreadIsRun(HERE);
	runner.quit();
}

void ThinkerRunnerHelper::markFinished()
{
	hopefullyCurrentThreadIsRun(HERE);

	QMutexLocker locker (&runner.stateMutex);

	if (runner.state == ThinkerRunner::RunnerCanceling) {
		// we don't let it transition to finished if abort is requested
	} else {
		runner.state.hopefullyInSet(ThinkerRunner::RunnerThinking, ThinkerRunner::RunnerPausing, HERE);
		runner.state.hopefullyAlter(ThinkerRunner::RunnerFinished, HERE);
		runner.stateWasChanged.wakeOne();
		runner.quit();
	}
}

ThinkerRunnerHelper::~ThinkerRunnerHelper()
{
	hopefullyCurrentThreadIsRun(HERE);
}



//
// ThinkerRunner
//

ThinkerRunner::ThinkerRunner(ThinkerHolder<ThinkerBase> holder) :
	QEventLoop (),
	state (RunnerQueued, HERE),
	holder (holder),
	helper ()
{
/*	hopefully(not holder.isNull(), HERE); */

	// need to check this, because we will later ask the manager to move the
	// Thinker to the thread of the QRunnable (when we find out what that thread
	// is; we don't know until the Thread Pool decides to run this).  Must ask
	// the manager thread because we can only push from a thread, not pull onto
	// an arbitrary one...
	hopefullyCurrentThreadIsManager(HERE);
	hopefully(getThinker().thread() == QThread::currentThread(), HERE);

	connect(this, SIGNAL(resumeThinking()), &getThinker(), SLOT(onResumeThinking()), Qt::QueuedConnection);
}

bool ThinkerRunner::hopefullyCurrentThreadIsManager(const codeplace& cp) const {
	return getManager().hopefullyCurrentThreadIsManager(cp);
}

bool ThinkerRunner::hopefullyCurrentThreadIsRun(const codeplace& cp) const {
	hopefully(helper, cp);
	return helper->hopefullyCurrentThreadIsRun(cp);
}

ThinkerManager& ThinkerRunner::getManager() const
{
	return getThinker().getManager();
}

const ThinkerBase& ThinkerRunner::getThinker() const {
    return holder.getThinkerBase();
}

ThinkerBase& ThinkerRunner::getThinker() {
    return holder.getThinker();
}

void ThinkerRunner::doThreadPushIfNecessary()
{
	hopefullyCurrentThreadIsManager(HERE);

	QMutexLocker locker (&stateMutex);
	if (state == RunnerThreadPush) {
		hopefully(helper, HERE);
		getThinker().moveToThread(helper->thread());
		state.hopefullyAlter(RunnerThinking, HERE);
		stateWasChanged.wakeOne();
	}
}

bool ThinkerRunner::runThinker()
{
	helper = QSharedPointer<ThinkerRunnerHelper> (new ThinkerRunnerHelper(*this));

	connect(this, SIGNAL(breakEventLoop()), helper.data(), SLOT(queuedQuit()), Qt::QueuedConnection);
	connect(&getThinker(), SIGNAL(done()), helper.data(), SLOT(markFinished()), Qt::DirectConnection);

	stateMutex.lock();

	if (state == RunnerQueuedButPaused) {
		stateWasChanged.wait(&stateMutex);
	}
	state.hopefullyInSet(RunnerQueued, RunnerCanceled, HERE);

	if (state == RunnerQueued) {
		QThread* originalThinkerThread (getThinker().thread());
		// Now that we know what thread the Thinker will be running on, we ask the
		// main thread to push it onto our current thread allocated to us by the pool
		state.hopefullyAlter(RunnerThreadPush, HERE);
		stateWasChanged.wakeOne();
		stateMutex.unlock();

		getManager().waitForPushToThread(this);

		// There are two places where the object will be pushed.  One is from the event
		// loop if the signal happens.  But if before that can happen any of our code
		// gets called from the manager thread then we'll preempt that.
		stateMutex.lock();
		state.hopefullyInSet(RunnerThinking, RunnerCanceling, HERE);
		bool didCancelOrFinish (state == RunnerCanceling);
		stateMutex.unlock();

		hopefully(getThinker().thread() == QThread::currentThread(), HERE);

		// TODO: will it be possible to use resumable coroutines so that an idle thinker
		// which is waiting for a message could delegate some time to another thinker?
		getThinker().afterThreadAttach();

		// The thinker thread needs to run until either it has finished (which it indicates by
		// emitting the done() signal)... or until it is canceled by the system.

		bool firstRun (true);

#ifndef Q_NO_EXCEPTIONS
		bool possiblyAbleToContinue (false);
#endif

		while (not didCancelOrFinish) {

#ifndef Q_NO_EXCEPTIONS
			try {
#endif
				if (firstRun)
					getThinker().start();
				else
					static_cast<void>(exec());  // returns 0 if quit() or exit(), N if exit(N)

#ifndef Q_NO_EXCEPTIONS
			} catch (const StopException& e) {
				possiblyAbleToContinue = false;
			}
#endif

			// we can get here if either the thinker itself announces being finished
			// or if we get a call to breakEventLoop()

			// the two things that call breakEventLoop are external requests to
			// pause or fully stop the thinker.

			// even if the thinker has finished, however, we can overwrite that with
			// a "Canceled" transition if the work the thinker has done was invalidated

			stateMutex.lock();

			if (state == RunnerFinished) {

				didCancelOrFinish = true;

			} else if (state == RunnerCanceling) {

				state.hopefullyTransition(RunnerCanceling, RunnerCanceled, HERE);
				stateWasChanged.wakeOne();
				didCancelOrFinish = true;

			} else {

				state.hopefullyTransition(RunnerPausing, RunnerPaused, HERE);
				stateWasChanged.wakeOne();
				stateWasChanged.wait(&stateMutex);

				// Once we are paused, we just wait for a signal that we are to
				// either be aborted or continue.  (Because we are paused
				// there is no need to pass through a "Canceling" state while
				// the event loop is still running.)

				if (state == RunnerCanceled) {
					didCancelOrFinish = true;
				} else {
#ifndef Q_NO_EXCEPTIONS
					// ...the Thinker may not have continueThinking implemented, even if
					// it returned cleanly from an isPauseRequested (instead of using
					// the exception variation)
					hopefully(possiblyAbleToContinue, HERE);
#endif
					state.hopefullyTransition(RunnerResuming, RunnerThinking, HERE);
					stateWasChanged.wakeOne();
				}
			}

			stateMutex.unlock();
		}

		getThinker().beforeThreadDetach();

		// For symmetry in constructor/destructor threading, we push the Thinker back to the
		// thread it was initially defined on.  This time we can do it directly instead of asking
		// that thread to do it for us.
		if (true) {
			getThinker().moveToThread(originalThinkerThread);
			hopefully(getThinker().thread() == originalThinkerThread, HERE);
		}

		stateMutex.lock();
	}

	state.hopefullyInSet(RunnerCanceled, RunnerCanceling, RunnerFinished, HERE);
	bool wasCanceled (state != RunnerFinished);
	stateMutex.unlock();

	// We no longer need the helper object
	helper.clear();

	return wasCanceled;
}

void ThinkerRunner::requestPauseCore(bool isCanceledOkay, const codeplace& cp)
{
	hopefullyCurrentThreadIsManager(HERE);
	getManager().processThreadPushes();

	QMutexLocker locker (&stateMutex);

	if (state == RunnerQueued) {
		state.hopefullyTransition(RunnerQueued, RunnerQueuedButPaused, HERE);
		stateWasChanged.wakeOne();
	} else if (state == RunnerFinished) {
		// do nothing
	} else if (isCanceledOkay and ((state == RunnerCanceling) or (state == RunnerCanceled))) {
		// do nothing
	} else {
		state.hopefullyTransition(RunnerThinking, RunnerPausing, cp);
		stateWasChanged.wakeOne();

		emit breakEventLoop();
	}
}

void ThinkerRunner::waitForPauseCore(bool isCanceledOkay)
{
	hopefullyCurrentThreadIsManager(HERE);
	getManager().processThreadPushes();

	QMutexLocker locker (&stateMutex);

	if ((state == RunnerFinished) or (state == RunnerPaused) or (state == RunnerQueuedButPaused)) {
		// do nothing
	} else if (isCanceledOkay and (state == RunnerCanceled)) {
		// do nothing
	} else if (isCanceledOkay and (state == RunnerCanceling)) {
		stateWasChanged.wait(&stateMutex);
		state.hopefullyEqualTo(RunnerCanceled, HERE);
	} else {
		state.hopefullyEqualTo(RunnerPausing, HERE);
		stateWasChanged.wait(&stateMutex);
		state.hopefullyInSet(RunnerPaused, RunnerFinished, HERE);
	}
}

void ThinkerRunner::requestCancelCore(bool isCanceledOkay, const codeplace& cp)
{
	hopefullyCurrentThreadIsManager(HERE);
	getManager().processThreadPushes();

	QMutexLocker locker (&stateMutex);

	if ((state == RunnerQueued) || (state == RunnerFinished) || (state == RunnerPaused) || (state == RunnerQueuedButPaused)) {
		state.hopefullyAlter(RunnerCanceled, cp);
		stateWasChanged.wakeOne();
	} else if (isCanceledOkay and ((state == RunnerCanceled) || (state == RunnerCanceling))) {
		// do nothing
	} else {
		// No one can request a pause or stop besides the worker
		// We should not multiply request stops and pauses...
		// so if it's not initializing and not finished it must be thinking!
		state.hopefullyTransition(RunnerThinking, RunnerCanceling, cp);
		stateWasChanged.wakeOne();

		emit breakEventLoop();
	}
}

void ThinkerRunner::requestResumeCore(bool isCanceledOkay, const codeplace& cp)
{
	hopefullyCurrentThreadIsManager(HERE);
	getManager().processThreadPushes();

	waitForPauseCore(isCanceledOkay);

	QMutexLocker locker (&stateMutex);

	if (state == RunnerQueuedButPaused) {
		state.hopefullyAlter(RunnerQueued, HERE);
		stateWasChanged.wakeOne();
	} else if (state == RunnerFinished) {
		// do nothing
	} else if (isCanceledOkay and (state == RunnerCanceled)) {
		// do nothing
	} else {
		state.hopefullyTransition(RunnerPaused, RunnerResuming, cp);
		stateWasChanged.wakeOne(); // only one person should be waiting on this, max...
		emit resumeThinking();
	}
}

void ThinkerRunner::waitForResume(const codeplace& /*cp*/)
{
	hopefullyCurrentThreadIsManager(HERE);
	getManager().processThreadPushes();

	QMutexLocker locker (&stateMutex);

	if ((state == RunnerThinking) or (state == RunnerFinished) or (state == RunnerQueued)) {
		// do nothing
	} else {
		state.hopefullyEqualTo(RunnerResuming, HERE);
		stateWasChanged.wait(&stateMutex);
		state.hopefullyInSet(RunnerResuming, RunnerThinking, RunnerFinished, HERE);
	}
}

void ThinkerRunner::waitForFinished(const codeplace& /*cp*/)
{
	hopefullyCurrentThreadIsManager(HERE);

	QMutexLocker locker (&stateMutex);

	if ((state == RunnerQueued) || (state == RunnerThreadPush)) {
		locker.unlock();
		getManager().processThreadPushesUntil(this);
		locker.relock();
	}

	// Caller should know if they paused the thinker, and resume it before
	// calling this routine!
	if (state == RunnerThinking)
		stateWasChanged.wait(&stateMutex);

	state.hopefullyInSet(RunnerCanceled, RunnerFinished, HERE);
}

bool ThinkerRunner::isFinished() const {
	hopefullyCurrentThreadIsManager(HERE);

	QMutexLocker locker (&stateMutex);

	bool result (false);
	switch (state) {
		case RunnerQueued:
		case RunnerThinking:
		case RunnerPausing:
		case RunnerPaused:
		case RunnerResuming:
			result = false;
			break;
		case RunnerFinished:
			result = true;
			break;
		case RunnerCanceled:
			// used to return indeterminate here but removed tribool/boost dependency
			result = false;
			break;
		default:
			hopefullyNotReached(HERE);
	}
	return result;
}

bool ThinkerRunner::isCanceled() const {
	hopefullyCurrentThreadIsManager(HERE);

	QMutexLocker locker (&stateMutex);

	bool result (false);
	result = (state == RunnerCanceled) or (state == RunnerCanceling);
	return result;
}

bool ThinkerRunner::isPaused() const {
	hopefullyCurrentThreadIsManager(HERE);

	QMutexLocker locker (&stateMutex);

	bool result (false);
	result = (state == RunnerPaused) or (state == RunnerPausing) or (state == RunnerQueuedButPaused);
	return result;
}

bool ThinkerRunner::wasPauseRequested(unsigned long time) const
{
	hopefullyCurrentThreadIsRun(HERE);

	QMutexLocker locker (&stateMutex);

	bool result (false);
	if ((state == RunnerPausing) or (state == RunnerCanceling)) {
		result = true;
	} else {
		state.hopefullyEqualTo(RunnerThinking, HERE);
		if (time == 0) {
			result = false;
		} else {
			bool didStateChange (stateWasChanged.wait(&stateMutex, time));
			if (didStateChange)
				state.hopefullyInSet(RunnerPausing, RunnerCanceling, HERE);
			else
				state.hopefullyEqualTo(RunnerThinking, HERE); // should not have changed
			result = didStateChange;
		}
	}
	return result;
}

#ifndef Q_NO_EXCEPTIONS
void ThinkerRunner::pollForStopException(unsigned long time) const
{
	if (wasPauseRequested(time))
		throw ThinkerRunner::StopException ();
}
#endif

ThinkerRunner::~ThinkerRunner()
{
	// The thread this is deleted on may be either the thread pool thread
	// or the manager thread... it's controlled by a QSharedPointer

	state.hopefullyInSet(RunnerCanceled, RunnerCanceling, RunnerFinished, HERE);
}


//
// ThinkerRunnerProxy
//

ThinkerRunnerProxy::ThinkerRunnerProxy (shared_ptr_type<ThinkerRunner> runner) :
	runner (runner)
{
	getManager().addToThinkerMap(runner);
}

ThinkerManager& ThinkerRunnerProxy::getManager() {
	return runner->getManager();
}

void ThinkerRunnerProxy::run() {
	getManager().addToThreadMap(runner, *QThread::currentThread());

	bool wasCanceled (runner->runThinker());
	getManager().removeFromThreadMap(runner, *QThread::currentThread());

	getManager().removeFromThinkerMap(runner, wasCanceled);
	// We should be cleaning up this object using auto-delete.
	hopefully(autoDelete(), HERE);
}

ThinkerRunnerProxy::~ThinkerRunnerProxy ()
{
}
