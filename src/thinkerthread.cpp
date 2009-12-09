//
// ThinkerThread.cpp
// This file is part of Thinker-Qt
// Copyright (C) 2009 HostileFork.com
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

#include "thinkerthread.h"
#include "thinkerqt/thinkermanager.h"

// operator << for ThinkerThread::State
//
// Required by tracked< > because it must be able to present a proper debug
// message about the tracked value.

inline QTextStream& operator << (QTextStream& o, const ThinkerThread::State& state)
{
	o << "ThinkerThread::";
	switch (state) {
	case ThinkerThread::Initializing:
		o << "Initializing";
		break;
	case ThinkerThread::Thinking:
		o << "Thinking";
		break;
	case ThinkerThread::Pausing:
		o << "Pausing";
		break;
	case ThinkerThread::Paused:
		o << "Paused";
		break;
	case ThinkerThread::Aborting:
		o << "Aborting";
		break;
	case ThinkerThread::Aborted:
		o << "Aborted";
		break;
	case ThinkerThread::Continuing:
		o << "Continuing";
		break;
	case ThinkerThread::Finished:
		o << "Finished";
		break;
	default:
		hopefullyNotReached(HERE);
	}
	return o;
}


//
// ThinkerThreadHelper
//

ThinkerThreadHelper::ThinkerThreadHelper(ThinkerThread* thread) :
	thread (thread)
{
	thread->getManager().hopefullyCurrentThreadIsThinker(HERE);
}

ThinkerThreadHelper::~ThinkerThreadHelper()
{
	thread->getManager().hopefullyCurrentThreadIsThinker(HERE);
}

void ThinkerThreadHelper::queuedQuit()
{
	thread->quit();
}

void ThinkerThreadHelper::markFinished()
{
	hopefully(QThread::currentThread() == thread, HERE);
	hopefully(thread->isRunning(), HERE);

	thread->signalMutex.lock();
	if (thread->state == ThinkerThread::Aborting) {
		// we don't let it transition to finished if abort is requested
	} else {
		thread->state.hopefullyInSet(ThinkerThread::Thinking, ThinkerThread::Pausing, HERE);
		thread->state.hopefullyAlter(ThinkerThread::Finished, HERE);
		thread->stateChangeSignal.wakeOne();
		thread->quit();
	}

	thread->signalMutex.unlock();
}


//
// ThinkerThread
//

ThinkerThread::ThinkerThread(QSharedPointer< ThinkerObject > thinker) :
	QThread (),
	state (Initializing, HERE),
	helper (),
	thinker (thinker),
	mapThinker (thinker.data(), this, thinker->getManager().getThreadMapManager(), HERE)
{
	getManager().hopefullyCurrentThreadIsManager(HERE);
	hopefully(not thinker.isNull(), HERE);

	getManager().hopefullyThreadIsManager(thinker->thread(), HERE);
	thinker->moveToThread(this);
	getManager().hopefullyThreadIsThinker(thinker->thread(), HERE);

	connect(this, SIGNAL(startThinking()), thinker.data(), SLOT(onStartThinking()), Qt::QueuedConnection);
	connect(this, SIGNAL(continueThinking()), thinker.data(), SLOT(onContinueThinking()), Qt::QueuedConnection);
	connect(thinker.data(), SIGNAL(madeProgress()), &thinker->getManager(), SIGNAL(madeProgress()), Qt::QueuedConnection);

	// TODO: The Thread object's affinity is the thread that created it.  This means that
	// if you connect a signal to the QThread, it will not be executed in that separate
	// thread of execution.  *HOWEVER* the "finished()" and "starting()" signals are
	// emitted from its independent thread of execution.  This is to say that if you were
	// to connect the thread's finished() signal to a slot on the thread, it would be
	// a queued connection.
	// I should research the handling of these signals, because right now they are
	// queued -- which means as long as the worker thread is doing something (e.g.
	// rendering, or regenerating decks) then the thread object will not be freed.
	connect(this, SIGNAL(finished()), &thinker->getManager(), SLOT(onThreadFinished()), Qt::QueuedConnection);

	start(); // need to think about thread pooling and other niceties
}

void ThinkerThread::run()
{
	helper = QSharedPointer< ThinkerThreadHelper >(new ThinkerThreadHelper(this));
	// The thinker thread needs to run until either it has finished (which it indicates by
	// emitting the finishedThinking signal)... or until it is canceled by the system.

	// The system notifies the thinker thread of its desire to interrupt processing in a
	// cooperative multitasking fashion.  The thinker must poll isPauseRequested()
	// periodically, and if it senses this it should return from whatever processing
	// it is doing.

	hopefully(getThinker().thread() == this, HERE); // must push thread affinity before calling start
	connect(this, SIGNAL(breakEventLoop()), getHelper(), SLOT(queuedQuit()), Qt::QueuedConnection);
	connect(&getThinker(), SIGNAL(thinkingFinished()), getHelper(), SLOT(markFinished()), Qt::DirectConnection);

	signalMutex.lock();
	state.hopefullyTransition(Initializing, Thinking, HERE);
	stateChangeSignal.wakeOne();
	signalMutex.unlock();

	thinker->afterThreadAttach();
	// in theory the thread might be detached any time the event loop is paused/etc.

	emit startThinking(); // should queue it so it's waiting for when exec() runs...

#ifndef Q_NO_EXCEPTIONS
	bool possiblyAbleToContinue (false);
#endif

	bool didAbortOrFinish (false);
	while (not didAbortOrFinish) {

		// there should be a startThinking() or continueThinking() message queued...

#ifndef Q_NO_EXCEPTIONS
		try {
#endif

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
		// a "Aborted" transition if the work the thinker has done was invalidated

		signalMutex.lock();

		if (state == Finished) {

			didAbortOrFinish = true;

		} else if (state == Aborting) {

			state.hopefullyTransition(Aborting, Aborted, HERE);
			stateChangeSignal.wakeOne();
			didAbortOrFinish = true;

		} else {

			state.hopefullyTransition(Pausing, Paused, HERE);
			stateChangeSignal.wakeOne();
			stateChangeSignal.wait(&signalMutex);

			// Once we are paused, we just wait for a signal that we are to
			// either be aborted or continue.  (Because we are paused
			// there is no need to pass through a "Aborting" state while
			// the event loop is still running.)

			if (state == Aborted) {
				didAbortOrFinish = true;
			} else {
#ifndef Q_NO_EXCEPTIONS
				// ...the Thinker may not have continueThinking implemented, even if
				// it returned cleanly from an isPauseRequested (instead of using
				// the exception variation)
				hopefully(possiblyAbleToContinue, HERE);
#endif
				state.hopefullyTransition(Continuing, Thinking, HERE);
				stateChangeSignal.wakeOne();
			}
		}

		signalMutex.unlock();
	}

	thinker->beforeThreadDetach();

	helper.clear();
}

void ThinkerThread::requestPauseCore(bool isAbortedOkay, const codeplace& cp)
{
	getManager().hopefullyCurrentThreadIsManager(HERE);

	signalMutex.lock();
	// TODO: Should we introduce an Initializing transition that allows
	// going directly to paused or aborted, rather than pass through
	// Thinking?  It might lower some latency.
	if (state == Initializing) {
		setPriority(QThread::NormalPriority); // raise it up if we slowed it down...
		stateChangeSignal.wait(&signalMutex);
	}
	if (state == Finished) {
		// do nothing
	} else if (isAbortedOkay and ((state == Aborting) or (state == Aborted))) {
		// do nothing
	} else {
		state.hopefullyTransition(Thinking, Pausing, cp);
		stateChangeSignal.wakeOne();
		setPriority(QThread::LowPriority);

		emit breakEventLoop();
	}
	signalMutex.unlock();
}

void ThinkerThread::waitForPauseCore(bool isAbortedOkay)
{
	getManager().hopefullyCurrentThreadIsManager(HERE);

	signalMutex.lock();
	if ((state == Finished) or (state == Paused)) {
		// do nothing
	} else if (isAbortedOkay and (state == Aborted)) {
		// do nothing
	} else if (isAbortedOkay and (state == Aborting)) {
		setPriority(QThread::NormalPriority);
		stateChangeSignal.wait(&signalMutex);
		state.hopefullyEqualTo(Aborted, HERE);
	} else {
		state.hopefullyEqualTo(Pausing, HERE);
		setPriority(QThread::NormalPriority); // raise it up if we slowed it down...
		stateChangeSignal.wait(&signalMutex);
		state.hopefullyInSet(Paused, Finished, HERE);
	}
	signalMutex.unlock();
}

void ThinkerThread::requestAbortCore(bool isAbortedOkay, const codeplace& cp)
{
	getManager().hopefullyCurrentThreadIsManager(HERE);

	signalMutex.lock();
	// TODO: Should we introduce an Initializing transition that allows
	// going directly to paused or aborted, rather than pass through
	// Thinking?  It might lower some latency.
	if (state == Initializing) {
		setPriority(QThread::NormalPriority); // raise it up if we slowed it down...
		stateChangeSignal.wait(&signalMutex);
	}
	if (state == Finished) {
		state.hopefullyTransition(Finished, Aborted, cp);
		stateChangeSignal.wakeOne();
	} else if (state == Paused) {
		state.hopefullyTransition(Paused, Aborted, cp);
		stateChangeSignal.wakeOne();
	} else if (isAbortedOkay and (state == Aborted)) {
		// do nothing
	} else {
		// No one can request a pause or stop besides the worker
		// We should not multiply request stops and pauses...
		// so if it's not initializing and not finished it must be thinking!
		state.hopefullyTransition(Thinking, Aborting, cp);
		stateChangeSignal.wakeOne();
		setPriority(QThread::LowPriority);

		emit breakEventLoop();
	}
	signalMutex.unlock();
}

void ThinkerThread::waitForAbort()
{
	getManager().hopefullyCurrentThreadIsManager(HERE);

	signalMutex.lock();
	if (state == Aborting) {
		setPriority(QThread::NormalPriority); // raise it up if we slowed it down...
		stateChangeSignal.wait(&signalMutex);
	}
	state.hopefullyEqualTo(Aborted, HERE);
	signalMutex.unlock();
}

void ThinkerThread::requestResumeCore(bool isAbortedOkay, const codeplace& cp)
{
	getManager().hopefullyCurrentThreadIsManager(HERE);

	waitForPauseCore(isAbortedOkay);

	signalMutex.lock();
	if (state == Finished) {
		// do nothing
	} else if (isAbortedOkay and (state == Aborted)) {
		// do nothing
	} else {
		state.hopefullyTransition(Paused, Continuing, cp);
		stateChangeSignal.wakeOne(); // only one person should be waiting on this, max...
		hopefully(isRunning(), cp);
		setPriority(QThread::NormalPriority);
		emit continueThinking();
	}
	signalMutex.unlock();
}

void ThinkerThread::waitForResume(const codeplace&cp)
{
	getManager().hopefullyCurrentThreadIsManager(HERE);

	signalMutex.lock();
	if ((state == Thinking) or (state == Finished)) {
		// do nothing
	} else {
		state.hopefullyEqualTo(Continuing, HERE);
		setPriority(QThread::NormalPriority); // raise it up if we slowed it down...
		stateChangeSignal.wait(&signalMutex);
		state.hopefullyInSet(Continuing, Thinking, Finished, HERE);
	}
	signalMutex.unlock();
}

void ThinkerThread::requestFinishAndWaitForFinish(const codeplace& cp)
{
	getManager().hopefullyCurrentThreadIsManager(HERE);

	signalMutex.lock();
	if (state == Finished) {
		// do nothing
	} else {
		// Caller should know if they paused the thinker, and resume it before
		// calling this routine!
		state.hopefullyEqualTo(Thinking, HERE);
		setPriority(QThread::NormalPriority); // raise it up if we slowed it down...
		stateChangeSignal.wait(&signalMutex);
		state.hopefullyEqualTo(Finished, HERE);
	}
	signalMutex.unlock();
}

bool ThinkerThread::isComplete() const {
	getManager().hopefullyCurrentThreadIsManager(HERE);

	bool result (false);
	signalMutex.lock();
	switch (state) {
		case Initializing:
		case Thinking:
		case Pausing:
		case Paused:
		case Continuing:
			result = false;
			break;
		case Finished:
			result = true;
			break;
		case Aborted:
			// used to return indeterminate here but removed tribool/boost dependency
			result = false;
			break;
		default:
			hopefullyNotReached(HERE);
	}
	signalMutex.unlock();
	return result;
}

bool ThinkerThread::isAborted() const {
	getManager().hopefullyCurrentThreadIsManager(HERE);

	bool result (false);
	signalMutex.lock();
	result = (state == Aborted);
	signalMutex.unlock();
	return result;
}

bool ThinkerThread::isPaused() const {
	getManager().hopefullyCurrentThreadIsManager(HERE);

	bool result (false);
	signalMutex.lock();
	result = (state == Paused);
	if (result) {
		tracked< bool > debugPausedThinkers (true, HERE);
		chronicle(debugPausedThinkers, "ThinkerObject was paused by " + state.whereLastAssigned().toString(), HERE);
	}
	signalMutex.unlock();
	return result;
}

bool ThinkerThread::isPauseRequested(unsigned long time) const
{
	hopefully(QThread::currentThread() == this, HERE);

	bool result (false);

	signalMutex.lock();
	if ((state == Pausing) or (state == Aborting)) {
		result = true;
	} else {
		state.hopefullyEqualTo(Thinking, HERE);
		if (time == 0) {
			result = false;
		} else {
			bool didStateChange (stateChangeSignal.wait(&signalMutex, time));
			if (didStateChange)
				state.hopefullyInSet(Pausing, Aborting, HERE);
			else
				state.hopefullyEqualTo(Thinking, HERE); // should not have changed
			result = didStateChange;
		}
	}
	signalMutex.unlock();

	return result;
}

#ifndef Q_NO_EXCEPTIONS
void ThinkerThread::pollForStopException(unsigned long time) const
{
	if (isPauseRequested(time))
		throw ThinkerThread::StopException ();
}
#endif

ThinkerThread::~ThinkerThread()
{
	getManager().hopefullyCurrentThreadIsManager(HERE);

	hopefully(wait(), HERE);
	hopefully(not isRunning(), HERE);
	state.hopefullyInSet(Aborted, Finished, HERE);
}