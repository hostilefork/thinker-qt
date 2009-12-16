//
// Thinker.cpp
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

#include "thinkerqt/thinker.h"
#include "thinkerqt/thinkermanager.h"
#include "thinkerthread.h"

//
// Thinker
//

ThinkerObject::ThinkerObject (ThinkerManager& mgr) :
	QObject (),
	state (StillThinking),
	mgr (mgr),
	wasAttachedToRunner (false),
	progressThrottler (new SignalThrottler (200, this)) // is 200 milliseconds a good default?
{
	getManager().hopefullyCurrentThreadIsManager(HERE);

	connect(progressThrottler->getAsQObject(), SIGNAL(throttled()), this, SIGNAL(madeProgress()), Qt::DirectConnection);
}

ThinkerManager& ThinkerObject::getManager() const
{
	return mgr;
}

void ThinkerObject::lockForWrite(const codeplace& cp)
{
	if (wasAttachedToRunner) {
		getManager().hopefullyCurrentThreadIsThinker(HERE);
	} else {
		// we currently allow locking a thinker for writing
		// on the manager thread between the time the
		// Snapshot base class constructor has run
		// and when it is attached to a ThinkerRunner
		getManager().hopefullyCurrentThreadIsManager(HERE);
	}
	SnapshottableBase::lockForWrite(cp);
}

void ThinkerObject::unlock(const codeplace& cp)
{
	if (wasAttachedToRunner) {
		getManager().hopefullyCurrentThreadIsThinker(HERE);
		progressThrottler->emitThrottled();
	} else {
		// we do not emit a progress signal if we're in
		// the time between base class running and
		// being attached to a ThinkerRunner.  In fact,
		// makeSnapshot should also be disabled
		// during this time.  Perhaps inherit privately
		// and then attach the snapshot function
		// to the ThinkerRunner?
		getManager().hopefullyCurrentThreadIsManager(HERE);
	}

	SnapshottableBase::unlock(cp);
}

bool ThinkerObject::isPauseRequested(unsigned long time) const
{
	ThinkerThread* thinkerThread (getManager().maybeGetThreadForThinker(*this));
	hopefully(thinkerThread == QThread::currentThread(), HERE);
	return thinkerThread->isPauseRequested(time);
}

#ifndef Q_NO_EXCEPTIONS
void ThinkerObject::pollForStopException(unsigned long time) const
{
	ThinkerThread* thinkerThread (getManager().maybeGetThreadForThinker(*this));
	hopefully(thinkerThread == QThread::currentThread(), HERE);
	thinkerThread->pollForStopException(time);
}
#endif

void ThinkerObject::onStartThinking()
{
	getManager().hopefullyCurrentThreadIsThinker(HERE);
	startThinking();
}

void ThinkerObject::onContinueThinking()
{
	getManager().hopefullyCurrentThreadIsThinker(HERE);
	continueThinking();
}

ThinkerObject::~ThinkerObject ()
{
	getManager().hopefullyCurrentThreadIsManager(HERE);
	hopefully(getManager().maybeGetThreadForThinker(*this) == NULL, HERE);
}