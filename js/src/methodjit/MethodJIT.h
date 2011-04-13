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

#if !defined jsjaeger_h__ && defined JS_METHODJIT
#define jsjaeger_h__

#include "jscntxt.h"
#include "jscompartment.h"

#include "assembler/assembler/MacroAssemblerCodeRef.h"
#include "assembler/assembler/CodeLocation.h"

#if !defined JS_CPU_X64 && \
    !defined JS_CPU_X86 && \
    !defined JS_CPU_ARM
# error "Oh no, you should define a platform so this compiles."
#endif

#if !defined(JS_NUNBOX32) && !defined(JS_PUNBOX64)
# error "No boxing format selected."
#endif

namespace js {

namespace mjit { struct JITScript; }

struct VMFrame
{
    union Arguments {
        struct {
            void *ptr;
            void *ptr2;
        } x;
        struct {
            uint32 lazyArgsObj;
            uint32 dynamicArgc;
        } call;
    } u;

    VMFrame      *previous;
    void         *scratch;
    JSFrameRegs  regs;
    JSContext    *cx;
    Value        *stackLimit;
    JSStackFrame *entryfp;

/*
 * Value stored in the 'scratch' field when making a native call. This is used
 * by the recompiler and this value must not be written in other cases
 * (i.e. scratch must be used to store a pointer, not an integer.
 * :XXX: remove horrible hack.
 */
#define NATIVE_CALL_SCRATCH_VALUE (void *) 0x1

#if defined(JS_CPU_X86)
    void *savedEBX;
    void *savedEDI;
    void *savedESI;
    void *savedEBP;
    void *savedEIP;

# ifdef JS_NO_FASTCALL
    inline void** returnAddressLocation() {
        return reinterpret_cast<void**>(this) - 5;
    }
# else
    inline void** returnAddressLocation() {
        return reinterpret_cast<void**>(this) - 1;
    }
# endif

    /* The gap between ebp and esp in JaegerTrampoline frames on X86 platforms. */
    static const uint32 STACK_BASE_DIFFERENCE = 0x38;

#elif defined(JS_CPU_X64)
    void *savedRBX;
# ifdef _WIN64
    void *savedRSI;
    void *savedRDI;
# endif
    void *savedR15;
    void *savedR14;
    void *savedR13;
    void *savedR12;
    void *savedRBP;
    void *savedRIP;

# ifdef _WIN64
    inline void** returnAddressLocation() {
        return reinterpret_cast<void**>(this) - 5;
    }
# else
    inline void** returnAddressLocation() {
        return reinterpret_cast<void**>(this) - 1;
    }
# endif

#elif defined(JS_CPU_ARM)
    void *savedR4;
    void *savedR5;
    void *savedR6;
    void *savedR7;
    void *savedR8;
    void *savedR9;
    void *savedR10;
    void *savedR11;
    void *savedLR;

    inline void** returnAddressLocation() {
        return reinterpret_cast<void**>(this) - 1;
    }
#else
# error "The VMFrame layout isn't defined for your processor architecture!"
#endif

    JSRuntime *runtime() { return cx->runtime; }

    /*
     * Get the current frame and JIT. Note that these are NOT stable in case
     * of recompilations; all code which expects these to be stable should
     * check that cx->recompilations() has not changed across a call that could
     * trigger recompilation (pretty much any time the VM is called into).
     */
    JSStackFrame *&fp() { return regs.fp; }
    mjit::JITScript *jit() { return fp()->jit(); }

    /* Get the inner script/PC in case of inlining. */
    inline JSScript *script();
    inline jsbytecode *pc();
};

#ifdef JS_CPU_ARM
// WARNING: Do not call this function directly from C(++) code because it is not ABI-compliant.
extern "C" void JaegerStubVeneer(void);
#endif

namespace mjit {

/* Helper to watch for recompilation and frame expansion activity on a compartment. */
struct RecompilationMonitor
{
    JSContext *cx;

    /*
     * If either a recompilation or expansion occurs, then ICs and stubs should
     * not depend on the frame or JITs being intact. The two are separated for logging.
     */
    unsigned recompilations;
    unsigned frameExpansions;

    RecompilationMonitor(JSContext *cx)
        : cx(cx),
          recompilations(cx->compartment->types.recompilations),
          frameExpansions(cx->compartment->types.frameExpansions)
    {}

    bool recompiled() {
        return cx->compartment->types.recompilations != recompilations
            || cx->compartment->types.frameExpansions != frameExpansions;
    }
};

/*
 * Trampolines to force returns from jit code.
 * See also TrampolineCompiler::generateForceReturn(Fast).
 */
struct Trampolines {
    typedef void (*TrampolinePtr)();

    TrampolinePtr       forceReturn;
    JSC::ExecutablePool *forceReturnPool;

#if (defined(JS_NO_FASTCALL) && defined(JS_CPU_X86)) || defined(_WIN64)
    TrampolinePtr       forceReturnFast;
    JSC::ExecutablePool *forceReturnFastPool;
#endif
};

/*
 * Method JIT compartment data. Currently, there is exactly one per
 * JS compartment. It would be safe for multiple JS compartments to
 * share a JaegerCompartment as long as only one thread can enter
 * the JaegerCompartment at a time.
 */
class JaegerCompartment {
    JSC::ExecutableAllocator *execAlloc_;    // allocator for jit code
    Trampolines              trampolines;    // force-return trampolines
    VMFrame                  *activeFrame_;  // current active VMFrame

    void Finish();

  public:
    bool Initialize();

    ~JaegerCompartment() { Finish(); }

    JSC::ExecutableAllocator *execAlloc() {
        return execAlloc_;
    }

    VMFrame *activeFrame() {
        return activeFrame_;
    }

    void pushActiveFrame(VMFrame *f) {
        f->previous = activeFrame_;
        f->scratch = NULL;
        activeFrame_ = f;
    }

    void popActiveFrame() {
        JS_ASSERT(activeFrame_);
        activeFrame_ = activeFrame_->previous;
    }

    void *forceReturnFromExternC() const {
        return JS_FUNC_TO_DATA_PTR(void *, trampolines.forceReturn);
    }

    void *forceReturnFromFastCall() const {
#if (defined(JS_NO_FASTCALL) && defined(JS_CPU_X86)) || defined(_WIN64)
        return JS_FUNC_TO_DATA_PTR(void *, trampolines.forceReturnFast);
#else
        return JS_FUNC_TO_DATA_PTR(void *, trampolines.forceReturn);
#endif
    }
};

/*
 * Allocation policy for compiler jstl objects. The goal is to free the
 * compiler from having to check and propagate OOM after every time we
 * append to a vector. We do this by reporting OOM to the engine and
 * setting a flag on the compiler when OOM occurs. The compiler is required
 * to check for OOM only before trying to use the contents of the list.
 */
class CompilerAllocPolicy : public ContextAllocPolicy
{
    bool *oomFlag;

    void *checkAlloc(void *p) {
        if (!p)
            *oomFlag = true;
        return p;
    }

  public:
    CompilerAllocPolicy(JSContext *cx, bool *oomFlag)
    : ContextAllocPolicy(cx), oomFlag(oomFlag) {}
    CompilerAllocPolicy(JSContext *cx, Compiler &compiler);

    void *malloc_(size_t bytes) { return checkAlloc(ContextAllocPolicy::malloc_(bytes)); }
    void *realloc_(void *p, size_t bytes) {
        return checkAlloc(ContextAllocPolicy::realloc_(p, bytes));
    }
};

namespace ic {
# if defined JS_POLYIC
    struct PICInfo;
    struct GetElementIC;
    struct SetElementIC;
# endif
# if defined JS_MONOIC
    struct GetGlobalNameIC;
    struct SetGlobalNameIC;
    struct EqualityICInfo;
    struct TraceICInfo;
    struct CallICInfo;
# endif
}
}

typedef void (JS_FASTCALL *VoidStub)(VMFrame &);
typedef void (JS_FASTCALL *VoidVpStub)(VMFrame &, Value *);
typedef void (JS_FASTCALL *VoidStubUInt32)(VMFrame &, uint32);
typedef void (JS_FASTCALL *VoidStubInt32)(VMFrame &, int32);
typedef JSBool (JS_FASTCALL *BoolStub)(VMFrame &);
typedef void * (JS_FASTCALL *VoidPtrStub)(VMFrame &);
typedef void * (JS_FASTCALL *VoidPtrStubPC)(VMFrame &, jsbytecode *);
typedef void * (JS_FASTCALL *VoidPtrStubUInt32)(VMFrame &, uint32);
typedef JSObject * (JS_FASTCALL *JSObjStub)(VMFrame &);
typedef JSObject * (JS_FASTCALL *JSObjStubUInt32)(VMFrame &, uint32);
typedef JSObject * (JS_FASTCALL *JSObjStubFun)(VMFrame &, JSFunction *);
typedef void (JS_FASTCALL *VoidStubFun)(VMFrame &, JSFunction *);
typedef JSObject * (JS_FASTCALL *JSObjStubJSObj)(VMFrame &, JSObject *);
typedef void (JS_FASTCALL *VoidStubAtom)(VMFrame &, JSAtom *);
typedef JSString * (JS_FASTCALL *JSStrStub)(VMFrame &);
typedef JSString * (JS_FASTCALL *JSStrStubUInt32)(VMFrame &, uint32);
typedef void (JS_FASTCALL *VoidStubJSObj)(VMFrame &, JSObject *);
typedef void (JS_FASTCALL *VoidStubPC)(VMFrame &, jsbytecode *);
typedef JSBool (JS_FASTCALL *BoolStubUInt32)(VMFrame &f, uint32);
#ifdef JS_MONOIC
typedef void (JS_FASTCALL *VoidStubCallIC)(VMFrame &, js::mjit::ic::CallICInfo *);
typedef void * (JS_FASTCALL *VoidPtrStubCallIC)(VMFrame &, js::mjit::ic::CallICInfo *);
typedef void (JS_FASTCALL *VoidStubGetGlobal)(VMFrame &, js::mjit::ic::GetGlobalNameIC *);
typedef void (JS_FASTCALL *VoidStubSetGlobal)(VMFrame &, js::mjit::ic::SetGlobalNameIC *);
typedef JSBool (JS_FASTCALL *BoolStubEqualityIC)(VMFrame &, js::mjit::ic::EqualityICInfo *);
typedef void * (JS_FASTCALL *VoidPtrStubTraceIC)(VMFrame &, js::mjit::ic::TraceICInfo *);
#endif
#ifdef JS_POLYIC
typedef void (JS_FASTCALL *VoidStubPIC)(VMFrame &, js::mjit::ic::PICInfo *);
typedef void (JS_FASTCALL *VoidStubGetElemIC)(VMFrame &, js::mjit::ic::GetElementIC *);
typedef void (JS_FASTCALL *VoidStubSetElemIC)(VMFrame &f, js::mjit::ic::SetElementIC *);
#endif

namespace mjit {

struct InlineFrame;
struct CallSite;
struct RejoinSite;

struct NativeMapEntry {
    size_t          bcOff;  /* bytecode offset in script */
    void            *ncode; /* pointer to native code */
};

struct JITScript {
    typedef JSC::MacroAssemblerCodeRef CodeRef;
    CodeRef         code;       /* pool & code addresses */

    JSScript        *script;

    void            *invokeEntry;       /* invoke address */
    void            *fastEntry;         /* cached entry, fastest */
    void            *arityCheckEntry;   /* arity check address */

    /*
     * This struct has several variable-length sections that are allocated on
     * the end:  nmaps, MICs, callICs, etc.  To save space -- worthwhile
     * because JITScripts are common -- we only record their lengths.  We can
     * find any of the sections from the lengths because we know their order.
     * Therefore, do not change the section ordering in finishThisUp() without
     * changing nMICs() et al as well.
     */
    uint32          nNmapPairs:30;      /* The NativeMapEntrys are sorted by .bcOff.
                                           .ncode values may not be NULL. */
    bool            singleStepMode:1;   /* compiled in "single step mode" */
    bool            rejoinPoints:1;     /* compiled with all rejoin points for
                                           inline frame expansions */
    uint32          nInlineFrames;
    uint32          nCallSites;
    uint32          nRejoinSites;
#ifdef JS_MONOIC
    uint32          nGetGlobalNames;
    uint32          nSetGlobalNames;
    uint32          nCallICs;
    uint32          nEqualityICs;
    uint32          nTraceICs;
#endif
#ifdef JS_POLYIC
    uint32          nGetElems;
    uint32          nSetElems;
    uint32          nPICs;
#endif

#ifdef JS_MONOIC
    /* Inline cache at function entry for checking this/argument types. */
    JSC::CodeLocationLabel argsCheckStub;
    JSC::CodeLocationLabel argsCheckFallthrough;
    JSC::CodeLocationJump  argsCheckJump;
    JSC::ExecutablePool *argsCheckPool;
    void resetArgsCheck();
#endif

    /* List of inline caches jumping to the fastEntry. */
    JSCList          callers;

#ifdef JS_MONOIC
    // Additional ExecutablePools that IC stubs were generated into.
    typedef Vector<JSC::ExecutablePool *, 0, SystemAllocPolicy> ExecPoolVector;
    ExecPoolVector execPools;
#endif

    NativeMapEntry *nmap() const;
    js::mjit::InlineFrame *inlineFrames() const;
    js::mjit::CallSite *callSites() const;
    js::mjit::RejoinSite *rejoinSites() const;
#ifdef JS_MONOIC
    ic::GetGlobalNameIC *getGlobalNames() const;
    ic::SetGlobalNameIC *setGlobalNames() const;
    ic::CallICInfo *callICs() const;
    ic::EqualityICInfo *equalityICs() const;
    ic::TraceICInfo *traceICs() const;
#endif
#ifdef JS_POLYIC
    ic::GetElementIC *getElems() const;
    ic::SetElementIC *setElems() const;
    ic::PICInfo     *pics() const;
#endif

    ~JITScript();

    bool isValidCode(void *ptr) {
        char *jitcode = (char *)code.m_code.executableAddress();
        char *jcheck = (char *)ptr;
        return jcheck >= jitcode && jcheck < jitcode + code.m_size;
    }

    void nukeScriptDependentICs();
    void sweepCallICs(JSContext *cx, bool purgeAll);
    void purgeMICs();
    void purgePICs();

    size_t scriptDataSize();

    size_t mainCodeSize() { return code.m_size; } /* doesn't account for fragmentation */

    jsbytecode *nativeToPC(void *returnAddress, CallSite **pinline) const;

    void trace(JSTracer *trc);

  private:
    /* Helpers used to navigate the variable-length sections. */
    char *commonSectionLimit() const;
    char *monoICSectionsLimit() const;
    char *polyICSectionsLimit() const;
};

/*
 * Execute the given mjit code. This is a low-level call and callers must
 * provide the same guarantees as JaegerShot/CheckStackAndEnterMethodJIT.
 */
JSBool EnterMethodJIT(JSContext *cx, JSStackFrame *fp, void *code, Value *stackLimit);

/* Execute a method that has been JIT compiled. */
JSBool JaegerShot(JSContext *cx);

/* Drop into the middle of a method at an arbitrary point, and execute. */
JSBool JaegerShotAtSafePoint(JSContext *cx, void *safePoint);

enum CompileStatus
{
    Compile_Okay,
    Compile_Abort,        // abort compilation
    Compile_InlineAbort,  // inlining attempt failed, continue compilation
    Compile_Retry,        // static overflow or failed inline, try to recompile
    Compile_Error,        // OOM
    Compile_Skipped
};

void JS_FASTCALL
ProfileStubCall(VMFrame &f);

CompileStatus JS_NEVER_INLINE
TryCompile(JSContext *cx, JSStackFrame *fp);

void
ReleaseScriptCode(JSContext *cx, JSScript *script, bool normal);

// Expand either the topmost stack frame or all stack frames inlined by the JIT.
void
ExpandInlineFrames(JSContext *cx, bool all);

// Information about an unsynced slot within a frame.
struct UnsyncedEntry
{
    // Slot being updated, in bytes from the start of the outer JSStackFrame.
    int32 offset;

    bool copy : 1;
    bool constant : 1;
    bool knownType : 1;
    union {
        int32 copiedOffset;
        Value value;
        JSValueType type;
    } u;
};

// Information about a frame inlined during compilation.
struct InlineFrame
{
    InlineFrame *parent;
    jsbytecode *parentpc;
    JSFunction *fun;

    // Total distance between the start of the outer JSStackFrame and the start
    // of this frame, in multiples of sizeof(Value).
    uint32 depth;

    // When making a call from an inline frame, only the slots owned by that
    // frame are guaranteed to be synced. Slots owned by parents (including the
    // this/callee/args of the call) may not be synced, and if they are
    // unsynced the entries here describe how to remat them in case of
    // recompilation. Note that since the arguments cannot be modified within
    // the call without triggering recompilation, the contents of these parent
    // slots are invariant within the call.
    uint32 nUnsyncedEntries;
    UnsyncedEntry *unsyncedEntries;
};

struct CallSite
{
    uint32 codeOffset;
    uint32 inlineIndex;
    uint32 pcOffset;
    size_t id;

    // The identifier is either the address of the stub function being called,
    // or one of the below magic identifiers. Each of these can appear at most
    // once per opcode.

    // Identifier for traps. Since traps can be removed, we make sure they carry over
    // from each compilation, and identify them with a single, canonical
    // ID. Hopefully a SpiderMonkey file won't have two billion source lines.
    static const size_t MAGIC_TRAP_ID = 0;

    // Identifier for the return site from a scripted call.
    static const size_t NCODE_RETURN_ID = 1;

    void initialize(uint32 codeOffset, uint32 inlineIndex, uint32 pcOffset, size_t id) {
        this->codeOffset = codeOffset;
        this->inlineIndex = inlineIndex;
        this->pcOffset = pcOffset;
        this->id = id;
    }

    bool isTrap() const {
        return id == MAGIC_TRAP_ID;
    }
};

struct RejoinSite
{
    // When doing on stack recompilation, we take a frame that made a call at
    // some CallSite in the original JIT and redirect it to a corresponding
    // RejoinSite in the new JIT. The rejoin sites are similar to call sites,
    // with the exception that they do additional checking and coercions from
    // int to double to ensure the stack types are consistent with what the new
    // JIT expects.

    // Note: we don't rejoin at sites within inline calls, such inline frames
    // are expanded first.
    uint32 codeOffset;
    uint32 pcOffset;
    size_t id;

    // Identifier which can match any callsite ID in the original script for
    // this PC. This should appear after all other rejoin sites at the PC.
    static const size_t VARIADIC_ID = 2;

    void initialize(uint32 codeOffset, uint32 pcOffset, size_t id) {
        this->codeOffset = codeOffset;
        this->pcOffset = pcOffset;
        this->id = id;
    }
};

/*
 * Re-enables a tracepoint in the method JIT. When full is true, we
 * also reset the iteration counter.
 */
void
ResetTraceHint(JSScript *script, jsbytecode *pc, uint16_t index, bool full);

uintN
GetCallTargetCount(JSScript *script, jsbytecode *pc);

inline void * bsearch_nmap(NativeMapEntry *nmap, size_t nPairs, size_t bcOff)
{
    size_t lo = 1, hi = nPairs;
    while (1) {
        /* current unsearched space is from lo-1 to hi-1, inclusive. */
        if (lo > hi)
            return NULL; /* not found */
        size_t mid       = (lo + hi) / 2;
        size_t bcOff_mid = nmap[mid-1].bcOff;
        if (bcOff < bcOff_mid) {
            hi = mid-1;
            continue;
        } 
        if (bcOff > bcOff_mid) {
            lo = mid+1;
            continue;
        }
        return nmap[mid-1].ncode;
    }
}

} /* namespace mjit */

inline JSScript *
VMFrame::script()
{
    if (regs.inlined)
        return jit()->inlineFrames()[regs.inlined->inlineIndex].fun->script();
    return fp()->script();
}

inline jsbytecode *
VMFrame::pc()
{
    if (regs.inlined)
        return script()->code + regs.inlined->pcOffset;
    return regs.pc;
}

} /* namespace js */

inline void *
JSScript::maybeNativeCodeForPC(bool constructing, jsbytecode *pc)
{
    js::mjit::JITScript *jit = getJIT(constructing);
    if (!jit)
        return NULL;
    JS_ASSERT(pc >= code && pc < code + length);
    return bsearch_nmap(jit->nmap(), jit->nNmapPairs, (size_t)(pc - code));
}

inline void *
JSScript::nativeCodeForPC(bool constructing, jsbytecode *pc)
{
    js::mjit::JITScript *jit = getJIT(constructing);
    JS_ASSERT(pc >= code && pc < code + length);
    void* native = bsearch_nmap(jit->nmap(), jit->nNmapPairs, (size_t)(pc - code));
    JS_ASSERT(native);
    return native;
}

extern "C" void JaegerTrampolineReturn();

#if defined(_MSC_VER) || defined(_WIN64)
extern "C" void *JaegerThrowpoline(js::VMFrame *vmFrame);
#else
extern "C" void JaegerThrowpoline();
#endif

#endif /* jsjaeger_h__ */

