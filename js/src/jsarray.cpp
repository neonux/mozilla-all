/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set sw=4 ts=8 et tw=78:
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

/*
 * JS array class.
 *
 * Array objects begin as "dense" arrays, optimized for index-only property
 * access over a vector of slots with high load factor.  Array methods
 * optimize for denseness by testing that the object's class is
 * &ArrayClass, and can then directly manipulate the slots for efficiency.
 *
 * We track these pieces of metadata for arrays in dense mode:
 *  - The array's length property as a uint32, accessible with
 *    getArrayLength(), setArrayLength().
 *  - The number of element slots (capacity), gettable with
 *    getDenseArrayCapacity().
 *  - The array's initialized length, accessible with
 *    getDenseArrayInitializedLength().
 *
 * In dense mode, holes in the array are represented by
 * MagicValue(JS_ARRAY_HOLE) invalid values.
 *
 * NB: the capacity and length of a dense array are entirely unrelated!  The
 * length may be greater than, less than, or equal to the capacity. The first
 * case may occur when the user writes "new Array(100), in which case the
 * length is 100 while the capacity remains 0 (indices below length and above
 * capacity must be treated as holes). See array_length_setter for another
 * explanation of how the first case may occur.
 *
 * The initialized length of a dense array specifies the number of elements
 * that have been initialized. All elements above the initialized length are
 * holes in the array, and the memory for all elements between the initialized
 * length and capacity is left uninitialized. When type inference is disabled,
 * the initialized length always equals the array's capacity. When inference is
 * enabled, the initialized length is some value less than or equal to both the
 * array's length and the array's capacity.
 *
 * With inference enabled, there is flexibility in exactly the value the
 * initialized length must hold, e.g. if an array has length 5, capacity 10,
 * completely empty, it is valid for the initialized length to be any value
 * between zero and 5, as long as the in memory values below the initialized
 * length have been initialized with a hole value. However, in such cases we
 * want to keep the initialized length as small as possible: if the array is
 * known to have no hole values below its initialized length, then it is a
 * "packed" array and can be accessed much faster by JIT code.
 *
 * Arrays are converted to use SlowArrayClass when any of these conditions
 * are met:
 *  - there are more than MIN_SPARSE_INDEX slots total and the load factor
 *    (COUNT / capacity) is less than 0.25
 *  - a property is set that is not indexed (and not "length")
 *  - a property is defined that has non-default property attributes.
 *
 * Dense arrays do not track property creation order, so unlike other native
 * objects and slow arrays, enumerating an array does not necessarily visit the
 * properties in the order they were created.  We could instead maintain the
 * scope to track property enumeration order, but still use the fast slot
 * access.  That would have the same memory cost as just using a
 * SlowArrayClass, but have the same performance characteristics as a dense
 * array for slot accesses, at some cost in code complexity.
 */
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "mozilla/RangedPtr.h"

#include "jstypes.h"
#include "jsstdint.h"
#include "jsutil.h"

#include "jsapi.h"
#include "jsarray.h"
#include "jsatom.h"
#include "jsbool.h"
#include "jsbuiltins.h"
#include "jscntxt.h"
#include "jsversion.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jsgcmark.h"
#include "jsinterp.h"
#include "jsiter.h"
#include "jslock.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsscope.h"
#include "jsstr.h"
#include "jstracer.h"
#include "jswrapper.h"
#include "methodjit/MethodJIT.h"
#include "methodjit/StubCalls.h"
#include "methodjit/StubCalls-inl.h"

#include "vm/ArgumentsObject.h"

#include "jsarrayinlines.h"
#include "jsatominlines.h"
#include "jscntxtinlines.h"
#include "jsobjinlines.h"
#include "jsscopeinlines.h"
#include "jscntxtinlines.h"
#include "jsstrinlines.h"

#include "vm/ArgumentsObject-inl.h"
#include "vm/Stack-inl.h"

using namespace mozilla;
using namespace js;
using namespace js::gc;
using namespace js::types;

JSBool
js_GetLengthProperty(JSContext *cx, JSObject *obj, jsuint *lengthp)
{
    if (obj->isArray()) {
        *lengthp = obj->getArrayLength();
        return true;
    }

    if (obj->isArguments()) {
        ArgumentsObject *argsobj = obj->asArguments();
        if (!argsobj->hasOverriddenLength()) {
            *lengthp = argsobj->initialLength();
            return true;
        }
    }

    AutoValueRooter tvr(cx);
    if (!obj->getProperty(cx, cx->runtime->atomState.lengthAtom, tvr.addr()))
        return false;

    if (tvr.value().isInt32()) {
        *lengthp = jsuint(jsint(tvr.value().toInt32())); /* jsuint cast does ToUint32 */
        return true;
    }

    JS_STATIC_ASSERT(sizeof(jsuint) == sizeof(uint32_t));
    return ValueToECMAUint32(cx, tvr.value(), (uint32_t *)lengthp);
}

namespace js {

/*
 * Determine if the id represents an array index or an XML property index.
 *
 * An id is an array index according to ECMA by (15.4):
 *
 * "Array objects give special treatment to a certain class of property names.
 * A property name P (in the form of a string value) is an array index if and
 * only if ToString(ToUint32(P)) is equal to P and ToUint32(P) is not equal
 * to 2^32-1."
 *
 * This means the largest allowed index is actually 2^32-2 (4294967294).  
 * 
 * In our implementation, it would be sufficient to check for JSVAL_IS_INT(id)
 * except that by using signed 31-bit integers we miss the top half of the
 * valid range. This function checks the string representation itself; note
 * that calling a standard conversion routine might allow strings such as
 * "08" or "4.0" as array indices, which they are not.
 *
 */
JS_FRIEND_API(bool)
StringIsArrayIndex(JSLinearString *str, jsuint *indexp)
{
    const jschar *s = str->chars();
    uint32 length = str->length();
    const jschar *end = s + length;

    if (length == 0 || length > (sizeof("4294967294") - 1) || !JS7_ISDEC(*s))
        return false;

    uint32 c = 0, previous = 0;    
    uint32 index = JS7_UNDEC(*s++);

    /* Don't allow leading zeros. */
    if (index == 0 && s != end)
        return false;

    for (; s < end; s++) {
        if (!JS7_ISDEC(*s))
            return false;

        previous = index;
        c = JS7_UNDEC(*s);
        index = 10 * index + c;
    }

    /* Make sure we didn't overflow. */
    if (previous < (MAX_ARRAY_INDEX / 10) || (previous == (MAX_ARRAY_INDEX / 10) && 
        c <= (MAX_ARRAY_INDEX % 10))) {
        JS_ASSERT(index <= MAX_ARRAY_INDEX);
        *indexp = index;
        return true;
    }
    
    return false;
}

}

static JSBool
BigIndexToId(JSContext *cx, JSObject *obj, jsuint index, JSBool createAtom,
             jsid *idp)
{
    JS_STATIC_ASSERT((jsuint)-1 == 4294967295U);
    JS_ASSERT(index > JSID_INT_MAX);

    jschar buf[10];
    jschar *start = ArrayEnd(buf);
    do {
        --start;
        *start = (jschar)('0' + index % 10);
        index /= 10;
    } while (index != 0);

    /*
     * Skip the atomization if the class is known to store atoms corresponding
     * to big indexes together with elements. In such case we know that the
     * array does not have an element at the given index if its atom does not
     * exist.  Dense arrays don't use atoms for any indexes, though it would be
     * rare to see them have a big index in any case.
     */
    JSAtom *atom;
    if (!createAtom && (obj->isSlowArray() || obj->isArguments() || obj->isObject())) {
        atom = js_GetExistingStringAtom(cx, start, ArrayEnd(buf) - start);
        if (!atom) {
            *idp = JSID_VOID;
            return JS_TRUE;
        }
    } else {
        atom = js_AtomizeChars(cx, start, ArrayEnd(buf) - start);
        if (!atom)
            return JS_FALSE;
    }

    *idp = ATOM_TO_JSID(atom);
    return JS_TRUE;
}

bool
JSObject::willBeSparseDenseArray(uintN requiredCapacity, uintN newElementsHint)
{
    JS_ASSERT(isDenseArray());
    JS_ASSERT(requiredCapacity > MIN_SPARSE_INDEX);

    uintN cap = numSlots();
    JS_ASSERT(requiredCapacity >= cap);

    if (requiredCapacity >= JSObject::NSLOTS_LIMIT)
        return true;

    uintN minimalDenseCount = requiredCapacity / 4;
    if (newElementsHint >= minimalDenseCount)
        return false;
    minimalDenseCount -= newElementsHint;

    if (minimalDenseCount > cap)
        return true;

    uintN len = getDenseArrayInitializedLength();
    const Value *elems = getDenseArrayElements();
    for (uintN i = 0; i < len; i++) {
        if (!elems[i].isMagic(JS_ARRAY_HOLE) && !--minimalDenseCount)
            return false;
    }
    return true;
}

static bool
ReallyBigIndexToId(JSContext* cx, jsdouble index, jsid* idp)
{
    return js_ValueToStringId(cx, DoubleValue(index), idp);
}

static bool
IndexToId(JSContext* cx, JSObject* obj, jsdouble index, JSBool* hole, jsid* idp,
          JSBool createAtom = JS_FALSE)
{
    if (index <= JSID_INT_MAX) {
        *idp = INT_TO_JSID(int(index));
        return JS_TRUE;
    }

    if (index <= jsuint(-1)) {
        if (!BigIndexToId(cx, obj, jsuint(index), createAtom, idp))
            return JS_FALSE;
        if (hole && JSID_IS_VOID(*idp))
            *hole = JS_TRUE;
        return JS_TRUE;
    }

    return ReallyBigIndexToId(cx, index, idp);
}

bool
JSObject::arrayGetOwnDataElement(JSContext *cx, size_t i, Value *vp)
{
    JS_ASSERT(isArray());

    if (isDenseArray()) {
        if (i >= getArrayLength())
            vp->setMagic(JS_ARRAY_HOLE);
        else
            *vp = getDenseArrayElement(uint32(i));
        return true;
    }

    JSBool hole;
    jsid id;
    if (!IndexToId(cx, this, i, &hole, &id))
        return false;

    const Shape *shape = nativeLookup(cx, id);
    if (!shape || !shape->isDataDescriptor())
        vp->setMagic(JS_ARRAY_HOLE);
    else
        *vp = getSlot(shape->slot);
    return true;
}

/*
 * If the property at the given index exists, get its value into location
 * pointed by vp and set *hole to false. Otherwise set *hole to true and *vp
 * to JSVAL_VOID. This function assumes that the location pointed by vp is
 * properly rooted and can be used as GC-protected storage for temporaries.
 */
static inline JSBool
DoGetElement(JSContext *cx, JSObject *obj, jsdouble index, JSBool *hole, Value *vp)
{
    AutoIdRooter idr(cx);

    *hole = JS_FALSE;
    if (!IndexToId(cx, obj, index, hole, idr.addr()))
        return JS_FALSE;
    if (*hole) {
        vp->setUndefined();
        return JS_TRUE;
    }

    JSObject *obj2;
    JSProperty *prop;
    if (!obj->lookupGeneric(cx, idr.id(), &obj2, &prop))
        return JS_FALSE;
    if (!prop) {
        vp->setUndefined();
        *hole = JS_TRUE;
    } else {
        if (!obj->getGeneric(cx, idr.id(), vp))
            return JS_FALSE;
        *hole = JS_FALSE;
    }
    return JS_TRUE;
}

static inline JSBool
DoGetElement(JSContext *cx, JSObject *obj, uint32 index, JSBool *hole, Value *vp)
{
    bool present;
    if (!obj->getElementIfPresent(cx, obj, index, vp, &present))
        return false;

    *hole = !present;
    if (*hole)
        vp->setUndefined();

    return true;
}

template<typename IndexType>
static JSBool
GetElement(JSContext *cx, JSObject *obj, IndexType index, JSBool *hole, Value *vp)
{
    JS_ASSERT(index >= 0);
    if (obj->isDenseArray() && index < obj->getDenseArrayInitializedLength() &&
        !(*vp = obj->getDenseArrayElement(uint32(index))).isMagic(JS_ARRAY_HOLE)) {
        *hole = JS_FALSE;
        return JS_TRUE;
    }
    if (obj->isArguments()) {
        if (obj->asArguments()->getElement(uint32(index), vp)) {
            *hole = JS_FALSE;
            return true;
        }
    }

    return DoGetElement(cx, obj, index, hole, vp);
}

namespace js {

static bool
GetElementsSlow(JSContext *cx, JSObject *aobj, uint32 length, Value *vp)
{
    for (uint32 i = 0; i < length; i++) {
        if (!aobj->getElement(cx, i, &vp[i]))
            return false;
    }

    return true;
}

bool
GetElements(JSContext *cx, JSObject *aobj, jsuint length, Value *vp)
{
    if (aobj->isDenseArray() && length <= aobj->getDenseArrayInitializedLength() &&
        !js_PrototypeHasIndexedProperties(cx, aobj)) {
        /* The prototype does not have indexed properties so hole = undefined */
        const Value *srcbeg = aobj->getDenseArrayElements();
        const Value *srcend = srcbeg + length;
        const Value *src = srcbeg;
        for (Value *dst = vp; src < srcend; ++dst, ++src)
            *dst = src->isMagic(JS_ARRAY_HOLE) ? UndefinedValue() : *src;
        return true;
    }

    if (aobj->isArguments()) {
        ArgumentsObject *argsobj = aobj->asArguments();
        if (!argsobj->hasOverriddenLength()) {
            if (argsobj->getElements(0, length, vp))
                return true;
        }
    }

    return GetElementsSlow(cx, aobj, length, vp);
}

}

/*
 * Set the value of the property at the given index to v assuming v is rooted.
 */
static JSBool
SetArrayElement(JSContext *cx, JSObject *obj, jsdouble index, const Value &v)
{
    JS_ASSERT(index >= 0);

    if (obj->isDenseArray()) {
        /* Predicted/prefetched code should favor the remains-dense case. */
        JSObject::EnsureDenseResult result = JSObject::ED_SPARSE;
        do {
            if (index > jsuint(-1))
                break;
            jsuint idx = jsuint(index);
            result = obj->ensureDenseArrayElements(cx, idx, 1);
            if (result != JSObject::ED_OK)
                break;
            if (idx >= obj->getArrayLength())
                obj->setDenseArrayLength(idx + 1);
            obj->setDenseArrayElementWithType(cx, idx, v);
            return true;
        } while (false);

        if (result == JSObject::ED_FAILED)
            return false;
        JS_ASSERT(result == JSObject::ED_SPARSE);
        if (!obj->makeDenseArraySlow(cx))
            return JS_FALSE;
    }

    AutoIdRooter idr(cx);

    if (!IndexToId(cx, obj, index, NULL, idr.addr(), JS_TRUE))
        return JS_FALSE;
    JS_ASSERT(!JSID_IS_VOID(idr.id()));

    Value tmp = v;
    return obj->setGeneric(cx, idr.id(), &tmp, true);
}

#ifdef JS_TRACER
JSBool JS_FASTCALL
js_EnsureDenseArrayCapacity(JSContext *cx, JSObject *obj, jsint i)
{
#ifdef DEBUG
    Class *origObjClasp = obj->getClass();
#endif
    jsuint u = jsuint(i);
    JSBool ret = (obj->ensureDenseArrayElements(cx, u, 1) == JSObject::ED_OK);

    /* Partially check the CallInfo's storeAccSet is correct. */
    JS_ASSERT(obj->getClass() == origObjClasp);
    return ret;
}
/* This function and its callees do not touch any object's .clasp field. */
JS_DEFINE_CALLINFO_3(extern, BOOL, js_EnsureDenseArrayCapacity, CONTEXT, OBJECT, INT32,
                     0, nanojit::ACCSET_STORE_ANY & ~tjit::ACCSET_OBJ_CLASP)
#endif

/*
 * Delete the element |index| from |obj|. If |strict|, do a strict
 * deletion: throw if the property is not configurable.
 *
 * - Return 1 if the deletion succeeds (that is, ES5's [[Delete]] would
 *   return true)
 *
 * - Return 0 if the deletion fails because the property is not
 *   configurable (that is, [[Delete]] would return false). Note that if
 *   |strict| is true we will throw, not return zero.
 *
 * - Return -1 if an exception occurs (that is, [[Delete]] would throw).
 */
static int
DeleteArrayElement(JSContext *cx, JSObject *obj, jsdouble index, bool strict)
{
    JS_ASSERT(index >= 0);
    if (obj->isDenseArray()) {
        if (index <= jsuint(-1)) {
            jsuint idx = jsuint(index);
            if (idx < obj->getDenseArrayInitializedLength()) {
                obj->markDenseArrayNotPacked(cx);
                obj->setDenseArrayElement(idx, MagicValue(JS_ARRAY_HOLE));
                if (!js_SuppressDeletedElement(cx, obj, idx))
                    return -1;
            }
        }
        return 1;
    }

    AutoIdRooter idr(cx);

    if (!IndexToId(cx, obj, index, NULL, idr.addr()))
        return -1;
    if (JSID_IS_VOID(idr.id()))
        return 1;

    Value v;
    if (!obj->deleteGeneric(cx, idr.id(), &v, strict))
        return -1;
    return v.isTrue() ? 1 : 0;
}

/*
 * When hole is true, delete the property at the given index. Otherwise set
 * its value to v assuming v is rooted.
 */
static JSBool
SetOrDeleteArrayElement(JSContext *cx, JSObject *obj, jsdouble index,
                        JSBool hole, const Value &v)
{
    if (hole) {
        JS_ASSERT(v.isUndefined());
        return DeleteArrayElement(cx, obj, index, true) >= 0;
    }
    return SetArrayElement(cx, obj, index, v);
}

JSBool
js_SetLengthProperty(JSContext *cx, JSObject *obj, jsdouble length)
{
    Value v = NumberValue(length);

    /* We don't support read-only array length yet. */
    return obj->setProperty(cx, cx->runtime->atomState.lengthAtom, &v, false);
}

/*
 * Since SpiderMonkey supports cross-class prototype-based delegation, we have
 * to be careful about the length getter and setter being called on an object
 * not of Array class. For the getter, we search obj's prototype chain for the
 * array that caused this getter to be invoked. In the setter case to overcome
 * the JSPROP_SHARED attribute, we must define a shadowing length property.
 */
static JSBool
array_length_getter(JSContext *cx, JSObject *obj, jsid id, Value *vp)
{
    do {
        if (obj->isArray()) {
            vp->setNumber(obj->getArrayLength());
            return JS_TRUE;
        }
    } while ((obj = obj->getProto()) != NULL);
    return JS_TRUE;
}

static JSBool
array_length_setter(JSContext *cx, JSObject *obj, jsid id, JSBool strict, Value *vp)
{
    if (!obj->isArray()) {
        return obj->defineProperty(cx, cx->runtime->atomState.lengthAtom, *vp,
                                   NULL, NULL, JSPROP_ENUMERATE);
    }

    uint32 newlen;
    if (!ValueToECMAUint32(cx, *vp, &newlen))
        return false;

    jsdouble d;
    if (!ToNumber(cx, *vp, &d))
        return false;

    if (d != newlen) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_ARRAY_LENGTH);
        return false;
    }

    uint32 oldlen = obj->getArrayLength();
    if (oldlen == newlen)
        return true;

    vp->setNumber(newlen);
    if (oldlen < newlen) {
        obj->setArrayLength(cx, newlen);
        return true;
    }

    if (obj->isDenseArray()) {
        /*
         * Don't reallocate if we're not actually shrinking our slots. If we do
         * shrink slots here, shrink the initialized length too.  This permits us
         * us to disregard length when reading from arrays as long we are within
         * the initialized capacity.
         */
        jsuint oldcap = obj->getDenseArrayCapacity();
        if (oldcap > newlen)
            obj->shrinkDenseArrayElements(cx, newlen);
        jsuint oldinit = obj->getDenseArrayInitializedLength();
        if (oldinit > newlen) {
            obj->setDenseArrayInitializedLength(newlen);
            if (!cx->typeInferenceEnabled())
                obj->backfillDenseArrayHoles(cx);
        }
    } else if (oldlen - newlen < (1 << 24)) {
        do {
            --oldlen;
            if (!JS_CHECK_OPERATION_LIMIT(cx)) {
                obj->setArrayLength(cx, oldlen + 1);
                return false;
            }
            int deletion = DeleteArrayElement(cx, obj, oldlen, strict);
            if (deletion <= 0) {
                obj->setArrayLength(cx, oldlen + 1);
                return deletion >= 0;
            }
        } while (oldlen != newlen);
    } else {
        /*
         * We are going to remove a lot of indexes in a presumably sparse
         * array. So instead of looping through indexes between newlen and
         * oldlen, we iterate through all properties and remove those that
         * correspond to indexes in the half-open range [newlen, oldlen).  See
         * bug 322135.
         */
        JSObject *iter = JS_NewPropertyIterator(cx, obj);
        if (!iter)
            return false;

        /* Protect iter against GC under JSObject::deleteProperty. */
        AutoObjectRooter tvr(cx, iter);

        jsuint gap = oldlen - newlen;
        for (;;) {
            if (!JS_CHECK_OPERATION_LIMIT(cx) || !JS_NextProperty(cx, iter, &id))
                return false;
            if (JSID_IS_VOID(id))
                break;
            jsuint index;
            Value junk;
            if (js_IdIsIndex(id, &index) && index - newlen < gap &&
                !obj->deleteElement(cx, index, &junk, false)) {
                return false;
            }
        }
    }

    obj->setArrayLength(cx, newlen);
    return true;
}

/* Returns true if the dense array has an own property at the index. */
static inline bool
IsDenseArrayIndex(JSObject *obj, uint32 index)
{
    JS_ASSERT(obj->isDenseArray());

    return index < obj->getDenseArrayInitializedLength() &&
           !obj->getDenseArrayElement(index).isMagic(JS_ARRAY_HOLE);
}

/*
 * We have only indexed properties up to initialized length, plus the
 * length property. For all else, we delegate to the prototype.
 */
static inline bool
IsDenseArrayId(JSContext *cx, JSObject *obj, jsid id)
{
    JS_ASSERT(obj->isDenseArray());

    uint32 i;
    return JSID_IS_ATOM(id, cx->runtime->atomState.lengthAtom) ||
           (js_IdIsIndex(id, &i) && IsDenseArrayIndex(obj, i));
}

static JSBool
array_lookupGeneric(JSContext *cx, JSObject *obj, jsid id, JSObject **objp,
                    JSProperty **propp)
{
    if (!obj->isDenseArray())
        return js_LookupProperty(cx, obj, id, objp, propp);

    if (IsDenseArrayId(cx, obj, id)) {
        *propp = (JSProperty *) 1;  /* non-null to indicate found */
        *objp = obj;
        return JS_TRUE;
    }

    JSObject *proto = obj->getProto();
    if (!proto) {
        *objp = NULL;
        *propp = NULL;
        return JS_TRUE;
    }
    return proto->lookupGeneric(cx, id, objp, propp);
}

static JSBool
array_lookupProperty(JSContext *cx, JSObject *obj, PropertyName *name, JSObject **objp,
                     JSProperty **propp)
{
    return array_lookupGeneric(cx, obj, ATOM_TO_JSID(name), objp, propp);
}

static JSBool
array_lookupElement(JSContext *cx, JSObject *obj, uint32 index, JSObject **objp,
                    JSProperty **propp)
{
    if (!obj->isDenseArray())
        return js_LookupElement(cx, obj, index, objp, propp);

    if (IsDenseArrayIndex(obj, index)) {
        *propp = (JSProperty *) 1;  /* non-null to indicate found */
        *objp = obj;
        return true;
    }

    if (JSObject *proto = obj->getProto())
        return proto->lookupElement(cx, index, objp, propp);

    *objp = NULL;
    *propp = NULL;
    return true;
}

static JSBool
array_lookupSpecial(JSContext *cx, JSObject *obj, SpecialId sid, JSObject **objp,
                    JSProperty **propp)
{
    return array_lookupGeneric(cx, obj, SPECIALID_TO_JSID(sid), objp, propp);
}

JSBool
js_GetDenseArrayElementValue(JSContext *cx, JSObject *obj, jsid id, Value *vp)
{
    JS_ASSERT(obj->isDenseArray());

    uint32 i;
    if (!js_IdIsIndex(id, &i)) {
        JS_ASSERT(JSID_IS_ATOM(id, cx->runtime->atomState.lengthAtom));
        vp->setNumber(obj->getArrayLength());
        return JS_TRUE;
    }
    *vp = obj->getDenseArrayElement(i);
    return JS_TRUE;
}

static JSBool
array_getGeneric(JSContext *cx, JSObject *obj, JSObject *receiver, jsid id, Value *vp)
{
    uint32 i;

    if (JSID_IS_ATOM(id, cx->runtime->atomState.lengthAtom)) {
        vp->setNumber(obj->getArrayLength());
        return JS_TRUE;
    }

    if (JSID_IS_ATOM(id, cx->runtime->atomState.protoAtom)) {
        vp->setObjectOrNull(obj->getProto());
        return JS_TRUE;
    }

    if (!obj->isDenseArray())
        return js_GetProperty(cx, obj, id, vp);

    if (!js_IdIsIndex(id, &i) || i >= obj->getDenseArrayInitializedLength() ||
        obj->getDenseArrayElement(i).isMagic(JS_ARRAY_HOLE)) {
        JSObject *proto = obj->getProto();
        if (!proto) {
            vp->setUndefined();
            return JS_TRUE;
        }

        return proto->getGeneric(cx, receiver, id, vp);
    }

    *vp = obj->getDenseArrayElement(i);

    /* Type information for dense array elements must be correct. */
    JS_ASSERT_IF(!obj->hasSingletonType(),
                 js::types::TypeHasProperty(cx, obj->type(), JSID_VOID, *vp));

    return JS_TRUE;
}

static JSBool
array_getProperty(JSContext *cx, JSObject *obj, JSObject *receiver, PropertyName *name, Value *vp)
{
    return array_getGeneric(cx, obj, receiver, ATOM_TO_JSID(name), vp);
}

static JSBool
array_getElement(JSContext *cx, JSObject *obj, JSObject *receiver, uint32 index, Value *vp)
{
    if (!obj->isDenseArray())
        return js_GetElement(cx, obj, index, vp);

    if (index < obj->getDenseArrayInitializedLength() &&
        !obj->getDenseArrayElement(index).isMagic(JS_ARRAY_HOLE))
    {
        *vp = obj->getDenseArrayElement(index);
        return true;
    }

    JSObject *proto = obj->getProto();
    if (!proto) {
        vp->setUndefined();
        return true;
    }

    return proto->getElement(cx, receiver, index, vp);
}

static JSBool
array_getSpecial(JSContext *cx, JSObject *obj, JSObject *receiver, SpecialId sid, Value *vp)
{
    return array_getGeneric(cx, obj, receiver, SPECIALID_TO_JSID(sid), vp);
}

static JSBool
slowarray_addProperty(JSContext *cx, JSObject *obj, jsid id, Value *vp)
{
    jsuint index, length;

    if (!js_IdIsIndex(id, &index))
        return JS_TRUE;
    length = obj->getArrayLength();
    if (index >= length)
        obj->setArrayLength(cx, index + 1);
    return JS_TRUE;
}

static JSType
array_typeOf(JSContext *cx, JSObject *obj)
{
    return JSTYPE_OBJECT;
}

static JSBool
array_setGeneric(JSContext *cx, JSObject *obj, jsid id, Value *vp, JSBool strict)
{
    uint32 i;

    if (JSID_IS_ATOM(id, cx->runtime->atomState.lengthAtom))
        return array_length_setter(cx, obj, id, strict, vp);

    if (!obj->isDenseArray())
        return js_SetPropertyHelper(cx, obj, id, 0, vp, strict);

    do {
        if (!js_IdIsIndex(id, &i))
            break;
        if (js_PrototypeHasIndexedProperties(cx, obj))
            break;

        JSObject::EnsureDenseResult result = obj->ensureDenseArrayElements(cx, i, 1);
        if (result != JSObject::ED_OK) {
            if (result == JSObject::ED_FAILED)
                return false;
            JS_ASSERT(result == JSObject::ED_SPARSE);
            break;
        }

        if (i >= obj->getArrayLength())
            obj->setDenseArrayLength(i + 1);
        obj->setDenseArrayElementWithType(cx, i, *vp);
        return true;
    } while (false);

    if (!obj->makeDenseArraySlow(cx))
        return false;
    return js_SetPropertyHelper(cx, obj, id, 0, vp, strict);
}

static JSBool
array_setProperty(JSContext *cx, JSObject *obj, PropertyName *name, Value *vp, JSBool strict)
{
    return array_setGeneric(cx, obj, ATOM_TO_JSID(name), vp, strict);
}

static JSBool
array_setElement(JSContext *cx, JSObject *obj, uint32 index, Value *vp, JSBool strict)
{
    jsid id;
    if (!IndexToId(cx, index, &id))
        return false;

    if (!obj->isDenseArray())
        return js_SetPropertyHelper(cx, obj, id, 0, vp, strict);

    do {
        /*
         * UINT32_MAX is not an array index and must not affect the length
         * property, so specifically reject it.
         */
        if (index == UINT32_MAX)
            break;
        if (js_PrototypeHasIndexedProperties(cx, obj))
            break;

        JSObject::EnsureDenseResult result = obj->ensureDenseArrayElements(cx, index, 1);
        if (result != JSObject::ED_OK) {
            if (result == JSObject::ED_FAILED)
                return false;
            JS_ASSERT(result == JSObject::ED_SPARSE);
            break;
        }

        if (index >= obj->getArrayLength())
            obj->setDenseArrayLength(index + 1);
        obj->setDenseArrayElementWithType(cx, index, *vp);
        return true;
    } while (false);

    if (!obj->makeDenseArraySlow(cx))
        return false;
    return js_SetPropertyHelper(cx, obj, id, 0, vp, strict);
}

static JSBool
array_setSpecial(JSContext *cx, JSObject *obj, SpecialId sid, Value *vp, JSBool strict)
{
    return array_setGeneric(cx, obj, SPECIALID_TO_JSID(sid), vp, strict);
}

JSBool
js_PrototypeHasIndexedProperties(JSContext *cx, JSObject *obj)
{
    /*
     * Walk up the prototype chain and see if this indexed element already
     * exists. If we hit the end of the prototype chain, it's safe to set the
     * element on the original object.
     */
    while ((obj = obj->getProto()) != NULL) {
        /*
         * If the prototype is a non-native object (possibly a dense array), or
         * a native object (possibly a slow array) that has indexed properties,
         * return true.
         */
        if (!obj->isNative())
            return JS_TRUE;
        if (obj->isIndexed())
            return JS_TRUE;
    }
    return JS_FALSE;
}

static JSBool
array_defineGeneric(JSContext *cx, JSObject *obj, jsid id, const Value *value,
                    JSPropertyOp getter, StrictPropertyOp setter, uintN attrs)
{
    if (JSID_IS_ATOM(id, cx->runtime->atomState.lengthAtom))
        return JS_TRUE;

    if (!obj->isDenseArray())
        return js_DefineProperty(cx, obj, id, value, getter, setter, attrs);

    do {
        uint32 i = 0;       // init to shut GCC up
        bool isIndex = js_IdIsIndex(id, &i);
        if (!isIndex || attrs != JSPROP_ENUMERATE)
            break;

        JSObject::EnsureDenseResult result = obj->ensureDenseArrayElements(cx, i, 1);
        if (result != JSObject::ED_OK) {
            if (result == JSObject::ED_FAILED)
                return false;
            JS_ASSERT(result == JSObject::ED_SPARSE);
            break;
        }

        if (i >= obj->getArrayLength())
            obj->setDenseArrayLength(i + 1);
        obj->setDenseArrayElementWithType(cx, i, *value);
        return true;
    } while (false);

    if (!obj->makeDenseArraySlow(cx))
        return false;
    return js_DefineProperty(cx, obj, id, value, getter, setter, attrs);
}

static JSBool
array_defineProperty(JSContext *cx, JSObject *obj, PropertyName *name, const Value *value,
                     JSPropertyOp getter, StrictPropertyOp setter, uintN attrs)
{
    return array_defineGeneric(cx, obj, ATOM_TO_JSID(name), value, getter, setter, attrs);
}

namespace js {

/* non-static for direct definition of array elements within the engine */
JSBool
array_defineElement(JSContext *cx, JSObject *obj, uint32 index, const Value *value,
                    PropertyOp getter, StrictPropertyOp setter, uintN attrs)
{
    if (!obj->isDenseArray())
        return js_DefineElement(cx, obj, index, value, getter, setter, attrs);

    jsid id;
    if (!IndexToId(cx, index, &id))
        return false;

    do {
        /*
         * UINT32_MAX is not an array index and must not affect the length
         * property, so specifically reject it.
         */
        if (attrs != JSPROP_ENUMERATE || index == UINT32_MAX)
            break;

        JSObject::EnsureDenseResult result = obj->ensureDenseArrayElements(cx, index, 1);
        if (result != JSObject::ED_OK) {
            if (result == JSObject::ED_FAILED)
                return false;
            JS_ASSERT(result == JSObject::ED_SPARSE);
            break;
        }

        if (index >= obj->getArrayLength())
            obj->setDenseArrayLength(index + 1);
        obj->setDenseArrayElementWithType(cx, index, *value);
        return true;
    } while (false);

    if (!obj->makeDenseArraySlow(cx))
        return false;
    return js_DefineElement(cx, obj, index, value, getter, setter, attrs);
}

} // namespace js

static JSBool
array_defineSpecial(JSContext *cx, JSObject *obj, SpecialId sid, const Value *value,
                    PropertyOp getter, StrictPropertyOp setter, uintN attrs)
{
    return array_defineGeneric(cx, obj, SPECIALID_TO_JSID(sid), value, getter, setter, attrs);
}

static JSBool
array_getGenericAttributes(JSContext *cx, JSObject *obj, jsid id, uintN *attrsp)
{
    *attrsp = JSID_IS_ATOM(id, cx->runtime->atomState.lengthAtom)
        ? JSPROP_PERMANENT : JSPROP_ENUMERATE;
    return true;
}

static JSBool
array_getPropertyAttributes(JSContext *cx, JSObject *obj, PropertyName *name, uintN *attrsp)
{
    *attrsp = (name == cx->runtime->atomState.lengthAtom)
              ? JSPROP_PERMANENT
              : JSPROP_ENUMERATE;
    return true;
}

static JSBool
array_getElementAttributes(JSContext *cx, JSObject *obj, uint32 index, uintN *attrsp)
{
    *attrsp = JSPROP_ENUMERATE;
    return true;
}

static JSBool
array_getSpecialAttributes(JSContext *cx, JSObject *obj, SpecialId sid, uintN *attrsp)
{
    *attrsp = JSPROP_ENUMERATE;
    return true;
}

static JSBool
array_setGenericAttributes(JSContext *cx, JSObject *obj, jsid id, uintN *attrsp)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CANT_SET_ARRAY_ATTRS);
    return false;
}

static JSBool
array_setPropertyAttributes(JSContext *cx, JSObject *obj, PropertyName *name, uintN *attrsp)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CANT_SET_ARRAY_ATTRS);
    return false;
}

static JSBool
array_setElementAttributes(JSContext *cx, JSObject *obj, uint32 index, uintN *attrsp)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CANT_SET_ARRAY_ATTRS);
    return false;
}

static JSBool
array_setSpecialAttributes(JSContext *cx, JSObject *obj, SpecialId sid, uintN *attrsp)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CANT_SET_ARRAY_ATTRS);
    return false;
}

static JSBool
array_deleteGeneric(JSContext *cx, JSObject *obj, jsid id, Value *rval, JSBool strict)
{
    uint32 i;

    if (!obj->isDenseArray())
        return js_DeleteProperty(cx, obj, id, rval, strict);

    if (JSID_IS_ATOM(id, cx->runtime->atomState.lengthAtom)) {
        rval->setBoolean(false);
        return true;
    }

    if (js_IdIsIndex(id, &i) && i < obj->getDenseArrayInitializedLength()) {
        obj->markDenseArrayNotPacked(cx);
        obj->setDenseArrayElement(i, MagicValue(JS_ARRAY_HOLE));
    }

    if (!js_SuppressDeletedProperty(cx, obj, id))
        return false;

    rval->setBoolean(true);
    return true;
}

static JSBool
array_deleteProperty(JSContext *cx, JSObject *obj, PropertyName *name, Value *rval, JSBool strict)
{
    return array_deleteGeneric(cx, obj, ATOM_TO_JSID(name), rval, strict);
}

namespace js {

/* non-static for direct deletion of array elements within the engine */
JSBool
array_deleteElement(JSContext *cx, JSObject *obj, uint32 index, Value *rval, JSBool strict)
{
    if (!obj->isDenseArray())
        return js_DeleteElement(cx, obj, index, rval, strict);

    if (index < obj->getDenseArrayInitializedLength()) {
        obj->markDenseArrayNotPacked(cx);
        obj->setDenseArrayElement(index, MagicValue(JS_ARRAY_HOLE));
    }

    if (!js_SuppressDeletedElement(cx, obj, index))
        return false;

    rval->setBoolean(true);
    return true;
}

} // namespace js

static JSBool
array_deleteSpecial(JSContext *cx, JSObject *obj, SpecialId sid, Value *rval, JSBool strict)
{
    return array_deleteGeneric(cx, obj, SPECIALID_TO_JSID(sid), rval, strict);
}

static void
array_trace(JSTracer *trc, JSObject *obj)
{
    JS_ASSERT(obj->isDenseArray());

    uint32 initLength = obj->getDenseArrayInitializedLength();
    MarkValueRange(trc, initLength, obj->getDenseArrayElements(), "element");
}

static JSBool
array_fix(JSContext *cx, JSObject *obj, bool *success, AutoIdVector *props)
{
    JS_ASSERT(obj->isDenseArray());

    /*
     * We must slowify dense arrays; otherwise, we'd need to detect assignments to holes,
     * since that is effectively adding a new property to the array.
     */
    if (!obj->makeDenseArraySlow(cx) ||
        !GetPropertyNames(cx, obj, JSITER_HIDDEN | JSITER_OWNONLY, props))
        return false;

    *success = true;
    return true;
}

Class js::ArrayClass = {
    "Array",
    Class::NON_NATIVE | JSCLASS_HAS_PRIVATE | JSCLASS_HAS_CACHED_PROTO(JSProto_Array),
    JS_PropertyStub,         /* addProperty */
    JS_PropertyStub,         /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    NULL,
    NULL,           /* reserved0   */
    NULL,           /* checkAccess */
    NULL,           /* call        */
    NULL,           /* construct   */
    NULL,           /* xdrObject   */
    NULL,           /* hasInstance */
    array_trace,    /* trace       */
    JS_NULL_CLASS_EXT,
    {
        array_lookupGeneric,
        array_lookupProperty,
        array_lookupElement,
        array_lookupSpecial,
        array_defineGeneric,
        array_defineProperty,
        array_defineElement,
        array_defineSpecial,
        array_getGeneric,
        array_getProperty,
        array_getElement,
        NULL, /* getElementIfPresent, because this is hard for now for
                 slow arrays */
        array_getSpecial,
        array_setGeneric,
        array_setProperty,
        array_setElement,
        array_setSpecial,
        array_getGenericAttributes,
        array_getPropertyAttributes,
        array_getElementAttributes,
        array_getSpecialAttributes,
        array_setGenericAttributes,
        array_setPropertyAttributes,
        array_setElementAttributes,
        array_setSpecialAttributes,
        array_deleteGeneric,
        array_deleteProperty,
        array_deleteElement,
        array_deleteSpecial,
        NULL,       /* enumerate      */
        array_typeOf,
        array_fix,
        NULL,       /* thisObject     */
        NULL,       /* clear          */
    }
};

Class js::SlowArrayClass = {
    "Array",
    JSCLASS_HAS_PRIVATE |
    JSCLASS_HAS_CACHED_PROTO(JSProto_Array),
    slowarray_addProperty,
    JS_PropertyStub,         /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub
};

static bool
AddLengthProperty(JSContext *cx, JSObject *obj)
{
    const jsid lengthId = ATOM_TO_JSID(cx->runtime->atomState.lengthAtom);
    JS_ASSERT(!obj->nativeLookup(cx, lengthId));

    return obj->addProperty(cx, lengthId, array_length_getter, array_length_setter,
                            SHAPE_INVALID_SLOT, JSPROP_PERMANENT | JSPROP_SHARED, 0, 0);
}

/*
 * Convert an array object from fast-and-dense to slow-and-flexible.
 */
JSBool
JSObject::makeDenseArraySlow(JSContext *cx)
{
    JS_ASSERT(isDenseArray());

    MarkTypeObjectFlags(cx, this,
                        OBJECT_FLAG_NON_PACKED_ARRAY |
                        OBJECT_FLAG_NON_DENSE_ARRAY);
    markDenseArrayNotPacked(cx);

    /*
     * Save old map now, before calling InitScopeForObject. We'll have to undo
     * on error. This is gross, but a better way is not obvious. Note: the
     * exact contents of the array are not preserved on error.
     */
    js::Shape *oldMap = lastProp;

    /* Create a native scope. */
    gc::AllocKind kind = getAllocKind();
    if (!InitScopeForObject(cx, this, &SlowArrayClass, getProto()->getNewType(cx), kind))
        return false;

    backfillDenseArrayHoles(cx);

    uint32 arrayCapacity = getDenseArrayCapacity();
    uint32 arrayInitialized = getDenseArrayInitializedLength();

    /*
     * Adjust the slots to account for the different layout between dense
     * arrays and other objects. The slots must be dynamic, and the fixed slots
     * are now available for newly added properties.
     */
    if (denseArrayHasInlineSlots()) {
        if (!allocSlots(cx, numSlots())) {
            setMap(oldMap);
            return false;
        }
        JS_ASSERT(!denseArrayHasInlineSlots());
    }
    capacity = numFixedSlots() + arrayCapacity;
    clasp = &SlowArrayClass;

    /*
     * Root all values in the array during conversion, as SlowArrayClass only
     * protects up to its slot span.
     */
    AutoValueArray autoArray(cx, slots, arrayInitialized);

    /* The initialized length is used iff this is a dense array. */
    initializedLength = 0;
    JS_ASSERT(newType == NULL);

    /*
     * Begin with the length property to share more of the property tree.
     * The getter/setter here will directly access the object's private value.
     */
    if (!AddLengthProperty(cx, this)) {
        setMap(oldMap);
        capacity = arrayCapacity;
        initializedLength = arrayInitialized;
        clasp = &ArrayClass;
        return false;
    }

    /*
     * Create new properties pointing to existing elements. Pack the array to
     * remove holes, so that shapes use successive slots (as for other objects).
     */
    uint32 next = 0;
    for (uint32 i = 0; i < arrayCapacity; i++) {
        /* Dense array indexes can always fit in a jsid. */
        jsid id;
        JS_ALWAYS_TRUE(ValueToId(cx, Int32Value(i), &id));

        if (slots[i].isMagic(JS_ARRAY_HOLE))
            continue;

        setSlot(next, slots[i]);

        if (!addDataProperty(cx, id, next, JSPROP_ENUMERATE)) {
            setMap(oldMap);
            capacity = arrayCapacity;
            initializedLength = arrayInitialized;
            clasp = &ArrayClass;
            return false;
        }

        next++;
    }

    clearSlotRange(next, capacity - next);

    return true;
}

#if JS_HAS_TOSOURCE
class ArraySharpDetector
{
    JSContext *cx;
    jschar *chars;
    JSHashEntry *he;
    bool sharp;

  public:
    ArraySharpDetector(JSContext *cx)
      : cx(cx),
        chars(NULL),
        he(NULL),
        sharp(false)
    {}

    bool init(JSObject *obj) {
        he = js_EnterSharpObject(cx, obj, NULL, &chars);
        if (!he)
            return false;
        sharp = IS_SHARP(he);
        return true;
    }

    bool initiallySharp() const {
        JS_ASSERT_IF(sharp, hasSharpChars());
        return sharp;
    }

    void makeSharp() {
        MAKE_SHARP(he);
    }

    bool hasSharpChars() const {
        return chars != NULL;
    }

    jschar *takeSharpChars() {
        jschar *ret = chars;
        chars = NULL;
        return ret;
    }

    ~ArraySharpDetector() {
        if (chars)
            cx->free_(chars);
        if (he && !sharp)
            js_LeaveSharpObject(cx, NULL);
    }
};

static JSBool
array_toSource(JSContext *cx, uintN argc, Value *vp)
{
    JS_CHECK_RECURSION(cx, return false);

    CallArgs args = CallArgsFromVp(argc, vp);
    JSObject *obj = ToObject(cx, &args.thisv());
    if (!obj)
        return false;
    if (!obj->isArray())
        return HandleNonGenericMethodClassMismatch(cx, args, array_toSource, &ArrayClass);

    ArraySharpDetector detector(cx);
    if (!detector.init(obj))
        return false;

    StringBuffer sb(cx);

#if JS_HAS_SHARP_VARS
    if (detector.initiallySharp()) {
        jschar *chars = detector.takeSharpChars();
        sb.replaceRawBuffer(chars, js_strlen(chars));
        goto make_string;
    } else if (detector.hasSharpChars()) {
        detector.makeSharp();
        jschar *chars = detector.takeSharpChars();
        sb.replaceRawBuffer(chars, js_strlen(chars));
    }
#else
    if (detector.initiallySharp()) {
        if (!sb.append("[]"))
            return false;
        goto make_string;
    }
#endif

    if (!sb.append('['))
        return false;

    jsuint length;
    if (!js_GetLengthProperty(cx, obj, &length))
        return false;

    for (jsuint index = 0; index < length; index++) {
        JSBool hole;
        Value elt;
        if (!JS_CHECK_OPERATION_LIMIT(cx) ||
            !GetElement(cx, obj, index, &hole, &elt)) {
            return false;
        }

        /* Get element's character string. */
        JSString *str;
        if (hole) {
            str = cx->runtime->emptyString;
        } else {
            str = js_ValueToSource(cx, elt);
            if (!str)
                return false;
        }

        /* Append element to buffer. */
        if (!sb.append(str))
            return false;
        if (index + 1 != length) {
            if (!sb.append(", "))
                return false;
        } else if (hole) {
            if (!sb.append(','))
                return false;
        }
    }

    /* Finalize the buffer. */
    if (!sb.append(']'))
        return false;

  make_string:
    JSString *str = sb.finishString();
    if (!str)
        return false;

    args.rval().setString(str);
    return true;
}
#endif

class AutoArrayCycleDetector
{
    JSContext *cx;
    JSObject *obj;
    uint32 genBefore;
    BusyArraysSet::AddPtr hashPointer;
    bool cycle;
    JS_DECL_USE_GUARD_OBJECT_NOTIFIER

  public:
    AutoArrayCycleDetector(JSContext *cx, JSObject *obj JS_GUARD_OBJECT_NOTIFIER_PARAM)
      : cx(cx),
        obj(obj),
        cycle(true)
    {
        JS_GUARD_OBJECT_NOTIFIER_INIT;
    }

    bool init()
    {
        BusyArraysSet &set = cx->busyArrays;
        hashPointer = set.lookupForAdd(obj);
        if (!hashPointer) {
            if (!set.add(hashPointer, obj))
                return false;
            cycle = false;
            genBefore = set.generation();
        }
        return true;
    }

    ~AutoArrayCycleDetector()
    {
        if (!cycle) {
            if (genBefore == cx->busyArrays.generation())
                cx->busyArrays.remove(hashPointer);
            else
                cx->busyArrays.remove(obj);
        }
    }

    bool foundCycle() { return cycle; }

  protected:
};

static JSBool
array_toString_sub(JSContext *cx, JSObject *obj, JSBool locale,
                   JSString *sepstr, CallArgs &args)
{
    static const jschar comma = ',';
    const jschar *sep;
    size_t seplen;
    if (sepstr) {
        seplen = sepstr->length();
        sep = sepstr->getChars(cx);
        if (!sep)
            return false;
    } else {
        sep = &comma;
        seplen = 1;
    }

    AutoArrayCycleDetector detector(cx, obj);
    if (!detector.init())
        return false;

    if (detector.foundCycle()) {
        args.rval().setString(cx->runtime->atomState.emptyAtom);
        return true;
    }

    jsuint length;
    if (!js_GetLengthProperty(cx, obj, &length))
        return false;

    StringBuffer sb(cx);

    if (!locale && !seplen && obj->isDenseArray() && !js_PrototypeHasIndexedProperties(cx, obj)) {
        /* Elements beyond the initialized length are 'undefined' and thus can be ignored. */
        const Value *beg = obj->getDenseArrayElements();
        const Value *end = beg + Min(length, obj->getDenseArrayInitializedLength());
        for (const Value *vp = beg; vp != end; ++vp) {
            if (!JS_CHECK_OPERATION_LIMIT(cx))
                return false;

            if (!vp->isMagic(JS_ARRAY_HOLE) && !vp->isNullOrUndefined()) {
                if (!ValueToStringBuffer(cx, *vp, sb))
                    return false;
            }
        }
    } else {
        for (jsuint index = 0; index < length; index++) {
            if (!JS_CHECK_OPERATION_LIMIT(cx))
                return false;

            JSBool hole;
            Value elt;
            if (!GetElement(cx, obj, index, &hole, &elt))
                return false;

            if (!hole && !elt.isNullOrUndefined()) {
                if (locale) {
                    JSObject *robj = ToObject(cx, &elt);
                    if (!robj)
                        return false;
                    jsid id = ATOM_TO_JSID(cx->runtime->atomState.toLocaleStringAtom);
                    if (!robj->callMethod(cx, id, 0, NULL, &elt))
                        return false;
                }
                if (!ValueToStringBuffer(cx, elt, sb))
                    return false;
            }

            if (index + 1 != length) {
                if (!sb.append(sep, seplen))
                    return false;
            }
        }
    }

    JSString *str = sb.finishString();
    if (!str)
        return false;
    args.rval().setString(str);
    return true;
}

/* ES5 15.4.4.2. NB: The algorithm here differs from the one in ES3. */
static JSBool
array_toString(JSContext *cx, uintN argc, Value *vp)
{
    JS_CHECK_RECURSION(cx, return false);

    CallArgs args = CallArgsFromVp(argc, vp);
    JSObject *obj = ToObject(cx, &args.thisv());
    if (!obj)
        return false;

    Value join = args.calleev();
    if (!obj->getProperty(cx, cx->runtime->atomState.joinAtom, &join))
        return false;

    if (!js_IsCallable(join)) {
        JSString *str = obj_toStringHelper(cx, obj);
        if (!str)
            return false;
        args.rval().setString(str);
        return true;
    }

    LeaveTrace(cx);
    InvokeArgsGuard ag;
    if (!cx->stack.pushInvokeArgs(cx, 0, &ag))
        return false;

    ag.calleev() = join;
    ag.thisv().setObject(*obj);

    /* Do the call. */
    if (!Invoke(cx, ag))
        return false;
    args.rval() = ag.rval();
    return true;
}

static JSBool
array_toLocaleString(JSContext *cx, uintN argc, Value *vp)
{
    JS_CHECK_RECURSION(cx, return false);

    CallArgs args = CallArgsFromVp(argc, vp);
    JSObject *obj = ToObject(cx, &args.thisv());
    if (!obj)
        return false;

    /*
     *  Passing comma here as the separator. Need a way to get a
     *  locale-specific version.
     */
    return array_toString_sub(cx, obj, JS_TRUE, NULL, args);
}

static inline bool
InitArrayTypes(JSContext *cx, TypeObject *type, const Value *vector, unsigned count)
{
    if (cx->typeInferenceEnabled() && !type->unknownProperties()) {
        AutoEnterTypeInference enter(cx);

        TypeSet *types = type->getProperty(cx, JSID_VOID, true);
        if (!types)
            return false;

        for (unsigned i = 0; i < count; i++) {
            if (vector[i].isMagic(JS_ARRAY_HOLE))
                continue;
            Type valtype = GetValueType(cx, vector[i]);
            types->addType(cx, valtype);
        }
    }
    return true;
}

static JSBool
InitArrayElements(JSContext *cx, JSObject *obj, jsuint start, jsuint count, const Value *vector, bool updateTypes)
{
    JS_ASSERT(count <= MAX_ARRAY_INDEX);

    if (count == 0)
        return JS_TRUE;

    if (updateTypes && !InitArrayTypes(cx, obj->getType(cx), vector, count))
        return JS_FALSE;

    /*
     * Optimize for dense arrays so long as adding the given set of elements
     * wouldn't otherwise make the array slow.
     */
    do {
        if (!obj->isDenseArray())
            break;
        if (js_PrototypeHasIndexedProperties(cx, obj))
            break;

        JSObject::EnsureDenseResult result = obj->ensureDenseArrayElements(cx, start, count);
        if (result != JSObject::ED_OK) {
            if (result == JSObject::ED_FAILED)
                return false;
            JS_ASSERT(result == JSObject::ED_SPARSE);
            break;
        }
        jsuint newlen = start + count;
        if (newlen > obj->getArrayLength())
            obj->setDenseArrayLength(newlen);

        JS_ASSERT(count < UINT32_MAX / sizeof(Value));
        obj->copyDenseArrayElements(start, vector, count);
        JS_ASSERT_IF(count != 0, !obj->getDenseArrayElement(newlen - 1).isMagic(JS_ARRAY_HOLE));
        return true;
    } while (false);

    const Value* end = vector + count;
    while (vector < end && start <= MAX_ARRAY_INDEX) {
        if (!JS_CHECK_OPERATION_LIMIT(cx) ||
            !SetArrayElement(cx, obj, start++, *vector++)) {
            return JS_FALSE;
        }
    }

    if (vector == end)
        return JS_TRUE;

    /* Finish out any remaining elements past the max array index. */
    if (obj->isDenseArray() && !obj->makeDenseArraySlow(cx))
        return JS_FALSE;

    JS_ASSERT(start == MAX_ARRAY_INDEX + 1);
    AutoValueRooter tvr(cx);
    AutoIdRooter idr(cx);
    Value idval = DoubleValue(MAX_ARRAY_INDEX + 1);
    do {
        *tvr.addr() = *vector++;
        if (!js_ValueToStringId(cx, idval, idr.addr()) ||
            !obj->setGeneric(cx, idr.id(), tvr.addr(), true)) {
            return JS_FALSE;
        }
        idval.getDoubleRef() += 1;
    } while (vector != end);

    return JS_TRUE;
}

#if 0
static JSBool
InitArrayObject(JSContext *cx, JSObject *obj, jsuint length, const Value *vector)
{
    JS_ASSERT(obj->isArray());

    JS_ASSERT(obj->isDenseArray());
    obj->setArrayLength(cx, length);
    if (!vector || !length)
        return true;

    if (!InitArrayTypes(cx, obj->getType(cx), vector, length))
        return false;

    /* Avoid ensureDenseArrayElements to skip sparse array checks there. */
    if (!obj->ensureSlots(cx, length))
        return false;

    if (cx->typeInferenceEnabled())
        obj->setDenseArrayInitializedLength(length);
    else
        obj->backfillDenseArrayHoles(cx);

    bool hole = false;
    for (jsuint i = 0; i < length; i++) {
        obj->setDenseArrayElement(i, vector[i]);
        hole |= vector[i].isMagic(JS_ARRAY_HOLE);
    }
    if (hole)
        obj->markDenseArrayNotPacked(cx);

    return true;
}
#endif

/*
 * Perl-inspired join, reverse, and sort.
 */
static JSBool
array_join(JSContext *cx, uintN argc, Value *vp)
{
    JS_CHECK_RECURSION(cx, return false);

    CallArgs args = CallArgsFromVp(argc, vp);
    JSString *str;
    if (args.length() == 0 || args[0].isUndefined()) {
        str = NULL;
    } else {
        str = js_ValueToString(cx, args[0]);
        if (!str)
            return JS_FALSE;
        args[0].setString(str);
    }
    JSObject *obj = ToObject(cx, &args.thisv());
    if (!obj)
        return false;
    return array_toString_sub(cx, obj, JS_FALSE, str, args);
}

static JSBool
array_reverse(JSContext *cx, uintN argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JSObject *obj = ToObject(cx, &args.thisv());
    if (!obj)
        return false;

    jsuint len;
    if (!js_GetLengthProperty(cx, obj, &len))
        return false;

    do {
        if (!obj->isDenseArray())
            break;
        if (js_PrototypeHasIndexedProperties(cx, obj))
            break;

        /* An empty array or an array with no elements is already reversed. */
        if (len == 0 || obj->getDenseArrayCapacity() == 0) {
            args.rval().setObject(*obj);
            return true;
        }

        /*
         * It's actually surprisingly complicated to reverse an array due to the
         * orthogonality of array length and array capacity while handling
         * leading and trailing holes correctly.  Reversing seems less likely to
         * be a common operation than other array mass-mutation methods, so for
         * now just take a probably-small memory hit (in the absence of too many
         * holes in the array at its start) and ensure that the capacity is
         * sufficient to hold all the elements in the array if it were full.
         */
        JSObject::EnsureDenseResult result = obj->ensureDenseArrayElements(cx, len, 0);
        if (result != JSObject::ED_OK) {
            if (result == JSObject::ED_FAILED)
                return false;
            JS_ASSERT(result == JSObject::ED_SPARSE);
            break;
        }

        /* Fill out the array's initialized length to its proper length. */
        obj->ensureDenseArrayInitializedLength(cx, len, 0);

        uint32 lo = 0, hi = len - 1;
        for (; lo < hi; lo++, hi--) {
            Value origlo = obj->getDenseArrayElement(lo);
            Value orighi = obj->getDenseArrayElement(hi);
            obj->setDenseArrayElement(lo, orighi);
            if (orighi.isMagic(JS_ARRAY_HOLE) &&
                !js_SuppressDeletedProperty(cx, obj, INT_TO_JSID(lo))) {
                return false;
            }
            obj->setDenseArrayElement(hi, origlo);
            if (origlo.isMagic(JS_ARRAY_HOLE) &&
                !js_SuppressDeletedProperty(cx, obj, INT_TO_JSID(hi))) {
                return false;
            }
        }

        /*
         * Per ECMA-262, don't update the length of the array, even if the new
         * array has trailing holes (and thus the original array began with
         * holes).
         */
        args.rval().setObject(*obj);
        return true;
    } while (false);

    Value lowval, hival;
    for (jsuint i = 0, half = len / 2; i < half; i++) {
        JSBool hole, hole2;
        if (!JS_CHECK_OPERATION_LIMIT(cx) ||
            !GetElement(cx, obj, i, &hole, &lowval) ||
            !GetElement(cx, obj, len - i - 1, &hole2, &hival) ||
            !SetOrDeleteArrayElement(cx, obj, len - i - 1, hole, lowval) ||
            !SetOrDeleteArrayElement(cx, obj, i, hole2, hival)) {
            return false;
        }
    }
    args.rval().setObject(*obj);
    return true;
}

typedef struct MSortArgs {
    size_t       elsize;
    JSComparator cmp;
    void         *arg;
    JSBool       isValue;
} MSortArgs;

/* Helper function for js_MergeSort. */
static JSBool
MergeArrays(MSortArgs *msa, void *src, void *dest, size_t run1, size_t run2)
{
    void *arg, *a, *b, *c;
    size_t elsize, runtotal;
    int cmp_result;
    JSComparator cmp;
    JSBool isValue;

    runtotal = run1 + run2;

    elsize = msa->elsize;
    cmp = msa->cmp;
    arg = msa->arg;
    isValue = msa->isValue;

#define CALL_CMP(a, b) \
    if (!cmp(arg, (a), (b), &cmp_result)) return JS_FALSE;

    /* Copy runs already in sorted order. */
    b = (char *)src + run1 * elsize;
    a = (char *)b - elsize;
    CALL_CMP(a, b);
    if (cmp_result <= 0) {
        memcpy(dest, src, runtotal * elsize);
        return JS_TRUE;
    }

#define COPY_ONE(p,q,n) \
    (isValue ? (void)(*(Value*)p = *(Value*)q) : (void)memcpy(p, q, n))

    a = src;
    c = dest;
    for (; runtotal != 0; runtotal--) {
        JSBool from_a = run2 == 0;
        if (!from_a && run1 != 0) {
            CALL_CMP(a,b);
            from_a = cmp_result <= 0;
        }

        if (from_a) {
            COPY_ONE(c, a, elsize);
            run1--;
            a = (char *)a + elsize;
        } else {
            COPY_ONE(c, b, elsize);
            run2--;
            b = (char *)b + elsize;
        }
        c = (char *)c + elsize;
    }
#undef COPY_ONE
#undef CALL_CMP

    return JS_TRUE;
}

/*
 * This sort is stable, i.e. sequence of equal elements is preserved.
 * See also bug #224128.
 */
bool
js_MergeSort(void *src, size_t nel, size_t elsize,
             JSComparator cmp, void *arg, void *tmp,
             JSMergeSortElemType elemType)
{
    void *swap, *vec1, *vec2;
    MSortArgs msa;
    size_t i, j, lo, hi, run;
    int cmp_result;

    JS_ASSERT_IF(JS_SORTING_VALUES, elsize == sizeof(Value));
    bool isValue = elemType == JS_SORTING_VALUES;

    /* Avoid memcpy overhead for word-sized and word-aligned elements. */
#define COPY_ONE(p,q,n) \
    (isValue ? (void)(*(Value*)p = *(Value*)q) : (void)memcpy(p, q, n))
#define CALL_CMP(a, b) \
    if (!cmp(arg, (a), (b), &cmp_result)) return JS_FALSE;
#define INS_SORT_INT 4

    /*
     * Apply insertion sort to small chunks to reduce the number of merge
     * passes needed.
     */
    for (lo = 0; lo < nel; lo += INS_SORT_INT) {
        hi = lo + INS_SORT_INT;
        if (hi >= nel)
            hi = nel;
        for (i = lo + 1; i < hi; i++) {
            vec1 = (char *)src + i * elsize;
            vec2 = (char *)vec1 - elsize;
            for (j = i; j > lo; j--) {
                CALL_CMP(vec2, vec1);
                /* "<=" instead of "<" insures the sort is stable */
                if (cmp_result <= 0) {
                    break;
                }

                /* Swap elements, using "tmp" as tmp storage */
                COPY_ONE(tmp, vec2, elsize);
                COPY_ONE(vec2, vec1, elsize);
                COPY_ONE(vec1, tmp, elsize);
                vec1 = vec2;
                vec2 = (char *)vec1 - elsize;
            }
        }
    }
#undef CALL_CMP
#undef COPY_ONE

    msa.elsize = elsize;
    msa.cmp = cmp;
    msa.arg = arg;
    msa.isValue = isValue;

    vec1 = src;
    vec2 = tmp;
    for (run = INS_SORT_INT; run < nel; run *= 2) {
        for (lo = 0; lo < nel; lo += 2 * run) {
            hi = lo + run;
            if (hi >= nel) {
                memcpy((char *)vec2 + lo * elsize, (char *)vec1 + lo * elsize,
                       (nel - lo) * elsize);
                break;
            }
            if (!MergeArrays(&msa, (char *)vec1 + lo * elsize,
                             (char *)vec2 + lo * elsize, run,
                             hi + run > nel ? nel - hi : run)) {
                return JS_FALSE;
            }
        }
        swap = vec1;
        vec1 = vec2;
        vec2 = swap;
    }
    if (src != vec1)
        memcpy(src, tmp, nel * elsize);

    return JS_TRUE;
}

struct CompareArgs
{
    JSContext          *context;
    InvokeArgsGuard    args;
    Value              fval;

    CompareArgs(JSContext *cx, Value fval)
      : context(cx), fval(fval)
    {}
};

static JS_REQUIRES_STACK JSBool
sort_compare(void *arg, const void *a, const void *b, int *result)
{
    const Value *av = (const Value *)a, *bv = (const Value *)b;
    CompareArgs *ca = (CompareArgs *) arg;
    JSContext *cx = ca->context;

    /*
     * array_sort deals with holes and undefs on its own and they should not
     * come here.
     */
    JS_ASSERT(!av->isMagic() && !av->isUndefined());
    JS_ASSERT(!av->isMagic() && !bv->isUndefined());

    if (!JS_CHECK_OPERATION_LIMIT(cx))
        return JS_FALSE;

    InvokeArgsGuard &ag = ca->args;
    if (!ag.pushed() && !cx->stack.pushInvokeArgs(cx, 2, &ag))
        return JS_FALSE;
        
    ag.calleeHasBeenReset();
    ag.calleev() = ca->fval;
    ag.thisv() = UndefinedValue();
    ag[0] = *av;
    ag[1] = *bv;

    if (!Invoke(cx, ag))
        return JS_FALSE;

    jsdouble cmp;
    if (!ToNumber(cx, ag.rval(), &cmp))
        return JS_FALSE;

    /* Clamp cmp to -1, 0, 1. */
    *result = 0;
    if (!JSDOUBLE_IS_NaN(cmp) && cmp != 0)
        *result = cmp > 0 ? 1 : -1;

    /*
     * XXX else report some kind of error here?  ECMA talks about 'consistent
     * compare functions' that don't return NaN, but is silent about what the
     * result should be.  So we currently ignore it.
     */

    return JS_TRUE;
}

typedef JSBool (JS_REQUIRES_STACK *JSRedComparator)(void*, const void*,
                                                    const void*, int *);

static inline JS_IGNORE_STACK JSComparator
comparator_stack_cast(JSRedComparator func)
{
    return func;
}

static int
sort_compare_strings(void *arg, const void *a, const void *b, int *result)
{
    JSContext *cx = (JSContext *)arg;
    JSString *astr = ((const Value *)a)->toString();
    JSString *bstr = ((const Value *)b)->toString();
    return JS_CHECK_OPERATION_LIMIT(cx) && CompareStrings(cx, astr, bstr, result);
}

JSBool
js::array_sort(JSContext *cx, uintN argc, Value *vp)
{
    jsuint len, newlen, i, undefs;
    size_t elemsize;
    JSString *str;
    
    CallArgs args = CallArgsFromVp(argc, vp);
    Value fval;
    if (args.length() > 0 && !args[0].isUndefined()) {
        if (args[0].isPrimitive()) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_SORT_ARG);
            return false;
        }
        fval = args[0];     /* non-default compare function */
    } else {
        fval.setNull();
    }

    JSObject *obj = ToObject(cx, &args.thisv());
    if (!obj)
        return false;
    if (!js_GetLengthProperty(cx, obj, &len))
        return false;
    if (len == 0) {
        args.rval().setObject(*obj);
        return true;
    }

    /*
     * We need a temporary array of 2 * len Value to hold the array elements
     * and the scratch space for merge sort. Check that its size does not
     * overflow size_t, which would allow for indexing beyond the end of the
     * malloc'd vector.
     */
#if JS_BITS_PER_WORD == 32
    if (size_t(len) > size_t(-1) / (2 * sizeof(Value))) {
        js_ReportAllocationOverflow(cx);
        return false;
    }
#endif

    /*
     * Initialize vec as a root. We will clear elements of vec one by
     * one while increasing the rooted amount of vec when we know that the
     * property at the corresponding index exists and its value must be rooted.
     *
     * In this way when sorting a huge mostly sparse array we will not
     * access the tail of vec corresponding to properties that do not
     * exist, allowing OS to avoiding committing RAM. See bug 330812.
     */
    {
        Value *vec = (Value *) cx->malloc_(2 * size_t(len) * sizeof(Value));
        if (!vec)
            return false;

        DEFINE_LOCAL_CLASS_OF_STATIC_FUNCTION(AutoFreeVector) {
            JSContext *const cx;
            Value *&vec;
           public:
            AutoFreeVector(JSContext *cx, Value *&vec) : cx(cx), vec(vec) { }
            ~AutoFreeVector() {
                cx->free_(vec);
            }
        } free_(cx, vec);

        AutoArrayRooter tvr(cx, 0, vec);

        /*
         * By ECMA 262, 15.4.4.11, a property that does not exist (which we
         * call a "hole") is always greater than an existing property with
         * value undefined and that is always greater than any other property.
         * Thus to sort holes and undefs we simply count them, sort the rest
         * of elements, append undefs after them and then make holes after
         * undefs.
         */
        undefs = 0;
        newlen = 0;
        bool allStrings = true;
        for (i = 0; i < len; i++) {
            if (!JS_CHECK_OPERATION_LIMIT(cx))
                return false;

            /* Clear vec[newlen] before including it in the rooted set. */
            JSBool hole;
            vec[newlen].setNull();
            tvr.changeLength(newlen + 1);
            if (!GetElement(cx, obj, i, &hole, &vec[newlen]))
                return false;

            if (hole)
                continue;

            if (vec[newlen].isUndefined()) {
                ++undefs;
                continue;
            }

            allStrings = allStrings && vec[newlen].isString();

            ++newlen;
        }

        if (newlen == 0) {
            args.rval().setObject(*obj);
            return true; /* The array has only holes and undefs. */
        }

        /*
         * The first newlen elements of vec are copied from the array object
         * (above). The remaining newlen positions are used as GC-rooted scratch
         * space for mergesort. We must clear the space before including it to
         * the root set covered by tvr.count.
         */
        Value *mergesort_tmp = vec + newlen;
        MakeRangeGCSafe(mergesort_tmp, newlen);
        tvr.changeLength(newlen * 2);

        /* Here len == 2 * (newlen + undefs + number_of_holes). */
        if (fval.isNull()) {
            /*
             * Sort using the default comparator converting all elements to
             * strings.
             */
            if (allStrings) {
                elemsize = sizeof(Value);
            } else {
                /*
                 * To avoid string conversion on each compare we do it only once
                 * prior to sorting. But we also need the space for the original
                 * values to recover the sorting result. To reuse
                 * sort_compare_strings we move the original values to the odd
                 * indexes in vec, put the string conversion results in the even
                 * indexes and pass 2 * sizeof(Value) as an element size to the
                 * sorting function. In this way sort_compare_strings will only
                 * see the string values when it casts the compare arguments as
                 * pointers to Value.
                 *
                 * This requires doubling the temporary storage including the
                 * scratch space for the merge sort. Since vec already contains
                 * the rooted scratch space for newlen elements at the tail, we
                 * can use it to rearrange and convert to strings first and try
                 * realloc only when we know that we successfully converted all
                 * the elements.
                 */
#if JS_BITS_PER_WORD == 32
                if (size_t(newlen) > size_t(-1) / (4 * sizeof(Value))) {
                    js_ReportAllocationOverflow(cx);
                    return false;
                }
#endif

                /*
                 * Rearrange and string-convert the elements of the vector from
                 * the tail here and, after sorting, move the results back
                 * starting from the start to prevent overwrite the existing
                 * elements.
                 */
                i = newlen;
                do {
                    --i;
                    if (!JS_CHECK_OPERATION_LIMIT(cx))
                        return false;
                    const Value &v = vec[i];
                    str = js_ValueToString(cx, v);
                    if (!str)
                        return false;
                    // Copying v must come first, because the following line overwrites v
                    // when i == 0.
                    vec[2 * i + 1] = v;
                    vec[2 * i].setString(str);
                } while (i != 0);

                JS_ASSERT(tvr.array == vec);
                vec = (Value *) cx->realloc_(vec, 4 * size_t(newlen) * sizeof(Value));
                if (!vec) {
                    vec = tvr.array;  /* N.B. AutoFreeVector */
                    return false;
                }
                mergesort_tmp = vec + 2 * newlen;
                MakeRangeGCSafe(mergesort_tmp, 2 * newlen);
                tvr.changeArray(vec, newlen * 4);
                elemsize = 2 * sizeof(Value);
            }
            if (!js_MergeSort(vec, size_t(newlen), elemsize,
                              sort_compare_strings, cx, mergesort_tmp,
                              JS_SORTING_GENERIC)) {
                return false;
            }
            if (!allStrings) {
                /*
                 * We want to make the following loop fast and to unroot the
                 * cached results of toString invocations before the operation
                 * callback has a chance to run the GC. For this reason we do
                 * not call JS_CHECK_OPERATION_LIMIT in the loop.
                 */
                i = 0;
                do {
                    vec[i] = vec[2 * i + 1];
                } while (++i != newlen);
            }
        } else {
            CompareArgs ca(cx, fval);
            if (!js_MergeSort(vec, size_t(newlen), sizeof(Value),
                              comparator_stack_cast(sort_compare),
                              &ca, mergesort_tmp,
                              JS_SORTING_VALUES)) {
                return false;
            }
        }

        /*
         * We no longer need to root the scratch space for the merge sort, so
         * unroot it now to make the job of a potential GC under
         * InitArrayElements easier.
         */
        tvr.changeLength(newlen);
        if (!InitArrayElements(cx, obj, 0, newlen, vec, false))
            return false;
    }

    /* Set undefs that sorted after the rest of elements. */
    while (undefs != 0) {
        --undefs;
        if (!JS_CHECK_OPERATION_LIMIT(cx) ||
            !SetArrayElement(cx, obj, newlen++, UndefinedValue())) {
            return false;
        }
    }

    /* Re-create any holes that sorted to the end of the array. */
    while (len > newlen) {
        if (!JS_CHECK_OPERATION_LIMIT(cx) || DeleteArrayElement(cx, obj, --len, true) < 0)
            return false;
    }
    args.rval().setObject(*obj);
    return true;
}

/*
 * Perl-inspired push, pop, shift, unshift, and splice methods.
 */
static bool
array_push_slowly(JSContext *cx, JSObject *obj, CallArgs &args)
{
    jsuint length;

    if (!js_GetLengthProperty(cx, obj, &length))
        return false;
    if (!InitArrayElements(cx, obj, length, args.length(), args.array(), true))
        return false;

    /* Per ECMA-262, return the new array length. */
    jsdouble newlength = length + jsdouble(args.length());
    args.rval().setNumber(newlength);
    return js_SetLengthProperty(cx, obj, newlength);
}

static bool
array_push1_dense(JSContext* cx, JSObject* obj, CallArgs &args)
{
    JS_ASSERT(args.length() == 1);

    uint32 length = obj->getArrayLength();
    JSObject::EnsureDenseResult result = obj->ensureDenseArrayElements(cx, length, 1);
    if (result != JSObject::ED_OK) {
        if (result == JSObject::ED_FAILED)
            return false;
        JS_ASSERT(result == JSObject::ED_SPARSE);
        if (!obj->makeDenseArraySlow(cx))
            return false;
        return array_push_slowly(cx, obj, args);
    }

    obj->setDenseArrayLength(length + 1);
    obj->setDenseArrayElementWithType(cx, length, args[0]);
    args.rval().setNumber(obj->getArrayLength());
    return true;
}

JS_ALWAYS_INLINE JSBool
NewbornArrayPushImpl(JSContext *cx, JSObject *obj, const Value &v)
{
    JS_ASSERT(!v.isMagic());

    uint32 length = obj->getArrayLength();
    if (obj->isSlowArray()) {
        /* This can happen in one evil case. See bug 630377. */
        jsid id;
        return IndexToId(cx, length, &id) &&
               js_DefineProperty(cx, obj, id, &v, NULL, NULL, JSPROP_ENUMERATE);
    }

    JS_ASSERT(obj->isDenseArray());
    JS_ASSERT(length <= obj->getDenseArrayCapacity());

    if (length == obj->getDenseArrayCapacity() && !obj->ensureSlots(cx, length + 1))
        return false;

    if (cx->typeInferenceEnabled())
        obj->setDenseArrayInitializedLength(length + 1);
    obj->setDenseArrayLength(length + 1);
    obj->setDenseArrayElementWithType(cx, length, v);
    return true;
}

JSBool
js_NewbornArrayPush(JSContext *cx, JSObject *obj, const Value &vp)
{
    return NewbornArrayPushImpl(cx, obj, vp);
}

#ifdef JS_TRACER
JSBool JS_FASTCALL
js_NewbornArrayPush_tn(JSContext *cx, JSObject *obj, ValueArgType v)
{
    TraceMonitor *tm = JS_TRACE_MONITOR_ON_TRACE(cx);

    if (!NewbornArrayPushImpl(cx, obj, ValueArgToConstRef(v))) {
        SetBuiltinError(tm);
        return JS_FALSE;
    }

    return WasBuiltinSuccessful(tm);
}
JS_DEFINE_CALLINFO_3(extern, BOOL_FAIL, js_NewbornArrayPush_tn, CONTEXT, OBJECT,
                     VALUE, 0, nanojit::ACCSET_STORE_ANY)
#endif

JSBool
js::array_push(JSContext *cx, uintN argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JSObject *obj = ToObject(cx, &args.thisv());
    if (!obj)
        return false;

    /* Insist on one argument and obj of the expected class. */
    if (args.length() != 1 || !obj->isDenseArray())
        return array_push_slowly(cx, obj, args);

    return array_push1_dense(cx, obj, args);
}

static JSBool
array_pop_slowly(JSContext *cx, JSObject* obj, CallArgs &args)
{
    jsuint index;
    if (!js_GetLengthProperty(cx, obj, &index))
        return false;

    if (index == 0) {
        args.rval().setUndefined();
        return js_SetLengthProperty(cx, obj, index);
    }

    index--;

    JSBool hole;
    Value elt;
    if (!GetElement(cx, obj, index, &hole, &elt))
        return false;

    if (!hole && DeleteArrayElement(cx, obj, index, true) < 0)
        return false;

    args.rval() = elt;
    return js_SetLengthProperty(cx, obj, index);
}

static JSBool
array_pop_dense(JSContext *cx, JSObject* obj, CallArgs &args)
{
    jsuint index = obj->getArrayLength();
    if (index == 0) {
        args.rval().setUndefined();
        return JS_TRUE;
    }

    index--;
    
    JSBool hole;
    Value elt;
    if (!GetElement(cx, obj, index, &hole, &elt))
        return JS_FALSE;

    if (!hole && DeleteArrayElement(cx, obj, index, true) < 0)
        return JS_FALSE;
    if (cx->typeInferenceEnabled() && obj->getDenseArrayInitializedLength() > index)
        obj->setDenseArrayInitializedLength(index);

    obj->setArrayLength(cx, index);

    args.rval() = elt;
    return JS_TRUE;
}

JSBool
js::array_pop(JSContext *cx, uintN argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JSObject *obj = ToObject(cx, &args.thisv());
    if (!obj)
        return false;
    if (obj->isDenseArray())
        return array_pop_dense(cx, obj, args);
    return array_pop_slowly(cx, obj, args);
}

#ifdef JS_METHODJIT
void JS_FASTCALL
mjit::stubs::ArrayShift(VMFrame &f)
{
    JSObject *obj = &f.regs.sp[-1].toObject();
    JS_ASSERT(obj->isDenseArray());
    JS_ASSERT(!js_PrototypeHasIndexedProperties(f.cx, obj));

    /*
     * At this point the length and initialized length have already been
     * decremented and the result fetched, so just shift the array elements
     * themselves.
     */
    uint32 initlen = obj->getDenseArrayInitializedLength();
    obj->moveDenseArrayElements(0, 1, initlen);
}
#endif /* JS_METHODJIT */

JSBool
js::array_shift(JSContext *cx, uintN argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JSObject *obj = ToObject(cx, &args.thisv());
    if (!obj)
        return JS_FALSE;

    jsuint length;
    if (!js_GetLengthProperty(cx, obj, &length))
        return JS_FALSE;

    if (length == 0) {
        args.rval().setUndefined();
    } else {
        length--;

        if (obj->isDenseArray() && !js_PrototypeHasIndexedProperties(cx, obj) &&
            length < obj->getDenseArrayCapacity() &&
            0 < obj->getDenseArrayInitializedLength()) {
            args.rval() = obj->getDenseArrayElement(0);
            if (args.rval().isMagic(JS_ARRAY_HOLE))
                args.rval().setUndefined();
            obj->moveDenseArrayElements(0, 1, length);
            if (cx->typeInferenceEnabled())
                obj->setDenseArrayInitializedLength(obj->getDenseArrayInitializedLength() - 1);
            else
                obj->setDenseArrayElement(length, MagicValue(JS_ARRAY_HOLE));
            obj->setArrayLength(cx, length);
            if (!js_SuppressDeletedProperty(cx, obj, INT_TO_JSID(length)))
                return JS_FALSE;
            return JS_TRUE;
        }

        JSBool hole;
        if (!GetElement(cx, obj, 0u, &hole, &args.rval()))
            return JS_FALSE;

        /* Slide down the array above the first element. */
        AutoValueRooter tvr(cx);
        for (jsuint i = 0; i < length; i++) {
            if (!JS_CHECK_OPERATION_LIMIT(cx) ||
                !GetElement(cx, obj, i + 1, &hole, tvr.addr()) ||
                !SetOrDeleteArrayElement(cx, obj, i, hole, tvr.value())) {
                return JS_FALSE;
            }
        }

        /* Delete the only or last element when it exists. */
        if (!hole && DeleteArrayElement(cx, obj, length, true) < 0)
            return JS_FALSE;
    }
    return js_SetLengthProperty(cx, obj, length);
}

static JSBool
array_unshift(JSContext *cx, uintN argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JSObject *obj = ToObject(cx, &args.thisv());
    if (!obj)
        return false;

    jsuint length;
    if (!js_GetLengthProperty(cx, obj, &length))
        return JS_FALSE;

    jsdouble newlen = length;
    if (args.length() > 0) {
        /* Slide up the array to make room for all args at the bottom. */
        if (length > 0) {
            bool optimized = false;
            do {
                if (!obj->isDenseArray())
                    break;
                if (js_PrototypeHasIndexedProperties(cx, obj))
                    break;
                JSObject::EnsureDenseResult result = obj->ensureDenseArrayElements(cx, length, args.length());
                if (result != JSObject::ED_OK) {
                    if (result == JSObject::ED_FAILED)
                        return false;
                    JS_ASSERT(result == JSObject::ED_SPARSE);
                    break;
                }
                obj->moveDenseArrayElements(args.length(), 0, length);
                for (uint32 i = 0; i < args.length(); i++)
                    obj->setDenseArrayElement(i, MagicValue(JS_ARRAY_HOLE));
                optimized = true;
            } while (false);

            if (!optimized) {
                jsdouble last = length;
                jsdouble upperIndex = last + args.length();
                AutoValueRooter tvr(cx);
                do {
                    --last, --upperIndex;
                    JSBool hole;
                    if (!JS_CHECK_OPERATION_LIMIT(cx) ||
                        !GetElement(cx, obj, last, &hole, tvr.addr()) ||
                        !SetOrDeleteArrayElement(cx, obj, upperIndex, hole, tvr.value())) {
                        return JS_FALSE;
                    }
                } while (last != 0);
            }
        }

        /* Copy from args to the bottom of the array. */
        if (!InitArrayElements(cx, obj, 0, args.length(), args.array(), true))
            return JS_FALSE;

        newlen += args.length();
    }
    if (!js_SetLengthProperty(cx, obj, newlen))
        return JS_FALSE;

    /* Follow Perl by returning the new array length. */
    args.rval().setNumber(newlen);
    return JS_TRUE;
}

static inline void
TryReuseArrayType(JSObject *obj, JSObject *nobj)
{
    /*
     * Try to change the type of a newly created array nobj to the same type
     * as obj. This can only be performed if the original object is an array
     * and has the same prototype.
     */
    JS_ASSERT(nobj->isDenseArray());
    JS_ASSERT(nobj->type() == nobj->getProto()->newType);

    if (obj->isArray() && !obj->hasSingletonType() && obj->getProto() == nobj->getProto())
        nobj->setType(obj->type());
}

/*
 * Returns true if this is a dense array whose |count| properties starting from
 * |startingIndex| may be accessed (get, set, delete) directly through its
 * contiguous vector of elements without fear of getters, setters, etc. along
 * the prototype chain, or of enumerators requiring notification of
 * modifications.
 */
static inline bool
CanOptimizeForDenseStorage(JSObject *arr, uint32 startingIndex, uint32 count, JSContext *cx)
{
    /* If the desired properties overflow dense storage, we can't optimize. */
    if (UINT32_MAX - startingIndex < count)
        return false;

    /* There's no optimizing possible if it's not a dense array. */
    if (!arr->isDenseArray())
        return false;

    /*
     * Don't optimize if the array might be in the midst of iteration.  We
     * rely on this to be able to safely move dense array elements around with
     * just a memmove (see JSObject::moveDenseArrayElements), without worrying
     * about updating any in-progress enumerators for properties implicitly
     * deleted if a hole is moved from one location to another location not yet
     * visited.  See bug 690622.
     *
     * Another potential wrinkle: what if the enumeration is happening on an
     * object which merely has |arr| on its prototype chain?  It turns out this
     * case can't happen, because any dense array used as the prototype of
     * another object is first slowified, for type inference's sake.
     */
    if (JS_UNLIKELY(arr->getType(cx)->hasAllFlags(OBJECT_FLAG_ITERATED)))
        return false;

    /* Now just watch out for getters and setters along the prototype chain. */
    return !js_PrototypeHasIndexedProperties(cx, arr) &&
           startingIndex + count <= arr->getDenseArrayInitializedLength();
}

static inline bool
CopyArrayElement(JSContext *cx, JSObject *source, uint32 sourceIndex,
                 JSObject *target, uint32 targetIndex)
{
    if (!JS_CHECK_OPERATION_LIMIT(cx))
        return false;

    JSBool hole;
    Value fromValue;
    return GetElement(cx, source, sourceIndex, &hole, &fromValue) &&
           SetOrDeleteArrayElement(cx, target, targetIndex, hole, fromValue);
}

/* ES5 15.4.4.12. */
static JSBool
array_splice(JSContext *cx, uintN argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    /* Step 1. */
    JSObject *obj = ToObject(cx, &args.thisv());
    if (!obj)
        return false;

    /* Steps 3-4. */
    uint32 len;
    if (!js_GetLengthProperty(cx, obj, &len))
        return false;

    /* Step 5. */
    double relativeStart;
    if (!ToInteger(cx, argc >= 1 ? args[0] : UndefinedValue(), &relativeStart))
        return false;

    /* Step 6. */
    uint32 actualStart;
    if (relativeStart < 0)
        actualStart = JS_MAX(len + relativeStart, 0);
    else
        actualStart = JS_MIN(relativeStart, len);

    /* Step 7. */
    uint32 actualDeleteCount;
    if (argc != 1) {
        jsdouble deleteCountDouble;
        if (!ToInteger(cx, argc >= 2 ? args[1] : Int32Value(0), &deleteCountDouble))
            return false;
        actualDeleteCount = JS_MIN(JS_MAX(deleteCountDouble, 0), len - actualStart);
    } else {
        /*
         * Non-standard: if start was specified but deleteCount was omitted,
         * delete to the end of the array.  See bug 668024 for discussion.
         */
        actualDeleteCount = len - actualStart;
    }

    JS_ASSERT(len - actualStart >= actualDeleteCount);

    /* Steps 2, 8-9. */
    JSObject *arr;
    if (CanOptimizeForDenseStorage(obj, actualStart, actualDeleteCount, cx)) {
        arr = NewDenseCopiedArray(cx, actualDeleteCount,
                                  obj->getDenseArrayElements() + actualStart);
        if (!arr)
            return false;
        TryReuseArrayType(obj, arr);
    } else {
        arr = NewDenseAllocatedArray(cx, actualDeleteCount);
        if (!arr)
            return false;
        TryReuseArrayType(obj, arr);

        for (uint32 k = 0; k < actualDeleteCount; k++) {
            JSBool hole;
            Value fromValue;
            if (!JS_CHECK_OPERATION_LIMIT(cx) ||
                !GetElement(cx, obj, actualStart + k, &hole, &fromValue) ||
                (!hole && !arr->defineElement(cx, k, fromValue)))
            {
                return false;
            }
        }
    }

    /* Step 11. */
    uint32 itemCount = (argc >= 2) ? (argc - 2) : 0;

    if (itemCount < actualDeleteCount) {
        /* Step 12: the array is being shrunk. */
        uint32 sourceIndex = actualStart + actualDeleteCount;
        uint32 targetIndex = actualStart + itemCount;
        uint32 finalLength = len - actualDeleteCount + itemCount;

        if (CanOptimizeForDenseStorage(obj, 0, len, cx)) {
            /* Steps 12(a)-(b). */
            obj->moveDenseArrayElements(targetIndex, sourceIndex, len - sourceIndex);

            /* Steps 12(c)-(d). */
            obj->shrinkDenseArrayElements(cx, finalLength);

            /*
             * The array's initialized length is now out of sync with the array
             * elements: resynchronize it.
             */
            if (cx->typeInferenceEnabled())
                obj->setDenseArrayInitializedLength(finalLength);

            /* Fix running enumerators for the deleted items. */
            if (!js_SuppressDeletedElements(cx, obj, finalLength, len))
                return false;
        } else {
            /*
             * This is all very slow if the length is very large. We don't yet
             * have the ability to iterate in sorted order, so we just do the
             * pessimistic thing and let JS_CHECK_OPERATION_LIMIT handle the
             * fallout.
             */

            /* Steps 12(a)-(b). */
            for (uint32 from = sourceIndex, to = targetIndex; from < len; from++, to++) {
                JSBool hole;
                Value fromValue;
                if (!JS_CHECK_OPERATION_LIMIT(cx) ||
                    !GetElement(cx, obj, from, &hole, &fromValue) ||
                    !SetOrDeleteArrayElement(cx, obj, to, hole, fromValue))
                {
                    return false;
                }
            }

            /* Steps 12(c)-(d). */
            for (uint32 k = len; k > finalLength; k--) {
                if (DeleteArrayElement(cx, obj, k - 1, true) < 0)
                    return false;
            }
        }
    } else if (itemCount > actualDeleteCount) {
        /* Step 13. */

        /*
         * Optimize only if the array is already dense and we can extend it to
         * its new length.
         */
        if (obj->isDenseArray()) {
            JSObject::EnsureDenseResult res =
                obj->ensureDenseArrayElements(cx, obj->getArrayLength(),
                                              itemCount - actualDeleteCount);
            if (res == JSObject::ED_FAILED)
                return false;

            if (res == JSObject::ED_SPARSE) {
                if (!obj->makeDenseArraySlow(cx))
                    return false;
            } else {
                JS_ASSERT(res == JSObject::ED_OK);
            }
        }

        if (CanOptimizeForDenseStorage(obj, len, itemCount - actualDeleteCount, cx)) {
            obj->moveDenseArrayElements(actualStart + itemCount,
                                        actualStart + actualDeleteCount,
                                        len - (actualStart + actualDeleteCount));

            if (cx->typeInferenceEnabled())
                obj->setDenseArrayInitializedLength(len + itemCount - actualDeleteCount);
        } else {
            for (jsdouble k = len - actualDeleteCount; k > actualStart; k--) {
                jsdouble from = k + actualDeleteCount - 1;
                jsdouble to = k + itemCount - 1;

                JSBool hole;
                Value fromValue;
                if (!JS_CHECK_OPERATION_LIMIT(cx) ||
                    !GetElement(cx, obj, from, &hole, &fromValue) ||
                    !SetOrDeleteArrayElement(cx, obj, to, hole, fromValue))
                {
                    return false;
                }
            }
        }
    }

    /* Step 10. */
    Value *items = args.array() + 2;

    /* Steps 14-15. */
    for (uint32 k = actualStart, i = 0; i < itemCount; i++, k++) {
        if (!SetArrayElement(cx, obj, k, items[i]))
            return false;
    }

    /* Step 16. */
    jsdouble finalLength = jsdouble(len) - actualDeleteCount + itemCount;
    if (!js_SetLengthProperty(cx, obj, finalLength))
        return false;

    /* Step 17. */
    args.rval().setObject(*arr);
    return true;
}

#ifdef JS_METHODJIT
void JS_FASTCALL
mjit::stubs::ArrayConcatTwoArrays(VMFrame &f)
{
    JSObject *result = &f.regs.sp[-3].toObject();
    JSObject *obj1 = &f.regs.sp[-2].toObject();
    JSObject *obj2 = &f.regs.sp[-1].toObject();

    JS_ASSERT(result->isDenseArray() && obj1->isDenseArray() && obj2->isDenseArray());

    uint32 initlen1 = obj1->getDenseArrayInitializedLength();
    JS_ASSERT(initlen1 == obj1->getArrayLength());

    uint32 initlen2 = obj2->getDenseArrayInitializedLength();
    JS_ASSERT(initlen2 == obj2->getArrayLength());

    /* No overflow here due to nslots limit. */
    uint32 len = initlen1 + initlen2;

    if (!result->ensureSlots(f.cx, len))
        THROW();

    result->copyDenseArrayElements(0, obj1->getDenseArrayElements(), initlen1);
    result->copyDenseArrayElements(initlen1, obj2->getDenseArrayElements(), initlen2);

    result->setDenseArrayInitializedLength(len);
    result->setDenseArrayLength(len);
}
#endif /* JS_METHODJIT */

/*
 * Python-esque sequence operations.
 */
JSBool
js::array_concat(JSContext *cx, uintN argc, Value *vp)
{
    /* Treat our |this| object as the first argument; see ECMA 15.4.4.4. */
    Value *p = JS_ARGV(cx, vp) - 1;

    /* Create a new Array object and root it using *vp. */
    JSObject *aobj = ToObject(cx, &vp[1]);
    if (!aobj)
        return false;

    JSObject *nobj;
    jsuint length;
    if (aobj->isDenseArray()) {
        length = aobj->getArrayLength();
        const Value *vector = aobj->getDenseArrayElements();
        jsuint initlen = aobj->getDenseArrayInitializedLength();
        nobj = NewDenseCopiedArray(cx, initlen, vector);
        if (!nobj)
            return JS_FALSE;
        TryReuseArrayType(aobj, nobj);
        nobj->setArrayLength(cx, length);
        if (!aobj->isPackedDenseArray())
            nobj->markDenseArrayNotPacked(cx);
        vp->setObject(*nobj);
        if (argc == 0)
            return JS_TRUE;
        argc--;
        p++;
    } else {
        nobj = NewDenseEmptyArray(cx);
        if (!nobj)
            return JS_FALSE;
        vp->setObject(*nobj);
        length = 0;
    }

    /* Loop over [0, argc] to concat args into nobj, expanding all Arrays. */
    for (uintN i = 0; i <= argc; i++) {
        if (!JS_CHECK_OPERATION_LIMIT(cx))
            return false;
        const Value &v = p[i];
        if (v.isObject()) {
            JSObject &obj = v.toObject();
            if (ObjectClassIs(obj, ESClass_Array, cx)) {
                jsuint alength;
                if (!js_GetLengthProperty(cx, &obj, &alength))
                    return false;
                for (uint32 slot = 0; slot < alength; slot++) {
                    JSBool hole;
                    Value tmp;
                    if (!JS_CHECK_OPERATION_LIMIT(cx) || !GetElement(cx, &obj, slot, &hole, &tmp))
                        return false;

                    /*
                     * Per ECMA 262, 15.4.4.4, step 9, ignore nonexistent
                     * properties.
                     */
                    if (!hole && !SetArrayElement(cx, nobj, length + slot, tmp))
                        return false;
                }
                length += alength;
                continue;
            }
        }

        if (!SetArrayElement(cx, nobj, length, v))
            return false;
        length++;
    }

    return js_SetLengthProperty(cx, nobj, length);
}

static JSBool
array_slice(JSContext *cx, uintN argc, Value *vp)
{
    JSObject *nobj;
    jsuint length, begin, end, slot;
    JSBool hole;

    CallArgs args = CallArgsFromVp(argc, vp);

    JSObject *obj = ToObject(cx, &args.thisv());
    if (!obj)
        return false;

    if (!js_GetLengthProperty(cx, obj, &length))
        return JS_FALSE;
    begin = 0;
    end = length;

    if (args.length() > 0) {
        jsdouble d;
        if (!ToInteger(cx, args[0], &d))
            return false;
        if (d < 0) {
            d += length;
            if (d < 0)
                d = 0;
        } else if (d > length) {
            d = length;
        }
        begin = (jsuint)d;

        if (args.length() > 1 && !args[1].isUndefined()) {
            if (!ToInteger(cx, args[1], &d))
                return false;
            if (d < 0) {
                d += length;
                if (d < 0)
                    d = 0;
            } else if (d > length) {
                d = length;
            }
            end = (jsuint)d;
        }
    }

    if (begin > end)
        begin = end;

    if (obj->isDenseArray() && end <= obj->getDenseArrayInitializedLength() &&
        !js_PrototypeHasIndexedProperties(cx, obj)) {
        nobj = NewDenseCopiedArray(cx, end - begin, obj->getDenseArrayElements() + begin);
        if (!nobj)
            return JS_FALSE;
        TryReuseArrayType(obj, nobj);
        if (!obj->isPackedDenseArray())
            nobj->markDenseArrayNotPacked(cx);
        args.rval().setObject(*nobj);
        return JS_TRUE;
    }

    nobj = NewDenseAllocatedArray(cx, end - begin);
    if (!nobj)
        return JS_FALSE;
    TryReuseArrayType(obj, nobj);

    AutoValueRooter tvr(cx);
    for (slot = begin; slot < end; slot++) {
        if (!JS_CHECK_OPERATION_LIMIT(cx) ||
            !GetElement(cx, obj, slot, &hole, tvr.addr())) {
            return JS_FALSE;
        }
        if (!hole && !SetArrayElement(cx, nobj, slot - begin, tvr.value()))
            return JS_FALSE;
    }

    args.rval().setObject(*nobj);
    return JS_TRUE;
}

enum IndexOfKind {
    IndexOf,
    LastIndexOf
};

static JSBool
array_indexOfHelper(JSContext *cx, IndexOfKind mode, CallArgs &args)
{
    jsuint length, i, stop;
    Value tosearch;
    jsint direction;
    JSBool hole;

    JSObject *obj = ToObject(cx, &args.thisv());
    if (!obj)
        return false;
    if (!js_GetLengthProperty(cx, obj, &length))
        return JS_FALSE;
    if (length == 0)
        goto not_found;

    if (args.length() <= 1) {
        i = (mode == LastIndexOf) ? length - 1 : 0;
        tosearch = (args.length() != 0) ? args[0] : UndefinedValue();
    } else {
        jsdouble start;

        tosearch = args[0];
        if (!ToInteger(cx, args[1], &start))
            return false;
        if (start < 0) {
            start += length;
            if (start < 0) {
                if (mode == LastIndexOf)
                    goto not_found;
                i = 0;
            } else {
                i = (jsuint)start;
            }
        } else if (start >= length) {
            if (mode == IndexOf)
                goto not_found;
            i = length - 1;
        } else {
            i = (jsuint)start;
        }
    }

    if (mode == LastIndexOf) {
        stop = 0;
        direction = -1;
    } else {
        stop = length - 1;
        direction = 1;
    }

    for (;;) {
        Value elt;
        if (!JS_CHECK_OPERATION_LIMIT(cx) ||
            !GetElement(cx, obj, (jsuint)i, &hole, &elt)) {
            return JS_FALSE;
        }
        if (!hole) {
            JSBool equal;
            if (!StrictlyEqual(cx, elt, tosearch, &equal))
                return JS_FALSE;
            if (equal) {
                args.rval().setNumber(i);
                return JS_TRUE;
            }
        }
        if (i == stop)
            goto not_found;
        i += direction;
    }

  not_found:
    args.rval().setInt32(-1);
    return JS_TRUE;
}

static JSBool
array_indexOf(JSContext *cx, uintN argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return array_indexOfHelper(cx, IndexOf, args);
}

static JSBool
array_lastIndexOf(JSContext *cx, uintN argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return array_indexOfHelper(cx, LastIndexOf, args);
}

/* Order is important; extras that take a predicate funarg must follow MAP. */
typedef enum ArrayExtraMode {
    FOREACH,
    REDUCE,
    REDUCE_RIGHT,
    MAP,
    FILTER,
    SOME,
    EVERY
} ArrayExtraMode;

#define REDUCE_MODE(mode) ((mode) == REDUCE || (mode) == REDUCE_RIGHT)

static JSBool
array_extra(JSContext *cx, ArrayExtraMode mode, CallArgs &args)
{
    JSObject *obj = ToObject(cx, &args.thisv());
    if (!obj)
        return false;

    jsuint length;
    if (!js_GetLengthProperty(cx, obj, &length))
        return JS_FALSE;

    /*
     * First, get or compute our callee, so that we error out consistently
     * when passed a non-callable object.
     */
    if (args.length() == 0) {
        js_ReportMissingArg(cx, args.calleev(), 0);
        return JS_FALSE;
    }
    JSObject *callable = js_ValueToCallableObject(cx, &args[0], JSV2F_SEARCH_STACK);
    if (!callable)
        return JS_FALSE;

    /*
     * Set our initial return condition, used for zero-length array cases
     * (and pre-size our map return to match our known length, for all cases).
     */
    jsuint newlen;
    JSObject *newarr;
    TypeObject *newtype = NULL;
#ifdef __GNUC__ /* quell GCC overwarning */
    newlen = 0;
    newarr = NULL;
#endif
    jsuint start = 0, end = length;
    jsint step = 1;

    switch (mode) {
      case REDUCE_RIGHT:
        start = length - 1, end = -1, step = -1;
        /* FALL THROUGH */
      case REDUCE:
        if (length == 0 && args.length() == 1) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                 JSMSG_EMPTY_ARRAY_REDUCE);
            return JS_FALSE;
        }
        if (args.length() >= 2) {
            args.rval() = args[1];
        } else {
            JSBool hole;
            do {
                if (!GetElement(cx, obj, start, &hole, &args.rval()))
                    return JS_FALSE;
                start += step;
            } while (hole && start != end);

            if (hole && start == end) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                     JSMSG_EMPTY_ARRAY_REDUCE);
                return JS_FALSE;
            }
        }
        break;
      case MAP:
      case FILTER:
        newlen = (mode == MAP) ? length : 0;
        newarr = NewDenseAllocatedArray(cx, newlen);
        if (!newarr)
            return JS_FALSE;
        newtype = GetTypeCallerInitObject(cx, JSProto_Array);
        if (!newtype)
            return JS_FALSE;
        newarr->setType(newtype);
        args.rval().setObject(*newarr);
        break;
      case SOME:
        args.rval().setBoolean(false);
        break;
      case EVERY:
        args.rval().setBoolean(true);
        break;
      case FOREACH:
        args.rval().setUndefined();
        break;
    }

    if (length == 0)
        return JS_TRUE;

    Value thisv = (args.length() > 1 && !REDUCE_MODE(mode)) ? args[1] : UndefinedValue();

    /*
     * For all but REDUCE, we call with 3 args (value, index, array). REDUCE
     * requires 4 args (accum, value, index, array).
     */
    uintN agArgc = 3 + REDUCE_MODE(mode);

    MUST_FLOW_THROUGH("out");
    JSBool ok = JS_TRUE;
    JSBool cond;

    Value objv = ObjectValue(*obj);
    AutoValueRooter tvr(cx);
    InvokeArgsGuard ag;
    for (jsuint i = start; i != end; i += step) {
        JSBool hole;
        ok = JS_CHECK_OPERATION_LIMIT(cx) &&
             GetElement(cx, obj, i, &hole, tvr.addr());
        if (!ok)
            goto out;
        if (hole)
            continue;

        if (!ag.pushed() && !cx->stack.pushInvokeArgs(cx, agArgc, &ag))
            return false;

        /*
         * Push callable and 'this', then args. We must do this for every
         * iteration around the loop since Invoke clobbers its arguments.
         */
        ag.calleeHasBeenReset();
        ag.calleev() = ObjectValue(*callable);
        ag.thisv() = thisv;
        uintN argi = 0;
        if (REDUCE_MODE(mode))
            ag[argi++] = args.rval();
        ag[argi++] = tvr.value();
        ag[argi++] = Int32Value(i);
        ag[argi]   = objv;

        /* Do the call. */
        ok = Invoke(cx, ag);
        if (!ok)
            break;

        const Value &rval = ag.rval();

        if (mode > MAP)
            cond = js_ValueToBoolean(rval);
#ifdef __GNUC__ /* quell GCC overwarning */
        else
            cond = JS_FALSE;
#endif

        switch (mode) {
          case FOREACH:
            break;
          case REDUCE:
          case REDUCE_RIGHT:
            args.rval() = rval;
            break;
          case MAP:
            if (!ok)
                goto out;
            ok = SetArrayElement(cx, newarr, i, rval);
            if (!ok)
                goto out;
            break;
          case FILTER:
            if (!cond)
                break;
            /* The element passed the filter, so push it onto our result. */
            if (!ok)
                goto out;
            ok = SetArrayElement(cx, newarr, newlen++, tvr.value());
            if (!ok)
                goto out;
            break;
          case SOME:
            if (cond) {
                args.rval().setBoolean(true);
                goto out;
            }
            break;
          case EVERY:
            if (!cond) {
                args.rval().setBoolean(false);
                goto out;
            }
            break;
        }
    }

  out:
    if (ok && mode == FILTER)
        ok = js_SetLengthProperty(cx, newarr, newlen);
    return ok;
}

static JSBool
array_forEach(JSContext *cx, uintN argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return array_extra(cx, FOREACH, args);
}

static JSBool
array_map(JSContext *cx, uintN argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return array_extra(cx, MAP, args);
}

static JSBool
array_reduce(JSContext *cx, uintN argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return array_extra(cx, REDUCE, args);
}

static JSBool
array_reduceRight(JSContext *cx, uintN argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return array_extra(cx, REDUCE_RIGHT, args);
}

static JSBool
array_filter(JSContext *cx, uintN argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return array_extra(cx, FILTER, args);
}

static JSBool
array_some(JSContext *cx, uintN argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return array_extra(cx, SOME, args);
}

static JSBool
array_every(JSContext *cx, uintN argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return array_extra(cx, EVERY, args);
}

static JSBool
array_isArray(JSContext *cx, uintN argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    bool isArray = args.length() > 0 &&
                   args[0].isObject() &&
                   ObjectClassIs(args[0].toObject(), ESClass_Array, cx);
    args.rval().setBoolean(isArray);
    return true;
}

#define GENERIC JSFUN_GENERIC_NATIVE

static JSFunctionSpec array_methods[] = {
#if JS_HAS_TOSOURCE
    JS_FN(js_toSource_str,      array_toSource,     0,0),
#endif
    JS_FN(js_toString_str,      array_toString,     0,0),
    JS_FN(js_toLocaleString_str,array_toLocaleString,0,0),

    /* Perl-ish methods. */
    JS_FN("join",               array_join,         1,JSFUN_GENERIC_NATIVE),
    JS_FN("reverse",            array_reverse,      0,JSFUN_GENERIC_NATIVE),
    JS_FN("sort",               array_sort,         1,JSFUN_GENERIC_NATIVE),
    JS_FN("push",               array_push,         1,JSFUN_GENERIC_NATIVE),
    JS_FN("pop",                array_pop,          0,JSFUN_GENERIC_NATIVE),
    JS_FN("shift",              array_shift,        0,JSFUN_GENERIC_NATIVE),
    JS_FN("unshift",            array_unshift,      1,JSFUN_GENERIC_NATIVE),
    JS_FN("splice",             array_splice,       2,JSFUN_GENERIC_NATIVE),

    /* Pythonic sequence methods. */
    JS_FN("concat",             array_concat,       1,JSFUN_GENERIC_NATIVE),
    JS_FN("slice",              array_slice,        2,JSFUN_GENERIC_NATIVE),

    JS_FN("indexOf",            array_indexOf,      1,JSFUN_GENERIC_NATIVE),
    JS_FN("lastIndexOf",        array_lastIndexOf,  1,JSFUN_GENERIC_NATIVE),
    JS_FN("forEach",            array_forEach,      1,JSFUN_GENERIC_NATIVE),
    JS_FN("map",                array_map,          1,JSFUN_GENERIC_NATIVE),
    JS_FN("reduce",             array_reduce,       1,JSFUN_GENERIC_NATIVE),
    JS_FN("reduceRight",        array_reduceRight,  1,JSFUN_GENERIC_NATIVE),
    JS_FN("filter",             array_filter,       1,JSFUN_GENERIC_NATIVE),
    JS_FN("some",               array_some,         1,JSFUN_GENERIC_NATIVE),
    JS_FN("every",              array_every,        1,JSFUN_GENERIC_NATIVE),

    JS_FS_END
};

static JSFunctionSpec array_static_methods[] = {
    JS_FN("isArray",            array_isArray,      1,0),
    JS_FS_END
};

/* ES5 15.4.2 */
JSBool
js_Array(JSContext *cx, uintN argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    TypeObject *type = GetTypeCallerInitObject(cx, JSProto_Array);
    if (!type)
        return JS_FALSE;

    if (args.length() != 1 || !args[0].isNumber()) {
        if (!InitArrayTypes(cx, type, args.array(), args.length()))
            return false;
        JSObject *obj = (args.length() == 0)
                        ? NewDenseEmptyArray(cx)
                        : NewDenseCopiedArray(cx, args.length(), args.array());
        if (!obj)
            return false;
        obj->setType(type);
        args.rval().setObject(*obj);
        return true;
    }

    uint32 length;
    if (args[0].isInt32()) {
        int32 i = args[0].toInt32();
        if (i < 0) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_ARRAY_LENGTH);
            return false;
        }
        length = uint32(i);
    } else {
        jsdouble d = args[0].toDouble();
        length = js_DoubleToECMAUint32(d);
        if (d != jsdouble(length)) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_ARRAY_LENGTH);
            return false;
        }
    }

    JSObject *obj = NewDenseUnallocatedArray(cx, length);
    if (!obj)
        return false;

    obj->setType(type);

    /* If the length calculation overflowed, make sure that is marked for the new type. */
    if (obj->getArrayLength() > INT32_MAX)
        obj->setArrayLength(cx, obj->getArrayLength());

    args.rval().setObject(*obj);
    return true;
}

JSObject *
js_InitArrayClass(JSContext *cx, JSObject *obj)
{
    JS_ASSERT(obj->isNative());

    GlobalObject *global = obj->asGlobal();

    JSObject *arrayProto = global->createBlankPrototype(cx, &SlowArrayClass);
    if (!arrayProto || !AddLengthProperty(cx, arrayProto))
        return NULL;
    arrayProto->setArrayLength(cx, 0);

    JSFunction *ctor = global->createConstructor(cx, js_Array, &ArrayClass,
                                                 CLASS_ATOM(cx, Array), 1);
    if (!ctor)
        return NULL;

    /* The default 'new' object for Array.prototype has unknown properties. */
    arrayProto->getNewType(cx, NULL, /* markUnknown = */ true);

    if (!LinkConstructorAndPrototype(cx, ctor, arrayProto))
        return NULL;

    if (!DefinePropertiesAndBrand(cx, arrayProto, NULL, array_methods) ||
        !DefinePropertiesAndBrand(cx, ctor, NULL, array_static_methods))
    {
        return NULL;
    }

    if (!DefineConstructorAndPrototype(cx, global, JSProto_Array, ctor, arrayProto))
        return NULL;

    return arrayProto;
}

/*
 * Array allocation functions.
 */
namespace js {

template<bool allocateCapacity>
static JS_ALWAYS_INLINE JSObject *
NewArray(JSContext *cx, jsuint length, JSObject *proto)
{
    JS_ASSERT_IF(proto, proto->isArray());

    gc::AllocKind kind = GuessObjectGCKind(length, true);
    JSObject *obj = detail::NewObject<WithProto::Class, false>(cx, &ArrayClass, proto, NULL, kind);
    if (!obj)
        return NULL;

    obj->setArrayLength(cx, length);

    if (!cx->typeInferenceEnabled()) {
        obj->markDenseArrayNotPacked(cx);
        obj->backfillDenseArrayHoles(cx);
    }

    if (allocateCapacity) {
        /* If ensureSlots creates dynamically allocated slots, then having fixedSlots is a waste. */
        DebugOnly<uint32> oldSlots = obj->numSlots();

        if (!obj->ensureSlots(cx, length))
            return NULL;

        JS_ASSERT_IF(obj->numFixedSlots(), oldSlots == obj->numSlots());
    }

    return obj;
}

JSObject * JS_FASTCALL
NewDenseEmptyArray(JSContext *cx, JSObject *proto)
{
    return NewArray<false>(cx, 0, proto);
}

JSObject * JS_FASTCALL
NewDenseAllocatedArray(JSContext *cx, uint32 length, JSObject *proto)
{
    return NewArray<true>(cx, length, proto);
}

JSObject * JS_FASTCALL
NewDenseAllocatedEmptyArray(JSContext *cx, uint length, JSObject *proto)
{
    return NewArray<true>(cx, length, proto);
}

JSObject * JS_FASTCALL
NewDenseUnallocatedArray(JSContext *cx, uint32 length, JSObject *proto)
{
    return NewArray<false>(cx, length, proto);
}

#ifdef JS_METHODJIT
JSObject * JS_FASTCALL
mjit::stubs::NewDenseUnallocatedArray(VMFrame &f, uint32 length)
{
    JSObject *proto = (JSObject *) f.scratch;
    JSObject *obj = NewArray<false>(f.cx, length, proto);
    if (!obj) {
        js_ReportOutOfMemory(f.cx);
        THROWV(NULL);
    }
    return obj;
}
#endif

JSObject *
NewDenseCopiedArray(JSContext *cx, uint32 length, const Value *vp, JSObject *proto /* = NULL */)
{
    JSObject* obj = NewArray<true>(cx, length, proto);
    if (!obj)
        return NULL;

    JS_ASSERT(obj->getDenseArrayCapacity() >= length);

    if (cx->typeInferenceEnabled())
        obj->setDenseArrayInitializedLength(vp ? length : 0);

    if (vp)
        obj->copyDenseArrayElements(0, vp, length);

    return obj;
}

#ifdef JS_TRACER
JS_DEFINE_CALLINFO_2(extern, OBJECT, NewDenseEmptyArray, CONTEXT, OBJECT, 0,
                     nanojit::ACCSET_STORE_ANY)
JS_DEFINE_CALLINFO_3(extern, OBJECT, NewDenseAllocatedArray, CONTEXT, UINT32, OBJECT, 0,
                     nanojit::ACCSET_STORE_ANY)
JS_DEFINE_CALLINFO_3(extern, OBJECT, NewDenseAllocatedEmptyArray, CONTEXT, UINT32, OBJECT, 0,
                     nanojit::ACCSET_STORE_ANY)
JS_DEFINE_CALLINFO_3(extern, OBJECT, NewDenseUnallocatedArray, CONTEXT, UINT32, OBJECT, 0,
                     nanojit::ACCSET_STORE_ANY)
#endif



JSObject *
NewSlowEmptyArray(JSContext *cx)
{
    JSObject *obj = NewNonFunction<WithProto::Class>(cx, &SlowArrayClass, NULL, NULL);
    if (!obj || !AddLengthProperty(cx, obj))
        return NULL;

    obj->setArrayLength(cx, 0);
    return obj;
}

}


#ifdef DEBUG
JSBool
js_ArrayInfo(JSContext *cx, uintN argc, jsval *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JSObject *array;

    for (uintN i = 0; i < args.length(); i++) {
        Value arg = args[i];

        char *bytes = DecompileValueGenerator(cx, JSDVG_SEARCH_STACK, arg, NULL);
        if (!bytes)
            return JS_FALSE;
        if (arg.isPrimitive() ||
            !(array = arg.toObjectOrNull())->isArray()) {
            fprintf(stderr, "%s: not array\n", bytes);
            cx->free_(bytes);
            continue;
        }
        fprintf(stderr, "%s: %s (len %u", bytes,
                array->isDenseArray() ? "dense" : "sparse",
                array->getArrayLength());
        if (array->isDenseArray()) {
            fprintf(stderr, ", capacity %u",
                    array->getDenseArrayCapacity());
        }
        fputs(")\n", stderr);
        cx->free_(bytes);
    }

    args.rval().setUndefined();
    return true;
}
#endif
