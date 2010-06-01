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

#include "StubCalls.h"
#include "StubCompiler.h"
#include "Compiler.h"
#include "assembler/assembler/LinkBuffer.h"
#include "FrameState-inl.h"

using namespace js;
using namespace mjit;

StubCompiler::StubCompiler(JSContext *cx, mjit::Compiler &cc, FrameState &frame, JSScript *script)
  : cx(cx), cc(cc), frame(frame), script(script), generation(1), lastGeneration(0),
    exits(SystemAllocPolicy()), joins(SystemAllocPolicy()), jumpList(SystemAllocPolicy())
{
}

bool
StubCompiler::init(uint32 nargs)
{
    return true;
}

/*
 * The "slow path" generation is interleaved with the main compilation phase,
 * though it is generated into a separate buffer. The fast path can "exit"
 * into the slow path, and the slow path rejoins into the fast path. The slow
 * path is kept in a separate buffer, but appended to the main one, for ideal
 * icache locality.
 */
void
StubCompiler::linkExit(Jump j)
{
    JaegerSpew(JSpew_Insns, " ---- BEGIN SLOW MERGE CODE ---- \n");
    if (lastGeneration == generation) {
        Jump j2 = masm.jump();
        jumpList.append(j2);
    }
    exits.append(CrossPatch(j, masm.label()));
    frame.sync(masm);
    lastGeneration = generation;
    JaegerSpew(JSpew_Insns, " ---- END SLOW MERGE CODE ---- \n");
}

void
StubCompiler::leave()
{
    for (size_t i = 0; i < jumpList.length(); i++)
        jumpList[i].linkTo(masm.label(), &masm);
    jumpList.clear();
}

void
StubCompiler::rejoin(uint32 invalidationDepth)
{
    JaegerSpew(JSpew_Insns, " ---- BEGIN SLOW RESTORE CODE ---- \n");

    frame.merge(masm, invalidationDepth);

    Jump j = masm.jump();
    joins.append(CrossPatch(j, cc.getLabel()));

    JaegerSpew(JSpew_Insns, " ---- END SLOW RESTORE CODE ---- \n");
}

typedef JSC::MacroAssembler::RegisterID RegisterID;
typedef JSC::MacroAssembler::ImmPtr ImmPtr;
typedef JSC::MacroAssembler::Imm32 Imm32;

JSC::MacroAssembler::Call
StubCompiler::stubCall(void *ptr)
{
    return stubCall(ptr, frame.stackDepth() + script->nfixed);
}

JSC::MacroAssembler::Call
StubCompiler::stubCall(void *ptr, uint32 slots)
{
    generation++;
    JaegerSpew(JSpew_Insns, " ---- BEGIN SLOW CALL CODE ---- \n");
    Call cl = masm.stubCall(ptr, cc.getPC(), slots);
    JaegerSpew(JSpew_Insns, " ---- END SLOW CALL CODE ---- \n");
    return cl;
}

void
StubCompiler::fixCrossJumps(uint8 *ncode, size_t offset, size_t total)
{
    JSC::LinkBuffer fast(ncode, total);
    JSC::LinkBuffer slow(ncode + offset, total - offset);

    for (size_t i = 0; i < exits.length(); i++)
        fast.link(exits[i].from, slow.locationOf(exits[i].to));

    for (size_t i = 0; i < scriptJoins.length(); i++) {
        const CrossJumpInScript &cj = scriptJoins[i];
        slow.link(cj.from, fast.locationOf(cc.labelOf(cj.pc)));
    }

    for (size_t i = 0; i < joins.length(); i++)
        slow.link(joins[i].from, fast.locationOf(joins[i].to));
}

void
StubCompiler::finalize(uint8 *ncode)
{
    masm.finalize(ncode);
}

JSC::MacroAssembler::Call
StubCompiler::vpInc(JSOp op, bool pushed)
{
    uint32 slots = frame.stackDepth() + script->nfixed;
    if (pushed) {
        JS_ASSERT(frame.stackDepth());
        slots--;
    }

    VoidVpStub stub = NULL;
    switch (op) {
      case JSOP_GLOBALINC:
        stub = stubs::VpInc;
        break;

      case JSOP_GLOBALDEC:
        stub = stubs::VpDec;
        break;

      case JSOP_INCGLOBAL:
        stub = stubs::IncVp;
        break;

      case JSOP_DECGLOBAL:
        stub = stubs::DecVp;
        break;

      default:
        JS_NOT_REACHED("unknown incdec op");
        break;
    }

    return stubCall(JS_FUNC_TO_DATA_PTR(void *, stub), slots);
}

void
StubCompiler::jumpInScript(Jump j, jsbytecode *target)
{
    if (cc.knownJump(target))
        joins.append(CrossPatch(j, cc.labelOf(target)));
    else
        scriptJoins.append(CrossJumpInScript(j, target));
}

