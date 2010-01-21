//
// ThinkerManager.h
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

#ifndef THINKERQT__THINKERMANAGER_H
#define THINKERQT__THINKERMANAGER_H

#include <QThread>
#include <QMutex>
#include <QWaitCondition>

#include "defs.h"
#include "thinker.h"
#include "thinkerpresent.h"

class ThinkerRunner;
class ThinkerRunnerHelper;
class ThinkerRunnerKeepalive;


//
// ThinkerManager
//
// You must create a ThinkerManager class to handle your thinker groups
// It emits a signal whenever any one of its thinkers has announced progress
//
// You must make all your requests of the ThinkerManager on the thread
// affinity of the ThinkerManager.
//

class ThinkerManager : public QObject {
	Q_OBJECT

public:
	ThinkerManager ();
	virtual ~ThinkerManager();

public:
	static ThinkerManager* globalInstance();

public:
	bool hopefullyThreadIsManager(const QThread& thread, const codeplace& cp);
	bool hopefullyCurrentThreadIsManager(const codeplace& cp);
	bool hopefullyThreadIsNotManager(const QThread& thread, const codeplace& cp);
	bool hopefullyCurrentThreadIsNotManager(const codeplace& cp);
	bool hopefullyThreadIsThinker(const QThread& thread, const codeplace& cp);
	bool hopefullyCurrentThreadIsThinker(const codeplace& cp);

public:
	const ThinkerBase* maybeGetThinkerForThread(const QThread& thread);

	// It used to be that Thinkers (QObjects) were created on the Manager thread and then pushed
	// to a thread of their own during the Run.  Since Run now queues, that push is deferred.  We
	// only know which thread the ThreadPool will put a Thinker onto when ThreadRunner::run()
	// happens, so we make a moveThinkerToThread request from that
signals:
	void pushToThreadMayBeNeeded();
private slots:
	void doThreadPushesIfNecessary();

signals:
	void anyThinkerWritten();
protected:
	void unlockThinker(ThinkerBase& thinker);
	friend class ThinkerBase;

private:
	void createRunnerForThinker(ThinkerHolder< ThinkerBase > holder, const codeplace& cp);

public:
	template<class ThinkerType>
	typename ThinkerType::Present run(ThinkerHolder< ThinkerType > holder, const codeplace& cp) {
		createRunnerForThinker(holder, cp);
		return typename ThinkerType::Present (holder);
	}

	template< class ThinkerType > ThinkerPresentBase runBase(ThinkerHolder< ThinkerType > holder, const codeplace& cp) {
		createRunnerForThinker(holder, cp);
		return ThinkerPresentBase (holder);
	}

	// If you pass in a raw pointer instead of a shared pointer, the manager will take
	// ownership of the thinker via shared pointer
	template<class ThinkerType>
	typename ThinkerType::Present run(ThinkerType* thinker, const codeplace& cp) {
		hopefully(thinker != NULL, cp);
		hopefully(thinker->parent() == NULL, cp);
		return run(ThinkerHolder< ThinkerType >(thinker), cp);
	}

	void ensureThinkersPaused(const codeplace& cp);
	void ensureThinkersResumed(const codeplace& cp);

#ifndef THINKERQT_REQUIRE_CODEPLACE
	// This will cause the any asserts to indicate a failure in thinkermanager.h instead
	// line instead of the offending line in the caller... not as good... see hoist
	// documentation http://hostilefork.com/hoist/

	template<class ThinkerType>
	typename ThinkerType::Present run(ThinkerHolder< ThinkerType > holder) {
		return run(holder, HERE);
	}
	template<class ThinkerType>
	typename ThinkerType::Present run(ThinkerType* thinker) {
		return run(thinker, HERE);
	}
	ThinkerPresentBase runBase(ThinkerHolder< ThinkerBase > holder) {
		return runBase(holder, HERE);
	}
	void ensureThinkersPaused()
	{
		ensureThinkersPaused(HERE);
	}
	void ensureThinkersResumed()
	{
		ensureThinkersResumed(HERE);
	}
#endif

	void requestAndWaitForCancelButAlreadyCanceledIsOkay(ThinkerBase& thinker);

	void ensureThinkerFinished(ThinkerBase& thinker);

public:
	void waitForPushToThread(ThinkerRunner* runner);
	void processThreadPushesUntil(ThinkerRunner* runner);
	void processThreadPushes()
	{
		processThreadPushesUntil(NULL);
	}

public:
	void addToThinkerMap(QSharedPointer<ThinkerRunner> runner);
	void removeFromThinkerMap(QSharedPointer<ThinkerRunner> runner, bool wasCanceled);
	void addToThreadMap(QSharedPointer<ThinkerRunner> runner, QThread& thread);
	void removeFromThreadMap(QSharedPointer<ThinkerRunner> runner, QThread& thread);

	// Runners are like "tasks".  There is not necessarily a one-to-one
	// correspondence between Runners and thinkers.  So you must be
	// careful not to assume that you can get a thread for a thinker.
	//
	// But somewhat tautologically, it is true that *if* thinker code is
	// running, it will be doing so on a thread of execution.
private:
	QSharedPointer< ThinkerRunner > maybeGetRunnerForThinker(const ThinkerBase& thinker);
	QSharedPointer< ThinkerRunner > maybeGetRunnerForThread(const QThread& thread);
	friend class ThinkerPresentBase;

private:
	SignalThrottler anyThinkerWrittenThrottler;
	QMutex mapsMutex;
	QMap< const QThread*, QSharedPointer< ThinkerRunner > > threadMap;
	QMap< const ThinkerBase*, QSharedPointer< ThinkerRunner > > thinkerMap;

	QMutex pushThreadMutex;
	QWaitCondition threadsWerePushed;
	QWaitCondition threadsNeedPushing;
	QSet< ThinkerRunner* > runnerSetToPush;
};

#endif