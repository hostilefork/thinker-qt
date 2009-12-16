//
// SignalThrottler.cpp
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

#include "thinkerqt/signalthrottler.h"

SignalThrottler::SignalThrottler (unsigned int milliseconds, QObject* parent) :
	QTimer (parent),
	millisecondsDefault (cast_hopefully<int>(milliseconds, HERE))
{
	setSingleShot(true);
	connect(this, SIGNAL(timeout()), this, SLOT(onTimeout()));
}

void SignalThrottler::setMillisecondsDefault(unsigned int milliseconds)
{
	// TODO: better formalize signal throttler thread semantics.  This lets you change
	// the throttle but any emits (including one currently being processed) will
	// possibly use the old value
	millisecondsDefault.fetchAndStoreRelaxed(cast_hopefully<int>(milliseconds, HERE));
}

void SignalThrottler::onTimeout()
{
	lastEmit.start();
	emit throttled();
	nextEmit = QTime ();
}

void SignalThrottler::emitThrottled()
{
	emitThrottled(millisecondsDefault);
}

void SignalThrottler::emitThrottled(unsigned int milliseconds)
{
	// can't risk multiple threads trying to call this routine at the same time
	hopefully(QThread::currentThread() == thread(), HERE);

	// There is some overhead associated with timers, signals, etc.
	// Don't set a timer if the time we'd wait to signal is less than that value.
	// TODO: get this number from timing data, perhaps gathered at startup?

	static const int overheadMsec = 5;

	QTime currentTime = QTime::currentTime();
	QTime worstCaseEmitTime = lastEmit.isNull() ? currentTime : lastEmit.addMSecs(milliseconds);
	int deltaMilliseconds = currentTime.msecsTo(worstCaseEmitTime);

	if (deltaMilliseconds < overheadMsec) {

		if (not nextEmit.isNull()) {
			QTimer::stop();
			nextEmit = QTime ();
		}

		lastEmit.start();
		emit throttled();

	} else if (nextEmit.isNull() or (nextEmit > worstCaseEmitTime)) {

		QTimer::start(deltaMilliseconds);
		nextEmit = worstCaseEmitTime;
	}
}

bool SignalThrottler::postpone()
{
	if (not nextEmit.isNull()) {
		QTimer::stop();
		nextEmit = QTime ();
		return true;
	}
	return false;
}

SignalThrottler::~SignalThrottler()
{
}