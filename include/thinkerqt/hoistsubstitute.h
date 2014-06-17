//
// HoistSubstitute.h
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

#ifndef THINKERQT_HOISTSUBSTITUTE_H
#define THINKERQT_HOISTSUBSTITUTE_H

// These definitions are stubs which let you build Thinker-Qt without hoist,
// turning the hoist features into simple asserts
//
//     http://hostilefork.com/hoist/

namespace hoist {

struct codeplace
{
    char const * filename;
    unsigned int line;
    char const * function;

    codeplace (
        char const * filename,
        unsigned int line,
        char const * function
    ) :
        filename (filename),
        line (line),
        function (function)
    {
    }
};


#define HERE codeplace (__FILE__, __LINE__, __FUNCTION__)

#define PLACE(str) HERE


inline bool hopefullyNotReached(char const * message, codeplace const & cp)
{
    qt_assert_x(message, cp.function, cp.filename, cp.line);

    qFatal(
        "%s in %s of %s, line %d",
        message,
        cp.function,
        cp.filename,
        cp.line
    );

    return false;
}


inline bool hopefully(bool condition, codeplace const & cp)
{
    if (not condition) {
        hopefullyNotReached("assertion failure", cp);
        return false;
    }
    return true;
}


inline bool hopefullyNotReached(codeplace const & cp)
{
    return hopefullyNotReached("unreachable code", cp);
}


template <class TrackType>
class tracked {
public:
    tracked (TrackType const & value, codeplace const & cp) :
        value (value)
    {
        Q_UNUSED(cp);
    }


public:
    void assign (TrackType const & newValue, codeplace const & cp) {
        Q_UNUSED(cp);
        value = newValue;
    }

    void ensure (TrackType const & newValue, codeplace const & cp)
    {
        if (value != newValue)
            assign(newValue, cp);
    }

    bool hopefullyAlter (TrackType const & newValue, codeplace const & cp)
    {
        bool result (hopefully(newValue != value, cp));
        assign(newValue, cp);
        return result;
    }

    bool hopefullyTransition (
        TrackType const & oldValue,
        TrackType const & newValue,
        codeplace const & cp
    ) {
        bool result (hopefully(newValue != oldValue, cp));
        assign(newValue, cp);
        return result;
    }


public:
    bool hopefullyEqualTo (
        TrackType const & goodValue,
        codeplace const & cp
    ) const
    {
        return hopefully(value == goodValue, cp);
    }

    bool hopefullyInSet (
        TrackType const & goodValue1,
        TrackType const & goodValue2,
        codeplace const & cp
    ) const
    {
        return hopefully((value == goodValue1) or (value == goodValue2), cp);
    }

    bool hopefullyInSet (
        TrackType const & goodValue1,
        TrackType const & goodValue2,
        TrackType const & goodValue3,
        codeplace const & cp
    ) const
    {
        return hopefully(
            (value == goodValue1)
            or (value == goodValue2)
            or (value == goodValue3),
            cp
        );
    }

    bool hopefullyNotEqualTo (
        TrackType const & badValue,
        codeplace const & cp
    ) {
        return hopefully (value != badValue, cp);
    }

    bool hopefullyNotInSet (
        TrackType const & badValue1,
        TrackType const & badValue2,
        codeplace const & cp
    ) const
    {
        return hopefully((value != badValue1) and (value != badValue2), cp);
    }

    bool hopefullyNotInSet (
        TrackType const & badValue1,
        TrackType const & badValue2,
        TrackType const & badValue3,
        codeplace const & cp
    ) const
    {
        return hopefully(
            (value != badValue1)
            and (value != badValue2)
            and (value != badValue3),
            cp
        );
    }


public:
    // The value has a cast operator so it acts like what it's tracking

    operator TrackType const & () const {
        return value;
    }

    TrackType const & get() const {
        return value;
    }

    TrackType const & operator-> () const {
        return value;
    }

private:
    TrackType value;
};

} // end namespace hoist

#endif
