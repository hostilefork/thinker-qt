//
// ThinkerPresent.h
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

#ifndef THINKERQT__THINKERPRESENT_H
#define THINKERQT__THINKERPRESENT_H

#include <QThread>

#include "defs.h"
#include "snapshottable.h"

class ThinkerBase;
class ThinkerManager;

// ThinkerHolder
//
// We only want to run the destructor of the Thinker on the thread where
// it was created.  But if we use QSharedPointer to a Thinker then that means
// we are surrendering the control of which thread will actually be the
// one to perform the deletion (it will happen on which ever thread happens
// to be the last one to release a reference).
//
// This uses a custom deleter to ensure that we call QObject::deleteLater
// http://doc.trolltech.com/4.5/qsharedpointer.html#QSharedPointer-3

template< class ThinkerType >
class ThinkerHolder : public QSharedPointer< ThinkerType > {
public:
	ThinkerHolder ( ThinkerType* thinker ) :
		QSharedPointer< ThinkerType > (thinker, doDeleteLater)
	{
	}

	ThinkerHolder () :
		QSharedPointer< ThinkerType > ()
	{
	}

	template< class T > ThinkerHolder(const ThinkerHolder< T > other) :
		QSharedPointer< ThinkerType >(other)
	{
	}

	ThinkerBase& getThinkerBase ()
	{
		return *cast_hopefully< ThinkerBase* >(this->data(), HERE);
	}

	ThinkerType& getThinker ()
	{
		return *this->data();
	}

private:
	// Deleters are tough to do as friends because one needs to generally friend
	// functions or classes within the smart pointer implementation.
	static void doDeleteLater ( ThinkerType* thinker )
	{
		if (thinker->thread() == QThread::currentThread())
			delete thinker;
		else
			thinker->deleteLater();
	}
};

//
// ThinkerPresent
//
// When you ask the ThinkerManager to start running a thinker, it hands you
// back a ThinkerPresent object.  It is a lightweight reference counted class.
// Following the convention of QFuture, when the last reference to the
// ThinkerPresent goes away this does not implicitly cancel the Thinker;

class ThinkerPresentBase
{
public:
	// Following pattern set up by QtConcurrent's QFuture
	// default construction yields an empty future that just thinks of itself as canceled
	ThinkerPresentBase ();
	ThinkerPresentBase (const ThinkerPresentBase& other);
	ThinkerPresentBase& operator= (const ThinkerPresentBase & other);
	virtual ~ThinkerPresentBase ();

protected:
	ThinkerPresentBase (ThinkerHolder< ThinkerBase > holder);
	friend class ThinkerManager;

public:
	bool operator!= (const ThinkerPresentBase& other) const;
	bool operator== (const ThinkerPresentBase& other) const;

protected:
	bool hopefullyCurrentThreadIsManager(const codeplace& cp) const;
	friend class ThinkerPresentWatcherBase;

protected:
	// Is this a good idea to export in the API?
	ThinkerBase& getThinkerBase();
	const ThinkerBase& getThinkerBase() const;

public:
	// QFuture thinks of returning a list of results, whereas we snapshot
	/* T result() const; */
	/* operator T () const; */
	/* T resultAt(int index) const; */
	/* int resultCount() const; */
	/* QList<T> results() const; */
	/* bool isResultReadyAt(int index) const; */

	SnapshotPointerBase* createSnapshotBase() const;

public:
	// The isStarted () method of QFuture isn't relevant to a Thinker
	// It was "started" the moment it was created
	// But everything else applies in this section

	/* bool isStarted() const */

	bool isCanceled() const;
	bool isFinished() const;
	bool isPaused() const;
	bool isRunning() const;

	void cancel();
	void pause();
	void resume();
	void setPaused(bool paused);
	void togglePaused();

	void waitForFinished();

public:
	// TODO: Should Thinkers implement a progress API like QFuture?
	// QFuture's does not apply to run() interfaces...

	/* int progressMaximum() const; */
	/* int progressMinimum() const; */
	/* QString progressText() const; */
	/* int progressValue() const; */

protected:
	ThinkerHolder< ThinkerBase > holder;
};

#endif