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

#include "jsobj.h"
#include "jsvalue.h"

#include "WrapperFactory.h"
#include "CrossOriginWrapper.h"
#include "FilteringWrapper.h"
#include "XrayWrapper.h"
#include "AccessCheck.h"
#include "XPCWrapper.h"

#include "xpcprivate.h"

namespace xpc {

// When chrome pulls a naked property across the membrane using
// .wrappedJSObject, we want it to cross the membrane into the
// chrome compartment without automatically being wrapped into an
// X-ray wrapper. We achieve this by wrapping it into a special
// transparent wrapper in the origin (non-chrome) compartment. When
// an object with that special wrapper applied crosses into chrome,
// we know to not apply an X-ray wrapper.
JSWrapper WaiveXrayWrapperWrapper(WrapperFactory::WAIVE_XRAY_WRAPPER_FLAG);

// When objects for which we waived the X-ray wrapper cross into
// chrome, we wrap them into a special cross-compartment wrapper
// that transitively extends the waiver to all properties we get
// off it.
CrossOriginWrapper CrossOriginWrapper::singleton(0);

static JSObject *
DoubleWrap(JSContext *cx, JSObject *obj, uintN flags)
{
    if (flags & WrapperFactory::WAIVE_XRAY_WRAPPER_FLAG) {
        js::SwitchToCompartment sc(cx, obj->compartment());
        return JSWrapper::New(cx, obj, NULL, obj->getParent(),
                              &WaiveXrayWrapperWrapper);
    }
    return obj;
}

static JSObject *
GetCurrentOuter(JSContext *cx, JSObject *obj)
{
    OBJ_TO_OUTER_OBJECT(cx, obj);
    if (obj->isWrapper() && !obj->getClass()->ext.innerObject) {
        obj = obj->unwrap();
        NS_ASSERTION(obj->getClass()->ext.innerObject,
                     "weird object, expecting an outer window proxy");
    }

    return obj;
}

JSObject *
WrapperFactory::PrepareForWrapping(JSContext *cx, JSObject *scope, JSObject *obj, uintN flags)
{
    // Don't unwrap an outer window, just double wrap it if needed.
    if (obj->getClass()->ext.innerObject)
        return DoubleWrap(cx, obj, flags);

    // Here are the rules for wrapping:
    // We should never get a proxy here (the JS engine unwraps those for us).
    JS_ASSERT(!obj->isWrapper());

    // As soon as an object is wrapped in a security wrapper, it morphs to be
    // a fat wrapper. (see also: bug XXX).
    if (IS_SLIM_WRAPPER(obj) && !MorphSlimWrapper(cx, obj))
        return nsnull;

    // We only hand out outer objects to script.
    obj = GetCurrentOuter(cx, obj);
    if (obj->getClass()->ext.innerObject)
        return DoubleWrap(cx, obj, flags);

    // Now, our object is ready to be wrapped, but several objects (notably
    // nsJSIIDs) have a wrapper per scope. If we are about to wrap one of
    // those objects in a security wrapper, then we need to hand back the
    // wrapper for the new scope instead. Also, global objects don't move
    // between scopes so for those we also want to return the wrapper. So...
    if (!IS_WN_WRAPPER(obj) || !obj->getParent())
        return DoubleWrap(cx, obj, flags);

    XPCWrappedNative *wn = static_cast<XPCWrappedNative *>(xpc_GetJSPrivate(obj));

    // If the object doesn't have classinfo we want to return the same
    // XPCWrappedNative so that we keep the same set of interfaces.
    if (!wn->GetClassInfo())
        return DoubleWrap(cx, obj, flags);

    // We know that DOM objects only allow one object, we can return early.
    if (wn->HasProto() && wn->GetProto()->ClassIsDOMObject())
        return DoubleWrap(cx, obj, flags);

    XPCCallContext ccx(JS_CALLER, cx, obj);
    if (NATIVE_HAS_FLAG(&ccx, WantPreCreate)) {
        // We have a precreate hook. This object might enforce that we only
        // ever create JS object for it.
        JSObject *originalScope = scope;
        nsresult rv = wn->GetScriptableInfo()->GetCallback()->
            PreCreate(wn->Native(), cx, scope, &scope);
        NS_ENSURE_SUCCESS(rv, DoubleWrap(cx, obj, flags));

        // If the handed back scope differs from the passed-in scope and is in
        // a separate compartment, then this object is explicitly requesting
        // that we don't create a second JS object for it: create a security
        // wrapper.
        if (originalScope->getCompartment() != scope->getCompartment())
            return DoubleWrap(cx, obj, flags);

        // Note: this penalizes objects that only have one wrapper, but are
        // being accessed across compartments. We would really prefer to
        // replace the above code with a test that says "do you only have one
        // wrapper?"
    }

    // The object we're looking at might allow us to create a new wrapped
    // native in the new scope. Try it and continue wrapping on the
    // possibly-new object.
    JSAutoEnterCompartment ac;
    if (!ac.enter(cx, scope))
        return nsnull;

    jsval v;
    nsresult rv =
        nsXPConnect::FastGetXPConnect()->WrapNativeToJSVal(cx, scope, wn->Native(), nsnull,
                                                           &NS_GET_IID(nsISupports), PR_FALSE,
                                                           &v, nsnull);
    if (NS_SUCCEEDED(rv))
        obj = JSVAL_TO_OBJECT(v);

    return DoubleWrap(cx, obj, flags);
}

JSObject *
WrapperFactory::Rewrap(JSContext *cx, JSObject *obj, JSObject *wrappedProto, JSObject *parent,
                       uintN flags)
{
    NS_ASSERTION(!obj->isWrapper() ||
                 (obj->isWrapper() &&
                  obj->getProxyHandler() == &WaiveXrayWrapperWrapper) ||
                 obj->getClass()->ext.innerObject,
                 "wrapped object passed to rewrap");
    NS_ASSERTION(JS_GET_CLASS(cx, obj) != &XrayUtils::HolderClass, "trying to wrap a holder");

    JSCompartment *origin = obj->getCompartment();
    JSCompartment *target = cx->compartment;
    JSObject *xrayHolder = nsnull;

    JSWrapper *wrapper;
    CompartmentPrivate *targetdata = static_cast<CompartmentPrivate *>(target->data);
    if (AccessCheck::isChrome(target)) {
        if (AccessCheck::isChrome(origin)) {
            wrapper = &JSCrossCompartmentWrapper::singleton;
        } else if (flags & WAIVE_XRAY_WRAPPER_FLAG) {
            // If we waived the X-ray wrapper for this object, wrap it into a
            // special wrapper to transitively maintain the X-ray waiver.
            wrapper = &CrossOriginWrapper::singleton;
        } else {
            // Native objects must be wrapped into an X-ray wrapper.
            if (!obj->getGlobal()->isSystem() &&
                (IS_WN_WRAPPER(obj) || obj->getClass()->ext.innerObject)) {
                typedef XrayWrapper<JSCrossCompartmentWrapper, CrossCompartmentXray> Xray;
                wrapper = &Xray::singleton;
                xrayHolder = Xray::createHolder(cx, obj, parent);
                if (!xrayHolder)
                    return nsnull;
            } else {
                wrapper = &JSCrossCompartmentWrapper::singleton;
            }
        }
    } else if (AccessCheck::isChrome(origin)) {
        wrapper = &FilteringWrapper<JSCrossCompartmentWrapper,
                                    ExposedPropertiesOnly>::singleton;
    } else if (AccessCheck::isSameOrigin(origin, target)) {
        // Same origin we use a transparent wrapper, unless the compartment asks
        // for an Xray or the wrapper needs a SOW.
        if (AccessCheck::needsSystemOnlyWrapper(obj)) {
            wrapper = &FilteringWrapper<JSCrossCompartmentWrapper,
                                        OnlyIfSubjectIsSystem>::singleton;
        } else if (targetdata && targetdata->preferXrays && IS_WN_WRAPPER(obj)) {
            typedef XrayWrapper<JSCrossCompartmentWrapper, CrossCompartmentXray> Xray;
            wrapper = &Xray::singleton;
            xrayHolder = Xray::createHolder(cx, obj, parent);
            if (!xrayHolder)
                return nsnull;
        } else {
            wrapper = &JSCrossCompartmentWrapper::singleton;
        }
    } else {
        NS_ASSERTION(!AccessCheck::needsSystemOnlyWrapper(obj),
                     "bad object exposed across origins");

        // Cross origin we want to disallow scripting and limit access to
        // a predefined set of properties. XrayWrapper adds a property
        // (.wrappedJSObject) which allows bypassing the XrayWrapper, but
        // we filter out access to that property.
        if (!IS_WN_WRAPPER(obj) && !obj->getClass()->ext.innerObject) {
            wrapper = &FilteringWrapper<JSCrossCompartmentWrapper,
                                        CrossOriginAccessiblePropertiesOnly>::singleton;
        } else {
            typedef XrayWrapper<JSCrossCompartmentWrapper, CrossCompartmentXray> Xray;

            // Location objects can become same origin after navigation, so we might
            // have to grant transparent access later on.
            if (IsLocationObject(obj)) {
                wrapper = &FilteringWrapper<Xray,
                    SameOriginOrCrossOriginAccessiblePropertiesOnly>::singleton;
            } else {
                wrapper= &FilteringWrapper<Xray,
                    CrossOriginAccessiblePropertiesOnly>::singleton;
            }

            xrayHolder = Xray::createHolder(cx, obj, parent);
            if (!xrayHolder)
                return nsnull;
        }
    }

    JSObject *wrapperObj = JSWrapper::New(cx, obj, wrappedProto, parent, wrapper);
    if (!wrapperObj || !xrayHolder)
        return wrapperObj;

    // NB: The fact that the only wrappers to use ProxyExtra are XrayWrappers
    // is relied on by XPCNativeWrapper.unwrap.
    wrapperObj->setProxyExtra(js::ObjectValue(*xrayHolder));
    return wrapperObj;
}

typedef FilteringWrapper<XrayWrapper<JSWrapper, SameCompartmentXray>,
                         SameOriginOrCrossOriginAccessiblePropertiesOnly> LW;

bool
WrapperFactory::IsLocationObject(JSObject *obj)
{
    const char *name = obj->getClass()->name;
    return name[0] == 'L' && !strcmp(name, "Location");
}

JSObject *
WrapperFactory::WrapLocationObject(JSContext *cx, JSObject *obj)
{
    JSObject *xrayHolder = LW::createHolder(cx, obj, obj->getParent());
    if (!xrayHolder)
        return NULL;
    JSObject *wrapperObj = JSWrapper::New(cx, obj, obj->getProto(), NULL, &LW::singleton);
    if (!wrapperObj)
        return NULL;
    wrapperObj->setProxyExtra(js::ObjectValue(*xrayHolder));
    return wrapperObj;
}

bool
WrapperFactory::WaiveXrayAndWrap(JSContext *cx, jsval *vp)
{
    if (JSVAL_IS_PRIMITIVE(*vp))
        return JS_WrapValue(cx, vp);

    JSObject *obj = JSVAL_TO_OBJECT(*vp)->unwrap();

    // We have to make sure that if we're wrapping an outer window, that
    // the .wrappedJSObject also wraps the outer window.
    obj = GetCurrentOuter(cx, obj);

    {
        js::SwitchToCompartment sc(cx, obj->compartment());
        obj = JSWrapper::New(cx, obj, NULL, obj->getParent(), &WaiveXrayWrapperWrapper);
        if (!obj)
            return false;
    }

    *vp = OBJECT_TO_JSVAL(obj);
    return JS_WrapValue(cx, vp);
}

JSObject *
WrapperFactory::WrapSOWObject(JSContext *cx, JSObject *obj)
{
    JSObject *wrapperObj =
        JSWrapper::New(cx, obj, obj->getProto(), NULL,
                       &FilteringWrapper<JSWrapper,
                                         OnlyIfSubjectIsSystem>::singleton);
    return wrapperObj;
}

}
