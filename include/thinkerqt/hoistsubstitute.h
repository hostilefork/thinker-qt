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

#ifndef THINKERQT__HOISTSUBSTITUTE_H
#define THINKERQT__HOISTSUBSTITUTE_H

#include <cassert>

// These definitions are stubs which let you build Thinker-Qt without hoist,
// turning the hoist features into simple asserts
//
//     http://hostilefork.com/hoist/

namespace hoist {

struct codeplace
{
	const char* filename;
	unsigned int line;
	const char* function;

	codeplace (const char* filename, unsigned int line, const char* function) :
		filename (filename),
		line (line),
		function (function)
	{
	}
};

#define HERE codeplace (__FILE__, __LINE__, __FUNCTION__)
#define PLACE(str) HERE

inline bool hopefully(bool condition, const codeplace& cp)
{
	if (not condition) {
		__assert_fail("assertion failure", cp.filename, cp.line, cp.function);
		return false;
	}
	return true;
}

inline bool hopefullyNotReached(const char* message, const codeplace& cp)
{
	__assert_fail(message, cp.filename, cp.line, cp.function);
	return false;
}

inline bool hopefullyNotReached(const codeplace& cp)
{
	return hopefullyNotReached("unreachable code", cp);
}

template < class TrackType >
class tracked {
public:
	tracked (const TrackType& value, const codeplace& /* cp */) :
		value (value)
	{
	}

public:
	void assign(const TrackType& newValue, const codeplace& cp)
	{
		value = newValue;
	}
	void ensure(const TrackType& newValue, const codeplace& cp)
	{
		if (value != newValue)
			assign(newValue, cp);
	}
	bool hopefullyAlter(const TrackType& newValue, const codeplace& cp)
	{
		bool result (hopefully(newValue != value, cp));
		assign(newValue, cp);
		return result;
	}
	bool hopefullyTransition(const TrackType& oldValue, const TrackType& newValue, const codeplace& cp)
	{
		bool result (hopefully(newValue != oldValue, cp));
		assign(newValue, cp);
		return result;
	}

public:
	bool hopefullyEqualTo(const TrackType& goodValue, const codeplace& cp) const
	{
		return hopefully(value == goodValue, cp);
	}
	bool hopefullyInSet(const TrackType& goodValue1, const TrackType& goodValue2, const codeplace& cp) const
	{
		return hopefully((value == goodValue1) || (value == goodValue2), cp);
	}
	bool hopefullyInSet(const TrackType& goodValue1, const TrackType& goodValue2, const TrackType& goodValue3, const codeplace& cp) const
	{
		return hopefully((value == goodValue1) || (value == goodValue2) || (value == goodValue3), cp);
	}
	bool hopefullyNotEqualTo(const TrackType& badValue, const codeplace& cp)
	{
		return hopefully (value != badValue, cp);
	}
	bool hopefullyNotInSet(const TrackType& badValue1, const TrackType& badValue2, const codeplace& cp) const
	{
		return hopefully((value != badValue1) && (value != badValue2), cp);
	}
	bool hopefullyNotInSet(const TrackType& badValue1, const TrackType& badValue2, const TrackType& badValue3, const codeplace& cp) const
	{
		return hopefully((value != badValue1) && (value != badValue2) && (value != badValue3), cp);
	}


public:
	// Basic accessors, the value has a cast operator so it acts like what it's tracking
	operator const TrackType&() const
		{ return value; }
	const TrackType& get() const
		{ return value; }
	const TrackType& operator-> () const
		{ return value; }

private:
	TrackType value;
};

template < class DestType, class SourceType >
DestType cast_hopefully(SourceType src, const codeplace& cp)
{
	return static_cast< DestType >(src);
}

};

#endif
