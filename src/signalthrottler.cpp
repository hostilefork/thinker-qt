//
// signalthrottler.cpp
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

#include "thinkerqt/signalthrottler.h"


SignalThrottler::SignalThrottler (
    int milliseconds,
    QObject * parent
) :
    QObject (parent),
    _lastEmit (QTime::currentTime()), // easier to lie than handle null case
    _millisecondsDefault (milliseconds),
    _timer (),
    _timerMutex (parent ? nullptr : new QMutex ())
{
    _timer.setSingleShot(true);
    connect(
        &_timer, &QTimer::timeout,
        this, &SignalThrottler::onTimeout,
        Qt::DirectConnection
    );
    connect(
        this, &SignalThrottler::rescheduled,
        this, &SignalThrottler::onReschedule,
        Qt::AutoConnection
    );
}


void SignalThrottler::enterThreadCheck () {
    if (_timerMutex)
        _timerMutex->lock();
    else
        hopefully(QThread::currentThread() == thread(), HERE);
}


void SignalThrottler::exitThreadCheck () {
    if (_timerMutex)
        _timerMutex->unlock();
}


void SignalThrottler::setMillisecondsDefault (int milliseconds) {
    // This lets you change the throttle but any emits (including one currently
    // being processed) will possibly use the old value
    _millisecondsDefault.fetchAndStoreRelaxed(milliseconds);
}


void SignalThrottler::onTimeout() {
    QTime emitTime = QTime::currentTime();

    emit throttled(); // likely queued, but could be direct... :-/

    enterThreadCheck();

    _lastEmit = emitTime;

    // They would have only sent us a _nextEmit if they expected it to
    // be happening *sooner* than a pending signal.  Which means there is
    // a reschedule.  Reschedule can handle null times

    if (_nextEmit <= _lastEmit)
        _nextEmit = QTime ();

    exitThreadCheck();
}


void SignalThrottler::onReschedule() {
    bool shouldEmit = false; // don't emit inside of thread check

    enterThreadCheck();

    // There is some overhead associated with _timers, signals, etc.
    // Don't set _timer if time we'd wait to signal is less than that value.
    // TODO: get this number from timing data, perhaps gathered at startup?

    static const int overheadMsec = 5;

    if (_nextEmit.isNull()) {
        // we could have gotten a reschedule, and then another reschedule
        // if the timings kept shortening, and zeroed out one.  So this
        // can happen.
    }
    else {
        int deltaMilliseconds = _lastEmit.msecsTo(_nextEmit);

        if (deltaMilliseconds < 0) {
            // forget it, the moment's passed and we already emitted...
            _nextEmit = QTime ();
        }
        else if (deltaMilliseconds < overheadMsec) {
            // don't bother resetting a timer, just emit the signal
            shouldEmit = true;
            _nextEmit = QTime ();
            _lastEmit = QTime::currentTime();
        }
        else {
            // go ahead and set (or reset) the timer.

            _timer.start(deltaMilliseconds);
        }
    }

    exitThreadCheck();

    if (shouldEmit)
        emit throttled();
}


void SignalThrottler::emitThrottled () {
    emitThrottled(_millisecondsDefault);
}


void SignalThrottler::emitThrottled (int milliseconds) {

    bool shouldReschedule = false; // don't emit inside threadcheck

    enterThreadCheck();

    QTime worstCaseEmitTime = QTime::currentTime().addMSecs(milliseconds);

    if (_nextEmit.isNull()) {
        _nextEmit = worstCaseEmitTime;
        shouldReschedule = true;
    }
    else {
        int deltaMilliseconds = _nextEmit.msecsTo(worstCaseEmitTime);

        if (deltaMilliseconds < 0) {
            // The next emit will actually happen earlier than we're
            // requesting, so don't worry about it.
        }
        else if (deltaMilliseconds < overheadMsec) {
            // same... don't bother with a new request, we won't be
            // able to update soon enough
        }
        else {
            // go ahead and update the scheduling
            _nextEmit = worstCaseEmitTime;
            shouldReschedule = true;
        }
    }

    exitThreadCheck();

    if (shouldReschedule)
        emit rescheduled();
}


SignalThrottler::~SignalThrottler () {
}
