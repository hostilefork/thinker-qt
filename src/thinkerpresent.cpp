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
	return holder->getManager().hopefullyCurrentThreadIsManager(cp);
}

ThinkerBase& ThinkerPresentBase::getThinkerBase() {
	hopefullyCurrentThreadIsManager(HERE);
	return *holder.data();
}

const ThinkerBase& ThinkerPresentBase::getThinkerBase() const {
	hopefullyCurrentThreadIsManager(HERE);
	return *holder.data();
}

bool ThinkerPresentBase::isCanceled () const
{
	// TODO: implement
	return false;
}

bool ThinkerPresentBase::isFinished () const
{
	// TODO: implement
	return false;
}

bool ThinkerPresentBase::isPaused () const
{
	// TODO: implement
	return false;
}

bool ThinkerPresentBase::isRunning () const
{
	// TODO: implement
	return false;
}

void ThinkerPresentBase::cancel ()
{
	// See QFuture for precedent... you can call cancel() on a default constructed
	// QFuture object and it's a no-op
	if (holder.isNull())
		return;

	ThinkerBase& thinker (getThinkerBase());

	ThinkerRunner* runner (thinker.getManager().maybeGetRunnerForThinker(thinker));
	if (runner != NULL) {
		// No need to enforceCancel at this point (which would cause a
		// synchronous pause of the worker thread that we'd like to avoid)
		// ...although unruly thinkers may seem to "leak" if they stall too
		// long before responding to isPauseRequested() signals

		// The bulkhead may have become invalidated which means it
		// could already be aborted.
		runner->requestCancelButAlreadyCanceledIsOkay(HERE);
	} else {
		thinker.state = ThinkerBase::ThinkerCanceled;
	}
}

void ThinkerPresentBase::pause ()
{
	// TODO: implement
}

void ThinkerPresentBase::resume ()
{
	// TODO: implement
}

void ThinkerPresentBase::setPaused ( bool paused )
{
	// TODO: implement
}

void ThinkerPresentBase::togglePaused ()
{
	// TODO: implement
}

void ThinkerPresentBase::waitForFinished ()
{
	// Following QFuture's lead in having a single waitForFinished that works
	// for both canceled as well as non-canceled results.  They seem to throw an
	// exception if the future had been paused...
	hopefully(not isPaused(), HERE);

	ThinkerBase& thinker (getThinkerBase());

	ThinkerRunner* runner (thinker.getManager().maybeGetRunnerForThinker(thinker));
	if (runner != NULL) {
		if (runner->isCanceled())
			runner->waitForCancel();
		else
			runner->requestFinishAndWaitForFinish(HERE);
	}
}

SnapshotPointerBase* ThinkerPresentBase::createSnapshotBase() const
{
	const ThinkerBase& thinker (getThinkerBase());

	hopefullyCurrentThreadIsManager(HERE);
	return static_cast< const SnapshottableBase* >(&thinker)->createSnapshotBase();
}

ThinkerPresentBase::~ThinkerPresentBase()
{
	// we leave wasAttachedToPresent as true here...
	// it may still be running and we want readable()/writable() to still work
	// from the thinker thread
}