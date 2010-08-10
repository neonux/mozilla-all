/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
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
 * The Original Code is Mozilla SpiderMonkey JavaScript 1.9 code, released
 * May 28, 2008.
 *
 * The Initial Developer of the Original Code is
 *   Brendan Eich <brendan@mozilla.org>
 *
 * Contributor(s):
 *   David Anderson <danderson@mozilla.com>
 *   David Mandelin <dmandelin@mozilla.com>
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

#include "jscntxt.h"
#include "jsscope.h"
#include "jsobj.h"
#include "jslibmath.h"
#include "jsiter.h"
#include "jsnum.h"
#include "jsxml.h"
#include "jsstaticcheck.h"
#include "jsbool.h"
#include "assembler/assembler/MacroAssemblerCodeRef.h"
#include "assembler/assembler/CodeLocation.h"
#include "assembler/assembler/RepatchBuffer.h"
#include "jsiter.h"
#include "jstypes.h"
#include "methodjit/StubCalls.h"
#include "jstracer.h"
#include "jspropertycache.h"
#include "methodjit/MonoIC.h"

#include "jsinterpinlines.h"
#include "jspropertycacheinlines.h"
#include "jsscopeinlines.h"
#include "jsscriptinlines.h"
#include "jsstrinlines.h"
#include "jsobjinlines.h"
#include "jscntxtinlines.h"
#include "jsatominlines.h"

#include "jsautooplen.h"

using namespace js;
using namespace js::mjit;
using namespace JSC;

#define THROW()  \
    do {         \
        void *ptr = JS_FUNC_TO_DATA_PTR(void *, JaegerThrowpoline); \
        *f.returnAddressLocation() = ptr; \
        return;  \
    } while (0)

#define THROWV(v)       \
    do {                \
        void *ptr = JS_FUNC_TO_DATA_PTR(void *, JaegerThrowpoline); \
        *f.returnAddressLocation() = ptr; \
        return v;       \
    } while (0)

static bool
InlineReturn(VMFrame &f, JSBool ok);

static jsbytecode *
FindExceptionHandler(JSContext *cx)
{
    JSStackFrame *fp = cx->fp();
    JSScript *script = fp->script();

top:
    if (cx->throwing && script->trynotesOffset) {
        // The PC is updated before every stub call, so we can use it here.
        unsigned offset = cx->regs->pc - script->main;

        JSTryNoteArray *tnarray = script->trynotes();
        for (unsigned i = 0; i < tnarray->length; ++i) {
            JSTryNote *tn = &tnarray->vector[i];
            JS_ASSERT(offset < script->length);
            // The following if condition actually tests two separate conditions:
            //   (1) offset - tn->start >= tn->length
            //       means the PC is not in the range of this try note, so we
            //       should continue searching, after considering:
            //   (2) offset - tn->start == tn->length
            //       means the PC is at the first op of the exception handler
            //       for this try note. This happens when an exception is thrown
            //       during recording: the interpreter sets the PC to the handler
            //       and then exits. In this case, we are in fact at the right
            //       exception handler. 
            //      
            //       Hypothetically, the op we are at might have thrown an
            //       exception, in which case this would not be the right handler.
            //       But the first ops of exception handlers generated by our
            //       bytecode compiler cannot throw, so this is not possible.
            if (offset - tn->start > tn->length)
                continue;
            if (tn->stackDepth > cx->regs->sp - fp->base())
                continue;

            jsbytecode *pc = script->main + tn->start + tn->length;
            JSBool ok = js_UnwindScope(cx, tn->stackDepth, JS_TRUE);
            JS_ASSERT(cx->regs->sp == fp->base() + tn->stackDepth);

            switch (tn->kind) {
                case JSTRY_CATCH:
                  JS_ASSERT(js_GetOpcode(cx, fp->script(), pc) == JSOP_ENTERBLOCK);

#if JS_HAS_GENERATORS
                  /* Catch cannot intercept the closing of a generator. */
                  if (JS_UNLIKELY(cx->exception.isMagic(JS_GENERATOR_CLOSING)))
                      break;
#endif

                  /*
                   * Don't clear cx->throwing to save cx->exception from GC
                   * until it is pushed to the stack via [exception] in the
                   * catch block.
                   */
                  return pc;

                case JSTRY_FINALLY:
                  /*
                   * Push (true, exception) pair for finally to indicate that
                   * [retsub] should rethrow the exception.
                   */
                  cx->regs->sp[0].setBoolean(true);
                  cx->regs->sp[1] = cx->exception;
                  cx->regs->sp += 2;
                  cx->throwing = JS_FALSE;
                  return pc;

                case JSTRY_ITER:
                {
                  /*
                   * This is similar to JSOP_ENDITER in the interpreter loop,
                   * except the code now uses the stack slot normally used by
                   * JSOP_NEXTITER, namely regs.sp[-1] before the regs.sp -= 2
                   * adjustment and regs.sp[1] after, to save and restore the
                   * pending exception.
                   */
                  AutoValueRooter tvr(cx, cx->exception);
                  JS_ASSERT(js_GetOpcode(cx, fp->script(), pc) == JSOP_ENDITER);
                  cx->throwing = JS_FALSE;
                  ok = !!js_CloseIterator(cx, &cx->regs->sp[-1].toObject());
                  cx->regs->sp -= 1;
                  if (!ok)
                      goto top;
                  cx->throwing = JS_TRUE;
                  cx->exception = tvr.value();
                }
            }
        }
    }

    return NULL;
}

static bool
InlineReturn(VMFrame &f, JSBool ok)
{
    JSContext *cx = f.cx;
    JSStackFrame *fp = f.regs.fp;

    JS_ASSERT(f.fp() != f.entryFp);

    JS_ASSERT(!fp->hasBlockChain());
    JS_ASSERT(!js_IsActiveWithOrBlock(cx, &fp->scopeChain(), 0));

    // Marker for debug support.
    if (JS_UNLIKELY(fp->hasHookData())) {
        JSInterpreterHook hook;
        JSBool status;

        hook = cx->debugHooks->callHook;
        if (hook) {
            /*
             * Do not pass &ok directly as exposing the address inhibits
             * optimizations and uninitialised warnings.
             */
            status = ok;
            hook(cx, fp, JS_FALSE, &status, fp->hookData());
            ok = (status == JS_TRUE);
            // CHECK_INTERRUPT_HANDLER();
        }
    }

    PutActivationObjects(cx, fp);

    /* :TODO: version stuff */

    if (fp->isConstructing() && fp->returnValue().isPrimitive())
        fp->setReturnValue(fp->thisValue());

    Value *newsp = fp->actualArgs() - 1;
    newsp[-1] = fp->returnValue();
    cx->stack().popInlineFrame(cx, fp->prev(), newsp);

    return ok;
}

JSBool JS_FASTCALL
stubs::NewObject(VMFrame &f, uint32 argc)
{
    JSContext *cx = f.cx;
    Value *vp = f.regs.sp - (argc + 2);

    JSObject *funobj = &vp[0].toObject();
    JS_ASSERT(funobj->isFunction());

    jsid id = ATOM_TO_JSID(cx->runtime->atomState.classPrototypeAtom);
    if (!funobj->getProperty(cx, id, &vp[1]))
        THROWV(JS_FALSE);

    JSObject *proto = vp[1].isObject() ? &vp[1].toObject() : NULL;
    JSObject *obj = NewNonFunction<WithProto::Class>(cx, &js_ObjectClass, proto, funobj->getParent());
    if (!obj)
        THROWV(JS_FALSE);

    vp[1].setObject(*obj);

    return JS_TRUE;
}

void JS_FASTCALL
stubs::SlowCall(VMFrame &f, uint32 argc)
{
    Value *vp = f.regs.sp - (argc + 2);

    if (!Invoke(f.cx, InvokeArgsAlreadyOnTheStack(vp, argc), 0))
        THROW();
}

void JS_FASTCALL
stubs::SlowNew(VMFrame &f, uint32 argc)
{
    JSContext *cx = f.cx;
    Value *vp = f.regs.sp - (argc + 2);

    if (!InvokeConstructor(cx, InvokeArgsAlreadyOnTheStack(vp, argc)))
        THROW();
}

/*
 * This function must only be called after the early prologue, since it depends
 * on fp->exec.fun.
 */
static inline void
RemovePartialFrame(JSContext *cx, JSStackFrame *fp)
{
    JSStackFrame *prev = fp->prev();
    Value *newsp = (Value *)fp;
    cx->stack().popInlineFrame(cx, prev, newsp);
}

/*
 * HitStackQuota is called after the early prologue pushing the new frame would
 * overflow f.stackLimit.
 */
void JS_FASTCALL
stubs::HitStackQuota(VMFrame &f)
{
    /* Include space to push another frame. */
    uintN nvals = f.fp()->script()->nslots + VALUES_PER_STACK_FRAME;
    JS_ASSERT(f.regs.sp == f.fp()->base());
    if (f.cx->stack().bumpCommitAndLimit(f.entryFp, f.regs.sp, nvals, &f.stackLimit))
        return;

    /* Remove the current partially-constructed frame before throwing. */
    RemovePartialFrame(f.cx, f.fp());
    js_ReportOverRecursed(f.cx);
    THROW();
}

/*
 * This function must only be called after the early prologue, since it depends
 * on fp->exec.fun.
 */
void * JS_FASTCALL
stubs::FixupArity(VMFrame &f, uint32 nactual)
{
    JSContext *cx = f.cx;
    JSStackFrame *oldfp = f.fp();

    JS_ASSERT(nactual != oldfp->numFormalArgs());

    /*
     * Grossssss! *move* the stack frame. If this ends up being perf-critical,
     * we can figure out how to spot-optimize it. Be careful to touch only the
     * members that have been initialized by initCallFrameCallerHalf and the
     * early prologue.
     */
    uint32 flags         = oldfp->isConstructingFlag();
    JSObject &scopeChain = oldfp->scopeChain();
    JSFunction *fun      = oldfp->fun();
    void *ncode          = oldfp->nativeReturnAddress();

    /* Pop the inline frame. */
    RemovePartialFrame(cx, oldfp);

    /* Reserve enough space for a callee frame. */
    JSStackFrame *newfp = cx->stack().getInlineFrameWithinLimit(cx, cx->regs->sp, nactual,
                                                                fun, fun->script(), &flags,
                                                                f.entryFp, &f.stackLimit);
    if (!newfp)
        THROWV(NULL);

    /* Reset the part of the stack frame set by the caller. */
    newfp->initCallFrameCallerHalf(cx, scopeChain, nactual, flags);

    /* Reset the part of the stack frame set by the prologue up to now. */
    newfp->initCallFrameEarlyPrologue(fun, ncode);

    /* The caller takes care of assigning fp to regs. */
    return newfp;
}

void * JS_FASTCALL
stubs::CompileFunction(VMFrame &f, uint32 nactual)
{
    /*
     * We have a partially constructed frame. That's not really good enough to
     * compile though because we could throw, so get a full, adjusted frame.
     */
    JSContext *cx = f.cx;
    JSStackFrame *fp = f.fp();

    /*
     * Since we can only use members set by initCallFrameCallerHalf,
     * we must carefully extract the callee from the nactual.
     */
    JSObject &callee = fp->formalArgsEnd()[-(int(nactual) + 2)].toObject();
    JSFunction *fun = callee.getFunctionPrivate();
    JSScript *script = fun->script();

    /*
     * FixupArity/RemovePartialFrame expect to be called after the early
     * prologue. Pass null for ncode: either we will jump into jit code, which
     * will set ncode, or we will jump into js::Interpret, which does not care
     * about ncode.
     */
    fp->initCallFrameEarlyPrologue(fun, NULL);

    /* Empty script does nothing. */
    if (script->isEmpty()) {
        bool callingNew = fp->isConstructing();
        RemovePartialFrame(cx, fp);
        Value *vp = f.regs.sp - (nactual + 2);
        if (callingNew)
            vp[0] = vp[1];
        else
            vp[0].setUndefined();
        return NULL;
    }

    if (nactual != fp->numFormalArgs()) {
        fp = (JSStackFrame *)FixupArity(f, nactual);
        if (!fp)
            return NULL;
    }

    /* Finish frame initialization. */
    fp->initCallFrameLatePrologue();

    /* These would have been initialized by the prologue. */
    f.regs.fp = fp;
    f.regs.sp = fp->base();
    f.regs.pc = script->code;

    if (fun->isHeavyweight() && !js_GetCallObject(cx, fp))
        THROWV(NULL);

    CompileStatus status = CanMethodJIT(cx, script, fun, &fp->scopeChain());
    if (status == Compile_Okay)
        return script->jit->invoke;

    /* Function did not compile... interpret it. */
    JSBool ok = Interpret(cx, fp);
    InlineReturn(f, ok);

    if (!ok)
        THROWV(NULL);

    return NULL;
}

static inline bool
UncachedInlineCall(VMFrame &f, uint32 flags, void **pret, uint32 argc)
{
    JSContext *cx = f.cx;
    JSStackFrame *fp = f.fp();
    Value *vp = f.regs.sp - (argc + 2);
    JSObject &callee = vp->toObject();
    JSFunction *newfun = callee.getFunctionPrivate();
    JSScript *newscript = newfun->script();

    /* Get pointer to new frame/slots, prepare arguments. */
    StackSpace &stack = cx->stack();
    JSStackFrame *newfp = stack.getInlineFrameWithinLimit(cx, f.regs.sp, argc,
                                                          newfun, newscript, &flags,
                                                          f.entryFp, &f.stackLimit);
    if (JS_UNLIKELY(!newfp))
        return false;
    JS_ASSERT_IF(!vp[1].isPrimitive(), IsSaneThisObject(vp[1].toObject()));

    /* Initialize frame, locals. */
    newfp->initCallFrame(cx, callee, newfun, argc, flags);
    SetValueRangeToUndefined(newfp->slots(), newscript->nfixed);

    /* :TODO: Switch version if currentVersion wasn't overridden. */
    newfp->setCallerVersion((JSVersion)cx->version);

    /* Officially push the frame. */
    stack.pushInlineFrame(cx, newscript, newfp, &f.regs);
    JS_ASSERT(newfp == f.regs.fp);

    /* Scope with a call object parented by callee's parent. */
    if (newfun->isHeavyweight() && !js_GetCallObject(cx, newfp))
        return false;

    /* Marker for debug support. */
    if (JSInterpreterHook hook = cx->debugHooks->callHook) {
        newfp->setHookData(hook(cx, fp, JS_TRUE, 0,
                                cx->debugHooks->callHookData));
    }

    /* Try to compile if not already compiled. */
    if (!newscript->ncode) {
        if (mjit::TryCompile(cx, newscript, newfp->fun(), &newfp->scopeChain()) == Compile_Error) {
            /* A runtime exception was thrown, get out. */
            InlineReturn(f, JS_FALSE);
            return false;
        }
    }

    /* If newscript was successfully compiled, run it. */
    JS_ASSERT(newscript->ncode);
    if (newscript->ncode != JS_UNJITTABLE_METHOD) {
        *pret = newscript->jit->invoke;
        return true;
    }

    /* Otherwise, run newscript in the interpreter. */
    bool ok = !!Interpret(cx, cx->fp());
    InlineReturn(f, JS_TRUE);

    *pret = NULL;
    return ok;
}

void * JS_FASTCALL
stubs::UncachedNew(VMFrame &f, uint32 argc)
{
    JSContext *cx = f.cx;
    Value *vp = f.regs.sp - (argc + 2);

    /* Try to do a fast inline call before the general Invoke path. */
    JSFunction *fun;
    if (IsFunctionObject(*vp, &fun) && fun->isInterpreted() && !fun->script()->isEmpty()) {
        void *ret;
        if (!UncachedInlineCall(f, JSFRAME_CONSTRUCTING, &ret, argc))
            THROWV(NULL);
        return ret;
    }

    if (!InvokeConstructor(cx, InvokeArgsAlreadyOnTheStack(vp, argc)))
        THROWV(NULL);
    return NULL;
}

void * JS_FASTCALL
stubs::UncachedCall(VMFrame &f, uint32 argc)
{
    JSContext *cx = f.cx;

    Value *vp = f.regs.sp - (argc + 2);

    JSObject *obj;
    if (IsFunctionObject(*vp, &obj)) {
        JSFunction *fun = GET_FUNCTION_PRIVATE(cx, obj);

        if (fun->isInterpreted()) {
            void *ret;

            if (fun->u.i.script->isEmpty()) {
                vp->setUndefined();
                f.regs.sp = vp + 1;
                return NULL;
            }

            if (!UncachedInlineCall(f, 0, &ret, argc))
                THROWV(NULL);

            return ret;
        }

        if (fun->isNative()) {
            if (!fun->u.n.native(cx, argc, vp))
                THROWV(NULL);
            return NULL;
        }
    }

    if (!Invoke(f.cx, InvokeArgsAlreadyOnTheStack(vp, argc), 0))
        THROWV(NULL);

    return NULL;
}

void JS_FASTCALL
stubs::PutCallObject(VMFrame &f)
{
    JS_ASSERT(f.fp()->hasCallObj());
    js_PutCallObject(f.cx, f.fp());
}

void JS_FASTCALL
stubs::PutActivationObjects(VMFrame &f)
{
    JS_ASSERT(f.fp()->hasCallObj() || f.fp()->hasArgsObj());
    js::PutActivationObjects(f.cx, f.fp());
}

extern "C" void *
js_InternalThrow(VMFrame &f)
{
    JSContext *cx = f.cx;

    // Make sure sp is up to date.
    JS_ASSERT(cx->regs == &f.regs);

    // Call the throw hook if necessary
    JSThrowHook handler = f.cx->debugHooks->throwHook;
    if (handler) {
        Value rval;
        switch (handler(cx, cx->fp()->script(), cx->regs->pc, Jsvalify(&rval),
                        cx->debugHooks->throwHookData)) {
          case JSTRAP_ERROR:
            cx->throwing = JS_FALSE;
            return NULL;

          case JSTRAP_RETURN:
            cx->throwing = JS_FALSE;
            cx->fp()->setReturnValue(rval);
            return JS_FUNC_TO_DATA_PTR(void *,
                   JS_METHODJIT_DATA(cx).trampolines.forceReturn);

          case JSTRAP_THROW:
            cx->exception = rval;
            break;

          default:
            break;
        }
    }

    jsbytecode *pc = NULL;
    for (;;) {
        pc = FindExceptionHandler(cx);
        if (pc)
            break;

        // If on the 'topmost' frame (where topmost means the first frame
        // called into through js_Interpret). In this case, we still unwind,
        // but we shouldn't return from a JS function, because we're not in a
        // JS function.
        bool lastFrame = (f.entryFp == f.fp());
        js_UnwindScope(cx, 0, cx->throwing);
        if (lastFrame)
            break;

        JS_ASSERT(f.regs.sp == cx->regs->sp);
        InlineReturn(f, JS_FALSE);
    }

    JS_ASSERT(f.regs.sp == cx->regs->sp);

    if (!pc)
        return NULL;

    return cx->fp()->script()->pcToNative(pc);
}

void JS_FASTCALL
stubs::GetCallObject(VMFrame &f)
{
    JS_ASSERT(f.fp()->fun()->isHeavyweight());
    if (!js_GetCallObject(f.cx, f.fp()))
        THROW();
}

static inline void
AdvanceReturnPC(JSContext *cx)
{
    /* Simulate an inline_return by advancing the pc. */
    JS_ASSERT(*cx->regs->pc == JSOP_CALL ||
              *cx->regs->pc == JSOP_NEW ||
              *cx->regs->pc == JSOP_EVAL ||
              *cx->regs->pc == JSOP_APPLY);
    cx->regs->pc += JSOP_CALL_LENGTH;
}

#ifdef JS_TRACER

static inline bool
SwallowErrors(VMFrame &f, JSStackFrame *stopFp)
{
    JSContext *cx = f.cx;

    /* Remove the bottom frame. */
    bool ok = false;
    for (;;) {
        JSStackFrame *fp = cx->fp();

        /* Look for an imacro with hard-coded exception handlers. */
        if (fp->hasImacropc() && cx->throwing) {
            cx->regs->pc = fp->imacropc();
            fp->clearImacropc();
            if (ok)
                break;
        }
        JS_ASSERT(!fp->hasImacropc());

        /* If there's an exception and a handler, set the pc and leave. */
        jsbytecode *pc = FindExceptionHandler(cx);
        if (pc) {
            cx->regs->pc = pc;
            ok = true;
            break;
        }

        /* Don't unwind if this was the entry frame. */
        if (fp == stopFp)
            break;

        /* Unwind and return. */
        ok &= bool(js_UnwindScope(cx, 0, cx->throwing));
        InlineReturn(f, ok);
    }

    /* Update the VMFrame before leaving. */
    JS_ASSERT(&f.regs == cx->regs);

    JS_ASSERT_IF(!ok, cx->fp() == stopFp);
    return ok;
}

static inline bool
AtSafePoint(JSContext *cx)
{
    JSStackFrame *fp = cx->fp();
    if (fp->hasImacropc())
        return false;

    JSScript *script = fp->script();
    if (!script->nmap)
        return false;

    JS_ASSERT(cx->regs->pc >= script->code && cx->regs->pc < script->code + script->length);
    return !!script->nmap[cx->regs->pc - script->code];
}

static inline JSBool
PartialInterpret(VMFrame &f)
{
    JSContext *cx = f.cx;
    JSStackFrame *fp = cx->fp();

    JS_ASSERT(fp->hasImacropc() || !fp->script()->nmap ||
              !fp->script()->nmap[cx->regs->pc - fp->script()->code]);

    JSBool ok = JS_TRUE;
    ok = Interpret(cx, fp, 0, JSINTERP_SAFEPOINT);

    return ok;
}

JS_STATIC_ASSERT(JSOP_NOP == 0);

static inline JSOp
FrameIsFinished(JSContext *cx)
{
    JSOp op = JSOp(*cx->regs->pc);
    return (op == JSOP_RETURN ||
            op == JSOP_RETRVAL ||
            op == JSOP_STOP)
        ? op
        : JSOP_NOP;
}

static bool
RemoveExcessFrames(VMFrame &f, JSStackFrame *entryFrame)
{
    JSContext *cx = f.cx;
    while (cx->fp() != entryFrame || entryFrame->hasImacropc()) {
        JSStackFrame *fp = cx->fp();

        if (AtSafePoint(cx)) {
            JSScript *script = fp->script();
            if (!JaegerShotAtSafePoint(cx, script->nmap[cx->regs->pc - script->code])) {
                if (!SwallowErrors(f, entryFrame))
                    return false;

                /* Could be anywhere - restart outer loop. */
                continue;
            }
            InlineReturn(f, JS_TRUE);
            AdvanceReturnPC(cx);
        } else {
            if (!PartialInterpret(f)) {
                if (!SwallowErrors(f, entryFrame))
                    return false;
            } else if (cx->fp() != entryFrame) {
                /*
                 * Partial interpret could have dropped us anywhere. Deduce the
                 * edge case: at a RETURN, needing to pop a frame.
                 */
                JS_ASSERT(!cx->fp()->hasImacropc());
                if (FrameIsFinished(cx)) {
                    JSOp op = JSOp(*cx->regs->pc);
                    if (op == JSOP_RETURN && !cx->fp()->isBailedAtReturn())
                        cx->fp()->setReturnValue(f.regs.sp[-1]);
                    InlineReturn(f, JS_TRUE);
                    AdvanceReturnPC(cx);
                }
            }
        }
    }

    return true;
}

#if JS_MONOIC
static void
DisableTraceHint(VMFrame &f, ic::MICInfo &mic)
{
    JS_ASSERT(mic.kind == ic::MICInfo::TRACER);

    /*
     * Hack: The value that will be patched is before the executable address,
     * so to get protection right, just unprotect the general region around
     * the jump.
     */
    uint8 *addr = (uint8 *)(mic.traceHint.executableAddress());
    JSC::RepatchBuffer repatch(addr - 64, 128);
    repatch.relink(mic.traceHint, mic.load);

    JaegerSpew(JSpew_PICs, "relinking trace hint %p to %p\n", mic.traceHint.executableAddress(),
               mic.load.executableAddress());

    if (mic.u.hasSlowTraceHint) {
        addr = (uint8 *)(mic.slowTraceHint.executableAddress());
        JSC::RepatchBuffer repatch(addr - 64, 128);
        repatch.relink(mic.slowTraceHint, mic.load);

        JaegerSpew(JSpew_PICs, "relinking trace hint %p to %p\n",
                   mic.slowTraceHint.executableAddress(),
                   mic.load.executableAddress());
    }
}
#endif

#if JS_MONOIC
void *
RunTracer(VMFrame &f, ic::MICInfo &mic)
#else
void *
RunTracer(VMFrame &f)
#endif
{
    JSContext *cx = f.cx;
    JSStackFrame *entryFrame = f.fp();
    TracePointAction tpa;

    /* :TODO: nuke PIC? */
    if (!cx->jitEnabled)
        return NULL;

    bool blacklist;
    uintN inlineCallCount = 0;
    tpa = MonitorTracePoint(f.cx, inlineCallCount, blacklist);
    JS_ASSERT(!TRACE_RECORDER(cx));

#if JS_MONOIC
    if (blacklist)
        DisableTraceHint(f, mic);
#endif

    if ((tpa == TPA_RanStuff || tpa == TPA_Recorded) && cx->throwing)
        tpa = TPA_Error;

	/* Sync up the VMFrame's view of cx->fp(). */
	f.fp() = cx->fp();

    switch (tpa) {
      case TPA_Nothing:
        return NULL;

      case TPA_Error:
        if (!SwallowErrors(f, entryFrame))
            THROWV(NULL);
        JS_ASSERT(!cx->fp()->hasImacropc());
        break;

      case TPA_RanStuff:
      case TPA_Recorded:
        break;
    }

    /*
     * The tracer could have dropped us off on any frame at any position.
     * Well, it could not have removed frames (recursion is disabled).
     *
     * Frames after the entryFrame cannot be entered via JaegerShotAtSafePoint()
     * unless each is at a safe point. We can JaegerShotAtSafePoint these
     * frames individually, but we must unwind to the entryFrame.
     *
     * Note carefully that JaegerShotAtSafePoint can resume methods at
     * arbitrary safe points whereas JaegerShot cannot.
     *
     * If we land on entryFrame without a safe point in sight, we'll end up
     * at the RETURN op. This is an edge case with two paths:
     *
     * 1) The entryFrame is the last inline frame. If it fell on a RETURN,
     *    move the return value down.
     * 2) The entryFrame is NOT the last inline frame. Pop the frame.
     *
     * In both cases, we hijack the stub to return to InjectJaegerReturn. This
     * moves |oldFp->rval| into the scripted return registers.
     */

  restart:
    /* Step 1. Initial removal of excess frames. */
    if (!RemoveExcessFrames(f, entryFrame))
        THROWV(NULL);

    /* IMacros are guaranteed to have been removed by now. */
    JS_ASSERT(!entryFrame->hasImacropc());

    /* Step 2. If entryFrame is at a safe point, just leave. */
    if (AtSafePoint(cx)) {
        uint32 offs = uint32(cx->regs->pc - entryFrame->script()->code);
        JS_ASSERT(entryFrame->script()->nmap[offs]);
        return entryFrame->script()->nmap[offs];
    }

    /* Step 3. If entryFrame is at a RETURN, then leave slightly differently. */
    if (JSOp op = FrameIsFinished(cx)) {
        /* We're not guaranteed that the RETURN was run. */
        if (op == JSOP_RETURN && !entryFrame->isBailedAtReturn())
            entryFrame->setReturnValue(f.regs.sp[-1]);

        /* Don't pop the frame if it's maybe owned by an Invoke. */
        if (f.fp() != f.entryFp) {
            if (!InlineReturn(f, JS_TRUE))
                THROWV(NULL);
        }
        void *retPtr = JS_FUNC_TO_DATA_PTR(void *, InjectJaegerReturn);
        *f.returnAddressLocation() = retPtr;
        return NULL;
    }

    /* Step 4. Do a partial interp, then restart the whole process. */
    if (!PartialInterpret(f)) {
        if (!SwallowErrors(f, entryFrame))
            THROWV(NULL);
    }

    goto restart;
}

#endif /* JS_TRACER */

#if defined JS_TRACER
# if defined JS_MONOIC
void *JS_FASTCALL
stubs::InvokeTracer(VMFrame &f, uint32 index)
{
    JSScript *script = f.fp()->script();
    ic::MICInfo &mic = script->mics[index];

    JS_ASSERT(mic.kind == ic::MICInfo::TRACER);

    return RunTracer(f, mic);
}

# else

void *JS_FASTCALL
stubs::InvokeTracer(VMFrame &f)
{
    return RunTracer(f);
}
# endif /* JS_MONOIC */
#endif /* JS_TRACER */

