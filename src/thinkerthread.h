//
// ThinkerThread.h
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

#ifndef THINKERQT__THINKERTHREAD_H
#define THINKERQT__THINKERTHREAD_H

#include <QWaitCondition>
#include <QMutex>
#include <QThread>

#include "thinkerqt/thinker.h"

class ThinkerThread;
class ThinkerManager;

// pretty much every thread object needs a member who was
// created in the thread's run() method, and thus dispatches
// messages within the thread's context
class ThinkerThreadHelper : public QObject
{
	Q_OBJECT

private:
	ThinkerThread* thread;

public:
	ThinkerThreadHelper(ThinkerThread* thread);
	~ThinkerThreadHelper();

public slots:
	void markFinished();
	void queuedQuit();
};

class ThinkerThread : public QThread
{
	Q_OBJECT

private:
	enum State {
		Initializing, // => Thinking
		Thinking, // => Pausing, Aborting, Finished
		Pausing, // => Paused
		Paused, // => Aborted, Continuing
		Aborting, // => Aborted
		Aborted, // terminal
		Continuing, // => Thinking
		Finished // => Aborted
	};

private:
#ifndef Q_NO_EXCEPTIONS
	class StopException {
	};
#endif

// http://www.learncpp.com/cpp-tutorial/93-overloading-the-io-operators/
friend QTextStream& operator << (QTextStream& o, const State& state);

private:

	tracked< State > state;
	// it's for communication between one manager and one thinker so use wakeOne()
	mutable QWaitCondition stateChangeSignal;
	mutable QMutex signalMutex;
	QSharedPointer< ThinkerThreadHelper > helper;
	QSharedPointer< ThinkerObject > thinker;
	mapped< const ThinkerObject*, ThinkerThread* > mapThinker;

friend class ThinkerThreadHelper;

private:
	ThinkerThreadHelper* getHelper()
	{
		hopefully(not helper.isNull(), HERE);
		return helper.data();
	}

public:
	ThinkerManager& getManager() const
	{
		return thinker->getManager();
	}

	ThinkerObject& getThinker() {
		return *thinker;
	}

public:
	ThinkerThread (QSharedPointer< ThinkerObject > thinker);

signals:
	void breakEventLoop();
	void startThinking();
	void continueThinking();
	void attachingToThinker();
	void detachingFromThinker();

private:
	void requestPauseCore(bool isAbortedOkay, const codeplace& cp);
	void waitForPauseCore(bool isAbortedOkay);
	void requestAbortCore(bool isAlreadyAbortedOkay, const codeplace& cp);
	void requestResumeCore(bool isAbortedOkay, const codeplace& cp);

public slots:
	void requestPause(const codeplace& cp) {
		requestPauseCore(false, cp);
	}
	void waitForPause() {
		waitForPauseCore(false);
	}

	void requestPauseButAbortedIsOkay(const codeplace& cp) {
		requestPauseCore(true, cp);
	}
	void waitForPauseButAbortedIsOkay() {
		waitForPauseCore(true);
	}

	void requestAbort(const codeplace& cp) {
		requestAbortCore(false, cp);
	}
	void requestAbortButAlreadyAbortedIsOkay(const codeplace& cp) {
		requestAbortCore(true, cp);
	}

	void waitForAbort(); // no variant because we can't distinguish between an abort we asked for and

	void requestResume(const codeplace& cp) {
		requestResumeCore(false, cp);
	}
	void requestResumeButAbortedIsOkay(const codeplace& cp) {
		requestResumeCore(true, cp);
	}
	void waitForResume(const codeplace& cp);

	void requestFinishAndWaitForFinish(const codeplace& cp);

public:
	bool isComplete() const;
	bool isAborted() const;
	bool isPaused() const;
	bool isPauseRequested(unsigned long time = 0) const;
#ifndef Q_NO_EXCEPTIONS
	void pollForStopException(unsigned long time = 0) const;
#endif

protected:
	void run(); // we want to know when this is done running

public:
	virtual ~ThinkerThread();
};

#endif
