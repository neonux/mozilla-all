/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=79 ft=cpp:
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
 * The Original Code is SpiderMonkey JavaScript engine.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef jsclass_h__
#define jsclass_h__
/*
 * A JSClass acts as a vtable for JS objects that allows JSAPI clients to
 * control various aspects of the behavior of an object like property lookup.
 * js::Class is an engine-private extension that allows more control over
 * object behavior and, e.g., allows custom slow layout.
 */
#include "jsapi.h"
#include "jsprvtd.h"

namespace js {

class AutoIdVector;

/* js::Class operation signatures. */

typedef JSBool
(* LookupPropOp)(JSContext *cx, JSObject *obj, jsid id, JSObject **objp,
                 JSProperty **propp);
typedef JSBool
(* LookupElementOp)(JSContext *cx, JSObject *obj, uint32 index, JSObject **objp,
                    JSProperty **propp);
typedef JSBool
(* DefinePropOp)(JSContext *cx, JSObject *obj, jsid id, const Value *value,
                 PropertyOp getter, StrictPropertyOp setter, uintN attrs);
typedef JSBool
(* DefineElementOp)(JSContext *cx, JSObject *obj, uint32 index, const Value *value,
                    PropertyOp getter, StrictPropertyOp setter, uintN attrs);
typedef JSBool
(* PropertyIdOp)(JSContext *cx, JSObject *obj, JSObject *receiver, jsid id, Value *vp);
typedef JSBool
(* ElementIdOp)(JSContext *cx, JSObject *obj, JSObject *receiver, uint32 index, Value *vp);
typedef JSBool
(* StrictPropertyIdOp)(JSContext *cx, JSObject *obj, jsid id, Value *vp, JSBool strict);
typedef JSBool
(* StrictElementIdOp)(JSContext *cx, JSObject *obj, uint32 index, Value *vp, JSBool strict);
typedef JSBool
(* AttributesOp)(JSContext *cx, JSObject *obj, jsid id, uintN *attrsp);
typedef JSBool
(* ElementAttributesOp)(JSContext *cx, JSObject *obj, uint32 index, uintN *attrsp);
typedef JSBool
(* DeleteIdOp)(JSContext *cx, JSObject *obj, jsid id, Value *vp, JSBool strict);
typedef JSBool
(* DeleteElementOp)(JSContext *cx, JSObject *obj, uint32 index, Value *vp, JSBool strict);
typedef JSType
(* TypeOfOp)(JSContext *cx, JSObject *obj);

/*
 * Prepare to make |obj| non-extensible; in particular, fully resolve its properties.
 * On error, return false.
 * If |obj| is now ready to become non-extensible, set |*fixed| to true and return true.
 * If |obj| refuses to become non-extensible, set |*fixed| to false and return true; the
 * caller will throw an appropriate error.
 */
typedef JSBool
(* FixOp)(JSContext *cx, JSObject *obj, bool *fixed, AutoIdVector *props);

typedef JSObject *
(* ObjectOp)(JSContext *cx, JSObject *obj);
typedef void
(* FinalizeOp)(JSContext *cx, JSObject *obj);

#define JS_CLASS_MEMBERS                                                      \
    const char          *name;                                                \
    uint32              flags;                                                \
                                                                              \
    /* Mandatory non-null function pointer members. */                        \
    JSPropertyOp        addProperty;                                          \
    JSPropertyOp        delProperty;                                          \
    JSPropertyOp        getProperty;                                          \
    JSStrictPropertyOp  setProperty;                                          \
    JSEnumerateOp       enumerate;                                            \
    JSResolveOp         resolve;                                              \
    JSConvertOp         convert;                                              \
    JSFinalizeOp        finalize;                                             \
                                                                              \
    /* Optionally non-null members start here. */                             \
    JSClassInternal     reserved0;                                            \
    JSCheckAccessOp     checkAccess;                                          \
    JSNative            call;                                                 \
    JSNative            construct;                                            \
    JSXDRObjectOp       xdrObject;                                            \
    JSHasInstanceOp     hasInstance;                                          \
    JSTraceOp           trace

/*
 * The helper struct to measure the size of JS_CLASS_MEMBERS to know how much
 * we have to padd js::Class to match the size of JSClass;
 */
struct ClassSizeMeasurement
{
    JS_CLASS_MEMBERS;
};

struct ClassExtension
{
    JSEqualityOp        equality;
    JSObjectOp          outerObject;
    JSObjectOp          innerObject;
    JSIteratorOp        iteratorObject;
    void               *unused;

    /*
     * isWrappedNative is true only if the class is an XPCWrappedNative.
     * WeakMaps use this to override the wrapper disposal optimization.
     */
    bool                isWrappedNative;
};

#define JS_NULL_CLASS_EXT   {NULL,NULL,NULL,NULL,NULL,false}

struct ObjectOps
{
    LookupPropOp        lookupProperty;
    LookupElementOp     lookupElement;
    DefinePropOp        defineProperty;
    DefineElementOp     defineElement;
    PropertyIdOp        getProperty;
    ElementIdOp         getElement;
    StrictPropertyIdOp  setProperty;
    StrictElementIdOp   setElement;
    AttributesOp        getAttributes;
    ElementAttributesOp getElementAttributes;
    AttributesOp        setAttributes;
    ElementAttributesOp setElementAttributes;
    DeleteIdOp          deleteProperty;
    DeleteElementOp     deleteElement;

    JSNewEnumerateOp    enumerate;
    TypeOfOp            typeOf;
    FixOp               fix;
    ObjectOp            thisObject;
    FinalizeOp          clear;
};

#define JS_NULL_OBJECT_OPS                                                    \
    {NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,   \
     NULL,NULL,NULL,NULL,NULL}

struct Class
{
    JS_CLASS_MEMBERS;
    ClassExtension      ext;
    ObjectOps           ops;
    uint8               pad[sizeof(JSClass) - sizeof(ClassSizeMeasurement) -
                            sizeof(ClassExtension) - sizeof(ObjectOps)];

    /* Class is not native and its map is not a scope. */
    static const uint32 NON_NATIVE = JSCLASS_INTERNAL_FLAG2;

    bool isNative() const {
        return !(flags & NON_NATIVE);
    }
};

JS_STATIC_ASSERT(offsetof(JSClass, name) == offsetof(Class, name));
JS_STATIC_ASSERT(offsetof(JSClass, flags) == offsetof(Class, flags));
JS_STATIC_ASSERT(offsetof(JSClass, addProperty) == offsetof(Class, addProperty));
JS_STATIC_ASSERT(offsetof(JSClass, delProperty) == offsetof(Class, delProperty));
JS_STATIC_ASSERT(offsetof(JSClass, getProperty) == offsetof(Class, getProperty));
JS_STATIC_ASSERT(offsetof(JSClass, setProperty) == offsetof(Class, setProperty));
JS_STATIC_ASSERT(offsetof(JSClass, enumerate) == offsetof(Class, enumerate));
JS_STATIC_ASSERT(offsetof(JSClass, resolve) == offsetof(Class, resolve));
JS_STATIC_ASSERT(offsetof(JSClass, convert) == offsetof(Class, convert));
JS_STATIC_ASSERT(offsetof(JSClass, finalize) == offsetof(Class, finalize));
JS_STATIC_ASSERT(offsetof(JSClass, reserved0) == offsetof(Class, reserved0));
JS_STATIC_ASSERT(offsetof(JSClass, checkAccess) == offsetof(Class, checkAccess));
JS_STATIC_ASSERT(offsetof(JSClass, call) == offsetof(Class, call));
JS_STATIC_ASSERT(offsetof(JSClass, construct) == offsetof(Class, construct));
JS_STATIC_ASSERT(offsetof(JSClass, xdrObject) == offsetof(Class, xdrObject));
JS_STATIC_ASSERT(offsetof(JSClass, hasInstance) == offsetof(Class, hasInstance));
JS_STATIC_ASSERT(offsetof(JSClass, trace) == offsetof(Class, trace));
JS_STATIC_ASSERT(sizeof(JSClass) == sizeof(Class));

static JS_ALWAYS_INLINE JSClass *
Jsvalify(Class *c)
{
    return (JSClass *)c;
}

static JS_ALWAYS_INLINE Class *
Valueify(JSClass *c)
{
    return (Class *)c;
}

}  /* namespace js */
#endif  /* jsclass_h__ */
