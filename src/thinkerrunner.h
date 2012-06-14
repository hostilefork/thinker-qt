//
// ThinkerRunner.h
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

#ifndef THINKERQT__THINKERRUNNER_H
#define THINKERQT__THINKERRUNNER_H

#include <QWaitCondition>
#include <QMutex>
#include <QSemaphore>
#include <QRunnable>
#include <QEventLoop>
#include <QTextStream>

#include "thinkerqt/thinker.h"

class ThinkerRunner;
class ThinkerManager;
class ThinkerRunnerProxy;

// pretty much every thread object needs a member who was
// created in the thread's run() method, and thus dispatches
// messages within the thread's context
class ThinkerRunnerHelper : public QObject
{
	Q_OBJECT

private:
	ThinkerRunner& runner;

public:
	ThinkerRunnerHelper (ThinkerRunner& runner);
	~ThinkerRunnerHelper ();

public:
	bool hopefullyCurrentThreadIsRun(const codeplace& cp) const
	{
		return hopefully(QThread::currentThread() == thread(), cp);
	}
public slots:
	void markFinished();
	void queuedQuit();
};

class ThinkerRunner : public QEventLoop
{
	Q_OBJECT

private:
	enum State {
		RunnerQueued, // => ThreadPush
		RunnerQueuedButPaused, // => Queued, Paused
		RunnerThreadPush, // => Thinking
		RunnerThinking, // => Pausing, Canceling, Finished
		RunnerPausing, // => Paused
		RunnerPaused, // => Canceled, Resuming
		RunnerResuming, // => Thinking
		RunnerFinished, // => Canceled
		RunnerCanceling, // => Canceled
		RunnerCanceled // terminal
	};

public:
	ThinkerRunner (ThinkerHolder< ThinkerBase > holder);
	virtual ~ThinkerRunner ();

public:
	ThinkerManager& getManager() const;
	ThinkerBase& getThinker();
	const ThinkerBase& getThinker() const;

public:
	void doThreadPushIfNecessary();

public:
	bool hopefullyCurrentThreadIsRun(const codeplace& cp) const;
	bool hopefullyCurrentThreadIsManager(const codeplace& cp) const;

signals:
	void breakEventLoop();
	void resumeThinking();

public:
	void requestPause (const codeplace& cp) {
		requestPauseCore(false, cp);
	}
	void waitForPause() {
		waitForPauseCore(false);
	}

	void requestPauseButCanceledIsOkay(const codeplace& cp) {
		requestPauseCore(true, cp);
	}
	void waitForPauseButCanceledIsOkay() {
		waitForPauseCore(true);
	}

	void requestCancel(const codeplace& cp) {
		requestCancelCore(false, cp);
	}
	void requestCancelButAlreadyCanceledIsOkay(const codeplace& cp) {
		requestCancelCore(true, cp);
	}

	void requestResume(const codeplace& cp) {
		requestResumeCore(false, cp);
	}
	void requestResumeButCanceledIsOkay(const codeplace& cp) {
		requestResumeCore(true, cp);
	}
	void waitForResume(const codeplace& cp);

	void waitForFinished(const codeplace& cp);

public:
	bool isFinished() const;
	bool isCanceled() const;
	bool isPaused() const;
	bool wasPauseRequested(unsigned long time = 0) const;

#ifndef Q_NO_EXCEPTIONS
public:
	void pollForStopException(unsigned long time = 0) const;
#endif

protected:
	bool runThinker();
	friend class ThinkerRunnerProxy;

private:
	void requestPauseCore(bool isCanceledOkay, const codeplace& cp);
	void waitForPauseCore(bool isCanceledOkay);
	void requestCancelCore(bool isAlreadyCanceledOkay, const codeplace& cp);
	void requestResumeCore(bool isCanceledOkay, const codeplace& cp);

#ifndef Q_NO_EXCEPTIONS
private:
	class StopException {
	};
#endif

private:
    tracked<State> state;
	// it's for communication between one manager and one thinker so use wakeOne()
	mutable QWaitCondition stateWasChanged;
	mutable QMutex stateMutex;
    ThinkerHolder<ThinkerBase> holder;
    QSharedPointer<ThinkerRunnerHelper> helper;

	// http://www.learncpp.com/cpp-tutorial/93-overloading-the-io-operators/
	friend QTextStream& operator<< (QTextStream& o, const State& state);
	friend class ThinkerRunnerHelper;
};

//
// ThinkerRunnerProxy
//
// An unfortunate aspect of using thread pools is that you cannot emit a signal from the
// pooled thread to a managing thread which you then use to destroy the object.
//
// Here's why: Even if the very last line of
// your QRunnable interface is  "emit deleteOkayNow()" the delete is not necessarily
// okay unless you specifically wait for the thread pool to finish all of its tasks.
//

class ThinkerRunnerProxy : public QRunnable {
public:
	ThinkerRunnerProxy (QSharedPointer<ThinkerRunner> runner);
	ThinkerManager& getManager();
	void run();
	~ThinkerRunnerProxy ();

private:
	QSharedPointer<ThinkerRunner> runner;
};

#endif
