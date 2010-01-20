//
// ThinkerPresent.cpp
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

#include "thinkerrunner.h"
#include "thinkerqt/thinkerpresent.h"
#include "thinkerqt/thinkermanager.h"

//
// ThinkerPresent
//

ThinkerPresentBase::ThinkerPresentBase () :
	holder (NULL)
{
}

ThinkerPresentBase::ThinkerPresentBase (const ThinkerPresentBase& other) :
	holder (other.holder)
{
}

ThinkerPresentBase::ThinkerPresentBase (ThinkerHolder< ThinkerBase > holder) :
	holder (holder)
{
}

bool ThinkerPresentBase::operator!= (const ThinkerPresentBase& other) const
{
	return holder != other.holder;
}

bool ThinkerPresentBase::operator== (const ThinkerPresentBase& other) const
{
	return holder == other.holder;
}

ThinkerPresentBase& ThinkerPresentBase::operator= (const ThinkerPresentBase & other) {
	if (this != &other) {
		holder = other.holder;
	}
	return *this;
}

bool ThinkerPresentBase::hopefullyCurrentThreadIsManager(const codeplace& cp) const
{
	return holder.isNull() ? true : holder->getManager().hopefullyCurrentThreadIsManager(cp);
}

ThinkerBase& ThinkerPresentBase::getThinkerBase() {
	hopefullyCurrentThreadIsManager(HERE);
	return *holder.data();
}

const ThinkerBase& ThinkerPresentBase::getThinkerBase() const {
	hopefullyCurrentThreadIsManager(HERE);
	return *holder.data();
}

bool ThinkerPresentBase::isCanceled() const
{
	hopefullyCurrentThreadIsManager(HERE);

	if (holder.isNull())
		return true;

	const ThinkerBase& thinker (getThinkerBase());
	ThinkerRunnerKeepalive runner (thinker.getManager().maybeGetRunnerForThinker(thinker));
	bool result = false;
	if (runner.isNull()) {
		result = (thinker.state == ThinkerBase::ThinkerCanceled);
	} else {
		result = runner->isCanceled();
	}
	return result;
}

bool ThinkerPresentBase::isFinished() const
{
	hopefullyCurrentThreadIsManager(HERE);

	if (holder.isNull())
		return false;

	const ThinkerBase& thinker (getThinkerBase());
	ThinkerRunnerKeepalive runner (thinker.getManager().maybeGetRunnerForThinker(thinker));
	bool result = false;
	if (runner.isNull()) {
		result = (thinker.state == ThinkerBase::ThinkerFinished);
	} else {
		result = runner->isFinished();
	}
	return result;
}

bool ThinkerPresentBase::isPaused() const
{
	hopefullyCurrentThreadIsManager(HERE);

	if (holder.isNull())
		return false;

	const ThinkerBase& thinker (getThinkerBase());
	ThinkerRunnerKeepalive runner (thinker.getManager().maybeGetRunnerForThinker(thinker));
	bool result = false;
	if (runner.isNull()) {
		result = (thinker.state == ThinkerBase::ThinkerPaused);
	} else {
		result = runner->isPaused();
	}
	return result;
}

void ThinkerPresentBase::cancel()
{
	hopefullyCurrentThreadIsManager(HERE);

	// See QFuture for precedent... you can call cancel() on a default constructed
	// QFuture object and it's a no-op
	if (holder.isNull())
		return;

	ThinkerBase& thinker (getThinkerBase());
	ThinkerRunnerKeepalive runner (thinker.getManager().maybeGetRunnerForThinker(thinker));
	if (runner.isNull()) {
		thinker.state = ThinkerBase::ThinkerCanceled;
	} else {
		// No need to enforceCancel at this point (which would cause a
		// synchronous pause of the worker thread that we'd like to avoid)
		// ...although unruly thinkers may seem to "leak" if they stall too
		// long before responding to wasPauseRequested() signals
		runner->requestCancelButAlreadyCanceledIsOkay(HERE);
	}
}

void ThinkerPresentBase::pause()
{
	hopefullyCurrentThreadIsManager(HERE);

	hopefully(not holder.isNull(), HERE); // what would it mean to pause a null?  What's precedent in QFuture?

	ThinkerBase& thinker (getThinkerBase());
	ThinkerRunnerKeepalive runner (thinker.getManager().maybeGetRunnerForThinker(thinker));
	if (runner.isNull()) {
		hopefully(thinker.state == ThinkerBase::ThinkerThinking, HERE);
		thinker.state = ThinkerBase::ThinkerPaused;
	} else {
		// If there is a pause, we should probably stop update signals and queue a
		// single update at the moment of resume
		runner->requestPause(HERE);
	}
}

void ThinkerPresentBase::resume()
{
	hopefullyCurrentThreadIsManager(HERE);

	hopefully(not holder.isNull(), HERE); // what would it mean to pause a null?  What's precedent in QFuture?

	ThinkerBase& thinker (getThinkerBase());
	ThinkerRunnerKeepalive runner (thinker.getManager().maybeGetRunnerForThinker(thinker));
	if (runner.isNull()) {
		hopefully(thinker.state == ThinkerBase::ThinkerPaused, HERE);
		thinker.state = ThinkerBase::ThinkerThinking;
	} else {
		// If there is a pause, we should probably stop update signals and queue a
		// single update at the moment of resume
		runner->requestResume(HERE);
	}
}

void ThinkerPresentBase::setPaused(bool paused)
{
	if (paused)
		resume();
	else
		pause();
}

void ThinkerPresentBase::togglePaused()
{
	if (isPaused())
		resume();
	else
		pause();
}

void ThinkerPresentBase::waitForFinished()
{
	hopefullyCurrentThreadIsManager(HERE);

	// Following QFuture's lead in having a single waitForFinished that works
	// for both canceled as well as non-canceled results.  They seem to throw an
	// exception if the future had been paused...
	hopefully(not isPaused(), HERE);

	ThinkerBase& thinker (getThinkerBase());

	ThinkerRunnerKeepalive runner (thinker.getManager().maybeGetRunnerForThinker(thinker));
	if (runner.isNull()) {
		// TODO: implement monitoring so we can tell when a runner
		// for this thinker gets picked up.
	} else {
		if (runner->isCanceled())
			runner->waitForCancel();
		else
			runner->requestFinishAndWaitForFinish(HERE);
	}
}

SnapshotPointerBase* ThinkerPresentBase::createSnapshotBase() const
{
	hopefullyCurrentThreadIsManager(HERE);

	const ThinkerBase& thinker (getThinkerBase());
	return static_cast< const SnapshottableBase* >(&thinker)->createSnapshotBase();
}

ThinkerPresentBase::~ThinkerPresentBase()
{
	hopefullyCurrentThreadIsManager(HERE);
}