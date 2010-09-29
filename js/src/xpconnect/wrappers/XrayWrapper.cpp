/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code, released
 * June 24, 2010.
 *
 * The Initial Developer of the Original Code is
 *    The Mozilla Foundation
 *
 * Contributor(s):
 *    Andreas Gal <gal@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "XrayWrapper.h"
#include "AccessCheck.h"
#include "FilteringWrapper.h"
#include "CrossOriginWrapper.h"
#include "WrapperFactory.h"

#include "jscntxt.h"

#include "XPCWrapper.h"
#include "xpcprivate.h"

namespace xpc {

using namespace js;

static const uint32 JSSLOT_WN_OBJ = JSSLOT_PRIVATE;
static const uint32 JSSLOT_RESOLVING = JSSLOT_PRIVATE + 1;

namespace XrayUtils {

const uint32 JSSLOT_PROXY_OBJ = JSSLOT_PRIVATE + 2;

}

class ResolvingId
{
  public:
    ResolvingId(JSObject *holder, jsid id)
      : mId(id),
        mPrev(getResolvingId(holder)),
        mHolder(holder)
    {
        holder->setSlot(JSSLOT_RESOLVING, PrivateValue(this));
    }

    ~ResolvingId() {
        NS_ASSERTION(getResolvingId(mHolder) == this, "unbalanced ResolvingIds");
        mHolder->setSlot(JSSLOT_RESOLVING, PrivateValue(mPrev));
    }

    static ResolvingId *getResolvingId(JSObject *holder) {
        return (ResolvingId *)holder->getSlot(JSSLOT_RESOLVING).toPrivate();
    }

    jsid mId;
    ResolvingId *mPrev;

  private:
    JSObject *mHolder;
};

static bool
IsResolving(JSObject *holder, jsid id)
{
    for (ResolvingId *cur = ResolvingId::getResolvingId(holder); cur; cur = cur->mPrev) {
        if (cur->mId == id)
            return true;
    }

    return false;
}

static JSBool
holder_get(JSContext *cx, JSObject *holder, jsid id, jsval *vp);

static JSBool
holder_set(JSContext *cx, JSObject *holder, jsid id, jsval *vp);

static JSBool
holder_enumerate(JSContext *cx, JSObject *holder);

namespace XrayUtils {

JSClass HolderClass = {
    "NativePropertyHolder",
    JSCLASS_HAS_RESERVED_SLOTS(3),
    JS_PropertyStub,        JS_PropertyStub, holder_get,      holder_set,
    holder_enumerate,       JS_ResolveStub,  JS_ConvertStub,  NULL,
    NULL,                   NULL,            NULL,            NULL,
    NULL,                   NULL,            NULL,            NULL
};

}

using namespace XrayUtils;

static JSObject *
GetHolder(JSObject *obj)
{
    return &obj->getProxyExtra().toObject();
}

static XPCWrappedNative *
GetWrappedNative(JSObject *obj)
{
    NS_ASSERTION(IS_WN_WRAPPER_OBJECT(obj), "expected a wrapped native here");
    return static_cast<XPCWrappedNative *>(obj->getPrivate());
}

static JSObject *
GetWrappedNativeObjectFromHolder(JSContext *cx, JSObject *holder)
{
    NS_ASSERTION(holder->getJSClass() == &HolderClass, "expected a native property holder object");
    JSObject *wrappedObj = &holder->getSlot(JSSLOT_WN_OBJ).toObject();
    OBJ_TO_INNER_OBJECT(cx, wrappedObj);
    return wrappedObj;
}

// Some DOM objects have shared properties that don't have an explicit
// getter/setter and rely on the class getter/setter. We install a
// class getter/setter on the holder object to trigger them.
static JSBool
holder_get(JSContext *cx, JSObject *wrapper, jsid id, jsval *vp)
{
    NS_ASSERTION(wrapper->isProxy(), "bad this object in get");
    JSObject *holder = GetHolder(wrapper);

    JSObject *wnObject = GetWrappedNativeObjectFromHolder(cx, holder);
    XPCWrappedNative *wn = GetWrappedNative(wnObject);
    if (NATIVE_HAS_FLAG(wn, WantGetProperty)) {
        JSBool retval = true;
        nsresult rv = wn->GetScriptableCallback()->GetProperty(wn, cx, wrapper, id, vp, &retval);
        if (NS_FAILED(rv)) {
            if (retval)
                XPCThrower::Throw(rv, cx);
            return false;
        }
    }
    return true;
}

static JSBool
holder_set(JSContext *cx, JSObject *wrapper, jsid id, jsval *vp)
{
    NS_ASSERTION(wrapper->isProxy(), "bad this object in set");
    JSObject *holder = GetHolder(wrapper);
    JSObject *wnObject = GetWrappedNativeObjectFromHolder(cx, holder);

    XPCWrappedNative *wn = GetWrappedNative(wnObject);
    if (NATIVE_HAS_FLAG(wn, WantSetProperty)) {
        JSBool retval = true;
        nsresult rv = wn->GetScriptableCallback()->SetProperty(wn, cx, wrapper, id, vp, &retval);
        if (NS_FAILED(rv)) {
            if (retval)
                XPCThrower::Throw(rv, cx);
            return false;
        }
    }
    return true;
}

static bool
ResolveNativeProperty(JSContext *cx, JSObject *wrapper, JSObject *holder, jsid id, bool set,
                      JSPropertyDescriptor *desc)
{
    desc->obj = NULL;

    NS_ASSERTION(holder->getJSClass() == &HolderClass, "expected a native property holder object");
    JSObject *wnObject = GetWrappedNativeObjectFromHolder(cx, holder);
    XPCWrappedNative *wn = GetWrappedNative(wnObject);

    // This will do verification and the method lookup for us.
    XPCCallContext ccx(JS_CALLER, cx, wnObject, nsnull, id);

    // Run the resolve hook of the wrapped native.
    if (NATIVE_HAS_FLAG(wn, WantNewResolve)) {
        JSAutoEnterCompartment ac;
        if (!ac.enter(cx, holder))
            return false;

        JSBool retval = true;
        JSObject *pobj = NULL;
        uintN flags = (set ? JSRESOLVE_ASSIGNING : 0) | JSRESOLVE_QUALIFIED;
        nsresult rv = wn->GetScriptableInfo()->GetCallback()->NewResolve(wn, cx, wrapper, id,
                                                                         flags, &pobj, &retval);
        if (NS_FAILED(rv)) {
            if (retval) {
                XPCThrower::Throw(rv, cx);
            }
            return false;
        }

        if (pobj)
            return JS_GetPropertyDescriptorById(cx, pobj, id, flags, desc);
    }

    // There are no native numeric properties, so we can shortcut here. We will not
    // find the property.
    if (!JSID_IS_ATOM(id)) {
        /* Not found */
        return true;
    }

    XPCNativeInterface *iface;
    XPCNativeMember *member;
    if (ccx.GetWrapper() != wn ||
        !wn->IsValid()  ||
        !(iface = ccx.GetInterface()) ||
        !(member = ccx.GetMember())) {
        /* Not found */
        return true;
    }

    desc->obj = holder;
    desc->attrs = JSPROP_ENUMERATE;
    desc->getter = NULL;
    desc->setter = NULL;
    desc->shortid = NULL;
    desc->value = JSVAL_VOID;

    jsval fval = JSVAL_VOID;
    if (member->IsConstant()) {
        if (!member->GetConstantValue(ccx, iface, &desc->value)) {
            JS_ReportError(cx, "Failed to convert constant native property to JS value");
            return false;
        }
    } else if (member->IsAttribute()) {
        // This is a getter/setter. Clone a function for it.
        if (!member->NewFunctionObject(ccx, iface, wnObject, &fval)) {
            JS_ReportError(cx, "Failed to clone function object for native getter/setter");
            return false;
        }

        desc->attrs |= JSPROP_GETTER;
        if (member->IsWritableAttribute())
            desc->attrs |= JSPROP_SETTER;

        // Make the property shared on the holder so no slot is allocated
        // for it. This avoids keeping garbage alive through that slot.
        desc->attrs |= JSPROP_SHARED;
    } else {
        // This is a method. Clone a function for it.
        if (!member->NewFunctionObject(ccx, iface, wnObject, &desc->value)) {
            JS_ReportError(cx, "Failed to clone function object for native function");
            return false;
        }

        // Without a wrapper the function would live on the prototype. Since we
        // don't have one, we have to avoid calling the scriptable helper's
        // GetProperty method for this property, so stub out the getter and
        // setter here explicitly.
        desc->getter = desc->setter = JS_PropertyStub;
    }

    JSAutoEnterCompartment ac;
    if (!ac.enter(cx, holder))
        return false;

    if (!JS_WrapValue(cx, &desc->value) || !JS_WrapValue(cx, &fval))
        return false;

    if (desc->attrs & JSPROP_GETTER)
        desc->getter = CastAsJSPropertyOp(JSVAL_TO_OBJECT(fval));
    if (desc->attrs & JSPROP_SETTER)
        desc->setter = desc->getter;

    // Define the property.
    return JS_DefinePropertyById(cx, holder, id, desc->value,
                                 desc->getter, desc->setter, desc->attrs);
}

static JSBool
holder_enumerate(JSContext *cx, JSObject *holder)
{
    // Ask the native wrapper for all its ids
    JSIdArray *ida = JS_Enumerate(cx, GetWrappedNativeObjectFromHolder(cx, holder));
    if (!ida)
        return false;

    JSObject *wrapper = &holder->getSlot(JSSLOT_PROXY_OBJ).toObject();

    // Resolve the underlying native properties onto the holder object
    jsid *idp = ida->vector;
    size_t length = ida->length;
    while (length-- > 0) {
        JSPropertyDescriptor dummy;
        if (!ResolveNativeProperty(cx, wrapper, holder, *idp++, false, &dummy))
            return false;
    }
    return true;
}

extern JSCrossCompartmentWrapper XrayWrapperWaivedWrapper;

static JSBool
wrappedJSObject_getter(JSContext *cx, JSObject *holder, jsid id, jsval *vp)
{
    if (holder->isWrapper())
        holder = GetHolder(holder);

    // If the caller intentionally waives the X-ray wrapper we usually
    // apply for wrapped natives, use a special wrapper to make sure the
    // membrane will not automatically apply an X-ray wrapper.
    JSObject *wn = GetWrappedNativeObjectFromHolder(cx, holder);

    // We have to make sure that if we're wrapping an outer window, that
    // the .wrappedJSObject also wraps the outer window.
    OBJ_TO_OUTER_OBJECT(cx, wn);
    if (!wn)
        return false;
    JSObject *obj = JSWrapper::New(cx, wn, NULL, holder->getParent(), &XrayWrapperWaivedWrapper);
    if (!obj)
        return false;
    *vp = OBJECT_TO_JSVAL(obj);
    return true;
}

template <typename Base, typename Policy>
XrayWrapper<Base, Policy>::XrayWrapper(int flags) : Base(JSWrapper::getWrapperFamily())
{
}

template <typename Base, typename Policy>
XrayWrapper<Base, Policy>::~XrayWrapper()
{
}

template <typename Base, typename Policy>
class AutoLeaveHelper
{
  public:
    AutoLeaveHelper(XrayWrapper<Base, Policy> &xray, JSContext *cx, JSObject *wrapper)
      : xray(xray), cx(cx), wrapper(wrapper)
    {
    }
    ~AutoLeaveHelper()
    {
        xray.leave(cx, wrapper);
    }

  private:
    XrayWrapper<Base, Policy> &xray;
    JSContext *cx;
    JSObject *wrapper;
};

template <typename Base, typename Policy>
bool
XrayWrapper<Base, Policy>::getPropertyDescriptor(JSContext *cx, JSObject *wrapper, jsid id,
                                                 bool set, PropertyDescriptor *desc_in)
{
    if (!this->enter(cx, wrapper, id, set ? JSWrapper::SET : JSWrapper::GET))
        return false;

    AutoLeaveHelper<Base, Policy> helper(*this, cx, wrapper);

    JSPropertyDescriptor *desc = Jsvalify(desc_in);

    if (id == nsXPConnect::GetRuntimeInstance()->GetStringID(XPCJSRuntime::IDX_WRAPPED_JSOBJECT)) {
        desc->obj = wrapper;
        desc->attrs = JSPROP_ENUMERATE|JSPROP_SHARED;
        desc->getter = wrappedJSObject_getter;
        desc->setter = NULL;
        desc->shortid = NULL;
        desc->value = JSVAL_VOID;
        return true;
    }

    JSObject *holder = GetHolder(wrapper);
    if (IsResolving(holder, id)) {
        desc->obj = NULL;
        return true;
    }

    ResolvingId resolving(holder, id);

    void *priv;
    if (!Policy::enter(cx, wrapper, &id, set ? JSWrapper::SET : JSWrapper::GET, &priv))
        return false;

    bool ok = ResolveNativeProperty(cx, wrapper, holder, id, false, desc);
    Policy::leave(cx, wrapper, priv);
    if (!ok || desc->obj)
        return ok;

    return JS_GetPropertyDescriptorById(cx, holder, id,
                                        (set ? JSRESOLVE_ASSIGNING : 0) | JSRESOLVE_QUALIFIED,
                                        desc);
}

template <typename Base, typename Policy>
bool
XrayWrapper<Base, Policy>::getOwnPropertyDescriptor(JSContext *cx, JSObject *wrapper, jsid id,
                                                    bool set, PropertyDescriptor *desc)
{
    return getPropertyDescriptor(cx, wrapper, id, set, desc);
}

template <typename Base, typename Policy>
bool
XrayWrapper<Base, Policy>::defineProperty(JSContext *cx, JSObject *wrapper, jsid id,
                                          js::PropertyDescriptor *desc)
{
    JSObject *holder = GetHolder(wrapper);
    PropertyDescriptor existing_desc;
    if (!getOwnPropertyDescriptor(cx, wrapper, id, true, &existing_desc))
        return false;

    if (existing_desc.obj && (existing_desc.attrs & JSPROP_PERMANENT))
        return true; // XXX throw?

    JSPropertyDescriptor *jsdesc = Jsvalify(desc);
    if (!(jsdesc->attrs & (JSPROP_GETTER | JSPROP_SETTER))) {
        if (!desc->getter)
            jsdesc->getter = holder_get;
        if (!desc->setter)
            jsdesc->setter = holder_set;
    }

    return JS_DefinePropertyById(cx, holder, id, jsdesc->value, jsdesc->getter, jsdesc->setter,
                                 jsdesc->attrs);
}

template <typename Base, typename Policy>
bool
XrayWrapper<Base, Policy>::getOwnPropertyNames(JSContext *cx, JSObject *wrapper,
                                               js::AutoIdVector &props)
{
    // XXX implement me.
    return true;
}

template <typename Base, typename Policy>
bool
XrayWrapper<Base, Policy>::delete_(JSContext *cx, JSObject *wrapper, jsid id, bool *bp)
{
    // XXX implement me.
    return true;
}

template <typename Base, typename Policy>
bool
XrayWrapper<Base, Policy>::enumerate(JSContext *cx, JSObject *wrapper, js::AutoIdVector &props)
{
    // XXX implement me.
    return true;
}

template <typename Base, typename Policy>
bool
XrayWrapper<Base, Policy>::fix(JSContext *cx, JSObject *proxy, js::Value *vp)
{
    vp->setUndefined();
    return true;
}

template <typename Base, typename Policy>
bool
XrayWrapper<Base, Policy>::get(JSContext *cx, JSObject *wrapper, JSObject *receiver, jsid id,
                               js::Value *vp)
{
    // Skip our Base if it isn't already JSProxyHandler.
    return JSProxyHandler::get(cx, wrapper, receiver, id, vp);
}

template <typename Base, typename Policy>
bool
XrayWrapper<Base, Policy>::set(JSContext *cx, JSObject *wrapper, JSObject *receiver, jsid id,
                               js::Value *vp)
{
    // Skip our Base if it isn't already JSProxyHandler.
    return JSProxyHandler::set(cx, wrapper, receiver, id, vp);
}

template <typename Base, typename Policy>
bool
XrayWrapper<Base, Policy>::has(JSContext *cx, JSObject *wrapper, jsid id, bool *bp)
{
    // Skip our Base if it isn't already JSProxyHandler.
    return JSProxyHandler::has(cx, wrapper, id, bp);
}

template <typename Base, typename Policy>
bool
XrayWrapper<Base, Policy>::hasOwn(JSContext *cx, JSObject *wrapper, jsid id, bool *bp)
{
    // Skip our Base if it isn't already JSProxyHandler.
    return JSProxyHandler::hasOwn(cx, wrapper, id, bp);
}

template <typename Base, typename Policy>
bool
XrayWrapper<Base, Policy>::enumerateOwn(JSContext *cx, JSObject *wrapper, js::AutoIdVector &props)
{
    // Skip our Base if it isn't already JSProxyHandler.
    return JSProxyHandler::enumerateOwn(cx, wrapper, props);
}

template <typename Base, typename Policy>
bool
XrayWrapper<Base, Policy>::iterate(JSContext *cx, JSObject *wrapper, uintN flags, js::Value *vp)
{
    // Skip our Base if it isn't already JSProxyHandler.
    return JSProxyHandler::iterate(cx, wrapper, flags, vp);
}

template <typename Base, typename Policy>
JSObject *
XrayWrapper<Base, Policy>::createHolder(JSContext *cx, JSObject *wrappedNative, JSObject *parent)
{
    JSObject *holder = JS_NewObjectWithGivenProto(cx, &HolderClass, nsnull, parent);
    if (!holder)
        return nsnull;

    holder->setSlot(JSSLOT_WN_OBJ, ObjectValue(*wrappedNative));
    holder->setSlot(JSSLOT_RESOLVING, PrivateValue(NULL));
    return holder;
}

bool
CrossCompartmentXray::enter(JSContext *cx, JSObject *wrapper, jsid *idp,
                            JSWrapper::Action act, void **priv)
{
    JSObject *target = wrapper->unwrap();
    JSCrossCompartmentCall *call = JS_EnterCrossCompartmentCall(cx, target);
    if (!call)
        return false;

    *priv = call;
    // XXX wrap id
    return true;
}

void
CrossCompartmentXray::leave(JSContext *cx, JSObject *wrapper, void *priv)
{
    JS_LeaveCrossCompartmentCall(static_cast<JSCrossCompartmentCall *>(priv));
}

#define XPCNW XrayWrapper<JSCrossCompartmentWrapper, CrossCompartmentXray>
#define SCNW XrayWrapper<JSWrapper, SameCompartmentXray>

template <> XPCNW XPCNW::singleton(0);
template <> SCNW SCNW::singleton(0);

template class XPCNW;
template class SCNW;

}
