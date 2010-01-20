//
// ThinkerManager.cpp
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

#include <QThreadPool>
#include <QMutexLocker>

#include "thinkerrunner.h"
#include "thinkerqt/thinkermanager.h"

//
// Q_GLOBAL_STATIC is an Internal Qt Macro, not part of the public API
// You can use it... but it may change without notice in future versions
//

Q_GLOBAL_STATIC(ThinkerManager, theInstance)


//
// ThinkerManager
//

ThinkerManager::ThinkerManager () :
	QObject (),
	anyThinkerWrittenThrottler (400)
{
	hopefullyCurrentThreadIsManager(HERE);

	connect(&anyThinkerWrittenThrottler, SIGNAL(throttled()), this, SIGNAL(anyThinkerWritten()), Qt::DirectConnection);
}

bool ThinkerManager::hopefullyThreadIsManager(const QThread& thread, const codeplace& cp)
{
	return hopefully(&thread == this->thread(), cp);
}

bool ThinkerManager::hopefullyCurrentThreadIsManager(const codeplace& cp)
{
	return hopefullyThreadIsManager(*QThread::currentThread(), cp);
}

bool ThinkerManager::hopefullyThreadIsNotManager(const QThread& thread, const codeplace& cp)
{
	return hopefully(&thread != this->thread(), cp);
}

bool ThinkerManager::hopefullyCurrentThreadIsNotManager(const codeplace& cp)
{
	return hopefullyThreadIsNotManager(*QThread::currentThread(), cp);
}

bool ThinkerManager::hopefullyThreadIsThinker(const QThread& thread, const codeplace& cp)
{
	return hopefully(not maybeGetRunnerForThread(thread).isNull(), cp);
}

bool ThinkerManager::hopefullyCurrentThreadIsThinker(const codeplace& cp)
{
	return hopefullyThreadIsThinker(*QThread::currentThread(), cp);
}

ThinkerManager *ThinkerManager::globalInstance()
{
	return theInstance();
}

void ThinkerManager::createRunnerForThinker(ThinkerHolder< ThinkerBase > holder, const codeplace& cp)
{
	hopefullyCurrentThreadIsManager(cp);
	hopefully(not holder.isNull(), cp);

	ThinkerRunner* runner (new ThinkerRunner (holder));

	// this may look like a bad idea because we are dynamically allocating and not hanging onto
	// the result so we can free it.  but it's okay because ThinkerRunners maintain a global table
	// which they insert themselves into on construction and delete themselves from during
	// destruction.  Also, when the thread emits the finished signal, we clean it up using deleteLater
	// (it is freed by the event loop when all events have been processed)
	runner->setAutoDelete(true);

	// QtConcurrent defines one global thread pool instance but maybe I'll let you specify your
	// own, not sure if that's useful.  They make a lot of global assumptions, perhaps I should
	// just piggy back on them.

	// Queue this runnable thing to the thread pool.  It may take a while before
	// a thread gets allocated to it.
	static_cast<void>(QThreadPool::globalInstance()->start(runner));
}

void ThinkerManager::ensureThinkersPaused(const codeplace& cp)
{
	hopefullyCurrentThreadIsManager(HERE);

	QMutexLocker locker (&mapsMutex);

	QMapIterator< const ThinkerBase*, ThinkerRunner* > i (thinkerMap);

	// First pass: request all thinkers to pause (accept it if they are aborting, as they
	// may be freed by the ThinkerPresent but not yet returned).
	while (i.hasNext()) {
		i.next();
		ThinkerRunner* runner (i.value());
		runner->requestPauseButCanceledIsOkay(cp);
	}

	i.toFront();

	// Second pass: wait for all the thinkers to actually get their code off the stack.
	while (i.hasNext()) {
		i.next();
		ThinkerRunner* runner (i.value());
		runner->waitForPauseButCanceledIsOkay();
	}
}

void ThinkerManager::ensureThinkersResumed(const codeplace& cp)
{
	hopefullyCurrentThreadIsManager(HERE);

	QMutexLocker locker (&mapsMutex);

	// any thinkers that have not been aborted can be resumed
	QMapIterator< const ThinkerBase*, ThinkerRunner* > i (thinkerMap);
	while (i.hasNext()) {
		i.next();
		ThinkerRunner* runner (i.value());
		runner->requestResumeButCanceledIsOkay(cp);
	}
}

ThinkerRunnerKeepalive ThinkerManager::maybeGetRunnerForThread(const QThread& thread)
{
	mapsMutex.lock();
	ThinkerRunner* result (threadMap.value(&thread, NULL));
	if (result != NULL) {
		return ThinkerRunnerKeepalive (*result); // leave mutex locked
	}
	return ThinkerRunnerKeepalive ();
}

ThinkerRunnerKeepalive ThinkerManager::maybeGetRunnerForThinker(const ThinkerBase& thinker)
{
	mapsMutex.lock();

	QMap< const ThinkerBase*, ThinkerRunner* >::iterator i (thinkerMap.find(&thinker));
	if (i != thinkerMap.end()) {
		return ThinkerRunnerKeepalive (*i.value()); // leave mutex locked
	}
	mapsMutex.unlock();
	return ThinkerRunnerKeepalive ();
}

const ThinkerBase* ThinkerManager::maybeGetThinkerForThread(const QThread& thread)
{
	ThinkerRunnerKeepalive runner (maybeGetRunnerForThread(thread));
	if (runner.isNull()) {
		return NULL;
	}
	return &(runner->getThinker());
}

void ThinkerManager::requestAndWaitForCancelButAlreadyCanceledIsOkay(ThinkerBase& thinker)
{
	ThinkerRunnerKeepalive runner (maybeGetRunnerForThinker(thinker));
	if(runner.isNull()) {
		thinker.state = ThinkerBase::ThinkerCanceled;
	} else {
		// thread should be paused or finished... or possibly aborted
		runner->requestCancelButAlreadyCanceledIsOkay(HERE);
		runner->waitForCancel();
		// manager's responsibility to update the thinker's state, not directly sync'd to
		// thread state (only when thread is terminated).
		thinker.state = ThinkerBase::ThinkerCanceled;
	}
	hopefully(thinker.state == ThinkerBase::ThinkerCanceled, HERE);
}

void ThinkerManager::ensureThinkerFinished(ThinkerBase& thinker)
{
	hopefullyCurrentThreadIsManager(HERE);

	if (thinker.state != ThinkerBase::ThinkerFinished) {
		ThinkerRunnerKeepalive runner (maybeGetRunnerForThinker(thinker));
		if (not runner.isNull()) {
			hopefully(not runner->isCanceled(), HERE); // can't finish if it's aborted or invalid!

			// we need to watch the state changes and ensure that
			// it completes... note user cancellation would mean that it
			// would not so we have to allow for that case!
			runner->requestResume(HERE);
			runner->waitForResume(HERE);
			runner->requestFinishAndWaitForFinish(HERE);
		}
	}

	hopefully(thinker.state != ThinkerBase::ThinkerCanceled, HERE); // can't finish if it's aborted or invalid!

	if (false) {
		// It would be nice if this completion signal could be true by the time we reach here
		// unfortunately this is not set until the message loop runs... hmmm.

		hopefully(thinker.state == ThinkerBase::ThinkerFinished, HERE);
	}
}

void ThinkerManager::onRunnerFinished(ThinkerBase* thinker, bool canceled)
{
	hopefullyCurrentThreadIsManager(HERE);
	thinker->state = canceled ? ThinkerBase::ThinkerCanceled : ThinkerBase::ThinkerFinished;
}

void ThinkerManager::unlockThinker(ThinkerBase& thinker)
{
	// do throttled emit to all the ThinkerPresentWatchers
	thinker.watchersLock.lockForRead();
	QSetIterator<ThinkerPresentWatcherBase*> i (thinker.watchers);
	while (i.hasNext()) {
		i.next()->notificationThrottler->emitThrottled();
	}
	thinker.watchersLock.unlock();

	// there is a notification throttler for all thinkers.  Review: should it be
	// possible to have a separate notification for groups?
	anyThinkerWrittenThrottler.emitThrottled();
}

bool ThinkerManager::maybeAddToRunnerMap(ThinkerRunner& runner)
{
	// We use a mutex to guard the addition and removal of Runners to the maps
	// If a Runner exists, then we look to its state information for cancellation--not
	// the Thinker.

	QMutexLocker locker (&mapsMutex);
	ThinkerBase& thinker (runner.getThinker());
	if (thinker.state == ThinkerBase::ThinkerCanceled) {
		return false;
	}

	hopefully(not threadMap.contains(thinker.thread()), HERE);
	threadMap.insert(thinker.thread(), &runner);

	hopefully(not thinkerMap.contains(&thinker), HERE);
	thinkerMap.insert(&thinker, &runner);
	return true;
}

void ThinkerManager::removeFromRunnerMap(ThinkerRunner& runner)
{
	QMutexLocker locker (&mapsMutex);
	ThinkerBase& thinker (runner.getThinker());

	hopefully(threadMap.remove(thinker.thread()) == 1, HERE);
	hopefully(thinkerMap.remove(&thinker) == 1, HERE);
}

ThinkerManager::~ThinkerManager()
{
	hopefullyCurrentThreadIsManager(HERE);

	// We catch you with an assertion if you do not make sure all your
	// Presents have been either canceled or completed
	bool anyRunners (false);

	if (true) {
		QMutexLocker locker (&mapsMutex);
		QMapIterator< const ThinkerBase*, ThinkerRunner* > i (thinkerMap);
		while (i.hasNext()) {
			i.next();
			ThinkerRunner* runner (i.value());
			hopefully(runner->isCanceled() or runner->isFinished(), HERE);
			anyRunners = true;
		}
	}

	if (anyRunners)
		QThreadPool::globalInstance()->waitForDone();
}