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
 * The Original Code is SpiderMonkey Debugger object.
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
#include "jsclist.h"
#include "jscntxt.h"
#include "jscompartment.h"
#include "jsgc.h"
#include "jshashtable.h"
#include "jsweakmap.h"
#include "jswrapper.h"
#include "jsvalue.h"
#include "vm/GlobalObject.h"

namespace js {

class Debugger {
    friend class js::Breakpoint;
    friend JSBool (::JS_DefineDebuggerObject)(JSContext *cx, JSObject *obj);

  public:
    enum NewScriptKind { NewNonHeldScript, NewHeldScript };

  private:
    JSCList link;                       // See JSRuntime::debuggerList.
    JSObject *object;                   // The Debugger object. Strong reference.
    GlobalObjectSet debuggees;          // Debuggee globals. Cross-compartment weak references.
    JSObject *hooksObject;              // See Debugger.prototype.hooks. Strong reference.
    JSObject *uncaughtExceptionHook;    // Strong reference.
    bool enabled;

    // True if hooksObject had a property of the respective name when the hooks
    // property was set.
    bool hasDebuggerHandler;            // hooks.debuggerHandler
    bool hasThrowHandler;               // hooks.throw
    bool hasNewScriptHandler;           // hooks.newScript
    bool hasEnterFrameHandler;          // hooks.enterFrame

    JSCList breakpoints;                // cyclic list of all js::Breakpoints in this debugger

    // Weak references to stack frames that are currently on the stack and thus
    // necessarily alive. We drop them as soon as they leave the stack (see
    // slowPathOnLeaveFrame) and in removeDebuggee.
    typedef HashMap<StackFrame *, JSObject *, DefaultHasher<StackFrame *>, RuntimeAllocPolicy>
        FrameMap;
    FrameMap frames;

    // The map from debuggee objects to their Debugger.Object instances.
    typedef WeakMap<JSObject *, JSObject *, DefaultHasher<JSObject *>, CrossCompartmentMarkPolicy>
        ObjectWeakMap;
    ObjectWeakMap objects;

    // An ephemeral map from script-holding objects to Debugger.Script
    // instances.
    typedef WeakMap<JSObject *, JSObject *, DefaultHasher<JSObject *>, CrossCompartmentMarkPolicy>
        ScriptWeakMap;

    // Map of Debugger.Script instances for garbage-collected JSScripts. For
    // function scripts, the key is the compiler-created, internal JSFunction;
    // for scripts returned by JSAPI functions, the key is the "Script"-class
    // JSObject.
    ScriptWeakMap heldScripts;

    // An ordinary (non-ephemeral) map from JSScripts to Debugger.Script
    // instances, for non-held scripts that are explicitly freed.
    typedef HashMap<JSScript *, JSObject *, DefaultHasher<JSScript *>, RuntimeAllocPolicy>
        ScriptMap;

    // Map from non-held JSScripts to their Debugger.Script objects. Non-held
    // scripts are scripts created for eval or JS_Evaluate* calls that are
    // explicitly destroyed when the call returns. Debugger.Script objects are
    // not strong references to such JSScripts; the Debugger.Script becomes
    // "dead" when the eval call returns.
    ScriptMap nonHeldScripts;

    bool addDebuggeeGlobal(JSContext *cx, GlobalObject *obj);
    void removeDebuggeeGlobal(JSContext *cx, GlobalObject *global,
                              GlobalObjectSet::Enum *compartmentEnum,
                              GlobalObjectSet::Enum *debugEnum);

    // Cope with an error or exception in a debugger hook.
    //
    // If callHook is true, then call the uncaughtExceptionHook, if any. If
    // additionally vp is non-null, then parse the value returned by
    // uncaughtExceptionHook as a resumption value.
    //
    // If there is no uncaughtExceptionHook, or if it fails, report and clear
    // the pending exception on ac.context and return JSTRAP_ERROR.
    //
    // This always calls ac.leave(); ac is a parameter because this method must
    // do some things in the debugger compartment and some things in the
    // debuggee compartment.
    //
    JSTrapStatus handleUncaughtException(AutoCompartment &ac, Value *vp, bool callHook);
    JSTrapStatus parseResumptionValue(AutoCompartment &ac, bool ok, const Value &rv, Value *vp,
                                      bool callHook = true);
    JSObject *unwrapDebuggeeArgument(JSContext *cx, Value *vp);

    static void traceObject(JSTracer *trc, JSObject *obj);
    void trace(JSTracer *trc);
    static void finalize(JSContext *cx, JSObject *obj);
    static void markKeysInCompartment(JSTracer *tracer, ObjectWeakMap &map);

    static Class jsclass;
    static JSBool getHooks(JSContext *cx, uintN argc, Value *vp);
    static JSBool setHooks(JSContext *cx, uintN argc, Value *vp);
    static JSBool getEnabled(JSContext *cx, uintN argc, Value *vp);
    static JSBool setEnabled(JSContext *cx, uintN argc, Value *vp);
    static JSBool getUncaughtExceptionHook(JSContext *cx, uintN argc, Value *vp);
    static JSBool setUncaughtExceptionHook(JSContext *cx, uintN argc, Value *vp);
    static JSBool addDebuggee(JSContext *cx, uintN argc, Value *vp);
    static JSBool removeDebuggee(JSContext *cx, uintN argc, Value *vp);
    static JSBool hasDebuggee(JSContext *cx, uintN argc, Value *vp);
    static JSBool getDebuggees(JSContext *cx, uintN argc, Value *vp);
    static JSBool getNewestFrame(JSContext *cx, uintN argc, Value *vp);
    static JSBool clearAllBreakpoints(JSContext *cx, uintN argc, Value *vp);
    static JSBool construct(JSContext *cx, uintN argc, Value *vp);
    static JSPropertySpec properties[];
    static JSFunctionSpec methods[];

    inline bool hasAnyLiveHooks() const;

    static void slowPathOnEnterFrame(JSContext *cx);
    static void slowPathOnLeaveFrame(JSContext *cx);
    static void slowPathOnNewScript(JSContext *cx, JSScript *script, JSObject *obj,
                                    NewScriptKind kind);
    static void slowPathOnDestroyScript(JSScript *script);

    typedef bool (Debugger::*DebuggerObservesMethod)() const;
    typedef JSTrapStatus (Debugger::*DebuggerHandleMethod)(JSContext *, Value *) const;
    static JSTrapStatus dispatchHook(JSContext *cx, js::Value *vp,
                                     DebuggerObservesMethod observesEvent,
                                     DebuggerHandleMethod handleEvent);

    bool observesDebuggerStatement() const;
    JSTrapStatus handleDebuggerStatement(JSContext *cx, Value *vp);

    bool observesThrow() const;
    JSTrapStatus handleThrow(JSContext *cx, Value *vp);

    void handleEnterFrame(JSContext *cx);

    // Allocate and initialize a Debugger.Script instance whose referent is |script| and
    // whose holder is |obj|. If |obj| is NULL, this creates a Debugger.Script whose holder
    // is null, for non-held scripts.
    JSObject *newDebuggerScript(JSContext *cx, JSScript *script, JSObject *obj);

    // Helper function for wrapFunctionScript and wrapJSAPIscript.
    JSObject *wrapHeldScript(JSContext *cx, JSScript *script, JSObject *obj);

    // Receive a "new script" event from the engine. A new script was compiled
    // or deserialized. If kind is NewHeldScript, obj is the holder
    // object. Otherwise kind is NewNonHeldScript and obj is an arbitrary
    // object in the same global as the scope in which the script is being
    // evaluated.
    void handleNewScript(JSContext *cx, JSScript *script, JSObject *obj, NewScriptKind kind);

    // Remove script from our table of non-held scripts.
    void destroyNonHeldScript(JSScript *script);

    static inline Debugger *fromLinks(JSCList *links);
    inline Breakpoint *firstBreakpoint() const;

  public:
    Debugger(JSContext *cx, JSObject *dbg, JSObject *hooks);
    ~Debugger();

    bool init(JSContext *cx);
    inline JSObject *toJSObject() const;
    static inline Debugger *fromJSObject(JSObject *obj);
    static Debugger *fromChildJSObject(JSObject *obj);

    /*********************************** Methods for interaction with the GC. */

    // A Debugger object is live if:
    //   * the Debugger JSObject is live (Debugger::trace handles this case); OR
    //   * it is in the middle of dispatching an event (the event dispatching
    //     code roots it in this case); OR
    //   * it is enabled, and it is debugging at least one live compartment,
    //     and at least one of the following is true:
    //       - it has a debugger hook installed
    //       - it has a breakpoint set on a live script
    //       - it has a watchpoint set on a live object.
    //
    // The last case is handled by the mark() method. If it finds any Debugger
    // objects that are definitely live but not yet marked, it marks them and
    // returns true. If not, it returns false.
    //
    static void markCrossCompartmentDebuggerObjectReferents(JSTracer *tracer);
    static bool mark(GCMarker *trc, JSGCInvocationKind gckind);
    static void sweepAll(JSContext *cx);
    static void detachAllDebuggersFromGlobal(JSContext *cx, GlobalObject *global,
                                             GlobalObjectSet::Enum *compartmentEnum);

    static inline void onEnterFrame(JSContext *cx);
    static inline void onLeaveFrame(JSContext *cx);
    static inline JSTrapStatus onDebuggerStatement(JSContext *cx, js::Value *vp);
    static inline JSTrapStatus onThrow(JSContext *cx, js::Value *vp);
    static inline void onNewScript(JSContext *cx, JSScript *script, JSObject *obj,
                                   NewScriptKind kind);
    static inline void onDestroyScript(JSScript *script);
    static JSTrapStatus onTrap(JSContext *cx, Value *vp);

    /**************************************** Functions for use by jsdbg.cpp. */

    inline bool observesEnterFrame() const;
    inline bool observesNewScript() const;
    inline bool observesScope(JSObject *obj) const;
    inline bool observesFrame(StackFrame *fp) const;

    // Precondition: *vp is a value from a debuggee compartment and cx is in
    // the debugger's compartment.
    //
    // Wrap *vp for the debugger compartment, wrap it in a Debugger.Object if it's
    // an object, store the result in *vp, and return true.
    //
    bool wrapDebuggeeValue(JSContext *cx, Value *vp);

    // NOT the inverse of wrapDebuggeeValue.
    //
    // Precondition: cx is in the debugger compartment. *vp is a value in that
    // compartment. (*vp is a "debuggee value", meaning it is the debugger's
    // reflection of a value in the debuggee.)
    //
    // If *vp is a Debugger.Object, store the referent in *vp. Otherwise, if *vp
    // is an object, throw a TypeError, because it is not a debuggee
    // value. Otherwise *vp is a primitive, so leave it alone.
    //
    // The value is not rewrapped for any debuggee compartment.
    //
    bool unwrapDebuggeeValue(JSContext *cx, Value *vp);

    // Store the Debugger.Frame object for the frame fp in *vp.
    bool getScriptFrame(JSContext *cx, StackFrame *fp, Value *vp);

    // Precondition: we are in the debuggee compartment (ac is entered) and ok
    // is true if the operation in the debuggee compartment succeeded, false on
    // error or exception.
    //
    // Postcondition: we are in the debugger compartment (ac is not entered)
    // whether creating the new completion value succeeded or not.
    //
    // On success, a completion value is in vp and ac.context does not have a
    // pending exception. (This ordinarily returns true even if the ok argument
    // is false.)
    //
    bool newCompletionValue(AutoCompartment &ac, bool ok, Value val, Value *vp);

    // Return the Debugger.Script object for |fun|'s script, or create a new
    // one if needed.  The context |cx| must be in the debugger compartment;
    // |fun| must be a cross-compartment wrapper referring to the JSFunction in
    // a debuggee compartment.
    JSObject *wrapFunctionScript(JSContext *cx, JSFunction *fun);

    // Return the Debugger.Script object for the Script object |obj|'s
    // JSScript, or create a new one if needed. The context |cx| must be in the
    // debugger compartment; |obj| must be a cross-compartment wrapper
    // referring to a script object in a debuggee compartment.
    JSObject *wrapJSAPIScript(JSContext *cx, JSObject *scriptObj);

    // Return the Debugger.Script object for the non-held script |script|, or
    // create a new one if needed. The context |cx| must be in the debugger
    // compartment; |script| must be a script in a debuggee compartment.
    JSObject *wrapNonHeldScript(JSContext *cx, JSScript *script);

  private:
    // Prohibit copying.
    Debugger(const Debugger &);
    Debugger & operator=(const Debugger &);
};

class BreakpointSite {
    friend class js::Breakpoint;
    friend class ::JSCompartment;
    friend class js::Debugger;

  public:
    JSScript * const script;
    jsbytecode * const pc;
    const JSOp realOpcode;

  private:
    // The holder object for script, if known, else NULL.  This is NULL for
    // non-held scripts and for JSD1 traps. It is always non-null for JSD2
    // breakpoints in held scripts.
    JSObject *scriptObject;

    JSCList breakpoints;  // cyclic list of all js::Breakpoints at this instruction
    size_t enabledCount;  // number of breakpoints in the list that are enabled
    JSTrapHandler trapHandler;  // jsdbgapi trap state
    Value trapClosure;

    bool recompile(JSContext *cx, bool forTrap);

  public:
    BreakpointSite(JSScript *script, jsbytecode *pc);
    Breakpoint *firstBreakpoint() const;
    bool hasBreakpoint(Breakpoint *bp);

    bool inc(JSContext *cx);
    void dec(JSContext *cx);
    bool setTrap(JSContext *cx, JSTrapHandler handler, const Value &closure);
    void clearTrap(JSContext *cx, BreakpointSiteMap::Enum *e = NULL,
                   JSTrapHandler *handlerp = NULL, Value *closurep = NULL);
    void destroyIfEmpty(JSRuntime *rt, BreakpointSiteMap::Enum *e);
};

class Breakpoint {
    friend class ::JSCompartment;
    friend class js::Debugger;

  public:
    Debugger * const debugger;
    BreakpointSite * const site;
  private:
    JSObject *handler;
    JSCList debuggerLinks;
    JSCList siteLinks;

  public:
    static Breakpoint *fromDebuggerLinks(JSCList *links);
    static Breakpoint *fromSiteLinks(JSCList *links);
    Breakpoint(Debugger *debugger, BreakpointSite *site, JSObject *handler);
    void destroy(JSContext *cx, BreakpointSiteMap::Enum *e = NULL);
    Breakpoint *nextInDebugger();
    Breakpoint *nextInSite();
    JSObject *getHandler() const { return handler; }
};

bool
Debugger::hasAnyLiveHooks() const
{
    return enabled && (hasDebuggerHandler ||
                       hasThrowHandler ||
                       hasNewScriptHandler ||
                       hasEnterFrameHandler ||
                       !JS_CLIST_IS_EMPTY(&breakpoints));
}

Debugger *
Debugger::fromLinks(JSCList *links)
{
    unsigned char *p = reinterpret_cast<unsigned char *>(links);
    return reinterpret_cast<Debugger *>(p - offsetof(Debugger, link));
}


Breakpoint *
Debugger::firstBreakpoint() const
{
    if (JS_CLIST_IS_EMPTY(&breakpoints))
        return NULL;
    return Breakpoint::fromDebuggerLinks(JS_NEXT_LINK(&breakpoints));
}

JSObject *
Debugger::toJSObject() const
{
    JS_ASSERT(object);
    return object;
}

Debugger *
Debugger::fromJSObject(JSObject *obj)
{
    JS_ASSERT(obj->getClass() == &jsclass);
    return (Debugger *) obj->getPrivate();
}

bool
Debugger::observesEnterFrame() const
{
    return enabled && hasEnterFrameHandler;
}

bool
Debugger::observesNewScript() const
{
    return enabled && hasNewScriptHandler;
}

bool
Debugger::observesScope(JSObject *obj) const
{
    return debuggees.has(obj->getGlobal());
}

bool
Debugger::observesFrame(StackFrame *fp) const
{
    return observesScope(&fp->scopeChain());
}

void
Debugger::onEnterFrame(JSContext *cx)
{
    if (!cx->compartment->getDebuggees().empty())
        slowPathOnEnterFrame(cx);
}

void
Debugger::onLeaveFrame(JSContext *cx)
{
    if (!cx->compartment->getDebuggees().empty() || !cx->compartment->breakpointSites.empty())
        slowPathOnLeaveFrame(cx);
}

JSTrapStatus
Debugger::onDebuggerStatement(JSContext *cx, js::Value *vp)
{
    return cx->compartment->getDebuggees().empty()
           ? JSTRAP_CONTINUE
           : dispatchHook(cx, vp,
                          DebuggerObservesMethod(&Debugger::observesDebuggerStatement),
                          DebuggerHandleMethod(&Debugger::handleDebuggerStatement));
}

JSTrapStatus
Debugger::onThrow(JSContext *cx, js::Value *vp)
{
    return cx->compartment->getDebuggees().empty()
           ? JSTRAP_CONTINUE
           : dispatchHook(cx, vp,
                          DebuggerObservesMethod(&Debugger::observesThrow),
                          DebuggerHandleMethod(&Debugger::handleThrow));
}

void
Debugger::onNewScript(JSContext *cx, JSScript *script, JSObject *obj, NewScriptKind kind)
{
    JS_ASSERT_IF(kind == NewHeldScript || script->compileAndGo, obj);
    if (!script->compartment->getDebuggees().empty())
        slowPathOnNewScript(cx, script, obj, kind);
}

void
Debugger::onDestroyScript(JSScript *script)
{
    if (!script->compartment->getDebuggees().empty())
        slowPathOnDestroyScript(script);
}

extern JSBool
EvaluateInScope(JSContext *cx, JSObject *scobj, StackFrame *fp, const jschar *chars,
                uintN length, const char *filename, uintN lineno, Value *rval);

}

#endif /* jsdbg_h__ */
