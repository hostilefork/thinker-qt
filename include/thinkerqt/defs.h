//
// defs.h
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

#ifndef THINKERQT_DEFS_H
#define THINKERQT_DEFS_H

// Old versions of Thinker-Qt had flag THINKERQT_USE_STD_SHARED_PTR, for this:
//
//     http://stackoverflow.com/questions/11150225/
//
// But I'm currently on to C++11 with my projects, so using std::shared_ptr

#include <memory>
using std::shared_ptr;
using std::unique_ptr;
using std::make_shared;

#if THINKERQT_USE_HOIST

// The hoist library is something that I use as an alternative system for
// reporting locations in source to assertions.  The project page is here:
//
// http://hostilefork.com/hoist/
//
// You don't have to have it on your system in order to build Thinker-Qt.
#include "hoist/hoist.h"

#else

// This is a small subset of definitions that substitute for hoist calls
#include "hoistsubstitute.h"

#endif

using namespace hoist;

#endif
