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
 *   Sean Stangl    <sstangl@mozilla.com>
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
#include "jsbool.h"
#include "jslibmath.h"
#include "jsnum.h"
#include "methodjit/MethodJIT.h"
#include "methodjit/Compiler.h"
#include "methodjit/StubCalls.h"
#include "methodjit/FrameState-inl.h"

using namespace js;
using namespace js::mjit;
using namespace JSC;

typedef JSC::MacroAssembler::FPRegisterID FPRegisterID;

static inline bool
JSOpBinaryTryConstantFold(JSContext *cx, FrameState &frame, JSOp op, FrameEntry *lhs, FrameEntry *rhs)
{
    if (!lhs->isConstant() || !rhs->isConstant())
        return false;

    const Value &L = lhs->getValue();
    const Value &R = rhs->getValue();

    if (!L.isPrimitive() || !R.isPrimitive() ||
        (op == JSOP_ADD && (L.isString() || R.isString()))) {
        return false;
    }

    double dL, dR;
    ValueToNumber(cx, L, &dL);
    ValueToNumber(cx, R, &dR);

    switch (op) {
      case JSOP_ADD:
        dL += dR;
        break;
      case JSOP_SUB:
        dL -= dR;
        break;
      case JSOP_MUL:
        dL *= dR;
        break;
      case JSOP_DIV:
        if (dR == 0) {
#ifdef XP_WIN
            if (JSDOUBLE_IS_NaN(dR))
                dL = js_NaN;
            else
#endif
            if (dL == 0 || JSDOUBLE_IS_NaN(dL))
                dL = js_NaN;
            else if (JSDOUBLE_IS_NEG(dL) != JSDOUBLE_IS_NEG(dR))
                dL = cx->runtime->negativeInfinityValue.toDouble();
            else
                dL = cx->runtime->positiveInfinityValue.toDouble();
        } else {
            dL /= dR;
        }
        break;
      case JSOP_MOD:
        if (dL == 0)
            dL = js_NaN;
        else
            dL = js_fmod(dR, dL);
        break;

      default:
        JS_NOT_REACHED("NYI");
        break;
    }

    Value v;
    v.setNumber(dL);
    frame.popn(2);
    frame.push(v);

    return true;
}

void
mjit::Compiler::slowLoadConstantDouble(Assembler &masm,
                                       FrameEntry *fe, FPRegisterID fpreg)
{
    DoublePatch patch;
    if (fe->getKnownType() == JSVAL_TYPE_INT32)
        patch.d = (double)fe->getValue().toInt32();
    else
        patch.d = fe->getValue().toDouble();
    patch.label = masm.loadDouble(NULL, fpreg);
    patch.ool = &masm != &this->masm;
    JS_ASSERT_IF(patch.ool, &masm == &stubcc.masm);
    doubleList.append(patch);
}

void
mjit::Compiler::maybeJumpIfNotInt32(Assembler &masm, MaybeJump &mj, FrameEntry *fe,
                                    MaybeRegisterID &mreg)
{
    if (!fe->isTypeKnown()) {
        if (mreg.isSet())
            mj.setJump(masm.testInt32(Assembler::NotEqual, mreg.reg()));
        else
            mj.setJump(masm.testInt32(Assembler::NotEqual, frame.addressOf(fe)));
    } else if (fe->getKnownType() != JSVAL_TYPE_INT32) {
        mj.setJump(masm.jump());
    }
}

void
mjit::Compiler::maybeJumpIfNotDouble(Assembler &masm, MaybeJump &mj, FrameEntry *fe,
                                    MaybeRegisterID &mreg)
{
    if (!fe->isTypeKnown()) {
        if (mreg.isSet())
            mj.setJump(masm.testDouble(Assembler::NotEqual, mreg.reg()));
        else
            mj.setJump(masm.testDouble(Assembler::NotEqual, frame.addressOf(fe)));
    } else if (fe->getKnownType() != JSVAL_TYPE_DOUBLE) {
        mj.setJump(masm.jump());
    }
}

void
mjit::Compiler::jsop_binary(JSOp op, VoidStub stub)
{
    FrameEntry *rhs = frame.peek(-1);
    FrameEntry *lhs = frame.peek(-2);

    if (JSOpBinaryTryConstantFold(cx, frame, op, lhs, rhs))
        return;

    /*
     * Bail out if there are unhandled types or ops.
     * This is temporary while ops are still being implemented.
     */
    if ((op == JSOP_MOD) ||
        (lhs->isTypeKnown() && (lhs->getKnownType() > JSVAL_UPPER_INCL_TYPE_OF_NUMBER_SET)) ||
        (rhs->isTypeKnown() && (rhs->getKnownType() > JSVAL_UPPER_INCL_TYPE_OF_NUMBER_SET)) 
#if defined(JS_CPU_ARM)
        /* ARM cannot detect integer overflow with multiplication. */
        || op == JSOP_MUL
#endif /* JS_CPU_ARM */
    ) {
        bool isStringResult = (op == JSOP_ADD) &&
                              ((lhs->isTypeKnown() && lhs->getKnownType() == JSVAL_TYPE_STRING) ||
                               (rhs->isTypeKnown() && rhs->getKnownType() == JSVAL_TYPE_STRING));
        prepareStubCall(Uses(2));
        stubCall(stub);
        frame.popn(2);
        if (isStringResult)
            frame.pushSyncedType(JSVAL_TYPE_STRING);
        else
            frame.pushSynced();
        return;
    }

    /* Can do int math iff there is no double constant and the op is not division. */
    bool canDoIntMath = op != JSOP_DIV &&
                        !((rhs->isTypeKnown() && rhs->getKnownType() == JSVAL_TYPE_DOUBLE) ||
                          (lhs->isTypeKnown() && lhs->getKnownType() == JSVAL_TYPE_DOUBLE));

    if (canDoIntMath)
        jsop_binary_full(lhs, rhs, op, stub);
    else
        jsop_binary_double(lhs, rhs, op, stub);
}

static void
EmitDoubleOp(JSOp op, FPRegisterID fpRight, FPRegisterID fpLeft, Assembler &masm)
{
    switch (op) {
      case JSOP_ADD:
        masm.addDouble(fpRight, fpLeft);
        break;

      case JSOP_SUB:
        masm.subDouble(fpRight, fpLeft);
        break;

      case JSOP_MUL:
        masm.mulDouble(fpRight, fpLeft);
        break;

      case JSOP_DIV:
        masm.divDouble(fpRight, fpLeft);
        break;

      default:
        JS_NOT_REACHED("unrecognized binary op");
    }
}

mjit::Compiler::MaybeJump
mjit::Compiler::loadDouble(FrameEntry *fe, FPRegisterID fpReg)
{
    MaybeJump notNumber;

    if (fe->isConstant()) {
        slowLoadConstantDouble(masm, fe, fpReg);
    } else if (!fe->isTypeKnown()) {
        frame.tempRegForType(fe);
        Jump j = frame.testDouble(Assembler::Equal, fe);
        notNumber = frame.testInt32(Assembler::NotEqual, fe);
        frame.convertInt32ToDouble(masm, fe, fpReg);
        Jump converted = masm.jump();
        j.linkTo(masm.label(), &masm);
        // CANDIDATE
        frame.loadDouble(fe, fpReg, masm);
        converted.linkTo(masm.label(), &masm);
    } else if (fe->getKnownType() == JSVAL_TYPE_INT32) {
        frame.tempRegForData(fe);
        frame.convertInt32ToDouble(masm, fe, fpReg);
    } else {
        JS_ASSERT(fe->getKnownType() == JSVAL_TYPE_DOUBLE);
        frame.loadDouble(fe, fpReg, masm);
    }

    return notNumber;
}

/*
 * This function emits a single fast-path for handling numerical arithmetic.
 * Unlike jsop_binary_full(), all integers are converted to doubles.
 */
void
mjit::Compiler::jsop_binary_double(FrameEntry *lhs, FrameEntry *rhs, JSOp op, VoidStub stub)
{
    FPRegisterID fpLeft = FPRegisters::First;
    FPRegisterID fpRight = FPRegisters::Second;

    MaybeJump lhsNotNumber = loadDouble(lhs, fpLeft);

    MaybeJump rhsNotNumber;
    if (frame.haveSameBacking(lhs, rhs))
        masm.moveDouble(fpLeft, fpRight);
    else
        rhsNotNumber = loadDouble(rhs, fpRight);

    EmitDoubleOp(op, fpRight, fpLeft, masm);
    masm.storeDouble(fpLeft, frame.addressOf(lhs));

    if (lhsNotNumber.isSet())
        stubcc.linkExit(lhsNotNumber.get(), Uses(2));
    if (rhsNotNumber.isSet())
        stubcc.linkExit(rhsNotNumber.get(), Uses(2));

    stubcc.leave();
    stubcc.call(stub);

    frame.popn(2);
    frame.pushNumber(MaybeRegisterID());

    stubcc.rejoin(Changes(1));
}

/*
 * Simpler version of jsop_binary_full() for when lhs == rhs.
 */
void
mjit::Compiler::jsop_binary_full_simple(FrameEntry *fe, JSOp op, VoidStub stub)
{
    FrameEntry *lhs = frame.peek(-2);

    /* Easiest case: known double. Don't bother conversion back yet? */
    if (fe->isTypeKnown() && fe->getKnownType() == JSVAL_TYPE_DOUBLE) {
        loadDouble(fe, FPRegisters::First);
        EmitDoubleOp(op, FPRegisters::First, FPRegisters::First, masm);
        frame.popn(2);
        frame.pushNumber(MaybeRegisterID());
        return;
    }

    /* Allocate all registers up-front. */
    FrameState::BinaryAlloc regs;
    frame.allocForSameBinary(fe, op, regs);

    MaybeJump notNumber;
    MaybeJump doublePathDone;
    if (!fe->isTypeKnown()) {
        Jump notInt = masm.testInt32(Assembler::NotEqual, regs.lhsType.reg());
        stubcc.linkExitDirect(notInt, stubcc.masm.label());

        notNumber = stubcc.masm.testDouble(Assembler::NotEqual, regs.lhsType.reg());
        frame.loadDouble(fe, FPRegisters::First, stubcc.masm);
        EmitDoubleOp(op, FPRegisters::First, FPRegisters::First, stubcc.masm);

        /* Force the double back to memory. */
        Address result = frame.addressOf(lhs);
        stubcc.masm.storeDouble(FPRegisters::First, result);

        /* Load the payload into the result reg so the rejoin is safe. */
        stubcc.masm.loadPayload(result, regs.result);

        doublePathDone = stubcc.masm.jump();
    }

    /* Okay - good to emit the integer fast-path. */
    MaybeJump overflow;
    switch (op) {
      case JSOP_ADD:
        overflow = masm.branchAdd32(Assembler::Overflow, regs.result, regs.result);
        break;

      case JSOP_SUB:
        overflow = masm.branchSub32(Assembler::Overflow, regs.result, regs.result);
        break;

#if !defined(JS_CPU_ARM)
      case JSOP_MUL:
        overflow = masm.branchMul32(Assembler::Overflow, regs.result, regs.result);
        break;
#endif

      default:
        JS_NOT_REACHED("unrecognized op");
    }
    
    JS_ASSERT(overflow.isSet());

    /*
     * Integer overflow path. Separate from the first double path, since we
     * know never to try and convert back to integer.
     */
    MaybeJump overflowDone;
    stubcc.linkExitDirect(overflow.get(), stubcc.masm.label());
    {
        if (regs.lhsNeedsRemat) {
            Address address = masm.payloadOf(frame.addressOf(lhs));
            stubcc.masm.convertInt32ToDouble(address, FPRegisters::First);
        } else if (!lhs->isConstant()) {
            stubcc.masm.convertInt32ToDouble(regs.lhsData.reg(), FPRegisters::First);
        } else {
            slowLoadConstantDouble(stubcc.masm, lhs, FPRegisters::First);
        }

        EmitDoubleOp(op, FPRegisters::First, FPRegisters::First, stubcc.masm);

        Address address = frame.addressOf(lhs);
        stubcc.masm.storeDouble(FPRegisters::First, address);
        stubcc.masm.loadPayload(address, regs.result);

        overflowDone = stubcc.masm.jump();
    }

    /* Slow paths funnel here. */
    if (notNumber.isSet())
        notNumber.get().linkTo(stubcc.masm.label(), &stubcc.masm);
    overflowDone.get().linkTo(stubcc.masm.label(), &stubcc.masm);

    /* Slow call - use frame.sync to avoid erroneous jump repatching in stubcc. */
    frame.sync(stubcc.masm, Uses(2));
    stubcc.leave();
    stubcc.call(stub);

    /* Finish up stack operations. */
    frame.popn(2);
    frame.pushNumber(regs.result, true);

    /* Merge back OOL double paths. */
    if (doublePathDone.isSet())
        stubcc.linkRejoin(doublePathDone.get());
    stubcc.linkRejoin(overflowDone.get());

    stubcc.rejoin(Changes(1));
}

/*
 * This function emits multiple fast-paths for handling numerical arithmetic.
 * Currently, it handles only ADD, SUB, and MUL, where both LHS and RHS are
 * known not to be doubles.
 *
 * The control flow of the emitted code depends on which types are known.
 * Given both types are unknown, the full spread looks like:
 *
 * Inline                              OOL
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Is LHS Int32?  ------ No -------->  Is LHS Double?  ----- No -------,
 *                                     Sync LHS                        |
 *                                     Load LHS into XMM1              |
 *                                     Is RHS Double? ---- Yes --,     |
 *                                       Is RHS Int32? ---- No --|-----|
 *                                       Convert RHS into XMM0   |     |
 *                                     Else  <-------------------'     |
 *                                       Sync RHS                      |
 *                                       Load RHS into XMM0            |
 *                                     [Add,Sub,Mul] XMM0,XMM1         |
 *                                     Jump ---------------------,     |
 *                                                               |     |
 * Is RHS Int32?  ------ No ------->   Is RHS Double? ----- No --|-----|
 *                                     Sync RHS                  |     |
 *                                     Load RHS into XMM0        |     |
 *                                     Convert LHS into XMM1     |     |
 *                                     [Add,Sub,Mul] XMM0,XMM1   |     |
 *                                     Jump ---------------------|   Slow Call
 *                                                               |
 * [Add,Sub,Mul] RHS, LHS                                        |
 * Overflow      ------ Yes ------->   Convert RHS into XMM0     |
 *                                     Convert LHS into XMM1     |
 *                                     [Add,Sub,Mul] XMM0,XMM1   |
 *                                     Sync XMM1 to stack    <---'
 *  <--------------------------------- Rejoin
 */
void
mjit::Compiler::jsop_binary_full(FrameEntry *lhs, FrameEntry *rhs, JSOp op, VoidStub stub)
{
    if (frame.haveSameBacking(lhs, rhs)) {
        jsop_binary_full_simple(lhs, op, stub);
        return;
    }

    /* Allocate all registers up-front. */
    FrameState::BinaryAlloc regs;
    frame.allocForBinary(lhs, rhs, op, regs);

    /* Quick-test some invariants. */
    JS_ASSERT_IF(lhs->isTypeKnown(), lhs->getKnownType() == JSVAL_TYPE_INT32);
    JS_ASSERT_IF(rhs->isTypeKnown(), rhs->getKnownType() == JSVAL_TYPE_INT32);

    FPRegisterID fpLeft = FPRegisters::First;
    FPRegisterID fpRight = FPRegisters::Second;

    MaybeJump lhsNotDouble, rhsNotNumber, lhsUnknownDone;
    if (!lhs->isTypeKnown())
        emitLeftDoublePath(lhs, rhs, regs, lhsNotDouble, rhsNotNumber, lhsUnknownDone);

    MaybeJump rhsNotNumber2;
    if (!rhs->isTypeKnown())
        emitRightDoublePath(lhs, rhs, regs, rhsNotNumber2);

    /* Perform the double addition. */
    MaybeJump doublePathDone;
    if (!rhs->isTypeKnown() || lhsUnknownDone.isSet()) {
        /* If the LHS type was not known, link its path here. */
        if (lhsUnknownDone.isSet())
            lhsUnknownDone.get().linkTo(stubcc.masm.label(), &stubcc.masm);
        
        /* Perform the double operation. */
        EmitDoubleOp(op, fpRight, fpLeft, stubcc.masm);

        /* Force the double back to memory. */
        Address result = frame.addressOf(lhs);
        stubcc.masm.storeDouble(fpLeft, result);

        /* Load the payload into the result reg so the rejoin is safe. */
        stubcc.masm.loadPayload(result, regs.result);

        /* We'll link this back up later, at the bottom of the op. */
        doublePathDone = stubcc.masm.jump();
    }

    /* Time to do the integer path. Figure out the immutable side. */
    int32 value = 0;
    JSOp origOp = op;
    MaybeRegisterID reg;
    if (!regs.resultHasRhs) {
        if (!regs.rhsData.isSet())
            value = rhs->getValue().toInt32();
        else
            reg = regs.rhsData.reg();
    } else {
        if (!regs.lhsData.isSet())
            value = lhs->getValue().toInt32();
        else
            reg = regs.lhsData.reg();
        if (op == JSOP_SUB) {
            masm.neg32(regs.result);
            op = JSOP_ADD;
        }
    }

    /* Okay - good to emit the integer fast-path. */
    MaybeJump overflow;
    switch (op) {
      case JSOP_ADD:
        if (reg.isSet())
            overflow = masm.branchAdd32(Assembler::Overflow, reg.reg(), regs.result);
        else
            overflow = masm.branchAdd32(Assembler::Overflow, Imm32(value), regs.result);
        break;

      case JSOP_SUB:
        if (reg.isSet())
            overflow = masm.branchSub32(Assembler::Overflow, reg.reg(), regs.result);
        else
            overflow = masm.branchSub32(Assembler::Overflow, Imm32(value), regs.result);
        break;

#if !defined(JS_CPU_ARM)
      case JSOP_MUL:
        JS_ASSERT(reg.isSet());
        overflow = masm.branchMul32(Assembler::Overflow, reg.reg(), regs.result);
        break;
#endif

      default:
        JS_NOT_REACHED("unrecognized op");
    }
    op = origOp;
    
    JS_ASSERT(overflow.isSet());

    /*
     * Integer overflow path. Separate from the first double path, since we
     * know never to try and convert back to integer.
     */
    MaybeJump overflowDone;
    stubcc.linkExitDirect(overflow.get(), stubcc.masm.label());
    {
        if (regs.lhsNeedsRemat) {
            Address address = masm.payloadOf(frame.addressOf(lhs));
            stubcc.masm.convertInt32ToDouble(address, fpLeft);
        } else if (!lhs->isConstant()) {
            stubcc.masm.convertInt32ToDouble(regs.lhsData.reg(), fpLeft);
        } else {
            slowLoadConstantDouble(stubcc.masm, lhs, fpLeft);
        }

        if (regs.rhsNeedsRemat) {
            Address address = masm.payloadOf(frame.addressOf(rhs));
            stubcc.masm.convertInt32ToDouble(address, fpRight);
        } else if (!rhs->isConstant()) {
            stubcc.masm.convertInt32ToDouble(regs.rhsData.reg(), fpRight);
        } else {
            slowLoadConstantDouble(stubcc.masm, rhs, fpRight);
        }

        EmitDoubleOp(op, fpRight, fpLeft, stubcc.masm);

        Address address = frame.addressOf(lhs);
        stubcc.masm.storeDouble(fpLeft, address);
        stubcc.masm.loadPayload(address, regs.result);

        overflowDone = stubcc.masm.jump();
    }

    /* The register allocator creates at most one temporary. */
    if (regs.extraFree.isSet())
        frame.freeReg(regs.extraFree.reg());

    /* Slow paths funnel here. */
    if (lhsNotDouble.isSet()) {
        lhsNotDouble.get().linkTo(stubcc.masm.label(), &stubcc.masm);
        if (rhsNotNumber.isSet())
            rhsNotNumber.get().linkTo(stubcc.masm.label(), &stubcc.masm);
    }
    if (rhsNotNumber2.isSet())
        rhsNotNumber2.get().linkTo(stubcc.masm.label(), &stubcc.masm);

    /* Slow call - use frame.sync to avoid erroneous jump repatching in stubcc. */
    frame.sync(stubcc.masm, Uses(2));
    stubcc.leave();
    stubcc.call(stub);

    /* Finish up stack operations. */
    frame.popn(2);
    frame.pushNumber(regs.result, true);

    /* Merge back OOL double paths. */
    if (doublePathDone.isSet())
        stubcc.linkRejoin(doublePathDone.get());
    stubcc.linkRejoin(overflowDone.get());

    stubcc.rejoin(Changes(1));
}

static const uint64 DoubleNegMask = 0x8000000000000000ULL;

void
mjit::Compiler::jsop_neg()
{
    FrameEntry *fe = frame.peek(-1);

    if (fe->isTypeKnown() && fe->getKnownType() > JSVAL_UPPER_INCL_TYPE_OF_NUMBER_SET) {
        prepareStubCall(Uses(1));
        stubCall(stubs::Neg);
        frame.pop();
        frame.pushSynced();
        return;
    }

    JS_ASSERT(!fe->isConstant());

    /* 
     * Load type information into register.
     */
    MaybeRegisterID feTypeReg;
    if (!fe->isTypeKnown() && !frame.shouldAvoidTypeRemat(fe)) {
        /* Safe because only one type is loaded. */
        feTypeReg.setReg(frame.tempRegForType(fe));

        /* Don't get clobbered by copyDataIntoReg(). */
        frame.pinReg(feTypeReg.reg());
    }

    /*
     * If copyDataIntoReg() is called here, syncAllRegs() is not required.
     * If called from the int path, the double and int states would
     * need to be explicitly synced in a currently unsupported manner.
     *
     * This function call can call allocReg() and clobber any register.
     * (It is also useful to think about the interplay between this
     *  code being in the int path, versus stub call syncing..)
     */
    RegisterID reg = frame.copyDataIntoReg(masm, fe);
    Label feSyncTarget = stubcc.syncExitAndJump(Uses(1));

    /* Try a double path (inline). */
    MaybeJump jmpNotDbl;
    {
        maybeJumpIfNotDouble(masm, jmpNotDbl, fe, feTypeReg);

        FPRegisterID fpreg = frame.copyEntryIntoFPReg(fe, FPRegisters::First);

#if defined JS_CPU_X86 || defined JS_CPU_X64
        masm.loadDouble(&DoubleNegMask, FPRegisters::Second);
        masm.xorDouble(FPRegisters::Second, fpreg);
#elif defined JS_CPU_ARM
        masm.negDouble(fpreg, fpreg);
#endif

        /* Overwrite pushed frame's memory (before push). */
        masm.storeDouble(fpreg, frame.addressOf(fe));
    }

    /* Try an integer path (out-of-line). */
    MaybeJump jmpNotInt;
    MaybeJump jmpIntZero;
    MaybeJump jmpMinInt;
    MaybeJump jmpIntRejoin;
    Label lblIntPath = stubcc.masm.label();
    {
        maybeJumpIfNotInt32(stubcc.masm, jmpNotInt, fe, feTypeReg);

        /* 0 (int) -> -0 (double). */
        jmpIntZero.setJump(stubcc.masm.branch32(Assembler::Equal, reg, Imm32(0)));
        /* int32 negation on (-2147483648) yields (-2147483648). */
        jmpMinInt.setJump(stubcc.masm.branch32(Assembler::Equal, reg, Imm32(1 << 31)));

        stubcc.masm.neg32(reg);

        /* Sync back with double path. */
        stubcc.masm.storePayload(reg, frame.addressOf(fe));
        stubcc.masm.storeTypeTag(ImmType(JSVAL_TYPE_INT32), frame.addressOf(fe));

        jmpIntRejoin.setJump(stubcc.masm.jump());
    }

    frame.freeReg(reg);
    if (feTypeReg.isSet())
        frame.unpinReg(feTypeReg.reg());

    stubcc.leave();
    stubcc.call(stubs::Neg);

    frame.pop();
    frame.pushSynced();

    /* Link jumps. */
    if (jmpNotDbl.isSet())
        stubcc.linkExitDirect(jmpNotDbl.getJump(), lblIntPath);

    if (jmpNotInt.isSet())
        jmpNotInt.getJump().linkTo(feSyncTarget, &stubcc.masm);
    if (jmpIntZero.isSet())
        jmpIntZero.getJump().linkTo(feSyncTarget, &stubcc.masm);
    if (jmpMinInt.isSet())
        jmpMinInt.getJump().linkTo(feSyncTarget, &stubcc.masm);
    if (jmpIntRejoin.isSet())
        stubcc.crossJump(jmpIntRejoin.getJump(), masm.label());

    stubcc.rejoin(Changes(1));
}

void
mjit::Compiler::jsop_mod()
{
#if defined(JS_CPU_X86)
    FrameEntry *lhs = frame.peek(-2);
    FrameEntry *rhs = frame.peek(-1);
    if ((lhs->isTypeKnown() && lhs->getKnownType() != JSVAL_TYPE_INT32) ||
        (rhs->isTypeKnown() && rhs->getKnownType() != JSVAL_TYPE_INT32))
#endif
    {
        prepareStubCall(Uses(2));
        stubCall(stubs::Mod);
        frame.popn(2);
        frame.pushSynced();
        return;
    }

#if defined(JS_CPU_X86)
    if (!lhs->isTypeKnown()) {
        Jump j = frame.testInt32(Assembler::NotEqual, lhs);
        stubcc.linkExit(j, Uses(2));
    }
    if (!rhs->isTypeKnown()) {
        Jump j = frame.testInt32(Assembler::NotEqual, rhs);
        stubcc.linkExit(j, Uses(2));
    }

    /* LHS must be in EAX:EDX */
    if (!lhs->isConstant()) {
        frame.copyDataIntoReg(lhs, X86Registers::eax);
    } else {
        frame.takeReg(X86Registers::eax);
        masm.move(Imm32(lhs->getValue().toInt32()), X86Registers::eax);
    }

    /* Get RHS into anything but EDX - could avoid more spilling? */
    MaybeRegisterID temp;
    RegisterID rhsReg;
    if (!rhs->isConstant()) {
        uint32 mask = Registers::AvailRegs & ~Registers::maskReg(X86Registers::edx);
        rhsReg = frame.tempRegInMaskForData(rhs, mask);
        JS_ASSERT(rhsReg != X86Registers::edx);
    } else {
        rhsReg = frame.allocReg(Registers::AvailRegs & ~Registers::maskReg(X86Registers::edx));
        JS_ASSERT(rhsReg != X86Registers::edx);
        masm.move(Imm32(rhs->getValue().toInt32()), rhsReg);
        temp = rhsReg;
    }
    frame.takeReg(X86Registers::edx);
    frame.freeReg(X86Registers::eax);

    if (temp.isSet())
        frame.freeReg(temp.reg());

    bool slowPath = !(lhs->isTypeKnown() && rhs->isTypeKnown());
    if (rhs->isConstant() && rhs->getValue().toInt32() != 0) {
        if (rhs->getValue().toInt32() == -1) {
            /* Guard against -1 / INT_MIN which throws a hardware exception. */
            Jump checkDivExc = masm.branch32(Assembler::Equal, X86Registers::eax,
                                             Imm32(0x80000000));
            stubcc.linkExit(checkDivExc, Uses(2));
            slowPath = true;
        }
    } else {
        Jump checkDivExc = masm.branch32(Assembler::Equal, X86Registers::eax, Imm32(0x80000000));
        stubcc.linkExit(checkDivExc, Uses(2));
        Jump checkZero = masm.branchTest32(Assembler::Zero, rhsReg, rhsReg);
        stubcc.linkExit(checkZero, Uses(2));
        slowPath = true;
    }

    /* Perform division. */
    masm.idiv(rhsReg);

    /* ECMA-262 11.5.3 requires the result to have the same sign as the lhs.
     * Thus, if the remainder of the div instruction is zero and the lhs is
     * negative, we must return negative 0. */

    bool lhsMaybeNeg = true;
    bool lhsIsNeg = false;
    if (lhs->isConstant()) {
        /* This condition is established at the top of this function. */
        JS_ASSERT(lhs->getValue().isInt32());
        lhsMaybeNeg = lhsIsNeg = (lhs->getValue().toInt32() < 0);
    }

    MaybeJump done;
    if (lhsMaybeNeg) {
        RegisterID lhsData;
        if (!lhsIsNeg)
            lhsData = frame.tempRegForData(lhs);
        Jump negZero1 = masm.branchTest32(Assembler::NonZero, X86Registers::edx);
        MaybeJump negZero2;
        if (!lhsIsNeg)
            negZero2 = masm.branchTest32(Assembler::Zero, lhsData, Imm32(0x80000000));
        /* Darn, negative 0. */
        masm.storeValue(DoubleValue(-0.0), frame.addressOf(lhs));

        /* :TODO: This is wrong, must load into EDX as well. */

        done = masm.jump();
        negZero1.linkTo(masm.label(), &masm);
        if (negZero2.isSet())
            negZero2.getJump().linkTo(masm.label(), &masm);
    }

    /* Better - integer. */
    masm.storeTypeTag(ImmType(JSVAL_TYPE_INT32), frame.addressOf(lhs));

    if (done.isSet())
        done.getJump().linkTo(masm.label(), &masm);

    if (slowPath) {
        stubcc.leave();
        stubcc.call(stubs::Mod);
    }

    frame.popn(2);
    frame.pushNumber(X86Registers::edx);

    if (slowPath)
        stubcc.rejoin(Changes(1));
#endif
}

void
mjit::Compiler::jsop_relational_int(JSOp op, BoolStub stub, jsbytecode *target, JSOp fused)
{
    FrameEntry *rhs = frame.peek(-1);
    FrameEntry *lhs = frame.peek(-2);

    /* Test the types. */
    if (!rhs->isTypeKnown()) {
        Jump rhsFail = frame.testInt32(Assembler::NotEqual, rhs);
        if (target)
            stubcc.linkExitForBranch(rhsFail);
        else
            stubcc.linkExit(rhsFail, Uses(2));
        frame.learnType(rhs, JSVAL_TYPE_INT32);
    }
    if (!lhs->isTypeKnown() && !frame.haveSameBacking(lhs, rhs)) {
        Jump lhsFail = frame.testInt32(Assembler::NotEqual, lhs);
        if (target)
            stubcc.linkExitForBranch(lhsFail);
        else
            stubcc.linkExit(lhsFail, Uses(2));
    }

    Assembler::Condition cond;
    switch (op) {
      case JSOP_EQ:
        cond = Assembler::Equal;
        break;
      case JSOP_NE:
        cond = Assembler::NotEqual;
        break;
      default:
        JS_NOT_REACHED("wat");
        return;
    }

    /* Swap the LHS and RHS if it makes register allocation better... or possible. */
    bool swapped = false;
    if (lhs->isConstant() ||
        (frame.shouldAvoidDataRemat(lhs) && !rhs->isConstant())) {
        FrameEntry *temp = rhs;
        rhs = lhs;
        lhs = temp;
        swapped = true;
    }

    stubcc.leave();
    stubcc.call(stub);

    if (target) {
        /* We can do a little better when we know the opcode is fused. */
        RegisterID lr = frame.ownRegForData(lhs);
        
        /* Initialize stuff to quell GCC warnings. */
        bool rhsConst;
        int32 rval = 0;
        RegisterID rr = Registers::ReturnReg;
        if (!(rhsConst = rhs->isConstant()))
            rr = frame.ownRegForData(rhs);
        else
            rval = rhs->getValue().toInt32();

        frame.pop();
        frame.pop();

        /*
         * Note: this resets the regster allocator, so rr and lr don't need
         * to be freed. We're not going to touch the frame.
         */
        frame.forgetEverything();

        /* Invert the test for IFEQ. */
        if (fused == JSOP_IFEQ) {
            switch (cond) {
              case Assembler::Equal:
                cond = Assembler::NotEqual;
                break;
              case Assembler::NotEqual:
                cond = Assembler::Equal;
                break;
              default:
                JS_NOT_REACHED("hello");
            }
        }

        Jump j, fast;
        if (!rhsConst)
            fast = masm.branch32(cond, lr, rr);
        else
            fast = masm.branch32(cond, lr, Imm32(rval));

        JaegerSpew(JSpew_Insns, " ---- BEGIN SLOW RESTORE CODE ---- \n");
        /*
         * The stub call has no need to rejoin, since state is synced.
         * Instead, we can just test the return value.
         */
        Assembler::Condition cond = (fused == JSOP_IFEQ)
                                    ? Assembler::Zero
                                    : Assembler::NonZero;
        j = stubcc.masm.branchTest32(cond, Registers::ReturnReg, Registers::ReturnReg);
        stubcc.jumpInScript(j, target);
        /* ^-- :TODO: use jumpAndTrace */

        /* Rejoin unnecessary - state is flushed. */
        j = stubcc.masm.jump();
        stubcc.crossJump(j, masm.label());

        /*
         * NB: jumpAndTrace emits to the OOL path, so make sure not to use it
         * in the middle of an in-progress slow path.
         */
        jumpAndTrace(fast, target);

        JaegerSpew(JSpew_Insns, " ---- END SLOW RESTORE CODE ---- \n");
    } else {
        /* No fusing. Compare, set, and push a boolean. */

        RegisterID reg = frame.ownRegForData(lhs);

        /* x86/64's SET instruction can only take single-byte regs.*/
        RegisterID resultReg = reg;
        if (!(Registers::maskReg(reg) & Registers::SingleByteRegs))
            resultReg = frame.allocReg(Registers::SingleByteRegs);

        /* Emit the compare & set. */
        if (rhs->isConstant()) {
            masm.set32(cond, reg, Imm32(rhs->getValue().toInt32()), resultReg);
        } else if (frame.shouldAvoidDataRemat(rhs)) {
            masm.set32(cond, reg,
                       masm.payloadOf(frame.addressOf(rhs)),
                       resultReg);
        } else {
            masm.set32(cond, reg, frame.tempRegForData(rhs), resultReg);
        }

        /* Clean up and push a boolean. */
        frame.pop();
        frame.pop();
        if (reg != resultReg)
            frame.freeReg(reg);
        frame.pushTypedPayload(JSVAL_TYPE_BOOLEAN, resultReg);
        stubcc.rejoin(Changes(1));
    }
}

/*
 * Emit an OOL path for a possibly double LHS, and possibly int32 or number RHS.
 */
void
mjit::Compiler::emitLeftDoublePath(FrameEntry *lhs, FrameEntry *rhs, FrameState::BinaryAlloc &regs,
                                   MaybeJump &lhsNotDouble, MaybeJump &rhsNotNumber,
                                   MaybeJump &lhsUnknownDone)
{
    FPRegisterID fpLeft = FPRegisters::First;
    FPRegisterID fpRight = FPRegisters::Second;

    /* If the LHS is not a 32-bit integer, take OOL path. */
    Jump lhsNotInt32 = masm.testInt32(Assembler::NotEqual, regs.lhsType.reg());
    stubcc.linkExitDirect(lhsNotInt32, stubcc.masm.label());

    /* OOL path for LHS as a double - first test LHS is double. */
    lhsNotDouble = stubcc.masm.testDouble(Assembler::NotEqual, regs.lhsType.reg());

    /* Ensure the RHS is a number. */
    MaybeJump rhsIsDouble;
    if (!rhs->isTypeKnown()) {
        rhsIsDouble = stubcc.masm.testDouble(Assembler::Equal, regs.rhsType.reg());
        rhsNotNumber = stubcc.masm.testInt32(Assembler::NotEqual, regs.rhsType.reg());
    }

    /* If RHS is constant, convert now. */
    if (rhs->isConstant())
        slowLoadConstantDouble(stubcc.masm, rhs, fpRight);
    else
        stubcc.masm.convertInt32ToDouble(regs.rhsData.reg(), fpRight);

    if (!rhs->isTypeKnown()) {
        /* Jump past double load, bind double type check. */
        Jump converted = stubcc.masm.jump();
        rhsIsDouble.get().linkTo(stubcc.masm.label(), &stubcc.masm);

        /* Load the double. */
        frame.loadDouble(regs.rhsType.reg(), regs.rhsData.reg(),
                         rhs, fpRight, stubcc.masm);

        converted.linkTo(stubcc.masm.label(), &stubcc.masm);
    }

    /* Load the LHS. */
    frame.loadDouble(regs.lhsType.reg(), regs.lhsData.reg(),
                     lhs, fpLeft, stubcc.masm);
    lhsUnknownDone = stubcc.masm.jump();
}

/*
 * Emit an OOL path for an integer LHS, possibly double RHS.
 */
void
mjit::Compiler::emitRightDoublePath(FrameEntry *lhs, FrameEntry *rhs, FrameState::BinaryAlloc &regs,
                                    MaybeJump &rhsNotNumber2)
{
    FPRegisterID fpLeft = FPRegisters::First;
    FPRegisterID fpRight = FPRegisters::Second;

    /* If the RHS is not a double, take OOL path. */
    Jump notInt32 = masm.testInt32(Assembler::NotEqual, regs.rhsType.reg());
    stubcc.linkExitDirect(notInt32, stubcc.masm.label());

    /* Now test if RHS is a double. */
    rhsNotNumber2 = stubcc.masm.testDouble(Assembler::NotEqual, regs.rhsType.reg());

    /* We know LHS is an integer. */
    if (lhs->isConstant())
        slowLoadConstantDouble(stubcc.masm, lhs, fpLeft);
    else
        stubcc.masm.convertInt32ToDouble(regs.lhsData.reg(), fpLeft);

    /* Load the RHS. */
    frame.loadDouble(regs.rhsType.reg(), regs.rhsData.reg(),
                     rhs, fpRight, stubcc.masm);
}

static inline Assembler::DoubleCondition
DoubleCondForOp(JSOp op, JSOp fused)
{
    bool ifeq = fused == JSOP_IFEQ;
    switch (op) {
      case JSOP_GT:
        return ifeq 
               ? Assembler::DoubleLessThanOrEqualOrUnordered
               : Assembler::DoubleGreaterThan;
      case JSOP_GE:
        return ifeq
               ? Assembler::DoubleLessThanOrUnordered
               : Assembler::DoubleGreaterThanOrEqual;
      case JSOP_LT:
        return ifeq
               ? Assembler::DoubleGreaterThanOrEqualOrUnordered
               : Assembler::DoubleLessThan;
      case JSOP_LE:
        return ifeq
               ? Assembler::DoubleGreaterThanOrUnordered
               : Assembler::DoubleLessThanOrEqual;
      default:
        JS_NOT_REACHED("unrecognized op");
        return Assembler::DoubleLessThan;
    }
}

void
mjit::Compiler::jsop_relational_double(JSOp op, BoolStub stub, jsbytecode *target, JSOp fused)
{
    FrameEntry *rhs = frame.peek(-1);
    FrameEntry *lhs = frame.peek(-2);

    FPRegisterID fpLeft = FPRegisters::First;
    FPRegisterID fpRight = FPRegisters::Second;

    JS_ASSERT_IF(!target, fused != JSOP_IFEQ);

    MaybeJump lhsNotNumber = loadDouble(lhs, fpLeft);
    MaybeJump rhsNotNumber = loadDouble(rhs, fpRight);

    Assembler::DoubleCondition dblCond = DoubleCondForOp(op, fused);

    if (target) {
        if (lhsNotNumber.isSet())
            stubcc.linkExitForBranch(lhsNotNumber.get());
        if (rhsNotNumber.isSet())
            stubcc.linkExitForBranch(rhsNotNumber.get());
        stubcc.leave();
        stubcc.call(stub);

        frame.forgetEverything();
        frame.popn(2);

        Jump j = masm.branchDouble(dblCond, fpLeft, fpRight);

        /*
         * The stub call has no need to rejoin since the state is synced.
         * Instead, we can just test the return value.
         */
        Assembler::Condition cond = (fused == JSOP_IFEQ)
                                    ? Assembler::Zero
                                    : Assembler::NonZero;
        Jump sj = stubcc.masm.branchTest32(cond, Registers::ReturnReg, Registers::ReturnReg);
        stubcc.jumpInScript(sj, target);
        /* ^-- :TODO: use jumpAndTrace  */

        /* Rejoin from the slow path. */
        sj = stubcc.masm.jump();
        stubcc.crossJump(sj, masm.label());

        /*
         * NB: jumpAndTrace emits to the OOL path, so make sure not to use it
         * in the middle of an in-progress slow path.
         */
        jumpAndTrace(j, target);
    } else {
        if (lhsNotNumber.isSet())
            stubcc.linkExit(lhsNotNumber.get(), Uses(2));
        if (rhsNotNumber.isSet())
            stubcc.linkExit(rhsNotNumber.get(), Uses(2));
        stubcc.leave();
        stubcc.call(stub);

        frame.popn(2);

        RegisterID reg = frame.allocReg();
        Jump j = masm.branchDouble(dblCond, fpLeft, fpRight);
        masm.move(Imm32(0), reg);
        Jump skip = masm.jump();
        j.linkTo(masm.label(), &masm);
        masm.move(Imm32(1), reg);
        skip.linkTo(masm.label(), &masm);

        frame.pushTypedPayload(JSVAL_TYPE_BOOLEAN, reg);

        stubcc.rejoin(Changes(1));
    }
}

void
mjit::Compiler::jsop_relational_self(JSOp op, BoolStub stub, jsbytecode *target, JSOp fused)
{
#ifdef DEBUG
    FrameEntry *rhs = frame.peek(-1);
    FrameEntry *lhs = frame.peek(-2);

    JS_ASSERT(frame.haveSameBacking(lhs, rhs));
#endif

    /* :TODO: optimize this?  */
    emitStubCmpOp(stub, target, fused);
}

/* See jsop_binary_full() for more information on how this works. */
void
mjit::Compiler::jsop_relational_full(JSOp op, BoolStub stub, jsbytecode *target, JSOp fused)
{
    FrameEntry *rhs = frame.peek(-1);
    FrameEntry *lhs = frame.peek(-2);

    /* Allocate all registers up-front. */
    FrameState::BinaryAlloc regs;
    frame.allocForBinary(lhs, rhs, op, regs, !target);

    FPRegisterID fpLeft = FPRegisters::First;
    FPRegisterID fpRight = FPRegisters::Second;

    MaybeJump lhsNotDouble, rhsNotNumber, lhsUnknownDone;
    if (!lhs->isTypeKnown())
        emitLeftDoublePath(lhs, rhs, regs, lhsNotDouble, rhsNotNumber, lhsUnknownDone);

    MaybeJump rhsNotNumber2;
    if (!rhs->isTypeKnown())
        emitRightDoublePath(lhs, rhs, regs, rhsNotNumber2);

    /* Both double paths will join here. */
    bool hasDoublePath = false;
    if (!rhs->isTypeKnown() || lhsUnknownDone.isSet())
        hasDoublePath = true;

    /* Integer path - figure out the immutable side. */
    JSOp cmpOp = op;
    int32 value = 0;
    RegisterID cmpReg;
    MaybeRegisterID reg;
    if (regs.lhsData.isSet()) {
        cmpReg = regs.lhsData.reg();
        if (!regs.rhsData.isSet())
            value = rhs->getValue().toInt32();
        else
            reg = regs.rhsData.reg();
    } else {
        cmpReg = regs.rhsData.reg();
        value = lhs->getValue().toInt32();
        switch (op) {
          case JSOP_GT:
            cmpOp = JSOP_LT;
            break;
          case JSOP_GE:
            cmpOp = JSOP_LE;
            break;
          case JSOP_LT:
            cmpOp = JSOP_GT;
            break;
          case JSOP_LE:
            cmpOp = JSOP_GE;
            break;
          default:
            JS_NOT_REACHED("unrecognized op");
            break;
        }
    }

    /*
     * Emit the actual comparisons. When a fusion is in play, it's faster to
     * combine the comparison with the jump, so these two cases are implemented
     * separately.
     */

    if (target) {
        /*
         * Emit the double path now, necessary to complete the OOL fast-path
         * before emitting the slow path.
         *
         * Note: doubles have not been swapped yet. Use original op.
         */
        MaybeJump doubleTest, doubleFall;
        Assembler::DoubleCondition dblCond = DoubleCondForOp(op, fused);
        if (hasDoublePath) {
            if (lhsUnknownDone.isSet())
                lhsUnknownDone.get().linkTo(stubcc.masm.label(), &stubcc.masm);
            frame.sync(stubcc.masm, Uses(frame.frameDepth()));
            doubleTest = stubcc.masm.branchDouble(dblCond, fpLeft, fpRight);
            doubleFall = stubcc.masm.jump();

            /* Link all incoming slow paths to here. */
            if (lhsNotDouble.isSet()) {
                lhsNotDouble.get().linkTo(stubcc.masm.label(), &stubcc.masm);
                if (rhsNotNumber.isSet())
                    rhsNotNumber.get().linkTo(stubcc.masm.label(), &stubcc.masm);
            }
            if (rhsNotNumber2.isSet())
                rhsNotNumber2.get().linkTo(stubcc.masm.label(), &stubcc.masm);

            /*
             * For fusions, spill the tracker state. xmm* remain intact. Note
             * that frame.sync() must be used directly, to avoid syncExit()'s
             * jumping logic.
             */
            frame.sync(stubcc.masm, Uses(frame.frameDepth()));
            stubcc.leave();
            stubcc.call(stub);
        }

        /* Forget the world, preserving data. */
        frame.pinReg(cmpReg);
        if (reg.isSet())
            frame.pinReg(reg.reg());
        frame.forgetEverything();
        frame.popn(2);

        /* Operands could have been reordered, so use cmpOp. */
        Assembler::Condition i32Cond;
        bool ifeq = fused == JSOP_IFEQ;
        switch (cmpOp) {
          case JSOP_GT:
            i32Cond = ifeq ? Assembler::LessThanOrEqual : Assembler::GreaterThan;
            break;
          case JSOP_GE:
            i32Cond = ifeq ? Assembler::LessThan : Assembler::GreaterThanOrEqual;
            break;
          case JSOP_LT:
            i32Cond = ifeq ? Assembler::GreaterThanOrEqual : Assembler::LessThan;
            break;
          case JSOP_LE:
            i32Cond = ifeq ? Assembler::GreaterThan : Assembler::LessThanOrEqual;
            break;
          default:
            JS_NOT_REACHED("unrecognized op");
            return;
        }

        /* Emit the i32 path. */
        Jump fast;
        if (reg.isSet())
            fast = masm.branch32(i32Cond, cmpReg, reg.reg());
        else
            fast = masm.branch32(i32Cond, cmpReg, Imm32(value));

        /*
         * The stub call has no need to rejoin since state is synced. Instead,
         * we can just test the return value.
         */
        Assembler::Condition cond = (fused == JSOP_IFEQ)
                                    ? Assembler::Zero
                                    : Assembler::NonZero;
        Jump j = stubcc.masm.branchTest32(cond, Registers::ReturnReg, Registers::ReturnReg);
        stubcc.jumpInScript(j, target);
        /* ^-- :TODO: use jumpAndTrace */

        /* Rejoin from the slow path. */
        j = stubcc.masm.jump();
        stubcc.crossJump(j, masm.label());

        /* :TODO: make double path invoke tracer. */
        if (hasDoublePath)
            stubcc.jumpInScript(doubleTest.get(), target);

        /*
         * NB: jumpAndTrace emits to the OOL path, so make sure not to use it
         * in the middle of an in-progress slow path.
         */
        jumpAndTrace(fast, target);

        /* Rejoin from the double path. */
        if (hasDoublePath)
            stubcc.crossJump(doubleFall.get(), masm.label());
    } else {
        /*
         * Emit the double path now, necessary to complete the OOL fast-path
         * before emitting the slow path.
         */
        MaybeJump doubleDone;
        Assembler::DoubleCondition dblCond = DoubleCondForOp(op, JSOP_NOP);
        if (hasDoublePath) {
            if (lhsUnknownDone.isSet())
                lhsUnknownDone.get().linkTo(stubcc.masm.label(), &stubcc.masm);
            /* :FIXME: Use SET if we can? */
            Jump test = stubcc.masm.branchDouble(dblCond, fpLeft, fpRight);
            stubcc.masm.move(Imm32(0), regs.result);
            Jump skip = stubcc.masm.jump();
            test.linkTo(stubcc.masm.label(), &stubcc.masm);
            stubcc.masm.move(Imm32(1), regs.result);
            skip.linkTo(stubcc.masm.label(), &stubcc.masm);
            doubleDone = stubcc.masm.jump();

            /* Link all incoming slow paths to here. */
            if (lhsNotDouble.isSet()) {
                lhsNotDouble.get().linkTo(stubcc.masm.label(), &stubcc.masm);
                if (rhsNotNumber.isSet())
                    rhsNotNumber.get().linkTo(stubcc.masm.label(), &stubcc.masm);
            }
            if (rhsNotNumber2.isSet())
                rhsNotNumber2.get().linkTo(stubcc.masm.label(), &stubcc.masm);

            /* Emit the slow path - note full frame syncage. */
            stubcc.syncExit(Uses(2));
            stubcc.leave();
            stubcc.call(stub);
        }

        /* Get an integer comparison condition. */
        Assembler::Condition i32Cond;
        switch (cmpOp) {
          case JSOP_GT:
            i32Cond = Assembler::GreaterThan;
            break;
          case JSOP_GE:
            i32Cond = Assembler::GreaterThanOrEqual;
            break;
          case JSOP_LT:
            i32Cond = Assembler::LessThan;
            break;
          case JSOP_LE:
            i32Cond = Assembler::LessThanOrEqual;
            break;
          default:
            JS_NOT_REACHED("unrecognized op");
            return;
        }

        /* Emit the compare & set. */
        if (Registers::maskReg(regs.result) & Registers::SingleByteRegs) {
            if (reg.isSet())
                masm.set32(i32Cond, cmpReg, reg.reg(), regs.result);
            else
                masm.set32(i32Cond, cmpReg, Imm32(value), regs.result);
        } else {
            Jump j;
            if (reg.isSet())
                j = masm.branch32(i32Cond, cmpReg, reg.reg());
            else
                j = masm.branch32(i32Cond, cmpReg, Imm32(value));
            masm.move(Imm32(0), regs.result);
            Jump skip = masm.jump();
            j.linkTo(masm.label(),  &masm);
            masm.move(Imm32(1), regs.result);
            skip.linkTo(masm.label(), &masm);
        }

        frame.popn(2);
        frame.pushTypedPayload(JSVAL_TYPE_BOOLEAN, regs.result);

        if (hasDoublePath)
            stubcc.crossJump(doubleDone.get(), masm.label());
        stubcc.rejoin(Changes(1));
    }
}

