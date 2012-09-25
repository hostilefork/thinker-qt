//
// Thinker.h
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

#ifndef THINKERQT_THINKERQT_H
#define THINKERQT_THINKERQT_H

#include "thinker.h"
#include "thinkermanager.h"

namespace ThinkerQt {

template<class ThinkerType, class... Args>
typename ThinkerType::Present run(Args&&... args)
{
    return ThinkerManager::globalInstance()->run(unique_ptr<ThinkerType>(new ThinkerType (std::forward<Args>(args)...)));
}

};

#endif
