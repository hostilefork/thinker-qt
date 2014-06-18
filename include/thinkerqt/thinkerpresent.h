//
// thinkerpresent.h
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

#ifndef THINKERQT_THINKERPRESENT_H
#define THINKERQT_THINKERPRESENT_H

#include <QThread>

#include "defs.h"
#include "snapshottable.h"

class ThinkerBase;
class ThinkerManager;


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
    // Following pattern set up by QtConcurrent's QFuture, whose default
    // construction yields an empty future that thinks of itself as canceled
    ThinkerPresentBase ();

    ThinkerPresentBase (ThinkerPresentBase const & other);

    ThinkerPresentBase & operator= (const ThinkerPresentBase & other);

    virtual ~ThinkerPresentBase ();


protected:
    friend class ThinkerManager;

    ThinkerPresentBase (shared_ptr<ThinkerBase> holder);


public:
    bool operator!= (ThinkerPresentBase const & other) const;

    bool operator== (ThinkerPresentBase const & other) const;


protected:
    friend class ThinkerPresentWatcherBase;

    bool hopefullyCurrentThreadIsDifferent (codeplace const & cp) const;


protected:
    // https://github.com/hostilefork/thinker-qt/issues/4

    ThinkerBase & getThinkerBase ();

    ThinkerBase const & getThinkerBase () const;


public:
    // QFuture thinks of returning a list of results, whereas we snapshot
#if RETURN_LIST_OF_RESULTS_LIKE_QFUTURE

    T result () const;

    operator T () const;

    T resultAt (int index) const;

    int resultCount () const;

    QList<T> results () const;

    bool isResultReadyAt (int index) const;

#endif

    SnapshotBase * createSnapshotBase () const;


public:
    // The isStarted() and isRunning() methods of QFuture are not
    // exposed by the ThinkerPresent... essentially any Thinker that
    // has been initialized with a shared data state and can be queried.
#if EXPOSE_ISSTARTED_AND_ISRUNNING_LIKE_QFUTURE

    bool isStarted () const;

    bool isRunning () const;

#endif

    bool isCanceled () const;

    bool isFinished () const;

    bool isPaused () const;


    void cancel ();

    void pause ();

    void resume ();

    void setPaused (bool paused);

    void togglePaused ();


    void waitForFinished ();


public:
    // TODO: Should Thinkers implement a progress API like QFuture?
    // QFuture's does not apply to run() interfaces...
#if OFFER_PROGRESS_API_LIKE_QFUTURE

    int progressMaximum () const;

    int progressMinimum () const;

    QString progressText () const;

    int progressValue () const;

#endif


protected:
    shared_ptr<ThinkerBase> _holder;

    // Should this be DEBUG only?
    QThread * _thread;
};

// we moc this file, though whether there are any QObjects or not may vary
// this dummy object suppresses the warning "No relevant classes found" w/moc
class THINKERPRESENT_no_moc_warning : public QObject { Q_OBJECT };

#endif
