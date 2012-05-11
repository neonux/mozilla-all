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

#include "nsThreadUtils.h"
#include "jsfriendapi.h"

#include "mozilla/Mutex.h"

#ifdef MOZILLA_INTERNAL_API
# include "nsThreadManager.h"
#else
# include "nsXPCOMCIDInternal.h"
# include "nsIThreadManager.h"
# include "nsServiceManagerUtils.h"
#endif

#ifdef XP_WIN
#include <windows.h>
#endif

#ifndef XPCOM_GLUE_AVOID_NSPR

NS_IMPL_THREADSAFE_ISUPPORTS1(nsRunnable, nsIRunnable)
  
NS_IMETHODIMP
nsRunnable::Run()
{
  // Do nothing
  return NS_OK;
}

#endif  // XPCOM_GLUE_AVOID_NSPR

//-----------------------------------------------------------------------------

NS_METHOD
NS_NewThread(nsIThread **result, nsIRunnable *event, PRUint32 stackSize)
{
  nsCOMPtr<nsIThread> thread;
#ifdef MOZILLA_INTERNAL_API
  nsresult rv = nsThreadManager::get()->
      nsThreadManager::NewThread(0, stackSize, getter_AddRefs(thread));
#else
  nsresult rv;
  nsCOMPtr<nsIThreadManager> mgr =
      do_GetService(NS_THREADMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mgr->NewThread(0, stackSize, getter_AddRefs(thread));
#endif
  NS_ENSURE_SUCCESS(rv, rv);

  if (event) {
    rv = thread->Dispatch(event, NS_DISPATCH_NORMAL);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  *result = nsnull;
  thread.swap(*result);
  return NS_OK;
}

NS_METHOD
NS_GetCurrentThread(nsIThread **result)
{
#ifdef MOZILLA_INTERNAL_API
  return nsThreadManager::get()->nsThreadManager::GetCurrentThread(result);
#else
  nsresult rv;
  nsCOMPtr<nsIThreadManager> mgr =
      do_GetService(NS_THREADMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  return mgr->GetCurrentThread(result);
#endif
}

NS_METHOD
NS_GetMainThread(nsIThread **result)
{
#ifdef MOZILLA_INTERNAL_API
  return nsThreadManager::get()->nsThreadManager::GetMainThread(result);
#else
  nsresult rv;
  nsCOMPtr<nsIThreadManager> mgr =
      do_GetService(NS_THREADMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  return mgr->GetMainThread(result);
#endif
}

#ifndef MOZILLA_INTERNAL_API
bool NS_IsMainThread()
{
  bool result = false;
  nsCOMPtr<nsIThreadManager> mgr =
    do_GetService(NS_THREADMANAGER_CONTRACTID);
  if (mgr)
    mgr->GetIsMainThread(&result);
  return bool(result);
}
#elif defined(XP_WIN)
extern DWORD gTLSThreadIDIndex;
bool
NS_IsMainThread()
{
  return TlsGetValue(gTLSThreadIDIndex) == (void*) mozilla::threads::Main;
}
#elif !defined(NS_TLS)
bool NS_IsMainThread()
{
  bool result = false;
  nsThreadManager::get()->nsThreadManager::GetIsMainThread(&result);
  return bool(result);
}
#endif

NS_METHOD
NS_DispatchToCurrentThread(nsIRunnable *event)
{
  if (NS_IsExecuteThread())
    return NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL, JS_ZONE_CHROME);

#ifdef MOZILLA_INTERNAL_API
  nsIThread *thread = NS_GetCurrentThread();
  if (!thread) { return NS_ERROR_UNEXPECTED; }
#else
  nsCOMPtr<nsIThread> thread;
  nsresult rv = NS_GetCurrentThread(getter_AddRefs(thread));
  NS_ENSURE_SUCCESS(rv, rv);
#endif
  return thread->Dispatch(event, NS_DISPATCH_NORMAL);
}

NS_METHOD
NS_DispatchToMainThread(nsIRunnable *event, PRUint32 dispatchFlags, JSZoneId zone)
{
  nsCOMPtr<nsIThread> thread;
  nsresult rv;
  if (zone == JS_ZONE_CHROME)
    rv = NS_GetMainThread(getter_AddRefs(thread));
  else
    rv = NS_GetExecuteThread(zone, getter_AddRefs(thread));
  NS_ENSURE_SUCCESS(rv, rv);
  return thread->Dispatch(event, dispatchFlags);
}

#ifndef XPCOM_GLUE_AVOID_NSPR
NS_METHOD
NS_ProcessPendingEvents(nsIThread *thread, PRIntervalTime timeout)
{
  nsresult rv = NS_OK;

#ifdef MOZILLA_INTERNAL_API
  if (!thread) {
    thread = NS_GetCurrentThread();
    NS_ENSURE_STATE(thread);
  }
#else
  nsCOMPtr<nsIThread> current;
  if (!thread) {
    rv = NS_GetCurrentThread(getter_AddRefs(current));
    NS_ENSURE_SUCCESS(rv, rv);
    thread = current.get();
  }
#endif

  PRIntervalTime start = PR_IntervalNow();
  for (;;) {
    bool processedEvent;
    rv = thread->ProcessNextEvent(false, &processedEvent);
    if (NS_FAILED(rv) || !processedEvent)
      break;
    if (PR_IntervalNow() - start > timeout)
      break;
  }
  return rv;
}
#endif // XPCOM_GLUE_AVOID_NSPR

inline bool
hasPendingEvents(nsIThread *thread)
{
  bool val;
  return NS_SUCCEEDED(thread->HasPendingEvents(&val)) && val;
}

bool
NS_HasPendingEvents(nsIThread *thread)
{
  if (!thread) {
#ifndef MOZILLA_INTERNAL_API
    nsCOMPtr<nsIThread> current;
    NS_GetCurrentThread(getter_AddRefs(current));
    return hasPendingEvents(current);
#else
    thread = NS_GetCurrentThread();
    NS_ENSURE_TRUE(thread, false);
#endif
  }
  return hasPendingEvents(thread);
}

bool
NS_ProcessNextEvent(nsIThread *thread, bool mayWait)
{
#ifdef MOZILLA_INTERNAL_API
  if (!thread) {
    thread = NS_GetCurrentThread();
    NS_ENSURE_TRUE(thread, false);
  }
#else
  nsCOMPtr<nsIThread> current;
  if (!thread) {
    NS_GetCurrentThread(getter_AddRefs(current));
    NS_ENSURE_TRUE(current, false);
    thread = current.get();
  }
#endif

  bool val;
  return NS_SUCCEEDED(thread->ProcessNextEvent(mayWait, &val)) && val;
}

#ifdef MOZILLA_INTERNAL_API
nsIThread *
NS_GetCurrentThread() {
  return nsThreadManager::get()->GetCurrentThread();
}
#endif

#ifdef MOZILLA_INTERNAL_API
#define GET_MANAGER()                                   \
  nsThreadManager *mgr = NULL;                          \
  if (nsThreadManager::initialized())                   \
    mgr = nsThreadManager::get()
#else
#define GET_MANAGER()                                   \
  nsresult rv_;                                         \
  nsCOMPtr<nsIThreadManager> mgr =                      \
      do_GetService(NS_THREADMANAGER_CONTRACTID, &rv_)
#endif

nsAutoLockZone::nsAutoLockZone(JSZoneId zone)
  : zone(zone)
{
  GET_MANAGER();
  if (mgr)
    mgr->LockZone(zone, false);
}

nsAutoLockZone::~nsAutoLockZone()
{
  GET_MANAGER();
  if (mgr)
    mgr->UnlockZone(zone);
}

nsAutoUnlockZone::nsAutoUnlockZone(JSZoneId zone)
  : zone(zone)
{
  GET_MANAGER();
  bool sticky_;
  mgr->ZoneLockDepth(zone, &depth, &sticky_);
  MOZ_ASSERT(!sticky_);
  for (size_t i = 0; i < depth; i++)
    mgr->UnlockZone(zone);
}

nsAutoUnlockZone::~nsAutoUnlockZone()
{
  GET_MANAGER();
  for (size_t i = 0; i < depth; i++)
    mgr->LockZone(zone, false);
}

nsAutoTryLockZone::nsAutoTryLockZone(JSZoneId zone)
  : zone(zone), succeeded(false)
{
  GET_MANAGER();
  MOZ_ASSERT(mgr);
  mgr->TryLockZone(zone, false, &succeeded);
}

nsAutoTryLockZone::~nsAutoTryLockZone()
{
  GET_MANAGER();
  MOZ_ASSERT(mgr);
  if (succeeded)
    mgr->UnlockZone(zone);
}

JSZoneId
NS_FindExecuteThreadZone()
{
  GET_MANAGER();

  PRInt32 result;
  mgr->FindExecuteThreadZone(&result);
  return (JSZoneId) result;
}

bool
NS_CanBlockOnContent()
{
  return !NS_IsMainThread() && NS_FindExecuteThreadZone() != JS_ZONE_CHROME;
}

NS_METHOD
NS_GetExecuteThread(JSZoneId zone, nsIThread **result)
{
  GET_MANAGER();

  if (!mgr)
    return NS_ERROR_FAILURE;

  mgr->GetExecuteThread((long) zone, result);
  return NS_OK;
}

JSZoneId
NS_AllocateContentZone()
{
  GET_MANAGER();

  PRInt32 zone;
  mgr->AllocateContentZone(&zone);
  MOZ_ASSERT(zone >= JS_ZONE_CONTENT_START);
  return (JSZoneId) zone;
}

void
NS_FreeContentZone(JSZoneId zone)
{
  GET_MANAGER();

  mgr->FreeContentZone(zone);
}

#ifdef NS_DEBUG
void
NS_FindThreadBitmask(PRThread **pthread, bool *pchrome, PRUint64 *pcontentMask)
{
  *pthread = NULL;
  *pchrome = false;
  *pcontentMask = 0;

  GET_MANAGER();
  if (!mgr) {
    *pchrome = true;
    *pcontentMask = PRUint64(-1);
    return;
  }

  if (NS_IsExecuteThread())
    mgr->FindThreadBitmask(pchrome, pcontentMask);
  else
    *pthread = PR_GetCurrentThread();
}
#endif

JSBool
NS_IsOwningThread(JSZoneId zone)
{
  GET_MANAGER();
  if (!mgr)
    return true;
  bool result;
  mgr->IsOwningThread(zone, &result);

  if (result)
    return true;

  // The cycle collector thread only runs when another thread has acquired all
  // locks and is blocked on the collection finishing.
  return NS_IsCycleCollectorThread();
}

uint32_t
NS_ThreadLockDepth(JSZoneId zone)
{
  GET_MANAGER();
  PRUint32 depth;
  bool sticky_;
  mgr->ZoneLockDepth(zone, &depth, &sticky_);
  MOZ_ASSERT(!sticky_);
  return depth;
}

void
NS_LockZone(JSZoneId zone)
{
  GET_MANAGER();
  mgr->LockZone(zone, false);
}

JSBool
NS_TryLockZone(JSZoneId zone)
{
  GET_MANAGER();
  bool succeeded;
  mgr->TryLockZone(zone, false, &succeeded);
  return succeeded;
}

void
NS_UnlockZone(JSZoneId zone)
{
  GET_MANAGER();
  mgr->UnlockZone(zone);
}

nsAutoTryLockEverything::nsAutoTryLockEverything()
{
  MOZ_ASSERT(NS_IsChromeOwningThread());
  GET_MANAGER();

  mCount = JS_ZONE_CONTENT_LIMIT;

  for (int i = 0; i < mCount; i++)
    mLockedArray.AppendElement(false);

  for (int zone = mCount - 1; zone >= 0; zone--) {
    bool succeeded;
    mgr->TryLockZone(zone, false, &succeeded);
    mLockedArray[zone] = succeeded;
  }
}

nsAutoTryLockEverything::~nsAutoTryLockEverything()
{
  MOZ_ASSERT(NS_IsChromeOwningThread());
  GET_MANAGER();

  for (int zone = 0; zone < mCount; zone++) {
    if (mLockedArray[zone])
      mgr->UnlockZone(zone);
  }
}

JSBool NS_LockEverything()
{
  GET_MANAGER();

  if (!mgr)
    return true;

  bool success;
  mgr->LockEverything(&success);

  return success;
}

JSBool NS_IsEverythingLocked()
{
  GET_MANAGER();

  if (!mgr)
    return true;

  bool success;
  mgr->IsEverythingLocked(&success);

  return success;
}

void NS_UnlockEverything()
{
  GET_MANAGER();

  if (!mgr)
    return;

  mgr->UnlockEverything();
}

uintptr_t NS_FindNativeStackTopForThread(/*(PRThread*)*/ uintptr_t thread)
{
  GET_MANAGER();

  PRUint64 top;
  mgr->NativeStackTopForThread((PRThread*)thread, &top);

  return (uintptr_t) top;
}

static nsAutoLockChromeUnstickContent *gUnstickList = NULL;

#ifdef NS_DEBUG
bool NS_CanUnstickLocks()
{
  nsAutoLockChrome lock;
  PRThread *thread = PR_GetCurrentThread();
  nsAutoLockChromeUnstickContent *unstick = gUnstickList;
  while (unstick) {
    if (unstick->mThread == thread)
      return true;
    unstick = unstick->mPrev;
  }
  return false;
}
#endif

void
NS_StickContentLock(JSZoneId zone)
{
  MOZ_ASSERT(zone >= JS_ZONE_CONTENT_START);
  MOZ_ASSERT(NS_CanUnstickLocks());

  GET_MANAGER();

  if (!mgr)
    return;

  mgr->LockZone(zone, true);
}

bool
NS_TryStickContentLock(JSZoneId zone)
{
  MOZ_ASSERT(zone >= JS_ZONE_CONTENT_START);
  MOZ_ASSERT(NS_CanUnstickLocks());

  GET_MANAGER();

  if (!mgr)
    return true;

  bool succeeded;
  mgr->TryLockZone(zone, true, &succeeded);

  return succeeded;
}

nsAutoLockChromeUnstickContent::nsAutoLockChromeUnstickContent()
  : nsAutoLockZone(JS_ZONE_CHROME)
{
#ifdef NS_DEBUG
  PRThread *thread;
  bool chrome;
  PRUint64 contentMask;
  NS_FindThreadBitmask(&thread, &chrome, &contentMask);
  MOZ_ASSERT(chrome && contentMask == 0);
#endif
  mThread = PR_GetCurrentThread();
  mPrev = gUnstickList;
  gUnstickList = this;
}

struct ContextStickInfo
{
  JSContext *cx;
  ContextStickInfo *next;
};

ContextStickInfo *gContextSticks = NULL;

void NS_RegisterContextStick(JSContext *cx)
{
  MOZ_ASSERT(JS_GetContextThread(cx) == (uintptr_t) PR_GetCurrentThread());

  nsAutoLockChrome lock;

#ifdef DEBUG
  {
    ContextStickInfo *stick = gContextSticks;
    while (stick) {
      MOZ_ASSERT(stick->cx != cx);
      stick = stick->next;
    }
  }
#endif

  ContextStickInfo *stick = new ContextStickInfo();
  stick->cx = cx;
  stick->next = gContextSticks;
  gContextSticks = stick;
}

void NS_UnregisterContextStick(JSContext *cx)
{
  nsAutoLockChrome lock;

  ContextStickInfo **pstick = &gContextSticks;
  while (*pstick) {
    ContextStickInfo *stick = *pstick;
    if (stick->cx == cx) {
      js::RemoveStickContent(stick->cx);
      *pstick = stick->next;
      delete stick;
    } else {
      pstick = &stick->next;
    }
  }
}

static void
RemoveContextSticks()
{
  MOZ_ASSERT(NS_IsChromeOwningThread());

  PRThread *thread = PR_GetCurrentThread();

  ContextStickInfo **pstick = &gContextSticks;
  while (*pstick) {
    ContextStickInfo *stick = *pstick;
    if (JS_GetContextThread(stick->cx) == (uintptr_t) thread) {
      js::RemoveStickContent(stick->cx);
      *pstick = stick->next;
      delete stick;
    } else {
      pstick = &stick->next;
    }
  }
}

nsAutoLockChromeUnstickContent::~nsAutoLockChromeUnstickContent()
{
  MOZ_ASSERT(NS_IsChromeOwningThread());
  nsAutoLockChromeUnstickContent **punstick = &gUnstickList;
  while (*punstick != this)
    punstick = &(*punstick)->mPrev;
  *punstick = (*punstick)->mPrev;

  GET_MANAGER();

  if (!mgr)
    return;

  mgr->UnstickAllContent();
  RemoveContextSticks();

#ifdef NS_DEBUG
  PRThread *thread;
  bool chrome;
  PRUint64 contentMask;
  NS_FindThreadBitmask(&thread, &chrome, &contentMask);
  MOZ_ASSERT(chrome && contentMask == 0);
#endif
}

nsAutoUnlockEverything::nsAutoUnlockEverything()
{
  MOZ_ASSERT(NS_IsChromeOwningThread());

  GET_MANAGER();

  if (!mgr)
    return;

  bool sticky_;
  mgr->ZoneLockDepth(JS_ZONE_CHROME, &mChromeDepth, &sticky_);
  MOZ_ASSERT(!sticky_);

  mCount = JS_ZONE_CONTENT_LIMIT;

  for (int i = 0; i < mCount; i++) {
    ZoneInfo zone;
    mgr->ZoneLockDepth((JSZoneId) i, &zone.depth, &zone.sticky);
    mContent.AppendElement(zone);
    for (size_t j = 0; j < zone.depth; j++)
      mgr->UnlockZone((JSZoneId) i);
  }

  mgr->UnstickAllContent();
  RemoveContextSticks();

#ifdef NS_DEBUG
  mSuspendedUnsticks = NULL;
  nsAutoLockChromeUnstickContent **punstick = &gUnstickList;
  while (*punstick) {
    nsAutoLockChromeUnstickContent *unstick = *punstick;
    if (unstick->mThread == PR_GetCurrentThread()) {
      *punstick = unstick->mPrev;
      unstick->mPrev = mSuspendedUnsticks;
      mSuspendedUnsticks = unstick;
    } else {
      punstick = &unstick->mPrev;
    }
  }
#endif

  for (size_t i = 0; i < mChromeDepth; i++)
    mgr->UnlockZone(JS_ZONE_CHROME);
}

nsAutoUnlockEverything::~nsAutoUnlockEverything()
{
#ifdef NS_DEBUG
  if (mSuspendedUnsticks) {
    nsAutoLockChrome lock;
    nsAutoLockChromeUnstickContent *unstick = mSuspendedUnsticks;
    while (unstick->mPrev)
      unstick = unstick->mPrev;
    unstick->mPrev = gUnstickList;
    gUnstickList = mSuspendedUnsticks;
  }
#endif

  GET_MANAGER();

  if (!mgr)
    return;

  for (int i = mCount - 1; i >= 0; i--) {
    const ZoneInfo &zone = mContent[i];
    for (int j = 0; j < zone.depth; j++)
      mgr->LockZone((JSZoneId) i, false);
    if (zone.sticky)
      mgr->LockZone((JSZoneId) i, true);
  }

  for (size_t i = 0; i < mChromeDepth; i++)
    mgr->LockZone(JS_ZONE_CHROME, false);

#ifdef NS_DEBUG
#endif
}

static size_t gCantLockNewContent = 0;

void
NS_BeginCantLockNewContent()
{
  GET_MANAGER();

  if (mgr)
    mgr->BeginCantLockNewContent();

  gCantLockNewContent++;
}

void
NS_EndCantLockNewContent()
{
  GET_MANAGER();

  if (mgr)
    mgr->EndCantLockNewContent();

  MOZ_ASSERT(gCantLockNewContent);
  gCantLockNewContent--;
}

bool
NS_CanLockNewContent()
{
  return !gCantLockNewContent;
}

nsAutoUnstickChrome::nsAutoUnstickChrome(JSContext *cx)
  : mCx(cx)
{
  mStuck.chromeStuck = false;
  mStuck.prev = js::ContextFriendFields::get(cx)->chromeStickState;
  js::ContextFriendFields::get(cx)->chromeStickState = &mStuck;
}

nsAutoUnstickChrome::~nsAutoUnstickChrome()
{
  MOZ_ASSERT(js::ContextFriendFields::get(mCx)->chromeStickState == &mStuck);
  if (mStuck.chromeStuck)
    NS_UnlockZone(JS_ZONE_CHROME);
  js::ContextFriendFields::get(mCx)->chromeStickState = mStuck.prev;
}

#include <execinfo.h>
#include <stdio.h>
#include <dlfcn.h>
#include <prlock.h>

#include "js/HashTable.h"

static FILE *backtraceFile = NULL;

class MySystemAllocPolicy
{
  public:
    void *malloc_(size_t bytes) { return malloc(bytes); }
    void *realloc_(void *p, size_t oldBytes, size_t bytes) { return realloc(p, bytes); }
    void free_(void *p) { free(p); }
    void reportAllocOverflow() const {}
};

typedef js::HashMap<void*, const char*, js::DefaultHasher<void*>, MySystemAllocPolicy> BacktraceMap;
static BacktraceMap *backtraceMap = NULL;
static PRLock *backtraceLock = NULL;

static const size_t BACKTRACE_CAPACITY = 20;

void
NS_DumpBacktrace(const char *str, bool flush)
{
  static bool active = false;
  static bool checked = false;
  if (!checked) {
    const char *env = getenv("DUMP_BACKTRACES");
    active = (env && *env);
    checked = true;
  }
  if (!active)
    return;

  if (!backtraceFile) {
    backtraceFile = fopen("backtrace.txt", "w");
    backtraceMap = new BacktraceMap();
    backtraceMap->init();
    backtraceLock = PR_NewLock();
  }

  PR_Lock(backtraceLock);

  fprintf(backtraceFile, "\nBACKTRACE THREAD %p %s\n", PR_GetCurrentThread(), str);

  void *buffer[BACKTRACE_CAPACITY];
  size_t count = backtrace(&buffer[0], BACKTRACE_CAPACITY);

  for (size_t i = 2; i < count; i++) {
    void *ptr = buffer[i];
    if (!ptr)
      continue;

    BacktraceMap::AddPtr p = backtraceMap->lookupForAdd(ptr);
    if (p) {
      fprintf(backtraceFile, "%s\n", p->value);
    } else {
      const char *str;
      if (JS_IsJITCodeAddress(ptr)) {
        // dladdr can produce garbage pointers for jitcode return addresses (whatever).
        str = "<JITCODE>";
      } else {
        Dl_info info;
        dladdr(ptr, &info);
        str = info.dli_sname ? info.dli_sname : "<UNKNOWN>";
      }
      backtraceMap->add(p, ptr, str);
      fprintf(backtraceFile, "%s\n", str);
    }
  }

  if (flush)
    fflush(backtraceFile);

  PR_Unlock(backtraceLock);
}
