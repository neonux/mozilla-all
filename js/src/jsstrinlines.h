/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef jsstrinlines_h___
#define jsstrinlines_h___

#include "jsstr.h"

inline JSString *
JSString::unitString(jschar c)
{
    JS_ASSERT(c < UNIT_STRING_LIMIT);
    return const_cast<JSString *>(&unitStringTable[c]);
}

inline JSString *
JSString::getUnitString(JSContext *cx, JSString *str, size_t index)
{
    JS_ASSERT(index < str->length());
    jschar c = str->chars()[index];
    if (c < UNIT_STRING_LIMIT)
        return unitString(c);
    return js_NewDependentString(cx, str, index, 1);
}

inline JSString *
JSString::length2String(jschar c1, jschar c2)
{
    JS_ASSERT(fitsInSmallChar(c1));
    JS_ASSERT(fitsInSmallChar(c2));
    return const_cast<JSString *>
           (&length2StringTable[(((size_t)toSmallChar[c1]) << 6) + toSmallChar[c2]]);
}

inline JSString *
JSString::length2String(uint32 i)
{
    JS_ASSERT(i < 100);
    return length2String('0' + i / 10, '0' + i % 10);
}

inline JSString *
JSString::intString(jsint i)
{
    jsuint u = jsuint(i);
    JS_ASSERT(u < INT_STRING_LIMIT);
    return const_cast<JSString *>(JSString::intStringTable[u]);
}

/* Get a static atomized string for chars if possible. */
inline JSString *
JSString::lookupStaticString(const jschar *chars, size_t length)
{
    if (length == 1) {
        if (chars[0] < UNIT_STRING_LIMIT)
            return unitString(chars[0]);
    }

    if (length == 2) {
        if (fitsInSmallChar(chars[0]) && fitsInSmallChar(chars[1]))
            return length2String(chars[0], chars[1]);
    }

    /*
     * Here we know that JSString::intStringTable covers only 256 (or at least
     * not 1000 or more) chars. We rely on order here to resolve the unit vs.
     * int string/length-2 string atom identity issue by giving priority to unit
     * strings for "0" through "9" and length-2 strings for "10" through "99".
     */
    JS_STATIC_ASSERT(INT_STRING_LIMIT <= 999);
    if (length == 3) {
        if ('1' <= chars[0] && chars[0] <= '9' &&
            '0' <= chars[1] && chars[1] <= '9' &&
            '0' <= chars[2] && chars[2] <= '9') {
            jsint i = (chars[0] - '0') * 100 +
                      (chars[1] - '0') * 10 +
                      (chars[2] - '0');

            if (jsuint(i) < INT_STRING_LIMIT)
                return intString(i);
        }
    }

    return NULL;
}

inline void
JSString::finalize(JSContext *cx) {
    JS_ASSERT(!JSString::isStatic(this));
    JS_RUNTIME_UNMETER(cx->runtime, liveStrings);
    if (isDependent()) {
        JS_ASSERT(dependentBase());
        JS_RUNTIME_UNMETER(cx->runtime, liveDependentStrings);
    } else if (isFlat()) {
        /*
         * flatChars for stillborn string is null, but cx->free checks
         * for a null pointer on its own.
         */
        cx->free(flatChars());
    } else if (isTopNode()) {
        cx->free(topNodeBuffer());
    }
}

inline void
JSShortString::finalize(JSContext *cx)
{
    JS_ASSERT(!JSString::isStatic(header()));
    JS_ASSERT(header()->isFlat());
    JS_RUNTIME_UNMETER(cx->runtime, liveStrings);
}

inline void
JSExternalString::finalize(JSContext *cx)
{
    JS_ASSERT(unsigned(externalStringType) < JS_ARRAY_LENGTH(str_finalizers));
    JS_ASSERT(!isStatic(this));
    JS_ASSERT(isFlat());
    JS_RUNTIME_UNMETER(cx->runtime, liveStrings);

    /* A stillborn string has null chars. */
    jschar *chars = flatChars();
    if (!chars)
        return;
    JSStringFinalizeOp finalizer = str_finalizers[externalStringType];
    if (finalizer)
        finalizer(cx, this);
}

inline void
JSExternalString::finalize()
{
    JS_ASSERT(unsigned(externalStringType) < JS_ARRAY_LENGTH(str_finalizers));
    JSStringFinalizeOp finalizer = str_finalizers[externalStringType];
    if (finalizer) {
        /*
         * Assume that the finalizer for the permanently interned
         * string knows how to deal with null context.
         */
        finalizer(NULL, this);
    }
}

inline
JSRopeBuilder::JSRopeBuilder(JSContext *cx)
  : cx(cx), mStr(cx->runtime->emptyString) {}

#endif /* jsstrinlines_h___ */
