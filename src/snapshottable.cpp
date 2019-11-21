//
// snapshottable.cpp
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

#include "thinkerqt/snapshottable.h"


SnapshottableBase::SnapshottableBase () :
    _dLock (),
    _lockedForWrite (false, HERE)
{
}


void SnapshottableBase::lockForWrite(codeplace const & cp)
{
    _lockedForWrite.hopefullyTransition(false, true, cp);
    _dLock.lockForWrite();
}


void SnapshottableBase::unlock(codeplace const & cp)
{
    _lockedForWrite.hopefullyTransition(true, false, cp);
    _dLock.unlock();
}


SnapshottableBase::~SnapshottableBase ()
{
}
