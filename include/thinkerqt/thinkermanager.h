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

#ifndef THINKERQT_THINKERMANAGER_H
#define THINKERQT_THINKERMANAGER_H

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QMap>

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
    bool hopefullyThreadIsManager(QThread const & thread, codeplace const & cp);
    bool hopefullyCurrentThreadIsManager(codeplace const & cp);
    bool hopefullyThreadIsNotManager(QThread const & thread, codeplace const & cp);
    bool hopefullyCurrentThreadIsNotManager(codeplace const & cp);
    bool hopefullyThreadIsThinker(QThread const & thread, codeplace const & cp);
    bool hopefullyCurrentThreadIsThinker(codeplace const & cp);

public:
    ThinkerBase const * getThinkerForThreadMaybeNull(QThread const & thread);

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
    void unlockThinker(ThinkerBase & thinker);
	friend class ThinkerBase;

private:
    void createRunnerForThinker(shared_ptr<ThinkerBase> holder, codeplace const & cp);

public:
	template<class ThinkerType>
    typename ThinkerType::Present run(unique_ptr<ThinkerType>&& holder, codeplace const & cp) {
        // We only want to run the destructor of the Thinker on the thread where
        // it was created.  But if we use shared_ptr to a Thinker then that means
        // we are surrendering the control of which thread will actually be the
        // one to perform the deletion (it will happen on which ever thread happens
        // to be the last one to release a reference).  A custom deleter addresses
        // this and we use Qt's "deleteLater()"
        hopefully(not holder->parent(), cp); // It's a QObject, but we're taking ownership...
        shared_ptr<ThinkerType> shared (holder.release(), [] (ThinkerType* thinker) {
            if (thinker == nullptr) {
                return;
            }

            if (thinker->thread() == QThread::currentThread())
                delete thinker;
            else
                thinker->deleteLater();
        });
        createRunnerForThinker(shared, cp);
        return typename ThinkerType::Present (shared);
	}

    template<class ThinkerType>
    ThinkerPresentBase runBase(unique_ptr<ThinkerType>&& holder, codeplace const & cp) {
        // We only want to run the destructor of the Thinker on the thread where
        // it was created.  But if we use shared_ptr to a Thinker then that means
        // we are surrendering the control of which thread will actually be the
        // one to perform the deletion (it will happen on which ever thread happens
        // to be the last one to release a reference).  A custom deleter addresses
        // this and we use Qt's "deleteLater()"
        // REVIEW: redundancy, revisit

        hopefully(not holder->parent(), cp); // It's a QObject, but we're taking ownership...
        shared_ptr<ThinkerType> shared (holder.release(), [] (ThinkerType* thinker) {
            if (thinker == nullptr) {
                return;
            }

            if (thinker->thread() == QThread::currentThread())
                delete thinker;
            else
                thinker->deleteLater();
        });
        createRunnerForThinker(shared, cp);
        return ThinkerPresentBase (shared);
	}

public:
    void ensureThinkersPaused(codeplace const & cp);
    void ensureThinkersResumed(codeplace const & cp);

#ifndef THINKERQT_REQUIRE_CODEPLACE
	// This will cause the any asserts to indicate a failure in thinkermanager.h instead
	// line instead of the offending line in the caller... not as good... see hoist
	// documentation http://hostilefork.com/hoist/
	template<class ThinkerType>
    typename ThinkerType::Present run(unique_ptr<ThinkerType>&& thinker) {
        return run(std::move(thinker), HERE);
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

    void requestAndWaitForCancelButAlreadyCanceledIsOkay(ThinkerBase & thinker);

    void ensureThinkerFinished(ThinkerBase & thinker);

public:
    void waitForPushToThread(ThinkerRunner * runner);
    void processThreadPushesUntil(ThinkerRunner * runner);
	void processThreadPushes()
	{
		processThreadPushesUntil(NULL);
	}

public:
    void addToThinkerMap(shared_ptr<ThinkerRunner> runner);
    void removeFromThinkerMap(shared_ptr<ThinkerRunner> runner, bool wasCanceled);
    void addToThreadMap(shared_ptr<ThinkerRunner> runner, QThread & thread);
    void removeFromThreadMap(shared_ptr<ThinkerRunner> runner, QThread & thread);

	// Runners are like "tasks".  There is not necessarily a one-to-one
	// correspondence between Runners and thinkers.  So you must be
	// careful not to assume that you can get a thread for a thinker.
	//
	// But somewhat tautologically, it is true that *if* thinker code is
	// running, it will be doing so on a thread of execution.
private:
    shared_ptr<ThinkerRunner> maybeGetRunnerForThinker(ThinkerBase const &thinker);
    shared_ptr<ThinkerRunner> maybeGetRunnerForThread(QThread const & thread);
	friend class ThinkerPresentBase;

private:
	SignalThrottler anyThinkerWrittenThrottler;
	QMutex mapsMutex;
    QMap<QThread const *, shared_ptr<ThinkerRunner> > threadMap;
    QMap<ThinkerBase const *, shared_ptr<ThinkerRunner> > thinkerMap;

	QMutex pushThreadMutex;
	QWaitCondition threadsWerePushed;
	QWaitCondition threadsNeedPushing;
    QSet<ThinkerRunner *> runnerSetToPush;
};

#endif
