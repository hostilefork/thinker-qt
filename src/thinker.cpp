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
#include "thinkerrunner.h"

//
// Thinker
//

ThinkerObject::ThinkerObject (ThinkerManager& mgr) :
	QObject (),
	state (ThinkerThinking),
	mgr (mgr)
{
	getManager().hopefullyCurrentThreadIsManager(HERE);
}

ThinkerObject::ThinkerObject () :
	QObject (),
	state (ThinkerThinking),
	mgr (*ThinkerManager::globalInstance())
{
	getManager().hopefullyCurrentThreadIsManager(HERE);
}

ThinkerManager& ThinkerObject::getManager() const
{
	return mgr;
}

void ThinkerObject::beforePresentDetach()
{
}

void ThinkerObject::afterThreadAttach()
{
}

void ThinkerObject::beforeThreadDetach()
{
}

void ThinkerObject::lockForWrite(const codeplace& cp)
{
	// we currently allow locking a thinker for writing
	// on the manager thread between the time the
	// Snapshot base class constructor has run
	// and when it is attached to a ThinkerPresent
	hopefully(thread() == QThread::currentThread(), HERE);

	SnapshottableBase::lockForWrite(cp);
}

void ThinkerObject::unlock(const codeplace& cp)
{
	// we currently allow locking a thinker for writing
	// on the manager thread between the time the
	// Snapshot base class constructor has run
	// and when it is attached to a ThinkerPresent
	hopefully(thread() == QThread::currentThread(), HERE);

	getManager().unlockThinker(*this);

	SnapshottableBase::unlock(cp);
}

bool ThinkerObject::wasPauseRequested(unsigned long time) const
{
	ThinkerRunner* runner (getManager().maybeGetRunnerForThinker(*this));
	runner->hopefullyCurrentThreadIsPooled(HERE);
	return runner->wasPauseRequested(time);
}

#ifndef Q_NO_EXCEPTIONS
void ThinkerObject::pollForStopException(unsigned long time) const
{
	ThinkerRunner* runner (getManager().maybeGetRunnerForThinker(*this));
	runner->hopefullyCurrentThreadIsPooled(HERE);
	runner->pollForStopException(time);
}
#endif

void ThinkerObject::onResumeThinking()
{
	getManager().hopefullyCurrentThreadIsThinker(HERE);
	resume();
}

ThinkerObject::~ThinkerObject ()
{
	getManager().hopefullyCurrentThreadIsManager(HERE);
	hopefully(getManager().maybeGetRunnerForThinker(*this) == NULL, HERE);
}