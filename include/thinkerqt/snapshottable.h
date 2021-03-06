//
// snapshottable.h
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

#ifndef THINKERQT_SNAPSHOTTABLE_H
#define THINKERQT_SNAPSHOTTABLE_H

#include <QObject>
#include <QSharedData>
#include <QSharedDataPointer>
#include <QReadWriteLock>

#include "defs.h"

//
// SnapshottableData
//
// Think of SnapshottableData as equivalent to QSharedData--use this
// as the base type for the class that represents the part of your object's
// state which will be "snapshottable".
//
// All things being equal, I'd have preferred to just use QSharedData here
// instead of creating another type.  Unfortunately, QSharedData has no
// virtual methods.  So dynamic_cast<Derived>(qshareddatapointer) just
// won't work.
//
//    http://bytes.com/topic/c/answers/134946-dynamic_cast-not-polymorphic-type
//
// Would it be worth it to allow use of QSharedData for those clients who
// didn't need to dynamic_cast?  Perhaps use a trick like this...
//
//    http://stackoverflow.com/questions/281725/
//

class SnapshottableData : public QSharedData
{
public:
    virtual ~SnapshottableData ()
    {
    }
};


//
// SnapshotBase
//
// A Snapshot offers read-only access to data that is under a copy-on-write
// policy.  Should the Snapshottable object modify its state after a snapshot
// is taken, the Snapshot will be unaffected.
//
// To do this, it stores a QSharedDataPointer and provides only the const
// data() method.  The reason this wrapper is necessary is because merely
// returning a "const QSharedDataPointer< X >" from makeSnapshot
// would not have the right protections.  You could get non-const access by
// simply assigning to a QSharedDataPointer.  (QSharedDataPointer< const X >
// doesn't work either because it would cause a detach() on dereference.)
//
// Technical considerations prevent putting a data member into the
// SnapshotBase such as:
//
//  QSharedDataPointer< SnapshottableBase > d;
//
// That is because QSharedDataPointer relies upon access to the copy
// constructor of the most-derived type for the copy-on-write implemention.
// The only way it would be able to do otherwise would be if it expected
// QSharedData types to implement a "virtual copy constructor"
// (e.g. a clone() method):
//
//    http://www.parashift.com/c++-faq-lite/virtual-functions.html#faq-20.8
//
// But it's not written that way.  Hence, you must always make a
// QSharedDataPointer to your concrete type--not base type.
//

class SnapshotBase
{
public:
    virtual void clear () = 0;
    virtual const SnapshottableData & dataBase () const = 0;

    // Avoids "deleting object of abstract class type 'SnapshotBase' which
    // has non-virtual destructor will cause undefined behavior" error.
    virtual ~SnapshotBase () {}

private:
    // (...there should be a member put here...)
    // QSharedDataPointer< Derived > d;
    // (...in the Snapshot< Derived > template...)
};


//
// SnapshottableBase
//
// "Snapshottable" is the interface of an object that is able to have
// a snapshot of itself taken.
//
// The reason there's yet another Snapshottable-as-template and
// SnapshottableBase interface separation is because of the
// tricky nature of making templated QObjects:
//
//    http://doc.trolltech.com/qq/qq15-academic.html
//
// See thinker.h for how this interacts with a templated QObject
//

class SnapshottableBase
{
public:
    SnapshottableBase ();
    virtual ~SnapshottableBase ();

public:
    // createSnapshotBase returns a pointer to an allocated object
    // due to technical restrictions, but createSnapshot proper returns
    // an implicitly shared type

    // TrollTech uses "create" when applied to factory-style things
    // Note: http://doc.trolltech.com/4.6/functions.html
    virtual SnapshotBase * createSnapshotBase() const = 0;

protected:
        // It's true that the shared data pointer protects us across threads
        // so we make copies safely.  But sometimes we have several
        // writes that go together and we don't want anyone to snapshot
        // the object in the middle of that.

    virtual void lockForWrite (codeplace const & cp);
    virtual void unlock (codeplace const & cp);

protected:
    mutable QReadWriteLock _dLock;
    tracked<bool> _lockedForWrite;
};


//
// Snapshottable
//
// The Snapshottable templated class inherits *virtually* from
// SnapshottableBase.  This allows a hierarchy in which your base
// class inherits from SnapshottableBase but a class derived from
// that base also inherits from Snapshottable.
//
//     http://en.wikipedia.org/wiki/Virtual_inheritance
//
// This is useful if you want to define your own templated object that
// has a non-template base (as is necessary when dealing with
// mixing QObjects and templates).
//
//    http://doc.trolltech.com/qq/qq15-academic.html
//

template <class T>
class Snapshottable : virtual public SnapshottableBase
{
public:
    typedef T DataType;

public:
    // Snapshot follows the convention set up by QFuture and friends
    // of sharing inside the type, as well as tolerating a default construction
    // Hence a Snapshot is actually a SnapshotPointer or a SharedSnapshot.
    // But that's wordy, as would QSharedFuture be.  We use the short name.

    class Snapshot : public SnapshotBase
    {
    public:
        Snapshot () :
            _d ()
        {
        }

        Snapshot (const Snapshot& other) :
            _d (other._d)
        {
        }

        Snapshot & operator= (const Snapshot & other) {
            if (this != &other) {
                _d = other._d;
            }
            return *this;
        }

        ~Snapshot ()
        {
        }

    protected:
        Snapshot (QSharedDataPointer<DataType> initialD) :
            _d (initialD)
        {
        }

    public:
        DataType const & data () const {
            hopefully(_d != QSharedDataPointer<DataType>(), HERE);
            return *_d.data();
        }

        DataType const * operator-> () const {
            return &data();
        }

        void clear() {
            _d = QSharedDataPointer<DataType> ();
        }

    protected:
        virtual SnapshottableData const & dataBase () const override {
            return dynamic_cast<SnapshottableData const &>(data());
        }

    private:
        QSharedDataPointer<DataType> _d;
        friend class Snapshottable<DataType>;
    };


    // NOTE: you must initialize the DataType member in the Snapshottable
    // constructor.  It may be the case that your derived class wishes to
    // perform some calculation in its constructor or in the initialization
    // of derived class members and then store that into the d member
    // *before* it can ever be snapshotted.  But then you must use the
    // readable()/writable() accessors to fix up the "incomplete" DataType
    // that you passed to the constructor.

public:
    template <class ...Args>
    Snapshottable (Args && ...args) :
        SnapshottableBase (),
        _d (QSharedDataPointer<DataType>(
            new DataType (std::forward<Args>(args)...))
        )
    {
    }

    virtual ~Snapshottable () override
    {
    }


public:
    Snapshot createSnapshot () const {
        QReadLocker lock (&this->_dLock);
        Snapshot result (_d);
        return result;
    }

    virtual SnapshotBase * createSnapshotBase () const override {
        return new Snapshot (createSnapshot());
    }


protected:
    // Due to the copy-on-write nature of Snapshottable objects,
    // there is no need for the Snapshottable object to do any
    // locks before reading

    DataType const & readable () const
    {
        return *_d;
    }


protected:
    // In order to prevent the case of a Snapshot being taken
    // during an incomplete state of the Snapshottable object,
    // you have to lock before getting write access.  When you
    // unlock it should be in a state that is okay to have a
    // snapshot taken

    DataType & writable (codeplace const & cp)
    {
        _lockedForWrite.hopefullyEqualTo(true, cp);
        return *_d;
    }


private:
    // you must initialize this "d" variable in your constructor, and
    // it is where you must put all of your state that you want to
    // be visible outside of the Snapshottable type when a
    // Snapshot is taken

    QSharedDataPointer<DataType> _d;
};

#endif
