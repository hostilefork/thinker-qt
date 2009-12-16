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

#include "thinkerthread.h"
#include "thinkerqt/thinkermanager.h"

//
// ThinkerRunner
//

ThinkerRunnerBase::ThinkerRunnerBase (QSharedPointer< ThinkerObject > thinker) :
	thinker (thinker)
{
}

bool ThinkerRunnerBase::hopefullyCurrentThreadIsManager(const codeplace& cp)
{
	return thinker->getManager().hopefullyCurrentThreadIsManager(cp);
}

ThinkerObject& ThinkerRunnerBase::getThinkerBase() {
	hopefullyCurrentThreadIsManager(HERE);
	return *thinker;
}

QSharedPointer< SnapshotBase > ThinkerRunnerBase::makeSnapshotBase()
{
	hopefullyCurrentThreadIsManager(HERE);
	return static_cast< SnapshottableBase* >(thinker.data())->makeSnapshotBase();
}

ThinkerRunnerBase::~ThinkerRunnerBase()
{
	// We need to be able to map descriptors to thinkers, but the problem is that
	// a thinker thread still running which we haven't been able to terminate yet
	// may exist for a descriptor that we now have a *new* thinker for.  So we
	// have to clean up any structures that treat this thinker as relevant when
	// we might allocate a new one...
	thinker->beforeRunnerDetach();

	// we leave _wasAttachedToRunner as true here...
	// it may still be running and we want readable()/writable() to still work
	// from the thinker thread

	ThinkerThread* thinkerThread (thinker->getManager().maybeGetThreadForThinker(*thinker));
	if (thinkerThread != NULL) {
		// No need to enforceAbort at this point (which would cause a
		// synchronous pause of the worker thread that we'd like to avoid)
		// ...although unruly thinkers may seem to "leak" if they stall too
		// long before responding to isPauseRequested() signals

		// The bulkhead may have become invalidated which means it
		// could already be aborted.
		thinkerThread->requestAbortButAlreadyAbortedIsOkay(HERE);
	}
}


//
// ThinkerManager
//

ThinkerManager::ThinkerManager () :
	QObject ()
{
	hopefullyCurrentThreadIsManager(HERE);
}

void ThinkerManager::createThreadForThinker(QSharedPointer< ThinkerObject > thinker)
{
	// this may look like a bad idea because we are dynamically allocating and not hanging onto
	// the result so we can free it.  but it's okay because ThinkerThreads maintain a global table
	// which they insert themselves into on construction and delete themselves from during
	// destruction.  Also, when the thread emits the finished signal, we clean it up using deleteLater
	// (it is freed by the event loop when all events have been processed)
	static_cast< void* >(new ThinkerThread (thinker));
}

void ThinkerManager::ensureThinkersPaused(const codeplace& cp)
{
	hopefullyCurrentThreadIsManager(HERE);

	QMap< const ThinkerObject*, tracked< ThinkerThread* > > map (threadMapManager.getMap());
	QMapIterator< const ThinkerObject*, tracked< ThinkerThread* > > i (map);

	// First pass: request all thinkers to pause (accept it if they are aborting, as they
	// may be freed by the ThinkerRunner but not yet returned).
	while (i.hasNext()) {
		i.next();
		ThinkerThread* thread (i.value());
		thread->requestPauseButAbortedIsOkay(cp);
	}

	i.toFront();

	// Second pass: wait for all the thinkers to actually get their code off the stack.
	while (i.hasNext()) {
		i.next();
		ThinkerThread* thread (i.value());
		thread->waitForPauseButAbortedIsOkay();
	}
}

void ThinkerManager::ensureThinkersResumed(const codeplace& cp)
{
	hopefullyCurrentThreadIsManager(HERE);

	// any thinkers that have not been aborted can be resumed
	QMap< const ThinkerObject*, tracked< ThinkerThread* > > map (threadMapManager.getMap());
	QMapIterator< const ThinkerObject*, tracked< ThinkerThread* > > i (map);
	while (i.hasNext()) {
		i.next();
		ThinkerThread* thread (i.value());
		thread->requestResumeButAbortedIsOkay(cp);
	}
}

const ThinkerThread* ThinkerManager::maybeCastToThinkerThread(const QThread* thread)
{
	hopefully(thread != NULL, HERE);
	const ThinkerThread* result (dynamic_cast< const ThinkerThread* >(thread));
	return result;
}

ThinkerThread* ThinkerManager::maybeGetThreadForThinker(const ThinkerObject& thinker)
{
	QMap< const ThinkerObject*, tracked< ThinkerThread* > > map (threadMapManager.getMap());
	QMap< const ThinkerObject*, tracked< ThinkerThread* > >::iterator i (map.find(&thinker));
	if (i != map.end())
		return i.value();
	return NULL;
}

ThinkerObject& ThinkerManager::getThinkerForThread(const ThinkerThread* thinkerThread)
{
	hopefully(thinkerThread != NULL, HERE);
	// not the most evil const_cast ever written, as we are the "thinker manager"
	// better to keep non-const ThinkerThread handles out of client code
	return const_cast< ThinkerThread* >(thinkerThread)->getThinker();
}

void ThinkerManager::requestAndWaitForAbortButAlreadyAbortedIsOkay(ThinkerObject& thinker)
{
	ThinkerThread* thinkerThread (maybeGetThreadForThinker(thinker));
	if(thinkerThread == NULL) {
		thinker.state = ThinkerObject::Aborted;
	} else {
		// thread should be paused or finished... or possibly aborted
		thinkerThread->requestAbortButAlreadyAbortedIsOkay(HERE);
		thinkerThread->waitForAbort();
		// manager's responsibility to update the thinker's state, not directly sync'd to
		// thread state (only when thread is terminated).
		thinker.state = ThinkerObject::Aborted;
	}
	hopefully(thinker.state == ThinkerObject::Aborted, HERE);
}

void ThinkerManager::ensureThinkerFinished(ThinkerObject& thinker)
{
	hopefullyCurrentThreadIsManager(HERE);

	if (thinker.state != ThinkerObject::DoneThinking) {
		ThinkerThread* thinkerThread (maybeGetThreadForThinker(thinker));
		if (thinkerThread != NULL) {
			hopefully(not thinkerThread->isAborted(), HERE); // can't finish if it's aborted or invalid!

			// make sure thread is resumed and finishes...
			bool threadComplete (thinkerThread->isComplete());

			// we need to watch the state changes and ensure that
			// it completes... note user cancellation would mean that it
			// would not so we have to allow for that case!
			thinkerThread->requestResume(HERE);
			thinkerThread->waitForResume(HERE);
			thinkerThread->requestFinishAndWaitForFinish(HERE);
		}
	}

	hopefully(thinker.state != ThinkerObject::Aborted, HERE); // can't finish if it's aborted or invalid!

	if (false) {
		// It would be nice if this completion signal could be true by the time we reach here
		// unfortunately this is not set until the message loop runs... hmmm.

		hopefully(thinker.state == ThinkerObject::DoneThinking, HERE);
	}
}

void ThinkerManager::throttleNotificationFrequency(ThinkerObject& thinker, unsigned int milliseconds)
{
	thinker.progressThrottler->setMillisecondsDefault(milliseconds);
}

void ThinkerManager::onThreadFinished()
{
	hopefullyCurrentThreadIsManager(HERE);
	hopefully(sender() != NULL, HERE);
	ThinkerThread& thread (*cast_hopefully< ThinkerThread* >(sender(), HERE));
	ThinkerObject& thinker (thread.getThinker());

	if (thread.isAborted()) {
		thinker.state = ThinkerObject::Aborted;
	} else if (thread.isComplete()) {
		thinker.state = ThinkerObject::DoneThinking;
	} else {
		hopefullyNotReached(HERE);
	}
	thread.deleteLater();
}

ThinkerManager::~ThinkerManager()
{
	hopefullyCurrentThreadIsManager(HERE);

	// By this point, all the ThinkerRunners should be freed.
	// This means all remaining threads should be in the aborted state
	// We wait for them to finish

	QMap< const ThinkerObject*, tracked< ThinkerThread* > > map (threadMapManager.getMap());
	QMapIterator< const ThinkerObject*, tracked< ThinkerThread* > > i (map);
	while (i.hasNext()) {
		i.next();
		ThinkerThread* thread (i.value());
		thread->waitForAbort();
	}
}