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
#include "PolyIC.h"
#include "StubCalls.h"
#include "CodeGenIncludes.h"
#include "StubCalls-inl.h"
#include "assembler/assembler/LinkBuffer.h"
#include "jsscope.h"
#include "jsnum.h"
#include "jsscopeinlines.h"
#include "jspropertycache.h"
#include "jspropertycacheinlines.h"

using namespace js;
using namespace js::mjit;

#if ENABLE_PIC

/* Rough over-estimate of how much memory we need to unprotect. */
static const uint32 INLINE_PATH_LENGTH = 64;

/* Maximum number of stubs for a given callsite. */
static const uint32 MAX_STUBS = 16;

typedef JSC::FunctionPtr FunctionPtr;
typedef JSC::RepatchBuffer RepatchBuffer;
typedef JSC::CodeBlock CodeBlock;
typedef JSC::CodeLocationLabel CodeLocationLabel;
typedef JSC::JITCode JITCode;
typedef JSC::MacroAssembler::Jump Jump;
typedef JSC::MacroAssembler::RegisterID RegisterID;
typedef JSC::MacroAssembler::Label Label;
typedef JSC::MacroAssembler::Imm32 Imm32;
typedef JSC::MacroAssembler::ImmPtr ImmPtr;
typedef JSC::MacroAssembler::Address Address;
typedef JSC::ReturnAddressPtr ReturnAddressPtr;
typedef JSC::MacroAssemblerCodePtr MacroAssemblerCodePtr;

struct AutoPropertyDropper
{
    JSContext *cx;
    JSObject *holder;
    JSProperty *prop;

  public:
    AutoPropertyDropper(JSContext *cx, JSObject *obj, JSProperty *prop)
      : cx(cx), holder(obj), prop(prop)
    {
        JS_ASSERT(prop);
    }

    ~AutoPropertyDropper()
    {
        holder->dropProperty(cx, prop);
    }
};

class PICStubCompiler
{
  protected:
    const char *type;
    VMFrame &f;
    JSScript *script;
    ic::PICInfo &pic;

  public:
    PICStubCompiler(const char *type, VMFrame &f, JSScript *script, ic::PICInfo &pic)
      : type(type), f(f), script(script), pic(pic)
    { }

    bool disable(const char *reason, VoidStub stub)
    {
        return disable(reason, JS_FUNC_TO_DATA_PTR(void *, stub));
    }

    bool disable(const char *reason, VoidStubUInt32 stub)
    {
        return disable(reason, JS_FUNC_TO_DATA_PTR(void *, stub));
    }

    bool disable(const char *reason, void *stub)
    {
        spew("disabled", reason);
        JITCode jitCode(pic.slowPathStart.executableAddress(), INLINE_PATH_LENGTH);
        CodeBlock codeBlock(jitCode);
        RepatchBuffer repatcher(&codeBlock);
        ReturnAddressPtr retPtr(pic.slowPathStart.callAtOffset(pic.callReturn).executableAddress());
        MacroAssemblerCodePtr target(stub);
        repatcher.relinkCallerToTrampoline(retPtr, target);
        return true;
    }

    JSC::ExecutablePool *getExecPool(size_t size)
    {
        mjit::ThreadData *jd = &JS_METHODJIT_DATA(f.cx);
        return jd->execPool->poolForSize(size);
    }

  protected:
    void spew(const char *event, const char *op)
    {
        JaegerSpew(JSpew_PICs, "%s %s: %s (%s: %d)\n",
                   type, event, op, script->filename,
                   js_FramePCToLineNumber(f.cx, f.fp));
    }
};

class PICRepatchBuffer : public JSC::RepatchBuffer
{
    ic::PICInfo &pic;
    JSC::CodeLocationLabel label;

  public:
    PICRepatchBuffer(ic::PICInfo &ic, JSC::CodeLocationLabel path)
      : JSC::RepatchBuffer(path.executableAddress(), INLINE_PATH_LENGTH),
        pic(ic), label(path)
    { }

    void relink(int32 offset, JSC::CodeLocationLabel target) {
        JSC::RepatchBuffer::relink(label.jumpAtOffset(offset), target);
    }
};

class SetPropCompiler : public PICStubCompiler
{
    JSObject *obj;
    JSAtom *atom;
    VoidStubUInt32 stub;

#ifdef JS_CPU_X86
    static const int32 INLINE_SHAPE_OFFSET = 6;
    static const int32 INLINE_SHAPE_JUMP   = 12;
    static const int32 DSLOTS_BEFORE_CONSTANT = -23;
    static const int32 DSLOTS_BEFORE_KTYPE    = -19;
    static const int32 DSLOTS_BEFORE_DYNAMIC  = -15;
    static const int32 INLINE_STORE_DYN_TYPE  = -6;
    static const int32 INLINE_STORE_DYN_DATA  = 0;
    static const int32 INLINE_STORE_KTYPE_TYPE  = -10;
    static const int32 INLINE_STORE_KTYPE_DATA  = 0;
    static const int32 INLINE_STORE_CONST_TYPE  = -14;
    static const int32 INLINE_STORE_CONST_DATA  = -4;
    static const int32 STUB_SHAPE_JUMP = 12;
#endif

    static int32 dslotsLoadOffset(ic::PICInfo &pic) {
        if (pic.u.vr.isConstant)
            return DSLOTS_BEFORE_CONSTANT;
        if (pic.u.vr.u.s.isTypeKnown)
            return DSLOTS_BEFORE_KTYPE;
        return DSLOTS_BEFORE_DYNAMIC;
    }

    inline int32 dslotsLoadOffset() {
        return dslotsLoadOffset(pic);
    }

    inline int32 inlineTypeOffset() {
        if (pic.u.vr.isConstant)
            return INLINE_STORE_CONST_TYPE;
        if (pic.u.vr.u.s.isTypeKnown)
            return INLINE_STORE_KTYPE_TYPE;
        return INLINE_STORE_DYN_TYPE;
    }

    inline int32 inlineDataOffset() {
        if (pic.u.vr.isConstant)
            return INLINE_STORE_CONST_DATA;
        if (pic.u.vr.u.s.isTypeKnown)
            return INLINE_STORE_KTYPE_DATA;
        return INLINE_STORE_DYN_DATA;
    }

  public:
    SetPropCompiler(VMFrame &f, JSScript *script, JSObject *obj, ic::PICInfo &pic, JSAtom *atom,
                    VoidStubUInt32 stub)
      : PICStubCompiler("setprop", f, script, pic), obj(obj), atom(atom), stub(stub)
    { }

    bool disable(const char *reason)
    {
        return PICStubCompiler::disable(reason, stub);
    }

    static void reset(ic::PICInfo &pic)
    {
        RepatchBuffer repatcher(pic.fastPathStart.executableAddress(), INLINE_PATH_LENGTH);
        repatcher.repatchLEAToLoadPtr(pic.storeBack.instructionAtOffset(dslotsLoadOffset(pic)));
        repatcher.repatch(pic.fastPathStart.dataLabel32AtOffset(pic.shapeGuard + INLINE_SHAPE_OFFSET),
                          int32(JSScope::INVALID_SHAPE));
        repatcher.relink(pic.fastPathStart.jumpAtOffset(pic.shapeGuard + INLINE_SHAPE_JUMP),
                         pic.slowPathStart);

        RepatchBuffer repatcher2(pic.slowPathStart.executableAddress(), INLINE_PATH_LENGTH);
        ReturnAddressPtr retPtr(pic.slowPathStart.callAtOffset(pic.callReturn).executableAddress());
        MacroAssemblerCodePtr target(JS_FUNC_TO_DATA_PTR(void *, ic::SetProp));
        repatcher.relinkCallerToTrampoline(retPtr, target);
    }

    bool patchInline(JSScopeProperty *sprop)
    {
        JS_ASSERT(!pic.inlinePathPatched);
        JaegerSpew(JSpew_PICs, "patch setprop inline at %p\n", pic.fastPathStart.executableAddress());

        PICRepatchBuffer repatcher(pic, pic.fastPathStart);

        int32 offset;
        if (sprop->slot < JS_INITIAL_NSLOTS) {
            JSC::CodeLocationInstruction istr;
            istr = pic.storeBack.instructionAtOffset(dslotsLoadOffset());
            repatcher.repatchLoadPtrToLEA(istr);

            // 
            // We've patched | mov dslots, [obj + DSLOTS_OFFSET]
            // To:           | lea fslots, [obj + DSLOTS_OFFSET]
            //
            // Because the offset is wrong, it's necessary to correct it
            // below.
            //
            int32 diff = int32(offsetof(JSObject, fslots)) -
                         int32(offsetof(JSObject, dslots));
            JS_ASSERT(diff != 0);
            offset  = (int32(sprop->slot) * sizeof(Value)) + diff;
        } else {
            offset = (sprop->slot - JS_INITIAL_NSLOTS) * sizeof(Value);
        }

        uint32 shapeOffs = pic.shapeGuard + INLINE_SHAPE_OFFSET;
        repatcher.repatch(pic.fastPathStart.dataLabel32AtOffset(shapeOffs),
                          obj->shape());
        repatcher.repatch(pic.storeBack.dataLabel32AtOffset(inlineTypeOffset()),
                          offset + 4);
        if (!pic.u.vr.isConstant || !Valueify(pic.u.vr.u.v).isUndefined()) {
            repatcher.repatch(pic.storeBack.dataLabel32AtOffset(inlineDataOffset()),
                              offset);
        }

        pic.inlinePathPatched = true;

        return true;
    }

    void patchPreviousToHere(PICRepatchBuffer &repatcher, CodeLocationLabel cs)
    {
        // Patch either the inline fast path or a generated stub. The stub
        // omits the prefix of the inline fast path that loads the shape, so
        // the offsets are different.
        int shapeGuardJumpOffset;
        if (pic.stubsGenerated)
            shapeGuardJumpOffset = STUB_SHAPE_JUMP;
        else
            shapeGuardJumpOffset = pic.shapeGuard + INLINE_SHAPE_JUMP;
        repatcher.relink(shapeGuardJumpOffset, cs);
    }

    bool generateStub(JSScopeProperty *sprop)
    {
        Assembler masm;
        Label start = masm.label();

        // Shape guard.
        Jump shapeMismatch = masm.branch32_force32(Assembler::NotEqual, pic.shapeReg,
                                                   Imm32(obj->shape()));

        // Write out the store.
        Address address(pic.objReg, offsetof(JSObject, fslots) + sprop->slot * sizeof(Value));
        if (sprop->slot >= JS_INITIAL_NSLOTS) {
            masm.loadPtr(Address(pic.objReg, offsetof(JSObject, dslots)), pic.objReg);
            address = Address(pic.objReg, (sprop->slot - JS_INITIAL_NSLOTS) * sizeof(Value));
        }

        // If the scope is branded, or has a method barrier. It's now necessary
        // to guard that we're not overwriting a function-valued property.
        Jump rebrand;
        JSScope *scope = obj->scope();
        if (scope->brandedOrHasMethodBarrier()) {
            masm.loadTypeTag(address, pic.shapeReg);
            rebrand = masm.branch32(Assembler::Equal, pic.shapeReg, ImmTag(JSVAL_TAG_FUNOBJ));
        }

        if (pic.u.vr.isConstant) {
            masm.storeValue(Valueify(pic.u.vr.u.v), address);
        } else {
            if (pic.u.vr.u.s.isTypeKnown)
                masm.storeTypeTag(ImmTag(pic.u.vr.u.s.type.tag), address);
            else
                masm.storeTypeTag(pic.u.vr.u.s.type.reg, address);
            masm.storeData32(pic.u.vr.u.s.data, address);
        }
        Jump done = masm.jump();

        JSC::ExecutablePool *ep = getExecPool(masm.size());
        if (!ep || !pic.execPools.append(ep)) {
            if (ep)
                ep->release();
            js_ReportOutOfMemory(f.cx);
            return false;
        }

        JSC::LinkBuffer buffer(&masm, ep);
        buffer.link(shapeMismatch, pic.slowPathStart);
        buffer.link(done, pic.storeBack);
        if (scope->branded() || scope->hasMethodBarrier())
            buffer.link(rebrand, pic.slowPathStart);
        CodeLocationLabel cs = buffer.finalizeCodeAddendum();
        JaegerSpew(JSpew_PICs, "generate setprop stub %p %d %d at %p\n",
                   (void*)&pic,
                   obj->shape(),
                   pic.stubsGenerated,
                   cs.executableAddress());

        PICRepatchBuffer repatcher(pic, pic.lastPathStart());

        // This function can patch either the inline fast path for a generated
        // stub. The stub omits the prefix of the inline fast path that loads
        // the shape, so the offsets are different.
        patchPreviousToHere(repatcher, cs);

        pic.stubsGenerated++;
        pic.lastStubStart = buffer.locationOf(start);

        if (pic.stubsGenerated == MAX_STUBS)
            disable("max stubs reached");

        return true;
    }

    bool update()
    {
        if (!pic.hit) {
            spew("first hit", "nop");
            pic.hit = true;
            return true;
        }

        JSObject *aobj = js_GetProtoIfDenseArray(obj);
        if (!aobj->isNative())
            return disable("non-native");

        JSObject *holder;
        JSProperty *prop = NULL;
        if (!aobj->lookupProperty(f.cx, ATOM_TO_JSID(atom), &holder, &prop))
            return false;
        if (!prop)
            return disable("property not found");

        AutoPropertyDropper dropper(f.cx, holder, prop);
        if (holder != obj)
            return disable("property not on object");

        JSScope *scope = obj->scope();
        JSScopeProperty *sprop = (JSScopeProperty *)prop;
        if (!sprop->writable())
            return disable("readonly");
        if (scope->sealed() && !sprop->hasSlot())
            return disable("what does this even mean");

        if (!sprop->hasDefaultSetter())
            return disable("setter");
        if (!SPROP_HAS_VALID_SLOT(sprop, scope))
            return disable("invalid slot");

        JS_ASSERT(obj == holder);
        if (!pic.inlinePathPatched &&
            !scope->brandedOrHasMethodBarrier() &&
            !obj->isDenseArray()) {
            return patchInline(sprop);
        } 

        return generateStub(sprop);
    }
};

class GetPropCompiler : public PICStubCompiler
{
    JSObject *obj;
    JSAtom *atom;
    void   *stub;
    int lastStubSecondShapeGuard;

    /* Offsets for patching, computed manually as reverse from the storeBack. */
#ifdef JS_CPU_X86
    static const int32 DSLOTS_LOAD  = -15;
    static const int32 TYPE_LOAD    = -6;
    static const int32 DATA_LOAD    = 0;
    static const int32 INLINE_TYPE_GUARD   = 12;
    static const int32 INLINE_SHAPE_OFFSET = 6;
    static const int32 INLINE_SHAPE_JUMP   = 12;
    static const int32 STUB_SHAPE_JUMP = 12;
#endif

  public:
    GetPropCompiler(VMFrame &f, JSScript *script, JSObject *obj, ic::PICInfo &pic, JSAtom *atom,
                    VoidStub stub)
      : PICStubCompiler("getprop", f, script, pic), obj(obj), atom(atom),
        stub(JS_FUNC_TO_DATA_PTR(void *, stub)),
        lastStubSecondShapeGuard(pic.u.get.secondShapeGuard)
    { }

    GetPropCompiler(VMFrame &f, JSScript *script, JSObject *obj, ic::PICInfo &pic, JSAtom *atom,
                    VoidStubUInt32 stub)
      : PICStubCompiler("callprop", f, script, pic), obj(obj), atom(atom),
        stub(JS_FUNC_TO_DATA_PTR(void *, stub)),
        lastStubSecondShapeGuard(pic.u.get.secondShapeGuard)
    { }

    static void reset(ic::PICInfo &pic)
    {
        RepatchBuffer repatcher(pic.fastPathStart.executableAddress(), INLINE_PATH_LENGTH);
        repatcher.repatchLEAToLoadPtr(pic.storeBack.instructionAtOffset(DSLOTS_LOAD));
        repatcher.repatch(pic.fastPathStart.dataLabel32AtOffset(pic.shapeGuard + INLINE_SHAPE_OFFSET),
                          int32(JSScope::INVALID_SHAPE));
        repatcher.relink(pic.fastPathStart.jumpAtOffset(pic.shapeGuard + INLINE_SHAPE_JUMP),
                         pic.slowPathStart);
        // :FIXME: :TODO: :XXX: :URGENT: re-patch type guard

        RepatchBuffer repatcher2(pic.slowPathStart.executableAddress(), INLINE_PATH_LENGTH);
        ReturnAddressPtr retPtr(pic.slowPathStart.callAtOffset(pic.callReturn).executableAddress());
        MacroAssemblerCodePtr target(JS_FUNC_TO_DATA_PTR(void *, ic::GetProp));
        repatcher.relinkCallerToTrampoline(retPtr, target);
    }

    bool generateArrayLengthStub()
    {
        Assembler masm;

        masm.loadPtr(Address(pic.objReg, offsetof(JSObject, clasp)), pic.shapeReg);
        Jump isDense = masm.branchPtr(Assembler::Equal, pic.shapeReg, ImmPtr(&js_ArrayClass));
        Jump notArray = masm.branchPtr(Assembler::NotEqual, pic.shapeReg,
                                       ImmPtr(&js_SlowArrayClass));

        isDense.linkTo(masm.label(), &masm);
        masm.loadData32(Address(pic.objReg, offsetof(JSObject, fslots) +
                                            JSObject::JSSLOT_ARRAY_LENGTH * sizeof(Value)),
                        pic.objReg);
        Jump oob = masm.branch32(Assembler::Above, pic.objReg, Imm32(JSVAL_INT_MAX));
        masm.move(ImmTag(JSVAL_TAG_INT32), pic.shapeReg);
        Jump done = masm.jump();

        JSC::ExecutablePool *ep = getExecPool(masm.size());
        if (!ep || !pic.execPools.append(ep)) {
            if (ep)
                ep->release();
            js_ReportOutOfMemory(f.cx);
            return false;
        }

        JSC::LinkBuffer buffer(&masm, ep);
        buffer.link(notArray, pic.slowPathStart);
        buffer.link(oob, pic.slowPathStart);
        buffer.link(done, pic.storeBack);

        CodeLocationLabel start = buffer.finalizeCodeAddendum();
        JaegerSpew(JSpew_PICs, "generate array length stub at %p\n",
                   start.executableAddress());

        PICRepatchBuffer repatcher(pic, pic.lastPathStart());
        patchPreviousToHere(repatcher, start);

        disable("array length done");

        return true;
    }

    bool generateStringCallStub()
    {
        JS_ASSERT(pic.hasTypeCheck());
        JS_ASSERT(pic.kind == ic::PICInfo::CALL);

        if (!f.fp->script->compileAndGo)
            return disable("String.prototype without compile-and-go");

        mjit::ThreadData &jm = JS_METHODJIT_DATA(f.cx);
        if (!jm.addScript(script)) {
            js_ReportOutOfMemory(f.cx);
            return false;
        }

        JSObject *holder;
        JSProperty *prop;
        if (!obj->lookupProperty(f.cx, ATOM_TO_JSID(atom), &holder, &prop))
            return false;
        if (!prop)
            return disable("property not found");

        AutoPropertyDropper dropper(f.cx, holder, prop);
        JSScopeProperty *sprop = (JSScopeProperty *)prop;
        if (holder != obj)
            return disable("proto walk on String.prototype");
        if (!sprop->hasDefaultGetterOrIsMethod())
            return disable("getter");
        if (!SPROP_HAS_VALID_SLOT(sprop, holder->scope()))
            return disable("invalid slot");

        JS_ASSERT(holder->isNative());

        Assembler masm;

        /* Only strings are allowed. */
        Jump notString = masm.branch32(Assembler::NotEqual, pic.typeReg(),
                                       ImmTag(JSVAL_TAG_STRING));

        /*
         * Sink pic.objReg, since we're about to lose it. This is optimistic,
         * we could reload it from objRemat if we wanted.
         *
         * Note: This is really hacky, and relies on f.regs.sp being set
         * correctly in ic::CallProp. Should we just move the store higher
         * up in the fast path, or put this offset in PICInfo?
         */
        uint32 thisvOffset = uint32(f.regs.sp - f.fp->slots()) - 1;
        Address thisv(JSFrameReg, sizeof(JSStackFrame) + thisvOffset * sizeof(Value));
        masm.storeTypeTag(ImmTag(JSVAL_TAG_STRING), thisv);
        masm.storeData32(pic.objReg, thisv);

        /*
         * Clobber objReg with String.prototype and do some PIC stuff. Well,
         * really this is now a MIC, except it won't ever be patched, so we
         * just disable the PIC at the end. :FIXME:? String.prototype probably
         * does not get random shape changes.
         */
        masm.move(ImmPtr(obj), pic.objReg);
        masm.loadShape(pic.objReg, pic.shapeReg);
        Jump shapeMismatch = masm.branch32(Assembler::NotEqual, pic.shapeReg,
                                           Imm32(obj->shape()));
        masm.loadSlot(pic.objReg, pic.objReg, sprop->slot, pic.shapeReg, pic.objReg);

        Jump done = masm.jump();

        JSC::ExecutablePool *ep = getExecPool(masm.size());
        if (!ep || !pic.execPools.append(ep)) {
            if (ep)
                ep->release();
            js_ReportOutOfMemory(f.cx);
            return false;
        }

        JSC::LinkBuffer patchBuffer(&masm, ep);

        int32 typeCheckOffset = -int32(pic.u.get.typeCheckOffset);
        patchBuffer.link(notString, pic.slowPathStart.labelAtOffset(typeCheckOffset));
        patchBuffer.link(shapeMismatch, pic.slowPathStart);
        patchBuffer.link(done, pic.storeBack);

        CodeLocationLabel cs = patchBuffer.finalizeCodeAddendum();
        JaegerSpew(JSpew_PICs, "generate string call stub at %p\n",
                   cs.executableAddress());

        /* Patch the type check to jump here. */
        RepatchBuffer repatcher(pic.fastPathStart.executableAddress(), INLINE_PATH_LENGTH);
        repatcher.relink(pic.fastPathStart.jumpAtOffset(INLINE_TYPE_GUARD), cs);

        /* Disable the PIC so we don't keep generating stubs on the above shape mismatch. */
        disable("generated string call stub");

        return true;
    }

    bool generateStringLengthStub()
    {
        JS_ASSERT(pic.hasTypeCheck());

        Assembler masm;
        Jump notString = masm.branch32(Assembler::NotEqual, pic.typeReg(),
                                       ImmTag(JSVAL_TAG_STRING));
        masm.loadPtr(Address(pic.objReg, offsetof(JSString, mLength)), pic.objReg);
        masm.move(ImmTag(JSVAL_TAG_INT32), pic.shapeReg);
        Jump done = masm.jump();

        JSC::ExecutablePool *ep = getExecPool(masm.size());
        if (!ep || !pic.execPools.append(ep)) {
            if (ep)
                ep->release();
            js_ReportOutOfMemory(f.cx);
            return false;
        }

        int32 typeCheckOffset = -int32(pic.u.get.typeCheckOffset);

        JSC::LinkBuffer patchBuffer(&masm, ep);
        patchBuffer.link(notString, pic.slowPathStart.labelAtOffset(typeCheckOffset));
        patchBuffer.link(done, pic.storeBack);

        CodeLocationLabel start = patchBuffer.finalizeCodeAddendum();
        JaegerSpew(JSpew_PICs, "generate string length stub at %p\n",
                   start.executableAddress());

        RepatchBuffer repatcher(pic.fastPathStart.executableAddress(), INLINE_PATH_LENGTH);
        repatcher.relink(pic.fastPathStart.jumpAtOffset(INLINE_TYPE_GUARD),
                         start);

        return true;
    }

    bool patchInline(JSObject *holder, JSScopeProperty *sprop)
    {
        spew("patch", "inline");
        PICRepatchBuffer repatcher(pic, pic.fastPathStart);

        mjit::ThreadData &jm = JS_METHODJIT_DATA(f.cx);
        if (!jm.addScript(script)) {
            js_ReportOutOfMemory(f.cx);
            return false;
        }

        int32 offset;
        if (sprop->slot < JS_INITIAL_NSLOTS) {
            JSC::CodeLocationInstruction istr;
            istr = pic.storeBack.instructionAtOffset(DSLOTS_LOAD);
            repatcher.repatchLoadPtrToLEA(istr);

            // 
            // We've patched | mov dslots, [obj + DSLOTS_OFFSET]
            // To:           | lea fslots, [obj + DSLOTS_OFFSET]
            //
            // Because the offset is wrong, it's necessary to correct it
            // below.
            //
            int32 diff = int32(offsetof(JSObject, fslots)) -
                         int32(offsetof(JSObject, dslots));
            JS_ASSERT(diff != 0);
            offset  = (int32(sprop->slot) * sizeof(Value)) + diff;
        } else {
            offset = (sprop->slot - JS_INITIAL_NSLOTS) * sizeof(Value);
        }

        uint32 shapeOffs = pic.shapeGuard + INLINE_SHAPE_OFFSET;
        repatcher.repatch(pic.fastPathStart.dataLabel32AtOffset(shapeOffs),
                          obj->shape());
        repatcher.repatch(pic.storeBack.dataLabel32AtOffset(TYPE_LOAD),
                          offset + 4);
        repatcher.repatch(pic.storeBack.dataLabel32AtOffset(DATA_LOAD),
                          offset);

        pic.inlinePathPatched = true;

        return true;
    }

    bool generateStub(JSObject *holder, JSScopeProperty *sprop)
    {
        Vector<Jump, 8> shapeMismatches(f.cx);

        Assembler masm;

        if (pic.objNeedsRemat()) {
            if (pic.objRemat() >= sizeof(JSStackFrame))
                masm.loadData32(Address(JSFrameReg, pic.objRemat()), pic.objReg);
            else
                masm.move(RegisterID(pic.objRemat()), pic.objReg);
            pic.u.get.objNeedsRemat = false;
        }

        Label start;
        Jump shapeGuard;
        if (obj->isDenseArray()) {
            start = masm.label();
            shapeGuard = masm.branchPtr(Assembler::NotEqual,
                                        Address(pic.objReg, offsetof(JSObject, clasp)),
                                        ImmPtr(obj->getClass()));
        } else {
            if (pic.shapeNeedsRemat()) {
                masm.loadShape(pic.objReg, pic.shapeReg);
                pic.u.get.shapeRegHasBaseShape = true;
            }

            start = masm.label();
            shapeGuard = masm.branch32_force32(Assembler::NotEqual, pic.shapeReg,
                                               Imm32(obj->shape()));
        }

        if (!shapeMismatches.append(shapeGuard))
            return false;

        if (obj != holder) {
            // Emit code that walks the prototype chain.
            JSObject *tempObj = obj;
            Address fslot(pic.objReg, offsetof(JSObject, fslots) + JSSLOT_PROTO * sizeof(Value));
            do {
                tempObj = tempObj->getProto();
                // FIXME: we should find out why this condition occurs. It is probably
                // related to PICs on globals.
                if (!tempObj)
                    return false;
                JS_ASSERT(tempObj);
                JS_ASSERT(tempObj->isNative());

                masm.loadData32(fslot, pic.objReg);
                pic.u.get.shapeRegHasBaseShape = false;
                pic.u.get.objNeedsRemat = true;

                Jump j = masm.branchTestPtr(Assembler::Zero, pic.objReg, pic.objReg);
                if (!shapeMismatches.append(j))
                    return false;
            } while (tempObj != holder);

            // Load the shape out of the holder and check it.
            masm.loadShape(pic.objReg, pic.shapeReg);
            Jump j = masm.branch32_force32(Assembler::NotEqual, pic.shapeReg,
                                           Imm32(holder->shape()));
            if (!shapeMismatches.append(j))
                return false;
            pic.u.get.secondShapeGuard = masm.distanceOf(masm.label()) - masm.distanceOf(start);
        } else {
            pic.u.get.secondShapeGuard = 0;
        }

        /* Load the value out of the object. */
        masm.loadSlot(pic.objReg, pic.objReg, sprop->slot, pic.shapeReg, pic.objReg);
        Jump done = masm.jump();

        JSC::ExecutablePool *ep = getExecPool(masm.size());
        if (!ep) {
            js_ReportOutOfMemory(f.cx);
            return false;
        }

        // :TODO: this can OOM 
        JSC::LinkBuffer buffer(&masm, ep);

        if (!pic.execPools.append(ep)) {
            ep->release();
            js_ReportOutOfMemory(f.cx);
            return false;
        }

        // The guard exit jumps to the original slow case.
        for (Jump *pj = shapeMismatches.begin(); pj != shapeMismatches.end(); ++pj)
            buffer.link(*pj, pic.slowPathStart);

        // The final exit jumps to the store-back in the inline stub.
        buffer.link(done, pic.storeBack);
        CodeLocationLabel cs = buffer.finalizeCodeAddendum();
        JaegerSpew(JSpew_PICs, "generated %s stub at %p\n", type, cs.executableAddress());

        PICRepatchBuffer repatcher(pic, pic.lastPathStart()); 
        patchPreviousToHere(repatcher, cs);

        pic.stubsGenerated++;
        pic.lastStubStart = buffer.locationOf(start);

        if (pic.stubsGenerated == MAX_STUBS)
            disable("max stubs reached");
        if (obj->isDenseArray())
            disable("dense array");

        return true;
    }

    void patchPreviousToHere(PICRepatchBuffer &repatcher, CodeLocationLabel cs)
    {
        // Patch either the inline fast path or a generated stub. The stub
        // omits the prefix of the inline fast path that loads the shape, so
        // the offsets are different.
        int shapeGuardJumpOffset;
        if (pic.stubsGenerated)
            shapeGuardJumpOffset = STUB_SHAPE_JUMP;
        else
            shapeGuardJumpOffset = pic.shapeGuard + INLINE_SHAPE_JUMP;
        repatcher.relink(shapeGuardJumpOffset, cs);
        if (lastStubSecondShapeGuard)
            repatcher.relink(lastStubSecondShapeGuard, cs);
    }

    bool update()
    {
        if (!pic.hit) {
            spew("first hit", "nop");
            pic.hit = true;
            return true;
        }

        JSObject *aobj = js_GetProtoIfDenseArray(obj);
        if (!aobj->isNative())
            return disable("non-native");

        JSObject *holder;
        JSProperty *prop;
        if (!aobj->lookupProperty(f.cx, ATOM_TO_JSID(atom), &holder, &prop))
            return false;

        if (!prop)
            return disable("lookup failed");

        AutoPropertyDropper dropper(f.cx, holder, prop);

        if (!holder->isNative())
            return disable("non-native holder");

        JSScopeProperty *sprop = (JSScopeProperty *)prop;
        if (!sprop->hasDefaultGetterOrIsMethod())
            return disable("getter");
        if (!SPROP_HAS_VALID_SLOT(sprop, holder->scope()))
            return disable("invalid slot");

        if (obj == holder && !pic.inlinePathPatched)
            return patchInline(holder, sprop);
        else
            return generateStub(holder, sprop);

        return true;
    }

    bool disable(const char *reason)
    {
        return PICStubCompiler::disable(reason, stub);
    }
};

void JS_FASTCALL
ic::GetProp(VMFrame &f, uint32 index)
{
    JSScript *script = f.fp->script;
    PICInfo &pic = script->pics[index];

    JSAtom *atom = pic.atom;
    if (atom == f.cx->runtime->atomState.lengthAtom) {
        if (f.regs.sp[-1].isString()) {
            GetPropCompiler cc(f, script, NULL, pic, NULL, stubs::Length);
            if (!cc.generateStringLengthStub()) {
                cc.disable("error");
                THROW();
            }
            JSString *str = f.regs.sp[-1].asString();
            f.regs.sp[-1].setInt32(str->length());
            return;
        } else if (!f.regs.sp[-1].isPrimitive()) {
            JSObject *obj = &f.regs.sp[-1].asObject();
            if (obj->isArray()) {
                GetPropCompiler cc(f, script, obj, pic, NULL, stubs::Length);
                if (!cc.generateArrayLengthStub()) {
                    cc.disable("error");
                    THROW();
                }
                f.regs.sp[-1].setNumber(obj->getArrayLength());
                return;
            }
        }
        atom = f.cx->runtime->atomState.lengthAtom;
    }

    JSObject *obj = ValueToObject(f.cx, &f.regs.sp[-1]);
    if (!obj)
        THROW();

    if (pic.shouldGenerate()) {
        GetPropCompiler cc(f, script, obj, pic, atom, stubs::GetProp);
        if (!cc.update()) {
            cc.disable("error");
            THROW();
        }
    }

    Value v;
    if (!obj->getProperty(f.cx, ATOM_TO_JSID(atom), &v))
        THROW();
    f.regs.sp[-1] = v;
}

static void JS_FASTCALL
SetPropDumb(VMFrame &f, uint32 index)
{
    JSScript *script = f.fp->script;
    ic::PICInfo &pic = script->pics[index];
    JS_ASSERT(pic.kind == ic::PICInfo::SET);
    JSAtom *atom = pic.atom;

    JSObject *obj = ValueToObject(f.cx, &f.regs.sp[-2]);
    if (!obj)
        THROW();
    Value rval = f.regs.sp[-1];
    if (!obj->setProperty(f.cx, ATOM_TO_JSID(atom), &f.regs.sp[-1]))
        THROW();
    f.regs.sp[-2] = rval;
}

static void JS_FASTCALL
SetPropSlow(VMFrame &f, uint32 index)
{
    JSScript *script = f.fp->script;
    ic::PICInfo &pic = script->pics[index];
    JS_ASSERT(pic.kind == ic::PICInfo::SET);

    JSAtom *atom = pic.atom;
    stubs::SetName(f, atom);
}

void JS_FASTCALL
ic::SetProp(VMFrame &f, uint32 index)
{
    JSObject *obj = ValueToObject(f.cx, &f.regs.sp[-2]);
    if (!obj)
        THROW();

    JSScript *script = f.fp->script;
    ic::PICInfo &pic = script->pics[index];
    JSAtom *atom = pic.atom;
    JS_ASSERT(pic.kind == ic::PICInfo::SET);

    //
    // Important: We update the PIC before looking up the property so that the
    // PIC is updated only if the property already exists. The PIC doesn't try
    // to optimize adding new properties; that is for the slow case.
    //
    // Also note, we can't use SetName for PROPINC PICs because the property
    // cache can't handle a GET and SET from the same scripted PC.
    //

    VoidStubUInt32 stub;
    switch (JSOp(*f.regs.pc)) {
      case JSOP_PROPINC:
      case JSOP_PROPDEC:
      case JSOP_INCPROP:
      case JSOP_DECPROP:
        stub = SetPropDumb;
        break;
      default:
        stub = SetPropSlow;
        break;
    }

    SetPropCompiler cc(f, script, obj, pic, atom, stub);
    if (!cc.update()) {
        cc.disable("error");
        THROW();
    }
    
    Value rval = f.regs.sp[-1];
    if (!obj->setProperty(f.cx, ATOM_TO_JSID(atom), &f.regs.sp[-1]))
        THROW();
    f.regs.sp[-2] = rval;
}

static void JS_FASTCALL
CallPropSlow(VMFrame &f, uint32 index)
{
    JSScript *script = f.fp->script;
    ic::PICInfo &pic = script->pics[index];
    stubs::CallProp(f, pic.atom);
}

void JS_FASTCALL
ic::CallProp(VMFrame &f, uint32 index)
{
    JSContext *cx = f.cx;
    JSFrameRegs &regs = f.regs;

    JSScript *script = f.fp->script;
    ic::PICInfo &pic = script->pics[index];
    JSAtom *origAtom = pic.atom;

    Value lval;
    lval = regs.sp[-1];

    Value objv;
    if (lval.isObject()) {
        objv = lval;
    } else {
        JSProtoKey protoKey;
        if (lval.isString()) {
            protoKey = JSProto_String;
        } else if (lval.isNumber()) {
            protoKey = JSProto_Number;
        } else if (lval.isBoolean()) {
            protoKey = JSProto_Boolean;
        } else {
            JS_ASSERT(lval.isNull() || lval.isUndefined());
            js_ReportIsNullOrUndefined(cx, -1, lval, NULL);
            THROW();
        }
        JSObject *pobj;
        if (!js_GetClassPrototype(cx, NULL, protoKey, &pobj))
            THROW();
        objv.setNonFunObj(*pobj);
    }

    JSObject *aobj = js_GetProtoIfDenseArray(&objv.asObject());
    Value rval;

    bool usePIC = true;

    PropertyCacheEntry *entry;
    JSObject *obj2;
    JSAtom *atom;
    JS_PROPERTY_CACHE(cx).test(cx, regs.pc, aobj, obj2, entry, atom);
    if (!atom) {
        if (entry->vword.isFunObj()) {
            rval.setFunObj(entry->vword.toFunObj());
        } else if (entry->vword.isSlot()) {
            uint32 slot = entry->vword.toSlot();
            JS_ASSERT(slot < obj2->scope()->freeslot);
            rval = obj2->lockedGetSlot(slot);
        } else {
            JS_ASSERT(entry->vword.isSprop());
            JSScopeProperty *sprop = entry->vword.toSprop();
            NATIVE_GET(cx, &objv.asObject(), obj2, sprop, JSGET_NO_METHOD_BARRIER, &rval,
                       THROW());
        }
        regs.sp++;
        regs.sp[-2] = rval;
        regs.sp[-1] = lval;
        goto end_callprop;
    }

    /*
     * Cache miss: use the immediate atom that was loaded for us under
     * PropertyCache::test.
     */
    jsid id;
    id = ATOM_TO_JSID(origAtom);

    regs.sp++;
    regs.sp[-1].setNull();
    if (lval.isObject()) {
        if (!js_GetMethod(cx, &objv.asObject(), id,
                          JS_LIKELY(aobj->map->ops->getProperty == js_GetProperty)
                          ? JSGET_CACHE_RESULT | JSGET_NO_METHOD_BARRIER
                          : JSGET_NO_METHOD_BARRIER,
                          &rval)) {
            THROW();
        }
        regs.sp[-1] = objv;
        regs.sp[-2] = rval;
    } else {
        JS_ASSERT(objv.asObject().map->ops->getProperty == js_GetProperty);
        if (!js_GetPropertyHelper(cx, &objv.asObject(), id,
                                  JSGET_CACHE_RESULT | JSGET_NO_METHOD_BARRIER,
                                  &rval)) {
            THROW();
        }
        regs.sp[-1] = lval;
        regs.sp[-2] = rval;
    }

  end_callprop:
    /* Wrap primitive lval in object clothing if necessary. */
    if (lval.isPrimitive()) {
        /* FIXME: https://bugzilla.mozilla.org/show_bug.cgi?id=412571 */
        if (!rval.isFunObj() ||
            !PrimitiveThisTest(GET_FUNCTION_PRIVATE(cx, &rval.asFunObj()), lval)) {
            if (!js_PrimitiveToObject(cx, &regs.sp[-1]))
                THROW();
            usePIC = false;
        }
    }

    GetPropCompiler cc(f, script, &objv.asObject(), pic, origAtom, CallPropSlow);
    if (usePIC) {
        if (lval.isObject()) {
            if (!cc.update()) {
                cc.disable("error");
                THROW();
            }
        } else if (lval.isString()) {
            if (!cc.generateStringCallStub()) {
                cc.disable("error");
                THROW();
            }
        } else {
            cc.disable("non-string primitive");
        }
    } else {
        cc.disable("wrapped primitive");
    }

#if JS_HAS_NO_SUCH_METHOD
    if (JS_UNLIKELY(rval.isUndefined())) {
        regs.sp[-2].setString(ATOM_TO_STRING(origAtom));
        if (!js_OnUnknownMethod(cx, regs.sp - 2))
            THROW();
    }
#endif
}

void
ic::PurgePICs(JSContext *cx, JSScript *script)
{
    uint32 npics = script->numPICs();
    for (uint32 i = 0; i < npics; i++) {
        ic::PICInfo &pic = script->pics[i];
        if (pic.kind == ic::PICInfo::SET)
            SetPropCompiler::reset(pic);
        else
            GetPropCompiler::reset(pic);
        pic.reset();
    }
}
#endif

