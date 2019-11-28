//
// thinkerpresentwatcher.cpp
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

#include "thinkerqt/thinkerpresentwatcher.h"
#include "thinkerqt/thinker.h"


ThinkerPresentWatcherBase::ThinkerPresentWatcherBase () :
    _present (),
    _milliseconds (200),  // !!! is 200 msec a good default?
    _notificationThrottler ()
{
    hopefullyCurrentThreadIsDifferent(HERE);
}


ThinkerPresentWatcherBase::ThinkerPresentWatcherBase (
    ThinkerPresentBase present
) :
    _present (present),
    _milliseconds (200),  // !!! is 200 msec a good default?
    _notificationThrottler ()
{
    hopefullyCurrentThreadIsDifferent(HERE);
    doConnections();
}


void ThinkerPresentWatcherBase::doConnections()
{
    if (ThinkerPresentBase() == _present)
        hopefully(not _notificationThrottler, HERE);
    else {
        // !!! The reason the notification throttler is created dynamically
        // each time a thread is assigned from the thread pool was historically
        // because that parent was assumed as the only thread that could call
        // the `emitThrottled()` function.  This was to try and avoid needing
        // a mutex lock when doing updates in the thinkers.
        //
        // However, changes in timers between Qt4 and Qt5 led to problems with
        // making the signal throttler be parented to something with a thread
        // affinity that didn't have an event loop.  Hence the signal throttler
        // is parented to the ThinkerPresent, making this dynamism not needed
        // for the throttler (though since Thinkers are getting their thread
        // affinity pushed around, still needed for any of their connections)
        //
        // This should be reviewed, and `_present.getThinkerBase().thread()`
        // might something passed into the throttler each time...maybe just
        // for debug reasons.  We preserve the dynamism for now.
        //
        _notificationThrottler = QSharedPointer<SignalThrottler>(
            new SignalThrottler (
                _milliseconds,
                this  // parent (must have event loop to make QTimer work)
            )
        );

        connect(
            _notificationThrottler.data(), &SignalThrottler::throttled,
            this, &ThinkerPresentWatcherBase::written,
            Qt::AutoConnection
        );

        connect(
            &_present.getThinkerBase(), &ThinkerBase::done,
            this, &ThinkerPresentWatcherBase::finished,
            Qt::AutoConnection
        );

        // add to the new watch list.  note that we may have missed the
        // "finished" signal so if it has finished, you will get an artificial
        // "finished" re-broadcast
        //
        ThinkerBase & thinker = this->_present.getThinkerBase();

        QWriteLocker lock (&thinker._watchersLock);
        
        hopefully(not thinker._watchers.contains(this), HERE);
        thinker._watchers.insert(this);
    }
}


void ThinkerPresentWatcherBase::doDisconnections() {
    if (this->_present == ThinkerPresentBase ())
        hopefully(not _notificationThrottler, HERE);
    else {
        // remove from the old watch list.
        // note that a signal may still be in the queue
        //
        ThinkerBase & thinker = this->_present.getThinkerBase();

        QWriteLocker lock (&thinker._watchersLock);
        hopefully(thinker._watchers.remove(this), HERE);

        _notificationThrottler = QSharedPointer<SignalThrottler>();
    }
}


void ThinkerPresentWatcherBase::setPresentBase(ThinkerPresentBase present)
{
    hopefullyCurrentThreadIsDifferent(HERE);

    if (this->_present == present)
        return;

    doDisconnections();

    this->_present = present;

    doConnections();
}


ThinkerPresentBase ThinkerPresentWatcherBase::presentBase()
{
    hopefullyCurrentThreadIsDifferent(HERE);

    return _present;
}


void ThinkerPresentWatcherBase::setThrottleTime(unsigned int milliseconds)
{
    hopefullyCurrentThreadIsDifferent(HERE);

    this->_milliseconds = milliseconds;
    if (_notificationThrottler)
        _notificationThrottler->setMillisecondsDefault(milliseconds);
}


ThinkerPresentWatcherBase::~ThinkerPresentWatcherBase ()
{
    hopefullyCurrentThreadIsDifferent(HERE);

    doDisconnections();
}
