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

#if !defined jsjaeger_framestate_h__ && defined JS_METHODJIT
#define jsjaeger_framestate_h__

#include "jsapi.h"
#include "methodjit/MachineRegs.h"
#include "methodjit/FrameEntry.h"
#include "CodeGenIncludes.h"
#include "ImmutableSync.h"

namespace js {
namespace mjit {

/*
 * The FrameState keeps track of values on the frame during compilation.
 * The compiler can query FrameState for information about arguments, locals,
 * and stack slots (all hereby referred to as "slots"). Slot information can
 * be requested in constant time. For each slot there is a FrameEntry *. If
 * this is non-NULL, it contains valid information and can be returned.
 *
 * The register allocator keeps track of registers as being in one of two
 * states. These are:
 *
 * 1) Unowned. Some code in the compiler is working on a register.
 * 2) Owned. The FrameState owns the register, and may spill it at any time.
 *
 * ------------------ Implementation Details ------------------
 * 
 * Observations:
 *
 * 1) We totally blow away known information quite often; branches, merge points.
 * 2) Every time we need a slow call, we must sync everything.
 * 3) Efficient side-exits need to quickly deltize state snapshots.
 * 4) Syncing is limited to constants and registers.
 * 5) Once a value is tracked, there is no reason to "forget" it until #1.
 * 
 * With these in mind, we want to make sure that the compiler doesn't degrade
 * badly as functions get larger.
 *
 * If the FE is NULL, a new one is allocated, initialized, and stored. They
 * are allocated from a pool such that (fe - pool) can be used to compute
 * the slot's Address.
 *
 * We keep a side vector of all tracked FrameEntry * to quickly generate
 * memory stores and clear the tracker.
 *
 * It is still possible to get really bad behavior with a very large script
 * that doesn't have branches or calls. That's okay, having this code in
 * minimizes damage and lets us introduce a hard cut-off point.
 */
class FrameState
{
    friend class ImmutableSync;

    typedef JSC::MacroAssembler::RegisterID RegisterID;
    typedef JSC::MacroAssembler::FPRegisterID FPRegisterID;
    typedef JSC::MacroAssembler::Address Address;
    typedef JSC::MacroAssembler::Jump Jump;
    typedef JSC::MacroAssembler::Imm32 Imm32;

    static const uint32 InvalidIndex = 0xFFFFFFFF;

    struct Tracker {
        Tracker()
          : entries(NULL), nentries(0)
        { }

        void add(FrameEntry *fe) {
            entries[nentries++] = fe;
        }

        void reset() {
            nentries = 0;
        }

        FrameEntry * operator [](uint32 n) const {
            JS_ASSERT(n < nentries);
            return entries[n];
        }

        FrameEntry **entries;
        uint32 nentries;
    };

    struct RegisterState {
        RegisterState()
        { }

        RegisterState(FrameEntry *fe, RematInfo::RematType type, bool weak)
          : fe(fe), type(type), weak(weak)
        { }

        /* FrameEntry owning this register, or NULL if not owned by a frame. */
        FrameEntry *fe;

        /* Hack - simplifies register allocation for pairs. */
        FrameEntry *save;
        
        /* Part of the FrameEntry that owns the FE. */
        RematInfo::RematType type;

        /* Weak means it is easily spillable. */
        bool weak;
    };

  public:
    FrameState(JSContext *cx, JSScript *script, Assembler &masm);
    ~FrameState();
    bool init(uint32 nargs);

    /*
     * Pushes a synced slot.
     */
    inline void pushSynced();

    /*
     * Pushes a slot that has a known, synced type and payload.
     */
    inline void pushSyncedType(JSValueMask32 tag);

    /*
     * Pushes a slot that has a known, synced type and payload.
     */
    inline void pushSynced(JSValueMask32 tag, RegisterID reg);

    /*
     * Pushes a constant value.
     */
    inline void push(const Value &v);

    /*
     * Loads a value from memory and pushes it.
     */
    inline void push(Address address);

    /*
     * Pushes a known type and allocated payload onto the operation stack.
     */
    inline void pushTypedPayload(JSValueMask32 tag, RegisterID payload);

    /*
     * Pushes a type register and data register pair.
     */
    inline void pushRegs(RegisterID type, RegisterID data);

    /*
     * Pushes a known type and allocated payload onto the operation stack.
     * This must be used when the type is known, but cannot be propagated
     * because it is not known to be correct at a slow-path merge point.
     */
    inline void pushUntypedPayload(JSValueMask32 tag, RegisterID payload);

    /*
     * Pops a value off the operation stack, freeing any of its resources.
     */
    inline void pop();

    /*
     * Pops a number of values off the operation stack, freeing any of their
     * resources.
     */
    inline void popn(uint32 n);

    /*
     * Temporarily increase and decrease local variable depth.
     */
    inline void enterBlock(uint32 n);
    inline void leaveBlock(uint32 n);

    /*
     * Pushes a copy of a local variable.
     */
    void pushLocal(uint32 n);

    /*
     * Allocates a temporary register for a FrameEntry's type. The register
     * can be spilled or clobbered by the frame. The compiler may only operate
     * on it temporarily, and must take care not to clobber it.
     */
    inline RegisterID tempRegForType(FrameEntry *fe);

    /*
     * Returns a register that is guaranteed to contain the frame entry's
     * data payload. The compiler may not modify the contents of the register,
     * though it may explicitly free it.
     */
    inline RegisterID tempRegForData(FrameEntry *fe);

    /*
     * Same as above, except register must match identically.
     */
    inline RegisterID tempRegForData(FrameEntry *fe, RegisterID reg);

    /*
     * Returns a register that contains the constant value of the
     * frame entry's data payload.
     */
    inline RegisterID tempRegForConstant(FrameEntry *fe);

    /*
     * Allocates a register for a FrameEntry's data, such that the compiler
     * can modify it in-place.
     *
     * The caller guarantees the FrameEntry will not be observed again. This
     * allows the compiler to avoid spilling. Only call this if the FE is
     * going to be popped before stubcc joins/guards or the end of the current
     * opcode.
     */
    RegisterID ownRegForData(FrameEntry *fe);

    /*
     * Allocates a register for a FrameEntry's type, such that the compiler
     * can modify it in-place.
     *
     * The caller guarantees the FrameEntry will not be observed again. This
     * allows the compiler to avoid spilling. Only call this if the FE is
     * going to be popped before stubcc joins/guards or the end of the current
     * opcode.
     */
    RegisterID ownRegForType(FrameEntry *fe);

    /*
     * Allocates a register for a FrameEntry's data, such that the compiler
     * can modify it in-place. The actual FE is not modified.
     */
    RegisterID copyDataIntoReg(FrameEntry *fe);

    /*
     * Types don't always have to be in registers, sometimes the compiler
     * can use addresses and avoid spilling. If this FrameEntry has a synced
     * address and no register, this returns true.
     */
    inline bool shouldAvoidTypeRemat(FrameEntry *fe);

    /*
     * Payloads don't always have to be in registers, sometimes the compiler
     * can use addresses and avoid spilling. If this FrameEntry has a synced
     * address and no register, this returns true.
     */
    inline bool shouldAvoidDataRemat(FrameEntry *fe);

    /*
     * Frees a temporary register. If this register is being tracked, then it
     * is not spilled; the backing data becomes invalidated!
     */
    inline void freeReg(RegisterID reg);

    /*
     * Allocates a register. If none are free, one may be spilled from the
     * tracker. If there are none available for spilling in the tracker,
     * then this is considered a compiler bug and an assert will fire.
     */
    inline RegisterID allocReg();

    /*
     * Allocates a register, except using a mask.
     */
    inline RegisterID allocReg(uint32 mask);

    /*
     * Allocates a specific register, evicting it if it's not avaliable.
     */
    void takeReg(RegisterID reg);

    /*
     * Returns a FrameEntry * for a slot on the operation stack.
     */
    inline FrameEntry *peek(int32 depth);

    /*
     * Fully stores a FrameEntry at an arbitrary address. popHint specifies
     * how hard the register allocator should try to keep the FE in registers.
     */
    void storeTo(FrameEntry *fe, Address address, bool popHint);

    /*
     * Stores the top stack slot back to a local variable.
     */
    void storeLocal(uint32 n);

    /*
     * Restores state from a slow path.
     */
    void merge(Assembler &masm, uint32 ivD) const;

    /*
     * Writes unsynced stores to an arbitrary buffer.
     */
    void sync(Assembler &masm) const;

    /*
     * Syncs all outstanding stores to memory and possibly kills regs in the
     * process.
     */
    void syncAndKill(uint32 mask); 

    /*
     * Clear all tracker entries, syncing all outstanding stores in the process.
     * The stack depth is in case some merge points' edges did not immediately
     * precede the current instruction.
     */
    inline void forgetEverything(uint32 newStackDepth);

    /*
     * Same as above, except the stack depth is not changed. This is used for
     * branching opcodes.
     */
    void forgetEverything();

    /*
     * Throw away the entire frame state, without syncing anything.
     */
    void throwaway();

    /*
     * Mark an existing slot with a type.
     */
    inline void learnType(FrameEntry *fe, JSValueMask32 tag);

    /*
     * Forget a type, syncing in the process.
     */
    inline void forgetType(FrameEntry *fe);

    /*
     * Helper function. Tests if a slot's type is an integer. Condition should
     * be Equal or NotEqual.
     */
    inline Jump testInt32(Assembler::Condition cond, FrameEntry *fe);

    /*
     * Helper function. Tests if a slot's type is a double. Condition should
     * be Equal or Not Equal.
     */
    inline Jump testDouble(Assembler::Condition cond, FrameEntry *fe);

    /*
     * Helper function. Tests if a slot's type is an integer. Condition should
     * be Equal or NotEqual.
     */
    inline Jump testBoolean(Assembler::Condition cond, FrameEntry *fe);

    /*
     * Helper function. Tests if a slot's type is a non-funobj. Condition should
     * be Equal or NotEqual.
     */
    inline Jump testNonFunObj(Assembler::Condition cond, FrameEntry *fe);

    /*
     * Marks a register such that it cannot be spilled by the register
     * allocator. Any pinned registers must be unpinned at the end of the op.
     * Note: This function should only be used on registers tied to FEs.
     */
    inline void pinReg(RegisterID reg);

    /*
     * Unpins a previously pinned register.
     */
    inline void unpinReg(RegisterID reg);

    /*
     * Dups the top item on the stack.
     */
    inline void dup();

    /*
     * Dups the top 2 items on the stack.
     */
    inline void dup2();

    /*
     * Returns the current stack depth of the frame.
     */
    uint32 stackDepth() const { return sp - spBase; }
    uint32 frameDepth() const { return stackDepth() + script->nfixed; }
    inline FrameEntry *tosFe() const;

#ifdef DEBUG
    void assertValidRegisterState() const;
#endif

    Address addressOf(const FrameEntry *fe) const;

    /*
     * This is similar to freeReg(ownRegForData(fe)) - except no movement takes place.
     * The fe is simply invalidated as if it were popped. This can be used to free
     * registers in the working area of the stack. Obviously, this can only be called
     * in infallible code that will pop these entries soon after.
     */
    inline void eviscerate(FrameEntry *fe);

    /*
     * Moves the top of the stack down N slots, popping each item above it.
     * Caller guarantees the slots below have been observed and eviscerated.
     */
    void shimmy(uint32 n);

  private:
    inline RegisterID allocReg(FrameEntry *fe, RematInfo::RematType type, bool weak);
    inline void forgetReg(RegisterID reg);
    RegisterID evictSomeReg(uint32 mask);
    void evictReg(RegisterID reg);
    inline FrameEntry *rawPush();
    inline FrameEntry *addToTracker(uint32 index);
    inline void syncType(const FrameEntry *fe, Address to, Assembler &masm) const;
    inline void syncData(const FrameEntry *fe, Address to, Assembler &masm) const;
    inline FrameEntry *getLocal(uint32 slot);
    inline void forgetAllRegs(FrameEntry *fe);
    inline void swapInTracker(FrameEntry *lhs, FrameEntry *rhs);
    inline uint32 localIndex(uint32 n);
    void pushCopyOf(uint32 index);
    void syncFancy(Assembler &masm, Registers avail, uint32 resumeAt) const;

    /*
     * "Uncopies" the backing store of a FrameEntry that has been copied. The
     * original FrameEntry is not invalidated; this is the responsibility of
     * the caller. The caller can check isCopied() to see if the registers
     * were moved to a copy.
     */
    void uncopy(FrameEntry *original);

    FrameEntry *entryFor(uint32 index) const {
        JS_ASSERT(base[index]);
        return &entries[index];
    }

    void moveOwnership(RegisterID reg, FrameEntry *newFe) {
        regstate[reg].fe = newFe;
    }

    RegisterID evictSomeReg() {
        return evictSomeReg(Registers::AvailRegs);
    }

    uint32 indexOf(int32 depth) {
        return uint32((sp + depth) - base);
    }

    uint32 indexOfFe(FrameEntry *fe) {
        return uint32(fe - entries);
    }

  private:
    JSContext *cx;
    JSScript *script;
    uint32 nargs;
    Assembler &masm;

    /* All allocated registers. */
    Registers freeRegs;

    /* Cache of FrameEntry objects. */
    FrameEntry *entries;

    /* Base pointer of the FrameEntry vector. */
    FrameEntry **base;

    /* Base pointer for arguments. */
    FrameEntry **args;

    /* Base pointer for local variables. */
    FrameEntry **locals;

    /* Base pointer for the stack. */
    FrameEntry **spBase;

    /* Dynamic stack pointer. */
    FrameEntry **sp;

    /* Vector of tracked slot indexes. */
    Tracker tracker;

    /*
     * Register ownership state. This can't be used alone; to find whether an
     * entry is active, you must check the allocated registers.
     */
    RegisterState regstate[Assembler::TotalRegisters];

    mutable ImmutableSync reifier;
};

} /* namespace mjit */
} /* namespace js */

#endif /* jsjaeger_framestate_h__ */

