//
// Thinker.cpp
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

#include "thinkerqt/thinker.h"
#include "thinkerqt/thinkermanager.h"
#include "thinkerrunner.h"

//
// Thinker
//

ThinkerBase::ThinkerBase (ThinkerManager& mgr) :
	QObject (),
	state (ThinkerThinking),
	mgr (mgr)
{
	getManager().hopefullyCurrentThreadIsManager(HERE);
}

ThinkerBase::ThinkerBase () :
	QObject (),
	state (ThinkerThinking),
	mgr (*ThinkerManager::globalInstance())
{
	getManager().hopefullyCurrentThreadIsManager(HERE);
}

ThinkerManager& ThinkerBase::getManager() const
{
	return mgr;
}

void ThinkerBase::afterThreadAttach()
{
}

void ThinkerBase::beforeThreadDetach()
{
}

void ThinkerBase::lockForWrite(const codeplace& cp)
{
	// we currently allow locking a thinker for writing
	// on the manager thread between the time the
	// Snapshot base class constructor has run
	// and when it is attached to a ThinkerPresent
	hopefully(thread() == QThread::currentThread(), HERE);

	SnapshottableBase::lockForWrite(cp);
}

void ThinkerBase::unlock(const codeplace& cp)
{
	// we currently allow locking a thinker for writing
	// on the manager thread between the time the
	// Snapshot base class constructor has run
	// and when it is attached to a ThinkerPresent
	hopefully(thread() == QThread::currentThread(), HERE);

	getManager().unlockThinker(*this);

	SnapshottableBase::unlock(cp);
}

bool ThinkerBase::wasPauseRequested(unsigned long time) const
{
	ThinkerRunner* runner (getManager().maybeGetRunnerForThinker(*this));
	runner->hopefullyCurrentThreadIsPooled(HERE);
	return runner->wasPauseRequested(time);
}

#ifndef Q_NO_EXCEPTIONS
void ThinkerBase::pollForStopException(unsigned long time) const
{
	ThinkerRunner* runner (getManager().maybeGetRunnerForThinker(*this));
	runner->hopefullyCurrentThreadIsPooled(HERE);
	runner->pollForStopException(time);
}
#endif

void ThinkerBase::onResumeThinking()
{
	getManager().hopefullyCurrentThreadIsThinker(HERE);
	resume();
}

ThinkerBase::~ThinkerBase ()
{
	getManager().hopefullyCurrentThreadIsManager(HERE);
	hopefully(getManager().maybeGetRunnerForThinker(*this) == NULL, HERE);
}