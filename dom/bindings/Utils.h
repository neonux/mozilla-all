/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* vim: set ts=2 sw=2 et tw=79: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bindings_Utils_h__
#define mozilla_dom_bindings_Utils_h__

#include "mozilla/dom/bindings/DOMJSClass.h"

#include "jsapi.h"
#include "jstypedarray.h"

#include "XPCQuickStubs.h"
#include "XPCWrapper.h"
#include "nsTraceRefcnt.h"
#include "nsWrapperCacheInlines.h"

namespace mozilla {
namespace dom {
namespace bindings {

template<bool mainThread>
inline bool
Throw(JSContext* cx, nsresult rv)
{
  // XXX Introduce exception machinery.
  if (mainThread) {
    XPCThrower::Throw(rv, cx);
  } else {
    if (!JS_IsExceptionPending(cx)) {
      JS_ReportError(cx, "Exception thrown (nsresult = %x).", rv);
    }
  }
  return false;
}

template<bool mainThread>
inline bool
ThrowMethodFailedWithDetails(JSContext* cx, nsresult rv,
                             const char* /* ifaceName */,
                             const char* /* memberName */)
{
  return Throw<mainThread>(cx, rv);
}

inline bool
IsDOMClass(const JSClass* clasp)
{
  return clasp->flags & JSCLASS_IS_DOMJSCLASS;
}

template <class T>
inline T*
UnwrapDOMObject(JSObject* obj, const JSClass* clasp)
{
  MOZ_ASSERT(IsDOMClass(clasp));
  MOZ_ASSERT(JS_GetClass(obj) == clasp);

  size_t slot = DOMJSClass::FromJSClass(clasp)->mNativeSlot;
  MOZ_ASSERT((slot == DOM_OBJECT_SLOT &&
              !(clasp->flags & JSCLASS_DOM_GLOBAL)) ||
             (slot == DOM_GLOBAL_OBJECT_SLOT &&
              (clasp->flags & JSCLASS_DOM_GLOBAL)));

  JS::Value val = js::GetReservedSlot(obj, slot);
  // XXXbz/khuey worker code tries to unwrap interface objects (which have
  // nothing here).  That needs to stop.
  // XXX We don't null-check UnwrapObject's result; aren't we going to crash
  // anyway?
  if (val.isUndefined()) {
    return NULL;
  }
  
  return static_cast<T*>(val.toPrivate());
}

template <class T>
inline T*
UnwrapDOMObject(JSObject* obj, const js::Class* clasp)
{
  return UnwrapDOMObject<T>(obj, Jsvalify(clasp));
}

// Some callers don't want to set an exception when unwrappin fails
// (for example, overload resolution uses unwrapping to tell what sort
// of thing it's looking at).
template <prototypes::ID PrototypeID, class T>
inline nsresult
UnwrapObject(JSContext* cx, JSObject* obj, T** value)
{
  /* First check to see whether we have a DOM object */
  JSClass* clasp = js::GetObjectJSClass(obj);
  if (!IsDOMClass(clasp)) {
    /* Maybe we have a security wrapper or outer window? */
    if (!js::IsWrapper(obj)) {
      /* Not a DOM object, not a wrapper, just bail */
      return NS_ERROR_XPC_BAD_CONVERT_JS;
    }

    obj = XPCWrapper::Unwrap(cx, obj, false);
    if (!obj) {
      return NS_ERROR_XPC_SECURITY_MANAGER_VETO;
    }
    MOZ_ASSERT(!js::IsWrapper(obj));
    clasp = js::GetObjectJSClass(obj);
    if (!IsDOMClass(clasp)) {
      /* We don't have a DOM object */
      return NS_ERROR_XPC_BAD_CONVERT_JS;
    }
  }

  MOZ_ASSERT(IsDOMClass(clasp));

  /* This object is a DOM object.  Double-check that it is safely
     castable to T by checking whether it claims to inherit from the
     class identified by protoID. */
  DOMJSClass* domClass = DOMJSClass::FromJSClass(clasp);
  if (domClass->mInterfaceChain[PrototypeTraits<PrototypeID>::Depth] ==
      PrototypeID) {
    *value = UnwrapDOMObject<T>(obj, clasp);
    return NS_OK;
  }

  /* It's the wrong sort of DOM object */
  return NS_ERROR_XPC_BAD_CONVERT_JS;
}

inline bool
IsArrayLike(JSContext* cx, JSObject* obj)
{
  MOZ_ASSERT(obj);
  // For simplicity, check for security wrappers up front
  if (js::IsWrapper(obj)) {
    obj = XPCWrapper::Unwrap(cx, obj, false);
    if (!obj) {
      // Let's say it's not
      return false;
    }
  }

  // XXXbz need to detect platform objects (including listbinding
  // ones) with indexGetters here!
  return JS_IsArrayObject(cx, obj);
}

inline bool
IsPlatformObject(JSContext* cx, JSObject* obj)
{
  // XXXbz Should be treating list-binding objects as platform objects
  // too?  The one consumer so far wants non-array-like platform
  // objects, so listbindings that have an indexGetter should test
  // false from here.  Maybe this function should have a different
  // name?
  MOZ_ASSERT(obj);
  // Fast-path the common case
  JSClass* clasp = js::GetObjectJSClass(obj);
  if (IsDOMClass(clasp)) {
    return true;
  }
  // Now for simplicity check for security wrappers before anything else
  if (js::IsWrapper(obj)) {
    obj = XPCWrapper::Unwrap(cx, obj, false);
    if (!obj) {
      // Let's say it's not
      return false;
    }
    clasp = js::GetObjectJSClass(obj);
  }
  return IS_WRAPPER_CLASS(js::Valueify(clasp)) || IsDOMClass(clasp) ||
    JS_IsArrayBufferObject(obj);
}

template <class T>
inline nsresult
UnwrapObject(JSContext* cx, JSObject* obj, T* *value)
{
  return UnwrapObject<static_cast<prototypes::ID>(
           PrototypeIDMap<T>::PrototypeID)>(cx, obj, value);
}

const size_t kProtoOrIfaceCacheCount =
  prototypes::id::_ID_Count + constructors::id::_ID_Count;

inline void
AllocateProtoOrIfaceCache(JSObject* obj)
{
  MOZ_ASSERT(js::GetObjectClass(obj)->flags & JSCLASS_DOM_GLOBAL);
  MOZ_ASSERT(js::GetReservedSlot(obj, DOM_PROTOTYPE_SLOT).isUndefined());

  // Important: The () at the end ensure zero-initialization
  JSObject** protoOrIfaceArray = new JSObject*[kProtoOrIfaceCacheCount]();

  js::SetReservedSlot(obj, DOM_PROTOTYPE_SLOT,
                      JS::PrivateValue(protoOrIfaceArray));
}

inline void
DestroyProtoOrIfaceCache(JSObject* obj)
{
  MOZ_ASSERT(js::GetObjectClass(obj)->flags & JSCLASS_DOM_GLOBAL);

  JSObject** protoOrIfaceArray = GetProtoOrIfaceArray(obj);

  delete [] protoOrIfaceArray;
}

struct ConstantSpec
{
  const char* name;
  JS::Value value;
};

/*
 * Create a DOM interface object (if constructorClass is non-null) and/or a
 * DOM interface prototype object (if protoClass is non-null).
 *
 * parentProto is the prototype to use for the interface prototype object.
 * protoClass is the JSClass to use for the interface prototype object.
 *            This is null if we should not create an interface prototype
 *            object.
 * constructorClass is the JSClass to use for the interface object.
 *                  This is null if we should not create an interface object.
 * methods and properties are to be defined on the interface prototype object;
 *                        these arguments are allowed to be null if there are no
 *                        methods or properties respectively.
 * constants are to be defined on the interface object and on the interface
 *           prototype object; allowed to be null if there are no constants.
 * staticMethods are to be defined on the interface object; allowed to be null
 *               if there are no static methods.
 *
 * At least one of protoClass and constructorClass should be non-null.
 * If constructorClass is non-null, the resulting interface object will be
 * defined on the given global with property name |name|, which must also be
 * non-null.
 *
 * returns the interface prototype object if protoClass is non-null, else it
 * returns the interface object.
 */
JSObject*
CreateInterfaceObjects(JSContext* cx, JSObject* global, JSObject* parentProto,
                       JSClass* protoClass, JSClass* constructorClass,
                       JSFunctionSpec* methods, JSPropertySpec* properties,
                       ConstantSpec* constants, JSFunctionSpec* staticMethods,
                       const char* name);

template <class T>
inline bool
WrapNewBindingObject(JSContext* cx, JSObject* scope, T* value, JS::Value* vp)
{
  JSObject* obj = value->GetWrapper();
  if (obj && js::GetObjectCompartment(obj) == js::GetObjectCompartment(scope)) {
    *vp = JS::ObjectValue(*obj);
    return true;
  }

  if (!obj) {
    bool triedToWrap;
    obj = value->WrapObject(cx, scope, &triedToWrap);
    if (!obj) {
      // At this point, obj is null, so just return false.  We could
      // try to communicate triedToWrap to the caller, but in practice
      // callers seem to be testing JS_IsExceptionPending(cx) to
      // figure out whether WrapObject() threw instead.
      return false;
    }
  }

  // Now make sure that |obj| is wrapped for the compartment of |scope|
  // correctly.  That means entering the compartment of |scope|.
  JSAutoEnterCompartment ac;
  if (!ac.enter(cx, scope)) {
    return false;
  }
  *vp = JS::ObjectValue(*obj);
  return JS_WrapValue(cx, vp);
}

// Helper for smart pointers (nsAutoPtr/nsRefPtr/nsCOMPtr).
template <template <class> class SmartPtr, class T>
inline bool
WrapNewBindingObject(JSContext* cx, JSObject* scope, const SmartPtr<T>& value,
                     JS::Value* vp)
{
  return WrapNewBindingObject(cx, scope, value.get(), vp);
}

/**
 * A method to handle new-binding wrap failure, by possibly falling back to
 * wrapping as a non-new-binding object.
 */
bool
DoHandleNewBindingWrappingFailure(JSContext* cx, JSObject* scope,
                                  nsISupports* value, JS::Value* vp);

/**
 * An easy way to call the above when you have a value which
 * multiply-inherits from nsISupports.
 */
template <class T>
bool
HandleNewBindingWrappingFailure(JSContext* cx, JSObject* scope, T* value,
                                JS::Value* vp)
{
  nsCOMPtr<nsISupports> val;
  CallQueryInterface(value, getter_AddRefs(val));
  return DoHandleNewBindingWrappingFailure(cx, scope, val, vp);
}

// Helper for smart pointers (nsAutoPtr/nsRefPtr/nsCOMPtr).
template <template <class> class SmartPtr, class T>
MOZ_ALWAYS_INLINE bool
HandleNewBindingWrappingFailure(JSContext* cx, JSObject* scope,
                                const SmartPtr<T>& value, JS::Value* vp)
{
  return HandleNewBindingWrappingFailure(cx, scope, value.get(), vp);
}

struct EnumEntry {
  const char* value;
  size_t length;
};

inline int
FindEnumStringIndex(JSContext* cx, JS::Value v, const EnumEntry* values, bool* ok)
{
  // JS_StringEqualsAscii is slow as molasses, so don't use it here.
  JSString* str = JS_ValueToString(cx, v);
  if (!str) {
    *ok = false;
    return 0;
  }
  JS::Anchor<JSString*> anchor(str);
  size_t length;
  const jschar* chars = JS_GetStringCharsAndLength(cx, str, &length);
  if (!chars) {
    *ok = false;
    return 0;
  }
  int i = 0;
  for (const EnumEntry* value = values; value->value; ++value, ++i) {
    if (length != value->length) {
      continue;
    }

    bool equal = true;
    const char* val = value->value;
    for (size_t j = 0; j != length; ++j) {
      if (unsigned(val[j]) != unsigned(chars[j])) {
        equal = false;
        break;
      }
    }

    if (equal) {
      *ok = true;
      return i;
    }
  }

  // XXX we don't know whether we're on the main thread, so play it safe
  *ok = Throw<false>(cx, NS_ERROR_XPC_BAD_CONVERT_JS);
  return 0;
}

inline nsWrapperCache*
GetWrapperCache(nsWrapperCache* cache)
{
  return cache;
}

// nsGlobalWindow implements nsWrapperCache, but doesn't always use it. Don't
// try to use it without fixing that first.
class nsGlobalWindow;
inline nsWrapperCache*
GetWrapperCache(nsGlobalWindow* not_allowed);

inline nsWrapperCache*
GetWrapperCache(void* p)
{
  return NULL;
}

// Only set allowNativeWrapper to false if you really know you need it, if in
// doubt use true. Setting it to false disables security wrappers.
bool
XPCOMObjectToJsval(JSContext* cx, JSObject* scope, xpcObjectHelper &helper,
                   const nsIID* iid, bool allowNativeWrapper, JS::Value* rval);

template<class T>
inline bool
WrapObject(JSContext* cx, JSObject* scope, T* p, nsWrapperCache* cache,
           const nsIID* iid, JS::Value* vp)
{
  if (xpc_FastGetCachedWrapper(cache, scope, vp))
    return true;
  qsObjectHelper helper(p, cache);
  return XPCOMObjectToJsval(cx, scope, helper, iid, true, vp);
}

template<class T>
inline bool
WrapObject(JSContext* cx, JSObject* scope, T* p, const nsIID* iid,
           JS::Value* vp)
{
  return WrapObject(cx, scope, p, GetWrapperCache(p), iid, vp);
}

template<class T>
inline bool
WrapObject(JSContext* cx, JSObject* scope, T* p, JS::Value* vp)
{
  return WrapObject(cx, scope, p, NULL, vp);
}

template<class T>
inline bool
WrapObject(JSContext* cx, JSObject* scope, nsCOMPtr<T> &p, const nsIID* iid,
           JS::Value* vp)
{
  return WrapObject(cx, scope, p.get(), iid, vp);
}

template<class T>
inline bool
WrapObject(JSContext* cx, JSObject* scope, nsCOMPtr<T> &p, JS::Value* vp)
{
  return WrapObject(cx, scope, p, NULL, vp);
}

template<class T>
inline bool
WrapObject(JSContext* cx, JSObject* scope, nsRefPtr<T> &p, const nsIID* iid,
           JS::Value* vp)
{
  return WrapObject(cx, scope, p.get(), iid, vp);
}

template<class T>
inline bool
WrapObject(JSContext* cx, JSObject* scope, nsRefPtr<T> &p, JS::Value* vp)
{
  return WrapObject(cx, scope, p, NULL, vp);
}

template<>
inline bool
WrapObject<JSObject>(JSContext* cx, JSObject* scope, JSObject* p, JS::Value* vp)
{
  vp->setObjectOrNull(p);
  return true;
}

template<class T>
static inline JSObject*
WrapNativeParent(JSContext* cx, JSObject* scope, T* p)
{
  if (!p)
    return scope;

  nsWrapperCache* cache = GetWrapperCache(p);
  JSObject* obj;
  if (cache && (obj = cache->GetWrapper())) {
#ifdef DEBUG
    qsObjectHelper helper(p, cache);
    JS::Value debugVal;

    bool ok = XPCOMObjectToJsval(cx, scope, helper, NULL, false, &debugVal);
    NS_ASSERTION(ok && JSVAL_TO_OBJECT(debugVal) == obj,
                 "Unexpected object in nsWrapperCache");
#endif
    return obj;
  }

  qsObjectHelper helper(p, cache);
  JS::Value v;
  return XPCOMObjectToJsval(cx, scope, helper, NULL, false, &v) ?
         JSVAL_TO_OBJECT(v) :
         NULL;
}

// Spec needs a name property
template <typename Spec>
static bool
InitIds(JSContext* cx, Spec* specs, jsid* ids)
{
  Spec* spec = specs;
  do {
    JSString *str = ::JS_InternString(cx, spec->name);
    if (!str) {
      return false;
    }

    *ids = INTERNED_STRING_TO_JSID(cx, str);
  } while (++ids, (++spec)->name);

  return true;
}

JSBool
QueryInterface(JSContext* cx, unsigned argc, JS::Value* vp);

} // namespace bindings
} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_bindings_Utils_h__ */
