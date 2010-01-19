//
// ThinkerPresentWatcher.h
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

#ifndef THINKERQT__THINKERPRESENTWATCHER_H
#define THINKERQT__THINKERPRESENTWATCHER_H

#include <QObject>

#include "defs.h"
#include "thinkerpresent.h"
#include "signalthrottler.h"

class ThinkerObject;
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

protected:
	ThinkerPresentBase present;
	unsigned int milliseconds;
	SignalThrottler* notificationThrottler;
	friend class ThinkerObject;

public:
	ThinkerPresentWatcherBase ();
protected:
	ThinkerPresentWatcherBase (ThinkerPresentBase present);
	friend class ThinkerManager;

protected:
	bool hopefullyCurrentThreadIsManager(const codeplace& cp) const
		{ return present.hopefullyCurrentThreadIsManager(cp); }

protected:
	// Is this a good idea to export in the API?
	ThinkerObject& getThinkerBase();
	const ThinkerObject& getThinkerBase() const;

public:
	// QFuture thinks of returning a list of results, whereas we snapshot
	/* T result () const;
	operator T () const; */

	SnapshotPointerBase* createSnapshotBase() const { return present.createSnapshotBase(); }

	/* T resultAt ( int index ) const;
	int resultCount () const;
	QList<T> results () const;
	bool isResultReadyAt ( int index ) const; */

public:
	// The isStarted () method of QFuture isn't relevant to a Thinker
	// It was "started" the moment it was created
	// But everything else applies
	/* bool isStarted () const */

	bool isCanceled () const { return present.isCanceled(); }
	bool isFinished () const { return present.isFinished(); }
	bool isPaused () const { return present.isPaused(); }
	bool isRunning () const { return present.isRunning(); }

	void cancel () { present.cancel(); }
	void pause () { present.pause(); }
	void resume () { present.resume(); }
	void setPaused ( bool paused ) { present.setPaused(paused); }
	void togglePaused () { present.togglePaused(); }

	void waitForFinished () { present.waitForFinished(); }

public:
	// TODO: Should Thinkers implement a progress API like QFuture?
	// QFuture's does not apply to run() interfaces...
	/* int progressMaximum () const;
	int progressMinimum () const;
	QString	progressText () const;
	int progressValue () const; */

signals:
	void written();
	void finished();

public:
	void setThrottleTime(unsigned int milliseconds);
	void setPresentBase(ThinkerPresentBase present);
	ThinkerPresentBase presentBase();

private:
	void removeFromThinkerWatchers();
public:
	virtual ~ThinkerPresentWatcherBase();
};

#endif
