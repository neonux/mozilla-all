/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Widget code.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@meer.net> (original author)
 *   Mats Palmgren <mats.palmgren@bredband.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "base/message_loop.h"

#include "nsBaseAppShell.h"
#include "nsThreadUtils.h"
#include "nsIObserverService.h"
#include "nsServiceManagerUtils.h"
#include "mozilla/Services.h"

// When processing the next thread event, the appshell may process native
// events (if not in performance mode), which can result in suppressing the
// next thread event for at most this many ticks:
#define THREAD_EVENT_STARVATION_LIMIT PR_MillisecondsToInterval(20)

NS_IMPL_THREADSAFE_ISUPPORTS3(nsBaseAppShell, nsIAppShell, nsIThreadObserver,
                              nsIObserver)

nsBaseAppShell::nsBaseAppShell()
  : mSuspendNativeCount(0)
  , mEventloopNestingLevel(0)
  , mBlockedWait(nsnull)
  , mFavorPerf(0)
  , mNativeEventPending(0)
  , mStarvationDelay(0)
  , mSwitchTime(0)
  , mLastNativeEventTime(0)
  , mEventloopNestingState(eEventloopNone)
  , mRunning(false)
  , mExiting(false)
  , mBlockNativeEvent(false)
{
}

nsBaseAppShell::~nsBaseAppShell()
{
  NS_ASSERTION(mSyncSections.IsEmpty(), "Must have run all sync sections");
}

nsresult
nsBaseAppShell::Init()
{
  // Configure ourselves as an observer for the current thread:

  nsCOMPtr<nsIThreadInternal> threadInt =
      do_QueryInterface(NS_GetCurrentThread());
  NS_ENSURE_STATE(threadInt);

  threadInt->SetObserver(this);

  nsCOMPtr<nsIObserverService> obsSvc =
    mozilla::services::GetObserverService();
  if (obsSvc)
    obsSvc->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
  return NS_OK;
}

// Called by nsAppShell's native event callback
void
nsBaseAppShell::NativeEventCallback()
{
  PRInt32 hasPending = PR_ATOMIC_SET(&mNativeEventPending, 0);
  if (hasPending == 0)
    return;

  // If DoProcessNextNativeEvent is on the stack, then we assume that we can
  // just unwind and let nsThread::ProcessNextEvent process the next event.
  // However, if we are called from a nested native event loop (maybe via some
  // plug-in or library function), then go ahead and process Gecko events now.
  if (mEventloopNestingState == eEventloopXPCOM) {
    mEventloopNestingState = eEventloopOther;
    // XXX there is a tiny risk we will never get a new NativeEventCallback,
    // XXX see discussion in bug 389931.
    return;
  }

  // nsBaseAppShell::Run is not being used to pump events, so this may be
  // our only opportunity to process pending gecko events.

  nsIThread *thread = NS_GetCurrentThread();
  bool prevBlockNativeEvent = mBlockNativeEvent;
  if (mEventloopNestingState == eEventloopOther) {
    if (!NS_HasPendingEvents(thread))
      return;
    // We're in a nested native event loop and have some gecko events to
    // process.  While doing that we block processing native events from the
    // appshell - instead, we want to get back to the nested native event
    // loop ASAP (bug 420148).
    mBlockNativeEvent = true;
  }

  MOZ_ASSERT(!NS_IsChromeOwningThread());

  ++mEventloopNestingLevel;
  EventloopNestingState prevVal = mEventloopNestingState;
  NS_ProcessPendingEvents(thread, THREAD_EVENT_STARVATION_LIMIT);
  mProcessedGeckoEvents = true;
  mEventloopNestingState = prevVal;
  mBlockNativeEvent = prevBlockNativeEvent;

  // Continue processing pending events later (we don't want to starve the
  // embedders event loop).
  if (NS_HasPendingEvents(thread))
    DoProcessMoreGeckoEvents();

  --mEventloopNestingLevel;
}

// Note, this is currently overidden on windows, see comments in nsAppShell for
// details. 
void
nsBaseAppShell::DoProcessMoreGeckoEvents()
{
  OnDispatchedEvent(nsnull);
}


// Main thread via OnProcessNextEvent below
bool
nsBaseAppShell::DoProcessNextNativeEvent(bool mayWait, PRUint32 recursionDepth)
{
  nsAutoUnlockEverything unlock;

  // The next native event to be processed may trigger our NativeEventCallback,
  // in which case we do not want it to process any thread events since we'll
  // do that when this function returns.
  //
  // If the next native event is not our NativeEventCallback, then we may end
  // up recursing into this function.
  //
  // However, if the next native event is not our NativeEventCallback, but it
  // results in another native event loop, then our NativeEventCallback could
  // fire and it will see mEventloopNestingState as eEventloopOther.
  //
  EventloopNestingState prevVal = mEventloopNestingState;
  mEventloopNestingState = eEventloopXPCOM;

  ++mEventloopNestingLevel;

  bool result = ProcessNextNativeEvent(mayWait);

  // Make sure that any sync sections registered during this most recent event
  // are run now. This is not considered a stable state because we're not back
  // to the event loop yet.
  RunSyncSections(false, recursionDepth);

  --mEventloopNestingLevel;

  mEventloopNestingState = prevVal;
  return result;
}

//-------------------------------------------------------------------------
// nsIAppShell methods:

NS_IMETHODIMP
nsBaseAppShell::Run(void)
{
  NS_ENSURE_STATE(!mRunning);  // should not call Run twice
  mRunning = true;

  nsIThread *thread = NS_GetCurrentThread();

  MessageLoop::current()->Run();

  NS_ProcessPendingEvents(thread);

  mRunning = false;
  return NS_OK;
}

NS_IMETHODIMP
nsBaseAppShell::Exit(void)
{
  if (mRunning && !mExiting) {
    MessageLoop::current()->Quit();
  }
  mExiting = true;
  return NS_OK;
}

NS_IMETHODIMP
nsBaseAppShell::FavorPerformanceHint(bool favorPerfOverStarvation,
                                     PRUint32 starvationDelay)
{
  mStarvationDelay = PR_MillisecondsToInterval(starvationDelay);
  if (favorPerfOverStarvation) {
    ++mFavorPerf;
  } else {
    --mFavorPerf;
    mSwitchTime = PR_IntervalNow();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsBaseAppShell::SuspendNative()
{
  ++mSuspendNativeCount;
  return NS_OK;
}

NS_IMETHODIMP
nsBaseAppShell::ResumeNative()
{
  --mSuspendNativeCount;
  NS_ASSERTION(mSuspendNativeCount >= 0, "Unbalanced call to nsBaseAppShell::ResumeNative!");
  return NS_OK;
}

NS_IMETHODIMP
nsBaseAppShell::GetEventloopNestingLevel(PRUint32* aNestingLevelResult)
{
  NS_ENSURE_ARG_POINTER(aNestingLevelResult);

  *aNestingLevelResult = mEventloopNestingLevel;

  return NS_OK;
}

//-------------------------------------------------------------------------
// nsIThreadObserver methods:

// Called from any thread
NS_IMETHODIMP
nsBaseAppShell::OnDispatchedEvent(nsIThreadInternal *thr)
{
  if (mBlockNativeEvent)
    return NS_OK;

  PRInt32 lastVal = PR_ATOMIC_SET(&mNativeEventPending, 1);
  if (lastVal == 1)
    return NS_OK;

  // Returns on the main thread in NativeEventCallback above
  ScheduleNativeEventCallback();
  return NS_OK;
}

// Called from the main thread
NS_IMETHODIMP
nsBaseAppShell::OnProcessNextEvent(nsIThreadInternal *thr, bool mayWait,
                                   PRUint32 recursionDepth)
{
  if (mBlockNativeEvent) {
    if (!mayWait)
      return NS_OK;
    // Hmm, we're in a nested native event loop and would like to get
    // back to it ASAP, but it seems a gecko event has caused us to
    // spin up a nested XPCOM event loop (eg. modal window), so we
    // really must start processing native events here again.
    mBlockNativeEvent = false;
    if (NS_HasPendingEvents(thr))
      OnDispatchedEvent(thr); // in case we blocked it earlier
  }

  PRIntervalTime start = PR_IntervalNow();
  PRIntervalTime limit = THREAD_EVENT_STARVATION_LIMIT;

  // Unblock outer nested wait loop (below).
  if (mBlockedWait)
    *mBlockedWait = false;

  bool *oldBlockedWait = mBlockedWait;
  mBlockedWait = &mayWait;

  // When mayWait is true, we need to make sure that there is an event in the
  // thread's event queue before we return.  Otherwise, the thread will block
  // on its event queue waiting for an event.
  bool needEvent = mayWait;
  // Reset prior to invoking DoProcessNextNativeEvent which might cause
  // NativeEventCallback to process gecko events.
  mProcessedGeckoEvents = false;

  if (mFavorPerf <= 0 && start > mSwitchTime + mStarvationDelay) {
    // Favor pending native events
    PRIntervalTime now = start;
    bool keepGoing;
    do {
      mLastNativeEventTime = now;
      keepGoing = DoProcessNextNativeEvent(false, recursionDepth);
    } while (keepGoing && ((now = PR_IntervalNow()) - start) < limit);
  } else {
    // Avoid starving native events completely when in performance mode
    if (start - mLastNativeEventTime > limit) {
      mLastNativeEventTime = start;
      DoProcessNextNativeEvent(false, recursionDepth);
    }
  }

  while (!NS_HasPendingEvents(thr) && !mProcessedGeckoEvents) {
    // If we have been asked to exit from Run, then we should not wait for
    // events to process.  Note that an inner nested event loop causes
    // 'mayWait' to become false too, through 'mBlockedWait'.
    if (mExiting)
      mayWait = false;

    mLastNativeEventTime = PR_IntervalNow();
    if (!DoProcessNextNativeEvent(mayWait, recursionDepth) || !mayWait)
      break;
  }

  mBlockedWait = oldBlockedWait;

  // Make sure that the thread event queue does not block on its monitor, as
  // it normally would do if it did not have any pending events.  To avoid
  // that, we simply insert a dummy event into its queue during shutdown.
  if (needEvent && !mExiting && !NS_HasPendingEvents(thr)) {
    DispatchDummyEvent(thr);
  }

  // We're about to run an event, so we're in a stable state.
  RunSyncSections(true, recursionDepth);

  return NS_OK;
}

bool
nsBaseAppShell::DispatchDummyEvent(nsIThread* aTarget)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!mDummyEvent)
    mDummyEvent = new nsRunnable();

  return NS_SUCCEEDED(aTarget->Dispatch(mDummyEvent, NS_DISPATCH_NORMAL));
}

void
nsBaseAppShell::RunSyncSectionsInternal(bool aStable,
                                        PRUint32 aThreadRecursionLevel)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!mSyncSections.IsEmpty(), "Nothing to do!");

  // We've got synchronous sections. Run all of them that are are awaiting a
  // stable state if aStable is true (i.e. we really are in a stable state).
  // Also run the synchronous sections that are simply waiting for the right
  // combination of event loop nesting level and thread recursion level.
  // Note that a synchronous section could add another synchronous section, so
  // we don't remove elements from mSyncSections until all sections have been
  // run, or else we'll screw up our iteration. Any sync sections that are not
  // ready to be run are saved for later.

  nsTArray<SyncSection> pendingSyncSections;

  for (PRUint32 i = 0; i < mSyncSections.Length(); i++) {
    SyncSection& section = mSyncSections[i];
    if ((aStable && section.mStable) ||
        (!section.mStable &&
         section.mEventloopNestingLevel == mEventloopNestingLevel &&
         section.mThreadRecursionLevel == aThreadRecursionLevel)) {
      section.mRunnable->Run();
    }
    else {
      // Add to pending list.
      SyncSection* pending = pendingSyncSections.AppendElement();
      section.Forget(pending);
    }
  }

  mSyncSections.SwapElements(pendingSyncSections);
}

void
nsBaseAppShell::ScheduleSyncSection(nsIRunnable* aRunnable, bool aStable)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");

  nsIThread* thread = NS_GetCurrentThread();

  // Add this runnable to our list of synchronous sections.
  SyncSection* section = mSyncSections.AppendElement();
  section->mStable = aStable;
  section->mRunnable = aRunnable;

  // If aStable is false then this synchronous section is supposed to run before
  // the next event at the current nesting level. Record the event loop nesting
  // level and the thread recursion level so that the synchronous section will
  // run at the proper time.
  if (!aStable) {
    section->mEventloopNestingLevel = mEventloopNestingLevel;

    nsCOMPtr<nsIThreadInternal> threadInternal = do_QueryInterface(thread);
    NS_ASSERTION(threadInternal, "This should never fail!");

    PRUint32 recursionLevel;
    if (NS_FAILED(threadInternal->GetRecursionDepth(&recursionLevel))) {
      NS_ERROR("This should never fail!");
    }

    // Due to the weird way that the thread recursion counter is implemented we
    // subtract one from the recursion level if we have one.
    section->mThreadRecursionLevel = recursionLevel ? recursionLevel - 1 : 0;
  }

  // Ensure we've got a pending event, else the callbacks will never run.
  if (!NS_HasPendingEvents(thread) && !DispatchDummyEvent(thread)) {
    RunSyncSections(true, 0);
  }
}

// Called from the main thread
NS_IMETHODIMP
nsBaseAppShell::AfterProcessNextEvent(nsIThreadInternal *thr,
                                      PRUint32 recursionDepth)
{
  // We've just finished running an event, so we're in a stable state. 
  RunSyncSections(true, recursionDepth);
  return NS_OK;
}

NS_IMETHODIMP
nsBaseAppShell::Observe(nsISupports *subject, const char *topic,
                        const PRUnichar *data)
{
  NS_ASSERTION(!strcmp(topic, NS_XPCOM_SHUTDOWN_OBSERVER_ID), "oops");
  Exit();
  return NS_OK;
}

NS_IMETHODIMP
nsBaseAppShell::RunInStableState(nsIRunnable* aRunnable)
{
  ScheduleSyncSection(aRunnable, true);
  return NS_OK;
}

NS_IMETHODIMP
nsBaseAppShell::RunBeforeNextEvent(nsIRunnable* aRunnable)
{
  ScheduleSyncSection(aRunnable, false);
  return NS_OK;
}
