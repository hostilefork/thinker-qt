//
// SignalThrottler.cpp
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

#include "thinkerqt/signalthrottler.h"

SignalThrottler::SignalThrottler (unsigned int milliseconds, QObject* parent) :
	QObject (parent),
	millisecondsDefault (cast_hopefully<int>(milliseconds, HERE)),
    timer (),
	timerMutex (NULL == parent ? new QMutex () : NULL)
{
	timer.setSingleShot(true);
	connect(&timer, SIGNAL(timeout()), this, SLOT(onTimeout()));
}

void SignalThrottler::enterThreadCheck()
{
	if (timerMutex)
		timerMutex->lock();
	else
		hopefully(QThread::currentThread() == thread(), HERE);
}

void SignalThrottler::exitThreadCheck()
{
	if (timerMutex)
		timerMutex->unlock();
}

void SignalThrottler::setMillisecondsDefault(unsigned int milliseconds)
{
	// This lets you change the throttle but any emits (including one currently
	// being processed) will possibly use the old value
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
	// There is some overhead associated with timers, signals, etc.
	// Don't set a timer if the time we'd wait to signal is less than that value.
	// TODO: get this number from timing data, perhaps gathered at startup?
	static const int overheadMsec = 5;
	QTime currentTime = QTime::currentTime();

	enterThreadCheck();

	QTime worstCaseEmitTime = lastEmit.isNull() ? currentTime : lastEmit.addMSecs(milliseconds);
	int deltaMilliseconds = currentTime.msecsTo(worstCaseEmitTime);

	if (deltaMilliseconds < overheadMsec) {

		if (not nextEmit.isNull()) {
			timer.stop();
			nextEmit = QTime ();
		}

		lastEmit.start();
		emit throttled();

	} else if (nextEmit.isNull() or (nextEmit > worstCaseEmitTime)) {

		timer.start(deltaMilliseconds);
		nextEmit = worstCaseEmitTime;
	}

	exitThreadCheck();
}

bool SignalThrottler::postpone()
{
	bool result (false);

	enterThreadCheck();

	if (not nextEmit.isNull()) {
		timer.stop();
		nextEmit = QTime ();
		result = true;
	}

	exitThreadCheck();

	return result;
}

SignalThrottler::~SignalThrottler()
{
}
