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

#if !defined jsjaeger_valueinfo_h__ && defined JS_METHODJIT
#define jsjaeger_valueinfo_h__

#include "jsapi.h"
#include "jsnum.h"
#include "jstypes.h"
#include "methodjit/MachineRegs.h"
#include "methodjit/RematInfo.h"
#include "assembler/assembler/MacroAssembler.h"

namespace js {
namespace mjit {

class FrameEntry
{
    friend class FrameState;
    friend class ImmutableSync;

  public:

    /* Accessors for entries which are known constants. */

    bool isConstant() const {
        if (isCopy() || isInvariant())
            return false;
        return data.isConstant();
    }

    const jsval_layout &getConstant() const {
        JS_ASSERT(isConstant());
        return v_;
    }

    const Value &getValue() const {
        JS_ASSERT(isConstant());
        return Valueify(JSVAL_FROM_LAYOUT(v_));
    }

#if defined JS_NUNBOX32
    uint32 getPayload() const {
        //JS_ASSERT(!Valueify(v_.asBits).isDouble() || type.synced());
        JS_ASSERT(isConstant());
        return v_.s.payload.u32;
    }
#elif defined JS_PUNBOX64
    uint64 getPayload() const {
        JS_ASSERT(isConstant());
        return v_.asBits & JSVAL_PAYLOAD_MASK;
    }
#endif

    /* For a constant double FrameEntry, truncate to an int32. */
    void convertConstantDoubleToInt32(JSContext *cx) {
        JS_ASSERT(isType(JSVAL_TYPE_DOUBLE) && isConstant());
        int32 value;
        ValueToECMAInt32(cx, getValue(), &value);

        Value newValue = Int32Value(value);
        setConstant(Jsvalify(newValue));
    }

    /*
     * Accessors for entries whose type is known. Any entry can have a known
     * type, and constant entries must have one.
     */

    bool isTypeKnown() const {
        return backing()->type.isConstant();
    }

    /*
     * The known type should not be used in generated code if it is JSVAL_TYPE_DOUBLE.
     * In such cases either the value is constant, in memory or in a floating point register.
     */
    JSValueType getKnownType() const {
        JS_ASSERT(isTypeKnown());
        return backing()->knownType;
    }

#if defined JS_NUNBOX32
    JSValueTag getKnownTag() const {
        JS_ASSERT(v_.s.tag != JSVAL_TAG_CLEAR);
        return backing()->v_.s.tag;
    }
#elif defined JS_PUNBOX64
    JSValueShiftedTag getKnownTag() const {
        return JSValueShiftedTag(backing()->v_.asBits & JSVAL_TAG_MASK);
    }
#endif

    // Return true iff the type of this value is definitely known to be type_.
    bool isType(JSValueType type_) const {
        return isTypeKnown() && getKnownType() == type_;
    }

    // Return true iff the type of this value is definitely known not to be type_.
    bool isNotType(JSValueType type_) const {
        return isTypeKnown() && getKnownType() != type_;
    }

    // Return true if the type of this value is definitely type_, or is unknown
    // and thus potentially type_ at runtime.
    bool mightBeType(JSValueType type_) const {
        return !isNotType(type_);
    }

    /* Accessors for entries which are copies of other mutable entries. */

    bool isCopy() const { return !!copy; }
    bool isCopied() const { return copied; }

    const FrameEntry *backing() const {
        return isCopy() ? copyOf() : this;
    }

    bool hasSameBacking(const FrameEntry *other) const {
        return backing() == other->backing();
    }

    /*
     * Accessors for entries which are copies of analysis temporaries. All
     * temporaries are invariant, so these behave more like constants than like
     * copies of mutable entries.
     */

    bool isInvariant() const { return !!invariant_; }

    FrameEntry *invariant() {
        JS_ASSERT(isInvariant());
        return invariant_;
    }

  private:
    void setType(JSValueType type_) {
        JS_ASSERT(!isCopy() && !isInvariant());
        type.setConstant();
#if defined JS_NUNBOX32
        v_.s.tag = JSVAL_TYPE_TO_TAG(type_);
#elif defined JS_PUNBOX64
        v_.asBits &= JSVAL_PAYLOAD_MASK;
        v_.asBits |= JSVAL_TYPE_TO_SHIFTED_TAG(type_);
#endif
        knownType = type_;
    }

    void track(uint32 index) {
        clear();
        index_ = index;
        tracked = true;
    }

    void clear() {
        copied = false;
        copy = NULL;
        invariant_ = NULL;
    }

    uint32 trackerIndex() {
        return index_;
    }

    /*
     * Marks the FE as unsynced & invalid.
     */
    void resetUnsynced() {
        clear();
        type.unsync();
        data.unsync();
        type.invalidate();
        data.invalidate();
    }

    /*
     * Marks the FE as synced & in memory.
     */
    void resetSynced() {
        clear();
        type.setMemory();
        data.setMemory();
    }

    /*
     * Marks the FE as having a constant.
     */
    void setConstant(const jsval &v) {
        clear();
        type.unsync();
        data.unsync();
        type.setConstant();
        data.setConstant();
        v_.asBits = JSVAL_BITS(v);
        Value cv = Valueify(v);
        if (cv.isDouble())
            knownType = JSVAL_TYPE_DOUBLE;
        else
            knownType = cv.extractNonDoubleType();
    }

    void setCopied() {
        JS_ASSERT(!isCopy());
        copied = true;
    }

    FrameEntry *copyOf() const {
        JS_ASSERT(isCopy());
        JS_ASSERT(copy < this);
        return copy;
    }

    void setNotCopied() {
        copied = false;
    }

    /*
     * Set copy index.
     */
    void setCopyOf(FrameEntry *fe) {
        JS_ASSERT(!isCopied());
        copy = fe;
        invariant_ = NULL;
        if (fe) {
            type.invalidate();
            data.invalidate();
        }
    }

    void setInvariant(FrameEntry *fe) {
        JS_ASSERT(!isCopied());
        copy = NULL;
        invariant_ = fe;
        type.invalidate();
        data.invalidate();
    }

    inline bool isTracked() const {
        return tracked;
    }

    inline void untrack() {
        tracked = false;
    }

    inline bool dataInRegister(AnyRegisterID reg) const {
        JS_ASSERT(!copy && !invariant_);
        return reg.isReg()
            ? (data.inRegister() && data.reg() == reg.reg())
            : (data.inFPRegister() && data.fpreg() == reg.fpreg());
    }

  private:
    JSValueType knownType;
    jsval_layout v_;
    RematInfo  type;
    RematInfo  data;
    uint32     index_;
    FrameEntry *invariant_;
    FrameEntry *copy;
    bool       copied;
    bool       tracked;
    bool       inlined;

    /*
     * Offset of the last loop in which this entry was written or had a loop
     * register assigned.
     */
    uint32     lastLoop;
};

} /* namespace mjit */
} /* namespace js */

#endif /* jsjaeger_valueinfo_h__ */

