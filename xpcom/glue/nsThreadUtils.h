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

#ifndef nsThreadUtils_h__
#define nsThreadUtils_h__

#include "jsapi.h"
#include "prthread.h"
#include "prinrval.h"
#include "nsIThreadManager.h"
#include "nsIThread.h"
#include "nsIRunnable.h"
#include "nsStringGlue.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "mozilla/threads/nsThreadIDs.h"

// This is needed on some systems to prevent collisions between the symbols
// appearing in xpcom_core and xpcomglue.  It may be unnecessary in the future
// with better toolchain support.
#ifdef MOZILLA_INTERNAL_API
# define NS_NewThread NS_NewThread_P
# define NS_GetCurrentThread NS_GetCurrentThread_P
# define NS_GetMainThread NS_GetMainThread_P
# define NS_IsMainThread NS_IsMainThread_P
# define NS_DispatchToCurrentThread NS_DispatchToCurrentThread_P
# define NS_DispatchToMainThread NS_DispatchToMainThread_P
# define NS_ProcessPendingEvents NS_ProcessPendingEvents_P
# define NS_HasPendingEvents NS_HasPendingEvents_P
# define NS_ProcessNextEvent NS_ProcessNextEvent_P
#endif

//-----------------------------------------------------------------------------
// These methods are alternatives to the methods on nsIThreadManager, provided
// for convenience.

/**
 * Create a new thread, and optionally provide an initial event for the thread.
 *
 * @param result
 *   The resulting nsIThread object.
 * @param initialEvent
 *   The initial event to run on this thread.  This parameter may be null.
 * @param stackSize
 *   The size in bytes to reserve for the thread's stack.
 *
 * @returns NS_ERROR_INVALID_ARG
 *   Indicates that the given name is not unique.
 */
extern NS_COM_GLUE NS_METHOD
NS_NewThread(nsIThread **result,
             nsIRunnable *initialEvent = nsnull,
             PRUint32 stackSize = nsIThreadManager::DEFAULT_STACK_SIZE);

/**
 * Get a reference to the current thread.
 *
 * @param result
 *   The resulting nsIThread object.
 */
extern NS_COM_GLUE NS_METHOD
NS_GetCurrentThread(nsIThread **result);

extern JSZoneId
NS_AllocateContentZone();

extern void
NS_FreeContentZone(JSZoneId zone);

extern JSBool
NS_IsOwningThread(JSZoneId zone);

inline bool
NS_IsChromeOwningThread() { return NS_IsOwningThread(JS_ZONE_CHROME); }

#ifdef NS_DEBUG
extern void
NS_FindThreadBitmask(PRThread **pthread, bool *pchrome, PRUint64 *pcontentMask);
#endif

extern uint32_t
NS_ThreadLockDepth(JSZoneId zone);

extern void
NS_LockZone(JSZoneId zone);

extern JSBool
NS_TryLockZone(JSZoneId zone);

extern void
NS_UnlockZone(JSZoneId zone);

extern void
NS_StickContentLock(JSZoneId zone);

extern bool
NS_TryStickContentLock(JSZoneId zone);

template <typename T>
inline void
NS_StickLock(const T &ptr)
{
  JSZoneId zone = ptr->GetZone();
  if (zone >= JS_ZONE_CONTENT_START)
    NS_StickContentLock(zone);
}

template <typename T>
inline bool
NS_TryStickLock(const T &ptr)
{
  JSZoneId zone = ptr->GetZone();
  if (zone >= JS_ZONE_CONTENT_START)
    return NS_TryStickContentLock(zone);
  return true;
}

struct nsAutoLockZone
{
  JSZoneId zone;
  nsAutoLockZone(JSZoneId zone);
  ~nsAutoLockZone();
};

struct nsAutoUnlockZone
{
  JSZoneId zone;
  PRUint32 depth;
  nsAutoUnlockZone(JSZoneId zone);
  ~nsAutoUnlockZone();
};

struct nsAutoTryLockZone
{
  JSZoneId zone;
  bool succeeded;
  nsAutoTryLockZone(JSZoneId zone);
  ~nsAutoTryLockZone();
};

struct nsAutoLockChrome : public nsAutoLockZone
{
  nsAutoLockChrome() : nsAutoLockZone(JS_ZONE_CHROME) {}
};

struct nsAutoUnlockChrome : public nsAutoUnlockZone
{
  nsAutoUnlockChrome() : nsAutoUnlockZone(JS_ZONE_CHROME) {}
};

struct nsAutoTryLockEverything
{
  nsAutoTryLockEverything();
  ~nsAutoTryLockEverything();

  PRInt32 mCount;
  nsTArray<bool> mLockedArray;
};

JSBool NS_LockEverything();
JSBool NS_IsEverythingLocked();
void NS_UnlockEverything();

uintptr_t NS_FindNativeStackTopForThread(/*(PRThread*)*/ uintptr_t thread);

struct nsAutoLockChromeUnstickContent : public nsAutoLockZone
{
  nsAutoLockChromeUnstickContent();
  ~nsAutoLockChromeUnstickContent();

  nsAutoLockChromeUnstickContent *mPrev;
  PRThread *mThread;
};

void NS_RegisterContextStick(JSContext *cx);
void NS_UnregisterContextStick(JSContext *cx);

struct nsAutoUnlockEverything
{
  nsAutoUnlockEverything();
  ~nsAutoUnlockEverything();

  struct ZoneInfo {
    bool sticky;
    PRUint32 depth;
  };

  PRUint32 mChromeDepth;

  PRInt32 mCount;
  nsTArray<ZoneInfo> mContent;

#ifdef NS_DEBUG
  nsAutoLockChromeUnstickContent *mSuspendedUnsticks;
#endif
};

void NS_BeginCantLockNewContent();
void NS_EndCantLockNewContent();

bool NS_CanLockNewContent();

struct nsAutoCantLockNewContent
{
  nsAutoCantLockNewContent() { NS_BeginCantLockNewContent(); }
  ~nsAutoCantLockNewContent() { NS_EndCantLockNewContent(); }
};

class nsAutoUnstickChrome
{
  JSContext *mCx;
  JS::StickState mStuck;

public:
  nsAutoUnstickChrome(JSContext *cx);
  ~nsAutoUnstickChrome();
};

/**
 * Get a reference to the main thread.
 *
 * @param result
 *   The resulting nsIThread object.
 */
extern NS_COM_GLUE NS_METHOD
NS_GetMainThread(nsIThread **result);

#if defined(MOZILLA_INTERNAL_API) && defined(XP_WIN)
bool NS_IsMainThread();
#elif defined(MOZILLA_INTERNAL_API) && defined(NS_TLS)
// This is defined in nsThreadManager.cpp and initialized to `Main` for the
// main thread by nsThreadManager::Init.
extern NS_TLS mozilla::threads::ID gTLSThreadID;
inline bool NS_IsMainThread()
{
  return gTLSThreadID == mozilla::threads::Main;
}
#else
/**
 * Test to see if the current thread is the main thread.
 *
 * @returns true if the current thread is the main thread, and false
 * otherwise.
 */
extern NS_COM_GLUE bool NS_IsMainThread();
#endif

// Get the current thread's associated zone, or JS_ZONE_NONE.
extern JSZoneId NS_FindExecuteThreadZone();

inline bool
NS_IsExecuteThread()
{
  return NS_IsMainThread() || NS_FindExecuteThreadZone() != JS_ZONE_NONE;
}

extern bool
NS_CanBlockOnContent();

extern NS_COM_GLUE NS_METHOD
NS_GetExecuteThread(JSZoneId zone, nsIThread **result);

extern NS_COM_GLUE void
NS_DumpBacktrace(const char *str, bool flush = false);

/**
 * Dispatch the given event to the current thread.
 *
 * @param event
 *   The event to dispatch.
 *
 * @returns NS_ERROR_INVALID_ARG
 *   If event is null.
 */
extern NS_COM_GLUE NS_METHOD
NS_DispatchToCurrentThread(nsIRunnable *event);

/**
 * Dispatch the given event to a gecko execution thread.
 *
 * @param event
 *   The event to dispatch.
 * @param dispatchFlags
 *   The flags to pass to the thread's dispatch method.
 *
 * @returns NS_ERROR_INVALID_ARG
 *   If event is null.
 */
extern NS_COM_GLUE NS_METHOD
NS_DispatchToMainThread(nsIRunnable *event,
                        PRUint32 dispatchFlags = NS_DISPATCH_NORMAL,
                        JSZoneId zone = JS_ZONE_CHROME);

#ifndef XPCOM_GLUE_AVOID_NSPR
/**
 * Process all pending events for the given thread before returning.  This
 * method simply calls ProcessNextEvent on the thread while HasPendingEvents
 * continues to return true and the time spent in NS_ProcessPendingEvents
 * does not exceed the given timeout value.
 *
 * @param thread
 *   The thread object for which to process pending events.  If null, then
 *   events will be processed for the current thread.
 * @param timeout
 *   The maximum number of milliseconds to spend processing pending events.
 *   Events are not pre-empted to honor this timeout.  Rather, the timeout
 *   value is simply used to determine whether or not to process another event.
 *   Pass PR_INTERVAL_NO_TIMEOUT to specify no timeout.
 */
extern NS_COM_GLUE NS_METHOD
NS_ProcessPendingEvents(nsIThread *thread,
                        PRIntervalTime timeout = PR_INTERVAL_NO_TIMEOUT);
#endif

/**
 * Shortcut for nsIThread::HasPendingEvents.
 *
 * It is an error to call this function when the given thread is not the
 * current thread.  This function will return false if called from some
 * other thread.
 *
 * @param thread
 *   The current thread or null.
 *
 * @returns
 *   A boolean value that if "true" indicates that there are pending events
 *   in the current thread's event queue.
 */
extern NS_COM_GLUE bool
NS_HasPendingEvents(nsIThread *thread = nsnull);

/**
 * Shortcut for nsIThread::ProcessNextEvent.
 *   
 * It is an error to call this function when the given thread is not the
 * current thread.  This function will simply return false if called
 * from some other thread.
 *
 * @param thread
 *   The current thread or null.
 * @param mayWait
 *   A boolean parameter that if "true" indicates that the method may block
 *   the calling thread to wait for a pending event.
 *
 * @returns
 *   A boolean value that if "true" indicates that an event from the current
 *   thread's event queue was processed.
 */
extern NS_COM_GLUE bool
NS_ProcessNextEvent(nsIThread *thread = nsnull, bool mayWait = true);

//-----------------------------------------------------------------------------
// Helpers that work with nsCOMPtr:

inline already_AddRefed<nsIThread>
do_GetCurrentThread() {
  nsIThread *thread = nsnull;
  NS_GetCurrentThread(&thread);
  return already_AddRefed<nsIThread>(thread);
}

inline already_AddRefed<nsIThread>
do_GetMainThread() {
  nsIThread *thread = nsnull;
  NS_GetMainThread(&thread);
  return already_AddRefed<nsIThread>(thread);
}

//-----------------------------------------------------------------------------

#ifdef MOZILLA_INTERNAL_API
// Fast access to the current thread.  Do not release the returned pointer!  If
// you want to use this pointer from some other thread, then you will need to
// AddRef it.  Otherwise, you should only consider this pointer valid from code
// running on the current thread.
extern NS_COM_GLUE nsIThread *NS_GetCurrentThread();
#endif

//-----------------------------------------------------------------------------

#ifndef XPCOM_GLUE_AVOID_NSPR

#undef  IMETHOD_VISIBILITY
#define IMETHOD_VISIBILITY NS_COM_GLUE

// This class is designed to be subclassed.
class NS_COM_GLUE nsRunnable : public nsIRunnable
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIRUNNABLE

  nsRunnable() {
  }

protected:
  virtual ~nsRunnable() {
  }
};

#undef  IMETHOD_VISIBILITY
#define IMETHOD_VISIBILITY NS_VISIBILITY_HIDDEN

// An event that can be used to call a method on a class.  The class type must
// support reference counting. This event supports Revoke for use
// with nsRevocableEventPtr.
template <class ClassType,
          typename ReturnType = void,
          bool Owning = true>
class nsRunnableMethod : public nsRunnable
{
public:
  virtual void Revoke() = 0;

  // These ReturnTypeEnforcer classes set up a blacklist for return types that
  // we know are not safe. The default ReturnTypeEnforcer compiles just fine but
  // already_AddRefed will not.
  template <typename OtherReturnType>
  class ReturnTypeEnforcer
  {
  public:
    typedef int ReturnTypeIsSafe;
  };

  template <class T>
  class ReturnTypeEnforcer<already_AddRefed<T> >
  {
    // No ReturnTypeIsSafe makes this illegal!
  };

  // Make sure this return type is safe.
  typedef typename ReturnTypeEnforcer<ReturnType>::ReturnTypeIsSafe check;
};

extern NS_COM_GLUE nsresult
NS_ReleaseReferenceRaw(/* nsISupports */ void *doomed);

template <class ClassType, bool Owning>
struct nsRunnableMethodReceiver {
  ClassType *mObj;
  JSZoneId mZone;
  nsRunnableMethodReceiver(ClassType *obj, JSZoneId zone)
    : mObj(obj), mZone(zone)
  {
    NS_IF_ADDREF(mObj);
  }
 ~nsRunnableMethodReceiver() { Revoke(); }
  void Revoke() {
    MOZ_ASSERT_IF(NS_IsExecuteThread(), NS_IsChromeOwningThread());
    if (mObj) {
      if (mZone >= JS_ZONE_CONTENT_START)
        NS_ReleaseReferenceRaw(mObj);
      else
        NS_RELEASE(mObj);
    }
    mObj = nsnull;
  }
};

template <class ClassType>
struct nsRunnableMethodReceiver<ClassType, false> {
  ClassType *mObj;
  JSZoneId mZone;
  nsRunnableMethodReceiver(ClassType *obj, JSZoneId zone) : mObj(obj), mZone(zone) {}
  void Revoke() {
    MOZ_ASSERT_IF(NS_IsExecuteThread(), NS_IsChromeOwningThread());
    mObj = nsnull;
  }
};

template <typename Method, bool Owning> struct nsRunnableMethodTraits;

template <class C, typename R, bool Owning>
struct nsRunnableMethodTraits<R (C::*)(), Owning> {
  typedef C class_type;
  typedef R return_type;
  typedef nsRunnableMethod<C, R, Owning> base_type;
};

#ifdef HAVE_STDCALL
template <class C, typename R, bool Owning>
struct nsRunnableMethodTraits<R (__stdcall C::*)(), Owning> {
  typedef C class_type;
  typedef R return_type;
  typedef nsRunnableMethod<C, R, Owning> base_type;
};
#endif

template <typename Method, bool Owning>
class nsRunnableMethodImpl
  : public nsRunnableMethodTraits<Method, Owning>::base_type
{
  typedef typename nsRunnableMethodTraits<Method, Owning>::class_type ClassType;
  nsRunnableMethodReceiver<ClassType, Owning> mReceiver;
  Method mMethod;

public:
  nsRunnableMethodImpl(ClassType *obj,
                       Method method,
                       JSZoneId zone)
    : mReceiver(obj, zone)
    , mMethod(method)
  {}

  NS_IMETHOD Run() {
    if (mReceiver.mZone >= JS_ZONE_CONTENT_START)
      NS_StickContentLock(mReceiver.mZone);
    if (NS_LIKELY(mReceiver.mObj))
      ((*mReceiver.mObj).*mMethod)();
    return NS_OK;
  }

  void Revoke() {
    mReceiver.Revoke();
  }
};

// Use this template function like so:
//
//   nsCOMPtr<nsIRunnable> event =
//     NS_NewRunnableMethod(myObject, &MyClass::HandleEvent);
//   NS_DispatchToCurrentThread(event);
//
// Statically enforced constraints:
//  - myObject must be of (or implicitly convertible to) type MyClass
//  - MyClass must defined AddRef and Release methods
//
template<typename PtrType, typename Method>
typename nsRunnableMethodTraits<Method, true>::base_type*
NS_NewRunnableMethod(PtrType ptr, Method method, JSZoneId zone = JS_ZONE_NONE)
{
  return new nsRunnableMethodImpl<Method, true>(ptr, method, zone);
}

template<typename PtrType, typename Method>
typename nsRunnableMethodTraits<Method, false>::base_type*
NS_NewNonOwningRunnableMethod(PtrType ptr, Method method, JSZoneId zone = JS_ZONE_NONE)
{
  return new nsRunnableMethodImpl<Method, false>(ptr, method, zone);
}

#endif  // XPCOM_GLUE_AVOID_NSPR

// This class is designed to be used when you have an event class E that has a
// pointer back to resource class R.  If R goes away while E is still pending,
// then it is important to "revoke" E so that it does not try use R after R has
// been destroyed.  nsRevocableEventPtr makes it easy for R to manage such
// situations:
//
//   class R;
//
//   class E : public nsRunnable {
//   public:
//     void Revoke() {
//       mResource = nsnull;
//     }
//   private:
//     R *mResource;
//   };
//
//   class R {
//   public:
//     void EventHandled() {
//       mEvent.Forget();
//     }
//   private:
//     nsRevocableEventPtr<E> mEvent;
//   };
//
//   void R::PostEvent() {
//     // Make sure any pending event is revoked.
//     mEvent->Revoke();
//
//     nsCOMPtr<nsIRunnable> event = new E();
//     if (NS_SUCCEEDED(NS_DispatchToCurrentThread(event))) {
//       // Keep pointer to event so we can revoke it.
//       mEvent = event;
//     }
//   }
//
//   NS_IMETHODIMP E::Run() {
//     if (!mResource)
//       return NS_OK;
//     ...
//     mResource->EventHandled();
//     return NS_OK;
//   }
//
template <class T>
class nsRevocableEventPtr {
public:
  nsRevocableEventPtr()
    : mEvent(nsnull) {
  }

  ~nsRevocableEventPtr() {
    Revoke();
  }

  const nsRevocableEventPtr& operator=(T *event) {
    if (mEvent != event) {
      Revoke();
      mEvent = event;
    }
    return *this;
  }

  void Revoke() {
    if (mEvent) {
      mEvent->Revoke();
      mEvent = nsnull;
    }
  }

  void Forget() {
    mEvent = nsnull;
  }

  bool IsPending() {
    return mEvent != nsnull;
  }
  
  T *get() { return mEvent; }

private:
  // Not implemented
  nsRevocableEventPtr(const nsRevocableEventPtr&);
  nsRevocableEventPtr& operator=(const nsRevocableEventPtr&);

  nsRefPtr<T> mEvent;
};

#endif  // nsThreadUtils_h__
