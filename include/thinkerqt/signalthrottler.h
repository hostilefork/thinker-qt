//
// SignalThrottler.h
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

#ifndef THINKERQT_SIGNALTHROTTLER_H
#define THINKERQT_SIGNALTHROTTLER_H

#include <QTimer>
#include <QTime>
#include <QAtomicInt>
#include <QMutex>
#include <QSharedPointer>
#include <QThread>

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

class SignalThrottler : public QObject
{
	Q_OBJECT

public:
	SignalThrottler (unsigned int milliseconds = 0, QObject* parent = NULL);
	~SignalThrottler ();

public:
	void setMillisecondsDefault(unsigned int milliseconds);
	void emitThrottled(unsigned int milliseconds);

	// Because we are dealing with a delay, it may be the case that
	// we don't want the signal to happen.  Postpone clears any
	// pending events and returns whether an event was pending
	bool postpone();

public slots:
	// This function is the slot you call or connect an unthrottled signal
	// to.  It sort of defeats the point a bit to connect an unthrottled
	// signal here which happens frequently rather than call it as a
	// function because you'll still pay for the event queue processing
	void emitThrottled();

signals:
	void throttled();

private slots:
	void onTimeout();

private:
	// A signal throttler is not thread safe if you allocate the object with
	// a parent.  It assumes in that case that all emitThrottled calls will
	// be done from the parent QObject::thread(), and checks this with
	// an assertion.  If there is no parent then a mutex is allocated and
	// calls to emitThrottled will be thread safe.
	void enterThreadCheck();
	void exitThreadCheck();

private:
	QTime lastEmit; // when was the last emit?  (null if never)
	QTime nextEmit; // when is the next emit scheduled?  (null if none)
	QAtomicInt millisecondsDefault;
	QTimer timer;
	QSharedPointer<QMutex> timerMutex; // only allocated if no parent given...
};

#endif
