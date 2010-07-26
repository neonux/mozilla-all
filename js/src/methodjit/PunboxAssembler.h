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
 *   Sean Stangl <sstangl@mozilla.com>
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

#if !defined jsjaeger_assembler64_h__ && defined JS_METHODJIT && defined JS_64BIT
#define jsjaeger_assembler64_h__

#include "methodjit/BaseAssembler.h"
#include "methodjit/MachineRegs.h"

namespace js {
namespace mjit {

class Imm64 : public JSC::MacroAssembler::ImmPtr
{
  public:
    Imm64(uint64 u)
      : ImmPtr((const void *)u)
    { }
};

class ImmShiftedTag : public JSC::MacroAssembler::ImmPtr
{
  public:
    ImmShiftedTag(JSValueShiftedTag shtag)
      : ImmPtr((const void *)shtag)
    { }
};

class ImmType : public ImmShiftedTag
{
  public:
    ImmType(JSValueType type)
      : ImmShiftedTag(JSValueShiftedTag(JSVAL_TYPE_TO_SHIFTED_TAG(type)))
    { }
};

class Assembler : public BaseAssembler
{
    static const uint32 PAYLOAD_OFFSET = 0;

  public:
    static const JSC::MacroAssembler::Scale JSVAL_SCALE = JSC::MacroAssembler::TimesEight;

    Address payloadOf(Address address) {
        return address;
    }

    BaseIndex payloadOf(BaseIndex address) {
        return address;
    }

    Address valueOf(Address address) {
        return address;
    }

    BaseIndex valueOf(BaseIndex address) {
        return address;
    }

    void loadValue(Address address, RegisterID dst) {
        loadPtr(address, dst);
    }

    void loadValue(BaseIndex address, RegisterID dst) {
        loadPtr(address, dst);
    }

    void convertValueToType(RegisterID val) {
        andPtr(Imm64(JSVAL_TAG_MASK), val);
    }

    void convertValueToPayload(RegisterID val) {
        andPtr(Imm64(JSVAL_PAYLOAD_MASK), val);
    }

    void loadValueThenType(Address address, RegisterID val, RegisterID type) {
        loadValue(valueOf(address), val);
        if (val != type)
            move(val, type);
        convertValueToType(type);
    }
    
    void loadValueThenType(BaseIndex address, RegisterID val, RegisterID type) {
        loadValue(valueOf(address), val);
        if (val != type)
            move(val, type);
        convertValueToType(type);
    }

    void loadValueThenPayload(Address address, RegisterID val, RegisterID payload) {
        loadValue(valueOf(address), val);
        if (val != payload)
            move(val, payload);
        convertValueToPayload(payload);
    }

    void loadValueThenPayload(BaseIndex address, RegisterID val, RegisterID payload) {
        loadValue(valueOf(address), val);
        if (val != payload)
            move(val, payload);
        convertValueToPayload(payload);
    }

    void loadTypeTag(Address address, RegisterID reg) {
        loadValueThenType(valueOf(address), reg, reg);
    }

    void loadTypeTag(BaseIndex address, RegisterID reg) {
        loadValueThenType(valueOf(address), reg, reg);
    }

    void storeTypeTag(ImmShiftedTag imm, Address address) {
        loadValue(valueOf(address), Registers::ValueReg);
        convertValueToPayload(Registers::ValueReg);
        orPtr(imm, Registers::ValueReg);
        storePtr(Registers::ValueReg, valueOf(address));
    }

    void storeTypeTag(ImmShiftedTag imm, BaseIndex address) {
        loadValue(valueOf(address), Registers::ValueReg);
        convertValueToPayload(Registers::ValueReg);
        orPtr(imm, Registers::ValueReg);
        storePtr(Registers::ValueReg, valueOf(address));
    }

    void storeTypeTag(RegisterID reg, Address address) {
        /* The type tag must be stored in shifted format. */
        loadValue(valueOf(address), Registers::ValueReg);
        convertValueToPayload(Registers::ValueReg);
        orPtr(reg, Registers::ValueReg);
        storePtr(Registers::ValueReg, valueOf(address));

    }

    void storeTypeTag(RegisterID reg, BaseIndex address) {
        /* The type tag must be stored in shifted format. */
        loadValue(valueOf(address), Registers::ValueReg);
        convertValueToPayload(Registers::ValueReg);
        orPtr(reg, Registers::ValueReg);
        storePtr(Registers::ValueReg, valueOf(address));
    }

    void loadPayload(Address address, RegisterID reg) {
        loadValueThenPayload(address, reg, reg);
    }

    void loadPayload(BaseIndex address, RegisterID reg) {
        loadValueThenPayload(address, reg, reg);
    }

    void storePayload(RegisterID reg, Address address) {
        /* Not for doubles. */
        loadValue(valueOf(address), Registers::ValueReg);
        convertValueToType(Registers::ValueReg);
        orPtr(reg, Registers::ValueReg);
        storePtr(Registers::ValueReg, valueOf(address));
    }

    void storePayload(RegisterID reg, BaseIndex address) {
        /* Not for doubles. */
        loadValue(valueOf(address), Registers::ValueReg);
        convertValueToType(Registers::ValueReg);
        orPtr(reg, Registers::ValueReg);
        storePtr(Registers::ValueReg, valueOf(address));
    }
    
    void storePayload(Imm64 imm, Address address) {
        /* Not for doubles. */
        storePtr(imm, valueOf(address));
    }

    void storeValue(const Value &v, Address address) {
        jsval_layout jv;
        jv.asBits = JSVAL_BITS(Jsvalify(v));

        storePtr(Imm64(jv.asBits), valueOf(address));
    }

    void storeValue(const Value &v, BaseIndex address) {
        jsval_layout jv;
        jv.asBits = JSVAL_BITS(Jsvalify(v));

        storePtr(Imm64(jv.asBits), valueOf(address));        
    }

    void loadFunctionPrivate(RegisterID base, RegisterID to) {
        Address privSlot(base, offsetof(JSObject, fslots) +
                               JSSLOT_PRIVATE * sizeof(Value));
        loadPtr(privSlot, to);
        lshiftPtr(Imm32(1), to);
    }

    Jump testNull(Assembler::Condition cond, RegisterID reg) {
        return branchPtr(cond, reg, ImmShiftedTag(JSVAL_SHIFTED_TAG_NULL));
    }

    Jump testNull(Assembler::Condition cond, Address address) {
        loadValueThenType(address, Registers::ValueReg, Registers::ValueReg);
        return branchPtr(cond, Registers::ValueReg, ImmShiftedTag(JSVAL_SHIFTED_TAG_NULL));
    }

    Jump testInt32(Assembler::Condition cond, RegisterID reg) {
        return branchPtr(cond, reg, ImmShiftedTag(JSVAL_SHIFTED_TAG_INT32));
    }

    Jump testInt32(Assembler::Condition cond, Address address) {
        loadValueThenType(address, Registers::ValueReg, Registers::ValueReg);
        return branchPtr(cond, Registers::ValueReg, ImmShiftedTag(JSVAL_SHIFTED_TAG_INT32));
    }

    Jump testNumber(Assembler::Condition cond, RegisterID reg) {
        cond = (cond == Assembler::Equal) ? Assembler::BelowOrEqual : Assembler::Above;
        return branchPtr(cond, reg, ImmShiftedTag(JSVAL_SHIFTED_TAG_INT32));
    }

    Jump testNumber(Assembler::Condition cond, Address address) {
        cond = (cond == Assembler::Equal) ? Assembler::BelowOrEqual : Assembler::Above;
        loadValueThenType(address, Registers::ValueReg, Registers::ValueReg);
        return branchPtr(cond, Registers::ValueReg, ImmShiftedTag(JSVAL_SHIFTED_TAG_INT32));
    }

    Jump testPrimitive(Assembler::Condition cond, RegisterID reg) {
        cond = (cond == Assembler::NotEqual) ? Assembler::AboveOrEqual : Assembler::Below;
        return branchPtr(cond, reg, ImmShiftedTag(JSVAL_SHIFTED_TAG_OBJECT));
    }

    Jump testPrimitive(Assembler::Condition cond, Address address) {
        cond = (cond == Assembler::NotEqual) ? Assembler::AboveOrEqual : Assembler::Below;
        loadValueThenType(address, Registers::ValueReg, Registers::ValueReg);
        return branchPtr(cond, Registers::ValueReg, ImmShiftedTag(JSVAL_SHIFTED_TAG_OBJECT));
    }

    Jump testObject(Assembler::Condition cond, RegisterID reg) {
        return branchPtr(cond, reg, ImmShiftedTag(JSVAL_SHIFTED_TAG_OBJECT));
    }

    Jump testObject(Assembler::Condition cond, Address address) {
        loadValueThenType(address, Registers::ValueReg, Registers::ValueReg);
        return branchPtr(cond, Registers::ValueReg, ImmShiftedTag(JSVAL_SHIFTED_TAG_OBJECT));
    }

    Jump testDouble(Assembler::Condition cond, RegisterID reg) {
        Assembler::Condition opcond;
        if (cond == Assembler::Equal)
            opcond = Assembler::Below;
        else
            opcond = Assembler::AboveOrEqual;
        return branchPtr(opcond, reg, ImmShiftedTag(JSVAL_SHIFTED_TAG_MAX_DOUBLE));
    }

    Jump testDouble(Assembler::Condition cond, Address address) {
        Assembler::Condition opcond;
        if (cond == Assembler::Equal)
            opcond = Assembler::Below;
        else
            opcond = Assembler::AboveOrEqual;
        loadValueThenType(address, Registers::ValueReg, Registers::ValueReg);
        return branchPtr(opcond, Registers::ValueReg, ImmShiftedTag(JSVAL_SHIFTED_TAG_MAX_DOUBLE));
    }

    Jump testBoolean(Assembler::Condition cond, RegisterID reg) {
        return branchPtr(cond, reg, ImmShiftedTag(JSVAL_SHIFTED_TAG_BOOLEAN));
    }

    Jump testBoolean(Assembler::Condition cond, Address address) {
        loadValueThenType(address, Registers::ValueReg, Registers::ValueReg);
        return branchPtr(cond, Registers::ValueReg, ImmShiftedTag(JSVAL_SHIFTED_TAG_BOOLEAN));
    }

    Jump testString(Assembler::Condition cond, RegisterID reg) {
        return branchPtr(cond, reg, ImmShiftedTag(JSVAL_SHIFTED_TAG_STRING));
    }

    Jump testString(Assembler::Condition cond, Address address) {
        loadValueThenType(address, Registers::ValueReg, Registers::ValueReg);
        return branchPtr(cond, Registers::ValueReg, ImmShiftedTag(JSVAL_SHIFTED_TAG_BOOLEAN));
    }
};

} /* namespace js */
} /* namespace mjit */

#endif

