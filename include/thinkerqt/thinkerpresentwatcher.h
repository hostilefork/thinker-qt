//
// thinkerpresentwatcher.h
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

#ifndef THINKERQT_THINKERPRESENTWATCHER_H
#define THINKERQT_THINKERPRESENTWATCHER_H

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

    ~ThinkerPresentWatcherBase () override;


signals:
    void written ();

    void finished ();


public:
    void setThrottleTime (unsigned int milliseconds);

    void setPresentBase (ThinkerPresentBase present);

    ThinkerPresentBase presentBase ();


public:
    SnapshotBase const * createSnapshotBase () const {
        return _present.createSnapshotBase();
    }


public:
    bool isCanceled () const {
        return _present.isCanceled();
    }

    bool isFinished () const {
        return _present.isFinished();
    }

    bool isPaused () const {
        return _present.isPaused();
    }


public slots:
    void cancel () {
        _present.cancel();
    }

    void pause () {
        _present.pause();
    }

    void resumeMaybeEmitDone () {
        _present.resumeMaybeEmitDone();
    }

    void setPaused (bool paused) {
        _present.setPaused(paused);
    }

    void togglePaused () {
        _present.togglePaused();
    }

public:
    void waitForFinished () {
        _present.waitForFinished();
    }


private:
    void doConnections ();

    void doDisconnections ();


protected:
    friend class ThinkerManager;

    ThinkerPresentWatcherBase (ThinkerPresentBase present);


protected:
    bool hopefullyCurrentThreadIsDifferent(codeplace const & cp) const {
        if (_present == ThinkerPresentBase()) {
            return true;
        } else {
            return _present.hopefullyCurrentThreadIsDifferent(cp);
        }
    }


protected:
    // https://github.com/hostilefork/thinker-qt/issues/4

    ThinkerBase & getThinkerBase();

    ThinkerBase const & getThinkerBase() const;


protected:
    ThinkerPresentBase _present;
    unsigned int _milliseconds;
    QSharedPointer<SignalThrottler> _notificationThrottler;
    friend class ThinkerBase;
};

#endif
