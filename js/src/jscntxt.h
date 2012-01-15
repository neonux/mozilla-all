/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
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

/* JS execution context. */

#ifndef jscntxt_h___
#define jscntxt_h___

#include "mozilla/Attributes.h"

#include <string.h>

#include "jsapi.h"
#include "jsfriendapi.h"
#include "jsprvtd.h"
#include "jsatom.h"
#include "jsclist.h"
#include "jsdhash.h"
#include "jsgc.h"
#include "jsgcchunk.h"
#include "jspropertycache.h"
#include "jspropertytree.h"
#include "jsutil.h"
#include "prmjtime.h"

#include "ds/LifoAlloc.h"
#include "gc/Statistics.h"
#include "js/HashTable.h"
#include "js/Vector.h"
#include "vm/StackSpace.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4100) /* Silence unreferenced formal parameter warnings */
#pragma warning(push)
#pragma warning(disable:4355) /* Silence warning about "this" used in base member initializer list */
#endif

JS_BEGIN_EXTERN_C
struct DtoaState;
JS_END_EXTERN_C

struct JSSharpObjectMap {
    jsrefcount  depth;
    uint32_t    sharpgen;
    JSHashTable *table;
};

namespace js {

namespace mjit {
class JaegerCompartment;
}

class WeakMapBase;
class InterpreterFrames;

class ScriptOpcodeCounts;
struct ScriptOpcodeCountsPair;

/*
 * GetSrcNote cache to avoid O(n^2) growth in finding a source note for a
 * given pc in a script. We use the script->code pointer to tag the cache,
 * instead of the script address itself, so that source notes are always found
 * by offset from the bytecode with which they were generated.
 */
struct GSNCache {
    typedef HashMap<jsbytecode *,
                    jssrcnote *,
                    PointerHasher<jsbytecode *, 0>,
                    SystemAllocPolicy> Map;

    jsbytecode      *code;
    Map             map;

    GSNCache() : code(NULL) { }

    void purge();
};

inline GSNCache *
GetGSNCache(JSContext *cx);

struct PendingProxyOperation {
    PendingProxyOperation   *next;
    JSObject                *object;
};

struct ThreadData {
    JSRuntime           *rt;

    /*
     * If non-zero, we were been asked to call the operation callback as soon
     * as possible.  If the thread has an active request, this contributes
     * towards rt->interruptCounter.
     */
    volatile int32_t    interruptFlags;

#ifdef JS_THREADSAFE
    /* The request depth for this thread. */
    unsigned            requestDepth;
#endif

    /* Keeper of the contiguous stack used by all contexts in this thread. */
    StackSpace          stackSpace;

    /* Temporary arena pool used while compiling and decompiling. */
    static const size_t TEMP_LIFO_ALLOC_PRIMARY_CHUNK_SIZE = 1 << 12;
    LifoAlloc           tempLifoAlloc;

  private:
    /*
     * Both of these allocators are used for regular expression code which is shared at the
     * thread-data level.
     */
    JSC::ExecutableAllocator    *execAlloc;
    WTF::BumpPointerAllocator   *bumpAlloc;
    js::RegExpPrivateCache      *repCache;

    JSC::ExecutableAllocator *createExecutableAllocator(JSContext *cx);
    WTF::BumpPointerAllocator *createBumpPointerAllocator(JSContext *cx);
    js::RegExpPrivateCache *createRegExpPrivateCache(JSContext *cx);

  public:
    JSC::ExecutableAllocator *getOrCreateExecutableAllocator(JSContext *cx) {
        if (execAlloc)
            return execAlloc;

        return createExecutableAllocator(cx);
    }

    WTF::BumpPointerAllocator *getOrCreateBumpPointerAllocator(JSContext *cx) {
        if (bumpAlloc)
            return bumpAlloc;

        return createBumpPointerAllocator(cx);
    }

    js::RegExpPrivateCache *getRegExpPrivateCache() {
        return repCache;
    }
    js::RegExpPrivateCache *getOrCreateRegExpPrivateCache(JSContext *cx) {
        if (repCache)
            return repCache;

        return createRegExpPrivateCache(cx);
    }

    /* Called at the end of the global GC sweep phase to deallocate repCache memory. */
    void purgeRegExpPrivateCache();

    /*
     * The GSN cache is per thread since even multi-cx-per-thread embeddings
     * do not interleave js_GetSrcNote calls.
     */
    GSNCache            gsnCache;

    /* Property cache for faster call/get/set invocation. */
    PropertyCache       propertyCache;

    /* State used by jsdtoa.cpp. */
    DtoaState           *dtoaState;

    /* Base address of the native stack for the current thread. */
    uintptr_t           *nativeStackBase;

    /* List of currently pending operations on proxies. */
    PendingProxyOperation *pendingProxyOperation;

    ConservativeGCThreadData conservativeGC;

#ifdef DEBUG
    size_t              noGCOrAllocationCheck;
#endif

    ThreadData(JSRuntime *rt);
    ~ThreadData();

    bool init();

    void mark(JSTracer *trc) {
        stackSpace.mark(trc);
    }

    void purge(JSContext *cx) {
        tempLifoAlloc.freeUnused();
        gsnCache.purge();

        /* FIXME: bug 506341. */
        propertyCache.purge(cx);
    }

#ifdef JS_THREADSAFE
    void sizeOfExcludingThis(JSMallocSizeOfFun mallocSizeOf, size_t *normal, size_t *temporary,
                             size_t *regexpCode, size_t *stackCommitted);
#endif

    /* This must be called with the GC lock held. */
    void triggerOperationCallback(JSRuntime *rt);

    /*
     * Frames currently running in js::Interpret. See InterpreterFrames for
     * details.
     */
    InterpreterFrames *interpreterFrames;
};

} /* namespace js */

#ifdef JS_THREADSAFE

/*
 * Structure uniquely representing a thread.  It holds thread-private data
 * that can be accessed without a global lock.
 */
struct JSThread {
    typedef js::HashMap<void *,
                        JSThread *,
                        js::DefaultHasher<void *>,
                        js::SystemAllocPolicy> Map;

    /* Linked list of all contexts in use on this thread. */
    JSCList             contextList;

    /* Opaque thread-id, from NSPR's PR_GetCurrentThread(). */
    void                *id;

    /* Number of JS_SuspendRequest calls without JS_ResumeRequest. */
    unsigned            suspendCount;

# ifdef DEBUG
    unsigned            checkRequestDepth;
# endif

    /* Factored out of JSThread for !JS_THREADSAFE embedding in JSRuntime. */
    js::ThreadData      data;

    JSThread(JSRuntime *rt, void *id)
      : id(id),
        suspendCount(0),
# ifdef DEBUG
        checkRequestDepth(0),
# endif
        data(rt)
    {
        JS_INIT_CLIST(&contextList);
    }

    ~JSThread() {
        /* The thread must have zero contexts. */
        JS_ASSERT(JS_CLIST_IS_EMPTY(&contextList));
    }

    bool init() {
        return data.init();
    }

    JS_FRIEND_API(void) sizeOfIncludingThis(JSMallocSizeOfFun mallocSizeOf, size_t *normal,
                                            size_t *temporary, size_t *regexpCode,
                                            size_t *stackCommitted);
};

#define JS_THREAD_DATA(cx)      (&(cx)->thread()->data)

extern JSThread *
js_CurrentThreadAndLockGC(JSRuntime *rt);

/*
 * The function takes the GC lock and does not release in successful return.
 * On error (out of memory) the function releases the lock but delegates
 * the error reporting to the caller.
 */
extern JSBool
js_InitContextThreadAndLockGC(JSContext *cx);

/*
 * On entrance the GC lock must be held and it will be held on exit.
 */
extern void
js_ClearContextThread(JSContext *cx);

#endif /* JS_THREADSAFE */

typedef enum JSDestroyContextMode {
    JSDCM_NO_GC,
    JSDCM_MAYBE_GC,
    JSDCM_FORCE_GC,
    JSDCM_NEW_FAILED
} JSDestroyContextMode;

typedef enum JSRuntimeState {
    JSRTS_DOWN,
    JSRTS_LAUNCHING,
    JSRTS_UP,
    JSRTS_LANDING
} JSRuntimeState;

typedef struct JSPropertyTreeEntry {
    JSDHashEntryHdr     hdr;
    js::Shape           *child;
} JSPropertyTreeEntry;

namespace js {

typedef Vector<ScriptOpcodeCountsPair, 0, SystemAllocPolicy> ScriptOpcodeCountsVector;

}

struct JSRuntime
{
    /* Default compartment. */
    JSCompartment       *atomsCompartment;

    /* List of compartments (protected by the GC lock). */
    js::CompartmentVector compartments;

    /* Runtime state, synchronized by the stateChange/gcLock condvar/lock. */
    JSRuntimeState      state;

    /* See comment for JS_AbortIfWrongThread in jsapi.h. */
#ifdef JS_THREADSAFE
  public:
    void clearOwnerThread();
    void setOwnerThread();
    JS_FRIEND_API(bool) onOwnerThread() const;
  private:
    void                *ownerThread_;
  public:
#else
  public:
    bool onOwnerThread() const { return true; }
#endif

    /* Context create/destroy callback. */
    JSContextCallback   cxCallback;

    /* Compartment create/destroy callback. */
    JSCompartmentCallback compartmentCallback;

    js::ActivityCallback  activityCallback;
    void                 *activityCallbackArg;

    /* Garbage collector state, used by jsgc.c. */

    /*
     * Set of all GC chunks with at least one allocated thing. The
     * conservative GC uses it to quickly check if a possible GC thing points
     * into an allocated chunk.
     */
    js::GCChunkSet      gcChunkSet;

    /*
     * Doubly-linked lists of chunks from user and system compartments. The GC
     * allocates its arenas from the corresponding list and when all arenas
     * in the list head are taken, then the chunk is removed from the list.
     * During the GC when all arenas in a chunk become free, that chunk is
     * removed from the list and scheduled for release.
     */
    js::gc::Chunk       *gcSystemAvailableChunkListHead;
    js::gc::Chunk       *gcUserAvailableChunkListHead;
    js::gc::ChunkPool   gcChunkPool;

    js::RootedValueMap  gcRootsHash;
    js::GCLocks         gcLocksHash;
    jsrefcount          gcKeepAtoms;
    size_t              gcBytes;
    size_t              gcTriggerBytes;
    size_t              gcLastBytes;
    size_t              gcMaxBytes;
    size_t              gcMaxMallocBytes;

    /*
     * Number of the committed arenas in all GC chunks including empty chunks.
     * The counter is volatile as it is read without the GC lock, see comments
     * in MaybeGC.
     */
    volatile uint32_t   gcNumArenasFreeCommitted;
    uint32_t            gcNumber;
    js::GCMarker        *gcIncrementalTracer;
    void                *gcVerifyData;
    bool                gcChunkAllocationSinceLastGC;
    int64_t             gcNextFullGCTime;
    int64_t             gcJitReleaseTime;
    JSGCMode            gcMode;
    volatile uintptr_t  gcBarrierFailed;
    volatile uintptr_t  gcIsNeeded;
    js::WeakMapBase     *gcWeakMapList;
    js::gcstats::Statistics gcStats;

    /* The reason that an interrupt-triggered GC should be called. */
    js::gcstats::Reason gcTriggerReason;

    /* Pre-allocated space for the GC mark stack. */
    uintptr_t           gcMarkStackArray[js::MARK_STACK_LENGTH];

    /*
     * Compartment that triggered GC. If more than one Compatment need GC,
     * gcTriggerCompartment is reset to NULL and a global GC is performed.
     */
    JSCompartment       *gcTriggerCompartment;

    /* Compartment that is currently involved in per-compartment GC */
    JSCompartment       *gcCurrentCompartment;

    /*
     * If this is non-NULL, all marked objects must belong to this compartment.
     * This is used to look for compartment bugs.
     */
    JSCompartment       *gcCheckCompartment;

    /*
     * We can pack these flags as only the GC thread writes to them. Atomic
     * updates to packed bytes are not guaranteed, so stores issued by one
     * thread may be lost due to unsynchronized read-modify-write cycles on
     * other threads.
     */
    bool                gcPoke;
    bool                gcMarkAndSweep;
    bool                gcRunning;

    /*
     * These options control the zealousness of the GC. The fundamental values
     * are gcNextScheduled and gcDebugCompartmentGC. At every allocation,
     * gcNextScheduled is decremented. When it reaches zero, we do either a
     * full or a compartmental GC, based on gcDebugCompartmentGC.
     *
     * At this point, if gcZeal_ >= 2 then gcNextScheduled is reset to the
     * value of gcZealFrequency. Otherwise, no additional GCs take place.
     *
     * You can control these values in several ways:
     *   - Pass the -Z flag to the shell (see the usage info for details)
     *   - Call gczeal() or schedulegc() from inside shell-executed JS code
     *     (see the help for details)
     *
     * Additionally, if gzZeal_ == 1 then we perform GCs in select places
     * (during MaybeGC and whenever a GC poke happens). This option is mainly
     * useful to embedders.
     *
     * We use gcZeal_ == 4 to enable write barrier verification. See the comment
     * in jsgc.cpp for more information about this.
     */
#ifdef JS_GC_ZEAL
    int                 gcZeal_;
    int                 gcZealFrequency;
    int                 gcNextScheduled;
    bool                gcDebugCompartmentGC;

    int gcZeal() { return gcZeal_; }

    bool needZealousGC() {
        if (gcNextScheduled > 0 && --gcNextScheduled == 0) {
            if (gcZeal() >= js::gc::ZealAllocThreshold && gcZeal() < js::gc::ZealVerifierThreshold)
                gcNextScheduled = gcZealFrequency;
            return true;
        }
        return false;
    }
#else
    int gcZeal() { return 0; }
    bool needZealousGC() { return false; }
#endif

    JSGCCallback        gcCallback;
    JSGCFinishedCallback gcFinishedCallback;

  private:
    /*
     * Malloc counter to measure memory pressure for GC scheduling. It runs
     * from gcMaxMallocBytes down to zero.
     */
    volatile ptrdiff_t  gcMallocBytes;

  public:
    /*
     * The trace operations to trace embedding-specific GC roots. One is for
     * tracing through black roots and the other is for tracing through gray
     * roots. The black/gray distinction is only relevant to the cycle
     * collector.
     */
    JSTraceDataOp       gcBlackRootsTraceOp;
    void                *gcBlackRootsData;
    JSTraceDataOp       gcGrayRootsTraceOp;
    void                *gcGrayRootsData;

    /* Strong references on scripts held for PCCount profiling API. */
    js::ScriptOpcodeCountsVector *scriptPCCounters;

    /* Well-known numbers held for use by this runtime's contexts. */
    js::Value           NaNValue;
    js::Value           negativeInfinityValue;
    js::Value           positiveInfinityValue;

    JSAtom              *emptyString;

    /* List of active contexts sharing this runtime; protected by gcLock. */
    JSCList             contextList;

    /* Per runtime debug hooks -- see jsprvtd.h and jsdbgapi.h. */
    JSDebugHooks        globalDebugHooks;

    /* If true, new compartments are initially in debug mode. */
    bool                debugMode;

    /* If true, new scripts must be created with PC counter information. */
    bool                profilingScripts;

    /* Had an out-of-memory error which did not populate an exception. */
    JSBool              hadOutOfMemory;

    /*
     * Linked list of all js::Debugger objects. This may be accessed by the GC
     * thread, if any, or a thread that is in a request and holds gcLock.
     */
    JSCList             debuggerList;

    /* Client opaque pointers */
    void                *data;

#ifdef JS_THREADSAFE
    /* These combine to interlock the GC and new requests. */
    PRLock              *gcLock;
    PRCondVar           *gcDone;
    PRCondVar           *requestDone;
    uint32_t            requestCount;
    JSThread            *gcThread;

    js::GCHelperThread  gcHelperThread;

    /* Lock and owning thread pointer for JS_LOCK_RUNTIME. */
    PRLock              *rtLock;
#ifdef DEBUG
    void *              rtLockOwner;
#endif

    /* Used to synchronize down/up state change; protected by gcLock. */
    PRCondVar           *stateChange;

    /*
     * Mapping from NSPR thread identifiers to JSThreads.
     *
     * This map can be accessed by the GC thread; or by the thread that holds
     * gcLock, if GC is not running.
     */
    JSThread::Map       threads;
#endif /* JS_THREADSAFE */

    uint32_t            debuggerMutations;

    /*
     * Security callbacks set on the runtime are used by each context unless
     * an override is set on the context.
     */
    JSSecurityCallbacks *securityCallbacks;

    /* Structured data callbacks are runtime-wide. */
    const JSStructuredCloneCallbacks *structuredCloneCallbacks;

    /* Call this to accumulate telemetry data. */
    JSAccumulateTelemetryDataCallback telemetryCallback;

    /*
     * The propertyRemovals counter is incremented for every JSObject::clear,
     * and for each JSObject::remove method call that frees a slot in the given
     * object. See js_NativeGet and js_NativeSet in jsobj.cpp.
     */
    int32_t             propertyRemovals;

    /* Script filename table. */
    struct JSHashTable  *scriptFilenameTable;
#ifdef JS_THREADSAFE
    PRLock              *scriptFilenameTableLock;
#endif

    /* Number localization, used by jsnum.c */
    const char          *thousandsSeparator;
    const char          *decimalSeparator;
    const char          *numGrouping;

    /*
     * Weak references to lazily-created, well-known XML singletons.
     *
     * NB: Singleton objects must be carefully disconnected from the rest of
     * the object graph usually associated with a JSContext's global object,
     * including the set of standard class objects.  See jsxml.c for details.
     */
    JSObject            *anynameObject;
    JSObject            *functionNamespaceObject;

#ifdef JS_THREADSAFE
    /* Number of threads with active requests and unhandled interrupts. */
    volatile int32_t    interruptCounter;
#else
    js::ThreadData      threadData;

#define JS_THREAD_DATA(cx)      (&(cx)->runtime->threadData)
#endif

  private:
    JSPrincipals        *trustedPrincipals_;
  public:
    void setTrustedPrincipals(JSPrincipals *p) { trustedPrincipals_ = p; }
    JSPrincipals *trustedPrincipals() const { return trustedPrincipals_; }

    /* Literal table maintained by jsatom.c functions. */
    JSAtomState         atomState;

    /* Tables of strings that are pre-allocated in the atomsCompartment. */
    js::StaticStrings   staticStrings;

    JSWrapObjectCallback wrapObjectCallback;
    JSPreWrapCallback    preWrapObjectCallback;
    js::PreserveWrapperCallback preserveWrapperCallback;

    /*
     * To ensure that cx->malloc does not cause a GC, we set this flag during
     * OOM reporting (in js_ReportOutOfMemory). If a GC is requested while
     * reporting the OOM, we ignore it.
     */
    int32_t             inOOMReport;

    JSRuntime();
    ~JSRuntime();

    bool init(uint32_t maxbytes);

    JSRuntime *thisFromCtor() { return this; }

    void setGCLastBytes(size_t lastBytes, JSGCInvocationKind gckind);
    void reduceGCTriggerBytes(size_t amount);

    /*
     * Call the system malloc while checking for GC memory pressure and
     * reporting OOM error when cx is not null. We will not GC from here.
     */
    void* malloc_(size_t bytes, JSContext *cx = NULL) {
        updateMallocCounter(bytes);
        void *p = ::js_malloc(bytes);
        return JS_LIKELY(!!p) ? p : onOutOfMemory(NULL, bytes, cx);
    }

    /*
     * Call the system calloc while checking for GC memory pressure and
     * reporting OOM error when cx is not null. We will not GC from here.
     */
    void* calloc_(size_t bytes, JSContext *cx = NULL) {
        updateMallocCounter(bytes);
        void *p = ::js_calloc(bytes);
        return JS_LIKELY(!!p) ? p : onOutOfMemory(reinterpret_cast<void *>(1), bytes, cx);
    }

    void* realloc_(void* p, size_t oldBytes, size_t newBytes, JSContext *cx = NULL) {
        JS_ASSERT(oldBytes < newBytes);
        updateMallocCounter(newBytes - oldBytes);
        void *p2 = ::js_realloc(p, newBytes);
        return JS_LIKELY(!!p2) ? p2 : onOutOfMemory(p, newBytes, cx);
    }

    void* realloc_(void* p, size_t bytes, JSContext *cx = NULL) {
        /*
         * For compatibility we do not account for realloc that increases
         * previously allocated memory.
         */
        if (!p)
            updateMallocCounter(bytes);
        void *p2 = ::js_realloc(p, bytes);
        return JS_LIKELY(!!p2) ? p2 : onOutOfMemory(p, bytes, cx);
    }

    inline void free_(void* p) {
        /* FIXME: Making this free in the background is buggy. Can it work? */
        js::Foreground::free_(p);
    }

    JS_DECLARE_NEW_METHODS(malloc_, JS_ALWAYS_INLINE)
    JS_DECLARE_DELETE_METHODS(free_, JS_ALWAYS_INLINE)

    bool isGCMallocLimitReached() const { return gcMallocBytes <= 0; }

    void resetGCMallocBytes() { gcMallocBytes = ptrdiff_t(gcMaxMallocBytes); }

    void setGCMaxMallocBytes(size_t value) {
        /*
         * For compatibility treat any value that exceeds PTRDIFF_T_MAX to
         * mean that value.
         */
        gcMaxMallocBytes = (ptrdiff_t(value) >= 0) ? value : size_t(-1) >> 1;
        resetGCMallocBytes();
    }

    /*
     * Call this after allocating memory held by GC things, to update memory
     * pressure counters or report the OOM error if necessary. If oomError and
     * cx is not null the function also reports OOM error.
     *
     * The function must be called outside the GC lock and in case of OOM error
     * the caller must ensure that no deadlock possible during OOM reporting.
     */
    void updateMallocCounter(size_t nbytes) {
        /* We tolerate any thread races when updating gcMallocBytes. */
        ptrdiff_t newCount = gcMallocBytes - ptrdiff_t(nbytes);
        gcMallocBytes = newCount;
        if (JS_UNLIKELY(newCount <= 0))
            onTooMuchMalloc();
    }

    /*
     * The function must be called outside the GC lock.
     */
    JS_FRIEND_API(void) onTooMuchMalloc();

    /*
     * This should be called after system malloc/realloc returns NULL to try
     * to recove some memory or to report an error. Failures in malloc and
     * calloc are signaled by p == null and p == reinterpret_cast<void *>(1).
     * Other values of p mean a realloc failure.
     *
     * The function must be called outside the GC lock.
     */
    JS_FRIEND_API(void *) onOutOfMemory(void *p, size_t nbytes, JSContext *cx);
};

/* Common macros to access thread-local caches in JSThread or JSRuntime. */
#define JS_PROPERTY_CACHE(cx)   (JS_THREAD_DATA(cx)->propertyCache)

#define JS_KEEP_ATOMS(rt)   JS_ATOMIC_INCREMENT(&(rt)->gcKeepAtoms);
#define JS_UNKEEP_ATOMS(rt) JS_ATOMIC_DECREMENT(&(rt)->gcKeepAtoms);

#ifdef JS_ARGUMENT_FORMATTER_DEFINED
/*
 * Linked list mapping format strings for JS_{Convert,Push}Arguments{,VA} to
 * formatter functions.  Elements are sorted in non-increasing format string
 * length order.
 */
struct JSArgumentFormatMap {
    const char          *format;
    size_t              length;
    JSArgumentFormatter formatter;
    JSArgumentFormatMap *next;
};
#endif

extern const JSDebugHooks js_NullDebugHooks;  /* defined in jsdbgapi.cpp */

namespace js {

template <typename T> class Root;
class CheckRoot;

struct AutoResolving;

static inline bool
OptionsHasXML(uint32_t options)
{
    return !!(options & JSOPTION_XML);
}

static inline bool
OptionsSameVersionFlags(uint32_t self, uint32_t other)
{
    static const uint32_t mask = JSOPTION_XML;
    return !((self & mask) ^ (other & mask));
}

/*
 * Flags accompany script version data so that a) dynamically created scripts
 * can inherit their caller's compile-time properties and b) scripts can be
 * appropriately compared in the eval cache across global option changes. An
 * example of the latter is enabling the top-level-anonymous-function-is-error
 * option: subsequent evals of the same, previously-valid script text may have
 * become invalid.
 */
namespace VersionFlags {
static const uintN MASK         = 0x0FFF; /* see JSVersion in jspubtd.h */
static const uintN HAS_XML      = 0x1000; /* flag induced by XML option */
static const uintN FULL_MASK    = 0x3FFF;
}

static inline JSVersion
VersionNumber(JSVersion version)
{
    return JSVersion(uint32_t(version) & VersionFlags::MASK);
}

static inline bool
VersionHasXML(JSVersion version)
{
    return !!(version & VersionFlags::HAS_XML);
}

/* @warning This is a distinct condition from having the XML flag set. */
static inline bool
VersionShouldParseXML(JSVersion version)
{
    return VersionHasXML(version) || VersionNumber(version) >= JSVERSION_1_6;
}

static inline JSVersion
VersionExtractFlags(JSVersion version)
{
    return JSVersion(uint32_t(version) & ~VersionFlags::MASK);
}

static inline void
VersionCopyFlags(JSVersion *version, JSVersion from)
{
    *version = JSVersion(VersionNumber(*version) | VersionExtractFlags(from));
}

static inline bool
VersionHasFlags(JSVersion version)
{
    return !!VersionExtractFlags(version);
}

static inline uintN
VersionFlagsToOptions(JSVersion version)
{
    uintN copts = VersionHasXML(version) ? JSOPTION_XML : 0;
    JS_ASSERT((copts & JSCOMPILEOPTION_MASK) == copts);
    return copts;
}

static inline JSVersion
OptionFlagsToVersion(uintN options, JSVersion version)
{
    return VersionSetXML(version, OptionsHasXML(options));
}

static inline bool
VersionIsKnown(JSVersion version)
{
    return VersionNumber(version) != JSVERSION_UNKNOWN;
}

typedef HashSet<JSObject *,
                DefaultHasher<JSObject *>,
                SystemAllocPolicy> BusyArraysSet;

} /* namespace js */

struct JSContext
{
    explicit JSContext(JSRuntime *rt);
    JSContext *thisDuringConstruction() { return this; }
    ~JSContext();

    /* JSRuntime contextList linkage. */
    JSCList             link;

  private:
    /* See JSContext::findVersion. */
    JSVersion           defaultVersion;      /* script compilation version */
    JSVersion           versionOverride;     /* supercedes defaultVersion when valid */
    bool                hasVersionOverride;

    /* Exception state -- the exception member is a GC root by definition. */
    JSBool              throwing;           /* is there a pending exception? */
    js::Value           exception;          /* most-recently-thrown exception */

    /* Per-context run options. */
    uintN               runOptions;            /* see jsapi.h for JSOPTION_* */

  public:
    int32_t             reportGranularity;  /* see jsprobes.h */

    /* Locale specific callbacks for string conversion. */
    JSLocaleCallbacks   *localeCallbacks;

    js::AutoResolving   *resolvingList;

    /*
     * True if generating an error, to prevent runaway recursion.
     * NB: generatingError packs with throwing below.
     */
    JSPackedBool        generatingError;

    /* Limit pointer for checking native stack consumption during recursion. */
    uintptr_t           stackLimit;

    /* Data shared by threads in an address space. */
    JSRuntime *const    runtime;

    /* GC heap compartment. */
    JSCompartment       *compartment;

    inline void setCompartment(JSCompartment *compartment);

#ifdef JS_THREADSAFE
  private:
    JSThread            *thread_;
  public:
    JSThread *thread() const { return thread_; }

    void setThread(JSThread *thread);
    static const size_t threadOffset() { return offsetof(JSContext, thread_); }
#endif

    /* Current execution stack. */
    js::ContextStack    stack;

    /* ContextStack convenience functions */
    inline bool hasfp() const;
    inline js::StackFrame* fp() const;
    inline js::StackFrame* maybefp() const;
    inline js::FrameRegs& regs() const;
    inline js::FrameRegs* maybeRegs() const;

    /* Set cx->compartment based on the current scope chain. */
    void resetCompartment();

    /* Wrap cx->exception for the current compartment. */
    void wrapPendingException();

  private:
    /* Lazily initialized pool of maps used during parse/emit. */
    js::ParseMapPool    *parseMapPool_;

  public:
    /* Top-level object and pointer to top stack frame's scope chain. */
    JSObject            *globalObject;

    /* State for object and array toSource conversion. */
    JSSharpObjectMap    sharpObjectMap;
    js::BusyArraysSet   busyArrays;

    /* Argument formatter support for JS_{Convert,Push}Arguments{,VA}. */
    JSArgumentFormatMap *argumentFormatMap;

    /* Last message string and log file for debugging. */
    char                *lastMessage;

    /* Per-context optional error reporter. */
    JSErrorReporter     errorReporter;

    /* Branch callback. */
    JSOperationCallback operationCallback;

    /* Client opaque pointers. */
    void                *data;
    void                *data2;

    inline js::RegExpStatics *regExpStatics();

  public:
    js::ParseMapPool &parseMapPool() {
        JS_ASSERT(parseMapPool_);
        return *parseMapPool_;
    }

    inline bool ensureParseMapPool();

    /*
     * The default script compilation version can be set iff there is no code running.
     * This typically occurs via the JSAPI right after a context is constructed.
     */
    inline bool canSetDefaultVersion() const;

    /* Force a version for future script compilation. */
    inline void overrideVersion(JSVersion newVersion);

    /* Set the default script compilation version. */
    void setDefaultVersion(JSVersion version) {
        defaultVersion = version;
    }

    void clearVersionOverride() { hasVersionOverride = false; }
    JSVersion getDefaultVersion() const { return defaultVersion; }
    bool isVersionOverridden() const { return hasVersionOverride; }

    JSVersion getVersionOverride() const {
        JS_ASSERT(isVersionOverridden());
        return versionOverride;
    }

    /*
     * Set the default version if possible; otherwise, force the version.
     * Return whether an override occurred.
     */
    inline bool maybeOverrideVersion(JSVersion newVersion);

    /*
     * If there is no code on the stack, turn the override version into the
     * default version.
     */
    void maybeMigrateVersionOverride() {
        JS_ASSERT(stack.empty());
        if (JS_UNLIKELY(isVersionOverridden())) {
            defaultVersion = versionOverride;
            clearVersionOverride();
        }
    }

    /*
     * Return:
     * - The override version, if there is an override version.
     * - The newest scripted frame's version, if there is such a frame.
     * - The default version.
     *
     * Note: if this ever shows up in a profile, just add caching!
     */
    inline JSVersion findVersion() const;

    void setRunOptions(uintN ropts) {
        JS_ASSERT((ropts & JSRUNOPTION_MASK) == ropts);
        runOptions = ropts;
    }

    /* Note: may override the version. */
    inline void setCompileOptions(uintN newcopts);

    uintN getRunOptions() const { return runOptions; }
    inline uintN getCompileOptions() const;
    inline uintN allOptions() const;

    bool hasRunOption(uintN ropt) const {
        JS_ASSERT((ropt & JSRUNOPTION_MASK) == ropt);
        return !!(runOptions & ropt);
    }

    bool hasStrictOption() const { return hasRunOption(JSOPTION_STRICT); }
    bool hasWErrorOption() const { return hasRunOption(JSOPTION_WERROR); }
    bool hasAtLineOption() const { return hasRunOption(JSOPTION_ATLINE); }
    bool hasJITHardeningOption() const { return !hasRunOption(JSOPTION_SOFTEN); }

    js::LifoAlloc &tempLifoAlloc() { return JS_THREAD_DATA(this)->tempLifoAlloc; }
    inline js::LifoAlloc &typeLifoAlloc();

#ifdef JS_THREADSAFE
    /*
     * AtomizeInline uses this flag to tell RunLastDitchGC and
     * js_ReportOutOfMemory that they should temporarily unlock the atoms
     * compartment.
     */
    bool                atomsCompartmentIsLocked;

    unsigned            outstandingRequests;/* number of JS_BeginRequest calls
                                               without the corresponding
                                               JS_EndRequest. */
    JSCList             threadLinks;        /* JSThread contextList linkage */
#endif

    /* Stack of thread-stack-allocated GC roots. */
    js::AutoGCRooter   *autoGCRooters;

#ifdef JSGC_ROOT_ANALYSIS

    /*
     * Stack allocated GC roots for stack GC heap pointers, which may be
     * overwritten if moved during a GC.
     */
    js::Root<js::gc::Cell*> *thingGCRooters[js::THING_ROOT_COUNT];

#ifdef DEBUG
    /*
     * Stack allocated list of stack locations which hold non-relocatable
     * GC heap pointers (where the target is rooted somewhere else) or integer
     * values which may be confused for GC heap pointers. These are used to
     * suppress false positives which occur when a rooting analysis treats the
     * location as holding a relocatable pointer, but have no other effect on
     * GC behavior.
     */
    js::CheckRoot *checkGCRooters;
#endif

#endif /* JSGC_ROOT_ANALYSIS */

    /* Debug hooks associated with the current context. */
    const JSDebugHooks  *debugHooks;

    /* Security callbacks that override any defined on the runtime. */
    JSSecurityCallbacks *securityCallbacks;

    /* Stored here to avoid passing it around as a parameter. */
    uintN               resolveFlags;

    /* Random number generator state, used by jsmath.cpp. */
    int64_t             rngSeed;

    /* Location to stash the iteration value between JSOP_MOREITER and JSOP_ITERNEXT. */
    js::Value           iterValue;

#ifdef JS_METHODJIT
    bool                 methodJitEnabled;

    inline js::mjit::JaegerCompartment *jaegerCompartment();
#endif

    bool                 inferenceEnabled;

    bool typeInferenceEnabled() { return inferenceEnabled; }

    /* Caller must be holding runtime->gcLock. */
    void updateJITEnabled();

#ifdef MOZ_TRACE_JSCALLS
    /* Function entry/exit debugging callback. */
    JSFunctionCallback    functionCallback;

    void doFunctionCallback(const JSFunction *fun,
                            const JSScript *scr,
                            int entering) const
    {
        if (functionCallback)
            functionCallback(fun, scr, this, entering);
    }
#endif

    DSTOffsetCache dstOffsetCache;

    /* List of currently active non-escaping enumerators (for-in). */
    JSObject *enumerators;

  private:
    /*
     * To go from a live generator frame (on the stack) to its generator object
     * (see comment js_FloatingFrameIfGenerator), we maintain a stack of active
     * generators, pushing and popping when entering and leaving generator
     * frames, respectively.
     */
    js::Vector<JSGenerator *, 2, js::SystemAllocPolicy> genStack;

  public:
    /* Return the generator object for the given generator frame. */
    JSGenerator *generatorFor(js::StackFrame *fp) const;

    /* Early OOM-check. */
    inline bool ensureGeneratorStackSpace();

    bool enterGenerator(JSGenerator *gen) {
        return genStack.append(gen);
    }

    void leaveGenerator(JSGenerator *gen) {
        JS_ASSERT(genStack.back() == gen);
        genStack.popBack();
    }

#ifdef JS_THREADSAFE
    /*
     * When non-null JSContext::free_ delegates the job to the background
     * thread.
     */
    js::GCHelperThread *gcBackgroundFree;
#endif

    js::ThreadData *threadData() { return JS_THREAD_DATA(this); }

    inline void* malloc_(size_t bytes) {
        return runtime->malloc_(bytes, this);
    }

    inline void* mallocNoReport(size_t bytes) {
        JS_ASSERT(bytes != 0);
        return runtime->malloc_(bytes, NULL);
    }

    inline void* calloc_(size_t bytes) {
        JS_ASSERT(bytes != 0);
        return runtime->calloc_(bytes, this);
    }

    inline void* realloc_(void* p, size_t bytes) {
        return runtime->realloc_(p, bytes, this);
    }

    inline void* realloc_(void* p, size_t oldBytes, size_t newBytes) {
        return runtime->realloc_(p, oldBytes, newBytes, this);
    }

    inline void free_(void* p) {
#ifdef JS_THREADSAFE
        if (gcBackgroundFree) {
            gcBackgroundFree->freeLater(p);
            return;
        }
#endif
        runtime->free_(p);
    }

    JS_DECLARE_NEW_METHODS(malloc_, inline)
    JS_DECLARE_DELETE_METHODS(free_, inline)

    void purge();

    /* For DEBUG. */
    inline void assertValidStackDepth(uintN depth);

    bool isExceptionPending() {
        return throwing;
    }

    js::Value getPendingException() {
        JS_ASSERT(throwing);
        return exception;
    }

    void setPendingException(js::Value v);

    void clearPendingException() {
        this->throwing = false;
        this->exception.setUndefined();
    }

    /*
     * Count of currently active compilations.
     * When there are compilations active for the context, the GC must not
     * purge the ParseMapPool.
     */
    uintN activeCompilations;

#ifdef DEBUG
    /*
     * Controls whether a quadratic-complexity assertion is performed during
     * stack iteration, defaults to true.
     */
    bool stackIterAssertionEnabled;
#endif

    /*
     * See JS_SetTrustedPrincipals in jsapi.h.
     * Note: !cx->compartment is treated as trusted.
     */
    bool runningWithTrustedPrincipals() const;

    JS_FRIEND_API(size_t) sizeOfIncludingThis(JSMallocSizeOfFun mallocSizeOf) const;

    static inline JSContext *fromLinkField(JSCList *link) {
        JS_ASSERT(link);
        return reinterpret_cast<JSContext *>(uintptr_t(link) - offsetof(JSContext, link));
    }

#ifdef JS_THREADSAFE
    static inline JSContext *fromThreadLinks(JSCList *link) {
        JS_ASSERT(link);
        return reinterpret_cast<JSContext *>(uintptr_t(link) - offsetof(JSContext, threadLinks));
    }
#endif

  private:
    /*
     * The allocation code calls the function to indicate either OOM failure
     * when p is null or that a memory pressure counter has reached some
     * threshold when p is not null. The function takes the pointer and not
     * a boolean flag to minimize the amount of code in its inlined callers.
     */
    JS_FRIEND_API(void) checkMallocGCPressure(void *p);
}; /* struct JSContext */

namespace js {

struct AutoResolving {
  public:
    enum Kind {
        LOOKUP,
        WATCH
    };

    AutoResolving(JSContext *cx, JSObject *obj, jsid id, Kind kind = LOOKUP
                  JS_GUARD_OBJECT_NOTIFIER_PARAM)
      : context(cx), object(obj), id(id), kind(kind), link(cx->resolvingList)
    {
        JS_GUARD_OBJECT_NOTIFIER_INIT;
        JS_ASSERT(obj);
        cx->resolvingList = this;
    }

    ~AutoResolving() {
        JS_ASSERT(context->resolvingList == this);
        context->resolvingList = link;
    }

    bool alreadyStarted() const {
        return link && alreadyStartedSlow();
    }

  private:
    bool alreadyStartedSlow() const;

    JSContext           *const context;
    JSObject            *const object;
    jsid                const id;
    Kind                const kind;
    AutoResolving       *const link;
    JS_DECL_USE_GUARD_OBJECT_NOTIFIER
};

/*
 * Moving GC Stack Rooting
 *
 * A moving GC may change the physical location of GC allocated things, even
 * when they are rooted, updating all pointers to the thing to refer to its new
 * location. The GC must therefore know about all live pointers to a thing,
 * not just one of them, in order to behave correctly.
 *
 * The classes below are used to root stack locations whose value may be held
 * live across a call that can trigger GC (i.e. a call which might allocate any
 * GC things). For a code fragment such as:
 *
 * Foo();
 * ... = obj->lastProperty();
 *
 * If Foo() can trigger a GC, the stack location of obj must be rooted to
 * ensure that the GC does not move the JSObject referred to by obj without
 * updating obj's location itself. This rooting must happen regardless of
 * whether there are other roots which ensure that the object itself will not
 * be collected.
 *
 * If Foo() cannot trigger a GC, and the same holds for all other calls made
 * between obj's definitions and its last uses, then no rooting is required.
 *
 * Several classes are available for rooting stack locations. All are templated
 * on the type T of the value being rooted, for which RootMethods<T> must
 * have an instantiation.
 *
 * - Root<T> roots an existing stack allocated variable or other location of
 *   type T. This is typically used either when a variable only needs to be
 *   rooted on certain rare paths, or when a function takes a bare GC thing
 *   pointer as an argument and needs to root it. In the latter case a
 *   Handle<T> is generally preferred, see below.
 *
 * - RootedVar<T> declares a variable of type T, whose value is always rooted.
 *
 * - Handle<T> is a const reference to a Root<T> or RootedVar<T>. Handles are
 *   coerced automatically from such a Root<T> or RootedVar<T>. Functions which
 *   take GC things or values as arguments and need to root those arguments
 *   should generally replace those arguments with handles and avoid any
 *   explicit rooting. This has two benefits. First, when several such
 *   functions call each other then redundant rooting of multiple copies of the
 *   GC thing can be avoided. Second, if the caller does not pass a rooted
 *   value a compile error will be generated, which is quicker and easier to
 *   fix than when relying on a separate rooting analysis.
 */

template <> struct RootMethods<const jsid>
{
    static jsid initial() { return JSID_VOID; }
    static ThingRootKind kind() { return THING_ROOT_ID; }
    static bool poisoned(jsid id) { return IsPoisonedId(id); }
};

template <> struct RootMethods<jsid>
{
    static jsid initial() { return JSID_VOID; }
    static ThingRootKind kind() { return THING_ROOT_ID; }
    static bool poisoned(jsid id) { return IsPoisonedId(id); }
};

template <> struct RootMethods<const Value>
{
    static Value initial() { return UndefinedValue(); }
    static ThingRootKind kind() { return THING_ROOT_VALUE; }
    static bool poisoned(const Value &v) { return IsPoisonedValue(v); }
};

template <> struct RootMethods<Value>
{
    static Value initial() { return UndefinedValue(); }
    static ThingRootKind kind() { return THING_ROOT_VALUE; }
    static bool poisoned(const Value &v) { return IsPoisonedValue(v); }
};

template <typename T>
struct RootMethods<T *>
{
    static T *initial() { return NULL; }
    static ThingRootKind kind() { return T::rootKind(); }
    static bool poisoned(T *v) { return IsPoisonedPtr(v); }
};

/*
 * Root a stack location holding a GC thing. This takes a stack pointer
 * and ensures that throughout its lifetime the referenced variable
 * will remain pinned against a moving GC.
 *
 * It is important to ensure that the location referenced by a Root is
 * initialized, as otherwise the GC may try to use the the uninitialized value.
 * It is generally preferable to use either RootedVar for local variables, or
 * Handle for arguments.
 */
template <typename T>
class Root
{
  public:
    Root(JSContext *cx, const T *ptr
         JS_GUARD_OBJECT_NOTIFIER_PARAM)
    {
#ifdef JSGC_ROOT_ANALYSIS
        ThingRootKind kind = RootMethods<T>::kind();
        this->stack = reinterpret_cast<Root<T>**>(&cx->thingGCRooters[kind]);
        this->prev = *stack;
        *stack = this;
#endif

        JS_ASSERT(!RootMethods<T>::poisoned(*ptr));

        this->ptr = ptr;

        JS_GUARD_OBJECT_NOTIFIER_INIT;
    }

    ~Root()
    {
#ifdef JSGC_ROOT_ANALYSIS
        JS_ASSERT(*stack == this);
        *stack = prev;
#endif
    }

#ifdef JSGC_ROOT_ANALYSIS
    Root<T> *previous() { return prev; }
#endif

    const T *address() const { return ptr; }

  private:

#ifdef JSGC_ROOT_ANALYSIS
    Root<T> **stack, *prev;
#endif
    const T *ptr;

    JS_DECL_USE_GUARD_OBJECT_NOTIFIER
};

template<typename T> template <typename S>
inline
Handle<T>::Handle(const Root<S> &root)
{
    testAssign<S>();
    ptr = reinterpret_cast<const T *>(root.address());
}

typedef Root<JSObject*>          RootObject;
typedef Root<JSFunction*>        RootFunction;
typedef Root<Shape*>             RootShape;
typedef Root<BaseShape*>         RootBaseShape;
typedef Root<types::TypeObject*> RootTypeObject;
typedef Root<JSString*>          RootString;
typedef Root<JSAtom*>            RootAtom;
typedef Root<jsid>               RootId;
typedef Root<Value>              RootValue;

/* Mark a stack location as a root for a rooting analysis. */
class CheckRoot
{
#if defined(DEBUG) && defined(JSGC_ROOT_ANALYSIS)

    CheckRoot **stack, *prev;
    const uint8_t *ptr;

  public:
    template <typename T>
    CheckRoot(JSContext *cx, const T *ptr
              JS_GUARD_OBJECT_NOTIFIER_PARAM)
    {
        this->stack = &cx->checkGCRooters;
        this->prev = *stack;
        *stack = this;
        this->ptr = static_cast<const uint8_t*>(ptr);
        JS_GUARD_OBJECT_NOTIFIER_INIT;
    }

    ~CheckRoot()
    {
        JS_ASSERT(*stack == this);
        *stack = prev;
    }

    CheckRoot *previous() { return prev; }

    bool contains(const uint8_t *v, size_t len) {
        return ptr >= v && ptr < v + len;
    }

#else /* DEBUG && JSGC_ROOT_ANALYSIS */

  public:
    template <typename T>
    CheckRoot(JSContext *cx, const T *ptr
              JS_GUARD_OBJECT_NOTIFIER_PARAM)
    {
        JS_GUARD_OBJECT_NOTIFIER_INIT;
    }

#endif /* DEBUG && JSGC_ROOT_ANALYSIS */

    JS_DECL_USE_GUARD_OBJECT_NOTIFIER
};

/* Make a local variable which stays rooted throughout its lifetime. */
template <typename T>
class RootedVar
{
  public:
    RootedVar(JSContext *cx)
        : ptr(RootMethods<T>::initial()), root(cx, &ptr)
    {}

    RootedVar(JSContext *cx, T initial)
        : ptr(initial), root(cx, &ptr)
    {}

    operator T () { return ptr; }
    T operator ->() { return ptr; }
    T * address() { return &ptr; }
    const T * address() const { return &ptr; }
    T raw() { return ptr; }

    T & operator =(T value)
    {
        JS_ASSERT(!RootMethods<T>::poisoned(value));
        ptr = value;
        return ptr;
    }

  private:
    T ptr;
    Root<T> root;
};

template <typename T> template <typename S>
inline
Handle<T>::Handle(const RootedVar<S> &root)
{
    ptr = reinterpret_cast<const T *>(root.address());
}

typedef RootedVar<JSObject*>          RootedVarObject;
typedef RootedVar<JSFunction*>        RootedVarFunction;
typedef RootedVar<Shape*>             RootedVarShape;
typedef RootedVar<BaseShape*>         RootedVarBaseShape;
typedef RootedVar<types::TypeObject*> RootedVarTypeObject;
typedef RootedVar<JSString*>          RootedVarString;
typedef RootedVar<JSAtom*>            RootedVarAtom;
typedef RootedVar<jsid>               RootedVarId;
typedef RootedVar<Value>              RootedVarValue;

#ifdef JS_HAS_XML_SUPPORT
class AutoXMLRooter : private AutoGCRooter {
  public:
    AutoXMLRooter(JSContext *cx, JSXML *xml
                  JS_GUARD_OBJECT_NOTIFIER_PARAM)
      : AutoGCRooter(cx, XML), xml(xml)
    {
        JS_GUARD_OBJECT_NOTIFIER_INIT;
        JS_ASSERT(xml);
    }

    friend void AutoGCRooter::trace(JSTracer *trc);
    friend void JS::MarkRuntime(JSTracer *trc);

  private:
    JSXML * const xml;
    JS_DECL_USE_GUARD_OBJECT_NOTIFIER
};
#endif /* JS_HAS_XML_SUPPORT */

class AutoUnlockGC {
  private:
    JSRuntime *rt;
    JS_DECL_USE_GUARD_OBJECT_NOTIFIER

  public:
    explicit AutoUnlockGC(JSRuntime *rt
                          JS_GUARD_OBJECT_NOTIFIER_PARAM)
      : rt(rt)
    {
        JS_GUARD_OBJECT_NOTIFIER_INIT;
        JS_UNLOCK_GC(rt);
    }
    ~AutoUnlockGC() { JS_LOCK_GC(rt); }
};

class AutoLockAtomsCompartment {
  private:
    JSContext *cx;
    JS_DECL_USE_GUARD_OBJECT_NOTIFIER

  public:
    AutoLockAtomsCompartment(JSContext *cx
                             JS_GUARD_OBJECT_NOTIFIER_PARAM)
      : cx(cx)
    {
        JS_GUARD_OBJECT_NOTIFIER_INIT;
#ifdef JS_THREADSAFE
        JS_ASSERT(!cx->atomsCompartmentIsLocked);
        JS_LOCK(cx, &cx->runtime->atomState.lock);
        cx->atomsCompartmentIsLocked = true;
#endif
    }

    ~AutoLockAtomsCompartment() {
#ifdef JS_THREADSAFE
        JS_ASSERT(cx->atomsCompartmentIsLocked);
        cx->atomsCompartmentIsLocked = false;
        JS_UNLOCK(cx, &cx->runtime->atomState.lock);
#endif
    }
};

class AutoUnlockAtomsCompartmentWhenLocked {
    JSContext *cx;
    JS_DECL_USE_GUARD_OBJECT_NOTIFIER

  public:
    AutoUnlockAtomsCompartmentWhenLocked(JSContext *cx
                                         JS_GUARD_OBJECT_NOTIFIER_PARAM)
      : cx(NULL)
    {
        JS_GUARD_OBJECT_NOTIFIER_INIT;
 #ifdef JS_THREADSAFE
        if (cx->atomsCompartmentIsLocked) {
            this->cx = cx;
            cx->atomsCompartmentIsLocked = false;
            JS_UNLOCK(cx, &cx->runtime->atomState.lock);
        }
#endif
    }

    ~AutoUnlockAtomsCompartmentWhenLocked() {
#ifdef JS_THREADSAFE
        if (cx) {
            JS_ASSERT(!cx->atomsCompartmentIsLocked);
            JS_LOCK(cx, &cx->runtime->atomState.lock);
            cx->atomsCompartmentIsLocked = true;
        }
#endif
    }
};

class AutoKeepAtoms {
    JSRuntime *rt;
    JS_DECL_USE_GUARD_OBJECT_NOTIFIER

  public:
    explicit AutoKeepAtoms(JSRuntime *rt
                           JS_GUARD_OBJECT_NOTIFIER_PARAM)
      : rt(rt)
    {
        JS_GUARD_OBJECT_NOTIFIER_INIT;
        JS_KEEP_ATOMS(rt);
    }
    ~AutoKeepAtoms() { JS_UNKEEP_ATOMS(rt); }
};

class AutoReleasePtr {
    JSContext   *cx;
    void        *ptr;
    JS_DECL_USE_GUARD_OBJECT_NOTIFIER

    AutoReleasePtr(const AutoReleasePtr &other) MOZ_DELETE;
    AutoReleasePtr operator=(const AutoReleasePtr &other) MOZ_DELETE;

  public:
    explicit AutoReleasePtr(JSContext *cx, void *ptr
                            JS_GUARD_OBJECT_NOTIFIER_PARAM)
      : cx(cx), ptr(ptr)
    {
        JS_GUARD_OBJECT_NOTIFIER_INIT;
    }
    ~AutoReleasePtr() { cx->free_(ptr); }
};

/*
 * FIXME: bug 602774: cleaner API for AutoReleaseNullablePtr
 */
class AutoReleaseNullablePtr {
    JSContext   *cx;
    void        *ptr;
    JS_DECL_USE_GUARD_OBJECT_NOTIFIER

    AutoReleaseNullablePtr(const AutoReleaseNullablePtr &other) MOZ_DELETE;
    AutoReleaseNullablePtr operator=(const AutoReleaseNullablePtr &other) MOZ_DELETE;

  public:
    explicit AutoReleaseNullablePtr(JSContext *cx, void *ptr
                                    JS_GUARD_OBJECT_NOTIFIER_PARAM)
      : cx(cx), ptr(ptr)
    {
        JS_GUARD_OBJECT_NOTIFIER_INIT;
    }
    void reset(void *ptr2) {
        if (ptr)
            cx->free_(ptr);
        ptr = ptr2;
    }
    ~AutoReleaseNullablePtr() { if (ptr) cx->free_(ptr); }
};

} /* namespace js */

class JSAutoResolveFlags
{
  public:
    JSAutoResolveFlags(JSContext *cx, uintN flags
                       JS_GUARD_OBJECT_NOTIFIER_PARAM)
      : mContext(cx), mSaved(cx->resolveFlags)
    {
        JS_GUARD_OBJECT_NOTIFIER_INIT;
        cx->resolveFlags = flags;
    }

    ~JSAutoResolveFlags() { mContext->resolveFlags = mSaved; }

  private:
    JSContext *mContext;
    uintN mSaved;
    JS_DECL_USE_GUARD_OBJECT_NOTIFIER
};

extern js::ThreadData *
js_CurrentThreadData(JSRuntime *rt);

extern JSBool
js_InitThreads(JSRuntime *rt);

extern void
js_FinishThreads(JSRuntime *rt);

extern void
js_PurgeThreads(JSContext *cx);

extern void
js_PurgeThreads_PostGlobalSweep(JSContext *cx);

namespace js {

#ifdef JS_THREADSAFE

/* Iterator over ThreadData from all JSThread instances. */
class ThreadDataIter : public JSThread::Map::Range
{
  public:
    ThreadDataIter(JSRuntime *rt) : JSThread::Map::Range(rt->threads.all()) {}

    ThreadData *threadData() const {
        return &front().value->data;
    }
};

#else /* !JS_THREADSAFE */

class ThreadDataIter
{
    JSRuntime *runtime;
    bool done;
  public:
    ThreadDataIter(JSRuntime *rt) : runtime(rt), done(false) {}

    bool empty() const {
        return done;
    }

    void popFront() {
        JS_ASSERT(!done);
        done = true;
    }

    ThreadData *threadData() const {
        JS_ASSERT(!done);
        return &runtime->threadData;
    }
};

#endif  /* !JS_THREADSAFE */

/*
 * Enumerate all contexts in a runtime that are in the same thread as a given
 * context.
 */
class ThreadContextRange {
    JSCList *begin;
    JSCList *end;

public:
    explicit ThreadContextRange(JSContext *cx) {
#ifdef JS_THREADSAFE
        end = &cx->thread()->contextList;
#else
        end = &cx->runtime->contextList;
#endif
        begin = end->next;
    }

    bool empty() const { return begin == end; }
    void popFront() { JS_ASSERT(!empty()); begin = begin->next; }

    JSContext *front() const {
#ifdef JS_THREADSAFE
        return JSContext::fromThreadLinks(begin);
#else
        return JSContext::fromLinkField(begin);
#endif
    }
};

} /* namespace js */

/*
 * Create and destroy functions for JSContext, which is manually allocated
 * and exclusively owned.
 */
extern JSContext *
js_NewContext(JSRuntime *rt, size_t stackChunkSize);

extern void
js_DestroyContext(JSContext *cx, JSDestroyContextMode mode);

/*
 * If unlocked, acquire and release rt->gcLock around *iterp update; otherwise
 * the caller must be holding rt->gcLock.
 */
extern JSContext *
js_ContextIterator(JSRuntime *rt, JSBool unlocked, JSContext **iterp);

/*
 * Iterate through contexts with active requests. The caller must be holding
 * rt->gcLock in case of a thread-safe build, or otherwise guarantee that the
 * context list is not alternated asynchroniously.
 */
extern JS_FRIEND_API(JSContext *)
js_NextActiveContext(JSRuntime *, JSContext *);

#ifdef va_start
extern JSBool
js_ReportErrorVA(JSContext *cx, uintN flags, const char *format, va_list ap);

extern JSBool
js_ReportErrorNumberVA(JSContext *cx, uintN flags, JSErrorCallback callback,
                       void *userRef, const uintN errorNumber,
                       JSBool charArgs, va_list ap);

extern JSBool
js_ExpandErrorArguments(JSContext *cx, JSErrorCallback callback,
                        void *userRef, const uintN errorNumber,
                        char **message, JSErrorReport *reportp,
                        bool charArgs, va_list ap);
#endif

extern void
js_ReportOutOfMemory(JSContext *cx);

extern JS_FRIEND_API(void)
js_ReportAllocationOverflow(JSContext *cx);

/*
 * Report an exception using a previously composed JSErrorReport.
 * XXXbe remove from "friend" API
 */
extern JS_FRIEND_API(void)
js_ReportErrorAgain(JSContext *cx, const char *message, JSErrorReport *report);

extern void
js_ReportIsNotDefined(JSContext *cx, const char *name);

/*
 * Report an attempt to access the property of a null or undefined value (v).
 */
extern JSBool
js_ReportIsNullOrUndefined(JSContext *cx, intN spindex, const js::Value &v,
                           JSString *fallback);

extern void
js_ReportMissingArg(JSContext *cx, const js::Value &v, uintN arg);

/*
 * Report error using js_DecompileValueGenerator(cx, spindex, v, fallback) as
 * the first argument for the error message. If the error message has less
 * then 3 arguments, use null for arg1 or arg2.
 */
extern JSBool
js_ReportValueErrorFlags(JSContext *cx, uintN flags, const uintN errorNumber,
                         intN spindex, const js::Value &v, JSString *fallback,
                         const char *arg1, const char *arg2);

#define js_ReportValueError(cx,errorNumber,spindex,v,fallback)                \
    ((void)js_ReportValueErrorFlags(cx, JSREPORT_ERROR, errorNumber,          \
                                    spindex, v, fallback, NULL, NULL))

#define js_ReportValueError2(cx,errorNumber,spindex,v,fallback,arg1)          \
    ((void)js_ReportValueErrorFlags(cx, JSREPORT_ERROR, errorNumber,          \
                                    spindex, v, fallback, arg1, NULL))

#define js_ReportValueError3(cx,errorNumber,spindex,v,fallback,arg1,arg2)     \
    ((void)js_ReportValueErrorFlags(cx, JSREPORT_ERROR, errorNumber,          \
                                    spindex, v, fallback, arg1, arg2))

extern JSErrorFormatString js_ErrorFormatString[JSErr_Limit];

#ifdef JS_THREADSAFE
# define JS_ASSERT_REQUEST_DEPTH(cx)  (JS_ASSERT((cx)->thread()),             \
                                       JS_ASSERT((cx)->thread()->data.requestDepth >= 1))
#else
# define JS_ASSERT_REQUEST_DEPTH(cx)  ((void) 0)
#endif

/*
 * If the operation callback flag was set, call the operation callback.
 * This macro can run the full GC. Return true if it is OK to continue and
 * false otherwise.
 */
#define JS_CHECK_OPERATION_LIMIT(cx)                                          \
    (JS_ASSERT_REQUEST_DEPTH(cx),                                             \
     (!JS_THREAD_DATA(cx)->interruptFlags || js_InvokeOperationCallback(cx)))

/*
 * Invoke the operation callback and return false if the current execution
 * is to be terminated.
 */
extern JSBool
js_InvokeOperationCallback(JSContext *cx);

extern JSBool
js_HandleExecutionInterrupt(JSContext *cx);

namespace js {

/* These must be called with GC lock taken. */

void
TriggerOperationCallback(JSContext *cx);

void
TriggerAllOperationCallbacks(JSRuntime *rt);

} /* namespace js */

/*
 * Get the topmost scripted frame in a context. Note: if the topmost frame is
 * in the middle of an inline call, that call will be expanded. To avoid this,
 * use cx->stack.currentScript or cx->stack.currentScriptedScopeChain.
 */
extern js::StackFrame *
js_GetScriptedCaller(JSContext *cx, js::StackFrame *fp);

extern jsbytecode*
js_GetCurrentBytecodePC(JSContext* cx);

extern JSScript *
js_GetCurrentScript(JSContext* cx);

namespace js {

#ifdef JS_METHODJIT
namespace mjit {
    void ExpandInlineFrames(JSCompartment *compartment);
}
#endif

} /* namespace js */

/* How much expansion of inlined frames to do when inspecting the stack. */
enum FrameExpandKind {
    FRAME_EXPAND_NONE = 0,
    FRAME_EXPAND_ALL = 1
};

namespace js {

/************************************************************************/

static JS_ALWAYS_INLINE void
MakeRangeGCSafe(Value *vec, size_t len)
{
    PodZero(vec, len);
}

static JS_ALWAYS_INLINE void
MakeRangeGCSafe(Value *beg, Value *end)
{
    PodZero(beg, end - beg);
}

static JS_ALWAYS_INLINE void
MakeRangeGCSafe(jsid *beg, jsid *end)
{
    for (jsid *id = beg; id != end; ++id)
        *id = INT_TO_JSID(0);
}

static JS_ALWAYS_INLINE void
MakeRangeGCSafe(jsid *vec, size_t len)
{
    MakeRangeGCSafe(vec, vec + len);
}

static JS_ALWAYS_INLINE void
MakeRangeGCSafe(const Shape **beg, const Shape **end)
{
    PodZero(beg, end - beg);
}

static JS_ALWAYS_INLINE void
MakeRangeGCSafe(const Shape **vec, size_t len)
{
    PodZero(vec, len);
}

static JS_ALWAYS_INLINE void
SetValueRangeToUndefined(Value *beg, Value *end)
{
    for (Value *v = beg; v != end; ++v)
        v->setUndefined();
}

static JS_ALWAYS_INLINE void
SetValueRangeToUndefined(Value *vec, size_t len)
{
    SetValueRangeToUndefined(vec, vec + len);
}

static JS_ALWAYS_INLINE void
SetValueRangeToNull(Value *beg, Value *end)
{
    for (Value *v = beg; v != end; ++v)
        v->setNull();
}

static JS_ALWAYS_INLINE void
SetValueRangeToNull(Value *vec, size_t len)
{
    SetValueRangeToNull(vec, vec + len);
}

class AutoObjectVector : public AutoVectorRooter<JSObject *>
{
  public:
    explicit AutoObjectVector(JSContext *cx
                              JS_GUARD_OBJECT_NOTIFIER_PARAM)
        : AutoVectorRooter<JSObject *>(cx, OBJVECTOR)
    {
        JS_GUARD_OBJECT_NOTIFIER_INIT;
    }

    JS_DECL_USE_GUARD_OBJECT_NOTIFIER
};

class AutoShapeVector : public AutoVectorRooter<const Shape *>
{
  public:
    explicit AutoShapeVector(JSContext *cx
                             JS_GUARD_OBJECT_NOTIFIER_PARAM)
        : AutoVectorRooter<const Shape *>(cx, SHAPEVECTOR)
    {
        JS_GUARD_OBJECT_NOTIFIER_INIT;
    }

    JS_DECL_USE_GUARD_OBJECT_NOTIFIER
};

class AutoValueArray : public AutoGCRooter
{
    const js::Value *start_;
    unsigned length_;

  public:
    AutoValueArray(JSContext *cx, const js::Value *start, unsigned length
                   JS_GUARD_OBJECT_NOTIFIER_PARAM)
        : AutoGCRooter(cx, VALARRAY), start_(start), length_(length)
    {
        JS_GUARD_OBJECT_NOTIFIER_INIT;
    }

    const Value *start() const { return start_; }
    unsigned length() const { return length_; }

    JS_DECL_USE_GUARD_OBJECT_NOTIFIER
};

/*
 * Allocation policy that uses JSRuntime::malloc_ and friends, so that
 * memory pressure is properly accounted for. This is suitable for
 * long-lived objects owned by the JSRuntime.
 *
 * Since it doesn't hold a JSContext (those may not live long enough), it
 * can't report out-of-memory conditions itself; the caller must check for
 * OOM and take the appropriate action.
 *
 * FIXME bug 647103 - replace these *AllocPolicy names.
 */
class RuntimeAllocPolicy
{
    JSRuntime *const runtime;

  public:
    RuntimeAllocPolicy(JSRuntime *rt) : runtime(rt) {}
    RuntimeAllocPolicy(JSContext *cx) : runtime(cx->runtime) {}
    void *malloc_(size_t bytes) { return runtime->malloc_(bytes); }
    void *realloc_(void *p, size_t bytes) { return runtime->realloc_(p, bytes); }
    void free_(void *p) { runtime->free_(p); }
    void reportAllocOverflow() const {}
};

/*
 * FIXME bug 647103 - replace these *AllocPolicy names.
 */
class ContextAllocPolicy
{
    JSContext *const cx;

  public:
    ContextAllocPolicy(JSContext *cx) : cx(cx) {}
    JSContext *context() const { return cx; }
    void *malloc_(size_t bytes) { return cx->malloc_(bytes); }
    void *realloc_(void *p, size_t oldBytes, size_t bytes) { return cx->realloc_(p, oldBytes, bytes); }
    void free_(void *p) { cx->free_(p); }
    void reportAllocOverflow() const { js_ReportAllocationOverflow(cx); }
};

} /* namespace js */

#ifdef _MSC_VER
#pragma warning(pop)
#pragma warning(pop)
#endif

#endif /* jscntxt_h___ */
