//
// ThinkerPresentWatcher.cpp
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

#include "thinkerqt/thinkerpresentwatcher.h"
#include "thinkerqt/thinker.h"

ThinkerPresentWatcherBase::ThinkerPresentWatcherBase () :
	present (),
	milliseconds (200),
	notificationThrottler (NULL)
{
}

ThinkerPresentWatcherBase::ThinkerPresentWatcherBase (ThinkerPresentBase present) :
	present (present),
	// we parent the SignalThrottler to the thinker so that they'll have the same thread affinity
	// after the reparenting.  But is 200 milliseconds a good default?
	milliseconds (200),
	notificationThrottler (new SignalThrottler (milliseconds, &present.getThinkerBase()))
{
	connect(notificationThrottler, SIGNAL(throttled()), this, SIGNAL(written()), Qt::DirectConnection);
}

void ThinkerPresentWatcherBase::setPresentBase(ThinkerPresentBase present)
{
	if (this->present == present)
		return;

	this->present = present;

	if (present == ThinkerPresentBase ()) {
		delete notificationThrottler;
		notificationThrottler = NULL;
	} else {
		notificationThrottler = new SignalThrottler (milliseconds, &present.getThinkerBase());
		connect(notificationThrottler, SIGNAL(throttled()), this, SIGNAL(written()), Qt::DirectConnection);

		// add to the new watch list.  note that we may have missed the "finished"
		// signal so if it has finished, you will get an artificial "finished" re-broadcast
		ThinkerObject& newThinker (this->present.getThinkerBase());
		newThinker.watchersLock.lockForWrite();
		hopefully(not newThinker.watchers.contains(this), HERE);
		newThinker.watchers.insert(this);
		newThinker.watchersLock.unlock();
	}
}

ThinkerPresentBase ThinkerPresentWatcherBase::presentBase()
{
	return present;
}

void ThinkerPresentWatcherBase::setThrottleTime(unsigned int milliseconds)
{
	this->milliseconds = milliseconds;
	if (notificationThrottler != NULL) {
		notificationThrottler->setMillisecondsDefault(milliseconds);
	}
}

void ThinkerPresentWatcherBase::removeFromThinkerWatchers()
{
	if (this->present != ThinkerPresentBase ()) {
		// remove from the old watch list.  note that a signal may still be in the queue
		ThinkerObject& oldThinker (this->present.getThinkerBase());
		oldThinker.watchersLock.lockForWrite();
		hopefully(oldThinker.watchers.remove(this), HERE);
		oldThinker.watchersLock.unlock();
		this->present = ThinkerPresentBase();
	}
}

ThinkerPresentWatcherBase::~ThinkerPresentWatcherBase ()
{
	removeFromThinkerWatchers();
}
