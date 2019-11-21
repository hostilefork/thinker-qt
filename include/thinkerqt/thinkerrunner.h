//
// thinkerrunner.h
// This file is part of Thinker-Qt
// Copyright (C) 2010-2014 HostileFork.com
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

#ifndef THINKERQT_THINKERRUNNER_H
#define THINKERQT_THINKERRUNNER_H

#include <QWaitCondition>
#include <QMutex>
#include <QSemaphore>
#include <QRunnable>
#include <QEventLoop>
#include <QTextStream>

#include "thinkerqt/thinker.h"

class ThinkerRunnerHelper;
class ThinkerManager;
class ThinkerRunnerProxy;

class ThinkerRunner : public QEventLoop
{
    Q_OBJECT


private:
    enum class State {
        Queued, // => ThreadPush
        QueuedButPaused, // => Queued, Paused
        ThreadPush, // => Thinking
        Thinking, // => Pausing, Canceling, Finished
        Pausing, // => Paused
        Paused, // => Canceled, Resuming
        Resuming, // => Thinking
        Finished, // => Canceled
        Canceling, // => Canceled
        Canceled // terminal
    };


public:
    ThinkerRunner (shared_ptr<ThinkerBase> holder);

    virtual ~ThinkerRunner () override;


public:
    ThinkerManager & getManager() const;

    ThinkerBase & getThinker();

    ThinkerBase const & getThinker() const;


public:
    void doThreadPushIfNecessary();


public:
    bool hopefullyCurrentThreadIsRun(codeplace const & cp) const;

    bool hopefullyCurrentThreadIsManager(codeplace const & cp) const;

    bool hopefullyCurrentThreadIsNotThinker(codeplace const & cp) const;


signals:
    void breakEventLoop();

    void resumeThinking();


public:
    void requestPause (codeplace const & cp) {
        requestPauseCore(false, false, cp);
    }

    void requestPauseButPausedIsOkay (codeplace const &cp) {
        requestPauseCore(true, false, cp);
    }

    void waitForPause() {
        waitForPauseCore(false);
    }

    void requestPauseButPausedOrCanceledIsOkay (codeplace const & cp) {
        requestPauseCore(true, true, cp);
    }

    void requestPauseButCanceledIsOkay (codeplace const & cp) {
        requestPauseCore(false, true, cp);
    }

    void waitForPauseButCanceledIsOkay () {
        waitForPauseCore(true);
    }

    void requestCancel (codeplace const & cp) {
        requestCancelCore(false, cp);
    }

    void requestCancelButAlreadyCanceledIsOkay (codeplace const & cp) {
        requestCancelCore(true, cp);
    }

    void requestResume (codeplace const & cp) {
        requestResumeCore(false, cp);
    }

    void requestResumeButCanceledIsOkay (codeplace const & cp) {
        requestResumeCore(true, cp);
    }

    void waitForResume (codeplace const & cp);

    void waitForFinished (codeplace const & cp);


public:
    bool isFinished () const;

    bool isCanceled () const;

    bool isPaused () const;

    bool wasPauseRequested (unsigned long time = 0) const;

#ifndef Q_NO_EXCEPTIONS
    void pollForStopException (unsigned long time = 0) const;
#endif


protected:
    friend class ThinkerRunnerProxy;

    bool runThinker();


private:
    void requestPauseCore (
        bool isPausedOkay,
        bool isCanceledOkay,
        codeplace const & cp
    );

    void waitForPauseCore (bool isCanceledOkay);

    void requestCancelCore (bool isAlreadyCanceledOkay, codeplace const & cp);

    void requestResumeCore (bool isCanceledOkay, codeplace const & cp);


#ifndef Q_NO_EXCEPTIONS
private:
    class StopException {
    };
#endif

private:
    tracked<State> _state;

    // for communication between one manager and one thinker so use wakeOne()
    mutable QWaitCondition _stateWasChanged;
    mutable QMutex _stateMutex;

    shared_ptr<ThinkerBase> _holder;
    QSharedPointer<ThinkerRunnerHelper> _helper;

    // http://www.learncpp.com/cpp-tutorial/93-overloading-the-io-operators/
    friend QTextStream & operator<< (QTextStream & o, State const & state);
    friend class ThinkerRunnerHelper;
};



// pretty much every thread object needs a member who was
// created in the thread's run() method, and thus dispatches
// messages within the thread's context
class ThinkerRunnerHelper : public QObject
{
    Q_OBJECT

private:
    ThinkerRunner & _runner;

public:
    ThinkerRunnerHelper (ThinkerRunner & runner);

    ~ThinkerRunnerHelper () override;

public:
    bool hopefullyCurrentThreadIsRun (codeplace const & cp) const {
        return hopefully(
            QThread::currentThread() == _runner.getThinker().thread(),
            cp
        );
    }


public slots:
    void markFinished();

    void queuedQuit();
};



//
// ThinkerRunnerProxy
//
// An unfortunate aspect of using thread pools is that you cannot emit a signal
// from the pooled thread to a managing thread usable to destroy the object.
//
// Here's why: Even if the very last line of your QRunnable interface is 
// "emit deleteOkayNow()" the delete is not necessarily okay unless you
// specifically wait for the thread pool to finish all of its tasks.
//

class ThinkerRunnerProxy : public QRunnable {

public:
    ThinkerRunnerProxy (shared_ptr<ThinkerRunner> runner);

    ~ThinkerRunnerProxy () override;


public:
    ThinkerManager & getManager();


public:
    void run() override;


private:
    shared_ptr<ThinkerRunner> _runner;
};

#endif
