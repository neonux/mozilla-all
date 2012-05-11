/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
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
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Darin Fisher <darin@meer.net>
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

#include "nsThreadManager.h"
#include "nsThread.h"
#include "nsThreadUtils.h"
#include "nsIClassInfoImpl.h"
#include "nsTArray.h"
#include "nsAutoPtr.h"
#include "nsCycleCollectorUtils.h"
#include "nsContentUtils.h"

#include <sys/time.h>

using namespace mozilla;

#ifdef XP_WIN
#include <windows.h>
DWORD gTLSThreadIDIndex = TlsAlloc();
#elif defined(NS_TLS)
NS_TLS mozilla::threads::ID gTLSThreadID = mozilla::threads::Generic;
#endif

typedef nsTArray< nsRefPtr<nsThread> > nsThreadArray;

//-----------------------------------------------------------------------------

static void
ReleaseObject(void *data)
{
  static_cast<nsISupports *>(data)->Release();
}

static PLDHashOperator
AppendAndRemoveThread(PRThread *key, nsRefPtr<nsThread> &thread, void *arg)
{
  nsThreadArray *threads = static_cast<nsThreadArray *>(arg);
  threads->AppendElement(thread);
  return PL_DHASH_REMOVE;
}

//-----------------------------------------------------------------------------

nsThreadManager nsThreadManager::sInstance;
bool nsThreadManager::sInitialized = false;

// statically allocated instance
NS_IMETHODIMP_(nsrefcnt) nsThreadManager::AddRef() { return 2; }
NS_IMETHODIMP_(nsrefcnt) nsThreadManager::Release() { return 1; }
NS_IMPL_CLASSINFO(nsThreadManager, NULL,
                  nsIClassInfo::THREADSAFE | nsIClassInfo::SINGLETON,
                  NS_THREADMANAGER_CID)
NS_IMPL_QUERY_INTERFACE1_CI(nsThreadManager, nsIThreadManager)
NS_IMPL_CI_INTERFACE_GETTER1(nsThreadManager, nsIThreadManager)

//-----------------------------------------------------------------------------

nsresult
nsThreadManager::Init()
{
  sInitialized = true;

  if (!mThreadsByPRThread.Init())
    return NS_ERROR_OUT_OF_MEMORY;

  if (PR_NewThreadPrivateIndex(&mCurThreadIndex, ReleaseObject) == PR_FAILURE)
    return NS_ERROR_FAILURE;

  mLock = new Mutex("nsThreadManager.mLock");

  mChromeZone.lock = PR_NewLock();
  for (size_t i = 0; i < JS_ZONE_CONTENT_LIMIT; i++) {
    Zone &zone = mContentZones[i];
    zone.lock = PR_NewLock();
    zone.thread = new nsThread(nsThread::GECKO_THREAD, 0);
    zone.thread->Init();
    zone.thread->GetPRThread(&zone.prThread);
  }
  mEverythingLocked = false;
  mAllocatedBitmask = 0;

  // Setup "main" thread
  mMainThread = mChromeZone.thread = new nsThread(nsThread::MAIN_THREAD, 0);
  if (!mMainThread)
    return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv = mMainThread->InitCurrentThread();
  if (NS_FAILED(rv)) {
    mMainThread = nsnull;
    return rv;
  }

  // We need to keep a pointer to the current thread, so we can satisfy
  // GetIsMainThread calls that occur post-Shutdown.
  mMainThread->GetPRThread(&mMainPRThread);

  mMainThreadStackPosition = 0;

#ifdef XP_WIN
  TlsSetValue(gTLSThreadIDIndex, (void*) mozilla::threads::Main);
#elif defined(NS_TLS)
  gTLSThreadID = mozilla::threads::Main;
#endif

  mCantLockNewContent = 0;

  mInitialized = true;
  return NS_OK;
}

void
nsThreadManager::Shutdown()
{
  NS_ASSERTION(NS_IsMainThread(), "shutdown not called from main thread");

  // Prevent further access to the thread manager (no more new threads!)
  //
  // XXX What happens if shutdown happens before NewThread completes?
  //     Fortunately, NewThread is only called on the main thread for now.
  //
  mInitialized = false;

  // Empty the main thread event queue before we begin shutting down threads.
  NS_ProcessPendingEvents(mMainThread);

  // We gather the threads from the hashtable into a list, so that we avoid
  // holding the hashtable lock while calling nsIThread::Shutdown.
  nsThreadArray threads;
  {
    MutexAutoLock lock(*mLock);
    mThreadsByPRThread.Enumerate(AppendAndRemoveThread, &threads);
  }

  // It's tempting to walk the list of threads here and tell them each to stop
  // accepting new events, but that could lead to badness if one of those
  // threads is stuck waiting for a response from another thread.  To do it
  // right, we'd need some way to interrupt the threads.
  // 
  // Instead, we process events on the current thread while waiting for threads
  // to shutdown.  This means that we have to preserve a mostly functioning
  // world until such time as the threads exit.

  // Shutdown all threads that require it (join with threads that we created).
  for (PRUint32 i = 0; i < threads.Length(); ++i) {
    nsThread *thread = threads[i];
    if (thread->ShutdownRequired())
      thread->Shutdown();
  }

  // In case there are any more events somehow...
  NS_ProcessPendingEvents(mMainThread);

  // There are no more background threads at this point.

  // Clear the table of threads.
  {
    MutexAutoLock lock(*mLock);
    mThreadsByPRThread.Clear();
  }

  // Normally thread shutdown clears the observer for the thread, but since the
  // main thread is special we do it manually here after we're sure all events
  // have been processed.
  mMainThread->SetObserver(nsnull);
  mMainThread->ClearObservers();

  // Release main thread object.
  mMainThread = nsnull;
  mLock = nsnull;

  // Remove the TLS entry for the main thread.
  PR_SetThreadPrivate(mCurThreadIndex, nsnull);

  sInitialized = false;
}

void
nsThreadManager::RegisterCurrentThread(nsThread *thread)
{
  NS_ASSERTION(thread->GetPRThread() == PR_GetCurrentThread(), "bad thread");

  MutexAutoLock lock(*mLock);

  mThreadsByPRThread.Put(thread->GetPRThread(), thread);  // XXX check OOM?

  NS_ADDREF(thread);  // for TLS entry
  PR_SetThreadPrivate(mCurThreadIndex, thread);
}

void
nsThreadManager::UnregisterCurrentThread(nsThread *thread)
{
  NS_ASSERTION(thread->GetPRThread() == PR_GetCurrentThread(), "bad thread");

  MutexAutoLock lock(*mLock);

  mThreadsByPRThread.Remove(thread->GetPRThread());

  PR_SetThreadPrivate(mCurThreadIndex, nsnull);
  // Ref-count balanced via ReleaseObject
}

nsThread *
nsThreadManager::GetCurrentThread()
{
  // read thread local storage
  void *data = PR_GetThreadPrivate(mCurThreadIndex);
  if (data)
    return static_cast<nsThread *>(data);

  if (!mInitialized) {
    return nsnull;
  }

  // OK, that's fine.  We'll dynamically create one :-)
  nsRefPtr<nsThread> thread = new nsThread(nsThread::OTHER_THREAD, 0);
  if (!thread || NS_FAILED(thread->InitCurrentThread()))
    return nsnull;

  return thread.get();  // reference held in TLS
}

NS_IMETHODIMP
nsThreadManager::NewThread(PRUint32 creationFlags,
                           PRUint32 stackSize,
                           nsIThread **result)
{
  // No new threads during Shutdown
  NS_ENSURE_TRUE(mInitialized, NS_ERROR_NOT_INITIALIZED);

  nsThread *thr = new nsThread(nsThread::OTHER_THREAD, stackSize);
  if (!thr)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(thr);

  nsresult rv = thr->Init();
  if (NS_FAILED(rv)) {
    NS_RELEASE(thr);
    return rv;
  }

  // At this point, we expect that the thread has been registered in mThread;
  // however, it is possible that it could have also been replaced by now, so
  // we cannot really assert that it was added.

  *result = thr;
  return NS_OK;
}

NS_IMETHODIMP
nsThreadManager::GetThreadFromPRThread(PRThread *thread, nsIThread **result)
{
  // Keep this functioning during Shutdown
  NS_ENSURE_TRUE(mMainThread, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_ARG_POINTER(thread);

  nsRefPtr<nsThread> temp;
  {
    MutexAutoLock lock(*mLock);
    mThreadsByPRThread.Get(thread, getter_AddRefs(temp));
  }

  NS_IF_ADDREF(*result = temp);
  return NS_OK;
}

NS_IMETHODIMP
nsThreadManager::GetMainThread(nsIThread **result)
{
  // Keep this functioning during Shutdown
  NS_ENSURE_TRUE(mMainThread, NS_ERROR_NOT_INITIALIZED);
  NS_ADDREF(*result = mMainThread);
  return NS_OK;
}

NS_IMETHODIMP
nsThreadManager::GetCurrentThread(nsIThread **result)
{
  // Keep this functioning during Shutdown
  NS_ENSURE_TRUE(mMainThread, NS_ERROR_NOT_INITIALIZED);
  *result = GetCurrentThread();
  if (!*result)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*result);
  return NS_OK;
}

NS_IMETHODIMP
nsThreadManager::GetIsMainThread(bool *result)
{
  // This method may be called post-Shutdown

  *result = (PR_GetCurrentThread() == mMainPRThread);
  return NS_OK;
}

NS_IMETHODIMP
nsThreadManager::GetIsCycleCollectorThread(bool *result)
{
  *result = bool(NS_IsCycleCollectorThread());
  return NS_OK;
}

void
nsThreadManager::SaveLock(SavedZone &v)
{
  v.depth = v.zone->depth;
  v.sticky = v.zone->sticky;
  v.zone->depth = 0;
  v.zone->sticky = false;
  v.zone->owner = NULL;
  v.zone->stalled = false;
  v.zone->unlockCount++;
  PR_Unlock(v.zone->lock);
}

void
nsThreadManager::RestoreLock(SavedZone &v, PRThread *current)
{
  PR_Lock(v.zone->lock);
  MOZ_ASSERT(v.zone->owner == NULL);
  v.zone->owner = current;
  v.zone->depth = v.depth;
  v.zone->sticky = v.sticky;
}

extern bool NS_CanUnstickLocks();

///////////////////////////
// BEGIN PROFILING STUFF //
///////////////////////////

struct LockProfile
{
  const char *name;
  PRInt32 count;

  LockProfile(const char *name);
  void Bump();
};

LockProfile gLockContent("LockContent");
LockProfile gLockChrome("LockChrome");
LockProfile gRelockContent("RelockContent");
LockProfile gRelockChrome("RelockChrome");
LockProfile gTryLockContent("TryLockContent");
LockProfile gTryLockChrome("TryLockChrome");
LockProfile gTryRelockContent("TryRelockContent");
LockProfile gTryRelockChrome("TryRelockChrome");

LockProfile *gProfileList[] = {
  &gLockContent,
  &gLockChrome,
  &gRelockContent,
  &gRelockChrome,
  &gTryLockContent,
  &gTryLockChrome,
  &gTryRelockContent,
  &gTryRelockChrome,
  NULL
};

LockProfile::LockProfile(const char *name)
  : name(name), count(0)
{}

void
LockProfile::Bump()
{
  PR_ATOMIC_INCREMENT(&count);
}

void NS_PrintLockProfiles()
{
  LockProfile **plock = gProfileList;
  while (*plock) {
    printf("Lock %s: %d\n", (*plock)->name, (*plock)->count);
    plock++;
  }
  fflush(stdout);
}

/////////////////////////
// END PROFILING STUFF //
/////////////////////////

NS_IMETHODIMP
nsThreadManager::LockZone(PRInt32 zone_, bool sticky)
{
  MOZ_ASSERT(NS_IsExecuteThread());
  MOZ_ASSERT_IF(sticky, zone_ >= JS_ZONE_CONTENT_START);

  PRThread *current = PR_GetCurrentThread();

  Zone &zone = getZone(zone_);

  if (current == zone.owner) {
    if (zone_ == JS_ZONE_CHROME)
      gRelockChrome.Bump();
    else
      gRelockContent.Bump();

    if (!sticky || !zone.sticky)
      zone.depth++;
    if (sticky)
      zone.sticky = true;
  } else {
    if (zone_ == JS_ZONE_CHROME)
      gLockChrome.Bump();
    else
      gLockContent.Bump();

    // XXX reenable
    /*
#ifdef DEBUG
    bool hasLockedContent = false;
    for (size_t i = 0; i < mContentZoneCount; i++) {
      if (getZone(i).owner == current)
        hasLockedContent = true;
    }
    MOZ_ASSERT_IF(zone_ != JS_ZONE_CHROME && hasLockedContent,
                  JS_GetExecutingContentScriptZone() == JS_ZONE_NONE);
#endif
    */

    nsTArray<SavedZone> relockZones;
    if (zone_ >= JS_ZONE_CONTENT_START) {
      if (mChromeZone.owner == current) {
        MOZ_ASSERT(!mCantLockNewContent);
        relockZones.AppendElement(&mChromeZone);
      }
      for (size_t i = 0; i < zone_; i++) {
        if (getZone(i).owner == current)
          relockZones.AppendElement(&getZone(i));
      }
    }

    MOZ_ASSERT_IF(mEverythingLocked, relockZones.Length() == 0);

    for (size_t i = 0; i < relockZones.Length(); i++)
      SaveLock(relockZones[i]);

    if (zone.waiting) {
      struct timespec ts;
      ts.tv_sec = 0;
      ts.tv_nsec = 400 * 1000;
      nanosleep(&ts, &ts);
    }

    PR_Lock(zone.lock);
    MOZ_ASSERT(zone.owner == NULL);
    zone.owner = current;
    zone.depth = 1;
    if (sticky)
      zone.sticky = true;

    for (int i = relockZones.Length() - 1; i >= 0; i--)
      RestoreLock(relockZones[i], current);
  }
  return NS_OK;
}

static const uint32_t TRY_LOCK_MILLIS = 1000;

NS_IMETHODIMP
nsThreadManager::TryLockZone(PRInt32 zone_, bool sticky, bool *result)
{
  MOZ_ASSERT(NS_IsExecuteThread());
  MOZ_ASSERT_IF(sticky, zone_ >= JS_ZONE_CONTENT_START);

  PRThread *current = PR_GetCurrentThread();

  Zone &zone = getZone(zone_);

  if (current == zone.owner) {
    if (zone_ == JS_ZONE_CHROME)
      gTryRelockChrome.Bump();
    else
      gTryRelockContent.Bump();

    if (!sticky || !zone.sticky)
      zone.depth++;
    if (sticky)
      zone.sticky = true;
    *result = true;
    return NS_OK;
  }

  if (zone_ == JS_ZONE_CHROME)
    gTryLockChrome.Bump();
  else
    gTryLockContent.Bump();

  if (zone.stalled && zone.owner) {
    NS_DumpBacktrace("STALL");
    *result = false;
  } else {
    bool unlockChrome = mChromeZone.owner == current && !mCantLockNewContent;

    SavedZone restoreChrome(&mChromeZone);
    if (unlockChrome)
      SaveLock(restoreChrome);

    size_t unlockCount = zone.unlockCount;

    zone.waiting = true;
    bool success = PR_TryLock(zone.lock, TRY_LOCK_MILLIS * 1000);
    zone.waiting = false;

    if (success) {
      MOZ_ASSERT(zone.owner == NULL);
      zone.owner = current;
      zone.depth++;
      zone.stalled = false;
      if (sticky)
        zone.sticky = true;
      *result = true;
    } else {
      zone.stalled = true;
      printf("ZONE %p %d STALLED %p %d\n", current, zone_, zone.owner, (int) (zone.unlockCount - unlockCount));
      *result = false;
    }

    if (unlockChrome)
      RestoreLock(restoreChrome, current);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsThreadManager::UnlockZone(PRInt32 zone_)
{
  Zone &zone = getZone(zone_);

  MOZ_ASSERT(zone.owner == PR_GetCurrentThread());
  MOZ_ASSERT(zone.depth > 0);
  MOZ_ASSERT_IF(zone.sticky, zone.depth > 1);
  if (--zone.depth == 0) {
    MOZ_ASSERT(!mEverythingLocked);
    MOZ_ASSERT_IF(zone_ == JS_ZONE_CHROME, !mCantLockNewContent);
    zone.owner = NULL;
    zone.stalled = false;
    zone.unlockCount++;
    PR_Unlock(zone.lock);

    int stackDummy;
    PRThread *current = PR_GetCurrentThread();
    if (current == mMainPRThread) {
      mMainThreadStackPosition = (uintptr_t) &stackDummy;
    } else {
      for (int i = JS_ZONE_CHROME; i < JS_ZONE_CONTENT_LIMIT; i++) {
        Zone &zone = (i == JS_ZONE_CHROME) ? mChromeZone : mContentZones[i];
        if (zone.prThread == current) {
          zone.threadStackPosition = (uintptr_t) &stackDummy;
          break;
        }
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsThreadManager::ZoneLockDepth(PRInt32 zone_, PRUint32 *result, bool *psticky)
{
  Zone &zone = getZone(zone_);

  // OK to race on zone.owner, other threads cannot change it to/from the current thread.
  if (zone.owner == PR_GetCurrentThread()) {
    *result = zone.sticky ? zone.depth - 1 : zone.depth;
    *psticky = zone.sticky;
  } else {
    *result = 0;
    *psticky = false;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsThreadManager::IsOwningThread(PRInt32 zone_, bool *result)
{
  if (zone_ == JS_ZONE_NONE) {
    *result = true;
  } else {
    Zone &zone = getZone(zone_);
    *result = (PR_GetCurrentThread() == zone.owner);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsThreadManager::FindThreadBitmask(bool *pchrome, PRUint64 *pcontentBitmask)
{
  *pchrome = false;
  *pcontentBitmask = 0;

  PRThread *current = PR_GetCurrentThread();

  if (current == mChromeZone.owner)
    *pchrome = true;

  for (size_t i = 0; i < JS_ZONE_CONTENT_LIMIT; i++) {
    if (getZone(i).owner == current)
      SetBit(pcontentBitmask, i);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsThreadManager::UnstickAllContent()
{
  PRThread *current = PR_GetCurrentThread();

  for (size_t i = 0; i < JS_ZONE_CONTENT_LIMIT; i++) {
    Zone &zone = getZone(i);
    if (current == zone.owner) {
      MOZ_ASSERT(zone.sticky);
      zone.sticky = false;
      if (--zone.depth == 0) {
        MOZ_ASSERT(!mEverythingLocked);
        zone.owner = NULL;
        zone.stalled = false;
        zone.unlockCount++;
        PR_Unlock(zone.lock);
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsThreadManager::LockEverything(bool *psuccess)
{
  uint64_t bitmask;
  {
    LockZone(JS_ZONE_CHROME, false);
    bitmask = mAllocatedBitmask;
    UnlockZone(JS_ZONE_CHROME);
  }

  for (int zone = JS_ZONE_CONTENT_LIMIT - 1; zone >= 0; zone--) {
    if (!HasBit(bitmask, zone))
      continue;
    bool succeeded;
    TryLockZone(zone, false, &succeeded);
    if (!succeeded) {
      for (zone++; zone < JS_ZONE_CONTENT_LIMIT; zone++) {
        if (HasBit(bitmask, zone))
          UnlockZone(zone);
      }
      *psuccess = false;
      return NS_OK;
    }
  }

  LockZone(JS_ZONE_CHROME, false);

  if (bitmask != mAllocatedBitmask) {
    for (int zone = 0; zone < JS_ZONE_CONTENT_LIMIT; zone++) {
      if (HasBit(bitmask, zone))
        UnlockZone(zone);
    }
    UnlockZone(JS_ZONE_CHROME);
    *psuccess = false;
    return NS_OK;
  }

  MOZ_ASSERT(!mEverythingLocked);
  mEverythingLocked = true;

  *psuccess = true;
  return NS_OK;
}

NS_IMETHODIMP
nsThreadManager::IsEverythingLocked(bool *psuccess)
{
  *psuccess = mEverythingLocked || !sInitialized;
  return NS_OK;
}

NS_IMETHODIMP
nsThreadManager::UnlockEverything()
{
  MOZ_ASSERT(mEverythingLocked);
  mEverythingLocked = false;

  for (int zone = 0; zone < JS_ZONE_CONTENT_LIMIT; zone++) {
    if (HasBit(mAllocatedBitmask, zone))
      UnlockZone(zone);
  }

  UnlockZone(JS_ZONE_CHROME);

  return NS_OK;
}

NS_IMETHODIMP
nsThreadManager::NativeStackTopForThread(PRThread *thread, PRUint64 *pstack)
{
  if (thread == mMainPRThread) {
    *pstack = (PRUint64) mMainThreadStackPosition;
    return NS_OK;
  }

  for (int i = JS_ZONE_CHROME; i < JS_ZONE_CONTENT_LIMIT; i++) {
    Zone &zone = (i == JS_ZONE_CHROME) ? mChromeZone : mContentZones[i];
    if (zone.prThread == thread) {
      *pstack = (PRUint64) zone.threadStackPosition;
      return NS_OK;
    }
  }

  *pstack = 0;
  return NS_OK;
}

NS_IMETHODIMP
nsThreadManager::BeginCantLockNewContent()
{
  MOZ_ASSERT(NS_IsChromeOwningThread());
  mCantLockNewContent++;
  return NS_OK;
}

NS_IMETHODIMP
nsThreadManager::EndCantLockNewContent()
{
  MOZ_ASSERT(NS_IsChromeOwningThread());
  MOZ_ASSERT(mCantLockNewContent > 0);
  mCantLockNewContent--;
  return NS_OK;
}

NS_IMETHODIMP
nsThreadManager::CanLockNewContent(bool *pres)
{
  MOZ_ASSERT(NS_IsChromeOwningThread());
  *pres = !mCantLockNewContent && !mEverythingLocked;
  return NS_OK;
}

NS_IMETHODIMP
nsThreadManager::FindExecuteThreadZone(PRInt32 *pzone)
{
  *pzone = (PRInt32) JS_ZONE_NONE;

  PRThread *current = PR_GetCurrentThread();

  if (current == mChromeZone.prThread) {
    *pzone = (PRInt32) JS_ZONE_CHROME;
    return NS_OK;
  }

  for (size_t i = 0; i < JS_ZONE_CONTENT_LIMIT; i++) {
    if (current == getZone(i).prThread) {
      *pzone = i;
      return NS_OK;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsThreadManager::GetExecuteThread(PRInt32 zone_, nsIThread **pthread)
{
  JSZoneId zone = (JSZoneId) zone_;
  MOZ_ASSERT(zone != JS_ZONE_NONE);

  *pthread = getZone(zone_).thread;
  NS_ADDREF(*pthread);
  return NS_OK;
}

NS_IMETHODIMP
nsThreadManager::AllocateContentZone(PRInt32 *pzone)
{
  MOZ_ASSERT(NS_IsChromeOwningThread());
  MOZ_ASSERT(NS_CanUnstickLocks());

  PRThread *current = PR_GetCurrentThread();

  *pzone = JS_ZONE_CONTENT_LIMIT;
  for (size_t i = 0; i < JS_ZONE_CONTENT_LIMIT; i++) {
    if (!HasBit(mAllocatedBitmask, i) &&
        (getZone(i).owner == current || getZone(i).owner == NULL) /* XXX: race? */) {
      *pzone = i;
      break;
    }
  }
  MOZ_ASSERT(*pzone < JS_ZONE_CONTENT_LIMIT);

  SetBit(&mAllocatedBitmask, *pzone);

  Zone &zone = getZone(*pzone);

  if (zone.owner != current) {
    PR_Lock(zone.lock);
    zone.owner = PR_GetCurrentThread();
    zone.depth = 1;
    zone.sticky = true;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsThreadManager::FreeContentZone(PRInt32 zone_)
{
  MOZ_ASSERT(NS_IsChromeOwningThread());
  MOZ_ASSERT(zone_ >= JS_ZONE_CONTENT_START && zone_ < JS_ZONE_CONTENT_LIMIT);
  MOZ_ASSERT(getZone(zone_).owner == PR_GetCurrentThread());

  MOZ_ASSERT(HasBit(mAllocatedBitmask, zone_));
  ClearBit(&mAllocatedBitmask, zone_);

  return NS_OK;
}
