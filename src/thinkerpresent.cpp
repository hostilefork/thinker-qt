//
// thinkerpresent.cpp
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

#include "thinkerqt/thinkerrunner.h"
#include "thinkerqt/thinkerpresent.h"
#include "thinkerqt/thinkermanager.h"

//
// ThinkerPresent
//

ThinkerPresentBase::ThinkerPresentBase () :
    _holder (nullptr),
    _thread (QThread::currentThread())
{
}


ThinkerPresentBase::ThinkerPresentBase (
    ThinkerPresentBase const & other
) :
    _holder (other._holder),
    _thread (QThread::currentThread())
{
}


ThinkerPresentBase::ThinkerPresentBase (
    shared_ptr<ThinkerBase> _holder
) :
    _holder (_holder),
    _thread (QThread::currentThread())
{
}


bool ThinkerPresentBase::operator!= (
    ThinkerPresentBase const & other
)
    const
{
    return _holder != other._holder;
}


bool ThinkerPresentBase::operator== (
    ThinkerPresentBase const & other
)
    const
{
    return _holder == other._holder;
}


ThinkerPresentBase & ThinkerPresentBase::operator= (
    const ThinkerPresentBase & other
) {
    if (this != &other) {
        _holder = other._holder;
    }
    return *this;
}


bool ThinkerPresentBase::hopefullyCurrentThreadIsDifferent (
    codeplace const & cp
)
    const
{
    // The primary restriction on communicating with a thinker with a
    // Present or a PresentWatcher is that we are not doing so on
    // the same thread that the Thinker itself is running on.  We
    // can take snapshots and such from pretty much any other thread
    // in the system.
    if (not _holder) {
        return true;
    }

    auto runner = _holder->_mgr.maybeGetRunnerForThinker(*_holder);
    if (not runner)
        return true;

    return hopefully(QThread::currentThread() != runner->thread(), cp);
}


ThinkerBase & ThinkerPresentBase::getThinkerBase () {
    return *_holder;
}


ThinkerBase const & ThinkerPresentBase::getThinkerBase () const {
    return *_holder;
}


bool ThinkerPresentBase::isCanceled () const {
    using State = ThinkerBase::State;

    hopefullyCurrentThreadIsDifferent(HERE);

    // If there are global objects or value members of classes before
    // manager is started...
    if (not _holder)
        return true;

    ThinkerBase const & thinker = getThinkerBase();
    auto runner = thinker.getManager().maybeGetRunnerForThinker(thinker);
    bool result = false;
    if (runner == nullptr) {
        result = (thinker._state == State::ThinkerCanceled);
    } else {
        result = runner->isCanceled();
    }
    return result;
}


bool ThinkerPresentBase::isFinished () const {
    using State = ThinkerBase::State;

    hopefullyCurrentThreadIsDifferent(HERE);

    if (_holder == nullptr) {
        // This return statement was commented out.  Why?
        hopefullyNotReached(HERE);
        return false;
    }

    ThinkerBase const & thinker = getThinkerBase();
    auto runner = thinker.getManager().maybeGetRunnerForThinker(thinker);
    bool result = false;
    if (runner == nullptr) {
        result = (thinker._state == State::ThinkerFinished);
    } else {
        result = runner->isFinished();
    }
    return result;
}


bool ThinkerPresentBase::isPaused () const {
    hopefullyCurrentThreadIsDifferent(HERE);

    if (_holder == nullptr) {
        // This return statement was commented out.  Why?
        return false;
    }

    ThinkerBase const & thinker = getThinkerBase();
    auto runner = thinker.getManager().maybeGetRunnerForThinker(thinker);
    bool result = false;
    if (runner == nullptr) {
        // the thinker has either finished or been canceled
    } else {
        result = runner->isPaused();
    }
    return result;
}


void ThinkerPresentBase::cancel () {
    using State = ThinkerBase::State;

    hopefullyCurrentThreadIsDifferent(HERE);

    // Precedent set by QFuture is you can call cancel() on default constructed
    // QFuture object and it's a no-op
    if (not _holder)
        return;

    ThinkerBase & thinker (getThinkerBase());
    auto runner = thinker.getManager().maybeGetRunnerForThinker(thinker);
    if (runner == nullptr) {
        thinker._state = State::ThinkerCanceled;
    } else {
        // No need to enforceCancel at this point (which would cause a
        // synchronous pause of the worker thread that we'd like to avoid)
        // ...although unruly thinkers may seem to "leak" if they stall too
        // long before responding to wasPauseRequested() signals
        runner->requestCancelButAlreadyCanceledIsOkay(HERE);
    }
}


void ThinkerPresentBase::pause () {
    hopefullyCurrentThreadIsDifferent(HERE);

    // what would it mean to pause a null?  What's precedent in QFuture?
    hopefully(_holder != nullptr, HERE);

    ThinkerBase & thinker (getThinkerBase());
    auto runner = thinker.getManager().maybeGetRunnerForThinker(thinker);

    // you can't pause a thinker that's finished or canceled
    hopefully(runner != nullptr, HERE);

    // If there is a pause, we should probably stop update signals and queue
    // a single update at the moment of resume
    runner->requestPause(HERE);
}


void ThinkerPresentBase::resumeMaybeEmitDone() {
    hopefullyCurrentThreadIsDifferent(HERE);

    // what would it mean to resume a null?  What's precedent in QFuture?
    hopefully(_holder != nullptr, HERE);

    ThinkerBase & thinker = getThinkerBase();
    auto runner = thinker.getManager().maybeGetRunnerForThinker(thinker);

    // you cannot resume a thinker that has finished or canceled
    hopefully(runner != nullptr, HERE); 

    // If there is a resume, we should probably stop update signals and queue
    // a single update at the moment of resume
    runner->requestResume(HERE);
}


void ThinkerPresentBase::setPaused (bool paused) {
    if (paused)
        resumeMaybeEmitDone();
    else
        pause();
}


void ThinkerPresentBase::togglePaused () {
    if (isPaused())
        resumeMaybeEmitDone();
    else
        pause();
}


void ThinkerPresentBase::waitForFinished () {
    hopefullyCurrentThreadIsDifferent(HERE);

    // Following QFuture's lead in having a single waitForFinished that works
    // for both canceled as well as non-canceled results.  They seem to throw
    // an exception if the future had been paused...
    hopefully(not isPaused(), HERE);

    ThinkerBase & thinker (getThinkerBase());

    auto runner = thinker.getManager().maybeGetRunnerForThinker(thinker);
    if (runner == nullptr) {
        // has either finished or canceled
    } else {
        runner->waitForFinished(HERE);
    }
}


SnapshotBase * ThinkerPresentBase::createSnapshotBase () const {
    hopefullyCurrentThreadIsDifferent(HERE);

    ThinkerBase const & thinker = getThinkerBase();

    return thinker.createSnapshotBase();
}


ThinkerPresentBase::~ThinkerPresentBase () {
    hopefully(QThread::currentThread() == _thread, HERE);
}
