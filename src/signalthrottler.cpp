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
    _millisecondsDefault (milliseconds),
    _timer (),
    _timerMutex (nullptr == parent ? new QMutex () : nullptr)
{
    _timer.setSingleShot(true);
    connect(&_timer, &QTimer::timeout, this, &SignalThrottler::onTimeout);
}


void SignalThrottler::enterThreadCheck ()
{
    if (_timerMutex)
        _timerMutex->lock();
    else
        hopefully(QThread::currentThread() == thread(), HERE);
}


void SignalThrottler::exitThreadCheck ()
{
    if (_timerMutex)
        _timerMutex->unlock();
}


void SignalThrottler::setMillisecondsDefault (int milliseconds)
{
    // This lets you change the throttle but any emits (including one currently
    // being processed) will possibly use the old value
    _millisecondsDefault.fetchAndStoreRelaxed(milliseconds);
}


void SignalThrottler::onTimeout()
{
    _lastEmit.start();
    emit throttled();
    _nextEmit = QTime ();
}


void SignalThrottler::emitThrottled ()
{
    emitThrottled(_millisecondsDefault);
}


void SignalThrottler::emitThrottled (int milliseconds)
{
    // There is some overhead associated with _timers, signals, etc.
    // Don't set _timer if time we'd wait to signal is less than that value.
    // TODO: get this number from timing data, perhaps gathered at startup?

    static const int overheadMsec = 5;

    QTime currentTime = QTime::currentTime();

    enterThreadCheck();

    QTime worstCaseEmitTime
        = _lastEmit.isNull()
        ? currentTime
        : _lastEmit.addMSecs(milliseconds);

    int deltaMilliseconds = currentTime.msecsTo(worstCaseEmitTime);

    if (deltaMilliseconds < overheadMsec) {

        if (not _nextEmit.isNull()) {
            _timer.stop();
            _nextEmit = QTime ();
        }

        _lastEmit.start();
        emit throttled();

    } else if (_nextEmit.isNull() or (_nextEmit > worstCaseEmitTime)) {

        _timer.start(deltaMilliseconds);
        _nextEmit = worstCaseEmitTime;
    }

    exitThreadCheck();
}


bool SignalThrottler::postpone ()
{
    bool result = false;

    enterThreadCheck();

    if (not _nextEmit.isNull()) {
        _timer.stop();
        _nextEmit = QTime ();
        result = true;
    }

    exitThreadCheck();

    return result;
}


SignalThrottler::~SignalThrottler ()
{
}
