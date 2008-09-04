/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40 -*- */
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
 * The Original Code is worker threads.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com> (Original Author)
 *   Ben Turner <bent.mozilla@gmail.com>
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

#include "nsDOMWorkerThread.h"

// Interfaces
#include "nsIDOMClassInfo.h"
#include "nsIJSContextStack.h"
#include "nsIJSRuntimeService.h"
#include "nsIScriptContext.h"
#include "nsIXPConnect.h"

// Other includes
#ifdef MOZ_SHARK
#include "jsdbgapi.h"
#endif
#include "nsAutoLock.h"
#include "nsContentUtils.h"
#include "nsJSUtils.h"
#include "nsJSEnvironment.h"

// DOMWorker includes
#include "nsDOMWorkerPool.h"
#include "nsDOMThreadService.h"
#include "nsDOMWorkerTimeout.h"

#define LOG(_args) PR_LOG(gDOMThreadsLog, PR_LOG_DEBUG, _args)

// XXX Could make these functions of nsDOMWorkerThread instead.
class nsDOMWorkerFunctions
{
public:
  // Same as window.dump().
  static JSBool Dump(JSContext* aCx, JSObject* aObj, uintN aArgc, jsval* aArgv,
                     jsval* aRval);

  // Debug-only version of window.dump(), like the JS component loader has.
  static JSBool DebugDump(JSContext* aCx, JSObject* aObj, uintN aArgc,
                          jsval* aArgv, jsval* aRval);

  // Same as nsIDOMWorkerThread::PostMessage
  static JSBool PostMessage(JSContext* aCx, JSObject* aObj, uintN aArgc,
                            jsval* aArgv, jsval* aRval);

  // Same as window.setTimeout().
  static JSBool SetTimeout(JSContext* aCx, JSObject* aObj, uintN aArgc,
                           jsval* aArgv, jsval* aRval) {
    return MakeTimeout(aCx, aObj, aArgc, aArgv, aRval, PR_FALSE);
  }

  // Same as window.setInterval().
  static JSBool SetInterval(JSContext* aCx, JSObject* aObj, uintN aArgc,
                            jsval* aArgv, jsval* aRval) {
    return MakeTimeout(aCx, aObj, aArgc, aArgv, aRval, PR_TRUE);
  }

  // Used for both clearTimeout() and clearInterval().
  static JSBool KillTimeout(JSContext* aCx, JSObject* aObj, uintN aArgc,
                            jsval* aArgv, jsval* aRval);

private:
  // Internal helper for SetTimeout and SetInterval.
  static JSBool MakeTimeout(JSContext* aCx, JSObject* aObj, uintN aArgc,
                            jsval* aArgv, jsval* aRval, PRBool aIsInterval);
};

JSBool JS_DLL_CALLBACK
nsDOMWorkerFunctions::Dump(JSContext* aCx,
                           JSObject* /* aObj */,
                           uintN aArgc,
                           jsval* aArgv,
                           jsval* /* aRval */)
{
  // XXX Expose this to the JS console? Only if that DOM pref is set?

  JSString* str;
  if (aArgc && (str = JS_ValueToString(aCx, aArgv[0])) && str) {
    nsDependentJSString string(str);
    fputs(NS_ConvertUTF16toUTF8(nsDependentJSString(str)).get(), stderr);
    fflush(stderr);
  }
  return JS_TRUE;
}

JSBool JS_DLL_CALLBACK
nsDOMWorkerFunctions::DebugDump(JSContext* aCx,
                                JSObject* aObj,
                                uintN aArgc,
                                jsval* aArgv,
                                jsval* aRval)
{
#ifdef DEBUG
  return nsDOMWorkerFunctions::Dump(aCx, aObj, aArgc, aArgv, aRval);
#else
  return JS_TRUE;
#endif
}

JSBool JS_DLL_CALLBACK
nsDOMWorkerFunctions::PostMessage(JSContext* aCx,
                                  JSObject* /* aObj */,
                                  uintN aArgc,
                                  jsval* aArgv,
                                  jsval* /* aRval */)
{
  nsDOMWorkerThread* worker =
    static_cast<nsDOMWorkerThread*>(JS_GetContextPrivate(aCx));
  NS_ASSERTION(worker, "This should be set by the DOM thread service!");

  if (worker->IsCanceled()) {
    return JS_FALSE;
  }

  nsRefPtr<nsDOMWorkerPool> pool = worker->Pool();
  NS_ASSERTION(pool, "Shouldn't ever be null!");

  nsresult rv;

  JSString* str;
  if (aArgc && (str = JS_ValueToString(aCx, aArgv[0])) && str) {
    rv = pool->PostMessageInternal(nsDependentJSString(str), worker);
  }
  else {
    rv = pool->PostMessageInternal(EmptyString(), worker);
  }
  NS_ENSURE_SUCCESS(rv, JS_FALSE);

  return JS_TRUE;
}

JSBool JS_DLL_CALLBACK
nsDOMWorkerFunctions::MakeTimeout(JSContext* aCx,
                                  JSObject* /* aObj */,
                                  uintN aArgc,
                                  jsval* aArgv,
                                  jsval* aRval,
                                  PRBool aIsInterval)
{
  nsDOMWorkerThread* worker =
    static_cast<nsDOMWorkerThread*>(JS_GetContextPrivate(aCx));
  NS_ASSERTION(worker, "This should be set by the DOM thread service!");

  if (worker->IsCanceled()) {
    return JS_FALSE;
  }

  PRUint32 id = ++worker->mNextTimeoutId;

  nsAutoPtr<nsDOMWorkerTimeout>
    timeout(new nsDOMWorkerTimeout(worker, id));
  NS_ENSURE_TRUE(timeout, JS_FALSE);

  nsresult rv = timeout->Init(aCx, aArgc, aArgv, aIsInterval);
  NS_ENSURE_SUCCESS(rv, JS_FALSE);

  timeout.forget();

  *aRval = INT_TO_JSVAL(id);
  return JS_TRUE;
}

JSBool JS_DLL_CALLBACK
nsDOMWorkerFunctions::KillTimeout(JSContext* aCx,
                                  JSObject* /* aObj */,
                                  uintN aArgc,
                                  jsval* aArgv,
                                  jsval* /* aRval */)
{
  nsDOMWorkerThread* worker =
    static_cast<nsDOMWorkerThread*>(JS_GetContextPrivate(aCx));
  NS_ASSERTION(worker, "This should be set by the DOM thread service!");

  // A canceled worker should have already killed all timeouts.
  if (worker->IsCanceled()) {
    return JS_TRUE;
  }

  if (!aArgc) {
    JS_ReportError(aCx, "Function requires at least 1 parameter");
    return JS_FALSE;
  }

  uint32 id;
  if (!JS_ValueToECMAUint32(aCx, aArgv[0], &id)) {
    JS_ReportError(aCx, "First argument must be a timeout id");
    return JS_FALSE;
  }

  worker->CancelTimeout(PRUint32(id));
  return JS_TRUE;
}

JSFunctionSpec gDOMWorkerFunctions[] = {
  { "dump",                  nsDOMWorkerFunctions::Dump,              1, 0, 0 },
  { "debug",                 nsDOMWorkerFunctions::DebugDump,         1, 0, 0 },
  { "postMessageToPool",     nsDOMWorkerFunctions::PostMessage,       1, 0, 0 },
  { "setTimeout",            nsDOMWorkerFunctions::SetTimeout,        1, 0, 0 },
  { "clearTimeout",          nsDOMWorkerFunctions::KillTimeout,       1, 0, 0 },
  { "setInterval",           nsDOMWorkerFunctions::SetInterval,       1, 0, 0 },
  { "clearInterval",         nsDOMWorkerFunctions::KillTimeout,       1, 0, 0 },
#ifdef MOZ_SHARK
  { "startShark",            js_StartShark,                           0, 0, 0 },
  { "stopShark",             js_StopShark,                            0, 0, 0 },
  { "connectShark",          js_ConnectShark,                         0, 0, 0 },
  { "disconnectShark",       js_DisconnectShark,                      0, 0, 0 },
#endif
  { nsnull,                  nsnull,                                  0, 0, 0 }
};

/**
 * An nsISupports that holds a weak ref to the worker. The worker owns the
 * thread context so we don't have to worry about nulling this out ever.
 */
class nsDOMWorkerThreadWeakRef : public nsIDOMWorkerThread,
                                 public nsIClassInfo
{
public:
  NS_DECL_ISUPPORTS
  NS_FORWARD_NSIDOMWORKERTHREAD(mWorker->)
  NS_FORWARD_NSICLASSINFO(mWorker->)

  nsDOMWorkerThreadWeakRef(nsDOMWorkerThread* aWorker)
  : mWorker(aWorker) { }

protected:
  nsDOMWorkerThread* mWorker;
};

NS_IMPL_THREADSAFE_ISUPPORTS2(nsDOMWorkerThreadWeakRef, nsIDOMWorkerThread,
                                                        nsIClassInfo)

/**
 * The 'threadContext' object for a worker's JS global object.
 */
class nsDOMWorkerThreadContext : public nsIDOMWorkerThreadContext,
                                 public nsIClassInfo
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMWORKERTHREADCONTEXT
  NS_DECL_NSICLASSINFO

  nsDOMWorkerThreadContext(nsDOMWorkerThread* aWorker)
  : mWorker(aWorker) { }

protected:
  nsDOMWorkerThread* mWorker;
  nsCOMPtr<nsIDOMWorkerThread> mWeakRef;
};

NS_IMPL_THREADSAFE_ISUPPORTS2(nsDOMWorkerThreadContext,
                              nsIDOMWorkerThreadContext,
                              nsIClassInfo)

NS_IMPL_CI_INTERFACE_GETTER1(nsDOMWorkerThreadContext,
                             nsIDOMWorkerThreadContext)

NS_IMPL_THREADSAFE_DOM_CI(nsDOMWorkerThreadContext)

NS_IMETHODIMP
nsDOMWorkerThreadContext::GetThisThread(nsIDOMWorkerThread** aThisThread)
{
  if (!mWeakRef) {
    mWeakRef = new nsDOMWorkerThreadWeakRef(mWorker);
    NS_ENSURE_TRUE(mWeakRef, NS_ERROR_OUT_OF_MEMORY);
  }

  NS_ADDREF(*aThisThread = mWeakRef);
  return NS_OK;
}

nsDOMWorkerThread::nsDOMWorkerThread(nsDOMWorkerPool* aPool,
                                     const nsAString& aSource)
: mPool(aPool),
  mSource(aSource),
  mGlobal(nsnull),
  mCompiled(PR_FALSE),
  mCallbackCount(0),
  mNextTimeoutId(0),
  mLock(nsnull)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!aSource.IsEmpty(), "Empty source string!");

  PR_INIT_CLIST(&mTimeouts);
}

nsDOMWorkerThread::~nsDOMWorkerThread()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!IsCanceled()) {
    nsRefPtr<nsDOMWorkerPool> pool = Pool();
    pool->NoteDyingWorker(this);
  }

  ClearTimeouts();

  // Only clean up if we created a global object
  if (mGlobal) {
    JSRuntime* rt;
    if (NS_SUCCEEDED(nsDOMThreadService::JSRuntimeService()->GetRuntime(&rt))) {
      JS_RemoveRootRT(rt, &mGlobal);
    }
    else {
      NS_ERROR("This shouldn't fail!");
    }
  }

  if (mLock) {
    nsAutoLock::DestroyLock(mLock);
  }
}

NS_IMPL_THREADSAFE_ISUPPORTS2(nsDOMWorkerThread, nsIDOMWorkerThread,
                                                 nsIClassInfo)
NS_IMPL_CI_INTERFACE_GETTER1(nsDOMWorkerThread, nsIDOMWorkerThread)

NS_IMPL_THREADSAFE_DOM_CI(nsDOMWorkerThread)

nsresult
nsDOMWorkerThread::Init()
{
  mLock = nsAutoLock::NewLock("nsDOMWorkerThread::mLock");
  NS_ENSURE_TRUE(mLock, NS_ERROR_OUT_OF_MEMORY);

  NS_ASSERTION(!mGlobal, "Already got a global?!");

  // This is pretty cool - all we have to do to get our script executed is to
  // pass a no-op runnable to the thread service and it will make sure we have
  // a context and global object.
  nsCOMPtr<nsIRunnable> runnable(new nsRunnable());
  NS_ENSURE_TRUE(runnable, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv = nsDOMThreadService::get()->Dispatch(this, runnable);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

// From nsDOMWorkerBase
nsresult
nsDOMWorkerThread::HandleMessage(const nsAString& aMessage,
                                 nsDOMWorkerBase* aSource)
{
  nsCOMPtr<nsIDOMWorkerMessageListener> messageListener = GetMessageListener();
  if (!messageListener) {
    LOG(("Message received on a worker with no listener!"));
    return NS_OK;
  }

  // We have to call this manually because XPConnect will replace our error
  // reporter with its own and we won't properly notify the pool of any
  // unhandled exceptions...

  JSContext* cx;
  nsresult rv =
    nsDOMThreadService::ThreadJSContextStack()->GetSafeJSContext(&cx);
  NS_ENSURE_SUCCESS(rv, rv);

  JSAutoRequest ar(cx);

  if (JS_IsExceptionPending(cx)) {
    JS_ClearPendingException(cx);
  }

  // Get a JS string for the message.
  JSString* message = JS_NewUCStringCopyN(cx, (jschar*)aMessage.BeginReading(),
                                          aMessage.Length());
  NS_ENSURE_TRUE(message, NS_ERROR_FAILURE);

  // Root it
  jsval messageVal = STRING_TO_JSVAL(message);
  nsAutoGCRoot rootedMessage(&messageVal, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsIXPConnect* xpc = nsContentUtils::XPConnect();

  nsCOMPtr<nsISupports> source;
  aSource->QueryInterface(NS_GET_IID(nsISupports), getter_AddRefs(source));
  NS_ASSERTION(source, "Impossible!");

  // Wrap the source thread.
  nsCOMPtr<nsIXPConnectJSObjectHolder> wrappedThread;
  rv = xpc->WrapNative(cx, mGlobal, source, NS_GET_IID(nsISupports),
                       getter_AddRefs(wrappedThread));
  NS_ENSURE_SUCCESS(rv, rv);

  JSObject* sourceThread;
  rv = wrappedThread->GetJSObject(&sourceThread);
  NS_ENSURE_SUCCESS(rv, rv);

  // Set up our arguments.
  jsval argv[2] = {
    STRING_TO_JSVAL(message),
    OBJECT_TO_JSVAL(sourceThread)
  };

  // Get the listener object out of our wrapped listener.
  nsCOMPtr<nsIXPConnectJSObjectHolder> wrappedListener =
    do_QueryInterface(messageListener);
  NS_ENSURE_TRUE(wrappedListener, NS_ERROR_NO_INTERFACE);

  JSObject* listener;
  rv = wrappedListener->GetJSObject(&listener);
  NS_ENSURE_SUCCESS(rv, rv);

  // And call it.
  jsval rval;
  PRBool success = JS_CallFunctionValue(cx, mGlobal, OBJECT_TO_JSVAL(listener),
                                        2, argv, &rval);
  if (!success) {
    // Make sure any pending exceptions are converted to errors for the pool.
    JS_ReportPendingException(cx);
  }

  // We shouldn't leave any pending exceptions - our error reporter should
  // clear any exception it reports.
  NS_ASSERTION(!JS_IsExceptionPending(cx), "Huh?!");

  return NS_OK;
}

// From nsDOMWorkerBase
nsresult
nsDOMWorkerThread::DispatchMessage(nsIRunnable* aRunnable)
{
  nsresult rv = nsDOMThreadService::get()->Dispatch(this, aRunnable);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

void
nsDOMWorkerThread::Cancel()
{
  nsDOMWorkerBase::Cancel();

  // If we're suspended there's a good chance that we're already paused waiting
  // on the pool's monitor. Waiting on the thread service's lock will deadlock.
  if (!IsSuspended()) {
    nsDOMThreadService::get()->WaitForCanceledWorker(this);
  }

  ClearTimeouts();
}

void
nsDOMWorkerThread::Suspend()
{
  nsDOMWorkerBase::Suspend();
  SuspendTimeouts();
}

void
nsDOMWorkerThread::Resume()
{
  nsDOMWorkerBase::Resume();
  ResumeTimeouts();
}

PRBool
nsDOMWorkerThread::SetGlobalForContext(JSContext* aCx)
{
  PRBool success = CompileGlobalObject(aCx);
  NS_ENSURE_TRUE(success, PR_FALSE);

  JS_SetGlobalObject(aCx, mGlobal);
  return PR_TRUE;
}

PRBool
nsDOMWorkerThread::CompileGlobalObject(JSContext* aCx)
{
  if (mGlobal) {
    return PR_TRUE;
  }

  if (mCompiled) {
    // Don't try to recompile a bad script.
    return PR_FALSE;
  }

  mCompiled = PR_TRUE;

  JSAutoRequest ar(aCx);

  JSObject* global = JS_NewObject(aCx, nsnull, nsnull, nsnull);
  NS_ENSURE_TRUE(global, PR_FALSE);

  NS_ASSERTION(!JS_GetGlobalObject(aCx), "Global object should be unset!");

  // This call will root global.
  PRBool success = JS_InitStandardClasses(aCx, global);
  NS_ENSURE_TRUE(success, PR_FALSE);

  // Set up worker thread functions
  success = JS_DefineFunctions(aCx, global, gDOMWorkerFunctions);
  NS_ENSURE_TRUE(success, PR_FALSE);

  nsRefPtr<nsDOMWorkerThreadContext>
    context(new nsDOMWorkerThreadContext(this));
  NS_ENSURE_TRUE(context, NS_ERROR_OUT_OF_MEMORY);

  nsIXPConnect* xpc = nsContentUtils::XPConnect();
  nsresult rv = xpc->InitClasses(aCx, global);
  NS_ENSURE_SUCCESS(rv, PR_FALSE);

  // XXX Fix this!
  success = JS_DeleteProperty(aCx, global, "Components");
  NS_ENSURE_TRUE(success, PR_FALSE);

  nsCOMPtr<nsIXPConnectJSObjectHolder> contextWrapper;
  rv = xpc->WrapNative(aCx, global,
                       NS_ISUPPORTS_CAST(nsIDOMWorkerThreadContext*, context),
                       NS_GET_IID(nsIDOMWorkerThreadContext),
                       getter_AddRefs(contextWrapper));
  NS_ENSURE_SUCCESS(rv, PR_FALSE);

  JSObject* contextObj;
  rv = contextWrapper->GetJSObject(&contextObj);
  NS_ENSURE_SUCCESS(rv, PR_FALSE);

  // Set up a name for our worker object
  success = JS_DefineProperty(aCx, global, "threadContext",
                              OBJECT_TO_JSVAL(contextObj), nsnull, nsnull,
                              JSPROP_ENUMERATE);
  NS_ENSURE_TRUE(success, PR_FALSE);

  JSScript* script = JS_CompileUCScript(aCx, global,
                                        reinterpret_cast<const jschar*>
                                            (mSource.BeginReading()),
                                        mSource.Length(), nsnull, 1);
  NS_ENSURE_TRUE(script, PR_FALSE);

  JSRuntime* rt;
  rv = nsDOMThreadService::JSRuntimeService()->GetRuntime(&rt);
  NS_ENSURE_SUCCESS(rv, PR_FALSE);

  mGlobal = global;
  success = JS_AddNamedRootRT(rt, &mGlobal, "nsDOMWorkerThread Global Object");
  if (!success) {
    NS_WARNING("Failed to root global object for worker thread!");
    mGlobal = nsnull;
    return PR_FALSE;
  }

  // Execute the script
  jsval val;
  success = JS_ExecuteScript(aCx, global, script, &val);
  if (!success) {
    NS_WARNING("Failed to evaluate script for worker thread!");
    JS_RemoveRootRT(rt, &mGlobal);
    mGlobal = nsnull;
    return PR_FALSE;
  }

  // See if the message listener function was defined.
  nsCOMPtr<nsIDOMWorkerMessageListener> listener;
  if (JS_LookupProperty(aCx, global, "messageListener", &val) &&
      JSVAL_IS_OBJECT(val) &&
      NS_SUCCEEDED(xpc->WrapJS(aCx, JSVAL_TO_OBJECT(val),
                               NS_GET_IID(nsIDOMWorkerMessageListener),
                               getter_AddRefs(listener)))) {
    SetMessageListener(listener);
  }

  return PR_TRUE;
}

nsDOMWorkerTimeout*
nsDOMWorkerThread::FirstTimeout()
{
  // Only called within the lock!
  PRCList* first = PR_LIST_HEAD(&mTimeouts);
  return first == &mTimeouts ?
                  nsnull :
                  static_cast<nsDOMWorkerTimeout*>(first);
}

nsDOMWorkerTimeout*
nsDOMWorkerThread::NextTimeout(nsDOMWorkerTimeout* aTimeout)
{
  // Only called within the lock!
  nsDOMWorkerTimeout* next =
    static_cast<nsDOMWorkerTimeout*>(PR_NEXT_LINK(aTimeout));
  return next == &mTimeouts ? nsnull : next;
}

void
nsDOMWorkerThread::AddTimeout(nsDOMWorkerTimeout* aTimeout)
{
  // This should only ever be called on the worker thread... but there's no way
  // to really assert that since we're using a thread pool.
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aTimeout, "Null pointer!");

  PRIntervalTime newInterval = aTimeout->GetInterval();

  if (IsSuspended()) {
    aTimeout->Suspend(PR_Now());
  }

  nsAutoLock lock(mLock);

  // XXX Currently stored in the order that they should execute (like the window
  //     timeouts are) but we don't flush all expired timeouts the same way that
  //     the window does... Either we should or this is unnecessary.
  for (nsDOMWorkerTimeout* timeout = FirstTimeout();
       timeout;
       timeout = NextTimeout(timeout)) {
    if (timeout->GetInterval() > newInterval) {
      PR_INSERT_BEFORE(aTimeout, timeout);
      return;
    }
  }

  PR_APPEND_LINK(aTimeout, &mTimeouts);
}

void
nsDOMWorkerThread::RemoveTimeout(nsDOMWorkerTimeout* aTimeout)
{
  nsAutoLock lock(mLock);

  PR_REMOVE_LINK(aTimeout);
}

void
nsDOMWorkerThread::ClearTimeouts()
{
  nsAutoTArray<nsRefPtr<nsDOMWorkerTimeout>, 20> timeouts;
  {
    nsAutoLock lock(mLock);
    for (nsDOMWorkerTimeout* timeout = FirstTimeout();
         timeout;
         timeout = NextTimeout(timeout)) {
      timeouts.AppendElement(timeout);
    }
  }

  PRUint32 count = timeouts.Length();
  for (PRUint32 i = 0; i < count; i++) {
    timeouts[i]->Cancel();
  }
}

void
nsDOMWorkerThread::CancelTimeout(PRUint32 aId)
{
  nsRefPtr<nsDOMWorkerTimeout> foundTimeout;
  {
    nsAutoLock lock(mLock);
    for (nsDOMWorkerTimeout* timeout = FirstTimeout();
         timeout;
         timeout = NextTimeout(timeout)) {
      if (timeout->GetId() == aId) {
        foundTimeout = timeout;
        break;
      }
    }
  }

  if (foundTimeout) {
    foundTimeout->Cancel();
  }
}

void
nsDOMWorkerThread::SuspendTimeouts()
{
  nsAutoTArray<nsRefPtr<nsDOMWorkerTimeout>, 20> timeouts;
  {
    nsAutoLock lock(mLock);
    for (nsDOMWorkerTimeout* timeout = FirstTimeout();
         timeout;
         timeout = NextTimeout(timeout)) {
      timeouts.AppendElement(timeout);
    }
  }

  PRTime now = PR_Now();

  PRUint32 count = timeouts.Length();
  for (PRUint32 i = 0; i < count; i++) {
    timeouts[i]->Suspend(now);
  }
}

void
nsDOMWorkerThread::ResumeTimeouts()
{
  nsAutoTArray<nsRefPtr<nsDOMWorkerTimeout>, 20> timeouts;
  {
    nsAutoLock lock(mLock);
    for (nsDOMWorkerTimeout* timeout = FirstTimeout();
         timeout;
         timeout = NextTimeout(timeout)) {
      NS_ASSERTION(timeout->IsSuspended(), "Should be suspended!");
      timeouts.AppendElement(timeout);
    }
  }

  PRTime now = PR_Now();

  PRUint32 count = timeouts.Length();
  for (PRUint32 i = 0; i < count; i++) {
    timeouts[i]->Resume(now);
  }
}

NS_IMETHODIMP
nsDOMWorkerThread::PostMessage(const nsAString& aMessage)
{
  nsresult rv = PostMessageInternal(aMessage);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}
