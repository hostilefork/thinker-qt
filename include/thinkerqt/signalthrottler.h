//
// SignalThrottler.h
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

#ifndef THINKERQT__SIGNALTHROTTLER_H
#define THINKERQT__SIGNALTHROTTLER_H

#include <QTimer>
#include <QTime>
#include <QAtomicInt>

#include "defs.h"

//
// SignalThrottler
//
// A signal throttler is just a timer with a memory of when the last signal
// was emitted.  It is used to avoid emitting signals too frequently, but
// will always ensure at least one signal will be emitted between the time
// you make the call and the elapsed time you provide.
//
// NOTE: If you call with a long throttle followed by a call with a short
// throttle, the short throttle duration will override the longer one.  You
// will only emit one signal, but it may happen sooner than the longer
// throttle would have wanted.  To make sure signals do not happen
// any closer together than a certain number of milliseconds, do not make
// any of your calls to emitThrottled with less than that number
//

class SignalThrottler : protected QTimer
{
	Q_OBJECT

private:
	QTime lastEmit; // when was the last emit?  (null if never)
	QTime nextEmit; // when is the next emit scheduled?  (null if none)
	QAtomicInt millisecondsDefault;

public:
	SignalThrottler (unsigned int milliseconds = 0, QObject* parent = NULL);
	void setMillisecondsDefault(unsigned int milliseconds);

signals:
	void throttled();

private slots:
	void onTimeout();

public slots:
	// This function is the slot you call or connect an unthrottled signal
	// to.  It sort of defeats the point a bit to connect an unthrottled
	// signal here which happens frequently rather than call it as a
	// function because you'll still pay for the event queue processing

	void emitThrottled();

public:
	void emitThrottled(unsigned int milliseconds);

	// Because we are dealing with a delay, it may be the case that
	// we don't want the signal to happen.  Postpone clears any
	// pending events and returns whether an event was pending

	bool postpone();

public:
	// Workaround for the inability to use QObject::connect if you inherit
	// private or protected from a QObject-derived type

	QObject* getAsQObject()
	{
		return static_cast< QObject* >(this);
	}

public:
	~SignalThrottler ();
};

#endif
