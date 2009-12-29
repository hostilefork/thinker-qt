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
#include "thinkerrunner.h"

class ThinkerThread;
class ThinkerThreadHelper;

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
friend class ThinkerPresentBase;

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
	typename ThinkerType::Present run(QSharedPointer< ThinkerType > thinker, const codeplace& cp) {
		hopefullyCurrentThreadIsManager(cp);
		hopefully(not thinker.isNull(), cp);
		hopefully(not thinker->wasAttachedToPresent, cp);
		thinker->wasAttachedToPresent = true;

		createThreadForThinker(thinker);

		return typename ThinkerType::Present (thinker);
	}

	ThinkerPresentBase runBase(QSharedPointer< ThinkerObject > thinker, const codeplace& cp) {
		hopefullyCurrentThreadIsManager(cp);
		hopefully(not thinker.isNull(), cp);
		hopefully(not thinker->wasAttachedToPresent, cp);
		thinker->wasAttachedToPresent = true;

		createThreadForThinker(thinker);

		return ThinkerPresentBase (thinker);
	}

	// If you pass in a raw pointer instead of a shared pointer, the manager will take
	// ownership of the thinker via shared pointer
	template<class ThinkerType>
	typename ThinkerType::Present run(ThinkerType* thinker, const codeplace& cp) {
		hopefully(thinker != NULL, cp);
		hopefully(thinker->parent() == NULL, cp);
		return run(QSharedPointer< ThinkerType >(thinker));
	}

	void ensureThinkersPaused(const codeplace& cp);
	void ensureThinkersResumed(const codeplace& cp);

#ifndef THINKERQT_REQUIRE_CODEPLACE
	// This will cause the any asserts to indicate a failure in thinkermanager.h instead
	// line instead of the offending line in the caller... not as good... see hoist
	// documentation http://hostilefork.com/hoist/

	template<class ThinkerType>
	typename ThinkerType::Present run(QSharedPointer< ThinkerType > thinker) {
		return run(thinker, HERE);
	}
	template<class ThinkerType>
	typename ThinkerType::Present run(ThinkerType* thinker) {
		return run(thinker, HERE);
	}
	ThinkerPresentBase runBase(QSharedPointer< ThinkerObject > thinker) {
		return runBase(thinker, HERE);
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

	void requestAndWaitForCancelButAlreadyCanceledIsOkay(ThinkerObject& thinker);

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
