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
    _milliseconds (200),
    _notificationThrottler ()
{
    hopefullyCurrentThreadIsDifferent(HERE);
}


ThinkerPresentWatcherBase::ThinkerPresentWatcherBase (
    ThinkerPresentBase present
) :
    _present (present),
    //
    // we parent the SignalThrottler to the thinker so that they'll have the
    // same thread affinity after the reparenting.  But is 200 milliseconds
    // a good default?
    //
    _milliseconds (200),
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
        _notificationThrottler = QSharedPointer<SignalThrottler>(
            new SignalThrottler (_milliseconds, &_present.getThinkerBase())
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
