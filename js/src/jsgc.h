/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef jsgc_h___
#define jsgc_h___

/*
 * JS Garbage Collector.
 */
#include <setjmp.h>

#include "jstypes.h"
#include "jsprvtd.h"
#include "jspubtd.h"
#include "jsdhash.h"
#include "jsbit.h"
#include "jsgcchunk.h"
#include "jsutil.h"
#include "jsvector.h"
#include "jsversion.h"
#include "jsobj.h"
#include "jsfun.h"
#include "jsgcstats.h"
#include "jscell.h"

struct JSCompartment;

extern "C" void
js_TraceXML(JSTracer *trc, JSXML* thing);

#if JS_STACK_GROWTH_DIRECTION > 0
# define JS_CHECK_STACK_SIZE(limit, lval)  ((jsuword)(lval) < limit)
#else
# define JS_CHECK_STACK_SIZE(limit, lval)  ((jsuword)(lval) > limit)
#endif

namespace js {

struct Shape;

namespace gc {

/* The kind of GC thing with a finalizer. */
enum FinalizeKind {
    FINALIZE_OBJECT0,
    FINALIZE_OBJECT0_BACKGROUND,
    FINALIZE_OBJECT2,
    FINALIZE_OBJECT2_BACKGROUND,
    FINALIZE_OBJECT4,
    FINALIZE_OBJECT4_BACKGROUND,
    FINALIZE_OBJECT8,
    FINALIZE_OBJECT8_BACKGROUND,
    FINALIZE_OBJECT12,
    FINALIZE_OBJECT12_BACKGROUND,
    FINALIZE_OBJECT16,
    FINALIZE_OBJECT16_BACKGROUND,
    FINALIZE_OBJECT_LAST = FINALIZE_OBJECT16_BACKGROUND,
    FINALIZE_FUNCTION,
    FINALIZE_FUNCTION_AND_OBJECT_LAST = FINALIZE_FUNCTION,
    FINALIZE_SHAPE,
#if JS_HAS_XML_SUPPORT
    FINALIZE_XML,
#endif
    FINALIZE_SHORT_STRING,
    FINALIZE_STRING,
    FINALIZE_EXTERNAL_STRING,
    FINALIZE_LIMIT
};

const size_t ArenaShift = 12;
const size_t ArenaSize = size_t(1) << ArenaShift;
const size_t ArenaMask = ArenaSize - 1;

template <typename T> struct Arena;

/* Every arena has a header. */
struct ArenaHeader {
    JSCompartment   *compartment;
    ArenaHeader     *next;
    FreeCell        *freeList;

  private:
    unsigned        thingKind;

  public:
    inline uintptr_t address() const;
    inline Chunk *chunk() const;
    inline ArenaBitmap *bitmap() const;

    template <typename T>
    Arena<T> *getArena() {
        return reinterpret_cast<Arena<T> *>(address());
    }

    unsigned getThingKind() const {
        return thingKind;
    }

    void setThingKind(unsigned kind) {
        thingKind = kind;
    }

    inline MarkingDelay *getMarkingDelay() const;

#ifdef DEBUG
    JS_FRIEND_API(size_t) getThingSize() const;
#endif
};

template <typename T, size_t N, size_t R1, size_t R2>
struct Things {
    char filler1[R1];
    T    things[N];
    char filler[R2];
};

template <typename T, size_t N, size_t R1>
struct Things<T, N, R1, 0> {
    char filler1[R1];
    T    things[N];
};

template <typename T, size_t N, size_t R2>
struct Things<T, N, 0, R2> {
    T    things[N];
    char filler2[R2];
};

template <typename T, size_t N>
struct Things<T, N, 0, 0> {
    T things[N];
};

template <typename T>
struct Arena {
    /*
     * Layout of an arena:
     * An arena is 4K. We want it to have a header followed by a list of T
     * objects. However, each object should be aligned to a sizeof(T)-boundary.
     * To achieve this, we pad before and after the object array.
     *
     * +-------------+-----+----+----+-----+----+-----+
     * | ArenaHeader | pad | T0 | T1 | ... | Tn | pad |
     * +-------------+-----+----+----+-----+----+-----+
     *
     * <----------------------------------------------> = 4096 bytes
     *               <-----> = Filler1Size
     * <-------------------> = HeaderSize
     *                     <--------------------------> = SpaceAfterHeader
     *                                          <-----> = Filler2Size
     */
    static const size_t Filler1Size =
        tl::If< sizeof(ArenaHeader) % sizeof(T) == 0, size_t,
                0,
                sizeof(T) - sizeof(ArenaHeader) % sizeof(T) >::result;
    static const size_t HeaderSize = sizeof(ArenaHeader) + Filler1Size;
    static const size_t SpaceAfterHeader = ArenaSize - HeaderSize;
    static const size_t Filler2Size = SpaceAfterHeader % sizeof(T);
    static const size_t ThingsPerArena = SpaceAfterHeader / sizeof(T);
    static const size_t FirstThingOffset = HeaderSize;
    static const size_t ThingsSpan = ThingsPerArena * sizeof(T);

    ArenaHeader aheader;
    Things<T, ThingsPerArena, Filler1Size, Filler2Size> t;

    static void staticAsserts() {
        /*
         * Everything we store in the heap must be a multiple of the cell
         * size.
         */
        JS_STATIC_ASSERT(sizeof(T) % Cell::CellSize == 0);
        JS_STATIC_ASSERT(offsetof(Arena<T>, t.things) % sizeof(T) == 0);
        JS_STATIC_ASSERT(sizeof(Arena<T>) == ArenaSize);
    }

    inline FreeCell *buildFreeList();

    bool finalize(JSContext *cx);
};

void FinalizeArena(ArenaHeader *aheader);

/*
 * Live objects are marked black. How many other additional colors are available
 * depends on the size of the GCThing.
 */
static const uint32 BLACK = 0;

/* An arena bitmap contains enough mark bits for all the cells in an arena. */
struct ArenaBitmap {
    static const size_t BitCount = ArenaSize / Cell::CellSize;
    static const size_t BitWords = BitCount / JS_BITS_PER_WORD;

    uintptr_t bitmap[BitWords];

    JS_ALWAYS_INLINE bool isMarked(size_t bit, uint32 color) {
        bit += color;
        JS_ASSERT(bit < BitCount);
        uintptr_t *word = &bitmap[bit / JS_BITS_PER_WORD];
        return *word & (uintptr_t(1) << (bit % JS_BITS_PER_WORD));
    }

    JS_ALWAYS_INLINE bool markIfUnmarked(size_t bit, uint32 color) {
        JS_ASSERT(bit + color < BitCount);
        uintptr_t *word = &bitmap[bit / JS_BITS_PER_WORD];
        uintptr_t mask = (uintptr_t(1) << (bit % JS_BITS_PER_WORD));
        if (*word & mask)
            return false;
        *word |= mask;
        if (color != BLACK) {
            bit += color;
            word = &bitmap[bit / JS_BITS_PER_WORD];
            mask = (uintptr_t(1) << (bit % JS_BITS_PER_WORD));
            if (*word & mask)
                return false;
            *word |= mask;
        }
        return true;
    }

    JS_ALWAYS_INLINE void unmark(size_t bit, uint32 color) {
        bit += color;
        JS_ASSERT(bit < BitCount);
        uintptr_t *word = &bitmap[bit / JS_BITS_PER_WORD];
        *word &= ~(uintptr_t(1) << (bit % JS_BITS_PER_WORD));
    }

#ifdef DEBUG
    bool noBitsSet() {
        for (unsigned i = 0; i < BitWords; i++) {
            if (bitmap[i] != uintptr_t(0))
                return false;
        }
        return true;
    }
#endif
};

/* Ensure that bitmap covers the whole arena. */
JS_STATIC_ASSERT(ArenaSize % Cell::CellSize == 0);
JS_STATIC_ASSERT(ArenaBitmap::BitCount % JS_BITS_PER_WORD == 0);

/*
 * When recursive marking uses too much stack the marking is delayed and
 * the corresponding arenas are put into a stack using a linked via the
 * following per arena structure.
 */
struct MarkingDelay {
    ArenaHeader *link;

    void init()
    {
        link = NULL;
    }

    /*
     * To separate arenas without things to mark later from the arena at the
     * marked delay stack bottom we use for the latter a special sentinel
     * value. We set it to the header for the second arena in the chunk
     * starting the 0 address.
     */
    static ArenaHeader *stackBottom() {
        return reinterpret_cast<ArenaHeader *>(ArenaSize);
    }
};

struct EmptyArenaLists {
    /* Arenas with no internal freelist prepared. */
    ArenaHeader *cellFreeList;

    /* Arenas with internal freelists prepared for a given finalize kind. */
    ArenaHeader *freeLists[FINALIZE_LIMIT];

    void init() {
        PodZero(this);
    }

    ArenaHeader *getOtherArena() {
        ArenaHeader *aheader = cellFreeList;
        if (aheader) {
            cellFreeList = aheader->next;
            return aheader;
        }
        for (int i = 0; i < FINALIZE_LIMIT; i++) {
            aheader = freeLists[i];
            if (aheader) {
                freeLists[i] = aheader->next;
                return aheader;
            }
        }
        JS_NOT_REACHED("No arena");
        return NULL;
    }

    ArenaHeader *getTypedFreeList(unsigned thingKind) {
        JS_ASSERT(thingKind < FINALIZE_LIMIT);
        ArenaHeader *aheader = freeLists[thingKind];
        if (aheader)
            freeLists[thingKind] = aheader->next;
        return aheader;
    }

    void insert(ArenaHeader *aheader) {
        unsigned thingKind = aheader->getThingKind();
        aheader->next = freeLists[thingKind];
        freeLists[thingKind] = aheader;
    }
};

/* The chunk header (located at the end of the chunk to preserve arena alignment). */
struct ChunkInfo {
    Chunk           *link;
    JSRuntime       *runtime;
    EmptyArenaLists emptyArenaLists;
    size_t          age;
    size_t          numFree;
#ifdef JS_THREADSAFE
    PRLock          *chunkLock;
#endif
};

/* Chunks contain arenas and associated data structures (mark bitmap, delayed marking state). */
struct Chunk {
    static const size_t BytesPerArena = ArenaSize +
                                        sizeof(ArenaBitmap) +
                                        sizeof(MarkingDelay);

    static const size_t ArenasPerChunk = (GC_CHUNK_SIZE - sizeof(ChunkInfo)) / BytesPerArena;

    Arena<FreeCell> arenas[ArenasPerChunk];
    ArenaBitmap     bitmaps[ArenasPerChunk];
    MarkingDelay    markingDelay[ArenasPerChunk];

    ChunkInfo       info;

    static Chunk *fromAddress(uintptr_t addr) {
        addr &= ~GC_CHUNK_MASK;
        return reinterpret_cast<Chunk *>(addr);
    }

    static bool withinArenasRange(uintptr_t addr) {
        uintptr_t offset = addr & GC_CHUNK_MASK;
        return offset < ArenasPerChunk * ArenaSize;
    }

    static size_t arenaIndex(uintptr_t addr) {
        JS_ASSERT(withinArenasRange(addr));
        return (addr & GC_CHUNK_MASK) >> ArenaShift;
    }

    void clearMarkBitmap();
    bool init(JSRuntime *rt);

    bool unused();
    bool hasAvailableArenas();
    bool withinArenasRange(Cell *cell);

    template <typename T>
    ArenaHeader *allocateArena(JSContext *cx, unsigned thingKind);

    void releaseArena(ArenaHeader *aheader);

    JSRuntime *getRuntime();
};
JS_STATIC_ASSERT(sizeof(Chunk) <= GC_CHUNK_SIZE);
JS_STATIC_ASSERT(sizeof(Chunk) + Chunk::BytesPerArena > GC_CHUNK_SIZE);

inline uintptr_t
Cell::address() const
{
    uintptr_t addr = uintptr_t(this);
    JS_ASSERT(addr % Cell::CellSize == 0);
    JS_ASSERT(Chunk::withinArenasRange(addr));
    return addr;
}

inline ArenaHeader *
Cell::arenaHeader() const
{
    uintptr_t addr = address();
    addr &= ~ArenaMask;
    return reinterpret_cast<ArenaHeader *>(addr);
}

Chunk *
Cell::chunk() const
{
    uintptr_t addr = uintptr_t(this);
    JS_ASSERT(addr % Cell::CellSize == 0);
    addr &= ~(GC_CHUNK_SIZE - 1);
    return reinterpret_cast<Chunk *>(addr);
}

ArenaBitmap *
Cell::bitmap() const
{
    return &chunk()->bitmaps[Chunk::arenaIndex(address())];
}

STATIC_POSTCONDITION_ASSUME(return < ArenaBitmap::BitCount)
size_t
Cell::cellIndex() const
{
    uintptr_t addr = address();
    return (addr & ArenaMask) >> Cell::CellShift;
}

#ifdef DEBUG
inline bool
Cell::isAligned() const
{
    uintptr_t offset = address() & ArenaMask;
    return offset % arenaHeader()->getThingSize() == 0;
}
#endif

inline uintptr_t
ArenaHeader::address() const
{
    uintptr_t addr = reinterpret_cast<uintptr_t>(this);
    JS_ASSERT(!(addr & ArenaMask));
    JS_ASSERT(Chunk::withinArenasRange(addr));
    return addr;
}

inline Chunk *
ArenaHeader::chunk() const
{
    return Chunk::fromAddress(address());
}

inline ArenaBitmap *
ArenaHeader::bitmap() const
{
    return &chunk()->bitmaps[Chunk::arenaIndex(address())];
}

inline MarkingDelay *
ArenaHeader::getMarkingDelay() const
{
    return &chunk()->markingDelay[Chunk::arenaIndex(address())];
}

static void
AssertValidColor(const void *thing, uint32 color)
{
#ifdef DEBUG
    ArenaHeader *aheader = reinterpret_cast<const js::gc::Cell *>(thing)->arenaHeader();
    JS_ASSERT_IF(color, color < aheader->getThingSize() / Cell::CellSize);
#endif
}

inline bool
Cell::isMarked(uint32 color = BLACK) const
{
    AssertValidColor(this, color);
    return bitmap()->isMarked(cellIndex(), color);
}

bool
Cell::markIfUnmarked(uint32 color = BLACK) const
{
    AssertValidColor(this, color);
    return bitmap()->markIfUnmarked(cellIndex(), color);
}

void
Cell::unmark(uint32 color) const
{
    JS_ASSERT(color != BLACK);
    AssertValidColor(this, color);
    bitmap()->unmark(cellIndex(), color);
}

JSCompartment *
Cell::compartment() const
{
    return arenaHeader()->compartment;
}

#define JSTRACE_XML         3

/*
 * One past the maximum trace kind.
 */
#define JSTRACE_LIMIT       4

/*
 * Lower limit after which we limit the heap growth
 */
const size_t GC_ARENA_ALLOCATION_TRIGGER = 30 * js::GC_CHUNK_SIZE;

/*
 * A GC is triggered once the number of newly allocated arenas
 * is GC_HEAP_GROWTH_FACTOR times the number of live arenas after
 * the last GC starting after the lower limit of
 * GC_ARENA_ALLOCATION_TRIGGER.
 */
const float GC_HEAP_GROWTH_FACTOR = 3.0f;

static inline size_t
GetFinalizableTraceKind(size_t thingKind)
{
    JS_STATIC_ASSERT(JSExternalString::TYPE_LIMIT == 8);

    static const uint8 map[FINALIZE_LIMIT] = {
        JSTRACE_OBJECT,     /* FINALIZE_OBJECT0 */
        JSTRACE_OBJECT,     /* FINALIZE_OBJECT0_BACKGROUND */
        JSTRACE_OBJECT,     /* FINALIZE_OBJECT2 */
        JSTRACE_OBJECT,     /* FINALIZE_OBJECT2_BACKGROUND */
        JSTRACE_OBJECT,     /* FINALIZE_OBJECT4 */
        JSTRACE_OBJECT,     /* FINALIZE_OBJECT4_BACKGROUND */
        JSTRACE_OBJECT,     /* FINALIZE_OBJECT8 */
        JSTRACE_OBJECT,     /* FINALIZE_OBJECT8_BACKGROUND */
        JSTRACE_OBJECT,     /* FINALIZE_OBJECT12 */
        JSTRACE_OBJECT,     /* FINALIZE_OBJECT12_BACKGROUND */
        JSTRACE_OBJECT,     /* FINALIZE_OBJECT16 */
        JSTRACE_OBJECT,     /* FINALIZE_OBJECT16_BACKGROUND */
        JSTRACE_OBJECT,     /* FINALIZE_FUNCTION */
        JSTRACE_SHAPE,      /* FINALIZE_SHAPE */
#if JS_HAS_XML_SUPPORT      /* FINALIZE_XML */
        JSTRACE_XML,
#endif
        JSTRACE_STRING,     /* FINALIZE_SHORT_STRING */
        JSTRACE_STRING,     /* FINALIZE_STRING */
        JSTRACE_STRING,     /* FINALIZE_EXTERNAL_STRING */
    };

    JS_ASSERT(thingKind < FINALIZE_LIMIT);
    return map[thingKind];
}

inline uint32
GetGCThingTraceKind(const void *thing);

static inline JSRuntime *
GetGCThingRuntime(void *thing)
{
    return reinterpret_cast<FreeCell *>(thing)->chunk()->info.runtime;
}

/* The arenas in a list have uniform kind. */
struct ArenaList {
    ArenaHeader           *head;          /* list start */
    ArenaHeader           *cursor;        /* arena with free things */
    volatile bool         hasToBeFinalized;

    inline void init() {
        head = NULL;
        cursor = NULL;
        hasToBeFinalized = false;
    }

    inline ArenaHeader *getNextWithFreeList() {
        JS_ASSERT(!hasToBeFinalized);
        while (cursor) {
            ArenaHeader *aheader = cursor;
            cursor = aheader->next;
            if (aheader->freeList)
                return aheader;
        }
        return NULL;
    }

#ifdef DEBUG
    bool markedThingsInArenaList() {
        for (ArenaHeader *aheader = head; aheader; aheader = aheader->next) {
            if (!aheader->bitmap()->noBitsSet())
                return true;
        }
        return false;
    }
#endif

    inline void insert(ArenaHeader *aheader) {
        aheader->next = head;
        head = aheader;
    }

    void releaseAll(unsigned thingKind) {
        while (head) {
            ArenaHeader *next = head->next;
            head->chunk()->releaseArena(head);
            head = next;
        }
        head = NULL;
        cursor = NULL;
    }

    inline bool isEmpty() const {
        return !head;
    }
};

struct FreeLists {
    FreeCell       **finalizables[FINALIZE_LIMIT];

    void purge();

    inline FreeCell *getNext(uint32 kind) {
        FreeCell *top = NULL;
        if (finalizables[kind]) {
            top = *finalizables[kind];
            if (top) {
                *finalizables[kind] = top->link;
            } else {
                finalizables[kind] = NULL;
            }
        }
        return top;
    }

    void populate(ArenaHeader *aheader, uint32 thingKind) {
        finalizables[thingKind] = &aheader->freeList;
    }

#ifdef DEBUG
    bool isEmpty() const {
        for (size_t i = 0; i != JS_ARRAY_LENGTH(finalizables); ++i) {
            if (finalizables[i])
                return false;
        }
        return true;
    }
#endif
};
}

typedef Vector<gc::Chunk *, 32, SystemAllocPolicy> GCChunks;

struct GCPtrHasher
{
    typedef void *Lookup;

    static HashNumber hash(void *key) {
        return HashNumber(uintptr_t(key) >> JS_GCTHING_ZEROBITS);
    }

    static bool match(void *l, void *k) { return l == k; }
};

typedef HashMap<void *, uint32, GCPtrHasher, SystemAllocPolicy> GCLocks;

struct RootInfo {
    RootInfo() {}
    RootInfo(const char *name, JSGCRootType type) : name(name), type(type) {}
    const char *name;
    JSGCRootType type;
};

typedef js::HashMap<void *,
                    RootInfo,
                    js::DefaultHasher<void *>,
                    js::SystemAllocPolicy> RootedValueMap;

/* If HashNumber grows, need to change WrapperHasher. */
JS_STATIC_ASSERT(sizeof(HashNumber) == 4);

struct WrapperHasher
{
    typedef Value Lookup;

    static HashNumber hash(Value key) {
        uint64 bits = JSVAL_BITS(Jsvalify(key));
        return (uint32)bits ^ (uint32)(bits >> 32);
    }

    static bool match(const Value &l, const Value &k) { return l == k; }
};

typedef HashMap<Value, Value, WrapperHasher, SystemAllocPolicy> WrapperMap;

class AutoValueVector;
class AutoIdVector;
}

static inline void
CheckGCFreeListLink(js::gc::FreeCell *cell)
{
    /*
     * The GC things on the free lists come from one arena and the things on
     * the free list are linked in ascending address order.
     */
    JS_ASSERT_IF(cell->link, cell->arenaHeader() == cell->link->arenaHeader());
    JS_ASSERT_IF(cell->link, cell < cell->link);
}

extern bool
RefillFinalizableFreeList(JSContext *cx, unsigned thingKind);

#ifdef DEBUG
extern bool
CheckAllocation(JSContext *cx);
#endif

extern JS_FRIEND_API(uint32)
js_GetGCThingTraceKind(void *thing);

#if 1
/*
 * Since we're forcing a GC from JS_GC anyway, don't bother wasting cycles
 * loading oldval.  XXX remove implied force, fix jsinterp.c's "second arg
 * ignored", etc.
 */
#define GC_POKE(cx, oldval) ((cx)->runtime->gcPoke = JS_TRUE)
#else
#define GC_POKE(cx, oldval) ((cx)->runtime->gcPoke = JSVAL_IS_GCTHING(oldval))
#endif

extern JSBool
js_InitGC(JSRuntime *rt, uint32 maxbytes);

extern void
js_FinishGC(JSRuntime *rt);

extern JSBool
js_AddRoot(JSContext *cx, js::Value *vp, const char *name);

extern JSBool
js_AddGCThingRoot(JSContext *cx, void **rp, const char *name);

#ifdef DEBUG
extern void
js_DumpNamedRoots(JSRuntime *rt,
                  void (*dump)(const char *name, void *rp, JSGCRootType type, void *data),
                  void *data);
#endif

extern uint32
js_MapGCRoots(JSRuntime *rt, JSGCRootMapFun map, void *data);

/* Table of pointers with count valid members. */
typedef struct JSPtrTable {
    size_t      count;
    void        **array;
} JSPtrTable;

extern JSBool
js_RegisterCloseableIterator(JSContext *cx, JSObject *obj);

#ifdef JS_TRACER
extern JSBool
js_ReserveObjects(JSContext *cx, size_t nobjects);
#endif

extern JSBool
js_LockGCThingRT(JSRuntime *rt, void *thing);

extern void
js_UnlockGCThingRT(JSRuntime *rt, void *thing);

extern JS_FRIEND_API(bool)
IsAboutToBeFinalized(JSContext *cx, const void *thing);

extern JS_FRIEND_API(bool)
js_GCThingIsMarked(void *thing, uintN color);

extern void
js_TraceStackFrame(JSTracer *trc, js::StackFrame *fp);

namespace js {

extern JS_REQUIRES_STACK void
MarkRuntime(JSTracer *trc);

extern void
TraceRuntime(JSTracer *trc);

extern JS_REQUIRES_STACK JS_FRIEND_API(void)
MarkContext(JSTracer *trc, JSContext *acx);

/* Must be called with GC lock taken. */
extern void
TriggerGC(JSRuntime *rt);

/* Must be called with GC lock taken. */
extern void
TriggerCompartmentGC(JSCompartment *comp);

extern void
MaybeGC(JSContext *cx);

} /* namespace js */

/*
 * Kinds of js_GC invocation.
 */
typedef enum JSGCInvocationKind {
    /* Normal invocation. */
    GC_NORMAL           = 0,

    /*
     * Called from js_DestroyContext for last JSContext in a JSRuntime, when
     * it is imperative that rt->gcPoke gets cleared early in js_GC.
     */
    GC_LAST_CONTEXT     = 1
} JSGCInvocationKind;

/* Pass NULL for |comp| to get a full GC. */
extern void
js_GC(JSContext *cx, JSCompartment *comp, JSGCInvocationKind gckind);

#ifdef JS_THREADSAFE
/*
 * This is a helper for code at can potentially run outside JS request to
 * ensure that the GC is not running when the function returns.
 *
 * This function must be called with the GC lock held.
 */
extern void
js_WaitForGC(JSRuntime *rt);

#else /* !JS_THREADSAFE */

# define js_WaitForGC(rt)    ((void) 0)

#endif

extern void
js_DestroyScriptsToGC(JSContext *cx, JSCompartment *comp);

extern void
FinalizeArenaList(JSContext *cx, js::gc::ArenaList *arenaList, js::gc::ArenaHeader *head);

namespace js {

#ifdef JS_THREADSAFE

/*
 * During the finalization we do not free immediately. Rather we add the
 * corresponding pointers to a buffer which we later release on a separated
 * thread.
 *
 * The buffer is implemented as a vector of 64K arrays of pointers, not as a
 * simple vector, to avoid realloc calls during the vector growth and to not
 * bloat the binary size of the inlined freeLater method. Any OOM during
 * buffer growth results in the pointer being freed immediately.
 */
class GCHelperThread {
    static const size_t FREE_ARRAY_SIZE = size_t(1) << 16;
    static const size_t FREE_ARRAY_LENGTH = FREE_ARRAY_SIZE / sizeof(void *);

    JSContext         *cx;
    PRThread*         thread;
    PRCondVar*        wakeup;
    PRCondVar*        sweepingDone;
    bool              shutdown;

    Vector<void **, 16, js::SystemAllocPolicy> freeVector;
    void            **freeCursor;
    void            **freeCursorEnd;

    struct FinalizeListAndHead {
        js::gc::ArenaList   *list;
        js::gc::ArenaHeader *head;

    };
    
    Vector<FinalizeListAndHead, 64, js::SystemAllocPolicy> finalizeVector;

    JS_FRIEND_API(void)
    replenishAndFreeLater(void *ptr);

    static void freeElementsAndArray(void **array, void **end) {
        JS_ASSERT(array <= end);
        for (void **p = array; p != end; ++p)
            js::Foreground::free_(*p);
        js::Foreground::free_(array);
    }

    static void threadMain(void* arg);

    void threadLoop(JSRuntime *rt);
    void doSweep();

  public:
    GCHelperThread()
      : thread(NULL),
        wakeup(NULL),
        sweepingDone(NULL),
        shutdown(false),
        freeCursor(NULL),
        freeCursorEnd(NULL),
        sweeping(false) { }

    volatile bool     sweeping;
    bool init(JSRuntime *rt);
    void finish(JSRuntime *rt);

    /* Must be called with GC lock taken. */
    void startBackgroundSweep(JSRuntime *rt);

    /* Must be called outside the GC lock. */
    void waitBackgroundSweepEnd(JSRuntime *rt);

    void freeLater(void *ptr) {
        JS_ASSERT(!sweeping);
        if (freeCursor != freeCursorEnd)
            *freeCursor++ = ptr;
        else
            replenishAndFreeLater(ptr);
    }

    bool finalizeLater(js::gc::ArenaList *list) {
        JS_ASSERT(!sweeping);
        JS_ASSERT(!list->hasToBeFinalized);
        if (!list->head)
            return true;
        FinalizeListAndHead f = {list, list->head};
        if (!finalizeVector.append(f))
            return false;
        list->hasToBeFinalized = true;
        return true;
    }

    void setContext(JSContext *context) { cx = context; }
};

#endif /* JS_THREADSAFE */

struct GCChunkHasher {
    typedef gc::Chunk *Lookup;

    /*
     * Strip zeros for better distribution after multiplying by the golden
     * ratio.
     */
    static HashNumber hash(gc::Chunk *chunk) {
        JS_ASSERT(!(jsuword(chunk) & GC_CHUNK_MASK));
        return HashNumber(jsuword(chunk) >> GC_CHUNK_SHIFT);
    }

    static bool match(gc::Chunk *k, gc::Chunk *l) {
        JS_ASSERT(!(jsuword(k) & GC_CHUNK_MASK));
        JS_ASSERT(!(jsuword(l) & GC_CHUNK_MASK));
        return k == l;
    }
};

typedef HashSet<js::gc::Chunk *, GCChunkHasher, SystemAllocPolicy> GCChunkSet;

struct ConservativeGCThreadData {

    /*
     * The GC scans conservatively between ThreadData::nativeStackBase and
     * nativeStackTop unless the latter is NULL.
     */
    jsuword             *nativeStackTop;

    union {
        jmp_buf         jmpbuf;
        jsuword         words[JS_HOWMANY(sizeof(jmp_buf), sizeof(jsuword))];
    } registerSnapshot;

    /*
     * Cycle collector uses this to communicate that the native stack of the
     * GC thread should be scanned only if the thread have more than the given
     * threshold of requests.
     */
    unsigned requestThreshold;

    ConservativeGCThreadData()
      : nativeStackTop(NULL), requestThreshold(0)
    {
    }

    ~ConservativeGCThreadData() {
#ifdef JS_THREADSAFE
        /*
         * The conservative GC scanner should be disabled when the thread leaves
         * the last request.
         */
        JS_ASSERT(!hasStackToScan());
#endif
    }

    JS_NEVER_INLINE void recordStackTop();

#ifdef JS_THREADSAFE
    void updateForRequestEnd(unsigned suspendCount) {
        if (suspendCount)
            recordStackTop();
        else
            nativeStackTop = NULL;
    }
#endif

    bool hasStackToScan() const {
        return !!nativeStackTop;
    }
};

template<class T>
struct MarkStack {
    T *stack;
    uintN tos, limit;

    bool push(T item) {
        if (tos == limit)
            return false;
        stack[tos++] = item;
        return true;
    }

    bool isEmpty() { return tos == 0; }

    T pop() {
        JS_ASSERT(!isEmpty());
        return stack[--tos];
    }

    T &peek() {
        JS_ASSERT(!isEmpty());
        return stack[tos-1];
    }

    MarkStack(void **buffer, size_t size)
    {
        tos = 0;
        limit = size / sizeof(T) - 1;
        stack = (T *)buffer;
    }
};

struct LargeMarkItem
{
    JSObject *obj;
    uintN markpos;

    LargeMarkItem(JSObject *obj) : obj(obj), markpos(0) {}
};

static const size_t OBJECT_MARK_STACK_SIZE = 32768 * sizeof(JSObject *);
static const size_t XML_MARK_STACK_SIZE = 1024 * sizeof(JSXML *);
static const size_t LARGE_MARK_STACK_SIZE = 64 * sizeof(LargeMarkItem);

struct GCMarker : public JSTracer {
  private:
    /* The color is only applied to objects, functions and xml. */
    uint32 color;

  public:
    /* See comments before delayMarkingChildren is jsgc.cpp. */
    js::gc::ArenaHeader *unmarkedArenaStackTop;
#ifdef DEBUG
    size_t              markLaterArenas;
#endif

#if defined(JS_DUMP_CONSERVATIVE_GC_ROOTS) || defined(JS_GCMETER)
    js::gc::ConservativeGCStats conservativeStats;
#endif

#ifdef JS_DUMP_CONSERVATIVE_GC_ROOTS
    Vector<void *, 0, SystemAllocPolicy> conservativeRoots;
    const char *conservativeDumpFileName;

    void dumpConservativeRoots();
#endif

    MarkStack<JSObject *> objStack;
    MarkStack<JSXML *> xmlStack;
    MarkStack<LargeMarkItem> largeStack;

  public:
    explicit GCMarker(JSContext *cx);
    ~GCMarker();

    uint32 getMarkColor() const {
        return color;
    }

    void setMarkColor(uint32 newColor) {
        /* We must process the mark stack here, otherwise we confuse colors. */
        drainMarkStack();
        color = newColor;
    }

    void delayMarkingChildren(const void *thing);

    void markDelayedChildren();

    bool isMarkStackEmpty() {
        return objStack.isEmpty() && xmlStack.isEmpty() && largeStack.isEmpty();
    }

    JS_FRIEND_API(void) drainMarkStack();

    void pushObject(JSObject *obj) {
        if (!objStack.push(obj))
            delayMarkingChildren(obj);
    }

    void pushXML(JSXML *xml) {
        if (!xmlStack.push(xml))
            delayMarkingChildren(xml);
    }
};

void
MarkStackRangeConservatively(JSTracer *trc, Value *begin, Value *end);

} /* namespace js */

extern void
js_FinalizeStringRT(JSRuntime *rt, JSString *str);

/*
 * This function is defined in jsdbgapi.cpp but is declared here to avoid
 * polluting jsdbgapi.h, a public API header, with internal functions.
 */
extern void
js_MarkTraps(JSTracer *trc);

namespace js {
namespace gc {

/*
 * Macro to test if a traversal is the marking phase of GC to avoid exposing
 * ScriptFilenameEntry to traversal implementations.
 */
#define IS_GC_MARKING_TRACER(trc) ((trc)->callback == NULL)

#if JS_HAS_XML_SUPPORT
# define JS_IS_VALID_TRACE_KIND(kind) ((uint32)(kind) < JSTRACE_LIMIT)
#else
# define JS_IS_VALID_TRACE_KIND(kind) ((uint32)(kind) <= JSTRACE_SHAPE)
#endif

/*
 * Set object's prototype while checking that doing so would not create
 * a cycle in the proto chain. The cycle check and proto change are done
 * only when all other requests are finished or suspended to ensure exclusive
 * access to the chain. If there is a cycle, return false without reporting
 * an error. Otherwise, set the proto and return true.
 */
extern bool
SetProtoCheckingForCycles(JSContext *cx, JSObject *obj, JSObject *proto);

JSCompartment *
NewCompartment(JSContext *cx, JSPrincipals *principals);

} /* namespace js */
} /* namespace gc */

inline JSCompartment *
JSObject::getCompartment() const
{
    return compartment();
}

#endif /* jsgc_h___ */
