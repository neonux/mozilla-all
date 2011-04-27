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

#ifndef jsdbg_h__
#define jsdbg_h__

#include "jsapi.h"
#include "jscompartment.h"
#include "jsgc.h"
#include "jswrapper.h"
#include "jsvalue.h"

namespace js {

class Debug {
    friend JSBool ::JS_DefineDebugObject(JSContext *cx, JSObject *obj);

  private:
    JSObject *object;  // The Debug object. Strong reference.
    JSCompartment *debuggeeCompartment;  // Weak reference.
    JSObject *hooksObject;  // See Debug.prototype.hooks. Strong reference.
    JSObject *uncaughtExceptionHook;  // Strong reference.
    bool enabled;

    // True if hooksObject had a debuggerHandler property when the hooks
    // property was set.
    bool hasDebuggerHandler;

    JSTrapStatus handleUncaughtException(AutoCompartment &ac, Value *vp, bool callHook);
    JSTrapStatus parseResumptionValue(AutoCompartment &ac, bool ok, const Value &rv, Value *vp,
                                      bool callHook = true);

    static void trace(JSTracer *trc, JSObject *obj);
    static void finalize(JSContext *cx, JSObject *obj);

    static Class jsclass;
    static JSBool getHooks(JSContext *cx, uintN argc, Value *vp);
    static JSBool setHooks(JSContext *cx, uintN argc, Value *vp);
    static JSBool getEnabled(JSContext *cx, uintN argc, Value *vp);
    static JSBool setEnabled(JSContext *cx, uintN argc, Value *vp);
    static JSBool getUncaughtExceptionHook(JSContext *cx, uintN argc, Value *vp);
    static JSBool setUncaughtExceptionHook(JSContext *cx, uintN argc, Value *vp);
    static JSBool construct(JSContext *cx, uintN argc, Value *vp);
    static JSPropertySpec properties[];

    inline bool hasAnyLiveHooks() const;

    inline bool observesDebuggerStatement() const;
    static JSTrapStatus dispatchDebuggerStatement(JSContext *cx, Value *vp);
    JSTrapStatus handleDebuggerStatement(JSContext *cx, Value *vp);

  public:
    Debug(JSObject *dbg, JSObject *hooks, JSCompartment *compartment);

    // Mark some Debug objects. A Debug object is live if:
    //   * the Debug JSObject is live (Debug::trace handles this case); OR
    //   * it is in the middle of dispatching an event (the event dispatching
    //     code roots it in this case); OR
    //   * it is enabled, and it is debugging at least one live compartment,
    //     and at least one of the following is true:
    //       - it has a debugger hook installed
    //       - it has a breakpoint set on a live script
    //       - it has a watchpoint set on a live object.
    //
    // The last case is handled by this method. If it finds any Debug objects
    // that are definitely live but not yet marked, it marks them and returns
    // true. If not, it returns false.
    //
    static bool mark(GCMarker *trc, JSCompartment *compartment, JSGCInvocationKind gckind);

    inline JSObject *toJSObject() const;
    static inline Debug *fromJSObject(JSObject *obj);

    inline bool observesCompartment(JSCompartment *c) const;
    void detachFrom(JSCompartment *c);

    static inline JSTrapStatus onDebuggerStatement(JSContext *cx, js::Value *vp);
};

bool
Debug::hasAnyLiveHooks() const
{
    return observesDebuggerStatement();
}

bool
Debug::observesCompartment(JSCompartment *c) const
{
    JS_ASSERT(c);
    return debuggeeCompartment == c;
}

JSObject *
Debug::toJSObject() const
{
    JS_ASSERT(object);
    return object;
}

Debug *
Debug::fromJSObject(JSObject *obj)
{
    JS_ASSERT(obj->getClass() == &jsclass);
    return (Debug *) obj->getPrivate();
}

bool
Debug::observesDebuggerStatement() const
{
    return enabled && hasDebuggerHandler;
}

JSTrapStatus
Debug::onDebuggerStatement(JSContext *cx, js::Value *vp)
{
    return cx->compartment->getDebuggers().empty()
           ? JSTRAP_CONTINUE
           : dispatchDebuggerStatement(cx, vp);
}

}

#endif /* jsdbg_h__ */
