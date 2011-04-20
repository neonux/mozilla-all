/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99 ft=cpp:
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
 * The Original Code is SpiderMonkey Debug object.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributors:
 *   Jim Blandy <jimb@mozilla.com>
 *   Jason Orendorff <jorendorff@mozilla.com>
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

#include "jsdbg.h"
#include "jsapi.h"
#include "jscntxt.h"
#include "jsobj.h"
#include "jswrapper.h"
#include "jsobjinlines.h"

using namespace js;

static bool
NotImplemented(JSContext *cx)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NEED_DIET, "API");
    return false;
}

bool
ReportMoreArgsNeeded(JSContext *cx, const char *name, uintN required)
{
    JS_ASSERT(required < 10);
    char s[2];
    s[0] = '0' + required;
    s[1] = '\0';
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_MORE_ARGS_NEEDED,
                         name, s, required == 1 ? "" : "s");
    return false;
}

#define REQUIRE_ARGC(name, n) \
    JS_BEGIN_MACRO \
        if (argc < n) \
            return ReportMoreArgsNeeded(cx, name, n); \
    JS_END_MACRO

bool
ReportObjectRequired(JSContext *cx)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NOT_NONNULL_OBJECT);
    return false;
}

JSObject *
CheckThisClass(JSContext *cx, Value *vp, Class *clasp, const char *fnname)
{
    if (!vp[1].isObject()) {
        ReportObjectRequired(cx);
        return NULL;
    }
    JSObject *thisobj = &vp[1].toObject();
    if (thisobj->getClass() != clasp) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_INCOMPATIBLE_PROTO,
                             clasp->name, fnname, thisobj->getClass()->name);
        return NULL;
    }

    // Check for e.g. Debug.prototype, which is of the Debug JSClass but isn't
    // really a Debug object.
    if ((clasp->flags & JSCLASS_HAS_PRIVATE) && !thisobj->getPrivate()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_INCOMPATIBLE_PROTO,
                             clasp->name, fnname, "prototype object");
        return NULL;
    }
    return thisobj;
}

#define THISOBJ(cx, vp, classname, fnname, thisobj, private)                 \
    JSObject *thisobj =                                                      \
        CheckThisClass(cx, vp, &js::classname::jsclass, fnname);             \
    if (!thisobj)                                                            \
        return false;                                                        \
    js::classname *private = (classname *) thisobj->getPrivate();

// === Debug hook dispatch

Debug::Debug(JSObject *dbg, JSObject *hooks, JSCompartment *compartment)
  : object(dbg), debuggeeCompartment(compartment), hooksObject(hooks),
    uncaughtExceptionHook(NULL), enabled(true), hasDebuggerHandler(false)
{
}

JSTrapStatus
Debug::handleUncaughtException(AutoCompartment &ac, Value *vp, bool callHook)
{
    JSContext *cx = ac.context;
    if (cx->isExceptionPending()) {
        if (callHook && uncaughtExceptionHook) {
            Value fval = ObjectValue(*uncaughtExceptionHook);
            Value exc = cx->getPendingException();
            Value rv;
            cx->clearPendingException();
            if (ExternalInvoke(cx, ObjectValue(*object), fval, 1, &exc, &rv))
                return parseResumptionValue(ac, true, rv, vp, false);
        }
 
        if (cx->isExceptionPending()) {
            JS_ReportPendingException(cx);
            cx->clearPendingException();
        }
    }
    return JSTRAP_ERROR;
}

JSTrapStatus
Debug::parseResumptionValue(AutoCompartment &ac, bool ok, const Value &rv, Value *vp,
                            bool callHook)
{
    vp->setUndefined();
    if (!ok)
        return handleUncaughtException(ac, vp, callHook);
    if (rv.isUndefined())
        return JSTRAP_CONTINUE;
    if (rv.isNull())
        return JSTRAP_ERROR;

    // Check that rv is {return: val} or {throw: val}.
    JSContext *cx = ac.context;
    JSObject *obj;
    const Shape *shape;
    jsid returnId = ATOM_TO_JSID(cx->runtime->atomState.returnAtom);
    jsid throwId = ATOM_TO_JSID(cx->runtime->atomState.throwAtom);
    if (!rv.isObject() ||
        !(obj = &rv.toObject())->isObject() ||
        !(shape = obj->lastProperty())->previous() ||
        shape->previous()->previous() ||
        (shape->id != returnId && shape->id != throwId) ||
        !shape->isDataDescriptor())
    {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_DEBUG_BAD_RESUMPTION);
        return handleUncaughtException(ac, vp, callHook);
    }

    if (!js_NativeGet(cx, obj, obj, shape, 0, vp))
        return handleUncaughtException(ac, vp, callHook);

    // Throwing or returning objects is not yet supported. It requires
    // unwrapping.
    if (vp->isObject()) {
        vp->setUndefined();
        NotImplemented(cx);
        return handleUncaughtException(ac, vp, callHook);
    }

    ac.leave();
    if (!ac.origin->wrap(cx, vp)) {
        // Swallow this exception rather than report it in the debuggee's
        // compartment.  But return JSTRAP_ERROR to terminate the debuggee.
        vp->setUndefined();
        cx->clearPendingException();
        return JSTRAP_ERROR;
    }
    return shape->id == returnId ? JSTRAP_RETURN : JSTRAP_THROW;
}

bool
CallMethodIfPresent(JSContext *cx, JSObject *obj, const char *name, int argc, Value *argv,
                    Value *rval)
{
    rval->setUndefined();
    JSAtom *atom = js_Atomize(cx, name, strlen(name), 0);
    Value fval;
    return atom &&
           js_GetMethod(cx, obj, ATOM_TO_JSID(atom), JSGET_NO_METHOD_BARRIER, &fval) &&
           (!js_IsCallable(fval) ||
            ExternalInvoke(cx, ObjectValue(*obj), fval, argc, argv, rval));
}

JSTrapStatus
Debug::onDebuggerStatement(JSContext *cx, Value *vp)
{
    if (!hasDebuggerHandler)
        return JSTRAP_CONTINUE;

    AutoCompartment ac(cx, hooksObject);
    if (!ac.enter())
        return JSTRAP_ERROR;

    // XXX debuggerHandler should receive a Frame.
    Value rv;
    bool ok = CallMethodIfPresent(cx, hooksObject, "debuggerHandler", 0, NULL, &rv);
    return parseResumptionValue(ac, ok, rv, vp);
}

JSTrapStatus
JSCompartment::dispatchDebuggerStatement(JSContext *cx, js::Value *vp)
{
    // Determine which debuggers will receive this event, and in what order.
    // Make a copy of the list, since the original is mutable and we will be
    // calling into arbitrary JS.
    // Note: In the general case, 'triggered' contains references to objects in
    // different compartments--every compartment *except* this one.
    AutoValueVector triggered(cx);
    for (Debug **p = debuggers.begin(); p != debuggers.end(); p++) {
        Debug *dbg = *p;
        if (dbg->observesDebuggerStatement()) {
            if (!triggered.append(ObjectValue(*dbg->toJSObject())))
                return JSTRAP_ERROR;
        }
    }

    // Deliver the event to each debugger, checking again to make sure it
    // should still be delivered.
    for (Value *p = triggered.begin(); p != triggered.end(); p++) {
        Debug *dbg = Debug::fromJSObject(&p->toObject());
        if (dbg->observesCompartment(this) && dbg->observesDebuggerStatement()) {
            JSTrapStatus st = dbg->onDebuggerStatement(cx, vp);
            if (st != JSTRAP_CONTINUE)
                return st;
        }
    }
    return JSTRAP_CONTINUE;
}


// === Debug JSObjects

bool
Debug::mark(GCMarker *trc, JSCompartment *comp, JSGCInvocationKind gckind)
{
    // Search for Debug objects in the given compartment. We do this by
    // searching all the compartments being debugged.
    bool markedAny = false;
    JSRuntime *rt = trc->context->runtime;
    for (JSCompartment **c = rt->compartments.begin(); c != rt->compartments.end(); c++) {
        JSCompartment *dc = *c;

        // If comp is non-null, this is a per-compartment GC and we
        // search every dc, except for comp (since no compartment can
        // debug itself). If comp is null, this is a global GC and we
        // search every dc that is live.
        if (comp ? dc != comp : !dc->isAboutToBeCollected(gckind)) {
            const JSCompartment::DebugVector &debuggers = dc->getDebuggers();
            for (Debug **p = debuggers.begin(); p != debuggers.end(); p++) {
                Debug *dbg = *p;
                JSObject *obj = dbg->toJSObject();

                // We only need to examine obj if it's in a compartment
                // being GC'd and it isn't already marked.
                if ((!comp || obj->compartment() == comp) &&
                    !obj->isMarked() &&
                    dbg->hasAnyLiveHooks())
                {
                    // obj is reachable only via its live, enabled debugger
                    // hooks, which may yet be called.
                    MarkObject(trc, *obj, "enabled Debug");
                    markedAny = true;
                }
            }
        }
    }
    return markedAny;
}

void
Debug::trace(JSTracer *trc, JSObject *obj)
{
    if (Debug *dbg = (Debug *) obj->getPrivate()) {
        MarkObject(trc, *dbg->hooksObject, "hooks");
        if (dbg->uncaughtExceptionHook)
            MarkObject(trc, *dbg->uncaughtExceptionHook, "hooks");
    }
}

void
JSCompartment::removeDebug(Debug *dbg)
{
    for (Debug **p = debuggers.begin(); p != debuggers.end(); p++) {
        if (*p == dbg) {
            debuggers.erase(p);
            return;
        }
    }
    JS_NOT_REACHED("JSCompartment::removeDebug");
}

void
Debug::finalize(JSContext *cx, JSObject *obj)
{
    Debug *dbg = (Debug *) obj->getPrivate();
    if (dbg && dbg->debuggeeCompartment) {
        dbg->debuggeeCompartment->removeDebug(dbg);
        dbg->debuggeeCompartment = NULL;
    }
}

Class Debug::jsclass = {
    "Debug", JSCLASS_HAS_PRIVATE,
    PropertyStub, PropertyStub, PropertyStub, StrictPropertyStub,
    EnumerateStub, ResolveStub, ConvertStub, Debug::finalize,
    NULL,                 /* reserved0   */
    NULL,                 /* checkAccess */
    NULL,                 /* call        */
    NULL,                 /* construct   */
    NULL,                 /* xdrObject   */
    NULL,                 /* hasInstance */
    Debug::trace
};

JSBool
Debug::getHooks(JSContext *cx, uintN argc, Value *vp)
{
    THISOBJ(cx, vp, Debug, "get hooks", thisobj, dbg);
    vp->setObject(*dbg->hooksObject);
    return true;
}

JSBool
Debug::setHooks(JSContext *cx, uintN argc, Value *vp)
{
    REQUIRE_ARGC("Debug.set hooks", 1);
    THISOBJ(cx, vp, Debug, "set hooks", thisobj, dbg);
    if (!vp[2].isObject())
        return ReportObjectRequired(cx);
    JSObject *hooksobj = &vp[2].toObject();

    JSBool found;
    if (!JS_HasProperty(cx, hooksobj, "debuggerHandler", &found))
        return false;
    dbg->hasDebuggerHandler = !!found;

    dbg->hooksObject = hooksobj;
    vp->setUndefined();
    return true;
}

JSBool
Debug::getEnabled(JSContext *cx, uintN argc, Value *vp)
{
    THISOBJ(cx, vp, Debug, "get enabled", thisobj, dbg);
    vp->setBoolean(dbg->enabled);
    return true;
}

JSBool
Debug::setEnabled(JSContext *cx, uintN argc, Value *vp)
{
    REQUIRE_ARGC("Debug.set enabled", 1);
    THISOBJ(cx, vp, Debug, "set enabled", thisobj, dbg);
    dbg->enabled = js_ValueToBoolean(vp[2]);
    vp->setUndefined();
    return true;
}

JSBool
Debug::getUncaughtExceptionHook(JSContext *cx, uintN argc, Value *vp)
{
    THISOBJ(cx, vp, Debug, "get uncaughtExceptionHook", thisobj, dbg);
    vp->setObjectOrNull(dbg->uncaughtExceptionHook);
    return true;
}

JSBool
Debug::setUncaughtExceptionHook(JSContext *cx, uintN argc, Value *vp)
{
    REQUIRE_ARGC("Debug.set uncaughtExceptionHook", 1);
    THISOBJ(cx, vp, Debug, "set uncaughtExceptionHook", thisobj, dbg);
    if (!vp[2].isNull() && (!vp[2].isObject() || !vp[2].toObject().isCallable())) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_ASSIGN_FUNCTION_OR_NULL,
                             "uncaughtExceptionHook");
        return false;
    }

    dbg->uncaughtExceptionHook = vp[2].toObjectOrNull();
    vp->setUndefined();
    return true;
}

JSBool
Debug::construct(JSContext *cx, uintN argc, Value *vp)
{
    REQUIRE_ARGC("Debug", 1);

    // Confirm that the first argument is a cross-compartment wrapper.
    const Value &arg = vp[2];
    if (!arg.isObject())
        return ReportObjectRequired(cx);
    JSObject *argobj = &arg.toObject();
    if (!argobj->isWrapper() ||
        (!argobj->getWrapperHandler()->flags() & JSWrapper::CROSS_COMPARTMENT))
    {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CCW_REQUIRED, "Debug");
        return false;
    }

    // Get Debug.prototype.
    Value v;
    jsid prototypeId = ATOM_TO_JSID(cx->runtime->atomState.classPrototypeAtom);
    if (!vp[0].toObject().getProperty(cx, prototypeId, &v))
        return false;
    JSObject *proto = &v.toObject();
    JS_ASSERT(proto->getClass() == &Debug::jsclass);

    // Make the new Debug object.
    JSObject *obj = NewNonFunction<WithProto::Given>(cx, &Debug::jsclass, proto, NULL);
    if (!obj)
        return false;
    JSObject *hooks = NewBuiltinClassInstance(cx, &js_ObjectClass);
    if (!hooks)
        return false;
    JSCompartment *debuggeeCompartment = argobj->getProxyPrivate().toObject().compartment();
    Debug *dbg = cx->new_<Debug>(obj, hooks, debuggeeCompartment);
    if (!dbg)
        return false;
    if (!debuggeeCompartment->addDebug(dbg)) {
        js_ReportOutOfMemory(cx);
        return false;
    }
    obj->setPrivate(dbg);

    vp->setObject(*obj);
    return true;
}

JSPropertySpec Debug::properties[] = {
    JS_PSGS("hooks", Debug::getHooks, Debug::setHooks, 0),
    JS_PSGS("enabled", Debug::getEnabled, Debug::setEnabled, 0),
    JS_PSGS("uncaughtExceptionHook", Debug::getUncaughtExceptionHook,
            Debug::setUncaughtExceptionHook, 0),
    JS_PS_END
};
    

extern JS_PUBLIC_API(JSBool)
JS_DefineDebugObject(JSContext *cx, JSObject *obj)
{
    JSObject *objProto;
    if (!js_GetClassPrototype(cx, obj, JSProto_Object, &objProto))
        return NULL;

    return !!js_InitClass(cx, obj, objProto, &Debug::jsclass, Debug::construct, 1,
                          Debug::properties, NULL, NULL, NULL);
}
