/* -*- Mode: c++; c-basic-offset: 4; tab-width: 40; indent-tabs-mode: nil -*- */
/* vim: set ts=40 sw=4 et tw=99: */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Mozilla SpiderMonkey bytecode analysis
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2010
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

/* Definitions for javascript analysis. */

#ifndef jsanalyze_h___
#define jsanalyze_h___

#include "jsarena.h"
#include "jscompartment.h"
#include "jscntxt.h"
#include "jsinfer.h"
#include "jsscript.h"

struct JSScript;

/* Forward declaration of downstream register allocations computed for join points. */
namespace js { namespace mjit { struct RegisterAllocation; } }

namespace js {
namespace analyze {

class Script;

/* Information about a bytecode instruction. */
struct Bytecode
{
    friend class Script;

    /* Whether there are any incoming jumps to this instruction. */
    bool jumpTarget : 1;    

    /* Whether there is fallthrough to this instruction from a non-branching instruction. */
    bool fallthrough : 1;

    /* Whether this instruction is the fall through point of a conditional jump. */
    bool jumpFallthrough : 1;

    /* Whether this instruction can be branched to from a switch statement.  Implies jumpTarget. */
    bool switchTarget : 1;

    /* Whether this instruction has been analyzed to get its output defines and stack. */
    bool analyzed : 1;

    /* Whether this is a catch/finally entry point. */
    bool exceptionEntry : 1;

    /* Whether this is in a try block. */
    bool inTryBlock : 1;

    /* Method JIT safe point. */
    bool safePoint : 1;

    /* Stack depth before this opcode. */
    uint32 stackDepth;

    /*
     * The set of locals defined at this point.  This does not include locals which
     * were unconditionally defined at an earlier point in the script.
     */
    uint32 defineCount;
    uint32 *defineArray;

    Bytecode()
    {
        PodZero(this);
    }

  private:
    bool mergeDefines(JSContext *cx, Script *script, bool initial,
                      uint32 newDepth, uint32 *newArray, uint32 newCount);

    /* Whether a local variable is in the define set at this bytecode. */
    bool isDefined(uint32 slot)
    {
        JS_ASSERT(analyzed);
        for (unsigned i = 0; i < defineCount; i++) {
            if (defineArray[i] == slot)
                return true;
        }
        return false;
    }
};

/* Information about a script. */
class Script
{
    friend struct Bytecode;

    JSScript *script;

    Bytecode **codeArray;

    /* Maximum number of locals to consider for a script. */
    static const unsigned LOCAL_LIMIT = 50;

    /* Offsets at which each local becomes unconditionally defined, or a value below. */
    uint32 *locals;

    static const uint32 LOCAL_USE_BEFORE_DEF = uint32(-1);
    static const uint32 LOCAL_CONDITIONALLY_DEFINED = uint32(-2);

    bool outOfMemory;
    bool hadFailure;
    bool usesRval;
    bool usesScope;
    bool usesThis;
    bool hasCalls;

    bool isInlineable;

    JSPackedBool *closedVars;
    JSPackedBool *closedArgs;

  public:
    /* Pool for allocating analysis structures which will not outlive this script. */
    JSArenaPool pool;

    Script();
    ~Script();

    void analyze(JSContext *cx, JSScript *script);

    bool OOM() { return outOfMemory; }
    bool failed() { return hadFailure; }
    bool inlineable(uint32 argc) { return isInlineable && argc == script->fun->nargs; }

    /* Whether there are POPV/SETRVAL bytecodes which can write to the frame's rval. */
    bool usesReturnValue() const { return usesRval; }

    /* Whether there are NAME bytecodes which can access the frame's scope chain. */
    bool usesScopeChain() const { return usesScope; }

    bool usesThisValue() const { return usesThis; }
    bool hasFunctionCalls() const { return hasCalls; }

    bool hasAnalyzed() const { return !!codeArray; }
    JSScript *getScript() const { return script; }

    /* Accessors for bytecode information. */

    Bytecode& getCode(uint32 offset) {
        JS_ASSERT(offset < script->length);
        JS_ASSERT(codeArray[offset]);
        return *codeArray[offset];
    }
    Bytecode& getCode(const jsbytecode *pc) { return getCode(pc - script->code); }

    Bytecode* maybeCode(uint32 offset) {
        JS_ASSERT(offset < script->length);
        return codeArray[offset];
    }
    Bytecode* maybeCode(const jsbytecode *pc) { return maybeCode(pc - script->code); }

    bool jumpTarget(uint32 offset) {
        JS_ASSERT(offset < script->length);
        return codeArray[offset] && codeArray[offset]->jumpTarget;
    }
    bool jumpTarget(const jsbytecode *pc) { return jumpTarget(pc - script->code); }

    /* Accessors for local variable information. */

    unsigned localCount() {
        return (script->nfixed >= LOCAL_LIMIT) ? LOCAL_LIMIT : script->nfixed;
    }

    bool localHasUseBeforeDef(uint32 local) {
        JS_ASSERT(!failed());
        return local >= localCount() || locals[local] == LOCAL_USE_BEFORE_DEF;
    }

    /* These return true for variables that may have a use before def. */
    bool localDefined(uint32 local, uint32 offset) {
        return localHasUseBeforeDef(local) || (locals[local] <= offset) ||
            getCode(offset).isDefined(local);
    }
    bool localDefined(uint32 local, jsbytecode *pc) {
        return localDefined(local, pc - script->code);
    }

    bool argEscapes(unsigned arg)
    {
        JS_ASSERT(script->fun && arg < script->fun->nargs);
        return script->usesEval || script->usesArguments || script->compartment->debugMode ||
            closedArgs[arg];
    }

    bool localEscapes(unsigned local)
    {
        return script->usesEval || script->compartment->debugMode || local >= localCount() ||
            closedVars[local];
    }

  private:
    void setOOM(JSContext *cx) {
        if (!outOfMemory)
            js_ReportOutOfMemory(cx);
        outOfMemory = true;
        hadFailure = true;
    }

    inline bool addJump(JSContext *cx, unsigned offset,
                        unsigned *currentOffset, unsigned *forwardJump,
                        unsigned stackDepth, uint32 *defineArray, unsigned defineCount);

    inline void setLocal(uint32 local, uint32 offset);
};

static inline unsigned
GetBytecodeLength(jsbytecode *pc)
{
    JSOp op = (JSOp)*pc;
    JS_ASSERT(op < JSOP_LIMIT);
    JS_ASSERT(op != JSOP_TRAP);

    if (js_CodeSpec[op].length != -1)
        return js_CodeSpec[op].length;
    return js_GetVariableBytecodeLength(pc);
}

static inline unsigned
GetUseCount(JSScript *script, unsigned offset)
{
    JS_ASSERT(offset < script->length);
    jsbytecode *pc = script->code + offset;
    if (js_CodeSpec[*pc].nuses == -1)
        return js_GetVariableStackUses(JSOp(*pc), pc);
    return js_CodeSpec[*pc].nuses;
}

static inline unsigned
GetDefCount(JSScript *script, unsigned offset)
{
    JS_ASSERT(offset < script->length);
    jsbytecode *pc = script->code + offset;
    if (js_CodeSpec[*pc].ndefs == -1)
        return js_GetEnterBlockStackDefs(NULL, script, pc);

    /*
     * Add an extra pushed value for OR/AND opcodes, so that they are included
     * in the pushed array of stack values for type inference.
     */
    switch (JSOp(*pc)) {
      case JSOP_OR:
      case JSOP_ORX:
      case JSOP_AND:
      case JSOP_ANDX:
        return 1;
      case JSOP_FILTER:
        return 2;
      default:
        return js_CodeSpec[*pc].ndefs;
    }
}

static inline JSOp
ReverseCompareOp(JSOp op)
{
    switch (op) {
      case JSOP_GT:
        return JSOP_LT;
      case JSOP_GE:
        return JSOP_LE;
      case JSOP_LT:
        return JSOP_GT;
      case JSOP_LE:
        return JSOP_GE;
      default:
        JS_NOT_REACHED("unrecognized op");
        return op;
    }
}

/* Untrap a single PC, and retrap it at scope exit. */
struct UntrapOpcode
{
    jsbytecode *pc;
    bool trap;

    UntrapOpcode(JSContext *cx, JSScript *script, jsbytecode *pc)
        : pc(pc), trap(JSOp(*pc) == JSOP_TRAP)
    {
        if (trap)
            *pc = JS_GetTrapOpcode(cx, script, pc);
    }

    ~UntrapOpcode()
    {
        if (trap)
            *pc = JSOP_TRAP;
    }
};

/*
 * Lifetime analysis. The goal of this analysis is to make a single backwards pass
 * over a script to approximate the regions where each variable is live, without
 * doing a full fixpointing live-variables pass. This is based on the algorithm
 * described in:
 *
 * "Quality and Speed in Linear-scan Register Allocation"
 * Traub et. al.
 * PLDI, 1998
 */

/*
 * Information about the lifetime of a local or argument. These form a linked
 * list describing successive intervals in the program where the variable's
 * value may be live. At points in the script not in one of these segments
 * (points in a 'lifetime hole'), the variable is dead and registers containing
 * its type/payload can be discarded without needing to be synced.
 */
struct Lifetime
{
    /*
     * Start and end offsets of this lifetime. The variable is live at the
     * beginning of every bytecode in this (inclusive) range.
     */
    uint32 start;
    uint32 end;

    /*
     * This is an artificial segment extending the lifetime of a variable to
     * the end of a loop, when it is live at the head of the loop. It will not
     * be used anymore in the loop body until the next iteration.
     */
    bool loopTail;

    /*
     * The start of this lifetime is a bytecode writing the variable. Each
     * write to a variable is associated with a lifetime.
     */
    bool write;

    /* Next lifetime. The variable is dead from this->end to next->start. */
    Lifetime *next;

    Lifetime(uint32 offset, Lifetime *next)
        : start(offset), end(offset), loopTail(false), write(false), next(next)
    {}
};

/* Lifetime and modset information for a loop. */
struct LifetimeLoop
{
    /* Offset of the head of the loop. */
    uint32 head;

    /*
     * Offset of the unique jump going to the head of the loop. The code
     * between the head and the backedge forms the loop body.
     */
    uint32 backedge;

    /* Target offset of the initial jump or fallthrough into the loop. */
    uint32 entry;

    /*
     * Start of the last basic block in the loop, excluding the initial jump to
     * entry. All code between lastBlock and the backedge runs in every
     * iteration, and if entry >= lastBlock all code between entry and the
     * backedge runs when the loop is initially entered.
     */
    uint32 lastBlock;

    /*
     * Any inequality known to hold at the head of the loop. This has the
     * form 'lhs <= rhs + constant' or 'lhs >= rhs + constant', depending on
     * lessEqual. The lhs may be modified within the loop body (the test is
     * invalid afterwards), and the rhs is invariant. This information is only
     * valid if the LHS/RHS are known integers.
     */
    enum { UNASSIGNED = uint32(-1) };
    uint32 testLHS;
    uint32 testRHS;
    int32 testConstant;
    bool testLessEqual;

    /*
     * A variable which will be incremented or decremented exactly once in each
     * iteration of the loop. The offset of the operation is indicated, which
     * may or may not run after the initial entry into the loop.
     */
    struct Increment {
        uint32 slot;
        uint32 offset;
    };
    Increment *increments;
    uint32 nIncrements;

    /* It is unknown which arrays grow or which objects are modified in this loop. */
    bool unknownModset;

    /*
     * This loop contains safe points in its body (which the interpreter might
     * join at directly.
     */
    bool hasSafePoints;

    /*
     * Arrays which might grow during this loop. This is a guess, and may
     * underapproximate the actual set of such arrays.
     */
    types::TypeObject **growArrays;
    uint32 nGrowArrays;
};

/* Lifetime and register information for a bytecode. */
struct LifetimeBytecode
{
    /* If this is a loop head, information about the loop. */
    LifetimeLoop *loop;

    /* Any allocation computed downstream for this bytecode. */
    mjit::RegisterAllocation *allocation;
};

/* Current lifetime information for a variable. */
struct LifetimeVariable
{
    /* If the variable is currently live, the lifetime segment. */
    Lifetime *lifetime;

    /* If the variable is currently dead, the next live segment. */
    Lifetime *saved;

    /* Jump preceding the basic block which killed this variable. */
    uint32 savedEnd;

    /* Whether this variable is live at offset. */
    Lifetime * live(uint32 offset) {
        if (lifetime && lifetime->end >= offset)
            return lifetime;
        Lifetime *segment = lifetime ? lifetime : saved;
        while (segment && segment->start <= offset) {
            if (segment->end >= offset)
                return segment;
            segment = segment->next;
        }
        return NULL;
    }

    /*
     * Get the offset of the first write to the variable in the body of a loop,
     * -1 if the loop never writes the variable.
     */
    uint32 firstWrite(LifetimeLoop *loop) {
        Lifetime *segment = lifetime ? lifetime : saved;
        while (segment && segment->start <= loop->backedge) {
            if (segment->start >= loop->head && segment->write)
                return segment->start;
            segment = segment->next;
        }
        return uint32(-1);
    }

    /*
     * If the variable is only written once in the body of a loop, offset of
     * that write. -1 otherwise.
     */
    uint32 onlyWrite(LifetimeLoop *loop) {
        uint32 offset = uint32(-1);
        Lifetime *segment = lifetime ? lifetime : saved;
        while (segment && segment->start <= loop->backedge) {
            if (segment->start >= loop->head && segment->write) {
                if (offset != uint32(-1))
                    return uint32(-1);
                offset = segment->start;
            }
            segment = segment->next;
        }
        return offset;
    }
};

/*
 * Analysis approximating variable liveness information at points in a script.
 * This is separate from analyze::Script as it is computed on every compilation
 * and thrown away afterwards.
 */
class LifetimeScript
{
    analyze::Script *analysis;
    JSScript *script;

    LifetimeBytecode *codeArray;
    LifetimeVariable *lifetimes;
    uint32 nLifetimes;

    LifetimeVariable **saved;
    unsigned savedCount;

  public:
    JSArenaPool pool;

    LifetimeScript();
    ~LifetimeScript();

    bool analyze(JSContext *cx, analyze::Script *analysis, JSScript *script);

    LifetimeBytecode &getCode(uint32 offset) {
        JS_ASSERT(analysis->maybeCode(offset));
        return codeArray[offset];
    }
    LifetimeBytecode &getCode(jsbytecode *pc) { return getCode(pc - script->code); }

#ifdef DEBUG
    void dumpVariable(LifetimeVariable &var);
    void dumpSlot(unsigned slot) {
        JS_ASSERT(slot < nLifetimes);
        dumpVariable(lifetimes[slot]);
    }
#endif

    Lifetime * live(uint32 slot, uint32 offset) {
        JS_ASSERT(slot < nLifetimes);
        return lifetimes[slot].live(offset);
    }

    uint32 firstWrite(uint32 slot, LifetimeLoop *loop) {
        JS_ASSERT(slot < nLifetimes);
        return lifetimes[slot].firstWrite(loop);
    }

    uint32 onlyWrite(uint32 slot, LifetimeLoop *loop) {
        JS_ASSERT(slot < nLifetimes);
        return lifetimes[slot].onlyWrite(loop);
    }

    /*
     * Note: loop analysis depends on the function not having had indirect
     * modification of its arguments. Clients must watch for this.
     */
    void analyzeLoopTest(LifetimeLoop *loop);
    bool analyzeLoopIncrements(JSContext *cx, LifetimeLoop *loop);
    bool analyzeLoopModset(JSContext *cx, LifetimeLoop *loop);

  private:

    inline bool addVariable(JSContext *cx, LifetimeVariable &var, unsigned offset);
    inline bool killVariable(JSContext *cx, LifetimeVariable &var, unsigned offset);
    inline bool extendVariable(JSContext *cx, LifetimeVariable &var, unsigned start, unsigned end);

    bool loopVariableAccess(LifetimeLoop *loop, jsbytecode *pc);
    bool getLoopTestAccess(jsbytecode *pc, uint32 *slotp, int32 *constantp);
};

} /* namespace analyze */
} /* namespace js */

#endif // jsanalyze_h___
