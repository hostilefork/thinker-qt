//
// thinker.cpp
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

#include "thinkerqt/thinker.h"
#include "thinkerqt/thinkermanager.h"
#include "thinkerqt/thinkerrunner.h"


#ifdef THINKERQT_EXPLICIT_MANAGER
    ThinkerBase::ThinkerBase (ThinkerManager & mgr) :
        QObject (),
        _state (State::ThinkerOwnedByRunner),
        _mgr (mgr)
    {
        getManager().hopefullyCurrentThreadIsManager(HERE);
    }
#else
    ThinkerBase::ThinkerBase () :
        QObject (),
        _state (State::ThinkerOwnedByRunner),
        _mgr (ThinkerManager::getGlobalManager())
    {
        getManager().hopefullyCurrentThreadIsManager(HERE);
    }
#endif


ThinkerManager & ThinkerBase::getManager() const
{
    return _mgr;
}


void ThinkerBase::afterThreadAttach()
{
}


void ThinkerBase::beforeThreadDetach()
{
}


void ThinkerBase::lockForWrite(codeplace const & cp) {
    hopefullyCurrentThreadIsThink(HERE);

    SnapshottableBase::lockForWrite(cp);
}


void ThinkerBase::unlock(codeplace const & cp) {
    hopefullyCurrentThreadIsThink(HERE);

    getManager().unlockThinker(*this);

    SnapshottableBase::unlock(cp);
}


bool ThinkerBase::wasPauseRequested(unsigned long time) const
{
    hopefullyCurrentThreadIsThink(HERE);

    auto runner = getManager().maybeGetRunnerForThinker(*this);
    if (runner == nullptr) {
        hopefully(_state == State::ThinkerFinished, HERE);
        return false;
    }
    return runner->wasPauseRequested(time);
}


#ifndef Q_NO_EXCEPTIONS
    void ThinkerBase::pollForStopException(unsigned long time) const
    {
        hopefullyCurrentThreadIsThink(HERE);

        auto runner = getManager().maybeGetRunnerForThinker(*this);
        if (runner == nullptr)
            hopefully(_state == State::ThinkerFinished, HERE);
        else
            runner->pollForStopException(time);
    }
#endif


ThinkerBase::~ThinkerBase ()
{
    getManager().hopefullyCurrentThreadIsManager(HERE);
    hopefully(getManager().maybeGetRunnerForThinker(*this) == nullptr, HERE);
}
