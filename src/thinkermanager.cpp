//
// ThinkerManager.cpp
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

#include <QThreadPool>

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
	QObject ()
{
	hopefullyCurrentThreadIsManager(HERE);

	// request cancel of all the threads and wait on them...
	connect(QCoreApplication::instance(), SIGNAL(aboutToQuit()), this, SLOT(onAboutToQuit()));
}

ThinkerManager *ThinkerManager::globalInstance()
{
	return theInstance();
}

void ThinkerManager::createRunnerForThinker(ThinkerHolder< ThinkerObject > holder, const codeplace& cp)
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
	QThreadPool::globalInstance()->start(runner);
}

void ThinkerManager::ensureThinkersPaused(const codeplace& cp)
{
	hopefullyCurrentThreadIsManager(HERE);

	QMap< const ThinkerObject*, tracked< ThinkerRunner* > > map (runnerMapManager.getMap());
	QMapIterator< const ThinkerObject*, tracked< ThinkerRunner* > > i (map);

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

	// any thinkers that have not been aborted can be resumed
	QMap< const ThinkerObject*, tracked< ThinkerRunner* > > map (runnerMapManager.getMap());
	QMapIterator< const ThinkerObject*, tracked< ThinkerRunner* > > i (map);
	while (i.hasNext()) {
		i.next();
		ThinkerRunner* runner (i.value());
		runner->requestResumeButCanceledIsOkay(cp);
	}
}

const ThinkerRunner* ThinkerManager::maybeGetRunnerForThread(const QThread* thread)
{
	hopefully(thread != NULL, HERE);
	// TODO: Look into race condition here
	const ThinkerRunner* result (threadMapManager.lookupValue(thread, NULL));
	return result;
}

ThinkerRunner* ThinkerManager::maybeGetRunnerForThinker(const ThinkerObject& thinker)
{
	QMap< const ThinkerObject*, tracked< ThinkerRunner* > > map (runnerMapManager.getMap());
	QMap< const ThinkerObject*, tracked< ThinkerRunner* > >::iterator i (map.find(&thinker));
	if (i != map.end())
		return i.value();
	return NULL;
}

ThinkerObject& ThinkerManager::getThinkerForRunner(const ThinkerRunner* runner)
{
	hopefully(runner != NULL, HERE);
	// not the most evil const_cast ever written, as we are the "thinker manager"
	// better to keep non-const ThinkerRunner handles out of client code
	return const_cast< ThinkerRunner* >(runner)->getThinker();
}

void ThinkerManager::requestAndWaitForCancelButAlreadyCanceledIsOkay(ThinkerObject& thinker)
{
	ThinkerRunner* thinkerRunner (maybeGetRunnerForThinker(thinker));
	if(thinkerRunner == NULL) {
		thinker.state = ThinkerObject::ThinkerCanceled;
	} else {
		// thread should be paused or finished... or possibly aborted
		thinkerRunner->requestCancelButAlreadyCanceledIsOkay(HERE);
		thinkerRunner->waitForCancel();
		// manager's responsibility to update the thinker's state, not directly sync'd to
		// thread state (only when thread is terminated).
		thinker.state = ThinkerObject::ThinkerCanceled;
	}
	hopefully(thinker.state == ThinkerObject::ThinkerCanceled, HERE);
}

void ThinkerManager::ensureThinkerFinished(ThinkerObject& thinker)
{
	hopefullyCurrentThreadIsManager(HERE);

	if (thinker.state != ThinkerObject::ThinkerFinished) {
		ThinkerRunner* thinkerRunner (maybeGetRunnerForThinker(thinker));
		if (thinkerRunner != NULL) {
			hopefully(not thinkerRunner->isCanceled(), HERE); // can't finish if it's aborted or invalid!

			// we need to watch the state changes and ensure that
			// it completes... note user cancellation would mean that it
			// would not so we have to allow for that case!
			thinkerRunner->requestResume(HERE);
			thinkerRunner->waitForResume(HERE);
			thinkerRunner->requestFinishAndWaitForFinish(HERE);
		}
	}

	hopefully(thinker.state != ThinkerObject::ThinkerCanceled, HERE); // can't finish if it's aborted or invalid!

	if (false) {
		// It would be nice if this completion signal could be true by the time we reach here
		// unfortunately this is not set until the message loop runs... hmmm.

		hopefully(thinker.state == ThinkerObject::ThinkerFinished, HERE);
	}
}

void ThinkerManager::onRunnerFinished(ThinkerObject* thinker, bool canceled)
{
	hopefullyCurrentThreadIsManager(HERE);
	thinker->state = canceled ? ThinkerObject::ThinkerCanceled : ThinkerObject::ThinkerFinished;
}

void ThinkerManager::onAboutToQuit()
{
	bool anyRunners (false);

	// This is not safe because we can't really iterate the collection on
	// the manager thread.  In fact, I've got to do something about the race
	// condition w/that, as Runners can just up-and-delete themselves at
	// an arbitrary moment on their own thread.  The map needs to be
	// protected by a mutex.
	QMap< const ThinkerObject*, tracked< ThinkerRunner* > > map (runnerMapManager.getMap());
	QMapIterator< const ThinkerObject*, tracked< ThinkerRunner* > > i (map);
	while (i.hasNext()) {
		i.next();
		ThinkerRunner* runner (i.value());
		hopefully(runner->isCanceled() or runner->isFinished(), HERE);
		anyRunners = true;
	}

	if (anyRunners)
		QThreadPool::globalInstance()->waitForDone();
}

void ThinkerManager::unlockThinker(ThinkerObject& thinker)
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
	/* notificationThrottler->emitThrottled(); */
}

ThinkerManager::~ThinkerManager()
{
	hopefullyCurrentThreadIsManager(HERE);

	// We catch you with an assertion if you do not make sure all your
	// Presents have been either canceled or completed
	onAboutToQuit();
}