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
#if !defined jsjaeger_framestate_inl_h__ && defined JS_METHODJIT
#define jsjaeger_framestate_inl_h__

namespace js {
namespace mjit {

inline FrameEntry *
FrameState::addToTracker(uint32 index)
{
    JS_ASSERT(!base[index]);
    FrameEntry *fe = &entries[index];
    base[index] = fe;
    fe->track(tracker.nentries);
    tracker.add(fe);
    return fe;
}

inline FrameEntry *
FrameState::peek(int32 depth)
{
    JS_ASSERT(depth < 0);
    JS_ASSERT(sp + depth >= spBase);
    FrameEntry *fe = sp[depth];
    if (!fe) {
        fe = addToTracker(indexOf(depth));
        fe->resetSynced();
    }
    return fe;
}

inline void
FrameState::popn(uint32 n)
{
    for (uint32 i = 0; i < n; i++)
        pop();
}

inline JSC::MacroAssembler::RegisterID
FrameState::allocReg()
{
    return alloc();
}

inline JSC::MacroAssembler::RegisterID
FrameState::allocReg(uint32 mask)
{
    RegisterID reg;
    if (freeRegs.hasRegInMask(mask))
        reg = freeRegs.takeRegInMask(mask);
    else
        reg = evictSomething(mask);
    regstate[reg].fe = NULL;
    return reg;
}

inline JSC::MacroAssembler::RegisterID
FrameState::alloc()
{
    RegisterID reg;
    if (!freeRegs.empty())
        reg = freeRegs.takeAnyReg();
    else
        reg = evictSomething();
    regstate[reg].fe = NULL;
    return reg;
}

inline JSC::MacroAssembler::RegisterID
FrameState::alloc(FrameEntry *fe, RematInfo::RematType type, bool weak)
{
    RegisterID reg;
    if (!freeRegs.empty())
        reg = freeRegs.takeAnyReg();
    else
        reg = evictSomething();
    regstate[reg] = RegisterState(fe, type, weak);
    return reg;
}

inline void
FrameState::pop()
{
    JS_ASSERT(sp > spBase);

    FrameEntry *fe = *--sp;
    if (!fe)
        return;

    forgetRegs(fe);
}

inline void
FrameState::freeReg(RegisterID reg)
{
    JS_ASSERT(regstate[reg].fe == NULL);
    freeRegs.putReg(reg);
}

inline void
FrameState::forgetReg(RegisterID reg)
{
#ifdef DEBUG
    if (regstate[reg].fe) {
        JS_ASSERT(!regstate[reg].fe->isCopy());
        if (regstate[reg].type == RematInfo::TYPE)
            regstate[reg].fe->type.invalidate();
        else
            regstate[reg].fe->data.invalidate();
    }
#endif
    freeRegs.putReg(reg);
}

inline void
FrameState::forgetEverything(uint32 newStackDepth)
{
    forgetEverything();
    sp = spBase + newStackDepth;
}

inline FrameEntry *
FrameState::rawPush()
{
    sp++;

    if (FrameEntry *fe = sp[-1])
        return fe;

    return addToTracker(&sp[-1] - base);
}

inline void
FrameState::push(const Value &v)
{
    FrameEntry *fe = rawPush();
    fe->setConstant(Jsvalify(v));
}

inline void
FrameState::pushSynced()
{
    sp++;

    if (FrameEntry *fe = sp[-1])
        fe->resetSynced();
}

inline void
FrameState::pushSyncedType(JSValueMask32 tag)
{
    FrameEntry *fe = rawPush();

    fe->resetSynced();
    fe->setTypeTag(tag);
}

inline void
FrameState::pushSynced(JSValueMask32 tag, RegisterID reg)
{
    FrameEntry *fe = rawPush();

    fe->resetUnsynced();
    fe->type.sync();
    fe->data.sync();
    fe->setTypeTag(tag);
    fe->data.setRegister(reg);
    regstate[reg] = RegisterState(fe, RematInfo::DATA, true);
}

inline void
FrameState::push(Address address)
{
    FrameEntry *fe = rawPush();

    /* :XXX: X64 */
    fe->resetUnsynced();

    /* Prevent us from clobbering this reg. */
    bool free = freeRegs.hasReg(address.base);
    if (free)
        freeRegs.takeReg(address.base);

    RegisterID reg = alloc(fe, RematInfo::DATA, true);
    masm.loadData32(address, reg);
    fe->data.setRegister(reg);

    /* Now it's safe to grab this register again. */
    if (free)
        freeRegs.putReg(address.base);

    reg = alloc(fe, RematInfo::TYPE, true);
    masm.loadTypeTag(address, reg);
    fe->type.setRegister(reg);
}

inline void
FrameState::pushTypedPayload(JSValueMask32 tag, RegisterID payload)
{
    JS_ASSERT(!freeRegs.hasReg(payload));

    FrameEntry *fe = rawPush();

    fe->resetUnsynced();
    fe->setTypeTag(tag);
    fe->data.setRegister(payload);
    regstate[payload] = RegisterState(fe, RematInfo::DATA, true);
}

inline void
FrameState::pushUntypedPayload(JSValueMask32 tag, RegisterID payload)
{
    JS_ASSERT(!freeRegs.hasReg(payload));

    FrameEntry *fe = rawPush();

    /* The forceful type sync will assert otherwise. */
#ifdef DEBUG
    fe->type.unsync();
#endif
    fe->clear();

    masm.storeTypeTag(ImmTag(tag), addressOf(fe));
    fe->type.setMemory();
    fe->data.unsync();
    fe->setNotCopied();
    fe->setCopyOf(NULL);
    fe->data.setRegister(payload);
    regstate[payload] = RegisterState(fe, RematInfo::DATA, true);
}

inline JSC::MacroAssembler::RegisterID
FrameState::tempRegForType(FrameEntry *fe)
{
    JS_ASSERT(!fe->type.isConstant());
    if (fe->isCopy())
        fe = fe->copyOf();

    if (fe->type.inRegister())
        return fe->type.reg();

    /* :XXX: X64 */

    RegisterID reg = alloc(fe, RematInfo::TYPE, true);
    masm.loadTypeTag(addressOf(fe), reg);
    fe->type.setRegister(reg);
    return reg;
}

inline JSC::MacroAssembler::RegisterID
FrameState::tempRegForData(FrameEntry *fe)
{
    JS_ASSERT(!fe->data.isConstant());

    if (fe->isCopy())
        fe = fe->copyOf();

    if (fe->data.inRegister())
        return fe->data.reg();

    RegisterID reg = alloc(fe, RematInfo::DATA, true);
    masm.loadData32(addressOf(fe), reg);
    fe->data.setRegister(reg);
    return reg;
}

inline JSC::MacroAssembler::RegisterID
FrameState::tempRegForData(FrameEntry *fe, RegisterID reg)
{
    JS_ASSERT(!fe->data.isConstant());

    if (fe->isCopy())
        fe = fe->copyOf();

    if (fe->data.inRegister()) {
        RegisterID old = fe->data.reg();
        if (old == reg)
            return reg;

        /* Keep the old register pinned. */
        regstate[old].fe = NULL;
        if (!freeRegs.hasReg(reg))
            evictReg(reg);
        else
            freeRegs.takeReg(reg);
        masm.move(old, reg);
        freeReg(old);
    } else {
        if (!freeRegs.hasReg(reg))
            evictReg(reg);
        else
            freeRegs.takeReg(reg);
        masm.loadData32(addressOf(fe), reg);
    }
    regstate[reg] = RegisterState(fe, RematInfo::DATA, true);
    fe->data.setRegister(reg);
    return reg;
}

inline bool
FrameState::shouldAvoidTypeRemat(FrameEntry *fe)
{
    return fe->type.inMemory();
}

inline bool
FrameState::shouldAvoidDataRemat(FrameEntry *fe)
{
    return fe->data.inMemory();
}

inline void
FrameState::syncType(const FrameEntry *fe, Address to, Assembler &masm) const
{
    JS_ASSERT_IF(fe->type.synced(),
                 fe->isCopied() && addressOf(fe).offset != to.offset);
    JS_ASSERT(fe->type.inRegister() || fe->type.isConstant());

    if (fe->type.isConstant()) {
        JS_ASSERT(fe->isTypeKnown());
        masm.storeTypeTag(ImmTag(fe->getTypeTag()), to);
    } else {
        masm.storeTypeTag(fe->type.reg(), to);
    }
}

inline void
FrameState::syncData(const FrameEntry *fe, Address to, Assembler &masm) const
{
    JS_ASSERT_IF(addressOf(fe).base == to.base &&
                 addressOf(fe).offset == to.offset,
                 !fe->data.synced());
    JS_ASSERT(fe->data.inRegister() || fe->data.isConstant());

    if (fe->data.isConstant()) {
       if (!fe->type.synced())
           masm.storeValue(fe->getValue(), to);
       else
           masm.storeData32(Imm32(fe->getPayload32()), to);
    } else {
        masm.storeData32(fe->data.reg(), to);
    }
}

inline void
FrameState::forgetType(FrameEntry *fe)
{
    JS_ASSERT(fe->isTypeKnown() && !fe->type.synced());
    syncType(fe, addressOf(fe), masm);
    fe->type.setMemory();
}

inline void
FrameState::learnType(FrameEntry *fe, JSValueMask32 tag)
{
    if (fe->type.inRegister())
        forgetReg(fe->type.reg());
    fe->setTypeTag(tag);
}

inline JSC::MacroAssembler::Address
FrameState::addressOf(const FrameEntry *fe) const
{
    uint32 index = (fe - entries);
    JS_ASSERT(index >= nargs);
    index -= nargs;
    return Address(Assembler::FpReg, sizeof(JSStackFrame) + sizeof(Value) * index);
}

inline JSC::MacroAssembler::Jump
FrameState::testInt32(Assembler::Condition cond, FrameEntry *fe)
{
    JS_ASSERT(cond == Assembler::Equal || cond == Assembler::NotEqual);
    if (shouldAvoidTypeRemat(fe))
        return masm.testInt32(cond, addressOf(fe));
    return masm.testInt32(cond, tempRegForType(fe));
}

inline JSC::MacroAssembler::Jump
FrameState::testDouble(Assembler::Condition cond, FrameEntry *fe)
{
    JS_ASSERT(cond == Assembler::Equal || cond == Assembler::NotEqual);
    if (shouldAvoidTypeRemat(fe))
        return masm.testDouble(cond, addressOf(fe));
    return masm.testDouble(cond, tempRegForType(fe));
}

inline JSC::MacroAssembler::Jump
FrameState::testBoolean(Assembler::Condition cond, FrameEntry *fe)
{
    JS_ASSERT(cond == Assembler::Equal || cond == Assembler::NotEqual);
    if (shouldAvoidTypeRemat(fe))
        return masm.testBoolean(cond, addressOf(fe));
    return masm.testBoolean(cond, tempRegForType(fe));
}

inline FrameEntry *
FrameState::getLocal(uint32 slot)
{
    uint32 index = nargs + slot;
    if (FrameEntry *fe = base[index])
        return fe;
    FrameEntry *fe = addToTracker(index);
    fe->resetSynced();
    return fe;
}

inline void
FrameState::pinReg(RegisterID reg)
{
    JS_ASSERT(regstate[reg].fe);
    regstate[reg].save = regstate[reg].fe;
    regstate[reg].fe = NULL;
}

inline void
FrameState::unpinReg(RegisterID reg)
{
    JS_ASSERT(!regstate[reg].fe);
    regstate[reg].fe = regstate[reg].save;
}

inline void
FrameState::forgetRegs(FrameEntry *fe)
{
    if (fe->type.inRegister())
        forgetReg(fe->type.reg());
    if (fe->data.inRegister())
        forgetReg(fe->data.reg());
}

inline FrameEntry *
FrameState::tosFe() const
{
    return &entries[uint32(sp - base)];
}

inline void
FrameState::swapInTracker(FrameEntry *lhs, FrameEntry *rhs)
{
    uint32 li = lhs->trackerIndex();
    uint32 ri = rhs->trackerIndex();
    JS_ASSERT(tracker[li] == lhs);
    JS_ASSERT(tracker[ri] == rhs);
    tracker.entries[ri] = lhs;
    tracker.entries[li] = rhs;
    lhs->index_ = ri;
    rhs->index_ = li;
}

inline uint32
FrameState::localIndex(uint32 n)
{
    return nargs + n;
}

inline void
FrameState::dup()
{
    FrameEntry *fe = peek(-1);
    pushCopyOf(indexOfFe(fe));
}

inline void
FrameState::dup2()
{
    FrameEntry *lhs = peek(-2);
    FrameEntry *rhs = peek(-1);
    pushCopyOf(indexOfFe(lhs));
    pushCopyOf(indexOfFe(rhs));
}

inline void
FrameState::pushLocal(uint32 n)
{
    pushCopyOf(indexOfFe(getLocal(n)));
}

inline void
FrameState::leaveBlock(uint32 n)
{
    popn(n);
}

inline void
FrameState::enterBlock(uint32 n)
{
    /* expect that tracker has 0 entries, for now. */
    JS_ASSERT(!tracker.nentries);
    JS_ASSERT(uint32(sp + n - locals) <= script->nslots);

    sp += n;
}

} /* namspace mjit */
} /* namspace js */

#endif /* include */

