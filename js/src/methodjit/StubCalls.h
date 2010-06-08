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

#ifndef jslogic_h__
#define jslogic_h__

#include "MethodJIT.h"

namespace js {
namespace mjit {
namespace stubs {

void JS_FASTCALL This(VMFrame &f);
JSObject * JS_FASTCALL NewInitArray(VMFrame &f);
JSObject * JS_FASTCALL NewInitObject(VMFrame &f, uint32 empty);
JSObject * JS_FASTCALL NewArray(VMFrame &f, uint32 len);
void JS_FASTCALL Interrupt(VMFrame &f);
void JS_FASTCALL InitElem(VMFrame &f, uint32 last);
void JS_FASTCALL InitProp(VMFrame &f, JSAtom *atom);
void JS_FASTCALL EndInit(VMFrame &f);
JSString * JS_FASTCALL ConcatN(VMFrame &f, uint32 argc);

void * JS_FASTCALL Call(VMFrame &f, uint32 argc);
void * JS_FASTCALL New(VMFrame &f, uint32 argc);
void * JS_FASTCALL Return(VMFrame &f);

JSObject * JS_FASTCALL BindName(VMFrame &f);
void JS_FASTCALL SetName(VMFrame &f, uint32 index);
void JS_FASTCALL Name(VMFrame &f, uint32 index);
void JS_FASTCALL GetProp(VMFrame &f);
void JS_FASTCALL GetElem(VMFrame &f);
void JS_FASTCALL SetElem(VMFrame &f);
void JS_FASTCALL CallName(VMFrame &f, uint32 index);
void JS_FASTCALL GetUpvar(VMFrame &f, uint32 index);

void JS_FASTCALL NameInc(VMFrame &f, JSAtom *atom);
void JS_FASTCALL NameDec(VMFrame &f, JSAtom *atom);
void JS_FASTCALL IncName(VMFrame &f, JSAtom *atom);
void JS_FASTCALL DecName(VMFrame &f, JSAtom *atom);
void JS_FASTCALL PropInc(VMFrame &f, JSAtom *atom);
void JS_FASTCALL PropDec(VMFrame &f, JSAtom *atom);
void JS_FASTCALL IncProp(VMFrame &f, JSAtom *atom);
void JS_FASTCALL DecProp(VMFrame &f, JSAtom *atom);
void JS_FASTCALL CallProp(VMFrame &f, JSAtom *atom);

void JS_FASTCALL DefFun(VMFrame &f, uint32 index);
JSObject * JS_FASTCALL DefLocalFun(VMFrame &f, JSFunction *fun);
JSObject * JS_FASTCALL RegExp(VMFrame &f, JSObject *regex);
JSObject * JS_FASTCALL Lambda(VMFrame &f, JSFunction *fun);

void JS_FASTCALL VpInc(VMFrame &f, Value *vp);
void JS_FASTCALL VpDec(VMFrame &f, Value *vp);
void JS_FASTCALL DecVp(VMFrame &f, Value *vp);
void JS_FASTCALL IncVp(VMFrame &f, Value *vp);

JSBool JS_FASTCALL LessThan(VMFrame &f);
JSBool JS_FASTCALL LessEqual(VMFrame &f);
JSBool JS_FASTCALL GreaterThan(VMFrame &f);
JSBool JS_FASTCALL GreaterEqual(VMFrame &f);
JSBool JS_FASTCALL Equal(VMFrame &f);
JSBool JS_FASTCALL NotEqual(VMFrame &f);

void JS_FASTCALL BitOr(VMFrame &f);
void JS_FASTCALL BitXor(VMFrame &f);
void JS_FASTCALL BitAnd(VMFrame &f);
void JS_FASTCALL BitNot(VMFrame &f);
void JS_FASTCALL Lsh(VMFrame &f);
void JS_FASTCALL Rsh(VMFrame &f);
void JS_FASTCALL Ursh(VMFrame &f);
void JS_FASTCALL Add(VMFrame &f);
void JS_FASTCALL Sub(VMFrame &f);
void JS_FASTCALL Mul(VMFrame &f);
void JS_FASTCALL Div(VMFrame &f);
void JS_FASTCALL Mod(VMFrame &f);
void JS_FASTCALL Neg(VMFrame &f);
void JS_FASTCALL ObjToStr(VMFrame &f);
void JS_FASTCALL Not(VMFrame &f);
JSBool JS_FASTCALL StrictEq(VMFrame &f);
JSBool JS_FASTCALL StrictNe(VMFrame &f);

void JS_FASTCALL Iter(VMFrame &f, uint32 flags);
void JS_FASTCALL IterNext(VMFrame &f);
JSBool JS_FASTCALL IterMore(VMFrame &f);
void JS_FASTCALL EndIter(VMFrame &f);

JSBool JS_FASTCALL ValueToBoolean(VMFrame &f);
JSString * JS_FASTCALL TypeOf(VMFrame &f);

}}} /* namespace stubs,mjit,js */

extern "C" void *
js_InternalThrow(js::VMFrame &f);

#endif /* jslogic_h__ */

