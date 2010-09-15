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
#include "jsbool.h"
#include "jslibmath.h"
#include "jsnum.h"
#include "jsscope.h"
#include "jsobjinlines.h"
#include "methodjit/MethodJIT.h"
#include "methodjit/Compiler.h"
#include "methodjit/StubCalls.h"
#include "methodjit/FrameState-inl.h"

#include "jsautooplen.h"

using namespace js;
using namespace js::mjit;

typedef JSC::MacroAssembler::RegisterID RegisterID;

RegisterID
mjit::Compiler::rightRegForShift(FrameEntry *rhs)
{
#if defined(JS_CPU_X86) || defined(JS_CPU_X64)
    /*
     * Gross: RHS _must_ be in ECX, on x86.
     * Note that we take this first so that we can't up with other register
     * allocations (below) owning ecx before rhs.
     */
    RegisterID reg = JSC::X86Registers::ecx;
    if (!rhs->isConstant())
        frame.copyDataIntoReg(rhs, reg);
    return reg;
#else
    if (rhs->isConstant())
        return frame.allocReg();
    return frame.copyDataIntoReg(rhs);
#endif
}

void
mjit::Compiler::jsop_rsh_const_int(FrameEntry *lhs, FrameEntry *rhs)
{
    RegisterID rhsData = rightRegForShift(rhs);
    RegisterID result = frame.allocReg();
    masm.move(Imm32(lhs->getValue().toInt32()), result);
    masm.rshift32(rhsData, result);

    frame.freeReg(rhsData);
    frame.popn(2);
    frame.pushTypedPayload(JSVAL_TYPE_INT32, result);
}

void
mjit::Compiler::jsop_rsh_int_int(FrameEntry *lhs, FrameEntry *rhs)
{
    RegisterID rhsData = rightRegForShift(rhs);
    RegisterID lhsData = frame.copyDataIntoReg(lhs);
    masm.rshift32(rhsData, lhsData);
    frame.freeReg(rhsData);
    frame.popn(2);
    frame.pushTypedPayload(JSVAL_TYPE_INT32, lhsData);
}

void
mjit::Compiler::jsop_rsh_int_const(FrameEntry *lhs, FrameEntry *rhs)
{
    int32 shiftAmount = rhs->getValue().toInt32();

    if (!shiftAmount) {
        frame.pop();
        return;
    }

    RegisterID result = frame.copyDataIntoReg(lhs);
    masm.rshift32(Imm32(shiftAmount), result);
    frame.popn(2);
    frame.pushTypedPayload(JSVAL_TYPE_INT32, result);
}

void
mjit::Compiler::jsop_rsh_unknown_const(FrameEntry *lhs, FrameEntry *rhs)
{
    int32 shiftAmount = rhs->getValue().toInt32();

    RegisterID lhsType = frame.tempRegForType(lhs);
    frame.pinReg(lhsType);
    RegisterID lhsData = frame.copyDataIntoReg(lhs);
    frame.unpinReg(lhsType);

    Jump lhsIntGuard = masm.testInt32(Assembler::NotEqual, lhsType);
    stubcc.linkExitDirect(lhsIntGuard, stubcc.masm.label());

    Jump lhsDoubleGuard = stubcc.masm.testDouble(Assembler::NotEqual, lhsType);
    frame.loadDouble(lhs, FPRegisters::First, stubcc.masm);
    Jump lhsTruncateGuard = stubcc.masm.branchTruncateDoubleToInt32(FPRegisters::First, lhsData);
    stubcc.crossJump(stubcc.masm.jump(), masm.label());

    lhsDoubleGuard.linkTo(stubcc.masm.label(), &stubcc.masm);
    lhsTruncateGuard.linkTo(stubcc.masm.label(), &stubcc.masm);

    frame.sync(stubcc.masm, Uses(2));
    stubcc.call(stubs::Rsh);

    if (shiftAmount)
        masm.rshift32(Imm32(shiftAmount), lhsData);

    frame.popn(2);
    frame.pushTypedPayload(JSVAL_TYPE_INT32, lhsData);

    stubcc.rejoin(Changes(1));
}

void
mjit::Compiler::jsop_rsh_const_unknown(FrameEntry *lhs, FrameEntry *rhs)
{
    RegisterID rhsData = rightRegForShift(rhs);
    RegisterID rhsType = frame.tempRegForType(rhs);
    frame.pinReg(rhsType);
    RegisterID result = frame.allocReg();
    frame.unpinReg(rhsType);

    Jump rhsIntGuard = masm.testInt32(Assembler::NotEqual, rhsType);
    stubcc.linkExit(rhsIntGuard, Uses(2));
    stubcc.leave();
    stubcc.call(stubs::Rsh);
    masm.move(Imm32(lhs->getValue().toInt32()), result);
    masm.rshift32(rhsData, result);
    frame.freeReg(rhsData);

    frame.popn(2);
    frame.pushTypedPayload(JSVAL_TYPE_INT32, result);
    stubcc.rejoin(Changes(1));
}

void
mjit::Compiler::jsop_rsh_int_unknown(FrameEntry *lhs, FrameEntry *rhs)
{
    RegisterID rhsData = rightRegForShift(rhs);
    RegisterID rhsType = frame.tempRegForType(rhs);
    frame.pinReg(rhsType);
    RegisterID lhsData = frame.copyDataIntoReg(lhs);
    frame.unpinReg(rhsType);

    Jump rhsIntGuard = masm.testInt32(Assembler::NotEqual, rhsType);
    stubcc.linkExit(rhsIntGuard, Uses(2));
    stubcc.leave();
    stubcc.call(stubs::Rsh);

    masm.rshift32(rhsData, lhsData);
    frame.freeReg(rhsData);
    frame.popn(2);
    frame.pushTypedPayload(JSVAL_TYPE_INT32, lhsData);

    stubcc.rejoin(Changes(1));
}

void
mjit::Compiler::jsop_rsh_unknown_any(FrameEntry *lhs, FrameEntry *rhs)
{
    JS_ASSERT(!lhs->isTypeKnown());
    JS_ASSERT(!rhs->isNotType(JSVAL_TYPE_INT32));

    /* Allocate registers. */
    RegisterID rhsData = rightRegForShift(rhs);

    MaybeRegisterID rhsType;
    if (!rhs->isTypeKnown()) {
        rhsType.setReg(frame.tempRegForType(rhs));
        frame.pinReg(rhsType.reg());
    }

    RegisterID lhsType = frame.tempRegForType(lhs);
    frame.pinReg(lhsType);
    RegisterID lhsData = frame.copyDataIntoReg(lhs);
    frame.unpinReg(lhsType);

    /* Non-integer rhs jumps to stub. */
    MaybeJump rhsIntGuard;
    if (rhsType.isSet()) {
        rhsIntGuard.setJump(masm.testInt32(Assembler::NotEqual, rhsType.reg()));
        frame.unpinReg(rhsType.reg());
    }

    /* Non-integer lhs jumps to double guard. */
    Jump lhsIntGuard = masm.testInt32(Assembler::NotEqual, lhsType);
    stubcc.linkExitDirect(lhsIntGuard, stubcc.masm.label());

    /* Attempt to convert lhs double to int32. */
    Jump lhsDoubleGuard = stubcc.masm.testDouble(Assembler::NotEqual, lhsType);
    frame.loadDouble(lhs, FPRegisters::First, stubcc.masm);
    Jump lhsTruncateGuard = stubcc.masm.branchTruncateDoubleToInt32(FPRegisters::First, lhsData);
    stubcc.crossJump(stubcc.masm.jump(), masm.label());

    lhsDoubleGuard.linkTo(stubcc.masm.label(), &stubcc.masm);
    lhsTruncateGuard.linkTo(stubcc.masm.label(), &stubcc.masm);

    if (rhsIntGuard.isSet())
        stubcc.linkExitDirect(rhsIntGuard.getJump(), stubcc.masm.label());
    frame.sync(stubcc.masm, Uses(2));
    stubcc.call(stubs::Rsh);

    masm.rshift32(rhsData, lhsData);

    frame.freeReg(rhsData);
    frame.popn(2);
    frame.pushTypedPayload(JSVAL_TYPE_INT32, lhsData);

    stubcc.rejoin(Changes(1));
}

void
mjit::Compiler::jsop_rsh()
{
    FrameEntry *rhs = frame.peek(-1);
    FrameEntry *lhs = frame.peek(-2);

    if (tryBinaryConstantFold(cx, frame, JSOP_RSH, lhs, rhs))
        return;

    if (lhs->isNotType(JSVAL_TYPE_INT32) || rhs->isNotType(JSVAL_TYPE_INT32)) {
        prepareStubCall(Uses(2));
        stubCall(stubs::Rsh);
        frame.popn(2);
        frame.pushSyncedType(JSVAL_TYPE_INT32);
        return;
    }

    JS_ASSERT(!(lhs->isConstant() && rhs->isConstant()));
    if (lhs->isConstant()) {
        if (rhs->isType(JSVAL_TYPE_INT32))
            jsop_rsh_const_int(lhs, rhs);
        else
            jsop_rsh_const_unknown(lhs, rhs);
    } else if (rhs->isConstant()) {
        if (lhs->isType(JSVAL_TYPE_INT32))
            jsop_rsh_int_const(lhs, rhs);
        else
            jsop_rsh_unknown_const(lhs, rhs);
    } else {
        if (lhs->isType(JSVAL_TYPE_INT32) && rhs->isType(JSVAL_TYPE_INT32))
            jsop_rsh_int_int(lhs, rhs);
        else if (lhs->isType(JSVAL_TYPE_INT32))
            jsop_rsh_int_unknown(lhs, rhs);
        else
            jsop_rsh_unknown_any(lhs, rhs);
    }
}

void
mjit::Compiler::jsop_bitnot()
{
    FrameEntry *top = frame.peek(-1);

    /* We only want to handle integers here. */
    if (top->isTypeKnown() && top->getKnownType() != JSVAL_TYPE_INT32) {
        prepareStubCall(Uses(1));
        stubCall(stubs::BitNot);
        frame.pop();
        frame.pushSyncedType(JSVAL_TYPE_INT32);
        return;
    }
           
    /* Test the type. */
    bool stubNeeded = false;
    if (!top->isTypeKnown()) {
        Jump intFail = frame.testInt32(Assembler::NotEqual, top);
        stubcc.linkExit(intFail, Uses(1));
        frame.learnType(top, JSVAL_TYPE_INT32);
        stubNeeded = true;
    }

    if (stubNeeded) {
        stubcc.leave();
        stubcc.call(stubs::BitNot);
    }

    RegisterID reg = frame.ownRegForData(top);
    masm.not32(reg);
    frame.pop();
    frame.pushTypedPayload(JSVAL_TYPE_INT32, reg);

    if (stubNeeded)
        stubcc.rejoin(Changes(1));
}

void
mjit::Compiler::jsop_bitop(JSOp op)
{
    FrameEntry *rhs = frame.peek(-1);
    FrameEntry *lhs = frame.peek(-2);

    VoidStub stub;
    switch (op) {
      case JSOP_BITOR:
        stub = stubs::BitOr;
        break;
      case JSOP_BITAND:
        stub = stubs::BitAnd;
        break;
      case JSOP_BITXOR:
        stub = stubs::BitXor;
        break;
      case JSOP_LSH:
        stub = stubs::Lsh;
        break;
      case JSOP_URSH:
        stub = stubs::Ursh;
        break;
      default:
        JS_NOT_REACHED("wat");
        return;
    }

    bool lhsIntOrDouble = !(lhs->isNotType(JSVAL_TYPE_DOUBLE) && 
                            lhs->isNotType(JSVAL_TYPE_INT32));
    
    /* Fast-path double to int conversion. */
    if (!lhs->isConstant() && rhs->isConstant() && lhsIntOrDouble &&
        rhs->isType(JSVAL_TYPE_INT32) && rhs->getValue().toInt32() == 0 &&
        (op == JSOP_BITOR || op == JSOP_LSH)) {
        RegisterID reg = frame.copyDataIntoReg(lhs);
        if (lhs->isType(JSVAL_TYPE_INT32)) {
            frame.popn(2);
            frame.pushTypedPayload(JSVAL_TYPE_INT32, reg);
            return;
        }
        MaybeJump isInt;
        if (!lhs->isType(JSVAL_TYPE_DOUBLE)) {
            RegisterID typeReg = frame.tempRegForType(lhs);
            isInt = masm.testInt32(Assembler::Equal, typeReg);
            Jump notDouble = masm.testDouble(Assembler::NotEqual, typeReg);
            stubcc.linkExit(notDouble, Uses(2));
        }
        frame.loadDouble(lhs, FPRegisters::First, masm);
        
        Jump truncateGuard = masm.branchTruncateDoubleToInt32(FPRegisters::First, reg);
        stubcc.linkExit(truncateGuard, Uses(2));
        stubcc.leave();
        stubcc.call(stub);
        
        if (isInt.isSet())
            isInt.get().linkTo(masm.label(), &masm);
        frame.popn(2);
        frame.pushTypedPayload(JSVAL_TYPE_INT32, reg);
        stubcc.rejoin(Changes(1));
        return;
    }

    /* We only want to handle integers here. */
    if (rhs->isNotType(JSVAL_TYPE_INT32) || lhs->isNotType(JSVAL_TYPE_INT32) || 
        (op == JSOP_URSH && rhs->isConstant() && rhs->getValue().toInt32() % 32 == 0)) {
        prepareStubCall(Uses(2));
        stubCall(stub);
        frame.popn(2);
        if (op == JSOP_URSH)
            frame.pushSynced();
        else
            frame.pushSyncedType(JSVAL_TYPE_INT32);
        return;
    }
           
    /* Test the types. */
    bool stubNeeded = false;
    if (!rhs->isTypeKnown()) {
        Jump rhsFail = frame.testInt32(Assembler::NotEqual, rhs);
        stubcc.linkExit(rhsFail, Uses(2));
        frame.learnType(rhs, JSVAL_TYPE_INT32);
        stubNeeded = true;
    }
    if (!lhs->isTypeKnown() && !frame.haveSameBacking(lhs, rhs)) {
        Jump lhsFail = frame.testInt32(Assembler::NotEqual, lhs);
        stubcc.linkExit(lhsFail, Uses(2));
        stubNeeded = true;
    }

    if (lhs->isConstant() && rhs->isConstant()) {
        int32 L = lhs->getValue().toInt32();
        int32 R = rhs->getValue().toInt32();

        frame.popn(2);
        switch (op) {
          case JSOP_BITOR:
            frame.push(Int32Value(L | R));
            return;
          case JSOP_BITXOR:
            frame.push(Int32Value(L ^ R));
            return;
          case JSOP_BITAND:
            frame.push(Int32Value(L & R));
            return;
          case JSOP_LSH:
            frame.push(Int32Value(L << R));
            return;
          case JSOP_URSH: 
          {
            uint32 unsignedL;
            if (ValueToECMAUint32(cx, lhs->getValue(), (uint32_t*)&unsignedL)) {
                frame.push(NumberValue(uint32(unsignedL >> (R & 31))));
                return;
            }
            break;
          }
          default:
            JS_NOT_REACHED("say wat");
        }
    }

    RegisterID reg;

    switch (op) {
      case JSOP_BITOR:
      case JSOP_BITXOR:
      case JSOP_BITAND:
      {
        /* Commutative, and we're guaranteed both are ints. */
        if (lhs->isConstant()) {
            JS_ASSERT(!rhs->isConstant());
            FrameEntry *temp = rhs;
            rhs = lhs;
            lhs = temp;
        }

        reg = frame.ownRegForData(lhs);
        if (rhs->isConstant()) {
            if (op == JSOP_BITAND)
                masm.and32(Imm32(rhs->getValue().toInt32()), reg);
            else if (op == JSOP_BITXOR)
                masm.xor32(Imm32(rhs->getValue().toInt32()), reg);
            else
                masm.or32(Imm32(rhs->getValue().toInt32()), reg);
        } else if (frame.shouldAvoidDataRemat(rhs)) {
            if (op == JSOP_BITAND)
                masm.and32(masm.payloadOf(frame.addressOf(rhs)), reg);
            else if (op == JSOP_BITXOR)
                masm.xor32(masm.payloadOf(frame.addressOf(rhs)), reg);
            else
                masm.or32(masm.payloadOf(frame.addressOf(rhs)), reg);
        } else {
            RegisterID rhsReg = frame.tempRegForData(rhs);
            if (op == JSOP_BITAND)
                masm.and32(rhsReg, reg);
            else if (op == JSOP_BITXOR)
                masm.xor32(rhsReg, reg);
            else
                masm.or32(rhsReg, reg);
        }

        break;
      }

      case JSOP_LSH:
      case JSOP_URSH:
      {
        /* Not commutative. */
        if (rhs->isConstant()) {
            RegisterID reg = frame.ownRegForData(lhs);
            int shift = rhs->getValue().toInt32() & 0x1F;

            if (shift) {
                if (op == JSOP_LSH)
                    masm.lshift32(Imm32(shift), reg);
                else
                    masm.urshift32(Imm32(shift), reg);
            }
            if (stubNeeded) {
                stubcc.leave();
                stubcc.call(stub);
            }
            frame.popn(2);
            
            /* x >>> 0 may result in a double, handled above. */
            JS_ASSERT_IF(op == JSOP_URSH, shift >= 1);
            frame.pushTypedPayload(JSVAL_TYPE_INT32, reg);

            if (stubNeeded)
                stubcc.rejoin(Changes(1));

            return;
        }
#if defined(JS_CPU_X86) || defined(JS_CPU_X64)
        /* Grosssssss! RHS _must_ be in ECX, on x86 */
        RegisterID rr = frame.tempRegInMaskForData(rhs,
                                                   Registers::maskReg(JSC::X86Registers::ecx));
#else
        RegisterID rr = frame.tempRegForData(rhs);
#endif

        frame.pinReg(rr);
        if (lhs->isConstant()) {
            reg = frame.allocReg();
            masm.move(Imm32(lhs->getValue().toInt32()), reg);
        } else {
            reg = frame.copyDataIntoReg(lhs);
        }
        frame.unpinReg(rr);
        
        if (op == JSOP_LSH) {
            masm.lshift32(rr, reg);
        } else {
            masm.urshift32(rr, reg);
            
            Jump isNegative = masm.branch32(Assembler::LessThan, reg, Imm32(0));
            stubcc.linkExit(isNegative, Uses(2));
            stubNeeded = true;
        }
        break;
      }

      default:
        JS_NOT_REACHED("NYI");
        return;
    }

    if (stubNeeded) {
        stubcc.leave();
        stubcc.call(stub);
    }

    frame.pop();
    frame.pop();

    if (op == JSOP_URSH)
        frame.pushNumber(reg, true);
    else
        frame.pushTypedPayload(JSVAL_TYPE_INT32, reg);

    if (stubNeeded)
        stubcc.rejoin(Changes(1));
}

void
mjit::Compiler::jsop_globalinc(JSOp op, uint32 index)
{
    uint32 slot = script->getGlobalSlot(index);

    bool popped = false;
    PC += JSOP_GLOBALINC_LENGTH;
    if (JSOp(*PC) == JSOP_POP && !analysis[PC].nincoming) {
        popped = true;
        PC += JSOP_POP_LENGTH;
    }

    int amt = (js_CodeSpec[op].format & JOF_INC) ? 1 : -1;
    bool post = !!(js_CodeSpec[op].format & JOF_POST);

    RegisterID data;
    RegisterID reg = frame.allocReg();
    Address addr = masm.objSlotRef(globalObj, reg, slot);
    uint32 depth = frame.stackDepth();

    if (post && !popped) {
        frame.push(addr);
        FrameEntry *fe = frame.peek(-1);
        Jump notInt = frame.testInt32(Assembler::NotEqual, fe);
        stubcc.linkExit(notInt, Uses(0));
        data = frame.copyDataIntoReg(fe);
    } else {
        Jump notInt = masm.testInt32(Assembler::NotEqual, addr);
        stubcc.linkExit(notInt, Uses(0));
        data = frame.allocReg();
        masm.loadPayload(addr, data);
    }

    Jump ovf;
    if (amt > 0)
        ovf = masm.branchAdd32(Assembler::Overflow, Imm32(1), data);
    else
        ovf = masm.branchSub32(Assembler::Overflow, Imm32(1), data);
    stubcc.linkExit(ovf, Uses(0));

    stubcc.leave();
    stubcc.masm.lea(addr, Registers::ArgReg1);
    stubcc.vpInc(op, depth);

    masm.storePayload(data, addr);

    if (!post && !popped)
        frame.pushInt32(data);
    else
        frame.freeReg(data);

    frame.freeReg(reg);

    stubcc.rejoin(Changes((!post && !popped) ? 1 : 0));
}

static inline bool
CheckNullOrUndefined(FrameEntry *fe)
{
    if (!fe->isTypeKnown())
        return false;
    JSValueType type = fe->getKnownType();
    return type == JSVAL_TYPE_NULL || type == JSVAL_TYPE_UNDEFINED;
}

void
mjit::Compiler::jsop_equality(JSOp op, BoolStub stub, jsbytecode *target, JSOp fused)
{
    FrameEntry *rhs = frame.peek(-1);
    FrameEntry *lhs = frame.peek(-2);

    /* The compiler should have handled constant folding. */
    JS_ASSERT(!(rhs->isConstant() && lhs->isConstant()));

    bool lhsTest;
    if ((lhsTest = CheckNullOrUndefined(lhs)) || CheckNullOrUndefined(rhs)) {
        /* What's the other mask? */
        FrameEntry *test = lhsTest ? rhs : lhs;

        if (test->isTypeKnown()) {
            emitStubCmpOp(stub, target, fused);
            return;
        }

        /* The other side must be null or undefined. */
        RegisterID reg = frame.ownRegForType(test);
        frame.pop();
        frame.pop();

        /*
         * :FIXME: Easier test for undefined || null?
         * Maybe put them next to each other, subtract, do a single compare?
         */

        if (target) {
            frame.forgetEverything();

            if ((op == JSOP_EQ && fused == JSOP_IFNE) ||
                (op == JSOP_NE && fused == JSOP_IFEQ)) {
                Jump j = masm.branchPtr(Assembler::Equal, reg, ImmType(JSVAL_TYPE_UNDEFINED));
                jumpAndTrace(j, target);
                j = masm.branchPtr(Assembler::Equal, reg, ImmType(JSVAL_TYPE_NULL));
                jumpAndTrace(j, target);
            } else {
                Jump j = masm.branchPtr(Assembler::Equal, reg, ImmType(JSVAL_TYPE_UNDEFINED));
                Jump j2 = masm.branchPtr(Assembler::NotEqual, reg, ImmType(JSVAL_TYPE_NULL));
                jumpAndTrace(j2, target);
                j.linkTo(masm.label(), &masm);
            }
        } else {
            Jump j = masm.branchPtr(Assembler::Equal, reg, ImmType(JSVAL_TYPE_UNDEFINED));
            Jump j2 = masm.branchPtr(Assembler::Equal, reg, ImmType(JSVAL_TYPE_NULL));
            masm.move(Imm32(op == JSOP_NE), reg);
            Jump j3 = masm.jump();
            j2.linkTo(masm.label(), &masm);
            j.linkTo(masm.label(), &masm);
            masm.move(Imm32(op == JSOP_EQ), reg);
            j3.linkTo(masm.label(), &masm);
            frame.pushTypedPayload(JSVAL_TYPE_BOOLEAN, reg);
        }
        return;
    }

    emitStubCmpOp(stub, target, fused);
}

void
mjit::Compiler::jsop_relational(JSOp op, BoolStub stub, jsbytecode *target, JSOp fused)
{
    FrameEntry *rhs = frame.peek(-1);
    FrameEntry *lhs = frame.peek(-2);

    /* The compiler should have handled constant folding. */
    JS_ASSERT(!(rhs->isConstant() && lhs->isConstant()));

    /* Always slow path... */
    if ((lhs->isNotType(JSVAL_TYPE_INT32) && lhs->isNotType(JSVAL_TYPE_DOUBLE) &&
         lhs->isNotType(JSVAL_TYPE_STRING)) ||
        (rhs->isNotType(JSVAL_TYPE_INT32) && rhs->isNotType(JSVAL_TYPE_DOUBLE) &&
         rhs->isNotType(JSVAL_TYPE_STRING))) {
        if (op == JSOP_EQ || op == JSOP_NE)
            jsop_equality(op, stub, target, fused);
        else
            emitStubCmpOp(stub, target, fused);
        return;
    }

    if (op == JSOP_EQ || op == JSOP_NE) {
        if ((lhs->isNotType(JSVAL_TYPE_INT32) && lhs->isNotType(JSVAL_TYPE_STRING)) ||
            (rhs->isNotType(JSVAL_TYPE_INT32) && rhs->isNotType(JSVAL_TYPE_STRING))) {
            emitStubCmpOp(stub, target, fused);
        } else if (!target && (lhs->isType(JSVAL_TYPE_STRING) || rhs->isType(JSVAL_TYPE_STRING))) {
            emitStubCmpOp(stub, target, fused);
        } else {
            jsop_equality_int_string(op, stub, target, fused);
        }
        return;
    }

    if (frame.haveSameBacking(lhs, rhs)) {
        jsop_relational_self(op, stub, target, fused);
    } else if (lhs->isType(JSVAL_TYPE_STRING) || rhs->isType(JSVAL_TYPE_STRING)) {
        emitStubCmpOp(stub, target, fused);
    } else if (lhs->isType(JSVAL_TYPE_DOUBLE) || rhs->isType(JSVAL_TYPE_DOUBLE)) {
        jsop_relational_double(op, stub, target, fused);
    } else {
        jsop_relational_full(op, stub, target, fused);
    }
}

void
mjit::Compiler::jsop_not()
{
    FrameEntry *top = frame.peek(-1);

    if (top->isConstant()) {
        const Value &v = top->getValue();
        frame.pop();
        frame.push(BooleanValue(!js_ValueToBoolean(v)));
        return;
    }

    if (top->isTypeKnown()) {
        JSValueType type = top->getKnownType();
        switch (type) {
          case JSVAL_TYPE_INT32:
          {
            RegisterID data = frame.allocReg(Registers::SingleByteRegs);
            if (frame.shouldAvoidDataRemat(top))
                masm.loadPayload(frame.addressOf(top), data);
            else
                masm.move(frame.tempRegForData(top), data);

            masm.set32(Assembler::Equal, data, Imm32(0), data);

            frame.pop();
            frame.pushTypedPayload(JSVAL_TYPE_BOOLEAN, data);
            break;
          }

          case JSVAL_TYPE_BOOLEAN:
          {
            RegisterID reg = frame.ownRegForData(top);

            masm.xor32(Imm32(1), reg);

            frame.pop();
            frame.pushTypedPayload(JSVAL_TYPE_BOOLEAN, reg);
            break;
          }

          case JSVAL_TYPE_OBJECT:
          {
            frame.pop();
            frame.push(BooleanValue(false));
            break;
          }

          default:
          {
            prepareStubCall(Uses(1));
            stubCall(stubs::ValueToBoolean);

            RegisterID reg = Registers::ReturnReg;
            frame.takeReg(reg);
            masm.xor32(Imm32(1), reg);

            frame.pop();
            frame.pushTypedPayload(JSVAL_TYPE_BOOLEAN, reg);
            break;
          }
        }

        return;
    }

    RegisterID data = frame.allocReg(Registers::SingleByteRegs);
    if (frame.shouldAvoidDataRemat(top))
        masm.loadPayload(frame.addressOf(top), data);
    else
        masm.move(frame.tempRegForData(top), data);
    RegisterID type = frame.tempRegForType(top);
    Label syncTarget = stubcc.syncExitAndJump(Uses(1));


    /* Inline path is for booleans. */
    Jump jmpNotBool = masm.testBoolean(Assembler::NotEqual, type);
    masm.xor32(Imm32(1), data);


    /* OOL path is for int + object. */
    Label lblMaybeInt32 = stubcc.masm.label();

    Jump jmpNotInt32 = stubcc.masm.testInt32(Assembler::NotEqual, type);
    stubcc.masm.set32(Assembler::Equal, data, Imm32(0), data);
    Jump jmpInt32Exit = stubcc.masm.jump();

    Label lblMaybeObject = stubcc.masm.label();
    Jump jmpNotObject = stubcc.masm.testPrimitive(Assembler::Equal, type);
    stubcc.masm.move(Imm32(0), data);
    Jump jmpObjectExit = stubcc.masm.jump();


    /* Rejoin location. */
    Label lblRejoin = masm.label();

    /* Patch up jumps. */
    stubcc.linkExitDirect(jmpNotBool, lblMaybeInt32);

    jmpNotInt32.linkTo(lblMaybeObject, &stubcc.masm);
    stubcc.crossJump(jmpInt32Exit, lblRejoin);

    jmpNotObject.linkTo(syncTarget, &stubcc.masm);
    stubcc.crossJump(jmpObjectExit, lblRejoin);
    

    /* Leave. */
    stubcc.leave();
    stubcc.call(stubs::Not);

    frame.pop();
    frame.pushTypedPayload(JSVAL_TYPE_BOOLEAN, data);

    stubcc.rejoin(Changes(1));
}

void
mjit::Compiler::jsop_typeof()
{
    FrameEntry *fe = frame.peek(-1);

    if (fe->isTypeKnown()) {
        JSRuntime *rt = cx->runtime;

        JSAtom *atom = NULL;
        switch (fe->getKnownType()) {
          case JSVAL_TYPE_STRING:
            atom = rt->atomState.typeAtoms[JSTYPE_STRING];
            break;
          case JSVAL_TYPE_UNDEFINED:
            atom = rt->atomState.typeAtoms[JSTYPE_VOID];
            break;
          case JSVAL_TYPE_NULL:
            atom = rt->atomState.typeAtoms[JSTYPE_OBJECT];
            break;
          case JSVAL_TYPE_OBJECT:
            atom = NULL;
            break;
          case JSVAL_TYPE_BOOLEAN:
            atom = rt->atomState.typeAtoms[JSTYPE_BOOLEAN];
            break;
          default:
            atom = rt->atomState.typeAtoms[JSTYPE_NUMBER];
            break;
        }

        if (atom) {
            frame.pop();
            frame.push(StringValue(ATOM_TO_STRING(atom)));
            return;
        }
    }

    prepareStubCall(Uses(1));
    stubCall(stubs::TypeOf);
    frame.pop();
    frame.takeReg(Registers::ReturnReg);
    frame.pushTypedPayload(JSVAL_TYPE_STRING, Registers::ReturnReg);
}

void
mjit::Compiler::booleanJumpScript(JSOp op, jsbytecode *target)
{
    FrameEntry *fe = frame.peek(-1);

    MaybeRegisterID type;
    MaybeRegisterID data;

    if (!fe->isTypeKnown() && !frame.shouldAvoidTypeRemat(fe))
        type.setReg(frame.copyTypeIntoReg(fe));
    data.setReg(frame.copyDataIntoReg(fe));

    /* :FIXME: Can something more lightweight be used? */
    frame.forgetEverything();

    Assembler::Condition cond = (op == JSOP_IFNE || op == JSOP_OR)
                                ? Assembler::NonZero
                                : Assembler::Zero;
    Assembler::Condition ncond = (op == JSOP_IFNE || op == JSOP_OR)
                                 ? Assembler::Zero
                                 : Assembler::NonZero;

    /* Inline path: Boolean guard + call script. */
    MaybeJump jmpNotBool;
    MaybeJump jmpNotExecScript;
    if (type.isSet()) {
        jmpNotBool.setJump(masm.testBoolean(Assembler::NotEqual, type.reg()));
    } else {
        if (!fe->isTypeKnown()) {
            jmpNotBool.setJump(masm.testBoolean(Assembler::NotEqual,
                                                frame.addressOf(fe)));
        } else if (fe->isNotType(JSVAL_TYPE_BOOLEAN) &&
                   fe->isNotType(JSVAL_TYPE_INT32)) {
            jmpNotBool.setJump(masm.jump());
        }
    }

    /* 
     * TODO: We don't need the second jump if
     * jumpInScript() can go from ool path to inline path.
     */
    jmpNotExecScript.setJump(masm.branchTest32(ncond, data.reg(), data.reg()));
    Label lblExecScript = masm.label();
    Jump j = masm.jump();


    /* OOL path: Conversion to boolean. */
    MaybeJump jmpCvtExecScript;
    MaybeJump jmpCvtRejoin;
    Label lblCvtPath = stubcc.masm.label();

    if (!fe->isTypeKnown() ||
        !(fe->isType(JSVAL_TYPE_BOOLEAN) || fe->isType(JSVAL_TYPE_INT32))) {
        stubcc.masm.fixScriptStack(frame.frameDepth());
        stubcc.masm.setupVMFrame();
#if defined(JS_NO_FASTCALL) && defined(JS_CPU_X86)
        stubcc.masm.push(Registers::ArgReg0);
#endif
        stubcc.masm.call(JS_FUNC_TO_DATA_PTR(void *, stubs::ValueToBoolean));
#if defined(JS_NO_FASTCALL) && defined(JS_CPU_X86)
        stubcc.masm.pop();
#endif

        jmpCvtExecScript.setJump(stubcc.masm.branchTest32(cond, Registers::ReturnReg,
                                                          Registers::ReturnReg));
        jmpCvtRejoin.setJump(stubcc.masm.jump());
    }

    /* Rejoin tag. */
    Label lblAfterScript = masm.label();

    /* Patch up jumps. */
    if (jmpNotBool.isSet())
        stubcc.linkExitDirect(jmpNotBool.getJump(), lblCvtPath);
    if (jmpNotExecScript.isSet())
        jmpNotExecScript.getJump().linkTo(lblAfterScript, &masm);

    if (jmpCvtExecScript.isSet())
        stubcc.crossJump(jmpCvtExecScript.getJump(), lblExecScript);
    if (jmpCvtRejoin.isSet())
        stubcc.crossJump(jmpCvtRejoin.getJump(), lblAfterScript);

    frame.pop();

    jumpAndTrace(j, target);
}

void
mjit::Compiler::jsop_ifneq(JSOp op, jsbytecode *target)
{
    FrameEntry *fe = frame.peek(-1);

    if (fe->isConstant()) {
        JSBool b = js_ValueToBoolean(fe->getValue());

        frame.pop();

        if (op == JSOP_IFEQ)
            b = !b;
        if (b) {
            frame.forgetEverything();
            jumpAndTrace(masm.jump(), target);
        }
        return;
    }

    booleanJumpScript(op, target);
}

void
mjit::Compiler::jsop_andor(JSOp op, jsbytecode *target)
{
    FrameEntry *fe = frame.peek(-1);

    if (fe->isConstant()) {
        JSBool b = js_ValueToBoolean(fe->getValue());
        
        /* Short-circuit. */
        if ((op == JSOP_OR && b == JS_TRUE) ||
            (op == JSOP_AND && b == JS_FALSE)) {
            frame.forgetEverything();
            jumpAndTrace(masm.jump(), target);
        }

        frame.pop();
        return;
    }

    booleanJumpScript(op, target);
}

void
mjit::Compiler::jsop_localinc(JSOp op, uint32 slot, bool popped)
{
    bool post = (op == JSOP_LOCALINC || op == JSOP_LOCALDEC);
    int32 amt = (op == JSOP_INCLOCAL || op == JSOP_LOCALINC) ? 1 : -1;

    frame.pushLocal(slot);

    FrameEntry *fe = frame.peek(-1);

    if (fe->isConstant() && fe->getValue().isPrimitive()) {
        Value v = fe->getValue();
        double d;
        ValueToNumber(cx, v, &d);
        if (post) {
            frame.push(NumberValue(d + amt));
            frame.storeLocal(slot);
            frame.pop();
        } else {
            frame.pop();
            frame.push(NumberValue(d + amt));
            frame.storeLocal(slot);
        }
        if (popped)
            frame.pop();
        return;
    }

    /*
     * If the local variable is not known to be an int32, or the pre-value
     * is observed, then do the simple thing and decompose x++ into simpler
     * opcodes.
     */
    if (fe->isNotType(JSVAL_TYPE_INT32) || (post && !popped)) {
        /* V */
        jsop_pos();
        /* N */

        if (post && !popped) {
            frame.dup();
            /* N N */
        }

        frame.push(Int32Value(1));
        /* N? N 1 */

        if (amt == 1)
            jsop_binary(JSOP_ADD, stubs::Add);
        else
            jsop_binary(JSOP_SUB, stubs::Sub);
        /* N? N+1 */

        frame.storeLocal(slot, post || popped);
        /* N? N+1 */

        if (post || popped)
            frame.pop();

        return;
    }

    /* If the pre value is not observed, we can emit better code. */
    if (!fe->isTypeKnown()) {
        Jump intFail = frame.testInt32(Assembler::NotEqual, fe);
        stubcc.linkExit(intFail, Uses(1));
    }

    RegisterID reg = frame.copyDataIntoReg(fe);

    Jump ovf;
    if (amt > 0)
        ovf = masm.branchAdd32(Assembler::Overflow, Imm32(1), reg);
    else
        ovf = masm.branchSub32(Assembler::Overflow, Imm32(1), reg);
    stubcc.linkExit(ovf, Uses(1));

    /* Note, stub call will push the original value again no matter what. */
    stubcc.leave();

    stubcc.masm.move(Imm32(slot), Registers::ArgReg1);
    if (op == JSOP_LOCALINC || op == JSOP_INCLOCAL)
        stubcc.call(stubs::IncLocal);
    else
        stubcc.call(stubs::DecLocal);

    frame.pop();
    frame.pushTypedPayload(JSVAL_TYPE_INT32, reg);
    frame.storeLocal(slot, popped, false);

    if (popped)
        frame.pop();
    else
        frame.forgetType(frame.peek(-1));

    stubcc.rejoin(Changes(0));
}

void
mjit::Compiler::jsop_arginc(JSOp op, uint32 slot, bool popped)
{
    int amt = (js_CodeSpec[op].format & JOF_INC) ? 1 : -1;
    bool post = !!(js_CodeSpec[op].format & JOF_POST);
    uint32 depth = frame.stackDepth();

    jsop_getarg(slot);
    if (post && !popped)
        frame.dup();

    FrameEntry *fe = frame.peek(-1);
    Jump notInt = frame.testInt32(Assembler::NotEqual, fe);
    stubcc.linkExit(notInt, Uses(0));

    RegisterID reg = frame.ownRegForData(fe);
    frame.pop();

    Jump ovf;
    if (amt > 0)
        ovf = masm.branchAdd32(Assembler::Overflow, Imm32(1), reg);
    else
        ovf = masm.branchSub32(Assembler::Overflow, Imm32(1), reg);
    stubcc.linkExit(ovf, Uses(0));

    stubcc.leave();
    stubcc.masm.addPtr(Imm32(JSStackFrame::offsetOfFormalArg(fun, slot)),
                       JSFrameReg, Registers::ArgReg1);
    stubcc.vpInc(op, depth);

    frame.pushTypedPayload(JSVAL_TYPE_INT32, reg);
    fe = frame.peek(-1);

    Address address = Address(JSFrameReg, JSStackFrame::offsetOfFormalArg(fun, slot));
    frame.storeTo(fe, address, popped);

    if (post || popped)
        frame.pop();
    else
        frame.forgetType(fe);

    stubcc.rejoin(Changes((post || popped) ? 0 : 1));
}

void
mjit::Compiler::jsop_setelem()
{
    FrameEntry *obj = frame.peek(-3);
    FrameEntry *id = frame.peek(-2);
    FrameEntry *fe = frame.peek(-1);

    if (obj->isNotType(JSVAL_TYPE_OBJECT) || id->isNotType(JSVAL_TYPE_INT32) ||
        (id->isConstant() && id->getValue().toInt32() < 0)) {
        jsop_setelem_slow();
        return;
    }

    /* id.isInt32() */
    if (!id->isTypeKnown()) {
        Jump j = frame.testInt32(Assembler::NotEqual, id);
        stubcc.linkExit(j, Uses(3));
    }

    /* obj.isObject() */
    if (!obj->isTypeKnown()) {
        Jump j = frame.testObject(Assembler::NotEqual, obj);
        stubcc.linkExit(j, Uses(3));
    }

    /* obj.isDenseArray() */
    RegisterID objReg = frame.copyDataIntoReg(obj);
    Jump guardDense = masm.branchPtr(Assembler::NotEqual,
                                      Address(objReg, offsetof(JSObject, clasp)),
                                      ImmPtr(&js_ArrayClass));
    stubcc.linkExit(guardDense, Uses(3));

    /* guard within capacity */
    Address capacity(objReg, offsetof(JSObject, fslots) +
                             JSObject::JSSLOT_DENSE_ARRAY_CAPACITY * sizeof(Value));

    Jump inRange;
    MaybeRegisterID maybeIdReg;
    if (id->isConstant()) {
        inRange = masm.branch32(Assembler::LessThanOrEqual,
                                masm.payloadOf(capacity),
                                Imm32(id->getValue().toInt32()));
    } else {
        maybeIdReg = frame.copyDataIntoReg(id);
        inRange = masm.branch32(Assembler::AboveOrEqual, maybeIdReg.reg(),
                                masm.payloadOf(capacity));
    }
    stubcc.linkExit(inRange, Uses(3));

    /* dslots non-NULL */
    masm.loadPtr(Address(objReg, offsetof(JSObject, dslots)), objReg);
    Jump guardSlots = masm.branchTestPtr(Assembler::Zero, objReg, objReg);
    stubcc.linkExit(guardSlots, Uses(3));

    /* guard within capacity */
    if (id->isConstant()) {
        /* guard not a hole */
        Address slot(objReg, id->getValue().toInt32() * sizeof(Value));
#if defined JS_NUNBOX32
        Jump notHole = masm.branch32(Assembler::Equal, masm.tagOf(slot), ImmType(JSVAL_TYPE_MAGIC));
#elif defined JS_PUNBOX64
        masm.loadTypeTag(slot, Registers::ValueReg);
        Jump notHole = masm.branchPtr(Assembler::Equal, Registers::ValueReg, ImmType(JSVAL_TYPE_MAGIC));
#endif
        stubcc.linkExit(notHole, Uses(3));

        stubcc.leave();
        stubcc.call(stubs::SetElem);

        /* Infallible, start killing everything. */
        frame.eviscerate(obj);
        frame.eviscerate(id);

        /* Perform the store. */
        if (fe->isConstant()) {
            masm.storeValue(fe->getValue(), slot);
        } else {
            masm.storePayload(frame.tempRegForData(fe), slot);
            if (fe->isTypeKnown())
                masm.storeTypeTag(ImmType(fe->getKnownType()), slot);
            else
                masm.storeTypeTag(frame.tempRegForType(fe), slot);
        }
    } else {
        RegisterID idReg = maybeIdReg.reg();

        /*
         * Register for use only in OOL hole path. TODO: would be nice
         * for the frame to do any associated spilling in the OOL path.
         */
        RegisterID T1 = frame.allocReg();

        Label syncTarget = stubcc.syncExitAndJump(Uses(3));

        /* guard not a hole */
        BaseIndex slot(objReg, idReg, Assembler::JSVAL_SCALE);
#if defined JS_NUNBOX32
        Jump notHole = masm.branch32(Assembler::Equal, masm.tagOf(slot), ImmType(JSVAL_TYPE_MAGIC));
#elif defined JS_PUNBOX64
        masm.loadTypeTag(slot, Registers::ValueReg);
        Jump notHole = masm.branchPtr(Assembler::Equal, Registers::ValueReg, ImmType(JSVAL_TYPE_MAGIC));
#endif

        /* Make an OOL path for setting array holes. */
        Label lblHole = stubcc.masm.label();
        stubcc.linkExitDirect(notHole, lblHole);

        /* Need a new handle on the object, as objReg now holds the dslots. */
        RegisterID baseReg = frame.tempRegForData(obj, objReg, stubcc.masm);

        /*
         * Check if the object has a prototype with indexed properties,
         * in which case it might have a setter for this element. For dense
         * arrays we need to check only Array.prototype and Object.prototype.
         * Indexed properties are indicated by the JSObject::INDEXED flag.
         */

        /* Test for indexed properties in Array.prototype. */
        stubcc.masm.loadPtr(Address(baseReg, offsetof(JSObject, proto)), T1);
        stubcc.masm.loadPtr(Address(T1, offsetof(JSObject, flags)), T1);
        Jump extendedArray = stubcc.masm.branchTest32(Assembler::NonZero, T1, Imm32(JSObject::INDEXED));
        extendedArray.linkTo(syncTarget, &stubcc.masm);

        /* Test for indexed properties in Object.prototype. */
        stubcc.masm.loadPtr(Address(baseReg, offsetof(JSObject, proto)), T1);
        stubcc.masm.loadPtr(Address(T1, offsetof(JSObject, proto)), T1);
        stubcc.masm.loadPtr(Address(T1, offsetof(JSObject, flags)), T1);
        Jump extendedObject = stubcc.masm.branchTest32(Assembler::NonZero, T1, Imm32(JSObject::INDEXED));
        extendedObject.linkTo(syncTarget, &stubcc.masm);

        /* Update the array length if needed. Don't worry about overflow. */
        Address arrayLength(baseReg, offsetof(JSObject, fslots[JSObject::JSSLOT_ARRAY_LENGTH]));
        stubcc.masm.loadPayload(arrayLength, T1);
        Jump underLength = stubcc.masm.branch32(Assembler::LessThan, idReg, T1);
        stubcc.masm.move(idReg, T1);
        stubcc.masm.add32(Imm32(1), T1);
        stubcc.masm.storePayload(T1, arrayLength);
        underLength.linkTo(stubcc.masm.label(), &stubcc.masm);

        /* Restore the dslots register if we clobbered it with the object. */
        if (baseReg == objReg)
            stubcc.masm.loadPtr(Address(objReg, offsetof(JSObject, dslots)), objReg);

        /* Rejoin OOL path with inline path to do the store itself. */
        Jump jmpHoleExit = stubcc.masm.jump();
        Label lblRejoin = masm.label();
        stubcc.crossJump(jmpHoleExit, lblRejoin);

        stubcc.leave();
        stubcc.call(stubs::SetElem);

        /* Infallible, start killing everything. */
        frame.eviscerate(obj);
        frame.eviscerate(id);

        /* Perform the store. */
        if (fe->isConstant()) {
            masm.storeValue(fe->getValue(), slot);
        } else {
            masm.storePayload(frame.tempRegForData(fe), slot);
            if (fe->isTypeKnown())
                masm.storeTypeTag(ImmType(fe->getKnownType()), slot);
            else
                masm.storeTypeTag(frame.tempRegForType(fe), slot);
        }

        frame.freeReg(idReg);
        frame.freeReg(T1);
    }
    frame.freeReg(objReg);

    frame.shimmy(2);
    stubcc.rejoin(Changes(0));
}

void
mjit::Compiler::jsop_getelem_dense(FrameEntry *obj, FrameEntry *id, RegisterID objReg,
                                   MaybeRegisterID &idReg, RegisterID tmpReg)
{
    /* Note: idReg is only valid if id is not a constant. */
    Jump guardDense = masm.branchPtr(Assembler::NotEqual,
                                     Address(objReg, offsetof(JSObject, clasp)),
                                     ImmPtr(&js_ArrayClass));
    stubcc.linkExit(guardDense, Uses(2));

    /* Guard within capacity. */
    Jump inRange;
    Address capacity(objReg, offsetof(JSObject, fslots) +
                             JSObject::JSSLOT_DENSE_ARRAY_CAPACITY * sizeof(Value));
    if (id->isConstant()) {
        inRange = masm.branch32(Assembler::LessThanOrEqual,
                                masm.payloadOf(capacity),
                                Imm32(id->getValue().toInt32()));
    } else {
        inRange = masm.branch32(Assembler::AboveOrEqual, idReg.reg(),
                                masm.payloadOf(capacity));
    }
    stubcc.linkExit(inRange, Uses(2));

    /* dslots non-NULL */
    masm.loadPtr(Address(objReg, offsetof(JSObject, dslots)), objReg);
    Jump guardSlots = masm.branchTestPtr(Assembler::Zero, objReg, objReg);
    stubcc.linkExit(guardSlots, Uses(2));

    /* guard within capacity */
    if (id->isConstant()) {
        /* guard not a hole */
        Address slot(objReg, id->getValue().toInt32() * sizeof(Value));
#if defined JS_NUNBOX32
        Jump notHole = masm.branch32(Assembler::Equal, masm.tagOf(slot), ImmType(JSVAL_TYPE_MAGIC));
#elif defined JS_PUNBOX64
        masm.loadTypeTag(slot, Registers::ValueReg);
        Jump notHole = masm.branchPtr(Assembler::Equal, Registers::ValueReg, ImmType(JSVAL_TYPE_MAGIC));
#endif
        stubcc.linkExit(notHole, Uses(2));

        /* Load slot address into regs. */
        masm.loadTypeTag(slot, tmpReg);
        masm.loadPayload(slot, objReg);
    } else {
        /* guard not a hole */
        BaseIndex slot(objReg, idReg.reg(), Assembler::JSVAL_SCALE);
#if defined JS_NUNBOX32
        Jump notHole = masm.branch32(Assembler::Equal, masm.tagOf(slot), ImmType(JSVAL_TYPE_MAGIC));
#elif defined JS_PUNBOX64
        masm.loadTypeTag(slot, Registers::ValueReg);
        Jump notHole = masm.branchPtr(Assembler::Equal, Registers::ValueReg, ImmType(JSVAL_TYPE_MAGIC));
#endif
        stubcc.linkExit(notHole, Uses(2));

        masm.loadTypeTag(slot, tmpReg);
        masm.loadPayload(slot, objReg);
    }
    /* Postcondition: type must be in tmpReg, data must be in objReg. */

    /* Note: linkExits will be hooked up to a leave() after this method completes. */
}

void
mjit::Compiler::jsop_getelem_known_type(FrameEntry *obj, FrameEntry *id, RegisterID tmpReg)
{
    switch (id->getKnownType()) {
      case JSVAL_TYPE_INT32:
      {
        /* Prologue. */
        RegisterID objReg = frame.copyDataIntoReg(obj);
        MaybeRegisterID idReg;
        if (!id->isConstant())
            idReg.setReg(frame.copyDataIntoReg(id));

        /* Meat. */
        jsop_getelem_dense(obj, id, objReg, idReg, tmpReg);
        stubcc.leave();
        stubcc.call(stubs::GetElem);

        /* Epilogue. */
        if (idReg.isSet())
            frame.freeReg(idReg.reg());
        frame.popn(2);
        frame.pushRegs(tmpReg, objReg);
        stubcc.rejoin(Changes(1));
        return;
      }
#ifdef JS_POLYIC
      case JSVAL_TYPE_STRING:
      {
        /* Prologue. */
        RegisterID objReg = frame.copyDataIntoReg(obj);
        RegisterID idReg = frame.copyDataIntoReg(id);

        /* Meat. */
        jsop_getelem_pic(obj, id, objReg, idReg, tmpReg);

        /* Epilogue. */
        frame.popn(2);
        frame.pushRegs(tmpReg, objReg);
        frame.freeReg(idReg);
        stubcc.rejoin(Changes(1));
        return;
      }
#endif
      default:
        JS_NOT_REACHED("Invalid known id type.");
    }
}

#ifdef JS_POLYIC
void
mjit::Compiler::jsop_getelem_with_pic(FrameEntry *obj, FrameEntry *id, RegisterID tmpReg)
{
    JS_ASSERT(!id->isTypeKnown());
    RegisterID objReg = frame.copyDataIntoReg(obj);
    MaybeRegisterID idReg(frame.copyDataIntoReg(id));

    RegisterID typeReg = frame.tempRegForType(id, tmpReg);
    Jump intGuard = masm.testInt32(Assembler::NotEqual, typeReg);

    JaegerSpew(JSpew_Insns, " ==== BEGIN DENSE ARRAY CODE ==== \n");

    jsop_getelem_dense(obj, id, objReg, idReg, tmpReg);
    Jump performedDense = masm.jump();

    JaegerSpew(JSpew_Insns, " ==== END DENSE ARRAY CODE ==== \n");

    intGuard.linkTo(masm.label(), &masm);
    Jump stringGuard = masm.testString(Assembler::NotEqual, typeReg);
    stubcc.linkExit(stringGuard, Uses(2)); /* Neither int nor string at this point. */

    stubcc.leave();
    stubcc.call(stubs::GetElem);
    Jump toFinalMerge = stubcc.masm.jump();

    jsop_getelem_pic(obj, id, objReg, idReg.reg(), tmpReg);
    performedDense.linkTo(masm.label(), &masm);
    frame.popn(2);
    frame.pushRegs(tmpReg, objReg);
    frame.freeReg(idReg.reg());
    toFinalMerge.linkTo(stubcc.masm.label(), &stubcc.masm);
    stubcc.rejoin(Changes(1));
}
#endif

void
mjit::Compiler::jsop_getelem_nopic(FrameEntry *obj, FrameEntry *id, RegisterID tmpReg)
{
    /* Only handle the int32 case. */
    RegisterID objReg = frame.copyDataIntoReg(obj);
    MaybeRegisterID idReg(frame.copyDataIntoReg(id));
    RegisterID typeReg = frame.tempRegForType(id, tmpReg);
    Jump intGuard = masm.testInt32(Assembler::NotEqual, typeReg);
    stubcc.linkExit(intGuard, Uses(2));

    /* Meat. */
    jsop_getelem_dense(obj, id, objReg, idReg, tmpReg);
    stubcc.leave();
    stubcc.call(stubs::GetElem);

    /* Epilogue. */
    frame.freeReg(idReg.reg());
    frame.popn(2);
    frame.pushRegs(tmpReg, objReg);
    stubcc.rejoin(Changes(1));
}

void
mjit::Compiler::jsop_getelem()
{
    FrameEntry *obj = frame.peek(-2);
    FrameEntry *id = frame.peek(-1);

    if (obj->isTypeKnown() && obj->getKnownType() != JSVAL_TYPE_OBJECT) {
        jsop_getelem_slow();
        return;
    }

    if (id->isTypeKnown() &&
        !(id->getKnownType() == JSVAL_TYPE_INT32
#ifdef JS_POLYIC
          || id->getKnownType() == JSVAL_TYPE_STRING
#endif
         )) {
        jsop_getelem_slow();
        return;
    }

    if (id->isTypeKnown() && id->getKnownType() == JSVAL_TYPE_INT32 && id->isConstant() &&
        id->getValue().toInt32() < 0) {
        jsop_getelem_slow();
        return;
    }

    if (id->isTypeKnown() && id->getKnownType() == JSVAL_TYPE_STRING && id->isConstant()) {
        /* Never happens, or I'd optimize it. */
        jsop_getelem_slow();
        return;
    }

    RegisterID tmpReg;
    if (obj->isTypeKnown()) {
        tmpReg = frame.allocReg();
    } else {
        tmpReg = frame.copyTypeIntoReg(obj);
        Jump objGuard = masm.testObject(Assembler::NotEqual, tmpReg);
        stubcc.linkExit(objGuard, Uses(2));
    }

    if (id->isTypeKnown())
        return jsop_getelem_known_type(obj, id, tmpReg);

#ifdef JS_POLYIC
    return jsop_getelem_with_pic(obj, id, tmpReg);
#else
    return jsop_getelem_nopic(obj, id, tmpReg);
#endif
}

static inline bool
ReallySimpleStrictTest(FrameEntry *fe)
{
    if (!fe->isTypeKnown())
        return false;
    JSValueType type = fe->getKnownType();
    return type == JSVAL_TYPE_NULL || type == JSVAL_TYPE_UNDEFINED;
}

static inline bool
BooleanStrictTest(FrameEntry *fe)
{
    return fe->isConstant() && fe->getKnownType() == JSVAL_TYPE_BOOLEAN;
}

void
mjit::Compiler::jsop_stricteq(JSOp op)
{
    FrameEntry *rhs = frame.peek(-1);
    FrameEntry *lhs = frame.peek(-2);

    Assembler::Condition cond = (op == JSOP_STRICTEQ) ? Assembler::Equal : Assembler::NotEqual;

    /*
     * NB: x64 can do full-Value comparisons. This is beneficial
     * to do if the payload/type are not yet in registers.
     */

    /* Constant-fold. */
    if (lhs->isConstant() && rhs->isConstant()) {
        bool b = StrictlyEqual(cx, lhs->getValue(), rhs->getValue());
        frame.popn(2);
        frame.push(BooleanValue((op == JSOP_STRICTEQ) ? b : !b));
        return;
    }

    if (frame.haveSameBacking(lhs, rhs)) {
        /* False iff NaN. */
        if (lhs->isTypeKnown() && lhs->isNotType(JSVAL_TYPE_DOUBLE)) {
            frame.popn(2);
            frame.push(BooleanValue(true));
            return;
        }
        
        /* Assume NaN is in canonical form. */
        RegisterID result = frame.allocReg(Registers::SingleByteRegs);
        RegisterID treg = frame.tempRegForType(lhs);

        Assembler::Condition oppositeCond = (op == JSOP_STRICTEQ) ? Assembler::NotEqual : Assembler::Equal;

#if defined JS_CPU_X86 || defined JS_CPU_ARM
        static const int CanonicalNaNType = 0x7FF80000;
        masm.setPtr(oppositeCond, treg, Imm32(CanonicalNaNType), result);
#elif defined JS_CPU_X64
        static const void *CanonicalNaNType = (void *)0x7FF8000000000000; 
        masm.move(ImmPtr(CanonicalNaNType), JSC::X86Registers::r11);
        masm.setPtr(oppositeCond, treg, JSC::X86Registers::r11, result);
#endif

        frame.popn(2);
        frame.pushTypedPayload(JSVAL_TYPE_BOOLEAN, result);
        return;
    }

    /* Comparison against undefined or null is super easy. */
    bool lhsTest;
    if ((lhsTest = ReallySimpleStrictTest(lhs)) || ReallySimpleStrictTest(rhs)) {
        FrameEntry *test = lhsTest ? rhs : lhs;
        FrameEntry *known = lhsTest ? lhs : rhs;

        if (test->isTypeKnown()) {
            frame.popn(2);
            frame.push(BooleanValue((test->getKnownType() == known->getKnownType()) ==
                                  (op == JSOP_STRICTEQ)));
            return;
        }

        /* This is only true if the other side is |null|. */
        RegisterID result = frame.allocReg(Registers::SingleByteRegs);
#if defined JS_CPU_X86 || defined JS_CPU_ARM
        JSValueTag mask = known->getKnownTag();
        if (frame.shouldAvoidTypeRemat(test))
            masm.set32(cond, masm.tagOf(frame.addressOf(test)), Imm32(mask), result);
        else
            masm.set32(cond, frame.tempRegForType(test), Imm32(mask), result);
#elif defined JS_CPU_X64
        RegisterID maskReg = frame.allocReg();
        masm.move(Imm64(known->getKnownShiftedTag()), maskReg);

        RegisterID r = frame.tempRegForType(test);
        masm.setPtr(cond, r, maskReg, result);
        frame.freeReg(maskReg);
#endif
        frame.popn(2);
        frame.pushTypedPayload(JSVAL_TYPE_BOOLEAN, result);
        return;
    }

    /* Hardcoded booleans are easy too. */
    if ((lhsTest = BooleanStrictTest(lhs)) || BooleanStrictTest(rhs)) {
        FrameEntry *test = lhsTest ? rhs : lhs;

        if (test->isTypeKnown() && test->isNotType(JSVAL_TYPE_BOOLEAN)) {
            frame.popn(2);
            frame.push(BooleanValue(op == JSOP_STRICTNE));
            return;
        }

        if (test->isConstant()) {
            frame.popn(2);
            const Value &L = lhs->getValue();
            const Value &R = rhs->getValue();
            frame.push(BooleanValue((L.toBoolean() == R.toBoolean()) == (op == JSOP_STRICTEQ)));
            return;
        }

        RegisterID result = frame.allocReg(Registers::SingleByteRegs);
        
        /* Is the other side boolean? */
        Jump notBoolean;
        if (!test->isTypeKnown())
           notBoolean = frame.testBoolean(Assembler::NotEqual, test);

        /* Do a dynamic test. */
        bool val = lhsTest ? lhs->getValue().toBoolean() : rhs->getValue().toBoolean();
#if defined JS_CPU_X86 || defined JS_CPU_ARM
        if (frame.shouldAvoidDataRemat(test))
            masm.set32(cond, masm.payloadOf(frame.addressOf(test)), Imm32(val), result);
        else
            masm.set32(cond, frame.tempRegForData(test), Imm32(val), result);
#elif defined JS_CPU_X64
        RegisterID r = frame.tempRegForData(test);
        masm.set32(cond, r, Imm32(val), result);
#endif

        if (!test->isTypeKnown()) {
            Jump done = masm.jump();
            notBoolean.linkTo(masm.label(), &masm);
            masm.move(Imm32((op == JSOP_STRICTNE)), result);
            done.linkTo(masm.label(), &masm);
        }

        frame.popn(2);
        frame.pushTypedPayload(JSVAL_TYPE_BOOLEAN, result);
        return;
    }

    /* Is it impossible that both Values are ints? */
    if ((lhs->isTypeKnown() && lhs->isNotType(JSVAL_TYPE_INT32)) ||
        (rhs->isTypeKnown() && rhs->isNotType(JSVAL_TYPE_INT32))) {
        prepareStubCall(Uses(2));

        if (op == JSOP_STRICTEQ)
            stubCall(stubs::StrictEq);
        else
            stubCall(stubs::StrictNe);

        frame.popn(2);
        frame.pushSyncedType(JSVAL_TYPE_BOOLEAN);
        return;
    }

#ifndef JS_CPU_ARM
    /* Try an integer fast-path. */
    bool needStub = false;
    if (!lhs->isTypeKnown()) {
        Jump j = frame.testInt32(Assembler::NotEqual, lhs);
        stubcc.linkExit(j, Uses(2));
        needStub = true;
    }

    if (!rhs->isTypeKnown() && !frame.haveSameBacking(lhs, rhs)) {
        Jump j = frame.testInt32(Assembler::NotEqual, rhs);
        stubcc.linkExit(j, Uses(2));
        needStub = true;
    }

    FrameEntry *test  = lhs->isConstant() ? rhs : lhs;
    FrameEntry *other = lhs->isConstant() ? lhs : rhs;

    /* ReturnReg is safely usable with set32, since %ah can be accessed. */
    RegisterID resultReg = Registers::ReturnReg;
    frame.takeReg(resultReg);
    RegisterID testReg = frame.tempRegForData(test);
    frame.pinReg(testReg);

    JS_ASSERT(resultReg != testReg);

    /* Set boolean in resultReg. */
    if (other->isConstant()) {
        masm.set32(cond, testReg, Imm32(other->getValue().toInt32()), resultReg);
    } else if (frame.shouldAvoidDataRemat(other)) {
        masm.set32(cond, testReg, frame.addressOf(other), resultReg);
    } else {
        RegisterID otherReg = frame.tempRegForData(other);

        JS_ASSERT(otherReg != resultReg);
        JS_ASSERT(otherReg != testReg);

        masm.set32(cond, testReg, otherReg, resultReg);
    }

    frame.unpinReg(testReg);

    if (needStub) {
        stubcc.leave();
        if (op == JSOP_STRICTEQ)
            stubcc.call(stubs::StrictEq);
        else
            stubcc.call(stubs::StrictNe);
    }

    frame.popn(2);
    frame.pushTypedPayload(JSVAL_TYPE_BOOLEAN, resultReg);

    if (needStub)
        stubcc.rejoin(Changes(1));
#else
    /* TODO: Port set32() logic to ARM. */
    prepareStubCall(Uses(2));

    if (op == JSOP_STRICTEQ)
        stubCall(stubs::StrictEq);
    else
        stubCall(stubs::StrictNe);

    frame.popn(2);
    frame.pushSyncedType(JSVAL_TYPE_BOOLEAN);
    return;
#endif
}

void
mjit::Compiler::jsop_pos()
{
    FrameEntry *top = frame.peek(-1);

    if (top->isTypeKnown()) {
        if (top->getKnownType() <= JSVAL_TYPE_INT32)
            return;
        prepareStubCall(Uses(1));
        stubCall(stubs::Pos);
        frame.pop();
        frame.pushSynced();
        return;
    }

    frame.giveOwnRegs(top);

    Jump j;
    if (frame.shouldAvoidTypeRemat(top))
        j = masm.testNumber(Assembler::NotEqual, frame.addressOf(top));
    else
        j = masm.testNumber(Assembler::NotEqual, frame.tempRegForType(top));
    stubcc.linkExit(j, Uses(1));

    stubcc.leave();
    stubcc.call(stubs::Pos);

    stubcc.rejoin(Changes(1));
}

