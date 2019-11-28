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
    _lastEmit (QTime::currentTime()),  // easier to lie than handle null case
    _millisecondsDefault (milliseconds),
    _timer (this),  // thread affinity must be something w/executing event loop
    _mutex ()
{
    _timer.setSingleShot(true);
    connect(
        &_timer, &QTimer::timeout,
        this, &SignalThrottler::onTimeout,
        Qt::DirectConnection
    );

    // Annoying overload problem, two QTimer::start() signatures, one with an
    // `int` and one without (we want the `int` one)
    // https://stackoverflow.com/a/16795664/211160
    //
    connect(
        this, &SignalThrottler::startTimer,
        &_timer, static_cast<void (QTimer::*)(int)>(&QTimer::start),
        Qt::QueuedConnection  // see notes on _timer for why we need this
    );
}


void SignalThrottler::setMillisecondsDefault(int milliseconds)
{
    // This lets you change the throttle but any emits (including one currently
    // being processed) will possibly use the old value
    //
    _millisecondsDefault.fetchAndStoreRelaxed(milliseconds);
}


void SignalThrottler::onTimeout()
{
    QTime emitTime = QTime::currentTime();

    emit throttled();  // can be queued or direct

    {
        QMutexLocker lock (&_mutex);  // emitThrottled() may run concurrently

        _lastEmit = emitTime;

      #ifndef DEBUG_LOG_THINKER
        qDebug() << "Emitted throttled signal at "
            << _lastEmit << "\n";
      #endif

        // They would have only sent us a _nextEmit if they expected it to
        // be happening *sooner* than a pending signal.  Which means there is
        // a reschedule.  Reschedule can handle null times

        if (_nextEmit <= _lastEmit)
            _nextEmit = QTime ();
    }
}


void SignalThrottler::emitThrottled(int milliseconds)
{
    QTime worstCaseEmitTime = QTime::currentTime().addMSecs(milliseconds);

    bool shouldReschedule = false;  // don't emit inside threadcheck
    int deltaMilliseconds;

    {
        QMutexLocker lock (&_mutex);  // if emitThrottled() during onTimeout()

        if (_nextEmit.isNull()) {  // no timer currently scheduled
            deltaMilliseconds = milliseconds;
            _nextEmit = worstCaseEmitTime;
            shouldReschedule = true;
        }
        else {  // timer already scheduled, we may want to schedule it sooner
            deltaMilliseconds = _nextEmit.msecsTo(worstCaseEmitTime);

            if (deltaMilliseconds < 0) {
                //
                // The next emit will actually happen earlier than we're
                // requesting, and a timer is set, so don't worry about it.
            }
            else if (deltaMilliseconds < overheadMsec) {
                //
                // same... don't bother with a new request, we won't be able to
                // update soon enough
            }
            else {
                // go ahead and update the scheduling
                //
                _nextEmit = worstCaseEmitTime;
                shouldReschedule = true;
            }
        }
    }

    if (shouldReschedule)
        emit startTimer(deltaMilliseconds);  // see _timer notes for why `emit`
}


void SignalThrottler::emitThrottled()  // overload for convenience
{
    emitThrottled(_millisecondsDefault);
}


SignalThrottler::~SignalThrottler ()
{
}
