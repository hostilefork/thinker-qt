//
// ThinkerPresentWatcher.cpp
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

#include "thinkerqt/thinkerpresentwatcher.h"
#include "thinkerqt/thinker.h"

ThinkerPresentWatcherBase::ThinkerPresentWatcherBase () :
	present (),
	milliseconds (200),
	notificationThrottler ()
{
	hopefullyCurrentThreadIsManager(HERE);
}

ThinkerPresentWatcherBase::ThinkerPresentWatcherBase (ThinkerPresentBase present) :
	present (present),
	// we parent the SignalThrottler to the thinker so that they'll have the same thread affinity
	// after the reparenting.  But is 200 milliseconds a good default?
	milliseconds (200),
	notificationThrottler ()
{
	hopefullyCurrentThreadIsManager(HERE);
	doConnections();
}

void ThinkerPresentWatcherBase::doConnections()
{
	if (ThinkerPresentBase() != present) {
		notificationThrottler = QSharedPointer<SignalThrottler>(
			new SignalThrottler (milliseconds, &present.getThinkerBase())
		);
		connect(notificationThrottler.data(), SIGNAL(throttled()), this, SIGNAL(written()), Qt::AutoConnection);
		connect(&present.getThinkerBase(), SIGNAL(done()), this, SIGNAL(finished()), Qt::AutoConnection);

		// add to the new watch list.  note that we may have missed the "finished"
		// signal so if it has finished, you will get an artificial "finished" re-broadcast
		ThinkerBase& thinker (this->present.getThinkerBase());
		thinker.watchersLock.lockForWrite();
		hopefully(not thinker.watchers.contains(this), HERE);
		thinker.watchers.insert(this);
		thinker.watchersLock.unlock();
	} else {
		hopefully(not notificationThrottler, HERE);
	}
}

void ThinkerPresentWatcherBase::doDisconnections()
{
	if (this->present != ThinkerPresentBase ()) {
		// remove from the old watch list.  note that a signal may still be in the queue
		ThinkerBase& thinker (this->present.getThinkerBase());
		thinker.watchersLock.lockForWrite();
		hopefully(thinker.watchers.remove(this), HERE);
		thinker.watchersLock.unlock();
		notificationThrottler = QSharedPointer<SignalThrottler>();
	} else {
		hopefully(not notificationThrottler, HERE);
	}
}

void ThinkerPresentWatcherBase::setPresentBase(ThinkerPresentBase present)
{
	hopefullyCurrentThreadIsManager(HERE);

	if (this->present == present)
		return;

	doDisconnections();

	this->present = present;

	doConnections();
}

ThinkerPresentBase ThinkerPresentWatcherBase::presentBase()
{
	hopefullyCurrentThreadIsManager(HERE);

	return present;
}

void ThinkerPresentWatcherBase::setThrottleTime(unsigned int milliseconds)
{
	hopefullyCurrentThreadIsManager(HERE);

	this->milliseconds = milliseconds;
	if (notificationThrottler) {
		notificationThrottler->setMillisecondsDefault(milliseconds);
	}
}

ThinkerPresentWatcherBase::~ThinkerPresentWatcherBase ()
{
	hopefullyCurrentThreadIsManager(HERE);

	doDisconnections();
}