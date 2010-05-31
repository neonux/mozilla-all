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

void * JS_FASTCALL Call(VMFrame &f, uint32 argc);
void * JS_FASTCALL Return(VMFrame &f);

JSObject * JS_FASTCALL BindName(VMFrame &f);
void JS_FASTCALL SetName(VMFrame &f, uint32 index);
void JS_FASTCALL Name(VMFrame &f, uint32 index);
void JS_FASTCALL CallName(VMFrame &f, uint32 index);

void JS_FASTCALL VpInc(VMFrame &f, Value *vp);
void JS_FASTCALL VpDec(VMFrame &f, Value *vp);
void JS_FASTCALL DecVp(VMFrame &f, Value *vp);
void JS_FASTCALL IncVp(VMFrame &f, Value *vp);

void JS_FASTCALL BitAnd(VMFrame &f);

}}} /* namespace stubs,mjit,js */

extern "C" void *
js_InternalThrow(js::VMFrame &f);

#endif /* jslogic_h__ */

