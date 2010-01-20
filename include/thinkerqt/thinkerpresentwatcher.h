//
// ThinkerPresentWatcher.h
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

#ifndef THINKERQT__THINKERPRESENTWATCHER_H
#define THINKERQT__THINKERPRESENTWATCHER_H

#include <QObject>
#include <QSharedPointer>

#include "defs.h"
#include "thinkerpresent.h"
#include "signalthrottler.h"

class ThinkerBase;
class ThinkerManager;

//
// ThinkerWatcher
//
// This class parallels the QFutureWatcher allowing you to receive
// signals as a Thinker makes progress or finishes.  Like the
// QFutureWatcher, it provides convenient access to the members of
// ThinkerPresent
//

class ThinkerPresentWatcherBase : public QObject
{
	Q_OBJECT

public:
	ThinkerPresentWatcherBase ();
	virtual ~ThinkerPresentWatcherBase ();

signals:
	void written();
	void finished();

public:
	void setThrottleTime(unsigned int milliseconds);
	void setPresentBase(ThinkerPresentBase present);
	ThinkerPresentBase presentBase();

public:
	SnapshotPointerBase* createSnapshotBase() const
		{ return present.createSnapshotBase(); }

public:
	bool isCanceled() const
		{ return present.isCanceled(); }

	bool isFinished() const
		{ return present.isFinished(); }

	bool isPaused() const
		{ return present.isPaused(); }

	bool isRunning() const
		{ return present.isRunning(); }

public:
	void cancel()
		{ present.cancel(); }

	void pause()
		{ present.pause(); }

	void resume()
		{ present.resume(); }

	void setPaused(bool paused)
		{ present.setPaused(paused); }

	void togglePaused()
		{ present.togglePaused(); }

	void waitForFinished()
		{ present.waitForFinished(); }

private:
	void doConnections();
	void doDisconnections();

protected:
	ThinkerPresentWatcherBase (ThinkerPresentBase present);
	friend class ThinkerManager;

protected:
	bool hopefullyCurrentThreadIsManager(const codeplace& cp) const
		{ return present != ThinkerPresentBase() ? present.hopefullyCurrentThreadIsManager(cp) : true; }

protected:
	// Is this a good idea to export in the API?
	ThinkerBase& getThinkerBase();
	const ThinkerBase& getThinkerBase() const;

protected:
	ThinkerPresentBase present;
	unsigned int milliseconds;
	QSharedPointer< SignalThrottler > notificationThrottler;
	friend class ThinkerBase;
};

#endif