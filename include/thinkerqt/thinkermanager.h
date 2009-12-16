//
// ThinkerManager.h
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

#ifndef THINKERQT__THINKERMANAGER_H
#define THINKERQT__THINKERMANAGER_H

#include <QThread>

#include "defs.h"
#include "thinker.h"

class ThinkerThread;
class ThinkerThreadHelper;

//
// ThinkerRunner
//
// When you ask the ThinkerManager to start running a thinker, it hands you
// back a shared pointer to a ThinkerRunner object.  When your last shared
// pointer to this object goes away, that is the indication that you want
// the manager to abandon the Thinker.
//
// However, Thinkers run on their own threads and may not terminate
// immediately.  If you have to do any bookkeeping at the moment a
// ThinkerRunner is destroyed, make sure to put that in the
// Thinker::beforeRunnerDetach method.
//

class ThinkerRunnerBase
{
	Q_DISABLE_COPY(ThinkerRunnerBase)

protected:
	QSharedPointer< ThinkerObject > thinker;

public:
	ThinkerRunnerBase (QSharedPointer< ThinkerObject > thinker);

protected:
	bool hopefullyCurrentThreadIsManager(const codeplace& cp);

public:
	ThinkerObject& getThinkerBase();
	QSharedPointer< SnapshotBase > makeSnapshotBase();

public:
	virtual ~ThinkerRunnerBase();
};

template< class ThinkerType >
class ThinkerRunner : public ThinkerRunnerBase
{
public:
	ThinkerRunner (QSharedPointer< ThinkerType > thinker) :
		ThinkerRunnerBase (thinker)
	{
	}
public:
	ThinkerType& getThinker() {
		hopefullyCurrentThreadIsManager(HERE);
		return *cast_hopefully< ThinkerType* >(thinker.data(), HERE);
	}
	QSharedPointer< typename ThinkerType::Snapshot > makeSnapshot()
	{
		hopefullyCurrentThreadIsManager(HERE);
		return cast_hopefully< ThinkerType* >(thinker.data(), HERE)->makeSnapshot();
	}

public:
	/* virtual */ ~ThinkerRunner()
	{
	}
};


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

friend class ThinkerObject;
friend class ThinkerThread;
friend class ThinkerThreadHelper;
friend class ThinkerRunnerBase;

public:
	ThinkerManager ();

private:
	bool hopefullyThreadIsManager(const QThread* thread, const codeplace& cp)
	{
		return hopefully(thread == this->thread(), cp);
	}
	bool hopefullyCurrentThreadIsManager(const codeplace& cp)
	{
		return hopefullyThreadIsManager(QThread::currentThread(), cp);
	}
	bool hopefullyThreadIsThinker(const QThread* thread, const codeplace& cp)
	{
		return hopefully(maybeCastToThinkerThread(thread) != NULL, cp);
	}
	bool hopefullyCurrentThreadIsThinker(const codeplace& cp)
	{
		return hopefullyThreadIsThinker(QThread::currentThread(), cp);
	}

	// Thinkers are like "tasks".  There is not necessarily a one-to-one
	// correspondence between Thinkers and threads.  So you must be
	// careful not to assume that you can get a thread for a thinker.
	//
	// But somewhat tautologically, it is true that *if* thinker code is
	// running, it will be doing so on a thread of execution.  So if you
	// have a QThread which passes the cast to a ThinkerObject thread...
	// then you may get the associated Thinker.
private:
	mapped< const ThinkerObject*, ThinkerThread* >::manager threadMapManager;
	mapped< const ThinkerObject*, ThinkerThread* >::manager& getThreadMapManager()
	{
		return threadMapManager;
	}
	ThinkerThread* maybeGetThreadForThinker(const ThinkerObject& thinker);
public:
	const ThinkerThread* maybeCastToThinkerThread(const QThread* thread);
	ThinkerObject& getThinkerForThread(const ThinkerThread* thread);

signals:
	void madeProgress();

private:
	void createThreadForThinker(QSharedPointer< ThinkerObject > thinker);

public:
	template<class ThinkerType>
	QSharedPointer< ThinkerRunner< ThinkerType > > makeRunner(QSharedPointer< ThinkerType > thinker, const codeplace& cp) {
		hopefullyCurrentThreadIsManager(cp);
		hopefully(not thinker.isNull(), cp);
		hopefully(not thinker->wasAttachedToRunner, cp);
		thinker->wasAttachedToRunner = true;

		createThreadForThinker(thinker);

		return QSharedPointer< ThinkerRunner< ThinkerType > >(new ThinkerRunner< ThinkerType >(thinker));
	}

	QSharedPointer< ThinkerRunnerBase > makeRunnerBase(QSharedPointer< ThinkerObject > thinker, const codeplace& cp) {
		hopefullyCurrentThreadIsManager(cp);
		hopefully(not thinker.isNull(), cp);
		hopefully(not thinker->wasAttachedToRunner, cp);
		thinker->wasAttachedToRunner = true;

		createThreadForThinker(thinker);

		return QSharedPointer< ThinkerRunnerBase >(new ThinkerRunnerBase (thinker));
	}

	void ensureThinkersPaused(const codeplace& cp);
	void ensureThinkersResumed(const codeplace& cp);

#ifndef THINKERQT_REQUIRE_CODEPLACE
	// This will cause the any asserts to indicate a failure in thinkermanager.h instead
	// line instead of the offending line in the caller... not as good... see hoist
	// documentation http://hostilefork.com/hoist/

	template<class ThinkerType>
	QSharedPointer< ThinkerRunner< ThinkerType > > makeRunner(QSharedPointer< ThinkerType > thinker) {
		return makeRunner(thinker, HERE);
	}
	QSharedPointer< ThinkerRunnerBase > makeRunnerBase(QSharedPointer< ThinkerObject > thinker) {
		return makeRunnerBase(thinker, HERE);
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

	void requestAndWaitForAbortButAlreadyAbortedIsOkay(ThinkerObject& thinker);

	void ensureThinkerFinished(ThinkerObject& thinker);

	void throttleNotificationFrequency(ThinkerObject& thinker, unsigned int milliseconds);

public slots:
	// TODO: review implications of
	// http://stackoverflow.com/questions/1351217/qthreadwait-and-qthreadfinished
	void onThreadFinished();

public:
	virtual ~ThinkerManager();
};

#endif
