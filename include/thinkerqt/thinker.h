//
// Thinker.h
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

#ifndef THINKERQT__THINKER_H
#define THINKERQT__THINKER_H

#include <QObject>

#include "defs.h"
#include "snapshottable.h"
#include "signalthrottler.h"
#include "thinkerrunner.h"

class ThinkerManager;
class ThinkerThread;

//
// ThinkerObject
//
// A "Thinker" is a task which runs on its own thread and is supposed to make
// some kind of calculation which other threads are interested in.  The way
// that progress is communicated back is through read-only "snapshots" of
// the object's state.
//
// The reason there is a base "ThinkerObject" which is separate from the
// "Thinker" template is due to limitations of Qt's moc in allowing you to
// declare templated QObjects.  See this article for more information:
//
//	http://doc.trolltech.com/qq/qq15-academic.html
//

class ThinkerObject : protected QObject, virtual public SnapshottableBase {
	Q_OBJECT

private:
	enum State {
		Thinking,
		Finished,
		Canceled
	};

private:
	// It is the ThinkerManager's responsibility to do state bookkeeping
	// Thus state changes *only on the manager's thread*

	State state;
	ThinkerManager& mgr;
	bool wasAttachedToPresent;
	SignalThrottler* notificationThrottler;

friend class ThinkerManager;

public:
	ThinkerObject (ThinkerManager& mgr);

public:
	ThinkerManager& getManager() const;

public:
	bool isPauseRequested(unsigned long time = 0) const;

// If exceptions are enabled, you can use this and it will throw an exception to the
// internal thread loop boilerplate on a pause request; only appropriate for
// non-continuable thinkers to use...
#ifndef Q_NO_EXCEPTIONS
	void pollForStopException(unsigned long time = 0) const;
#endif

protected:
	// When a runner is detached from a thinker, then that is the cue
	// that it will be stopped and destroyed.  However, since the thread
	// is in the middle of processing the Thinker destructor will not
	// immediately run.  This hook lets you do some bookkeeping
	// when the runner goes away.

	virtual void beforePresentDetach();

friend class ThinkerPresentBase;

public:
	virtual void afterThreadAttach();
	virtual void beforeThreadDetach();

friend class ThinkerThread;

signals:
	void madeProgress();
public:
	// To help eliminate the misunderstanding of introducing control signals designed to
	// change a thinker from the simple forward path of start=>pause=>continue/stop,
	// it does not directly expose itself as a QObject.  But you do have to connect
	// and disconnect progress signals from it.  This narrower API is for that.
	bool connectProgressTo(const QObject * receiver, const char * member);
	bool disconnectProgressFrom(const QObject * receiver, const char * member = 0);

protected:
	// These overrides provide added checking and also signal
	// "progress" when the unlock runs.

	/* virtual */ void lockForWrite(const codeplace& cp);
	/* virtual */ void unlock(const codeplace& cp);
#ifndef THINKERQT_REQUIRE_CODEPLACE
	// This will cause the any asserts to indicate a failure in thinker.h instead
	// line instead of the offending line in the caller... not as good... see hoist
	// documentation http://hostilefork.com/hoist/
	void lockForWrite()
	{
		return lockForWrite(HERE);
	}
	void unlock()
	{
		return unlock(HERE);
	}
#endif

signals:
	// TODO: Should the Thinker be forced to announce that it has returned
	// from the thinking function even though a pause was not requested
	// because it intends to process the event loop?  It is somewhat error
	// prone to allow a return from a non-paused thinker without enforcing
	// a finishedThinking() signal, but essential to allow for the
	// thinking to involve signal/slot processing.

	void thinkingFinished();

private slots:
	void onStartThinking();
	void onContinueThinking();

protected:
	virtual void startThinking() = 0;
	virtual void continueThinking()
	{
		// Making a restartable thinker typically involves extra work to
		// make it into a coroutine.  You don't have to do that work if
		// you don't intend on pausing and restarting thinkers.  In that
		// case, isPauseRequested really just means isStopRequested...

		hopefullyNotReached("This Thinker was not designed to be restartable.", HERE);
	}

public:
	virtual ~ThinkerObject();
};


// If you aren't trying to create a class from which you will be deriving
// other classes or would not need derived classes to have virtually defined
// signals and slots, you can derive directly from Thinker< DataType >
//
// It gets trickier if you want to interject your own virtual methods on a
// type that can think, and that trickiness is part of the reason I've divided
// this into so many component classes.  You'd simply do your own derivation
// like the below to create your base template class, except instead of
// ThinkerObject you would derive from your QObject base class that
// you derived from ThinkerObject.
//
// In order to get finer control over when and how snapshots can be
// taken, we inherit *privately* from the Snapshottable.  This disables
// direct calls to Snapshottable::makeSnapshot.  Instead, snapshots
// are made through ThinkerPresent.  When designing your own
// Thinker-derived types you are free to get rid of that limitation, but
// it can be a good sanity check to keep thinkers from snapshotting
// themselves...

template< class DataTypeParam >
class Thinker : public ThinkerObject, virtual private Snapshottable< DataTypeParam >
{
public:
	typedef DataTypeParam DataType;
	typedef typename Snapshottable< DataType >::Snapshot Snapshot;

public:
	class Present : public ThinkerPresentBase
	{
	public:
		Present () :
			ThinkerPresentBase ()
		{
		}
		Present (const Present& other) :
			ThinkerPresentBase (other)
		{
		}
	protected:
		Present (QSharedPointer< ThinkerObject > thinker) :
			ThinkerPresentBase (thinker)
		{
		}
		friend class ThinkerManager;
	public:
		typename Thinker::Snapshot createSnapshot() const
		{
			hopefullyCurrentThreadIsManager(HERE);
			SnapshotBase* allocatedSnapshot (createSnapshotBase());
			Snapshot result (*cast_hopefully< Snapshot* >(allocatedSnapshot, HERE));
			delete allocatedSnapshot;
			return result;
		}

	public:
		/* virtual */ ~Present()
		{
		}
	};

public:
	// This is the most efficient and general constructor, which does not
	// require your state object to be default-constructible.  See notes in
	// snapshottable about the other constructor variants.

	Thinker (ThinkerManager& mgr, QSharedDataPointer< DataType > d) :
		ThinkerObject (mgr),
		Snapshottable< DataType > (d)
	{
	}

	Thinker (ThinkerManager& mgr, const DataType& d) :
		ThinkerObject (mgr),
		Snapshottable< DataType > (d)
	{
	}

	Thinker  (ThinkerManager& mgr) :
		ThinkerObject (mgr),
		Snapshottable< DataType > ()
	{
	}

private:
	// You call makeSnapshot from the ThinkerPresent and not from the
	// thinker itself.

	/*template< class T >*/ friend class Present;
	Snapshot makeSnapshot()
	{
		return Snapshottable< DataType >::makeSnapshot();
	}

public:
	// These overrides are here because we are inheriting privately
	// from Snapshottable, but want readable() and writable() to
	// be public.

	const DataTypeParam& readable(const codeplace& cp) const
	{
		return Snapshottable< DataType >::readable(cp);
	}
	DataTypeParam& writable(const codeplace& cp)
	{
		return Snapshottable< DataType >::writable(cp);
	}
#ifndef THINKERQT_REQUIRE_CODEPLACE
	// This will cause the any asserts to indicate a failure in thinker.h instead
	// line instead of the offending line in the caller... not as good... see hoist
	// documentation http://hostilefork.com/hoist/
	const DataTypeParam& readable() const
	{
		return readable(HERE);
	}
	DataTypeParam& writable()
	{
		return writable(HERE);
	}
#endif

public:
	/* virtual */ ~Thinker ()
	{
	}
};

#endif