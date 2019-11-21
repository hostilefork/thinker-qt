//
// thinker.h
// This file is part of Thinker-Qt
// Copyright (C) 2010-2014 HostileFork.com
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

#ifndef THINKERQT_THINKER_H
#define THINKERQT_THINKER_H

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
//  http://doc.trolltech.com/qq/qq15-academic.html
//

class ThinkerBase : public QObject, virtual public SnapshottableBase {

    Q_OBJECT

    friend class ThinkerManager;
    friend class ThinkerPresentWatcherBase;
    friend class ThinkerPresentBase;
    friend class ThinkerRunner;


private:
    enum class State {
        ThinkerOwnedByRunner = 0,
        ThinkerFinished = 1,
        ThinkerCanceled = 2
    };


public:
#ifdef THINKERQT_EXPLICIT_MANAGER
    ThinkerBase (ThinkerManager & mgr);
#else
    ThinkerBase ();
#endif

    virtual ~ThinkerBase () override;


public:
    ThinkerManager & getManager() const;


public:
    bool wasPauseRequested (unsigned long time = 0) const;

// If exceptions are enabled, you can use this and it will throw an exception
// to the internal thread loop boilerplate on a pause request; only appropriate
// for non-continuable thinkers to use...
#ifndef Q_NO_EXCEPTIONS
    void pollForStopException (unsigned long time = 0) const;
#endif


public:
    virtual void afterThreadAttach ();

    virtual void beforeThreadDetach ();


public:
    bool hopefullyCurrentThreadIsThink (codeplace const & cp) const {
        // we currently allow locking a thinker for writing
        // on the manager thread between the time the
        // Snapshot base class constructor has run
        // and when it is attached to a ThinkerPresent
        return hopefully(thread() == QThread::currentThread(), cp);
    }


protected:
    // These overrides provide added checking and also signal
    // "progress" when the unlock runs.

    virtual void lockForWrite (codeplace const & cp) override;

    virtual void unlock (codeplace const & cp) override;

#ifndef THINKERQT_REQUIRE_CODEPLACE
    // This will cause the any asserts to indicate a failure in thinker.h
    // instead of at the offending line in the caller... not as good... see
    // hoist documentation http://hostilefork.com/hoist/
    void lockForWrite () {
        return lockForWrite(HERE);
    }

    void unlock () {
        return unlock(HERE);
    }
#endif


signals:
    // The done signal is used by the ThinkerPresentWatcher.  Once it was
    // the responsibility of a thinker's start/resume methods to emit this
    // signal, but that was switched to returning true or false.  Given
    // that change there may be better ways of notifying the watcher of
    // completion...but to keep things working as they were the signal
    // has been left here and is emitted by wrapping functions.

    void done ();


private:
    bool startMaybeEmitDone() {
        if (start()) {
            emit done();
            return true;
        }
        return false;
    }

    bool resumeMaybeEmitDone() {
        if (resume()) {
            emit done();
            return true;
        }
        return false;
    }

protected:
    virtual bool start () = 0;

    virtual bool resume () {
        // Making a restartable thinker typically involves extra work to
        // make it into a coroutine.  You don't have to do that work if
        // you don't intend on pausing and restarting thinkers.  In that
        // case, wasPauseRequested really just means wasStopRequested...

        hopefullyNotReached("Thinker not designed to be resumable.", HERE);
        return false;
    }


private:
    State _state;
    ThinkerManager & _mgr;
    QReadWriteLock _watchersLock;
    QSet<ThinkerPresentWatcherBase *> _watchers;
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

template <class T>
class Thinker : public ThinkerBase, private Snapshottable<T>
{
public:
    typedef T DataType;
    typedef typename Snapshottable<DataType>::Snapshot Snapshot;

public:
    class Present : public ThinkerPresentBase
    {
    public:
        Present () :
            ThinkerPresentBase ()
        {
        }

    protected:
        void verifyPresentType(codeplace const & cp) {
            // If we are generating a templated Thinker<T>::Present from a
            // base Present, ensure that the thinker held by that Present
            // is actually convertible to Thinker<T>!
            hopefully(
                dynamic_cast<Thinker *>(&ThinkerPresentBase::getThinkerBase())
                != nullptr,
                cp
            );
        }

    public:
        Present (ThinkerPresentBase & base) :
            ThinkerPresentBase (base)
        {
            verifyPresentType(HERE);
        }

    protected:
        Present (shared_ptr<ThinkerBase> holder) :
            ThinkerPresentBase (holder)
        {
            verifyPresentType(HERE);
        }

    public:
        Present (Present const & other) :
            ThinkerPresentBase (other)
        {
        }

        virtual ~Present () override
        {
        }

        friend class ThinkerManager;

    public:
        typename Thinker::Snapshot createSnapshot () const
        {
            // This restriction will have to be relaxed, but it may still be
            // helpful to offer some kind of virtual method hook to do thread
            // checking.
            hopefullyCurrentThreadIsDifferent(HERE);

            SnapshotBase * allocatedSnapshot = createSnapshotBase();
            Snapshot * ptr = dynamic_cast<Snapshot *>(allocatedSnapshot);
            hopefully(ptr != nullptr, HERE);

            Snapshot result = *ptr;
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
        const typename Thinker::Snapshot createSnapshot () const
        {
            hopefullyCurrentThreadIsDifferent(HERE);
            const SnapshotBase * allocatedSnapshot = createSnapshotBase();

            const Snapshot * ptr = dynamic_cast<const Snapshot *>(allocatedSnapshot);
            hopefully(ptr != nullptr, HERE);

            const Snapshot result = *ptr;
            delete allocatedSnapshot;
            return result;
        }

        void setPresent (Present present)
        {
            setPresentBase(present);
        }

        Present present ()
        {
            return Present (presentBase());
        }
    };


public:
#ifdef THINKERQT_EXPLICIT_MANAGER

    Thinker (ThinkerManager & mgr) :
        ThinkerBase (mgr),
        Snapshottable<DataType> ()
    {
    }

    template <class... Args>
    Thinker (ThinkerManager & mgr, Args &&... args) :
        ThinkerBase (mgr),
        Snapshottable<DataType> (std::forward<Args>(args)...)
    {
    }

#else

    Thinker () :
        ThinkerBase (),
        Snapshottable<DataType> ()
    {
    }

    template <class... Args>
    Thinker (Args &&... args) :
        ThinkerBase (),
        Snapshottable<DataType> (std::forward<Args>(args)...)
    {
    }

#endif

    virtual ~Thinker () override
    {
    }


private:
    // You call makeSnapshot from the ThinkerPresent and not from the
    // thinker itself.

    friend class Present;
    Snapshot makeSnapshot ()
    {
        return Snapshottable<DataType>::makeSnapshot();
    }


public:
    // These overrides are here because we are inheriting privately
    // from Snapshottable, but want readable() and writable() to
    // be public.

    const T & readable () const
    {
        return Snapshottable<DataType>::readable();
    }

    T & writable (codeplace const & cp)
    {
        return Snapshottable<DataType>::writable(cp);
    }

#ifndef THINKERQT_REQUIRE_CODEPLACE
    T & writable()
    {
        return writable(HERE);
    }
#endif
};



//
// If for convenience you just want one on-demand manager, this provides an
// analogue to QtConcurrent::run just from including thinker.h - otherwise
// somewhere in your program you will have to include thinkermanager.h
// and instantiate it yourself
//

#ifndef THINKERQT_EXPLICIT_MANAGER

#include "thinkermanager.h"

namespace ThinkerQt {

    template <class ThinkerType, class... Args>
    typename ThinkerType::Present run (Args&&... args) {
        return ThinkerManager::getGlobalManager().run(unique_ptr<ThinkerType>(
            new ThinkerType (std::forward<Args>(args)...))
        );
    }
}
#endif

#endif
