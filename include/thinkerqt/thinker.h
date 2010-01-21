//
// Thinker.h
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

#ifndef THINKERQT__THINKER_H
#define THINKERQT__THINKER_H

#include <QObject>
#include <QSet>
#include <QReadWriteLock>

#include "defs.h"
#include "snapshottable.h"
#include "signalthrottler.h"
#include "thinkerpresent.h"
#include "thinkerpresentwatcher.h"

class ThinkerManager;
class ThinkerRunner;
class ThinkerPresentWatcherBase;

//
// ThinkerBase
//
// A "Thinker" is a task which runs on its own thread and is supposed to make
// some kind of calculation which other threads are interested in.  The way
// that progress is communicated back is through read-only "snapshots" of
// the object's state.
//
// The reason there is a base "ThinkerBase" which is separate from the
// "Thinker" template is due to limitations of Qt's moc in allowing you to
// declare templated QObjects.  See this article for more information:
//
//	http://doc.trolltech.com/qq/qq15-academic.html
//

class ThinkerBase : protected QObject, virtual public SnapshottableBase {
	// To help eliminate the misunderstanding of introducing control signals designed to
	// change a thinker from the simple forward path of start=>pause=>continue/stop,
	// it does not publicly inherit from QObject.
	Q_OBJECT

private:
	enum State {
		ThinkerOwnedByRunner = 0,
		ThinkerFinished = 1,
		ThinkerCanceled = 2
	};

public:
	ThinkerBase (ThinkerManager& mgr);
	ThinkerBase ();
	virtual ~ThinkerBase ();

public:
	ThinkerManager& getManager() const;

public:
	bool wasPauseRequested(unsigned long time = 0) const;

// If exceptions are enabled, you can use this and it will throw an exception to the
// internal thread loop boilerplate on a pause request; only appropriate for
// non-continuable thinkers to use...
#ifndef Q_NO_EXCEPTIONS
	void pollForStopException(unsigned long time = 0) const;
#endif

friend class ThinkerPresentBase;

public:
	virtual void afterThreadAttach();
	virtual void beforeThreadDetach();

friend class ThinkerRunner;
template< class ThinkerType > friend class ThinkerHolder;

public:
	bool hopefullyCurrentThreadIsThink(const codeplace& cp) const
	{
		// we currently allow locking a thinker for writing
		// on the manager thread between the time the
		// Snapshot base class constructor has run
		// and when it is attached to a ThinkerPresent
		return hopefully(thread() == QThread::currentThread(), cp);
	}

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
	// a done() signal, but essential to allow for the
	// thinking to involve signal/slot processing.

	void done();

private slots:
	void onResumeThinking();

protected:
	virtual void start() = 0;
	virtual void resume()
	{
		// Making a restartable thinker typically involves extra work to
		// make it into a coroutine.  You don't have to do that work if
		// you don't intend on pausing and restarting thinkers.  In that
		// case, wasPauseRequested really just means wasStopRequested...

		hopefullyNotReached("This Thinker was not designed to be resumable.", HERE);
	}

private:
	State state;
	ThinkerManager& mgr;
	QReadWriteLock watchersLock;
	QSet<ThinkerPresentWatcherBase*> watchers;

	friend class ThinkerManager;
	friend class ThinkerPresentWatcherBase;
};


// If you aren't trying to create a class from which you will be deriving
// other classes or would not need derived classes to have virtually defined
// signals and slots, you can derive directly from Thinker< DataType >
//
// It gets trickier if you want to interject your own virtual methods on a
// type that can think, and that trickiness is part of the reason I've divided
// this into so many component classes.  You'd simply do your own derivation
// like the below to create your base template class, except instead of
// ThinkerBase you would derive from your QObject base class that
// you derived from ThinkerBase.
//
// In order to get finer control over when and how snapshots can be
// taken, we inherit *privately* from the Snapshottable.  This disables
// direct calls to Snapshottable::makeSnapshot.  Instead, snapshots
// are made through ThinkerPresent.  When designing your own
// Thinker-derived types you are free to get rid of that limitation, but
// it can be a good sanity check to keep thinkers from snapshotting
// themselves...

template< class DataTypeParam >
class Thinker : public ThinkerBase, virtual private Snapshottable< DataTypeParam >
{
public:
	typedef DataTypeParam DataType;
	typedef typename Snapshottable< DataType >::SnapshotPointer SnapshotPointer;

public:
	class Present : public ThinkerPresentBase
	{
	public:
		Present () :
			ThinkerPresentBase ()
		{
		}

		Present (ThinkerPresentBase& base) :
			ThinkerPresentBase (base)
		{
			static_cast<void>(cast_hopefully< Present* >(&base, HERE));
		}

		Present (const Present& other) :
			ThinkerPresentBase (other)
		{
		}

		/* virtual */ ~Present ()
		{
		}

	protected:
		Present (ThinkerHolder< ThinkerBase > holder) :
			ThinkerPresentBase (holder)
		{
		}
		friend class ThinkerManager;

	public:
		typename Thinker::SnapshotPointer createSnapshot() const
		{
			hopefullyCurrentThreadIsManager(HERE);
			SnapshotPointerBase* allocatedSnapshot (createSnapshotBase());
			SnapshotPointer result (*cast_hopefully< SnapshotPointer* >(allocatedSnapshot, HERE));
			delete allocatedSnapshot;
			return result;
		}
	};

public:
	class PresentWatcher : public ThinkerPresentWatcherBase
	{
	public:
		PresentWatcher (Present present) :
			ThinkerPresentWatcherBase (present)
		{
		}

		PresentWatcher () :
			ThinkerPresentWatcherBase ()
		{
		}

		~PresentWatcher ()
		{
		}

	public:
		typename Thinker::SnapshotPointer createSnapshot() const
		{
			hopefullyCurrentThreadIsManager(HERE);
			SnapshotPointerBase* allocatedSnapshot (createSnapshotBase());
			SnapshotPointer result (*cast_hopefully< SnapshotPointer* >(allocatedSnapshot, HERE));
			delete allocatedSnapshot;
			return result;
		}

		void setPresent(Present present)
		{
			setPresentBase(present);
		}

		Present present()
		{
			return Present (presentBase());
		}
	};

public:
	// This is the most efficient and general constructor, which does not
	// require your state object to be default-constructible.  See notes in
	// snapshottable about the other constructor variants.

	Thinker (QSharedDataPointer< DataType > d) :
		ThinkerBase (),
		Snapshottable< DataType > (d)
	{
	}
	Thinker (ThinkerManager& mgr, QSharedDataPointer< DataType > d) :
		ThinkerBase (mgr),
		Snapshottable< DataType > (d)
	{
	}

	Thinker (const DataType &d) :
		ThinkerBase (),
		Snapshottable< DataType > (d)
	{
	}
	Thinker (ThinkerManager& mgr, const DataType& d) :
		ThinkerBase (mgr),
		Snapshottable< DataType > (d)
	{
	}

	Thinker () :
		ThinkerBase (),
		Snapshottable< DataType > ()
	{
	}
	Thinker  (ThinkerManager& mgr) :
		ThinkerBase (mgr),
		Snapshottable< DataType > ()
	{
	}

	/* virtual */ ~Thinker ()
	{
	}

private:
	// You call makeSnapshot from the ThinkerPresent and not from the
	// thinker itself.

	/*template< class T >*/ friend class Present;
	SnapshotPointer makeSnapshot()
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
};

#endif
