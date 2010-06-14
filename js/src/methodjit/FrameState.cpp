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
#include "jscntxt.h"
#include "FrameState.h"
#include "FrameState-inl.h"

using namespace js;
using namespace js::mjit;

/* Because of Value alignment */
JS_STATIC_ASSERT(sizeof(FrameEntry) % 8 == 0);

FrameState::FrameState(JSContext *cx, JSScript *script, Assembler &masm)
  : cx(cx), script(script), masm(masm), entries(NULL)
{
}

FrameState::~FrameState()
{
    cx->free(entries);
}

bool
FrameState::init(uint32 nargs)
{
    this->nargs = nargs;

    uint32 nslots = script->nslots + nargs;
    if (!nslots) {
        sp = spBase = locals = args = base = NULL;
        return true;
    }

    uint8 *cursor = (uint8 *)cx->malloc(sizeof(FrameEntry) * nslots +       // entries[]
                                        sizeof(FrameEntry *) * nslots +     // base[]
                                        sizeof(FrameEntry *) * nslots       // tracker.entries[]
                                        );
    if (!cursor)
        return false;

    entries = (FrameEntry *)cursor;
    cursor += sizeof(FrameEntry) * nslots;

    base = (FrameEntry **)cursor;
    args = base;
    locals = base + nargs;
    spBase = locals + script->nfixed;
    sp = spBase;
    memset(base, 0, sizeof(FrameEntry *) * nslots);
    cursor += sizeof(FrameEntry *) * nslots;

    tracker.entries = (FrameEntry **)cursor;

    return true;
}

void
FrameState::takeReg(RegisterID reg)
{
    if (freeRegs.hasReg(reg)) {
        freeRegs.takeReg(reg);
    } else {
        JS_ASSERT(regstate[reg].fe);
        evictReg(reg);
        regstate[reg].fe = NULL;
    }
}

void
FrameState::evictReg(RegisterID reg)
{
    FrameEntry *fe = regstate[reg].fe;

    if (regstate[reg].type == RematInfo::TYPE) {
        if (!fe->type.synced()) {
            syncType(fe, addressOf(fe), masm);
            fe->type.sync();
        }
        fe->type.setMemory();
    } else {
        if (!fe->data.synced()) {
            syncData(fe, addressOf(fe), masm);
            fe->data.sync();
        }
        fe->data.setMemory();
    }
}

JSC::MacroAssembler::RegisterID
FrameState::evictSomething(uint32 mask)
{
#ifdef DEBUG
    bool fallbackSet = false;
#endif
    RegisterID fallback = Registers::ReturnReg;

    for (uint32 i = 0; i < JSC::MacroAssembler::TotalRegisters; i++) {
        RegisterID reg = RegisterID(i);

        /* Register is not allocatable, don't bother.  */
        if (!(Registers::maskReg(reg) & mask))
            continue;

        /* Register is not owned by the FrameState. */
        FrameEntry *fe = regstate[i].fe;
        if (!fe)
            continue;

        /* Try to find a candidate... that doesn't need spilling. */
#ifdef DEBUG
        fallbackSet = true;
#endif
        fallback = reg;

        if (regstate[i].type == RematInfo::TYPE && fe->type.synced()) {
            fe->type.setMemory();
            return fallback;
        }
        if (regstate[i].type == RematInfo::DATA && fe->data.synced()) {
            fe->data.setMemory();
            return fallback;
        }
    }

    JS_ASSERT(fallbackSet);

    evictReg(fallback);
    return fallback;
}

void
FrameState::forgetEverything()
{
    syncAndKill(Registers::AvailRegs);

    for (uint32 i = 0; i < tracker.nentries; i++)
        base[indexOfFe(tracker[i])] = NULL;

    tracker.reset();
    freeRegs.reset();
}

void
FrameState::storeTo(FrameEntry *fe, Address address, bool popped)
{
    if (fe->isConstant()) {
        masm.storeValue(fe->getValue(), address);
        return;
    }

    if (fe->isCopy())
        fe = fe->copyOf();

    /* Cannot clobber the address's register. */
    JS_ASSERT(!freeRegs.hasReg(address.base));

    if (fe->data.inRegister()) {
        masm.storeData32(fe->data.reg(), address);
    } else {
        JS_ASSERT(fe->data.inMemory());
        RegisterID reg = popped ? alloc() : alloc(fe, RematInfo::DATA, true);
        masm.loadData32(addressOf(fe), reg);
        masm.storeData32(reg, address);
        if (popped)
            freeReg(reg);
        else
            fe->data.setRegister(reg);
    }

    if (fe->isTypeKnown()) {
        masm.storeTypeTag(ImmTag(fe->getTypeTag()), address);
    } else if (fe->type.inRegister()) {
        masm.storeTypeTag(fe->type.reg(), address);
    } else {
        JS_ASSERT(fe->type.inMemory());
        RegisterID reg = popped ? alloc() : alloc(fe, RematInfo::TYPE, true);
        masm.loadTypeTag(addressOf(fe), reg);
        masm.storeTypeTag(reg, address);
        if (popped)
            freeReg(reg);
        else
            fe->type.setRegister(reg);
    }
}

#ifdef DEBUG
void
FrameState::assertValidRegisterState() const
{
    Registers checkedFreeRegs;

    FrameEntry *tos = tosFe();
    for (uint32 i = 0; i < tracker.nentries; i++) {
        FrameEntry *fe = tracker[i];
        if (fe >= tos)
            continue;

        JS_ASSERT(i == fe->trackerIndex());
        JS_ASSERT_IF(fe->isCopy(),
                     fe->trackerIndex() > fe->copyOf()->trackerIndex());
        JS_ASSERT_IF(fe->isCopy(), !fe->type.inRegister() && !fe->data.inRegister());

        if (fe->isCopy())
            continue;
        if (fe->type.inRegister()) {
            checkedFreeRegs.takeReg(fe->type.reg());
            JS_ASSERT(regstate[fe->type.reg()].fe == fe);
        }
        if (fe->data.inRegister()) {
            checkedFreeRegs.takeReg(fe->data.reg());
            JS_ASSERT(regstate[fe->data.reg()].fe == fe);
        }
    }

    JS_ASSERT(checkedFreeRegs == freeRegs);
}
#endif

namespace js {
namespace mjit {

struct SyncRegInfo {
    FrameEntry *fe;
    RematInfo::RematType type;
    bool synced;
};

/*
 * While emitting sync code from the fast path to the slow path, we may get
 * a situation where a copy's backing store has no register. If we run into
 * this situation, the structure below is used to allocate registers.
 *
 * While walking the tracker, we remember all registers that have been sunk,
 * and can thus be clobbered. These are given out on a first-come, first-serve
 * basis. If none are available, one is evicted (explained later).
 *
 * After allocating a register, it is forced into the FrameEntry's RematInfo,
 * _despite_ this whole process being immutable with respect to the tracker.
 * This is so further copies of the FE can easily re-use the register.
 *
 * Once the backing store is reached (guaranteed to be after all copies,
 * thanks to the tracker ordering), the registers are placed back into the
 * free bucket. Here we necessarily undo the mucking from above. If the FE
 * never actually owned the register, we restore the fact that it was really
 * in memory.
 */
struct SyncRegs {
    typedef JSC::MacroAssembler::RegisterID RegisterID;

    SyncRegs(const FrameState &frame, Assembler &masm, Registers avail)
      : frame(frame), masm(masm), avail(avail)
    {
        memset(regs, 0, sizeof(regs));
    }

    bool shouldSyncData(FrameEntry *fe) {
        if (fe->data.synced())
            return false;
        if (!fe->data.inRegister())
            return true;
        return !regs[fe->data.reg()].synced;
    }

    bool shouldSyncType(FrameEntry *fe) {
        if (fe->type.synced())
            return false;
        if (!fe->type.inRegister())
            return true;
        return !regs[fe->type.reg()].synced;
    }

    void giveTypeReg(FrameEntry *fe) {
        JS_ASSERT(fe->isCopied());
        RegisterID reg = allocFor(fe, RematInfo::TYPE);
        masm.loadTypeTag(frame.addressOf(fe), reg);
        fe->type.setRegister(reg);
    }

    void giveDataReg(FrameEntry *fe) {
        JS_ASSERT(fe->isCopied());
        RegisterID reg = allocFor(fe, RematInfo::DATA);
        masm.loadData32(frame.addressOf(fe), reg);
        fe->data.setRegister(reg);
    }

    void forget(FrameEntry *fe) {
        JS_ASSERT(!fe->isCopy());
        if (fe->type.inRegister()) {
            if (forgetReg(fe, RematInfo::TYPE, fe->type.reg()))
                fe->type.setMemory();
        }
        if (fe->data.inRegister()) {
            if (forgetReg(fe, RematInfo::DATA, fe->data.reg()))
                fe->data.setMemory();
        }
    }

  private:
    RegisterID allocFor(FrameEntry *fe, RematInfo::RematType type) {
        RegisterID reg;
        if (!avail.empty())
            reg = avail.takeAnyReg();
        else
            reg = evict();

        regs[reg].fe = fe;
        regs[reg].type = type;

        return reg;
    }

    RegisterID evict() {
        /*
         * Worst case. This register is backed by a synced FE that might back
         * copies. Evicting it could result in a load.
         */
        uint32 any = FrameState::InvalidIndex;
        uint32 worst = FrameState::InvalidIndex;

        /*
         * Next best case. This register backs an unsynced FE on the fast
         * path, and clobbering it will emit a store early.
         */
        uint32 nbest = FrameState::InvalidIndex;

        for (uint32 i = 0; i < Assembler::TotalRegisters; i++) {
            RegisterID reg = RegisterID(i);
            if (!(Registers::maskReg(reg) & Registers::AvailRegs))
                continue;

            any = i;

            FrameEntry *myFe = regs[reg].fe;
            if (!myFe) {
                FrameEntry *fe = frame.regstate[reg].fe;
                if (!fe)
                    continue;

                if (!fe->isCopied())
                    nbest = i;

                if (frame.regstate[reg].type == RematInfo::TYPE && fe->type.synced())
                    return reg;
                else if (frame.regstate[reg].type == RematInfo::DATA && fe->data.synced())
                    return reg;
            } else {
                worst = i;
            }
        }

        /* We can early-sync another FE and grab its register. */
        if (nbest != FrameState::InvalidIndex) {
            FrameEntry *fe = frame.regstate[nbest].fe;
            RematInfo::RematType type = frame.regstate[nbest].type;
            if (type == RematInfo::TYPE)
                frame.syncType(fe, frame.addressOf(fe), masm);
            else
                frame.syncData(fe, frame.addressOf(fe), masm);
            regs[nbest].synced = true;
            return RegisterID(nbest);
        }

        if (worst != FrameState::InvalidIndex) {
            JS_ASSERT(regs[worst].fe);
            if (regs[worst].type == RematInfo::TYPE)
                regs[worst].fe->type.setMemory();
            else
                regs[worst].fe->data.setMemory();
            regs[worst].fe = NULL;
            return RegisterID(worst);
        }

        JS_NOT_REACHED("wat");

        return RegisterID(worst);
    }

    /* Returns true if had an fe */
    bool forgetReg(FrameEntry *checkFe, RematInfo::RematType type, RegisterID reg) {
        /*
         * Unchecked, because evict() can steal registers of synced FEs before
         * the tracker has a chance to put them in the free list. This is
         * harmless, and just avoids an assert.
         */
        avail.putRegUnchecked(reg);

        /*
         * It is necessary to check the type as well, for example an FE could
         * have its type synced to EDI, and data unsynced. EDI could be
         * evicted, but we only want to reset data, not type.
         *
         * The control flow could be better here, maybe this check should be
         * in forgetRegs?
         */
        FrameEntry *fe = regs[reg].fe;
        if (!fe || fe != checkFe || regs[reg].type != type)
            return false;

        regs[reg].fe = NULL;

        return true;
    }

    const FrameState &frame;
    Assembler &masm;
    SyncRegInfo regs[Assembler::TotalRegisters];
    Registers avail;
};

} } /* namespace js, mjit */

void
FrameState::syncFancy(Assembler &masm, Registers avail, uint32 resumeAt) const
{
    SyncRegs sr(*this, masm, avail);

    FrameEntry *tos = tosFe();
    for (uint32 i = resumeAt; i < tracker.nentries; i--) {
        FrameEntry *fe = tracker[i];
        if (fe >= tos)
            continue;

        Address address = addressOf(fe);

        if (!fe->isCopy()) {
            sr.forget(fe);

            if (sr.shouldSyncData(fe)) {
                syncData(fe, address, masm);
                if (fe->isConstant())
                    continue;
            }
            if (sr.shouldSyncType(fe))
                syncType(fe, addressOf(fe), masm);
        } else {
            FrameEntry *backing = fe->copyOf();
            JS_ASSERT(backing != fe);
            JS_ASSERT(!backing->isConstant() && !fe->isConstant());

            if (!fe->type.synced()) {
                /* :TODO: we can do better, the type is learned for all copies. */
                if (fe->isTypeKnown()) {
                    //JS_ASSERT(fe->getTypeTag() == backing->getTypeTag());
                    masm.storeTypeTag(ImmTag(fe->getTypeTag()), address);
                } else {
                    if (!backing->type.inRegister())
                        sr.giveTypeReg(backing);
                    masm.storeTypeTag(backing->type.reg(), address);
                }
            }

            if (!fe->data.synced()) {
                if (!backing->data.inRegister())
                    sr.giveDataReg(backing);
                masm.storeData32(backing->data.reg(), address);
            }
        }
    }
}

void
FrameState::sync(Assembler &masm) const
{
    /*
     * Keep track of free registers using a bitmask. If we have to drop into
     * syncFancy(), then this mask will help avoid eviction.
     */
    Registers avail(freeRegs);

    FrameEntry *tos = tosFe();
    for (uint32 i = tracker.nentries - 1; i < tracker.nentries; i--) {
        FrameEntry *fe = tracker[i];
        if (fe >= tos)
            continue;

        Address address = addressOf(fe);

        if (!fe->isCopy()) {
            /* Keep track of registers that can be clobbered. */
            if (fe->data.inRegister())
                avail.putReg(fe->data.reg());
            if (fe->type.inRegister())
                avail.putReg(fe->type.reg());

            /* Sync. */
            if (!fe->data.synced()) {
                syncData(fe, address, masm);
                if (fe->isConstant())
                    continue;
            }
            if (!fe->type.synced())
                syncType(fe, addressOf(fe), masm);
        } else {
            FrameEntry *backing = fe->copyOf();
            JS_ASSERT(backing != fe);
            JS_ASSERT(!backing->isConstant() && !fe->isConstant());

            /*
             * If the copy is backed by something not in a register, fall back
             * to a slower sync algorithm.
             */
            if ((!fe->type.synced() && !fe->type.inRegister()) ||
                (!fe->data.synced() && !fe->data.inRegister())) {
                syncFancy(masm, avail, i);
                return;
            }

            if (!fe->type.synced()) {
                /* :TODO: we can do better, the type is learned for all copies. */
                if (fe->isTypeKnown()) {
                    //JS_ASSERT(fe->getTypeTag() == backing->getTypeTag());
                    masm.storeTypeTag(ImmTag(fe->getTypeTag()), address);
                } else {
                    masm.storeTypeTag(backing->type.reg(), address);
                }
            }

            if (!fe->data.synced())
                masm.storeData32(backing->data.reg(), address);
        }
    }
}

void
FrameState::syncAndKill(uint32 mask)
{
    Registers kill(mask);

    /* Backwards, so we can allocate registers to backing slots better. */
    FrameEntry *tos = tosFe();
    for (uint32 i = tracker.nentries - 1; i < tracker.nentries; i--) {
        FrameEntry *fe = tracker[i];
        if (fe >= tos)
            continue;

        Address address = addressOf(fe);
        FrameEntry *backing = fe;
        if (fe->isCopy())
            backing = fe->copyOf();

        JS_ASSERT_IF(i == 0, !fe->isCopy());

        if (!fe->data.synced()) {
            if (backing != fe && backing->data.inMemory())
                tempRegForData(backing);
            syncData(backing, address, masm);
            fe->data.sync();
            if (fe->isConstant() && !fe->type.synced())
                fe->type.sync();
        }
        if (fe->data.inRegister() && kill.hasReg(fe->data.reg())) {
            JS_ASSERT(backing == fe);
            JS_ASSERT(fe->data.synced());
            forgetReg(fe->data.reg());
            fe->data.setMemory();
        }
        if (!fe->type.synced()) {
            if (backing != fe && backing->type.inMemory())
                tempRegForType(backing);
            syncType(backing, address, masm);
            fe->type.sync();
        }
        if (fe->type.inRegister() && kill.hasReg(fe->type.reg())) {
            JS_ASSERT(backing == fe);
            JS_ASSERT(fe->type.synced());
            forgetReg(fe->type.reg());
            fe->type.setMemory();
        }
    }
}

void
FrameState::merge(Assembler &masm, uint32 iVD) const
{
    FrameEntry *tos = tosFe();
    for (uint32 i = 0; i < tracker.nentries; i++) {
        FrameEntry *fe = tracker[i];
        if (fe >= tos)
            continue;

        /* Copies do not have registers. */
        if (fe->isCopy()) {
            JS_ASSERT(!fe->data.inRegister());
            JS_ASSERT(!fe->type.inRegister());
            continue;
        }

        if (fe->data.inRegister())
            masm.loadData32(addressOf(fe), fe->data.reg());
        if (fe->type.inRegister())
            masm.loadTypeTag(addressOf(fe), fe->type.reg());
    }
}

JSC::MacroAssembler::RegisterID
FrameState::copyData(FrameEntry *fe)
{
    JS_ASSERT(!fe->data.isConstant());

    if (fe->isCopy())
        fe = fe->copyOf();

    if (fe->data.inRegister()) {
        RegisterID reg = fe->data.reg();
        if (freeRegs.empty()) {
            if (!fe->data.synced())
                syncData(fe, addressOf(fe), masm);
            fe->data.setMemory();
            regstate[reg].fe = NULL;
        } else {
            RegisterID newReg = alloc();
            masm.move(reg, newReg);
            reg = newReg;
        }
        return reg;
    }

    RegisterID reg = alloc();

    if (!freeRegs.empty())
        masm.move(tempRegForData(fe), reg);
    else
        masm.loadData32(addressOf(fe),reg);

    return reg;
}

JSC::MacroAssembler::RegisterID
FrameState::ownRegForType(FrameEntry *fe)
{
    JS_ASSERT(!fe->type.isConstant());

    RegisterID reg;
    if (fe->isCopy()) {
        /* For now, just do an extra move. The reg must be mutable. */
        FrameEntry *backing = fe->copyOf();
        if (!backing->type.inRegister()) {
            JS_ASSERT(backing->type.inMemory());
            tempRegForType(backing);
        }

        if (freeRegs.empty()) {
            /* For now... just steal the register that already exists. */
            if (!backing->type.synced())
                syncType(backing, addressOf(backing), masm);
            reg = backing->type.reg();
            backing->type.setMemory();
            moveOwnership(reg, NULL);
        } else {
            reg = alloc();
            masm.move(backing->type.reg(), reg);
        }
        return reg;
    }

    if (fe->type.inRegister()) {
        reg = fe->type.reg();
        /* Remove ownership of this register. */
        JS_ASSERT(regstate[reg].fe == fe);
        JS_ASSERT(regstate[reg].type == RematInfo::TYPE);
        regstate[reg].fe = NULL;
        fe->type.invalidate();
    } else {
        JS_ASSERT(fe->type.inMemory());
        reg = alloc();
        masm.loadTypeTag(addressOf(fe), reg);
    }
    return reg;
}

JSC::MacroAssembler::RegisterID
FrameState::ownRegForData(FrameEntry *fe)
{
    JS_ASSERT(!fe->data.isConstant());

    RegisterID reg;
    if (fe->isCopy()) {
        /* For now, just do an extra move. The reg must be mutable. */
        FrameEntry *backing = fe->copyOf();
        if (!backing->data.inRegister()) {
            JS_ASSERT(backing->data.inMemory());
            tempRegForData(backing);
        }

        if (freeRegs.empty()) {
            /* For now... just steal the register that already exists. */
            if (!backing->data.synced())
                syncData(backing, addressOf(backing), masm);
            reg = backing->data.reg();
            backing->data.setMemory();
            moveOwnership(reg, NULL);
        } else {
            reg = alloc();
            masm.move(backing->data.reg(), reg);
        }
        return reg;
    }

    if (fe->isCopied()) {
        uncopy(fe);
        if (fe->isCopied()) {
            reg = alloc();
            masm.loadData32(addressOf(fe), reg);
            return reg;
        }
    }
    
    if (fe->data.inRegister()) {
        reg = fe->data.reg();
        /* Remove ownership of this register. */
        JS_ASSERT(regstate[reg].fe == fe);
        JS_ASSERT(regstate[reg].type == RematInfo::DATA);
        regstate[reg].fe = NULL;
        fe->data.invalidate();
    } else {
        JS_ASSERT(fe->data.inMemory());
        reg = alloc();
        masm.loadData32(addressOf(fe), reg);
    }
    return reg;
}

void
FrameState::pushCopyOf(uint32 index)
{
    FrameEntry *backing = entryFor(index);
    FrameEntry *fe = rawPush();
    fe->resetUnsynced();
    if (backing->isConstant()) {
        fe->setConstant(Jsvalify(backing->getValue()));
    } else {
        if (backing->isTypeKnown())
            fe->setTypeTag(backing->getTypeTag());
        else
            fe->type.invalidate();
        fe->data.invalidate();
        if (backing->isCopy()) {
            backing = backing->copyOf();
            fe->setCopyOf(backing);
        } else {
            fe->setCopyOf(backing);
            backing->setCopied();
        }

        /* Maintain tracker ordering guarantees for copies. */
        JS_ASSERT(backing->isCopied());
        if (fe->trackerIndex() < backing->trackerIndex())
            swapInTracker(fe, backing);
    }
}

void
FrameState::uncopy(FrameEntry *original)
{
    JS_ASSERT(original->isCopied());

    /* Find the first copy. */
    uint32 firstCopy = InvalidIndex;
    FrameEntry *tos = tosFe();
    for (uint32 i = 0; i < tracker.nentries; i++) {
        FrameEntry *fe = tracker[i];
        if (fe >= tos)
            continue;
        if (fe->isCopy() && fe->copyOf() == original) {
            firstCopy = i;
            break;
        }
    }

    if (firstCopy == InvalidIndex) {
        original->copied = false;
        return;
    }

    /* Mark all extra copies as copies of the new backing index. */
    FrameEntry *fe = tracker[firstCopy];

    fe->setCopyOf(NULL);
    for (uint32 i = firstCopy + 1; i < tracker.nentries; i++) {
        FrameEntry *other = tracker[i];
        if (other >= tos)
            continue;

        /* The original must be tracked before copies. */
        JS_ASSERT(other != original);

        if (!other->isCopy() || other->copyOf() != original)
            continue;

        other->setCopyOf(fe);
        fe->setCopied();
    }

    /*
     * Switch the new backing store to the old backing store. During
     * this process we also necessarily make sure the copy can be
     * synced.
     */
    if (!original->isTypeKnown()) {
        /*
         * If the copy is unsynced, and the original is in memory,
         * give the original a register. We do this below too; it's
         * okay if it's spilled.
         */
        if (original->type.inMemory() && !fe->type.synced())
            tempRegForType(original);
        fe->type.inherit(original->type);
        if (fe->type.inRegister())
            moveOwnership(fe->type.reg(), fe);
    } else {
        JS_ASSERT(fe->isTypeKnown());
        JS_ASSERT(fe->getTypeTag() == original->getTypeTag());
    }
    if (original->data.inMemory() && !fe->data.synced())
        tempRegForData(original);
    fe->data.inherit(original->data);
    if (fe->data.inRegister())
        moveOwnership(fe->data.reg(), fe);
}

void
FrameState::storeLocal(uint32 n)
{
    FrameEntry *localFe = getLocal(n);

    /* Detect something like (x = x) which is a no-op. */
    FrameEntry *top = peek(-1);
    if (top->isCopy() && top->copyOf() == localFe) {
        JS_ASSERT(localFe->isCopied());
        return;
    }

    /* Completely invalidate the local variable. */
    if (localFe->isCopied()) {
        uncopy(localFe);
        if (!localFe->isCopied())
            forgetRegs(localFe);
    } else {
        forgetRegs(localFe);
    }

    localFe->resetUnsynced();

    /* Constants are easy to propagate. */
    if (top->isConstant()) {
        localFe->setCopyOf(NULL);
        localFe->setNotCopied();
        localFe->setConstant(Jsvalify(top->getValue()));
        return;
    }

    /*
     * When dealing with copies, there are two important invariants:
     *
     * 1) The backing store precedes all copies in the tracker.
     * 2) The backing store of a local is never a stack slot, UNLESS the local
     *    variable itself is a stack slot (blocks) that precesed the stack
     *    slot.
     *
     * If the top is a copy, and the second condition holds true, the local
     * can be rewritten as a copy of the original backing slot. If the first
     * condition does not hold, force it to hold by swapping in-place.
     */
    FrameEntry *backing = top;
    if (top->isCopy()) {
        backing = top->copyOf();
        JS_ASSERT(backing->trackerIndex() < top->trackerIndex());

        uint32 backingIndex = indexOfFe(backing);
        uint32 tol = uint32(spBase - base);
        if (backingIndex < tol || backingIndex < localIndex(n)) {
            /* local.idx < backing.idx means local cannot be a copy yet */
            if (localFe->trackerIndex() < backing->trackerIndex())
                swapInTracker(backing, localFe);
            localFe->setNotCopied();
            localFe->setCopyOf(backing);
            if (backing->isTypeKnown())
                localFe->setTypeTag(backing->getTypeTag());
            else
                localFe->type.invalidate();
            localFe->data.invalidate();
            return;
        }

        /*
         * If control flow lands here, then there was a bytecode sequence like
         *
         *  ENTERBLOCK 2
         *  GETLOCAL 1
         *  SETLOCAL 0
         *
         * The problem is slot N can't be backed by M if M could be popped
         * before N. We want a guarantee that when we pop M, even if it was
         * copied, it has no outstanding copies.
         * 
         * Because of |let| expressions, it's kind of hard to really know
         * whether a region on the stack will be popped all at once. Bleh!
         *
         * This should be rare except in browser code (and maybe even then),
         * but even so there's a quick workaround. We take all copies of the
         * backing fe, and redirect them to be copies of the destination.
         */
        FrameEntry *tos = tosFe();
        for (uint32 i = backing->trackerIndex() + 1; i < tracker.nentries; i++) {
            FrameEntry *fe = tracker[i];
            if (fe >= tos)
                continue;
            if (fe->isCopy() && fe->copyOf() == backing)
                fe->setCopyOf(localFe);
        }
    }
    backing->setNotCopied();
    
    /*
     * This is valid from the top->isCopy() path because we're guaranteed a
     * consistent ordering - all copies of |backing| are tracked after 
     * |backing|. Transitively, only one swap is needed.
     */
    if (backing->trackerIndex() < localFe->trackerIndex())
        swapInTracker(backing, localFe);

    /*
     * Move the backing store down - we spill registers here, but we could be
     * smarter and re-use the type reg.
     */
    RegisterID reg = tempRegForData(backing);
    localFe->data.setRegister(reg);
    moveOwnership(reg, localFe);

    if (backing->isTypeKnown()) {
        localFe->setTypeTag(backing->getTypeTag());
    } else {
        RegisterID reg = tempRegForType(backing);
        localFe->type.setRegister(reg);
        moveOwnership(reg, localFe);
    }

    if (!backing->isTypeKnown())
        backing->type.invalidate();
    backing->data.invalidate();
    backing->setCopyOf(localFe);
    localFe->setCopied();

    JS_ASSERT(top->copyOf() == localFe);
}


