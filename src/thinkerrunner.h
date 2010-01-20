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

#include "thinkerqt/thinker.h"

class ThinkerRunner;
class ThinkerManager;

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

class ThinkerRunner : public QEventLoop, public QRunnable
{
	Q_OBJECT

private:
	enum State {
		RunnerInitializing, // => Thinking
		RunnerThinking, // => Pausing, Canceling, Finished
		RunnerPausing, // => Paused
		RunnerPaused, // => Canceled, Resuming
		RunnerCanceling, // => Canceled
		RunnerCanceled, // terminal
		RunnerResuming, // => Thinking
		RunnerFinished // => Canceled
	};

public:
	ThinkerRunner (ThinkerHolder< ThinkerBase > holder);
	virtual ~ThinkerRunner ();

public:
	ThinkerManager& getManager() const;
	ThinkerBase& getThinker();
	const ThinkerBase& getThinker() const;

	// It used to be that Thinkers (QObjects) were created on the Manager thread and then pushed
	// to a thread of their own during the Run.  Since Run now queues, that push is deferred.  We
	// only know which thread the ThreadPool will put a Thinker onto when ThreadRunner::run()
	// happens, so we make a moveThinkerToThread request from that
signals:
	void moveThinkerToThread(QThread* thread, QSemaphore* numThreadsMoved);

public slots:
	void onMoveThinkerToThread(QThread* thread, QSemaphore* numThreadsMoved);

public:
	bool hopefullyCurrentThreadIsRun(const codeplace& cp) const;
	bool hopefullyCurrentThreadIsManager(const codeplace& cp) const;

signals:
	void breakEventLoop();
	void resumeThinking();
	void finished(ThinkerBase* thinker, bool canceled);

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

	void waitForCancel(); // no variant because we can't distinguish between an abort we asked for and

	void requestResume(const codeplace& cp) {
		requestResumeCore(false, cp);
	}
	void requestResumeButCanceledIsOkay(const codeplace& cp) {
		requestResumeCore(true, cp);
	}
	void waitForResume(const codeplace& cp);

	void requestFinishAndWaitForFinish(const codeplace& cp);

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
	void run();

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
	tracked< State > state;
	// it's for communication between one manager and one thinker so use wakeOne()
	mutable QWaitCondition stateChangeSignal;
	mutable QMutex signalMutex;
	ThinkerHolder< ThinkerBase > holder;
	QSharedPointer< ThinkerRunnerHelper > helper;

	// http://www.learncpp.com/cpp-tutorial/93-overloading-the-io-operators/
	friend QTextStream& operator<< (QTextStream& o, const State& state);
	friend class ThinkerRunnerHelper;
};

//
// ThinkerRunnerKeepalive
//
// An unfortunate aspect of using thread pools is that you cannot emit a signal from the
// pooled thread to a managing thread which you then use to destroy the object.
//
// Here's why: Even if the very last line of
// your QRunnable interface is  "emit deleteOkayNow()" the delete is not necessarily
// okay unless you specifically wait for the thread pool to finish all of its tasks.
//
// You can use a QFuture and a QFutureWatcher and QtConcurrent::run() instead of
// running from the thread pool but that adds overhead.  Instead we let the ThreadPool
// take ownership of the ThinkerRunner and delete it for us, but we don't let the run()
// function return until we're sure there are no ThinkerRunnerKeepalives outstanding.
//

class ThinkerRunnerKeepalive : protected QSharedPointer<ThinkerRunner*> {
public:
	ThinkerRunnerKeepalive ();
	ThinkerRunnerKeepalive (ThinkerRunner& runner);
	ThinkerRunnerKeepalive (const ThinkerRunnerKeepalive& other)  :
		QSharedPointer< ThinkerRunner* >(other)
	{
	}
	ThinkerRunnerKeepalive& operator= (const ThinkerRunnerKeepalive& other)
	{
		if (&other == this)
			return *this;
      		static_cast< QSharedPointer< ThinkerRunner* >& >(*this) = static_cast< const QSharedPointer< ThinkerRunner* >& >(other);
		return *this;
	}
	bool isNull() const
	{
		return QSharedPointer<ThinkerRunner*>::isNull();
	}
	ThinkerRunner& getRunner() const
	{
		return **data();
	}
	ThinkerRunner* operator-> ()
	{
		return *data();
	}
private:
	static void deleteAndReleaseLock(ThinkerRunner** runnerPointer);
};

#endif