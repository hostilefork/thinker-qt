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

#include "thinkerbase.h"


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
