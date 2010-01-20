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

#include "thinkerrunner.h"
#include "thinkerqt/thinkermanager.h"

// operator << for ThinkerRunner::State
//
// Required by tracked< > because it must be able to present a proper debug
// message about the tracked value.

inline QTextStream& operator << (QTextStream& o, const ThinkerRunner::State& state)
{
	o << "ThinkerRunner::";
	switch (state) {
	case ThinkerRunner::RunnerInitializing:
		o << "RunnerInitializing";
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
	case ThinkerRunner::RunnerCanceling:
		o << "RunnerCanceling";
		break;
	case ThinkerRunner::RunnerCanceled:
		o << "RunnerCanceled";
		break;
	case ThinkerRunner::RunnerResuming:
		o << "RunnerResuming";
		break;
	case ThinkerRunner::RunnerFinished:
		o << "RunnerFinished";
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
	runner.getManager().hopefullyCurrentThreadIsThinker(HERE);
	runner.quit();
}

void ThinkerRunnerHelper::markFinished()
{
	runner.hopefullyCurrentThreadIsPooled(HERE);

	runner.signalMutex.lock();
	if (runner.state == ThinkerRunner::RunnerCanceling) {
		// we don't let it transition to finished if abort is requested
	} else {
		runner.state.hopefullyInSet(ThinkerRunner::RunnerThinking, ThinkerRunner::RunnerPausing, HERE);
		runner.state.hopefullyAlter(ThinkerRunner::RunnerFinished, HERE);
		runner.stateChangeSignal.wakeOne();
		runner.quit();
	}

	runner.signalMutex.unlock();
}

ThinkerRunnerHelper::~ThinkerRunnerHelper()
{
	runner.getManager().hopefullyCurrentThreadIsNotManager(HERE);
}



//
// ThinkerRunner
//

ThinkerRunner::ThinkerRunner(ThinkerHolder< ThinkerBase > holder) :
	QEventLoop (),
	state (RunnerInitializing, HERE),
	holder (holder)
{
	hopefully(not holder.isNull(), HERE);

	// need to check this, because we will later ask the manager to move the
	// Thinker to the thread of the QRunnable (when we find out what that thread
	// is; we don't know until the Thread Pool decides to run this).  Must ask
	// the manager thread because we can only push from a thread, not pull onto
	// an arbitrary one...
	getManager().hopefullyCurrentThreadIsManager(HERE);
	hopefully(getThinker().thread() == QThread::currentThread(), HERE);

	connect(this, SIGNAL(resumeThinking()), &getThinker(), SLOT(onResumeThinking()), Qt::QueuedConnection);

	// TODO: The Thread object's affinity is the thread that created it.  This means that
	// if you connect a signal to the QThread, it will not be executed in that separate
	// thread of execution.  *HOWEVER* the "finished()" and "starting()" signals are
	// emitted from its independent thread of execution.  This is to say that if you were
	// to connect the thread's finished() signal to a slot on the thread, it would be
	// a queued connection.
	// I should research the handling of these signals, because right now they are
	// queued -- which means as long as the worker thread is doing something (e.g.
	// rendering, or regenerating decks) then the thread object will not be freed.
	connect(this, SIGNAL(finished(ThinkerBase*, bool)), &getManager(), SLOT(onRunnerFinished(ThinkerBase*, bool)), Qt::QueuedConnection);
}

bool ThinkerRunner::hopefullyCurrentThreadIsPooled(const codeplace& cp) const {
	// TODO should do a stronger check w/the manager to make sure that we not only
	// are a thinker thread but the thinker thread currently running in the thread
	// pool
	return getManager().hopefullyCurrentThreadIsThinker(cp);
}


ThinkerManager& ThinkerRunner::getManager() const
{
	return getThinker().getManager();
}

const ThinkerBase& ThinkerRunner::getThinker() const {
	return *holder.data();
}

ThinkerBase& ThinkerRunner::getThinker() {
	return *holder.data();
}

void ThinkerRunner::onMoveThinkerToThread(QThread* thread, QSemaphore* numThreadsMoved)
{
	getManager().hopefullyCurrentThreadIsManager(HERE);
	getThinker().moveToThread(thread);
	numThreadsMoved->release(1); // bump threads moved count
}

void ThinkerRunner::run()
{
	tracked< bool > debugThinkerRunner_Run (false, HERE);

	if (debugThinkerRunner_Run) {
		QString message;
		QTextStream ts (&message);
		ts << "entering ThinkerRunner::run() this = " << this;
		chronicle(debugThinkerRunner_Run, message, HERE);
	}

	ThinkerRunnerHelper helper (*this);

	connect(this, SIGNAL(moveThinkerToThread(QThread*, QSemaphore*)),
		this, SLOT(onMoveThinkerToThread(QThread*, QSemaphore*)), Qt::QueuedConnection);

	QThread* originalThinkerThread (getThinker().thread());

	// Now that we know what thread the Thinker will be running on, we ask the
	// main thread to push the Thinker onto our current thread
	if (true) {
		QSemaphore numThreadsMoved (0); // number of threads moved to main starts at 0

		emit moveThinkerToThread(QThread::currentThread(), &numThreadsMoved);
		numThreadsMoved.acquire(); // blocks until the move bumps the semaphore

		hopefully(getThinker().thread() == QThread::currentThread(), HERE);
	}

	// TODO: Tend to race condition when someone is enumerating the runners and tries to
	// pause them all, but this addition happens during the enumeration, etc.

	if (getThinker().state == ThinkerBase::ThinkerCanceled) {
		state.hopefullyTransition(RunnerInitializing, RunnerCanceled, HERE);
	} else {
		mapped< const QThread*, ThinkerRunner* > mapThread (QThread::currentThread(), this, getManager().getThreadMapManager(), HERE);
		mapped< const ThinkerBase*, ThinkerRunner* > mapThinker (&getThinker(), this, getManager().getRunnerMapManager(), HERE);

		if (debugThinkerRunner_Run) {
			QString message;
			QTextStream ts (&message);
			ts << "added to maps this = " << this;
			chronicle(debugThinkerRunner_Run, message, HERE);
		}

		// The system notifies the thinker thread of its desire to interrupt processing in a
		// cooperative multitasking fashion.  The thinker must poll isPauseRequested()
		// periodically, and if it senses this it should return from whatever processing
		// it is doing.

		connect(this, SIGNAL(breakEventLoop()), &helper, SLOT(queuedQuit()), Qt::QueuedConnection);

		// The thinker thread needs to run until either it has finished (which it indicates by
		// emitting the thinkingFinished signal)... or until it is canceled by the system.
		connect(&getThinker(), SIGNAL(done()), &helper, SLOT(markFinished()), Qt::DirectConnection);

		signalMutex.lock();
		state.hopefullyTransition(RunnerInitializing, RunnerThinking, HERE);
		stateChangeSignal.wakeOne();
		signalMutex.unlock();

		getThinker().afterThreadAttach();
		// in theory the thread might be detached any time the event loop is paused/etc.

		bool firstRun (true);

#ifndef Q_NO_EXCEPTIONS
		bool possiblyAbleToContinue (false);
#endif

		bool didCancelOrFinish (false);
		while (not didCancelOrFinish) {

			// there should be a startThinking() or continueThinking() message queued...

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

			signalMutex.lock();

			if (state == RunnerFinished) {

				didCancelOrFinish = true;

			} else if (state == RunnerCanceling) {

				state.hopefullyTransition(RunnerCanceling, RunnerCanceled, HERE);
				stateChangeSignal.wakeOne();
				didCancelOrFinish = true;

			} else {

				state.hopefullyTransition(RunnerPausing, RunnerPaused, HERE);
				stateChangeSignal.wakeOne();
				stateChangeSignal.wait(&signalMutex);

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
					stateChangeSignal.wakeOne();
				}
			}

			signalMutex.unlock();
		}

		getThinker().beforeThreadDetach();
	}

	// For symmetry in constructor/destructor threading, we push the Thinker back to the
	// thread it was initially defined on.  This time we can do it directly instead of asking
	// that thread to do it for us.
	if (true) {
		getThinker().moveToThread(originalThinkerThread);
		hopefully(getThinker().thread() == originalThinkerThread, HERE);
	}

	if (state == RunnerCanceled) {
		emit finished(&getThinker(), true);
	} else if (state == RunnerFinished) {
		emit finished(&getThinker(), false);
	} else {
		hopefullyNotReached(HERE);
	}

	if (debugThinkerRunner_Run) {
		QString message;
		QTextStream ts (&message);
		ts << "exiting ThinkerRunner::run() this = " << this;
		chronicle(debugThinkerRunner_Run, message, HERE);
	}

	// We should be cleaning up this object using auto-delete.
	hopefully(autoDelete(), HERE);

	// After this routine returns, the thread pool may well destroy the thread
	// The helper and other thread-related objects must be cleaned up
}

void ThinkerRunner::requestPauseCore(bool isCanceledOkay, const codeplace& cp)
{
	getManager().hopefullyCurrentThreadIsManager(HERE);

	signalMutex.lock();
	// TODO: Should we introduce an Initializing transition that allows
	// going directly to paused or aborted, rather than pass through
	// Thinking?  It might lower some latency.
	if (state == RunnerInitializing) {
		stateChangeSignal.wait(&signalMutex);
	}
	if (state == RunnerFinished) {
		// do nothing
	} else if (isCanceledOkay and ((state == RunnerCanceling) or (state == RunnerCanceled))) {
		// do nothing
	} else {
		state.hopefullyTransition(RunnerThinking, RunnerPausing, cp);
		stateChangeSignal.wakeOne();

		emit breakEventLoop();
	}
	signalMutex.unlock();
}

void ThinkerRunner::waitForPauseCore(bool isCanceledOkay)
{
	getManager().hopefullyCurrentThreadIsManager(HERE);

	signalMutex.lock();
	if ((state == RunnerFinished) or (state == RunnerPaused)) {
		// do nothing
	} else if (isCanceledOkay and (state == RunnerCanceled)) {
		// do nothing
	} else if (isCanceledOkay and (state == RunnerCanceling)) {
		stateChangeSignal.wait(&signalMutex);
		state.hopefullyEqualTo(RunnerCanceled, HERE);
	} else {
		state.hopefullyEqualTo(RunnerPausing, HERE);
		stateChangeSignal.wait(&signalMutex);
		state.hopefullyInSet(RunnerPaused, RunnerFinished, HERE);
	}
	signalMutex.unlock();
}

void ThinkerRunner::requestCancelCore(bool isCanceledOkay, const codeplace& cp)
{
	getManager().hopefullyCurrentThreadIsManager(HERE);

	signalMutex.lock();
	// TODO: Should we introduce an Initializing transition that allows
	// going directly to paused or aborted, rather than pass through
	// Thinking?  It might lower some latency.
	if (state == RunnerInitializing) {
		stateChangeSignal.wait(&signalMutex);
	}
	if (state == RunnerFinished) {
		state.hopefullyTransition(RunnerFinished, RunnerCanceled, cp);
		stateChangeSignal.wakeOne();
	} else if (state == RunnerPaused) {
		state.hopefullyTransition(RunnerPaused, RunnerCanceled, cp);
		stateChangeSignal.wakeOne();
	} else if (isCanceledOkay and (state == RunnerCanceled)) {
		// do nothing
	} else {
		// No one can request a pause or stop besides the worker
		// We should not multiply request stops and pauses...
		// so if it's not initializing and not finished it must be thinking!
		state.hopefullyTransition(RunnerThinking, RunnerCanceling, cp);
		stateChangeSignal.wakeOne();

		emit breakEventLoop();
	}
	signalMutex.unlock();
}

void ThinkerRunner::waitForCancel()
{
	getManager().hopefullyCurrentThreadIsManager(HERE);

	signalMutex.lock();
	if (state == RunnerCanceling) {
		stateChangeSignal.wait(&signalMutex);
	}
	state.hopefullyEqualTo(RunnerCanceled, HERE);
	signalMutex.unlock();
}

void ThinkerRunner::requestResumeCore(bool isCanceledOkay, const codeplace& cp)
{
	getManager().hopefullyCurrentThreadIsManager(HERE);

	waitForPauseCore(isCanceledOkay);

	signalMutex.lock();
	if (state == RunnerFinished) {
		// do nothing
	} else if (isCanceledOkay and (state == RunnerCanceled)) {
		// do nothing
	} else {
		state.hopefullyTransition(RunnerPaused, RunnerResuming, cp);
		stateChangeSignal.wakeOne(); // only one person should be waiting on this, max...
		emit resumeThinking();
	}
	signalMutex.unlock();
}

void ThinkerRunner::waitForResume(const codeplace&cp)
{
	getManager().hopefullyCurrentThreadIsManager(HERE);

	signalMutex.lock();
	if ((state == RunnerThinking) or (state == RunnerFinished)) {
		// do nothing
	} else {
		state.hopefullyEqualTo(RunnerResuming, HERE);
		stateChangeSignal.wait(&signalMutex);
		state.hopefullyInSet(RunnerResuming, RunnerThinking, RunnerFinished, HERE);
	}
	signalMutex.unlock();
}

void ThinkerRunner::requestFinishAndWaitForFinish(const codeplace& cp)
{
	getManager().hopefullyCurrentThreadIsManager(HERE);

	signalMutex.lock();
	if (state == RunnerFinished) {
		// do nothing
	} else {
		// Caller should know if they paused the thinker, and resume it before
		// calling this routine!
		state.hopefullyEqualTo(RunnerThinking, HERE);
		stateChangeSignal.wait(&signalMutex);
		state.hopefullyEqualTo(RunnerFinished, HERE);
	}
	signalMutex.unlock();
}

bool ThinkerRunner::isFinished() const {
	getManager().hopefullyCurrentThreadIsManager(HERE);

	bool result (false);
	signalMutex.lock();
	switch (state) {
		case RunnerInitializing:
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
	signalMutex.unlock();
	return result;
}

bool ThinkerRunner::isCanceled() const {
	getManager().hopefullyCurrentThreadIsManager(HERE);

	bool result (false);
	signalMutex.lock();
	result = (state == RunnerCanceled) or (state == RunnerCanceling);
	signalMutex.unlock();
	return result;
}

bool ThinkerRunner::isPaused() const {
	getManager().hopefullyCurrentThreadIsManager(HERE);

	bool result (false);
	signalMutex.lock();
	result = (state == RunnerPaused) or (state == RunnerPausing);
	if (result) {
		tracked< bool > debugPausedThinkers (true, HERE);
		chronicle(debugPausedThinkers, "ThinkerBase was paused by " + state.whereLastAssigned().toString(), HERE);
	}
	signalMutex.unlock();
	return result;
}

bool ThinkerRunner::wasPauseRequested(unsigned long time) const
{
	hopefullyCurrentThreadIsPooled(HERE);

	bool result (false);

	signalMutex.lock();
	if ((state == RunnerPausing) or (state == RunnerCanceling)) {
		result = true;
	} else {
		state.hopefullyEqualTo(RunnerThinking, HERE);
		if (time == 0) {
			result = false;
		} else {
			bool didStateChange (stateChangeSignal.wait(&signalMutex, time));
			if (didStateChange)
				state.hopefullyInSet(RunnerPausing, RunnerCanceling, HERE);
			else
				state.hopefullyEqualTo(RunnerThinking, HERE); // should not have changed
			result = didStateChange;
		}
	}
	signalMutex.unlock();

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
	// Thread pool deletes this object.  There's no safe moment
	// to delete it on the manager thread unless you have called waitForDone()
	getManager().hopefullyCurrentThreadIsNotManager(HERE);

	state.hopefullyInSet(RunnerCanceled, RunnerFinished, HERE);
}