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
	connect(this, SIGNAL(pushToThreadMayBeNeeded()),
		this, SLOT(doThreadPushesIfNecessary()), Qt::QueuedConnection);

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

	QSharedPointer<ThinkerRunner> runner (new ThinkerRunner (holder));
	ThinkerRunnerProxy* runnerProxy (new ThinkerRunnerProxy (runner));

	// this may look like a bad idea because we are dynamically allocating and not hanging onto
	// the result so we can free it.  but it's okay because ThinkerRunners maintain a global table
	// which they insert themselves into on construction and delete themselves from during
	// destruction.  Also, when the thread emits the finished signal, we clean it up using deleteLater
	// (it is freed by the event loop when all events have been processed)
	runnerProxy->setAutoDelete(true);

	// QtConcurrent defines one global thread pool instance but maybe I'll let you specify your
	// own, not sure if that's useful.  They make a lot of global assumptions, perhaps I should
	// just piggy back on them.

	// Queue this runnable thing to the thread pool.  It may take a while before
	// a thread gets allocated to it.
	static_cast<void>(QThreadPool::globalInstance()->start(runnerProxy));
}

void ThinkerManager::ensureThinkersPaused(const codeplace& cp)
{
	hopefullyCurrentThreadIsManager(HERE);

	QMutexLocker locker (&mapsMutex);
	// we have to make a copy of the map
    QMap<const ThinkerBase*, QSharedPointer<ThinkerRunner> > mapCopy (thinkerMap);
	locker.unlock();

    QMapIterator<const ThinkerBase*, QSharedPointer<ThinkerRunner> > i (mapCopy);

	// First pass: request all thinkers to pause (accept it if they are aborting, as they
	// may be freed by the ThinkerPresent but not yet returned).
	while (i.hasNext()) {
		i.next();
        QSharedPointer<ThinkerRunner> runner (i.value());
		runner->requestPauseButCanceledIsOkay(cp);
	}

	i.toFront();

	// Second pass: wait for all the thinkers to actually get their code off the stack.
	while (i.hasNext()) {
		i.next();
        QSharedPointer<ThinkerRunner> runner (i.value());
		runner->waitForPauseButCanceledIsOkay();
	}
}

void ThinkerManager::ensureThinkersResumed(const codeplace& cp)
{
	hopefullyCurrentThreadIsManager(HERE);

	QMutexLocker locker (&mapsMutex);

	// any thinkers that have not been aborted can be resumed
    QMapIterator<const ThinkerBase*, QSharedPointer<ThinkerRunner> > i (thinkerMap);
	while (i.hasNext()) {
		i.next();
        QSharedPointer<ThinkerRunner> runner (i.value());
		if (runner->isPaused())
			runner->requestResumeButCanceledIsOkay(cp);
	}
}

QSharedPointer<ThinkerRunner> ThinkerManager::maybeGetRunnerForThread(const QThread& thread)
{
	QMutexLocker locker (&mapsMutex);
    QSharedPointer<ThinkerRunner> result (threadMap.value(&thread, QSharedPointer<ThinkerRunner> ()));
	return result;
}

QSharedPointer< ThinkerRunner > ThinkerManager::maybeGetRunnerForThinker(const ThinkerBase& thinker)
{
	QMutexLocker locker (&mapsMutex);
    QSharedPointer<ThinkerRunner> result (thinkerMap.value(&thinker, QSharedPointer<ThinkerRunner> ()));
	if (result.isNull())
		hopefully((thinker.state == ThinkerBase::ThinkerCanceled) || (thinker.state == ThinkerBase::ThinkerFinished), HERE);
	return result;
}

const ThinkerBase* ThinkerManager::maybeGetThinkerForThread(const QThread& thread)
{
    QSharedPointer<ThinkerRunner> runner (maybeGetRunnerForThread(thread));
	if (runner.isNull()) {
		return NULL;
	}
	return &(runner->getThinker());
}

void ThinkerManager::requestAndWaitForCancelButAlreadyCanceledIsOkay(ThinkerBase& thinker)
{
	QSharedPointer< ThinkerRunner > runner (maybeGetRunnerForThinker(thinker));
	if(runner.isNull()) {
		thinker.state = ThinkerBase::ThinkerCanceled;
	} else {
		// thread should be paused or finished... or possibly aborted
		runner->requestCancelButAlreadyCanceledIsOkay(HERE);
		runner->waitForFinished(HERE);
	}
	hopefully(thinker.state == ThinkerBase::ThinkerCanceled, HERE);
}

void ThinkerManager::ensureThinkerFinished(ThinkerBase& thinker)
{
	hopefullyCurrentThreadIsManager(HERE);

    QSharedPointer<ThinkerRunner> runner (maybeGetRunnerForThinker(thinker));
	if (not runner.isNull()) {
		hopefully(not runner->isCanceled(), HERE); // can't finish if it's aborted or invalid!

		// we need to watch the state changes and ensure that
		// it completes... note user cancellation would mean that it
		// would not so we have to allow for that case!
		if (runner->isPaused()) {
			runner->requestResume(HERE);
			runner->waitForResume(HERE);
		}

		runner->waitForFinished(HERE);
	}

	hopefully(thinker.state == ThinkerBase::ThinkerFinished, HERE);
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

void ThinkerManager::addToThinkerMap(QSharedPointer<ThinkerRunner> runner)
{
	// We use a mutex to guard the addition and removal of Runners to the maps
	// If a Runner exists, then we look to its state information for cancellation--not
	// the Thinker.

	QMutexLocker locker (&mapsMutex);
	ThinkerBase& thinker (runner->getThinker());
	hopefully(not thinkerMap.contains(&thinker), HERE);
	thinkerMap.insert(&thinker, runner);
}

void ThinkerManager::removeFromThinkerMap(QSharedPointer<ThinkerRunner> runner, bool wasCanceled)
{
	QMutexLocker locker (&mapsMutex);
	ThinkerBase& thinker (runner->getThinker());
	hopefully(thinkerMap.remove(&thinker) == 1, HERE);
	hopefully(thinker.state == ThinkerBase::ThinkerOwnedByRunner, HERE);
	thinker.state = wasCanceled ? ThinkerBase::ThinkerCanceled : ThinkerBase::ThinkerFinished;
}

void ThinkerManager::addToThreadMap(QSharedPointer<ThinkerRunner> runner, QThread& thread)
{
	QMutexLocker locker (&mapsMutex);
	hopefully(not threadMap.contains(&thread), HERE);
	threadMap.insert(&thread, runner);
}

void ThinkerManager::removeFromThreadMap(QSharedPointer<ThinkerRunner> /*runner*/, QThread& thread)
{
	QMutexLocker locker (&mapsMutex);
	hopefully(threadMap.remove(&thread) == 1, HERE);
}

void ThinkerManager::waitForPushToThread(ThinkerRunner* runner)
{
	hopefullyCurrentThreadIsNotManager(HERE);

	QMutexLocker locker (&pushThreadMutex);
	runnerSetToPush.insert(runner);
	threadsNeedPushing.wakeOne();
	emit pushToThreadMayBeNeeded();
	threadsWerePushed.wait(&pushThreadMutex);
}

void ThinkerManager::processThreadPushesUntil(ThinkerRunner* runner)
{
	hopefullyCurrentThreadIsManager(HERE);

	QMutexLocker locker (&pushThreadMutex);
	forever {
		bool found = false;
		foreach (ThinkerRunner* runnerToPush, runnerSetToPush) {
     			runnerToPush->doThreadPushIfNecessary();
			if ((NULL != runner) && (runnerToPush == runner))
				found = true;
		}
		runnerSetToPush.clear();
		threadsWerePushed.wakeAll();
		if (found || runner == NULL)
			return;
		threadsNeedPushing.wait(&pushThreadMutex);
	}
}

void ThinkerManager::doThreadPushesIfNecessary()
{
	processThreadPushesUntil(NULL);
}

ThinkerManager::~ThinkerManager()
{
	hopefullyCurrentThreadIsManager(HERE);

	// We catch you with an assertion if you do not make sure all your
	// Presents have been either canceled or completed
	bool anyRunners (false);

	if (true) {
		QMutexLocker locker (&mapsMutex);
        QMapIterator<const ThinkerBase*, QSharedPointer<ThinkerRunner> > i (thinkerMap);
		while (i.hasNext()) {
			i.next();
            QSharedPointer<ThinkerRunner> runner (i.value());
			hopefully(runner->isCanceled() or runner->isFinished(), HERE);
			anyRunners = true;
		}
	}

	if (anyRunners)
		QThreadPool::globalInstance()->waitForDone();
}
