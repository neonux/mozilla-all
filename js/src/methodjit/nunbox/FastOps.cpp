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
#include "methodjit/MethodJIT.h"
#include "methodjit/Compiler.h"
#include "methodjit/StubCalls.h"
#include "methodjit/FrameState-inl.h"

#include "jsautooplen.h"

using namespace js;
using namespace js::mjit;

void
mjit::Compiler::jsop_bindname(uint32 index)
{
    RegisterID reg = frame.allocReg();
    masm.loadPtr(Address(Assembler::FpReg, offsetof(JSStackFrame, scopeChain)), reg);

    Address address(reg, offsetof(JSObject, fslots) + JSSLOT_PARENT * sizeof(jsval));

    Jump j = masm.branch32(Assembler::NotEqual, masm.payloadOf(address), Imm32(0));

    stubcc.linkExit(j);
    stubcc.leave();
    stubcc.call(stubs::BindName);

    frame.pushTypedPayload(JSVAL_MASK32_NONFUNOBJ, reg);

    stubcc.rejoin(1);
}

void
mjit::Compiler::jsop_bitnot()
{
    FrameEntry *top = frame.peek(-1);

    /* We only want to handle integers here. */
    if (top->isTypeKnown() && top->getTypeTag() != JSVAL_MASK32_INT32) {
        prepareStubCall();
        stubCall(stubs::BitNot, Uses(1), Defs(1));
        frame.pop();
        frame.pushSyncedType(JSVAL_MASK32_INT32);
        return;
    }
           
    /* Test the type. */
    bool stubNeeded = false;
    if (!top->isTypeKnown()) {
        RegisterID reg = frame.tempRegForType(top);
        Jump intFail = masm.testInt32(Assembler::NotEqual, reg);
        stubcc.linkExit(intFail);
        frame.learnType(top, JSVAL_MASK32_INT32);
        stubNeeded = true;
    }

    if (stubNeeded) {
        stubcc.leave();
        stubcc.call(stubs::BitNot);
    }

    RegisterID reg = frame.ownRegForData(top);
    masm.not32(reg);
    frame.pop();
    frame.pushTypedPayload(JSVAL_MASK32_INT32, reg);

    if (stubNeeded)
        stubcc.rejoin(1);
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
      case JSOP_RSH:
        stub = stubs::Rsh;
        break;
      default:
        JS_NOT_REACHED("wat");
        return;
    }

    /* We only want to handle integers here. */
    if ((rhs->isTypeKnown() && rhs->getTypeTag() != JSVAL_MASK32_INT32) ||
        (lhs->isTypeKnown() && lhs->getTypeTag() != JSVAL_MASK32_INT32)) {
        prepareStubCall();
        stubCall(stub, Uses(2), Defs(1));
        frame.popn(2);
        frame.pushSyncedType(JSVAL_MASK32_INT32);
        return;
    }
           
    /* Test the types. */
    bool stubNeeded = false;
    if (!rhs->isTypeKnown()) {
        RegisterID reg = frame.tempRegForType(rhs);
        Jump rhsFail = masm.testInt32(Assembler::NotEqual, reg);
        stubcc.linkExit(rhsFail);
        frame.learnType(rhs, JSVAL_MASK32_INT32);
        stubNeeded = true;
    }
    if (!lhs->isTypeKnown()) {
        RegisterID reg = frame.tempRegForType(lhs);
        Jump lhsFail = masm.testInt32(Assembler::NotEqual, reg);
        stubcc.linkExit(lhsFail);
        stubNeeded = true;
    }

    if (stubNeeded) {
        stubcc.leave();
        stubcc.call(stub);
    }

    if (lhs->isConstant() && rhs->isConstant()) {
        int32 L = lhs->getValue().asInt32();
        int32 R = rhs->getValue().asInt32();

        frame.popn(2);
        switch (op) {
          case JSOP_BITOR:
            frame.push(Int32Tag(L | R));
            break;
          case JSOP_BITXOR:
            frame.push(Int32Tag(L ^ R));
            break;
          case JSOP_BITAND:
            frame.push(Int32Tag(L & R));
            return;
          case JSOP_LSH:
            frame.push(Int32Tag(L << R));
            return;
          case JSOP_RSH:
            frame.push(Int32Tag(L >> R));
            return;
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
                masm.and32(Imm32(rhs->getValue().asInt32()), reg);
            else if (op == JSOP_BITXOR)
                masm.xor32(Imm32(rhs->getValue().asInt32()), reg);
            else
                masm.or32(Imm32(rhs->getValue().asInt32()), reg);
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
      case JSOP_RSH:
      {
        /* Not commutative. */
        if (rhs->isConstant()) {
            int32 shift = rhs->getValue().asInt32() & 0x1F;

            if (!shift) {
                /*
                 * Just pop RHS - leave LHS. ARM can't shift by 0.
                 * Type of LHS should be learned already.
                 */
                masm.pop();
                if (stubNeeded)
                    stubcc.rejoin(2);
                return;
            }

            reg = frame.ownRegForData(lhs);

            switch (op) {
              case JSOP_LSH:
                masm.lshift32(Imm32(shift), reg);
                break;
              case JSOP_RSH:
                masm.rshift32(Imm32(shift), reg);
                break;
              default:
                JS_NOT_REACHED("NYI");
            }
        } else {
#if defined(JS_CPU_X86) || defined(JS_CPU_X64)
            /* Grosssssss! RHS _must_ be in ECX, on x86 */
            RegisterID rr = frame.tempRegForData(rhs, JSC::X86Registers::ecx);
#else
            RegisterID rr = frame.tempRegForData(rhs);
#endif

            frame.pinReg(rr);
            if (lhs->isConstant()) {
                reg = frame.allocReg();
                masm.move(Imm32(lhs->getValue().asInt32()), reg);
            } else {
                reg = frame.ownRegForData(lhs);
            }
            frame.unpinReg(rr);

            switch (op) {
              case JSOP_LSH:
                masm.lshift32(rr, reg);
                break;
              case JSOP_RSH:
                masm.rshift32(rr, reg);
                break;
              default:
                JS_NOT_REACHED("NYI");
            }
        }
        break;
      }

      default:
        JS_NOT_REACHED("NYI");
        return;
    }

    frame.pop();
    frame.pop();
    frame.pushTypedPayload(JSVAL_MASK32_INT32, reg);

    if (stubNeeded)
        stubcc.rejoin(2);
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

    if (post && !popped) {
        frame.push(addr);
        FrameEntry *fe = frame.peek(-1);
        Jump notInt = frame.testInt32(Assembler::NotEqual, fe);
        stubcc.linkExit(notInt);
        data = frame.copyData(fe);
    } else {
        Jump notInt = masm.testInt32(Assembler::NotEqual, addr);
        stubcc.linkExit(notInt);
        data = frame.allocReg();
        masm.loadData32(addr, data);
    }

    Jump ovf;
    if (amt > 0)
        ovf = masm.branchAdd32(Assembler::Overflow, Imm32(1), data);
    else
        ovf = masm.branchSub32(Assembler::Overflow, Imm32(1), data);
    stubcc.linkExit(ovf);

    stubcc.leave();
    stubcc.masm.lea(addr, Registers::ArgReg1);
    stubcc.vpInc(op, post && !popped);

    masm.storeData32(data, addr);

    if (!post && !popped)
        frame.pushUntypedPayload(JSVAL_MASK32_INT32, data);
    else
        frame.freeReg(data);

    frame.freeReg(reg);

    stubcc.rejoin(1);
}

void
mjit::Compiler::jsop_relational(JSOp op, BoolStub stub, jsbytecode *target, JSOp fused)
{
    FrameEntry *rhs = frame.peek(-1);
    FrameEntry *lhs = frame.peek(-2);

    /* The compiler should have handled constant folding. */
    JS_ASSERT(!(rhs->isConstant() && lhs->isConstant()));

    /* Always slow path... */
    if ((rhs->isTypeKnown() && rhs->getTypeTag() != JSVAL_MASK32_INT32) ||
        (lhs->isTypeKnown() && lhs->getTypeTag() != JSVAL_MASK32_INT32)) {
        emitStubCmpOp(stub, target, fused);
        return;
    }

    /* Test the types. */
    if (!rhs->isTypeKnown()) {
        RegisterID reg = frame.tempRegForType(rhs);
        Jump rhsFail = masm.testInt32(Assembler::NotEqual, reg);
        stubcc.linkExit(rhsFail);
        frame.learnType(rhs, JSVAL_MASK32_INT32);
    }
    if (!lhs->isTypeKnown()) {
        RegisterID reg = frame.tempRegForType(lhs);
        Jump lhsFail = masm.testInt32(Assembler::NotEqual, reg);
        stubcc.linkExit(lhsFail);
    }

    Assembler::Condition cond;
    switch (op) {
      case JSOP_LT:
        cond = Assembler::LessThan;
        break;
      case JSOP_LE:
        cond = Assembler::LessThanOrEqual;
        break;
      case JSOP_GT:
        cond = Assembler::GreaterThan;
        break;
      case JSOP_GE:
        cond = Assembler::GreaterThanOrEqual;
        break;
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

        switch (cond) {
          case Assembler::LessThan:
            cond = Assembler::GreaterThan;
            break;
          case Assembler::LessThanOrEqual:
            cond = Assembler::GreaterThanOrEqual;
            break;
          case Assembler::GreaterThan:
            cond = Assembler::LessThan;
            break;
          case Assembler::GreaterThanOrEqual:
            cond = Assembler::LessThanOrEqual;
            break;
          case Assembler::Equal: /* fall through */
          case Assembler::NotEqual:
            /* Equal and NotEqual are commutative. */
            break;
          default:
            JS_NOT_REACHED("wat");
            break;
        }
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
            rval = rhs->getValue().asInt32();

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
              case Assembler::LessThan:
                cond = Assembler::GreaterThanOrEqual;
                break;
              case Assembler::LessThanOrEqual:
                cond = Assembler::GreaterThan;
                break;
              case Assembler::GreaterThan:
                cond = Assembler::LessThanOrEqual;
                break;
              case Assembler::GreaterThanOrEqual:
                cond = Assembler::LessThan;
                break;
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

        Jump j;
        if (!rhsConst)
            j = masm.branch32(cond, lr, rr);
        else
            j = masm.branch32(cond, lr, Imm32(rval));

        jumpInScript(j, target);

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

        /* Rejoin unnecessary - state is flushed. */
        j = stubcc.masm.jump();
        stubcc.crossJump(j, masm.label());
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
            masm.set32(cond, reg, Imm32(rhs->getValue().asInt32()), resultReg);
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
        frame.pushTypedPayload(JSVAL_MASK32_BOOLEAN, resultReg);
        stubcc.rejoin(1);
    }
}

void
mjit::Compiler::jsop_neg()
{
    prepareStubCall();
    stubCall(stubs::Neg, Uses(1), Defs(1));
    frame.pop();
    frame.pushSynced();
}

void
mjit::Compiler::jsop_objtostr()
{
    prepareStubCall();
    stubCall(stubs::ObjToStr, Uses(1), Defs(1));
    frame.pop();
    frame.pushSynced();
}

void
mjit::Compiler::jsop_not()
{
    FrameEntry *top = frame.peek(-1);

    if (top->isConstant()) {
        const Value &v = top->getValue();
        frame.pop();
        frame.push(BooleanTag(!js_ValueToBoolean(v)));
        return;
    }

    if (top->isTypeKnown()) {
        uint32 mask = top->getTypeTag();
        switch (mask) {
          case JSVAL_MASK32_INT32:
          case JSVAL_MASK32_BOOLEAN:
          {
            /* :FIXME: X64 */
            /* :FIXME: Faster to xor 1, zero-extend */
            RegisterID reg = frame.ownRegForData(top);
            Jump t = masm.branchTest32(Assembler::NotEqual, reg, reg);
            masm.move(Imm32(1), reg);
            Jump d = masm.jump();
            t.linkTo(masm.label(), &masm);
            masm.move(Imm32(0), reg);
            d.linkTo(masm.label(), &masm);
            frame.pop();
            frame.pushTypedPayload(JSVAL_MASK32_BOOLEAN, reg);
            break;
          }

          case JSVAL_MASK32_NONFUNOBJ:
          case JSVAL_MASK32_FUNOBJ:
          {
            frame.pop();
            frame.push(BooleanTag(false));
            break;
          }

          default:
          {
            /* :FIXME: overkill to spill everything - can use same xor trick too */
            RegisterID reg = Registers::ReturnReg;
            prepareStubCall();
            stubCall(stubs::ValueToBoolean, Uses(0), Defs(0));
            frame.takeReg(reg);
            Jump t = masm.branchTest32(Assembler::NotEqual, reg, reg);
            masm.move(Imm32(1), reg);
            Jump d = masm.jump();
            t.linkTo(masm.label(), &masm);
            masm.move(Imm32(0), reg);
            d.linkTo(masm.label(), &masm);
            frame.pop();
            frame.pushTypedPayload(JSVAL_MASK32_BOOLEAN, reg);
            break;
          }
        }

        return;
    }

    /* Fast-path here is boolean. */
    Jump boolFail = frame.testBoolean(Assembler::NotEqual, top);
    stubcc.linkExit(boolFail);
    frame.learnType(top, JSVAL_MASK32_BOOLEAN);

    stubcc.leave();
    stubcc.call(stubs::Not);

    RegisterID reg = frame.ownRegForData(top);
    masm.xor32(Imm32(1), reg);
    frame.pop();
    frame.pushTypedPayload(JSVAL_MASK32_BOOLEAN, reg);

    stubcc.rejoin(1);
}

