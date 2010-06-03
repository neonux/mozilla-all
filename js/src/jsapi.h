/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
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

#ifndef jsapi_h___
#define jsapi_h___
/*
 * JavaScript API.
 */
#include <stddef.h>
#include <stdio.h>
#include "js-config.h"
#include "jspubtd.h"
#include "jsutil.h"

JS_BEGIN_EXTERN_C

/* Well-known JS values, initialized on startup. */
#define JSVAL_NULL   BUILD_JSVAL(JSVAL_MASK32_NULL,      0)
#define JSVAL_ZERO   BUILD_JSVAL(JSVAL_MASK32_INT32,     0)
#define JSVAL_ONE    BUILD_JSVAL(JSVAL_MASK32_INT32,     1)
#define JSVAL_FALSE  BUILD_JSVAL(JSVAL_MASK32_BOOLEAN,   JS_FALSE)
#define JSVAL_TRUE   BUILD_JSVAL(JSVAL_MASK32_BOOLEAN,   JS_TRUE)
#define JSVAL_VOID   BUILD_JSVAL(JSVAL_MASK32_UNDEFINED, 0)

/* Predicates for type testing. */

static JS_ALWAYS_INLINE JSBool
JSVAL_IS_NULL(jsval v)
{
    jsval_layout l = { v };
    return JSVAL_IS_NULL_IMPL(l);
}

static JS_ALWAYS_INLINE JSBool
JSVAL_IS_VOID(jsval v)
{
    jsval_layout l = { v };
    return JSVAL_IS_UNDEFINED_IMPL(l);
}

static JS_ALWAYS_INLINE JSBool
JSVAL_IS_INT(jsval v)
{
    jsval_layout l = { v };
    return l.s.mask32 == JSVAL_MASK32_INT32;
}

static JS_ALWAYS_INLINE jsint
JSVAL_TO_INT(jsval v)
{
    JS_ASSERT(JSVAL_IS_INT(v));
    jsval_layout l = { v };
    return l.s.payload.i32;
}

#define JSVAL_INT_BITS          32
#define JSVAL_INT_MIN           ((jsint)0x80000000)
#define JSVAL_INT_MAX           ((jsint)0x7fffffff)

static JS_ALWAYS_INLINE bool
INT_FITS_IN_JSVAL(jsint i)
{
    return JS_TRUE;
}

static JS_ALWAYS_INLINE jsval
INT_TO_JSVAL(int32 i)
{
    return INT32_TO_JSVAL_IMPL(i).asBits;
}

static JS_ALWAYS_INLINE JSBool
JSVAL_IS_DOUBLE(jsval v)
{
    jsval_layout l = { v };
    return JSVAL_IS_DOUBLE_IMPL(l);
}

static JS_ALWAYS_INLINE jsdouble
JSVAL_TO_DOUBLE(jsval v)
{
    JS_ASSERT(JSVAL_IS_DOUBLE(v));
    jsval_layout l = { v };
    return l.asDouble;
}

static JS_ALWAYS_INLINE jsval
DOUBLE_TO_JSVAL(jsdouble d)
{
    return DOUBLE_TO_JSVAL_IMPL(d).asBits;
}

static JS_ALWAYS_INLINE JSBool
JSVAL_IS_NUMBER(jsval v)
{
    jsval_layout l = { v };
    return JSVAL_IS_NUMBER_IMPL(l);
}

static JS_ALWAYS_INLINE JSBool
JSVAL_IS_STRING(jsval v)
{
    jsval_layout l = { v };
    return l.s.mask32 == JSVAL_MASK32_STRING;
}

static JS_ALWAYS_INLINE JSString *
JSVAL_TO_STRING(jsval v)
{
    JS_ASSERT(JSVAL_IS_STRING(v));
    jsval_layout l = { v };
    return JSVAL_TO_STRING_IMPL(l);
}

static JS_ALWAYS_INLINE jsval
STRING_TO_JSVAL(JSString *str)
{
    return STRING_TO_JSVAL_IMPL(str).asBits;
}

static JS_ALWAYS_INLINE JSBool
JSVAL_IS_OBJECT(jsval v)
{
    jsval_layout l = { v };
    return JSVAL_IS_OBJECT_OR_NULL_IMPL(l);
}

static JS_ALWAYS_INLINE JSObject *
JSVAL_TO_OBJECT(jsval v)
{
    JS_ASSERT(JSVAL_IS_OBJECT(v));
    jsval_layout l = { v };
    return JSVAL_TO_OBJECT_IMPL(l);
}

/* N.B. Not cheap; uses public API instead of obj->isFunction()! */
static JS_ALWAYS_INLINE jsval
OBJECT_TO_JSVAL(JSObject *obj)
{
    extern JS_PUBLIC_API(JSBool)
    JS_ObjectIsFunction(JSContext *cx, JSObject *obj);

    if (!obj)
        return JSVAL_NULL;

    JSValueMask32 mask = JS_ObjectIsFunction(NULL, obj) ? JSVAL_MASK32_FUNOBJ
                                                        : JSVAL_MASK32_NONFUNOBJ;
    return OBJECT_TO_JSVAL_IMPL(mask, obj).asBits;
}

static JS_ALWAYS_INLINE JSBool
JSVAL_IS_BOOLEAN(jsval v)
{
    jsval_layout l = { v };
    return l.s.mask32 == JSVAL_MASK32_BOOLEAN;
}

static JS_ALWAYS_INLINE JSBool
JSVAL_TO_BOOLEAN(jsval v)
{
    JS_ASSERT(JSVAL_IS_BOOLEAN(v));
    jsval_layout l = { v };
    return l.s.payload.boo;
}

static JS_ALWAYS_INLINE jsval
BOOLEAN_TO_JSVAL(JSBool b)
{
    return BOOLEAN_TO_JSVAL_IMPL(b).asBits;
}

static JS_ALWAYS_INLINE JSBool
JSVAL_IS_PRIMITIVE(jsval v)
{
    jsval_layout l = { v };
    return JSVAL_IS_PRIMITIVE_IMPL(l);
}

static JS_ALWAYS_INLINE JSBool
JSVAL_IS_GCTHING(jsval v)
{
    jsval_layout l = { v };
    return JSVAL_IS_GCTHING_IMPL(l);
}

static JS_ALWAYS_INLINE void *
JSVAL_TO_GCTHING(jsval v)
{
    JS_ASSERT(JSVAL_IS_GCTHING(v));
    jsval_layout l = { v };
    return JSVAL_TO_GCTHING_IMPL(l);
}

/* To be GC-safe, privates are tagged as doubles. */

static JS_ALWAYS_INLINE jsval
PRIVATE_TO_JSVAL(void *ptr)
{
    return PRIVATE_TO_JSVAL_IMPL(ptr).asBits;
}

static JS_ALWAYS_INLINE void *
JSVAL_TO_PRIVATE(jsval v)
{
    JS_ASSERT(JSVAL_IS_DOUBLE(v));
    jsval_layout l = { v };
    return JSVAL_TO_PRIVATE_IMPL(l);
}

/* Lock and unlock the GC thing held by a jsval. */
#define JSVAL_LOCK(cx,v)        (JSVAL_IS_GCTHING(v)                          \
                                 ? JS_LockGCThing(cx, JSVAL_TO_GCTHING(v))    \
                                 : JS_TRUE)
#define JSVAL_UNLOCK(cx,v)      (JSVAL_IS_GCTHING(v)                          \
                                 ? JS_UnlockGCThing(cx, JSVAL_TO_GCTHING(v))  \
                                 : JS_TRUE)

/* Property attributes, set in JSPropertySpec and passed to API functions. */
#define JSPROP_ENUMERATE        0x01    /* property is visible to for/in loop */
#define JSPROP_READONLY         0x02    /* not settable: assignment is no-op */
#define JSPROP_PERMANENT        0x04    /* property cannot be deleted */
#define JSPROP_GETTER           0x10    /* property holds getter function */
#define JSPROP_SETTER           0x20    /* property holds setter function */
#define JSPROP_SHARED           0x40    /* don't allocate a value slot for this
                                           property; don't copy the property on
                                           set of the same-named property in an
                                           object that delegates to a prototype
                                           containing this property */
#define JSPROP_INDEX            0x80    /* name is actually (jsint) index */

/* Function flags, set in JSFunctionSpec and passed to JS_NewFunction etc. */
#define JSFUN_LAMBDA            0x08    /* expressed, not declared, function */
#define JSFUN_GETTER            JSPROP_GETTER
#define JSFUN_SETTER            JSPROP_SETTER
#define JSFUN_BOUND_METHOD      0x40    /* bind this to fun->object's parent */
#define JSFUN_HEAVYWEIGHT       0x80    /* activation requires a Call object */

#define JSFUN_DISJOINT_FLAGS(f) ((f) & 0x0f)
#define JSFUN_GSFLAGS(f)        ((f) & (JSFUN_GETTER | JSFUN_SETTER))

#define JSFUN_GETTER_TEST(f)       ((f) & JSFUN_GETTER)
#define JSFUN_SETTER_TEST(f)       ((f) & JSFUN_SETTER)
#define JSFUN_BOUND_METHOD_TEST(f) ((f) & JSFUN_BOUND_METHOD)
#define JSFUN_HEAVYWEIGHT_TEST(f)  (!!((f) & JSFUN_HEAVYWEIGHT))

#define JSFUN_GSFLAG2ATTR(f)       JSFUN_GSFLAGS(f)

#define JSFUN_THISP_FLAGS(f)  (f)
#define JSFUN_THISP_TEST(f,t) ((f) & t)

#define JSFUN_THISP_STRING    0x0100    /* |this| may be a primitive string */
#define JSFUN_THISP_NUMBER    0x0200    /* |this| may be a primitive number */
#define JSFUN_THISP_BOOLEAN   0x0400    /* |this| may be a primitive boolean */
#define JSFUN_THISP_PRIMITIVE 0x0700    /* |this| may be any primitive value */

#define JSFUN_FAST_NATIVE     0x0800    /* JSFastNative needs no JSStackFrame */

#define JSFUN_FLAGS_MASK      0x0ff8    /* overlay JSFUN_* attributes --
                                           bits 12-15 are used internally to
                                           flag interpreted functions */

#define JSFUN_STUB_GSOPS      0x1000    /* use JS_PropertyStub getter/setter
                                           instead of defaulting to class gsops
                                           for property holding function */

/*
 * Re-use JSFUN_LAMBDA, which applies only to scripted functions, for use in
 * JSFunctionSpec arrays that specify generic native prototype methods, i.e.,
 * methods of a class prototype that are exposed as static methods taking an
 * extra leading argument: the generic |this| parameter.
 *
 * If you set this flag in a JSFunctionSpec struct's flags initializer, then
 * that struct must live at least as long as the native static method object
 * created due to this flag by JS_DefineFunctions or JS_InitClass.  Typically
 * JSFunctionSpec structs are allocated in static arrays.
 */
#define JSFUN_GENERIC_NATIVE    JSFUN_LAMBDA

/*
 * Microseconds since the epoch, midnight, January 1, 1970 UTC.  See the
 * comment in jstypes.h regarding safe int64 usage.
 */
extern JS_PUBLIC_API(int64)
JS_Now(void);

/* Don't want to export data, so provide accessors for non-inline jsvals. */
extern JS_PUBLIC_API(jsval)
JS_GetNaNValue(JSContext *cx);

extern JS_PUBLIC_API(jsval)
JS_GetNegativeInfinityValue(JSContext *cx);

extern JS_PUBLIC_API(jsval)
JS_GetPositiveInfinityValue(JSContext *cx);

extern JS_PUBLIC_API(jsval)
JS_GetEmptyStringValue(JSContext *cx);

/*
 * Format is a string of the following characters (spaces are insignificant),
 * specifying the tabulated type conversions:
 *
 *   b      JSBool          Boolean
 *   c      uint16/jschar   ECMA uint16, Unicode char
 *   i      int32           ECMA int32
 *   u      uint32          ECMA uint32
 *   j      int32           Rounded int32 (coordinate)
 *   d      jsdouble        IEEE double
 *   I      jsdouble        Integral IEEE double
 *   s      char *          C string
 *   S      JSString *      Unicode string, accessed by a JSString pointer
 *   W      jschar *        Unicode character vector, 0-terminated (W for wide)
 *   o      JSObject *      Object reference
 *   f      JSFunction *    Function private
 *   v      jsval           Argument value (no conversion)
 *   *      N/A             Skip this argument (no vararg)
 *   /      N/A             End of required arguments
 *
 * The variable argument list after format must consist of &b, &c, &s, e.g.,
 * where those variables have the types given above.  For the pointer types
 * char *, JSString *, and JSObject *, the pointed-at memory returned belongs
 * to the JS runtime, not to the calling native code.  The runtime promises
 * to keep this memory valid so long as argv refers to allocated stack space
 * (so long as the native function is active).
 *
 * Fewer arguments than format specifies may be passed only if there is a /
 * in format after the last required argument specifier and argc is at least
 * the number of required arguments.  More arguments than format specifies
 * may be passed without error; it is up to the caller to deal with trailing
 * unconverted arguments.
 */
extern JS_PUBLIC_API(JSBool)
JS_ConvertArguments(JSContext *cx, uintN argc, jsval *argv, const char *format,
                    ...);

#ifdef va_start
extern JS_PUBLIC_API(JSBool)
JS_ConvertArgumentsVA(JSContext *cx, uintN argc, jsval *argv,
                      const char *format, va_list ap);
#endif

#ifdef JS_ARGUMENT_FORMATTER_DEFINED

/*
 * Add and remove a format string handler for JS_{Convert,Push}Arguments{,VA}.
 * The handler function has this signature (see jspubtd.h):
 *
 *   JSBool MyArgumentFormatter(JSContext *cx, const char *format,
 *                              JSBool fromJS, jsval **vpp, va_list *app);
 *
 * It should return true on success, and return false after reporting an error
 * or detecting an already-reported error.
 *
 * For a given format string, for example "AA", the formatter is called from
 * JS_ConvertArgumentsVA like so:
 *
 *   formatter(cx, "AA...", JS_TRUE, &sp, &ap);
 *
 * sp points into the arguments array on the JS stack, while ap points into
 * the stdarg.h va_list on the C stack.  The JS_TRUE passed for fromJS tells
 * the formatter to convert zero or more jsvals at sp to zero or more C values
 * accessed via pointers-to-values at ap, updating both sp (via *vpp) and ap
 * (via *app) to point past the converted arguments and their result pointers
 * on the C stack.
 *
 * When called from JS_PushArgumentsVA, the formatter is invoked thus:
 *
 *   formatter(cx, "AA...", JS_FALSE, &sp, &ap);
 *
 * where JS_FALSE for fromJS means to wrap the C values at ap according to the
 * format specifier and store them at sp, updating ap and sp appropriately.
 *
 * The "..." after "AA" is the rest of the format string that was passed into
 * JS_{Convert,Push}Arguments{,VA}.  The actual format trailing substring used
 * in each Convert or PushArguments call is passed to the formatter, so that
 * one such function may implement several formats, in order to share code.
 *
 * Remove just forgets about any handler associated with format.  Add does not
 * copy format, it points at the string storage allocated by the caller, which
 * is typically a string constant.  If format is in dynamic storage, it is up
 * to the caller to keep the string alive until Remove is called.
 */
extern JS_PUBLIC_API(JSBool)
JS_AddArgumentFormatter(JSContext *cx, const char *format,
                        JSArgumentFormatter formatter);

extern JS_PUBLIC_API(void)
JS_RemoveArgumentFormatter(JSContext *cx, const char *format);

#endif /* JS_ARGUMENT_FORMATTER_DEFINED */

extern JS_PUBLIC_API(JSBool)
JS_ConvertValue(JSContext *cx, jsval v, JSType type, jsval *vp);

extern JS_PUBLIC_API(JSBool)
JS_ValueToObject(JSContext *cx, jsval v, JSObject **objp);

extern JS_PUBLIC_API(JSFunction *)
JS_ValueToFunction(JSContext *cx, jsval v);

extern JS_PUBLIC_API(JSFunction *)
JS_ValueToConstructor(JSContext *cx, jsval v);

extern JS_PUBLIC_API(JSString *)
JS_ValueToString(JSContext *cx, jsval v);

extern JS_PUBLIC_API(JSString *)
JS_ValueToSource(JSContext *cx, jsval v);

extern JS_PUBLIC_API(JSBool)
JS_ValueToNumber(JSContext *cx, jsval v, jsdouble *dp);

extern JS_PUBLIC_API(JSBool)
JS_DoubleIsInt32(jsdouble d, jsint *ip);

/*
 * Convert a value to a number, then to an int32, according to the ECMA rules
 * for ToInt32.
 */
extern JS_PUBLIC_API(JSBool)
JS_ValueToECMAInt32(JSContext *cx, jsval v, int32 *ip);

/*
 * Convert a value to a number, then to a uint32, according to the ECMA rules
 * for ToUint32.
 */
extern JS_PUBLIC_API(JSBool)
JS_ValueToECMAUint32(JSContext *cx, jsval v, uint32 *ip);

/*
 * Convert a value to a number, then to an int32 if it fits by rounding to
 * nearest; but failing with an error report if the double is out of range
 * or unordered.
 */
extern JS_PUBLIC_API(JSBool)
JS_ValueToInt32(JSContext *cx, jsval v, int32 *ip);

/*
 * ECMA ToUint16, for mapping a jsval to a Unicode point.
 */
extern JS_PUBLIC_API(JSBool)
JS_ValueToUint16(JSContext *cx, jsval v, uint16 *ip);

extern JS_PUBLIC_API(JSBool)
JS_ValueToBoolean(JSContext *cx, jsval v, JSBool *bp);

extern JS_PUBLIC_API(JSType)
JS_TypeOfValue(JSContext *cx, jsval v);

extern JS_PUBLIC_API(const char *)
JS_GetTypeName(JSContext *cx, JSType type);

extern JS_PUBLIC_API(JSBool)
JS_StrictlyEqual(JSContext *cx, jsval v1, jsval v2);

extern JS_PUBLIC_API(JSBool)
JS_SameValue(JSContext *cx, jsval v1, jsval v2);

/************************************************************************/

/*
 * Initialization, locking, contexts, and memory allocation.
 *
 * It is important that the first runtime and first context be created in a
 * single-threaded fashion, otherwise the behavior of the library is undefined.
 * See: http://developer.mozilla.org/en/docs/Category:JSAPI_Reference
 */
#define JS_NewRuntime       JS_Init
#define JS_DestroyRuntime   JS_Finish
#define JS_LockRuntime      JS_Lock
#define JS_UnlockRuntime    JS_Unlock

extern JS_PUBLIC_API(JSRuntime *)
JS_NewRuntime(uint32 maxbytes);

extern JS_PUBLIC_API(void)
JS_CommenceRuntimeShutDown(JSRuntime *rt);

extern JS_PUBLIC_API(void)
JS_DestroyRuntime(JSRuntime *rt);

extern JS_PUBLIC_API(void)
JS_ShutDown(void);

JS_PUBLIC_API(void *)
JS_GetRuntimePrivate(JSRuntime *rt);

JS_PUBLIC_API(void)
JS_SetRuntimePrivate(JSRuntime *rt, void *data);

extern JS_PUBLIC_API(void)
JS_BeginRequest(JSContext *cx);

extern JS_PUBLIC_API(void)
JS_EndRequest(JSContext *cx);

/* Yield to pending GC operations, regardless of request depth */
extern JS_PUBLIC_API(void)
JS_YieldRequest(JSContext *cx);

extern JS_PUBLIC_API(jsrefcount)
JS_SuspendRequest(JSContext *cx);

extern JS_PUBLIC_API(void)
JS_ResumeRequest(JSContext *cx, jsrefcount saveDepth);

extern JS_PUBLIC_API(void)
JS_TransferRequest(JSContext *cx, JSContext *another);

#ifdef __cplusplus
JS_END_EXTERN_C

class JSAutoRequest {
  public:
    JSAutoRequest(JSContext *cx JS_GUARD_OBJECT_NOTIFIER_PARAM)
        : mContext(cx), mSaveDepth(0) {
        JS_GUARD_OBJECT_NOTIFIER_INIT;
        JS_BeginRequest(mContext);
    }
    ~JSAutoRequest() {
        JS_EndRequest(mContext);
    }

    void suspend() {
        mSaveDepth = JS_SuspendRequest(mContext);
    }
    void resume() {
        JS_ResumeRequest(mContext, mSaveDepth);
    }

  protected:
    JSContext *mContext;
    jsrefcount mSaveDepth;
    JS_DECL_USE_GUARD_OBJECT_NOTIFIER

#if 0
  private:
    static void *operator new(size_t) CPP_THROW_NEW { return 0; };
    static void operator delete(void *, size_t) { };
#endif
};

class JSAutoSuspendRequest {
  public:
    JSAutoSuspendRequest(JSContext *cx JS_GUARD_OBJECT_NOTIFIER_PARAM)
        : mContext(cx), mSaveDepth(0) {
        JS_GUARD_OBJECT_NOTIFIER_INIT;
        if (mContext) {
            mSaveDepth = JS_SuspendRequest(mContext);
        }
    }
    ~JSAutoSuspendRequest() {
        resume();
    }

    void resume() {
        if (mContext) {
            JS_ResumeRequest(mContext, mSaveDepth);
            mContext = 0;
        }
    }

  protected:
    JSContext *mContext;
    jsrefcount mSaveDepth;
    JS_DECL_USE_GUARD_OBJECT_NOTIFIER

#if 0
  private:
    static void *operator new(size_t) CPP_THROW_NEW { return 0; };
    static void operator delete(void *, size_t) { };
#endif
};

class JSAutoTransferRequest
{
  public:
    JSAutoTransferRequest(JSContext* cx1, JSContext* cx2)
        : cx1(cx1), cx2(cx2) {
        if(cx1 != cx2)
            JS_TransferRequest(cx1, cx2);
    }
    ~JSAutoTransferRequest() {
        if(cx1 != cx2)
            JS_TransferRequest(cx2, cx1);
    }
  private:
    JSContext* const cx1;
    JSContext* const cx2;

    /* Not copyable. */
    JSAutoTransferRequest(JSAutoTransferRequest &);
    void operator =(JSAutoTransferRequest&);
};

JS_BEGIN_EXTERN_C
#endif

extern JS_PUBLIC_API(void)
JS_Lock(JSRuntime *rt);

extern JS_PUBLIC_API(void)
JS_Unlock(JSRuntime *rt);

extern JS_PUBLIC_API(JSContextCallback)
JS_SetContextCallback(JSRuntime *rt, JSContextCallback cxCallback);

extern JS_PUBLIC_API(JSContext *)
JS_NewContext(JSRuntime *rt, size_t stackChunkSize);

extern JS_PUBLIC_API(void)
JS_DestroyContext(JSContext *cx);

extern JS_PUBLIC_API(void)
JS_DestroyContextNoGC(JSContext *cx);

extern JS_PUBLIC_API(void)
JS_DestroyContextMaybeGC(JSContext *cx);

extern JS_PUBLIC_API(void *)
JS_GetContextPrivate(JSContext *cx);

extern JS_PUBLIC_API(void)
JS_SetContextPrivate(JSContext *cx, void *data);

extern JS_PUBLIC_API(JSRuntime *)
JS_GetRuntime(JSContext *cx);

extern JS_PUBLIC_API(JSContext *)
JS_ContextIterator(JSRuntime *rt, JSContext **iterp);

extern JS_PUBLIC_API(JSVersion)
JS_GetVersion(JSContext *cx);

extern JS_PUBLIC_API(JSVersion)
JS_SetVersion(JSContext *cx, JSVersion version);

extern JS_PUBLIC_API(const char *)
JS_VersionToString(JSVersion version);

extern JS_PUBLIC_API(JSVersion)
JS_StringToVersion(const char *string);

/*
 * JS options are orthogonal to version, and may be freely composed with one
 * another as well as with version.
 *
 * JSOPTION_VAROBJFIX is recommended -- see the comments associated with the
 * prototypes for JS_ExecuteScript, JS_EvaluateScript, etc.
 */
#define JSOPTION_STRICT         JS_BIT(0)       /* warn on dubious practice */
#define JSOPTION_WERROR         JS_BIT(1)       /* convert warning to error */
#define JSOPTION_VAROBJFIX      JS_BIT(2)       /* make JS_EvaluateScript use
                                                   the last object on its 'obj'
                                                   param's scope chain as the
                                                   ECMA 'variables object' */
#define JSOPTION_PRIVATE_IS_NSISUPPORTS \
                                JS_BIT(3)       /* context private data points
                                                   to an nsISupports subclass */
#define JSOPTION_COMPILE_N_GO   JS_BIT(4)       /* caller of JS_Compile*Script
                                                   promises to execute compiled
                                                   script once only; enables
                                                   compile-time scope chain
                                                   resolution of consts. */
#define JSOPTION_ATLINE         JS_BIT(5)       /* //@line number ["filename"]
                                                   option supported for the
                                                   XUL preprocessor and kindred
                                                   beasts. */
#define JSOPTION_XML            JS_BIT(6)       /* EMCAScript for XML support:
                                                   parse <!-- --> as a token,
                                                   not backward compatible with
                                                   the comment-hiding hack used
                                                   in HTML script tags. */
#define JSOPTION_DONT_REPORT_UNCAUGHT \
                                JS_BIT(8)       /* When returning from the
                                                   outermost API call, prevent
                                                   uncaught exceptions from
                                                   being converted to error
                                                   reports */

#define JSOPTION_RELIMIT        JS_BIT(9)       /* Throw exception on any
                                                   regular expression which
                                                   backtracks more than n^3
                                                   times, where n is length
                                                   of the input string */
#define JSOPTION_ANONFUNFIX     JS_BIT(10)      /* Disallow function () {} in
                                                   statement context per
                                                   ECMA-262 Edition 3. */

#define JSOPTION_JIT            JS_BIT(11)      /* Enable JIT compilation. */

#define JSOPTION_NO_SCRIPT_RVAL JS_BIT(12)      /* A promise to the compiler
                                                   that a null rval out-param
                                                   will be passed to each call
                                                   to JS_ExecuteScript. */
#define JSOPTION_UNROOTED_GLOBAL JS_BIT(13)     /* The GC will not root the
                                                   contexts' global objects
                                                   (see JS_GetGlobalObject),
                                                   leaving that up to the
                                                   embedding. */

extern JS_PUBLIC_API(uint32)
JS_GetOptions(JSContext *cx);

extern JS_PUBLIC_API(uint32)
JS_SetOptions(JSContext *cx, uint32 options);

extern JS_PUBLIC_API(uint32)
JS_ToggleOptions(JSContext *cx, uint32 options);

extern JS_PUBLIC_API(const char *)
JS_GetImplementationVersion(void);

extern JS_PUBLIC_API(JSObject *)
JS_GetGlobalObject(JSContext *cx);

extern JS_PUBLIC_API(void)
JS_SetGlobalObject(JSContext *cx, JSObject *obj);

/*
 * Initialize standard JS class constructors, prototypes, and any top-level
 * functions and constants associated with the standard classes (e.g. isNaN
 * for Number).
 *
 * NB: This sets cx's global object to obj if it was null.
 */
extern JS_PUBLIC_API(JSBool)
JS_InitStandardClasses(JSContext *cx, JSObject *obj);

/*
 * Resolve id, which must contain either a string or an int, to a standard
 * class name in obj if possible, defining the class's constructor and/or
 * prototype and storing true in *resolved.  If id does not name a standard
 * class or a top-level property induced by initializing a standard class,
 * store false in *resolved and just return true.  Return false on error,
 * as usual for JSBool result-typed API entry points.
 *
 * This API can be called directly from a global object class's resolve op,
 * to define standard classes lazily.  The class's enumerate op should call
 * JS_EnumerateStandardClasses(cx, obj), to define eagerly during for..in
 * loops any classes not yet resolved lazily.
 */
extern JS_PUBLIC_API(JSBool)
JS_ResolveStandardClass(JSContext *cx, JSObject *obj, jsval id,
                        JSBool *resolved);

extern JS_PUBLIC_API(JSBool)
JS_EnumerateStandardClasses(JSContext *cx, JSObject *obj);

/*
 * Enumerate any already-resolved standard class ids into ida, or into a new
 * JSIdArray if ida is null.  Return the augmented array on success, null on
 * failure with ida (if it was non-null on entry) destroyed.
 */
extern JS_PUBLIC_API(JSIdArray *)
JS_EnumerateResolvedStandardClasses(JSContext *cx, JSObject *obj,
                                    JSIdArray *ida);

extern JS_PUBLIC_API(JSBool)
JS_GetClassObject(JSContext *cx, JSObject *obj, JSProtoKey key,
                  JSObject **objp);

extern JS_PUBLIC_API(JSObject *)
JS_GetScopeChain(JSContext *cx);

extern JS_PUBLIC_API(JSObject *)
JS_GetGlobalForObject(JSContext *cx, JSObject *obj);

#ifdef JS_HAS_CTYPES
/*
 * Initialize the 'ctypes' object on a global variable 'obj'. The 'ctypes'
 * object will be sealed.
 */
extern JS_PUBLIC_API(JSBool)
JS_InitCTypesClass(JSContext *cx, JSObject *global);
#endif

/*
 * Macros to hide interpreter stack layout details from a JSFastNative using
 * its jsval *vp parameter. The stack layout underlying invocation can't change
 * without breaking source and binary compatibility (argv[-2] is well-known to
 * be the callee jsval, and argv[-1] is as well known to be |this|).
 *
 * Note well: However, argv[-1] may be JSVAL_NULL where with slow natives it
 * is the global object, so embeddings implementing fast natives *must* call
 * JS_THIS or JS_THIS_OBJECT and test for failure indicated by a null return,
 * which should propagate as a false return from native functions and hooks.
 *
 * To reduce boilerplace checks, JS_InstanceOf and JS_GetInstancePrivate now
 * handle a null obj parameter by returning false (throwing a TypeError if
 * given non-null argv), so most native functions that type-check their |this|
 * parameter need not add null checking.
 *
 * NB: there is an anti-dependency between JS_CALLEE and JS_SET_RVAL: native
 * methods that may inspect their callee must defer setting their return value
 * until after any such possible inspection. Otherwise the return value will be
 * inspected instead of the callee function object.
 *
 * WARNING: These are not (yet) mandatory macros, but new code outside of the
 * engine should use them. In the Mozilla 2.0 milestone their definitions may
 * change incompatibly.
 */
#define JS_CALLEE(cx,vp)        ((vp)[0])
#define JS_ARGV_CALLEE(argv)    ((argv)[-2])
#define JS_THIS(cx,vp)          JS_ComputeThis(cx, vp)
#define JS_THIS_OBJECT(cx,vp)   (JSVAL_TO_OBJECT(JS_THIS(cx,vp)))
#define JS_ARGV(cx,vp)          ((vp) + 2)
#define JS_RVAL(cx,vp)          (*(vp))
#define JS_SET_RVAL(cx,vp,v)    (*(vp) = (v))

extern JS_PUBLIC_API(jsval)
JS_ComputeThis(JSContext *cx, jsval *vp);

extern JS_PUBLIC_API(void *)
JS_malloc(JSContext *cx, size_t nbytes);

extern JS_PUBLIC_API(void *)
JS_realloc(JSContext *cx, void *p, size_t nbytes);

extern JS_PUBLIC_API(void)
JS_free(JSContext *cx, void *p);

extern JS_PUBLIC_API(void)
JS_updateMallocCounter(JSContext *cx, size_t nbytes);

extern JS_PUBLIC_API(char *)
JS_strdup(JSContext *cx, const char *s);

extern JS_PUBLIC_API(JSBool)
JS_NewNumberValue(JSContext *cx, jsdouble d, jsval *rval);

/*
 * A GC root is a pointer to a jsval, JSObject *, or JSString * that itself
 * points into the GC heap. JS_AddValueRoot takes pointers to jsvals and
 * JS_AddGCThingRoot takes pointers to a JSObject * or JString *.
 *
 * Note that, since JS_Add*Root stores the address of a (jsval, JSString *, or
 * JSObject *) variable, that variable must be alive until JS_RemoveRoot is
 * called to remove that variable. For example, after writing:
 *
 *   jsval v;
 *   JS_AddNamedRootedValue(cx, &v, "name");
 *
 * the caller must perform
 *
 *   JS_RemoveRootedValue(cx, &v);
 *
 * before 'v' goes out of scope.
 *
 * Also, use JS_AddNamedRoot(cx, &structPtr->memberObj, "structPtr->memberObj")
 * in preference to JS_AddRoot(cx, &structPtr->memberObj), in order to identify
 * roots by their source callsites.  This way, you can find the callsite while
 * debugging if you should fail to do JS_RemoveRoot(cx, &structPtr->memberObj)
 * before freeing structPtr's memory.
 */
extern JS_PUBLIC_API(JSBool)
JS_AddValueRoot(JSContext *cx, jsval *vp);

extern JS_PUBLIC_API(JSBool)
JS_AddStringRoot(JSContext *cx, JSString **rp);

extern JS_PUBLIC_API(JSBool)
JS_AddObjectRoot(JSContext *cx, JSObject **rp);

#ifdef NAME_ALL_GC_ROOTS
#define JS_DEFINE_TO_TOKEN(def) #def
#define JS_DEFINE_TO_STRING(def) JS_DEFINE_TO_TOKEN(def)
#define JS_AddValueRoot(cx,vp) JS_AddNamedValueRoot((cx), (vp), (__FILE__ ":" JS_TOKEN_TO_STRING(__LINE__))
#define JS_AddStringRoot(cx,rp) JS_AddNamedStringRoot((cx), (rp), (__FILE__ ":" JS_TOKEN_TO_STRING(__LINE__))
#define JS_AddObjectRoot(cx,rp) JS_AddNamedObjectRoot((cx), (rp), (__FILE__ ":" JS_TOKEN_TO_STRING(__LINE__))
#endif

extern JS_PUBLIC_API(JSBool)
JS_AddNamedValueRoot(JSContext *cx, jsval *vp, const char *name);

extern JS_PUBLIC_API(JSBool)
JS_AddNamedStringRoot(JSContext *cx, JSString **rp, const char *name);

extern JS_PUBLIC_API(JSBool)
JS_AddNamedObjectRoot(JSContext *cx, JSObject **rp, const char *name);

extern JS_PUBLIC_API(JSBool)
JS_AddNamedValueRootRT(JSRuntime *rt, jsval *vp, const char *name);

extern JS_PUBLIC_API(JSBool)
JS_AddNamedStringRootRT(JSRuntime *rt, JSString **rp, const char *name);

extern JS_PUBLIC_API(JSBool)
JS_AddNamedObjectRootRT(JSRuntime *rt, JSObject **rp, const char *name);

extern JS_PUBLIC_API(JSBool)
JS_RemoveValueRoot(JSContext *cx, jsval *vp);

extern JS_PUBLIC_API(JSBool)
JS_RemoveStringRoot(JSContext *cx, JSString **rp);

extern JS_PUBLIC_API(JSBool)
JS_RemoveObjectRoot(JSContext *cx, JSObject **rp);

extern JS_PUBLIC_API(JSBool)
JS_RemoveValueRootRT(JSRuntime *rt, jsval *vp);

extern JS_PUBLIC_API(JSBool)
JS_RemoveStringRootRT(JSRuntime *rt, JSString **rp);

extern JS_PUBLIC_API(JSBool)
JS_RemoveObjectRootRT(JSRuntime *rt, JSObject **rp);

/*
 * The last GC thing of each type (object, string, double, external string
 * types) created on a given context is kept alive until another thing of the
 * same type is created, using a newborn root in the context.  These newborn
 * roots help native code protect newly-created GC-things from GC invocations
 * activated before those things can be rooted using local or global roots.
 *
 * However, the newborn roots can also entrain great gobs of garbage, so the
 * JS_GC entry point clears them for the context on which GC is being forced.
 * Embeddings may need to do likewise for all contexts.
 *
 * See the scoped local root API immediately below for a better way to manage
 * newborns in cases where native hooks (functions, getters, setters, etc.)
 * create many GC-things, potentially without connecting them to predefined
 * local roots such as *rval or argv[i] in an active native function.  Using
 * JS_EnterLocalRootScope disables updating of the context's per-gc-thing-type
 * newborn roots, until control flow unwinds and leaves the outermost nesting
 * local root scope.
 */
extern JS_PUBLIC_API(void)
JS_ClearNewbornRoots(JSContext *cx);

/*
 * Scoped local root management allows native functions, getter/setters, etc.
 * to avoid worrying about the newborn root pigeon-holes, overloading local
 * roots allocated in argv and *rval, or ending up having to call JS_Add*Root
 * and JS_RemoveRoot to manage global roots temporarily.
 *
 * Instead, calling JS_EnterLocalRootScope and JS_LeaveLocalRootScope around
 * the body of the native hook causes the engine to allocate a local root for
 * each newborn created in between the two API calls, using a local root stack
 * associated with cx.  For example:
 *
 *    JSBool
 *    my_GetProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
 *    {
 *        JSBool ok;
 *
 *        if (!JS_EnterLocalRootScope(cx))
 *            return JS_FALSE;
 *        ok = my_GetPropertyBody(cx, obj, id, vp);
 *        JS_LeaveLocalRootScope(cx);
 *        return ok;
 *    }
 *
 * NB: JS_LeaveLocalRootScope must be called once for every prior successful
 * call to JS_EnterLocalRootScope.  If JS_EnterLocalRootScope fails, you must
 * not make the matching JS_LeaveLocalRootScope call.
 *
 * JS_LeaveLocalRootScopeWithResult(cx, rval) is an alternative way to leave
 * a local root scope that protects a result or return value, by effectively
 * pushing it in the caller's local root scope.
 *
 * In case a native hook allocates many objects or other GC-things, but the
 * native protects some of those GC-things by storing them as property values
 * in an object that is itself protected, the hook can call JS_ForgetLocalRoot
 * to free the local root automatically pushed for the now-protected GC-thing.
 *
 * JS_ForgetLocalRoot works on any GC-thing allocated in the current local
 * root scope, but it's more time-efficient when called on references to more
 * recently created GC-things.  Calling it successively on other than the most
 * recently allocated GC-thing will tend to average the time inefficiency, and
 * may risk O(n^2) growth rate, but in any event, you shouldn't allocate too
 * many local roots if you can root as you go (build a tree of objects from
 * the top down, forgetting each latest-allocated GC-thing immediately upon
 * linking it to its parent).
 */
extern JS_PUBLIC_API(JSBool)
JS_EnterLocalRootScope(JSContext *cx);

extern JS_PUBLIC_API(void)
JS_LeaveLocalRootScope(JSContext *cx);

extern JS_PUBLIC_API(void)
JS_LeaveLocalRootScopeWithResult(JSContext *cx, jsval rval);

extern JS_PUBLIC_API(void)
JS_ForgetLocalRoot(JSContext *cx, void *thing);

#ifdef __cplusplus
JS_END_EXTERN_C

class JSAutoLocalRootScope {
  public:
    JSAutoLocalRootScope(JSContext *cx JS_GUARD_OBJECT_NOTIFIER_PARAM)
        : mContext(cx) {
        JS_GUARD_OBJECT_NOTIFIER_INIT;
        JS_EnterLocalRootScope(mContext);
    }
    ~JSAutoLocalRootScope() {
        JS_LeaveLocalRootScope(mContext);
    }

    void forget(void *thing) {
        JS_ForgetLocalRoot(mContext, thing);
    }

  protected:
    JSContext *mContext;
    JS_DECL_USE_GUARD_OBJECT_NOTIFIER

#if 0
  private:
    static void *operator new(size_t) CPP_THROW_NEW { return 0; };
    static void operator delete(void *, size_t) { };
#endif
};

JS_BEGIN_EXTERN_C
#endif

typedef enum JSGCRootType {
    JS_GC_ROOT_VALUE_PTR,
    JS_GC_ROOT_GCTHING_PTR
} JSGCRootType;

#ifdef DEBUG
extern JS_PUBLIC_API(void)
JS_DumpNamedRoots(JSRuntime *rt,
                  void (*dump)(const char *name, void *rp, JSGCRootType type, void *data),
                  void *data);
#endif

/*
 * Call JS_MapGCRoots to map the GC's roots table using map(rp, name, data).
 * The root is pointed at by rp; if the root is unnamed, name is null; data is
 * supplied from the third parameter to JS_MapGCRoots.
 *
 * The map function should return JS_MAP_GCROOT_REMOVE to cause the currently
 * enumerated root to be removed.  To stop enumeration, set JS_MAP_GCROOT_STOP
 * in the return value.  To keep on mapping, return JS_MAP_GCROOT_NEXT.  These
 * constants are flags; you can OR them together.
 *
 * This function acquires and releases rt's GC lock around the mapping of the
 * roots table, so the map function should run to completion in as few cycles
 * as possible.  Of course, map cannot call JS_GC, JS_MaybeGC, JS_BeginRequest,
 * or any JS API entry point that acquires locks, without double-tripping or
 * deadlocking on the GC lock.
 *
 * The JSGCRootType parameter indicates whether rp is a pointer to a Value
 * (which is obtained by '(Value *)rp') or a pointer to a GC-thing pointer
 * (which is obtained by '(void **)rp').
 *
 * JS_MapGCRoots returns the count of roots that were successfully mapped.
 */
#define JS_MAP_GCROOT_NEXT      0       /* continue mapping entries */
#define JS_MAP_GCROOT_STOP      1       /* stop mapping entries */
#define JS_MAP_GCROOT_REMOVE    2       /* remove and free the current entry */

typedef intN
(* JSGCRootMapFun)(void *rp, JSGCRootType type, const char *name, void *data);

extern JS_PUBLIC_API(uint32)
JS_MapGCRoots(JSRuntime *rt, JSGCRootMapFun map, void *data);

extern JS_PUBLIC_API(JSBool)
JS_LockGCThing(JSContext *cx, void *thing);

extern JS_PUBLIC_API(JSBool)
JS_LockGCThingRT(JSRuntime *rt, void *thing);

extern JS_PUBLIC_API(JSBool)
JS_UnlockGCThing(JSContext *cx, void *thing);

extern JS_PUBLIC_API(JSBool)
JS_UnlockGCThingRT(JSRuntime *rt, void *thing);

/*
 * Register externally maintained GC roots.
 *
 * traceOp: the trace operation. For each root the implementation should call
 *          JS_CallTracer whenever the root contains a traceable thing.
 * data:    the data argument to pass to each invocation of traceOp.
 */
extern JS_PUBLIC_API(void)
JS_SetExtraGCRoots(JSRuntime *rt, JSTraceDataOp traceOp, void *data);

/*
 * For implementors of JSMarkOp. All new code should implement JSTraceOp
 * instead.
 */
extern JS_PUBLIC_API(void)
JS_MarkGCThing(JSContext *cx, jsval v, const char *name, void *arg);

/*
 * JS_CallTracer API and related macros for implementors of JSTraceOp, to
 * enumerate all references to traceable things reachable via a property or
 * other strong ref identified for debugging purposes by name or index or
 * a naming callback.
 *
 * By definition references to traceable things include non-null pointers
 * to JSObject, JSString and jsdouble and corresponding jsvals.
 *
 * See the JSTraceOp typedef in jspubtd.h.
 */

/* Trace kinds to pass to JS_Tracing. */
#define JSTRACE_OBJECT  0
#define JSTRACE_STRING  1

/* Engine private; not the trace kind of any jsval. */
#define JSTRACE_DOUBLE  2

/*
 * Use the following macros to check if a particular jsval is a traceable
 * thing and to extract the thing and its kind to pass to JS_CallTracer.
 */
static JS_ALWAYS_INLINE JSBool
JSVAL_IS_TRACEABLE(jsval v)
{
    return JSVAL_IS_GCTHING(v);
}

static JS_ALWAYS_INLINE void *
JSVAL_TO_TRACEABLE(jsval v)
{
    return JSVAL_TO_GCTHING(v);
}

static JS_ALWAYS_INLINE uint32
JSVAL_TRACE_KIND(jsval v)
{
    JS_ASSERT(JSVAL_IS_GCTHING(v));
    jsval_layout l = { v };
    return JSVAL_TRACE_KIND_IMPL(l);
}

struct JSTracer {
    JSContext           *context;
    JSTraceCallback     callback;
    JSTraceNamePrinter  debugPrinter;
    const void          *debugPrintArg;
    size_t              debugPrintIndex;
};

/*
 * The method to call on each reference to a traceable thing stored in a
 * particular JSObject or other runtime structure. With DEBUG defined the
 * caller before calling JS_CallTracer must initialize JSTracer fields
 * describing the reference using the macros below.
 */
extern JS_PUBLIC_API(void)
JS_CallTracer(JSTracer *trc, void *thing, uint32 kind);

/*
 * Set debugging information about a reference to a traceable thing to prepare
 * for the following call to JS_CallTracer.
 *
 * When printer is null, arg must be const char * or char * C string naming
 * the reference and index must be either (size_t)-1 indicating that the name
 * alone describes the reference or it must be an index into some array vector
 * that stores the reference.
 *
 * When printer callback is not null, the arg and index arguments are
 * available to the callback as debugPrinterArg and debugPrintIndex fields
 * of JSTracer.
 *
 * The storage for name or callback's arguments needs to live only until
 * the following call to JS_CallTracer returns.
 */
#ifdef DEBUG
# define JS_SET_TRACING_DETAILS(trc, printer, arg, index)                     \
    JS_BEGIN_MACRO                                                            \
        (trc)->debugPrinter = (printer);                                      \
        (trc)->debugPrintArg = (arg);                                         \
        (trc)->debugPrintIndex = (index);                                     \
    JS_END_MACRO
#else
# define JS_SET_TRACING_DETAILS(trc, printer, arg, index)                     \
    JS_BEGIN_MACRO                                                            \
    JS_END_MACRO
#endif

/*
 * Convenience macro to describe the argument of JS_CallTracer using C string
 * and index.
 */
# define JS_SET_TRACING_INDEX(trc, name, index)                               \
    JS_SET_TRACING_DETAILS(trc, NULL, name, index)

/*
 * Convenience macro to describe the argument of JS_CallTracer using C string.
 */
# define JS_SET_TRACING_NAME(trc, name)                                       \
    JS_SET_TRACING_DETAILS(trc, NULL, name, (size_t)-1)

/*
 * Convenience macro to invoke JS_CallTracer using C string as the name for
 * the reference to a traceable thing.
 */
# define JS_CALL_TRACER(trc, thing, kind, name)                               \
    JS_BEGIN_MACRO                                                            \
        JS_SET_TRACING_NAME(trc, name);                                       \
        JS_CallTracer((trc), (thing), (kind));                                \
    JS_END_MACRO

/*
 * Convenience macros to invoke JS_CallTracer when jsval represents a
 * reference to a traceable thing.
 */
#define JS_CALL_VALUE_TRACER(trc, val, name)                                  \
    JS_BEGIN_MACRO                                                            \
        if (JSVAL_IS_TRACEABLE(val)) {                                        \
            JS_CALL_TRACER((trc), JSVAL_TO_GCTHING(val),                      \
                           JSVAL_TRACE_KIND(val), name);                      \
        }                                                                     \
    JS_END_MACRO

#define JS_CALL_OBJECT_TRACER(trc, object, name)                              \
    JS_BEGIN_MACRO                                                            \
        JSObject *obj_ = (object);                                            \
        JS_ASSERT(obj_);                                                      \
        JS_CALL_TRACER((trc), obj_, JSTRACE_OBJECT, name);                    \
    JS_END_MACRO

#define JS_CALL_STRING_TRACER(trc, string, name)                              \
    JS_BEGIN_MACRO                                                            \
        JSString *str_ = (string);                                            \
        JS_ASSERT(str_);                                                      \
        JS_CALL_TRACER((trc), str_, JSTRACE_STRING, name);                    \
    JS_END_MACRO

/*
 * API for JSTraceCallback implementations.
 */
# define JS_TRACER_INIT(trc, cx_, callback_)                                  \
    JS_BEGIN_MACRO                                                            \
        (trc)->context = (cx_);                                               \
        (trc)->callback = (callback_);                                        \
        (trc)->debugPrinter = NULL;                                           \
        (trc)->debugPrintArg = NULL;                                          \
        (trc)->debugPrintIndex = (size_t)-1;                                  \
    JS_END_MACRO

extern JS_PUBLIC_API(void)
JS_TraceChildren(JSTracer *trc, void *thing, uint32 kind);

extern JS_PUBLIC_API(void)
JS_TraceRuntime(JSTracer *trc);

#ifdef DEBUG

extern JS_PUBLIC_API(void)
JS_PrintTraceThingInfo(char *buf, size_t bufsize, JSTracer *trc,
                       void *thing, uint32 kind, JSBool includeDetails);

/*
 * DEBUG-only method to dump the object graph of heap-allocated things.
 *
 * fp:              file for the dump output.
 * start:           when non-null, dump only things reachable from start
 *                  thing. Otherwise dump all things reachable from the
 *                  runtime roots.
 * startKind:       trace kind of start if start is not null. Must be 0 when
 *                  start is null.
 * thingToFind:     dump only paths in the object graph leading to thingToFind
 *                  when non-null.
 * maxDepth:        the upper bound on the number of edges to descend from the
 *                  graph roots.
 * thingToIgnore:   thing to ignore during the graph traversal when non-null.
 */
extern JS_PUBLIC_API(JSBool)
JS_DumpHeap(JSContext *cx, FILE *fp, void* startThing, uint32 startKind,
            void *thingToFind, size_t maxDepth, void *thingToIgnore);

#endif

/*
 * Garbage collector API.
 */
extern JS_PUBLIC_API(void)
JS_GC(JSContext *cx);

extern JS_PUBLIC_API(void)
JS_MaybeGC(JSContext *cx);

extern JS_PUBLIC_API(JSGCCallback)
JS_SetGCCallback(JSContext *cx, JSGCCallback cb);

extern JS_PUBLIC_API(JSGCCallback)
JS_SetGCCallbackRT(JSRuntime *rt, JSGCCallback cb);

extern JS_PUBLIC_API(JSBool)
JS_IsGCMarkingTracer(JSTracer *trc);

extern JS_PUBLIC_API(JSBool)
JS_IsAboutToBeFinalized(JSContext *cx, void *thing);

typedef enum JSGCParamKey {
    /* Maximum nominal heap before last ditch GC. */
    JSGC_MAX_BYTES          = 0,

    /* Number of JS_malloc bytes before last ditch GC. */
    JSGC_MAX_MALLOC_BYTES   = 1,

    /* Hoard stackPools for this long, in ms, default is 30 seconds. */
    JSGC_STACKPOOL_LIFESPAN = 2,

    /*
     * The factor that defines when the GC is invoked. The factor is a
     * percent of the memory allocated by the GC after the last run of
     * the GC. When the current memory allocated by the GC is more than
     * this percent then the GC is invoked. The factor cannot be less
     * than 100 since the current memory allocated by the GC cannot be less
     * than the memory allocated after the last run of the GC.
     */
    JSGC_TRIGGER_FACTOR = 3,

    /* Amount of bytes allocated by the GC. */
    JSGC_BYTES = 4,

    /* Number of times when GC was invoked. */
    JSGC_NUMBER = 5,

    /* Max size of the code cache in bytes. */
    JSGC_MAX_CODE_CACHE_BYTES = 6
} JSGCParamKey;

extern JS_PUBLIC_API(void)
JS_SetGCParameter(JSRuntime *rt, JSGCParamKey key, uint32 value);

extern JS_PUBLIC_API(uint32)
JS_GetGCParameter(JSRuntime *rt, JSGCParamKey key);

extern JS_PUBLIC_API(void)
JS_SetGCParameterForThread(JSContext *cx, JSGCParamKey key, uint32 value);

extern JS_PUBLIC_API(uint32)
JS_GetGCParameterForThread(JSContext *cx, JSGCParamKey key);

/*
 * Flush the code cache for the current thread. The operation might be
 * delayed if the cache cannot be flushed currently because native
 * code is currently executing.
 */

extern JS_PUBLIC_API(void)
JS_FlushCaches(JSContext *cx);

/*
 * Add a finalizer for external strings created by JS_NewExternalString (see
 * below) using a type-code returned from this function, and that understands
 * how to free or release the memory pointed at by JS_GetStringChars(str).
 *
 * Return a nonnegative type index if there is room for finalizer in the
 * global GC finalizers table, else return -1.  If the engine is compiled
 * JS_THREADSAFE and used in a multi-threaded environment, this function must
 * be invoked on the primordial thread only, at startup -- or else the entire
 * program must single-thread itself while loading a module that calls this
 * function.
 */
extern JS_PUBLIC_API(intN)
JS_AddExternalStringFinalizer(JSStringFinalizeOp finalizer);

/*
 * Remove finalizer from the global GC finalizers table, returning its type
 * code if found, -1 if not found.
 *
 * As with JS_AddExternalStringFinalizer, there is a threading restriction
 * if you compile the engine JS_THREADSAFE: this function may be called for a
 * given finalizer pointer on only one thread; different threads may call to
 * remove distinct finalizers safely.
 *
 * You must ensure that all strings with finalizer's type have been collected
 * before calling this function.  Otherwise, string data will be leaked by the
 * GC, for want of a finalizer to call.
 */
extern JS_PUBLIC_API(intN)
JS_RemoveExternalStringFinalizer(JSStringFinalizeOp finalizer);

/*
 * Create a new JSString whose chars member refers to external memory, i.e.,
 * memory requiring spe, type-specific finalization.  The type code must
 * be a nonnegative return value from JS_AddExternalStringFinalizer.
 */
extern JS_PUBLIC_API(JSString *)
JS_NewExternalString(JSContext *cx, jschar *chars, size_t length, intN type);

/*
 * Returns the external-string finalizer index for this string, or -1 if it is
 * an "internal" (native to JS engine) string.
 */
extern JS_PUBLIC_API(intN)
JS_GetExternalStringGCType(JSRuntime *rt, JSString *str);

/*
 * Sets maximum (if stack grows upward) or minimum (downward) legal stack byte
 * address in limitAddr for the thread or process stack used by cx.  To disable
 * stack size checking, pass 0 for limitAddr.
 */
extern JS_PUBLIC_API(void)
JS_SetThreadStackLimit(JSContext *cx, jsuword limitAddr);

/*
 * Set the quota on the number of bytes that stack-like data structures can
 * use when the runtime compiles and executes scripts. These structures
 * consume heap space, so JS_SetThreadStackLimit does not bound their size.
 * The default quota is 32MB which is quite generous.
 *
 * The function must be called before any script compilation or execution API
 * calls, i.e. either immediately after JS_NewContext or from JSCONTEXT_NEW
 * context callback.
 */
extern JS_PUBLIC_API(void)
JS_SetScriptStackQuota(JSContext *cx, size_t quota);

#define JS_DEFAULT_SCRIPT_STACK_QUOTA   ((size_t) 0x2000000)

/************************************************************************/

/*
 * Classes, objects, and properties.
 */

/* For detailed comments on the function pointer types, see jspubtd.h. */
struct JSClass {
    const char          *name;
    uint32              flags;

    /* Mandatory non-null function pointer members. */
    JSPropertyOp        addProperty;
    JSPropertyOp        delProperty;
    JSPropertyOp        getProperty;
    JSPropertyOp        setProperty;
    JSEnumerateOp       enumerate;
    JSResolveOp         resolve;
    JSConvertOp         convert;
    JSFinalizeOp        finalize;

    /* Optionally non-null members start here. */
    JSGetObjectOps      getObjectOps;
    JSCheckAccessOp     checkAccess;
    JSNative            call;
    JSNative            construct;
    JSXDRObjectOp       xdrObject;
    JSHasInstanceOp     hasInstance;
    JSMarkOp            mark;
    JSReserveSlotsOp    reserveSlots;
};

struct JSExtendedClass {
    JSClass             base;
    JSEqualityOp        equality;
    JSObjectOp          outerObject;
    JSObjectOp          innerObject;
    JSIteratorOp        iteratorObject;
    JSObjectOp          wrappedObject;          /* NB: infallible, null
                                                   returns are treated as
                                                   the original object */
    void                (*reserved0)(void);
    void                (*reserved1)(void);
    void                (*reserved2)(void);
};

#define JSCLASS_HAS_PRIVATE             (1<<0)  /* objects have private slot */
#define JSCLASS_NEW_ENUMERATE           (1<<1)  /* has JSNewEnumerateOp hook */
#define JSCLASS_NEW_RESOLVE             (1<<2)  /* has JSNewResolveOp hook */
#define JSCLASS_PRIVATE_IS_NSISUPPORTS  (1<<3)  /* private is (nsISupports *) */
/* (1<<4) was JSCLASS_SHARE_ALL_PROPERTIES, now obsolete. See bug 527805. */
#define JSCLASS_NEW_RESOLVE_GETS_START  (1<<5)  /* JSNewResolveOp gets starting
                                                   object in prototype chain
                                                   passed in via *objp in/out
                                                   parameter */
#define JSCLASS_CONSTRUCT_PROTOTYPE     (1<<6)  /* call constructor on class
                                                   prototype */
#define JSCLASS_DOCUMENT_OBSERVER       (1<<7)  /* DOM document observer */

/*
 * To reserve slots fetched and stored via JS_Get/SetReservedSlot, bitwise-or
 * JSCLASS_HAS_RESERVED_SLOTS(n) into the initializer for JSClass.flags, where
 * n is a constant in [1, 255].  Reserved slots are indexed from 0 to n-1.
 */
#define JSCLASS_RESERVED_SLOTS_SHIFT    8       /* room for 8 flags below */
#define JSCLASS_RESERVED_SLOTS_WIDTH    8       /* and 16 above this field */
#define JSCLASS_RESERVED_SLOTS_MASK     JS_BITMASK(JSCLASS_RESERVED_SLOTS_WIDTH)
#define JSCLASS_HAS_RESERVED_SLOTS(n)   (((n) & JSCLASS_RESERVED_SLOTS_MASK)  \
                                         << JSCLASS_RESERVED_SLOTS_SHIFT)
#define JSCLASS_RESERVED_SLOTS(clasp)   (((clasp)->flags                      \
                                          >> JSCLASS_RESERVED_SLOTS_SHIFT)    \
                                         & JSCLASS_RESERVED_SLOTS_MASK)

#define JSCLASS_HIGH_FLAGS_SHIFT        (JSCLASS_RESERVED_SLOTS_SHIFT +       \
                                         JSCLASS_RESERVED_SLOTS_WIDTH)

/* True if JSClass is really a JSExtendedClass. */
#define JSCLASS_IS_EXTENDED             (1<<(JSCLASS_HIGH_FLAGS_SHIFT+0))
#define JSCLASS_IS_ANONYMOUS            (1<<(JSCLASS_HIGH_FLAGS_SHIFT+1))
#define JSCLASS_IS_GLOBAL               (1<<(JSCLASS_HIGH_FLAGS_SHIFT+2))

/* Indicates that JSClass.mark is a tracer with JSTraceOp type. */
#define JSCLASS_MARK_IS_TRACE           (1<<(JSCLASS_HIGH_FLAGS_SHIFT+3))

/*
 * ECMA-262 requires that most constructors used internally create objects
 * with "the original Foo.prototype value" as their [[Prototype]] (__proto__)
 * member initial value.  The "original ... value" verbiage is there because
 * in ECMA-262, global properties naming class objects are read/write and
 * deleteable, for the most part.
 *
 * Implementing this efficiently requires that global objects have classes
 * with the following flags.  Failure to use JSCLASS_GLOBAL_FLAGS won't break
 * anything except the ECMA-262 "original prototype value" behavior, which was
 * broken for years in SpiderMonkey.  In other words, without these flags you
 * get backward compatibility.
 */
#define JSCLASS_GLOBAL_FLAGS \
    (JSCLASS_IS_GLOBAL | JSCLASS_HAS_RESERVED_SLOTS(JSProto_LIMIT))

/* Fast access to the original value of each standard class's prototype. */
#define JSCLASS_CACHED_PROTO_SHIFT      (JSCLASS_HIGH_FLAGS_SHIFT + 8)
#define JSCLASS_CACHED_PROTO_WIDTH      8
#define JSCLASS_CACHED_PROTO_MASK       JS_BITMASK(JSCLASS_CACHED_PROTO_WIDTH)
#define JSCLASS_HAS_CACHED_PROTO(key)   ((key) << JSCLASS_CACHED_PROTO_SHIFT)
#define JSCLASS_CACHED_PROTO_KEY(clasp) ((JSProtoKey)                         \
                                         (((clasp)->flags                     \
                                           >> JSCLASS_CACHED_PROTO_SHIFT)     \
                                          & JSCLASS_CACHED_PROTO_MASK))

/* Initializer for unused members of statically initialized JSClass structs. */
#define JSCLASS_NO_OPTIONAL_MEMBERS     0,0,0,0,0,0,0,0
#define JSCLASS_NO_RESERVED_MEMBERS     0,0,0

struct JSIdArray {
    void *self;
    jsint length;
    jsid  vector[1];    /* actually, length jsid words */
};

extern JS_PUBLIC_API(void)
JS_DestroyIdArray(JSContext *cx, JSIdArray *ida);

extern JS_PUBLIC_API(JSBool)
JS_ValueToId(JSContext *cx, jsval v, jsid *idp);

extern JS_PUBLIC_API(void)
JS_IdToValue(JSContext *cx, jsid id, jsval *vp);

/*
 * The magic XML namespace id is int-tagged, but not a valid integer jsval.
 * Global object classes in embeddings that enable JS_HAS_XML_SUPPORT (E4X)
 * should handle this id specially before converting id via JSVAL_TO_INT.
 */
#define JS_DEFAULT_XML_NAMESPACE_ID ((jsid) JSVAL_VOID)

/*
 * JSNewResolveOp flag bits.
 */
#define JSRESOLVE_QUALIFIED     0x01    /* resolve a qualified property id */
#define JSRESOLVE_ASSIGNING     0x02    /* resolve on the left of assignment */
#define JSRESOLVE_DETECTING     0x04    /* 'if (o.p)...' or '(o.p) ?...:...' */
#define JSRESOLVE_DECLARING     0x08    /* var, const, or function prolog op */
#define JSRESOLVE_CLASSNAME     0x10    /* class name used when constructing */
#define JSRESOLVE_WITH          0x20    /* resolve inside a with statement */

extern JS_PUBLIC_API(JSBool)
JS_PropertyStub(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

extern JS_PUBLIC_API(JSBool)
JS_EnumerateStub(JSContext *cx, JSObject *obj);

extern JS_PUBLIC_API(JSBool)
JS_ResolveStub(JSContext *cx, JSObject *obj, jsval id);

extern JS_PUBLIC_API(JSBool)
JS_ConvertStub(JSContext *cx, JSObject *obj, JSType type, jsval *vp);

extern JS_PUBLIC_API(void)
JS_FinalizeStub(JSContext *cx, JSObject *obj);

struct JSConstDoubleSpec {
    jsdouble        dval;
    const char      *name;
    uint8           flags;
    uint8           spare[3];
};

/*
 * To define an array element rather than a named property member, cast the
 * element's index to (const char *) and initialize name with it, and set the
 * JSPROP_INDEX bit in flags.
 */
struct JSPropertySpec {
    const char      *name;
    int8            tinyid;
    uint8           flags;
    JSPropertyOp    getter;
    JSPropertyOp    setter;
};

struct JSFunctionSpec {
    const char      *name;
    JSNative        call;
    uint16          nargs;
    uint16          flags;

    /*
     * extra & 0xFFFF:  Number of extra argument slots for local GC roots.
     *                  If fast native, must be zero.
     * extra >> 16:     Reserved for future use (must be 0).
     */
    uint32          extra;
};

/*
 * Terminating sentinel initializer to put at the end of a JSFunctionSpec array
 * that's passed to JS_DefineFunctions or JS_InitClass.
 */
#define JS_FS_END JS_FS(NULL,NULL,0,0,0)

/*
 * Initializer macro for a JSFunctionSpec array element. This is the original
 * kind of native function specifier initializer. Use JS_FN ("fast native", see
 * JSFastNative in jspubtd.h) for all functions that do not need a stack frame
 * when activated.
 */
#define JS_FS(name,call,nargs,flags,extra)                                    \
    {name, call, nargs, flags, extra}

/*
 * "Fast native" initializer macro for a JSFunctionSpec array element. Use this
 * in preference to JS_FS if the native in question does not need its own stack
 * frame when activated.
 */
#define JS_FN(name,fastcall,nargs,flags)                                      \
    JS_FS(name, (JSNative)(fastcall), nargs,                                  \
          (flags) | JSFUN_FAST_NATIVE | JSFUN_STUB_GSOPS, 0)

extern JS_PUBLIC_API(JSObject *)
JS_InitClass(JSContext *cx, JSObject *obj, JSObject *parent_proto,
             JSClass *clasp, JSNative constructor, uintN nargs,
             JSPropertySpec *ps, JSFunctionSpec *fs,
             JSPropertySpec *static_ps, JSFunctionSpec *static_fs);

#ifdef JS_THREADSAFE
extern JS_PUBLIC_API(JSClass *)
JS_GetClass(JSContext *cx, JSObject *obj);

#define JS_GET_CLASS(cx,obj) JS_GetClass(cx, obj)
#else
extern JS_PUBLIC_API(JSClass *)
JS_GetClass(JSObject *obj);

#define JS_GET_CLASS(cx,obj) JS_GetClass(obj)
#endif

extern JS_PUBLIC_API(JSBool)
JS_InstanceOf(JSContext *cx, JSObject *obj, JSClass *clasp, jsval *argv);

extern JS_PUBLIC_API(JSBool)
JS_HasInstance(JSContext *cx, JSObject *obj, jsval v, JSBool *bp);

extern JS_PUBLIC_API(void *)
JS_GetPrivate(JSContext *cx, JSObject *obj);

extern JS_PUBLIC_API(JSBool)
JS_SetPrivate(JSContext *cx, JSObject *obj, void *data);

extern JS_PUBLIC_API(void *)
JS_GetInstancePrivate(JSContext *cx, JSObject *obj, JSClass *clasp,
                      jsval *argv);

extern JS_PUBLIC_API(JSObject *)
JS_GetPrototype(JSContext *cx, JSObject *obj);

extern JS_PUBLIC_API(JSBool)
JS_SetPrototype(JSContext *cx, JSObject *obj, JSObject *proto);

extern JS_PUBLIC_API(JSObject *)
JS_GetParent(JSContext *cx, JSObject *obj);

extern JS_PUBLIC_API(JSBool)
JS_SetParent(JSContext *cx, JSObject *obj, JSObject *parent);

extern JS_PUBLIC_API(JSObject *)
JS_GetConstructor(JSContext *cx, JSObject *proto);

/*
 * Get a unique identifier for obj, good for the lifetime of obj (even if it
 * is moved by a copying GC).  Return false on failure (likely out of memory),
 * and true with *idp containing the unique id on success.
 */
extern JS_PUBLIC_API(JSBool)
JS_GetObjectId(JSContext *cx, JSObject *obj, jsid *idp);

extern JS_PUBLIC_API(JSObject *)
JS_NewObject(JSContext *cx, JSClass *clasp, JSObject *proto, JSObject *parent);

/*
 * Unlike JS_NewObject, JS_NewObjectWithGivenProto does not compute a default
 * proto if proto's actual parameter value is null.
 */
extern JS_PUBLIC_API(JSObject *)
JS_NewObjectWithGivenProto(JSContext *cx, JSClass *clasp, JSObject *proto,
                           JSObject *parent);

extern JS_PUBLIC_API(JSBool)
JS_SealObject(JSContext *cx, JSObject *obj, JSBool deep);

extern JS_PUBLIC_API(JSObject *)
JS_ConstructObject(JSContext *cx, JSClass *clasp, JSObject *proto,
                   JSObject *parent);

extern JS_PUBLIC_API(JSObject *)
JS_ConstructObjectWithArguments(JSContext *cx, JSClass *clasp, JSObject *proto,
                                JSObject *parent, uintN argc, jsval *argv);

extern JS_PUBLIC_API(JSObject *)
JS_New(JSContext *cx, JSObject *ctor, uintN argc, jsval *argv);

extern JS_PUBLIC_API(JSObject *)
JS_DefineObject(JSContext *cx, JSObject *obj, const char *name, JSClass *clasp,
                JSObject *proto, uintN attrs);

extern JS_PUBLIC_API(JSBool)
JS_DefineConstDoubles(JSContext *cx, JSObject *obj, JSConstDoubleSpec *cds);

extern JS_PUBLIC_API(JSBool)
JS_DefineProperties(JSContext *cx, JSObject *obj, JSPropertySpec *ps);

extern JS_PUBLIC_API(JSBool)
JS_DefineProperty(JSContext *cx, JSObject *obj, const char *name, jsval value,
                  JSPropertyOp getter, JSPropertyOp setter, uintN attrs);

extern JS_PUBLIC_API(JSBool)
JS_DefinePropertyById(JSContext *cx, JSObject *obj, jsid id, jsval value,
                      JSPropertyOp getter, JSPropertyOp setter, uintN attrs);

extern JS_PUBLIC_API(JSBool)
JS_DefineOwnProperty(JSContext *cx, JSObject *obj, jsid id, jsval descriptor, JSBool *bp);

/*
 * Determine the attributes (JSPROP_* flags) of a property on a given object.
 *
 * If the object does not have a property by that name, *foundp will be
 * JS_FALSE and the value of *attrsp is undefined.
 */
extern JS_PUBLIC_API(JSBool)
JS_GetPropertyAttributes(JSContext *cx, JSObject *obj, const char *name,
                         uintN *attrsp, JSBool *foundp);

/*
 * The same, but if the property is native, return its getter and setter via
 * *getterp and *setterp, respectively (and only if the out parameter pointer
 * is not null).
 */
extern JS_PUBLIC_API(JSBool)
JS_GetPropertyAttrsGetterAndSetter(JSContext *cx, JSObject *obj,
                                   const char *name,
                                   uintN *attrsp, JSBool *foundp,
                                   JSPropertyOp *getterp,
                                   JSPropertyOp *setterp);

extern JS_PUBLIC_API(JSBool)
JS_GetPropertyAttrsGetterAndSetterById(JSContext *cx, JSObject *obj,
                                       jsid id,
                                       uintN *attrsp, JSBool *foundp,
                                       JSPropertyOp *getterp,
                                       JSPropertyOp *setterp);

/*
 * Set the attributes of a property on a given object.
 *
 * If the object does not have a property by that name, *foundp will be
 * JS_FALSE and nothing will be altered.
 */
extern JS_PUBLIC_API(JSBool)
JS_SetPropertyAttributes(JSContext *cx, JSObject *obj, const char *name,
                         uintN attrs, JSBool *foundp);

extern JS_PUBLIC_API(JSBool)
JS_DefinePropertyWithTinyId(JSContext *cx, JSObject *obj, const char *name,
                            int8 tinyid, jsval value,
                            JSPropertyOp getter, JSPropertyOp setter,
                            uintN attrs);

extern JS_PUBLIC_API(JSBool)
JS_AliasProperty(JSContext *cx, JSObject *obj, const char *name,
                 const char *alias);

extern JS_PUBLIC_API(JSBool)
JS_AlreadyHasOwnProperty(JSContext *cx, JSObject *obj, const char *name,
                         JSBool *foundp);

extern JS_PUBLIC_API(JSBool)
JS_AlreadyHasOwnPropertyById(JSContext *cx, JSObject *obj, jsid id,
                             JSBool *foundp);

extern JS_PUBLIC_API(JSBool)
JS_HasProperty(JSContext *cx, JSObject *obj, const char *name, JSBool *foundp);

extern JS_PUBLIC_API(JSBool)
JS_HasPropertyById(JSContext *cx, JSObject *obj, jsid id, JSBool *foundp);

extern JS_PUBLIC_API(JSBool)
JS_LookupProperty(JSContext *cx, JSObject *obj, const char *name, jsval *vp);

extern JS_PUBLIC_API(JSBool)
JS_LookupPropertyById(JSContext *cx, JSObject *obj, jsid id, jsval *vp);

extern JS_PUBLIC_API(JSBool)
JS_LookupPropertyWithFlags(JSContext *cx, JSObject *obj, const char *name,
                           uintN flags, jsval *vp);

extern JS_PUBLIC_API(JSBool)
JS_LookupPropertyWithFlagsById(JSContext *cx, JSObject *obj, jsid id,
                               uintN flags, JSObject **objp, jsval *vp);

struct JSPropertyDescriptor {
    JSObject     *obj;
    uintN        attrs;
    JSPropertyOp getter;
    JSPropertyOp setter;
    jsval        value;
};

/*
 * Like JS_GetPropertyAttrsGetterAndSetterById but will return a property on
 * an object on the prototype chain (returned in objp). If data->obj is null,
 * then this property was not found on the prototype chain.
 */
extern JS_PUBLIC_API(JSBool)
JS_GetPropertyDescriptorById(JSContext *cx, JSObject *obj, jsid id, uintN flags,
                             JSPropertyDescriptor *desc);

extern JS_PUBLIC_API(JSBool)
JS_GetOwnPropertyDescriptor(JSContext *cx, JSObject *obj, jsid id, jsval *vp);

extern JS_PUBLIC_API(JSBool)
JS_GetProperty(JSContext *cx, JSObject *obj, const char *name, jsval *vp);

extern JS_PUBLIC_API(JSBool)
JS_GetPropertyById(JSContext *cx, JSObject *obj, jsid id, jsval *vp);

extern JS_PUBLIC_API(JSBool)
JS_GetMethodById(JSContext *cx, JSObject *obj, jsid id, JSObject **objp,
                 jsval *vp);

extern JS_PUBLIC_API(JSBool)
JS_GetMethod(JSContext *cx, JSObject *obj, const char *name, JSObject **objp,
             jsval *vp);

extern JS_PUBLIC_API(JSBool)
JS_SetProperty(JSContext *cx, JSObject *obj, const char *name, jsval *vp);

extern JS_PUBLIC_API(JSBool)
JS_SetPropertyById(JSContext *cx, JSObject *obj, jsid id, jsval *vp);

extern JS_PUBLIC_API(JSBool)
JS_DeleteProperty(JSContext *cx, JSObject *obj, const char *name);

extern JS_PUBLIC_API(JSBool)
JS_DeleteProperty2(JSContext *cx, JSObject *obj, const char *name,
                   jsval *rval);

extern JS_PUBLIC_API(JSBool)
JS_DeletePropertyById(JSContext *cx, JSObject *obj, jsid id);

extern JS_PUBLIC_API(JSBool)
JS_DeletePropertyById2(JSContext *cx, JSObject *obj, jsid id, jsval *rval);

extern JS_PUBLIC_API(JSBool)
JS_DefineUCProperty(JSContext *cx, JSObject *obj,
                    const jschar *name, size_t namelen, jsval value,
                    JSPropertyOp getter, JSPropertyOp setter,
                    uintN attrs);

/*
 * Determine the attributes (JSPROP_* flags) of a property on a given object.
 *
 * If the object does not have a property by that name, *foundp will be
 * JS_FALSE and the value of *attrsp is undefined.
 */
extern JS_PUBLIC_API(JSBool)
JS_GetUCPropertyAttributes(JSContext *cx, JSObject *obj,
                           const jschar *name, size_t namelen,
                           uintN *attrsp, JSBool *foundp);

/*
 * The same, but if the property is native, return its getter and setter via
 * *getterp and *setterp, respectively (and only if the out parameter pointer
 * is not null).
 */
extern JS_PUBLIC_API(JSBool)
JS_GetUCPropertyAttrsGetterAndSetter(JSContext *cx, JSObject *obj,
                                     const jschar *name, size_t namelen,
                                     uintN *attrsp, JSBool *foundp,
                                     JSPropertyOp *getterp,
                                     JSPropertyOp *setterp);

/*
 * Set the attributes of a property on a given object.
 *
 * If the object does not have a property by that name, *foundp will be
 * JS_FALSE and nothing will be altered.
 */
extern JS_PUBLIC_API(JSBool)
JS_SetUCPropertyAttributes(JSContext *cx, JSObject *obj,
                           const jschar *name, size_t namelen,
                           uintN attrs, JSBool *foundp);


extern JS_PUBLIC_API(JSBool)
JS_DefineUCPropertyWithTinyId(JSContext *cx, JSObject *obj,
                              const jschar *name, size_t namelen,
                              int8 tinyid, jsval value,
                              JSPropertyOp getter, JSPropertyOp setter,
                              uintN attrs);

extern JS_PUBLIC_API(JSBool)
JS_AlreadyHasOwnUCProperty(JSContext *cx, JSObject *obj, const jschar *name,
                           size_t namelen, JSBool *foundp);

extern JS_PUBLIC_API(JSBool)
JS_HasUCProperty(JSContext *cx, JSObject *obj,
                 const jschar *name, size_t namelen,
                 JSBool *vp);

extern JS_PUBLIC_API(JSBool)
JS_LookupUCProperty(JSContext *cx, JSObject *obj,
                    const jschar *name, size_t namelen,
                    jsval *vp);

extern JS_PUBLIC_API(JSBool)
JS_GetUCProperty(JSContext *cx, JSObject *obj,
                 const jschar *name, size_t namelen,
                 jsval *vp);

extern JS_PUBLIC_API(JSBool)
JS_SetUCProperty(JSContext *cx, JSObject *obj,
                 const jschar *name, size_t namelen,
                 jsval *vp);

extern JS_PUBLIC_API(JSBool)
JS_DeleteUCProperty2(JSContext *cx, JSObject *obj,
                     const jschar *name, size_t namelen,
                     jsval *rval);

extern JS_PUBLIC_API(JSObject *)
JS_NewArrayObject(JSContext *cx, jsint length, jsval *vector);

extern JS_PUBLIC_API(JSBool)
JS_IsArrayObject(JSContext *cx, JSObject *obj);

extern JS_PUBLIC_API(JSBool)
JS_GetArrayLength(JSContext *cx, JSObject *obj, jsuint *lengthp);

extern JS_PUBLIC_API(JSBool)
JS_SetArrayLength(JSContext *cx, JSObject *obj, jsuint length);

extern JS_PUBLIC_API(JSBool)
JS_HasArrayLength(JSContext *cx, JSObject *obj, jsuint *lengthp);

extern JS_PUBLIC_API(JSBool)
JS_DefineElement(JSContext *cx, JSObject *obj, jsint index, jsval value,
                 JSPropertyOp getter, JSPropertyOp setter, uintN attrs);

extern JS_PUBLIC_API(JSBool)
JS_AliasElement(JSContext *cx, JSObject *obj, const char *name, jsint alias);

extern JS_PUBLIC_API(JSBool)
JS_AlreadyHasOwnElement(JSContext *cx, JSObject *obj, jsint index,
                        JSBool *foundp);

extern JS_PUBLIC_API(JSBool)
JS_HasElement(JSContext *cx, JSObject *obj, jsint index, JSBool *foundp);

extern JS_PUBLIC_API(JSBool)
JS_LookupElement(JSContext *cx, JSObject *obj, jsint index, jsval *vp);

extern JS_PUBLIC_API(JSBool)
JS_GetElement(JSContext *cx, JSObject *obj, jsint index, jsval *vp);

extern JS_PUBLIC_API(JSBool)
JS_SetElement(JSContext *cx, JSObject *obj, jsint index, jsval *vp);

extern JS_PUBLIC_API(JSBool)
JS_DeleteElement(JSContext *cx, JSObject *obj, jsint index);

extern JS_PUBLIC_API(JSBool)
JS_DeleteElement2(JSContext *cx, JSObject *obj, jsint index, jsval *rval);

extern JS_PUBLIC_API(void)
JS_ClearScope(JSContext *cx, JSObject *obj);

extern JS_PUBLIC_API(JSIdArray *)
JS_Enumerate(JSContext *cx, JSObject *obj);

/*
 * Create an object to iterate over enumerable properties of obj, in arbitrary
 * property definition order.  NB: This differs from longstanding for..in loop
 * order, which uses order of property definition in obj.
 */
extern JS_PUBLIC_API(JSObject *)
JS_NewPropertyIterator(JSContext *cx, JSObject *obj);

/*
 * Return true on success with *idp containing the id of the next enumerable
 * property to visit using iterobj, or JSVAL_VOID if there is no such property
 * left to visit.  Return false on error.
 */
extern JS_PUBLIC_API(JSBool)
JS_NextProperty(JSContext *cx, JSObject *iterobj, jsid *idp);

extern JS_PUBLIC_API(JSBool)
JS_CheckAccess(JSContext *cx, JSObject *obj, jsid id, JSAccessMode mode,
               jsval *vp, uintN *attrsp);

extern JS_PUBLIC_API(JSBool)
JS_GetReservedSlot(JSContext *cx, JSObject *obj, uint32 index, jsval *vp);

extern JS_PUBLIC_API(JSBool)
JS_SetReservedSlot(JSContext *cx, JSObject *obj, uint32 index, jsval v);

/************************************************************************/

/*
 * Security protocol.
 */
struct JSPrincipals {
    char *codebase;

    /* XXX unspecified and unused by Mozilla code -- can we remove these? */
    void * (* getPrincipalArray)(JSContext *cx, JSPrincipals *);
    JSBool (* globalPrivilegesEnabled)(JSContext *cx, JSPrincipals *);

    /* Don't call "destroy"; use reference counting macros below. */
    jsrefcount refcount;

    void   (* destroy)(JSContext *cx, JSPrincipals *);
    JSBool (* subsume)(JSPrincipals *, JSPrincipals *);
};

#ifdef JS_THREADSAFE
#define JSPRINCIPALS_HOLD(cx, principals)   JS_HoldPrincipals(cx,principals)
#define JSPRINCIPALS_DROP(cx, principals)   JS_DropPrincipals(cx,principals)

extern JS_PUBLIC_API(jsrefcount)
JS_HoldPrincipals(JSContext *cx, JSPrincipals *principals);

extern JS_PUBLIC_API(jsrefcount)
JS_DropPrincipals(JSContext *cx, JSPrincipals *principals);

#else
#define JSPRINCIPALS_HOLD(cx, principals)   (++(principals)->refcount)
#define JSPRINCIPALS_DROP(cx, principals)                                     \
    ((--(principals)->refcount == 0)                                          \
     ? ((*(principals)->destroy)((cx), (principals)), 0)                      \
     : (principals)->refcount)
#endif


struct JSSecurityCallbacks {
    JSCheckAccessOp            checkObjectAccess;
    JSPrincipalsTranscoder     principalsTranscoder;
    JSObjectPrincipalsFinder   findObjectPrincipals;
    JSCSPEvalChecker           contentSecurityPolicyAllows;
};

extern JS_PUBLIC_API(JSSecurityCallbacks *)
JS_SetRuntimeSecurityCallbacks(JSRuntime *rt, JSSecurityCallbacks *callbacks);

extern JS_PUBLIC_API(JSSecurityCallbacks *)
JS_GetRuntimeSecurityCallbacks(JSRuntime *rt);

extern JS_PUBLIC_API(JSSecurityCallbacks *)
JS_SetContextSecurityCallbacks(JSContext *cx, JSSecurityCallbacks *callbacks);

extern JS_PUBLIC_API(JSSecurityCallbacks *)
JS_GetSecurityCallbacks(JSContext *cx);

/************************************************************************/

/*
 * Functions and scripts.
 */
extern JS_PUBLIC_API(JSFunction *)
JS_NewFunction(JSContext *cx, JSNative call, uintN nargs, uintN flags,
               JSObject *parent, const char *name);

extern JS_PUBLIC_API(JSObject *)
JS_GetFunctionObject(JSFunction *fun);

/*
 * Deprecated, useful only for diagnostics.  Use JS_GetFunctionId instead for
 * anonymous vs. "anonymous" disambiguation and Unicode fidelity.
 */
extern JS_PUBLIC_API(const char *)
JS_GetFunctionName(JSFunction *fun);

/*
 * Return the function's identifier as a JSString, or null if fun is unnamed.
 * The returned string lives as long as fun, so you don't need to root a saved
 * reference to it if fun is well-connected or rooted, and provided you bound
 * the use of the saved reference by fun's lifetime.
 *
 * Prefer JS_GetFunctionId over JS_GetFunctionName because it returns null for
 * truly anonymous functions, and because it doesn't chop to ISO-Latin-1 chars
 * from UTF-16-ish jschars.
 */
extern JS_PUBLIC_API(JSString *)
JS_GetFunctionId(JSFunction *fun);

/*
 * Return JSFUN_* flags for fun.
 */
extern JS_PUBLIC_API(uintN)
JS_GetFunctionFlags(JSFunction *fun);

/*
 * Return the arity (length) of fun.
 */
extern JS_PUBLIC_API(uint16)
JS_GetFunctionArity(JSFunction *fun);

/*
 * Infallible predicate to test whether obj is a function object (faster than
 * comparing obj's class name to "Function", but equivalent unless someone has
 * overwritten the "Function" identifier with a different constructor and then
 * created instances using that constructor that might be passed in as obj).
 */
extern JS_PUBLIC_API(JSBool)
JS_ObjectIsFunction(JSContext *cx, JSObject *obj);

extern JS_PUBLIC_API(JSBool)
JS_DefineFunctions(JSContext *cx, JSObject *obj, JSFunctionSpec *fs);

extern JS_PUBLIC_API(JSFunction *)
JS_DefineFunction(JSContext *cx, JSObject *obj, const char *name, JSNative call,
                  uintN nargs, uintN attrs);

extern JS_PUBLIC_API(JSFunction *)
JS_DefineUCFunction(JSContext *cx, JSObject *obj,
                    const jschar *name, size_t namelen, JSNative call,
                    uintN nargs, uintN attrs);

extern JS_PUBLIC_API(JSObject *)
JS_CloneFunctionObject(JSContext *cx, JSObject *funobj, JSObject *parent);

/*
 * Given a buffer, return JS_FALSE if the buffer might become a valid
 * javascript statement with the addition of more lines.  Otherwise return
 * JS_TRUE.  The intent is to support interactive compilation - accumulate
 * lines in a buffer until JS_BufferIsCompilableUnit is true, then pass it to
 * the compiler.
 */
extern JS_PUBLIC_API(JSBool)
JS_BufferIsCompilableUnit(JSContext *cx, JSObject *obj,
                          const char *bytes, size_t length);

/*
 * The JSScript objects returned by the following functions refer to string and
 * other kinds of literals, including doubles and RegExp objects.  These
 * literals are vulnerable to garbage collection; to root script objects and
 * prevent literals from being collected, create a rootable object using
 * JS_NewScriptObject, and root the resulting object using JS_Add[Named]Root.
 */
extern JS_PUBLIC_API(JSScript *)
JS_CompileScript(JSContext *cx, JSObject *obj,
                 const char *bytes, size_t length,
                 const char *filename, uintN lineno);

extern JS_PUBLIC_API(JSScript *)
JS_CompileScriptForPrincipals(JSContext *cx, JSObject *obj,
                              JSPrincipals *principals,
                              const char *bytes, size_t length,
                              const char *filename, uintN lineno);

extern JS_PUBLIC_API(JSScript *)
JS_CompileUCScript(JSContext *cx, JSObject *obj,
                   const jschar *chars, size_t length,
                   const char *filename, uintN lineno);

extern JS_PUBLIC_API(JSScript *)
JS_CompileUCScriptForPrincipals(JSContext *cx, JSObject *obj,
                                JSPrincipals *principals,
                                const jschar *chars, size_t length,
                                const char *filename, uintN lineno);

extern JS_PUBLIC_API(JSScript *)
JS_CompileFile(JSContext *cx, JSObject *obj, const char *filename);

extern JS_PUBLIC_API(JSScript *)
JS_CompileFileHandle(JSContext *cx, JSObject *obj, const char *filename,
                     FILE *fh);

extern JS_PUBLIC_API(JSScript *)
JS_CompileFileHandleForPrincipals(JSContext *cx, JSObject *obj,
                                  const char *filename, FILE *fh,
                                  JSPrincipals *principals);

/*
 * NB: you must use JS_NewScriptObject and root a pointer to its return value
 * in order to keep a JSScript and its atoms safe from garbage collection after
 * creating the script via JS_Compile* and before a JS_ExecuteScript* call.
 * E.g., and without error checks:
 *
 *    JSScript *script = JS_CompileFile(cx, global, filename);
 *    JSObject *scrobj = JS_NewScriptObject(cx, script);
 *    JS_AddNamedRoot(cx, &scrobj, "scrobj");
 *    do {
 *        jsval result;
 *        JS_ExecuteScript(cx, global, script, &result);
 *        JS_GC();
 *    } while (!JSVAL_IS_BOOLEAN(result) || JSVAL_TO_BOOLEAN(result));
 *    JS_RemoveRoot(cx, &scrobj);
 */
extern JS_PUBLIC_API(JSObject *)
JS_NewScriptObject(JSContext *cx, JSScript *script);

/*
 * Infallible getter for a script's object.  If JS_NewScriptObject has not been
 * called on script yet, the return value will be null.
 */
extern JS_PUBLIC_API(JSObject *)
JS_GetScriptObject(JSScript *script);

extern JS_PUBLIC_API(void)
JS_DestroyScript(JSContext *cx, JSScript *script);

extern JS_PUBLIC_API(JSFunction *)
JS_CompileFunction(JSContext *cx, JSObject *obj, const char *name,
                   uintN nargs, const char **argnames,
                   const char *bytes, size_t length,
                   const char *filename, uintN lineno);

extern JS_PUBLIC_API(JSFunction *)
JS_CompileFunctionForPrincipals(JSContext *cx, JSObject *obj,
                                JSPrincipals *principals, const char *name,
                                uintN nargs, const char **argnames,
                                const char *bytes, size_t length,
                                const char *filename, uintN lineno);

extern JS_PUBLIC_API(JSFunction *)
JS_CompileUCFunction(JSContext *cx, JSObject *obj, const char *name,
                     uintN nargs, const char **argnames,
                     const jschar *chars, size_t length,
                     const char *filename, uintN lineno);

extern JS_PUBLIC_API(JSFunction *)
JS_CompileUCFunctionForPrincipals(JSContext *cx, JSObject *obj,
                                  JSPrincipals *principals, const char *name,
                                  uintN nargs, const char **argnames,
                                  const jschar *chars, size_t length,
                                  const char *filename, uintN lineno);

extern JS_PUBLIC_API(JSString *)
JS_DecompileScript(JSContext *cx, JSScript *script, const char *name,
                   uintN indent);

/*
 * API extension: OR this into indent to avoid pretty-printing the decompiled
 * source resulting from JS_DecompileFunction{,Body}.
 */
#define JS_DONT_PRETTY_PRINT    ((uintN)0x8000)

extern JS_PUBLIC_API(JSString *)
JS_DecompileFunction(JSContext *cx, JSFunction *fun, uintN indent);

extern JS_PUBLIC_API(JSString *)
JS_DecompileFunctionBody(JSContext *cx, JSFunction *fun, uintN indent);

/*
 * NB: JS_ExecuteScript and the JS_Evaluate*Script* quadruplets use the obj
 * parameter as the initial scope chain header, the 'this' keyword value, and
 * the variables object (ECMA parlance for where 'var' and 'function' bind
 * names) of the execution context for script.
 *
 * Using obj as the variables object is problematic if obj's parent (which is
 * the scope chain link; see JS_SetParent and JS_NewObject) is not null: in
 * this case, variables created by 'var x = 0', e.g., go in obj, but variables
 * created by assignment to an unbound id, 'x = 0', go in the last object on
 * the scope chain linked by parent.
 *
 * ECMA calls that last scoping object the "global object", but note that many
 * embeddings have several such objects.  ECMA requires that "global code" be
 * executed with the variables object equal to this global object.  But these
 * JS API entry points provide freedom to execute code against a "sub-global",
 * i.e., a parented or scoped object, in which case the variables object will
 * differ from the last object on the scope chain, resulting in confusing and
 * non-ECMA explicit vs. implicit variable creation.
 *
 * Caveat embedders: unless you already depend on this buggy variables object
 * binding behavior, you should call JS_SetOptions(cx, JSOPTION_VAROBJFIX) or
 * JS_SetOptions(cx, JS_GetOptions(cx) | JSOPTION_VAROBJFIX) -- the latter if
 * someone may have set other options on cx already -- for each context in the
 * application, if you pass parented objects as the obj parameter, or may ever
 * pass such objects in the future.
 *
 * Why a runtime option?  The alternative is to add six or so new API entry
 * points with signatures matching the following six, and that doesn't seem
 * worth the code bloat cost.  Such new entry points would probably have less
 * obvious names, too, so would not tend to be used.  The JS_SetOption call,
 * OTOH, can be more easily hacked into existing code that does not depend on
 * the bug; such code can continue to use the familiar JS_EvaluateScript,
 * etc., entry points.
 */
extern JS_PUBLIC_API(JSBool)
JS_ExecuteScript(JSContext *cx, JSObject *obj, JSScript *script, jsval *rval);

/*
 * Execute either the function-defining prolog of a script, or the script's
 * main body, but not both.
 */
typedef enum JSExecPart { JSEXEC_PROLOG, JSEXEC_MAIN } JSExecPart;

extern JS_PUBLIC_API(JSBool)
JS_EvaluateScript(JSContext *cx, JSObject *obj,
                  const char *bytes, uintN length,
                  const char *filename, uintN lineno,
                  jsval *rval);

extern JS_PUBLIC_API(JSBool)
JS_EvaluateScriptForPrincipals(JSContext *cx, JSObject *obj,
                               JSPrincipals *principals,
                               const char *bytes, uintN length,
                               const char *filename, uintN lineno,
                               jsval *rval);

extern JS_PUBLIC_API(JSBool)
JS_EvaluateUCScript(JSContext *cx, JSObject *obj,
                    const jschar *chars, uintN length,
                    const char *filename, uintN lineno,
                    jsval *rval);

extern JS_PUBLIC_API(JSBool)
JS_EvaluateUCScriptForPrincipals(JSContext *cx, JSObject *obj,
                                 JSPrincipals *principals,
                                 const jschar *chars, uintN length,
                                 const char *filename, uintN lineno,
                                 jsval *rval);

extern JS_PUBLIC_API(JSBool)
JS_CallFunction(JSContext *cx, JSObject *obj, JSFunction *fun, uintN argc,
                jsval *argv, jsval *rval);

extern JS_PUBLIC_API(JSBool)
JS_CallFunctionName(JSContext *cx, JSObject *obj, const char *name, uintN argc,
                    jsval *argv, jsval *rval);

extern JS_PUBLIC_API(JSBool)
JS_CallFunctionValue(JSContext *cx, JSObject *obj, jsval fval, uintN argc,
                     jsval *argv, jsval *rval);

/*
 * These functions allow setting an operation callback that will be called
 * from the thread the context is associated with some time after any thread
 * triggered the callback using JS_TriggerOperationCallback(cx).
 *
 * In a threadsafe build the engine internally triggers operation callbacks
 * under certain circumstances (i.e. GC and title transfer) to force the
 * context to yield its current request, which the engine always
 * automatically does immediately prior to calling the callback function.
 * The embedding should thus not rely on callbacks being triggered through
 * the external API only.
 *
 * Important note: Additional callbacks can occur inside the callback handler
 * if it re-enters the JS engine. The embedding must ensure that the callback
 * is disconnected before attempting such re-entry.
 */

extern JS_PUBLIC_API(JSOperationCallback)
JS_SetOperationCallback(JSContext *cx, JSOperationCallback callback);

extern JS_PUBLIC_API(JSOperationCallback)
JS_GetOperationCallback(JSContext *cx);

extern JS_PUBLIC_API(void)
JS_TriggerOperationCallback(JSContext *cx);

extern JS_PUBLIC_API(void)
JS_TriggerAllOperationCallbacks(JSRuntime *rt);

extern JS_PUBLIC_API(JSBool)
JS_IsRunning(JSContext *cx);

extern JS_PUBLIC_API(JSBool)
JS_IsConstructing(JSContext *cx);

/*
 * Saving and restoring frame chains.
 *
 * These two functions are used to set aside cx's call stack while that stack
 * is inactive. After a call to JS_SaveFrameChain, it looks as if there is no
 * code running on cx. Before calling JS_RestoreFrameChain, cx's call stack
 * must be balanced and all nested calls to JS_SaveFrameChain must have had
 * matching JS_RestoreFrameChain calls.
 *
 * JS_SaveFrameChain deals with cx not having any code running on it. A null
 * return does not signify an error, and JS_RestoreFrameChain handles a null
 * frame pointer argument safely.
 */
extern JS_PUBLIC_API(JSStackFrame *)
JS_SaveFrameChain(JSContext *cx);

extern JS_PUBLIC_API(void)
JS_RestoreFrameChain(JSContext *cx, JSStackFrame *fp);

/************************************************************************/

/*
 * Strings.
 *
 * NB: JS_NewString takes ownership of bytes on success, avoiding a copy; but
 * on error (signified by null return), it leaves bytes owned by the caller.
 * So the caller must free bytes in the error case, if it has no use for them.
 * In contrast, all the JS_New*StringCopy* functions do not take ownership of
 * the character memory passed to them -- they copy it.
 */
extern JS_PUBLIC_API(JSString *)
JS_NewString(JSContext *cx, char *bytes, size_t length);

extern JS_PUBLIC_API(JSString *)
JS_NewStringCopyN(JSContext *cx, const char *s, size_t n);

extern JS_PUBLIC_API(JSString *)
JS_NewStringCopyZ(JSContext *cx, const char *s);

extern JS_PUBLIC_API(JSString *)
JS_InternString(JSContext *cx, const char *s);

extern JS_PUBLIC_API(JSString *)
JS_NewUCString(JSContext *cx, jschar *chars, size_t length);

extern JS_PUBLIC_API(JSString *)
JS_NewUCStringCopyN(JSContext *cx, const jschar *s, size_t n);

extern JS_PUBLIC_API(JSString *)
JS_NewUCStringCopyZ(JSContext *cx, const jschar *s);

extern JS_PUBLIC_API(JSString *)
JS_InternUCStringN(JSContext *cx, const jschar *s, size_t length);

extern JS_PUBLIC_API(JSString *)
JS_InternUCString(JSContext *cx, const jschar *s);

extern JS_PUBLIC_API(char *)
JS_GetStringBytes(JSString *str);

extern JS_PUBLIC_API(jschar *)
JS_GetStringChars(JSString *str);

extern JS_PUBLIC_API(size_t)
JS_GetStringLength(JSString *str);

extern JS_PUBLIC_API(const char *)
JS_GetStringBytesZ(JSContext *cx, JSString *str);

extern JS_PUBLIC_API(const jschar *)
JS_GetStringCharsZ(JSContext *cx, JSString *str);

extern JS_PUBLIC_API(intN)
JS_CompareStrings(JSString *str1, JSString *str2);

/*
 * Mutable string support.  A string's characters are never mutable in this JS
 * implementation, but a growable string has a buffer that can be reallocated,
 * and a dependent string is a substring of another (growable, dependent, or
 * immutable) string.  The direct data members of the (opaque to API clients)
 * JSString struct may be changed in a single-threaded way for growable and
 * dependent strings.
 *
 * Therefore mutable strings cannot be used by more than one thread at a time.
 * You may call JS_MakeStringImmutable to convert the string from a mutable
 * (growable or dependent) string to an immutable (and therefore thread-safe)
 * string.  The engine takes care of converting growable and dependent strings
 * to immutable for you if you store strings in multi-threaded objects using
 * JS_SetProperty or kindred API entry points.
 *
 * If you store a JSString pointer in a native data structure that is (safely)
 * accessible to multiple threads, you must call JS_MakeStringImmutable before
 * retiring the store.
 */
extern JS_PUBLIC_API(JSString *)
JS_NewGrowableString(JSContext *cx, jschar *chars, size_t length);

/*
 * Create a dependent string, i.e., a string that owns no character storage,
 * but that refers to a slice of another string's chars.  Dependent strings
 * are mutable by definition, so the thread safety comments above apply.
 */
extern JS_PUBLIC_API(JSString *)
JS_NewDependentString(JSContext *cx, JSString *str, size_t start,
                      size_t length);

/*
 * Concatenate two strings, resulting in a new growable string.  If you create
 * the left string and pass it to JS_ConcatStrings on a single thread, try to
 * use JS_NewGrowableString to create the left string -- doing so helps Concat
 * avoid allocating a new buffer for the result and copying left's chars into
 * the new buffer.  See above for thread safety comments.
 */
extern JS_PUBLIC_API(JSString *)
JS_ConcatStrings(JSContext *cx, JSString *left, JSString *right);

/*
 * Convert a dependent string into an independent one.  This function does not
 * change the string's mutability, so the thread safety comments above apply.
 */
extern JS_PUBLIC_API(const jschar *)
JS_UndependString(JSContext *cx, JSString *str);

/*
 * Convert a mutable string (either growable or dependent) into an immutable,
 * thread-safe one.
 */
extern JS_PUBLIC_API(JSBool)
JS_MakeStringImmutable(JSContext *cx, JSString *str);

/*
 * Return JS_TRUE if C (char []) strings passed via the API and internally
 * are UTF-8.
 */
JS_PUBLIC_API(JSBool)
JS_CStringsAreUTF8(void);

/*
 * Update the value to be returned by JS_CStringsAreUTF8(). Once set, it
 * can never be changed. This API must be called before the first call to
 * JS_NewRuntime.
 */
JS_PUBLIC_API(void)
JS_SetCStringsAreUTF8(void);

/*
 * Character encoding support.
 *
 * For both JS_EncodeCharacters and JS_DecodeBytes, set *dstlenp to the size
 * of the destination buffer before the call; on return, *dstlenp contains the
 * number of bytes (JS_EncodeCharacters) or jschars (JS_DecodeBytes) actually
 * stored.  To determine the necessary destination buffer size, make a sizing
 * call that passes NULL for dst.
 *
 * On errors, the functions report the error. In that case, *dstlenp contains
 * the number of characters or bytes transferred so far.  If cx is NULL, no
 * error is reported on failure, and the functions simply return JS_FALSE.
 *
 * NB: Neither function stores an additional zero byte or jschar after the
 * transcoded string.
 *
 * If JS_CStringsAreUTF8() is true then JS_EncodeCharacters encodes to
 * UTF-8, and JS_DecodeBytes decodes from UTF-8, which may create additional
 * errors if the character sequence is malformed.  If UTF-8 support is
 * disabled, the functions deflate and inflate, respectively.
 */
JS_PUBLIC_API(JSBool)
JS_EncodeCharacters(JSContext *cx, const jschar *src, size_t srclen, char *dst,
                    size_t *dstlenp);

JS_PUBLIC_API(JSBool)
JS_DecodeBytes(JSContext *cx, const char *src, size_t srclen, jschar *dst,
               size_t *dstlenp);

/*
 * A variation on JS_EncodeCharacters where a null terminated string is
 * returned that you are expected to call JS_free on when done.
 */
JS_PUBLIC_API(char *)
JS_EncodeString(JSContext *cx, JSString *str);

/************************************************************************/
/*
 * JSON functions
 */
typedef JSBool (* JSONWriteCallback)(const jschar *buf, uint32 len, void *data);

/*
 * JSON.stringify as specified by ES3.1 (draft)
 */
JS_PUBLIC_API(JSBool)
JS_Stringify(JSContext *cx, jsval *vp, JSObject *replacer, jsval space,
             JSONWriteCallback callback, void *data);

/*
 * Retrieve a toJSON function. If found, set vp to its result.
 */
JS_PUBLIC_API(JSBool)
JS_TryJSON(JSContext *cx, jsval *vp);

/*
 * JSON.parse as specified by ES3.1 (draft)
 */
JS_PUBLIC_API(JSONParser *)
JS_BeginJSONParse(JSContext *cx, jsval *vp);

JS_PUBLIC_API(JSBool)
JS_ConsumeJSONText(JSContext *cx, JSONParser *jp, const jschar *data, uint32 len);

JS_PUBLIC_API(JSBool)
JS_FinishJSONParse(JSContext *cx, JSONParser *jp, jsval reviver);

/************************************************************************/

/*
 * Locale specific string conversion and error message callbacks.
 */
struct JSLocaleCallbacks {
    JSLocaleToUpperCase     localeToUpperCase;
    JSLocaleToLowerCase     localeToLowerCase;
    JSLocaleCompare         localeCompare;
    JSLocaleToUnicode       localeToUnicode;
    JSErrorCallback         localeGetErrorMessage;
};

/*
 * Establish locale callbacks. The pointer must persist as long as the
 * JSContext.  Passing NULL restores the default behaviour.
 */
extern JS_PUBLIC_API(void)
JS_SetLocaleCallbacks(JSContext *cx, JSLocaleCallbacks *callbacks);

/*
 * Return the address of the current locale callbacks struct, which may
 * be NULL.
 */
extern JS_PUBLIC_API(JSLocaleCallbacks *)
JS_GetLocaleCallbacks(JSContext *cx);

/************************************************************************/

/*
 * Error reporting.
 */

/*
 * Report an exception represented by the sprintf-like conversion of format
 * and its arguments.  This exception message string is passed to a pre-set
 * JSErrorReporter function (set by JS_SetErrorReporter; see jspubtd.h for
 * the JSErrorReporter typedef).
 */
extern JS_PUBLIC_API(void)
JS_ReportError(JSContext *cx, const char *format, ...);

/*
 * Use an errorNumber to retrieve the format string, args are char *
 */
extern JS_PUBLIC_API(void)
JS_ReportErrorNumber(JSContext *cx, JSErrorCallback errorCallback,
                     void *userRef, const uintN errorNumber, ...);

/*
 * Use an errorNumber to retrieve the format string, args are jschar *
 */
extern JS_PUBLIC_API(void)
JS_ReportErrorNumberUC(JSContext *cx, JSErrorCallback errorCallback,
                     void *userRef, const uintN errorNumber, ...);

/*
 * As above, but report a warning instead (JSREPORT_IS_WARNING(report.flags)).
 * Return true if there was no error trying to issue the warning, and if the
 * warning was not converted into an error due to the JSOPTION_WERROR option
 * being set, false otherwise.
 */
extern JS_PUBLIC_API(JSBool)
JS_ReportWarning(JSContext *cx, const char *format, ...);

extern JS_PUBLIC_API(JSBool)
JS_ReportErrorFlagsAndNumber(JSContext *cx, uintN flags,
                             JSErrorCallback errorCallback, void *userRef,
                             const uintN errorNumber, ...);

extern JS_PUBLIC_API(JSBool)
JS_ReportErrorFlagsAndNumberUC(JSContext *cx, uintN flags,
                               JSErrorCallback errorCallback, void *userRef,
                               const uintN errorNumber, ...);

/*
 * Complain when out of memory.
 */
extern JS_PUBLIC_API(void)
JS_ReportOutOfMemory(JSContext *cx);

/*
 * Complain when an allocation size overflows the maximum supported limit.
 */
extern JS_PUBLIC_API(void)
JS_ReportAllocationOverflow(JSContext *cx);

struct JSErrorReport {
    const char      *filename;      /* source file name, URL, etc., or null */
    uintN           lineno;         /* source line number */
    const char      *linebuf;       /* offending source line without final \n */
    const char      *tokenptr;      /* pointer to error token in linebuf */
    const jschar    *uclinebuf;     /* unicode (original) line buffer */
    const jschar    *uctokenptr;    /* unicode (original) token pointer */
    uintN           flags;          /* error/warning, etc. */
    uintN           errorNumber;    /* the error number, e.g. see js.msg */
    const jschar    *ucmessage;     /* the (default) error message */
    const jschar    **messageArgs;  /* arguments for the error message */
};

/*
 * JSErrorReport flag values.  These may be freely composed.
 */
#define JSREPORT_ERROR      0x0     /* pseudo-flag for default case */
#define JSREPORT_WARNING    0x1     /* reported via JS_ReportWarning */
#define JSREPORT_EXCEPTION  0x2     /* exception was thrown */
#define JSREPORT_STRICT     0x4     /* error or warning due to strict option */

/*
 * This condition is an error in strict mode code, a warning if
 * JS_HAS_STRICT_OPTION(cx), and otherwise should not be reported at
 * all.  We check the strictness of the context's top frame's script;
 * where that isn't appropriate, the caller should do the right checks
 * itself instead of using this flag.
 */
#define JSREPORT_STRICT_MODE_ERROR 0x8

/*
 * If JSREPORT_EXCEPTION is set, then a JavaScript-catchable exception
 * has been thrown for this runtime error, and the host should ignore it.
 * Exception-aware hosts should also check for JS_IsExceptionPending if
 * JS_ExecuteScript returns failure, and signal or propagate the exception, as
 * appropriate.
 */
#define JSREPORT_IS_WARNING(flags)      (((flags) & JSREPORT_WARNING) != 0)
#define JSREPORT_IS_EXCEPTION(flags)    (((flags) & JSREPORT_EXCEPTION) != 0)
#define JSREPORT_IS_STRICT(flags)       (((flags) & JSREPORT_STRICT) != 0)
#define JSREPORT_IS_STRICT_MODE_ERROR(flags) (((flags) &                      \
                                              JSREPORT_STRICT_MODE_ERROR) != 0)

extern JS_PUBLIC_API(JSErrorReporter)
JS_SetErrorReporter(JSContext *cx, JSErrorReporter er);

/************************************************************************/

/*
 * Regular Expressions.
 */
#define JSREG_FOLD      0x01    /* fold uppercase to lowercase */
#define JSREG_GLOB      0x02    /* global exec, creates array of matches */
#define JSREG_MULTILINE 0x04    /* treat ^ and $ as begin and end of line */
#define JSREG_STICKY    0x08    /* only match starting at lastIndex */
#define JSREG_FLAT      0x10    /* parse as a flat regexp */
#define JSREG_NOCOMPILE 0x20    /* do not try to compile to native code */

extern JS_PUBLIC_API(JSObject *)
JS_NewRegExpObject(JSContext *cx, char *bytes, size_t length, uintN flags);

extern JS_PUBLIC_API(JSObject *)
JS_NewUCRegExpObject(JSContext *cx, jschar *chars, size_t length, uintN flags);

extern JS_PUBLIC_API(void)
JS_SetRegExpInput(JSContext *cx, JSString *input, JSBool multiline);

extern JS_PUBLIC_API(void)
JS_ClearRegExpStatics(JSContext *cx);

extern JS_PUBLIC_API(void)
JS_ClearRegExpRoots(JSContext *cx);

/* TODO: compile, exec, get/set other statics... */

/************************************************************************/

extern JS_PUBLIC_API(JSBool)
JS_IsExceptionPending(JSContext *cx);

extern JS_PUBLIC_API(JSBool)
JS_GetPendingException(JSContext *cx, jsval *vp);

extern JS_PUBLIC_API(void)
JS_SetPendingException(JSContext *cx, jsval v);

extern JS_PUBLIC_API(void)
JS_ClearPendingException(JSContext *cx);

extern JS_PUBLIC_API(JSBool)
JS_ReportPendingException(JSContext *cx);

/*
 * Save the current exception state.  This takes a snapshot of cx's current
 * exception state without making any change to that state.
 *
 * The returned state pointer MUST be passed later to JS_RestoreExceptionState
 * (to restore that saved state, overriding any more recent state) or else to
 * JS_DropExceptionState (to free the state struct in case it is not correct
 * or desirable to restore it).  Both Restore and Drop free the state struct,
 * so callers must stop using the pointer returned from Save after calling the
 * Release or Drop API.
 */
extern JS_PUBLIC_API(JSExceptionState *)
JS_SaveExceptionState(JSContext *cx);

extern JS_PUBLIC_API(void)
JS_RestoreExceptionState(JSContext *cx, JSExceptionState *state);

extern JS_PUBLIC_API(void)
JS_DropExceptionState(JSContext *cx, JSExceptionState *state);

/*
 * If the given value is an exception object that originated from an error,
 * the exception will contain an error report struct, and this API will return
 * the address of that struct.  Otherwise, it returns NULL.  The lifetime of
 * the error report struct that might be returned is the same as the lifetime
 * of the exception object.
 */
extern JS_PUBLIC_API(JSErrorReport *)
JS_ErrorFromException(JSContext *cx, jsval v);

/*
 * Given a reported error's message and JSErrorReport struct pointer, throw
 * the corresponding exception on cx.
 */
extern JS_PUBLIC_API(JSBool)
JS_ThrowReportedError(JSContext *cx, const char *message,
                      JSErrorReport *reportp);

/*
 * Throws a StopIteration exception on cx.
 */
extern JS_PUBLIC_API(JSBool)
JS_ThrowStopIteration(JSContext *cx);

/*
 * Associate the current thread with the given context.  This is done
 * implicitly by JS_NewContext.
 *
 * Returns the old thread id for this context, which should be treated as
 * an opaque value.  This value is provided for comparison to 0, which
 * indicates that ClearContextThread has been called on this context
 * since the last SetContextThread, or non-0, which indicates the opposite.
 */
extern JS_PUBLIC_API(jsword)
JS_GetContextThread(JSContext *cx);

extern JS_PUBLIC_API(jsword)
JS_SetContextThread(JSContext *cx);

extern JS_PUBLIC_API(jsword)
JS_ClearContextThread(JSContext *cx);

/************************************************************************/

#ifdef DEBUG
#define JS_GC_ZEAL 1
#endif

#ifdef JS_GC_ZEAL
extern JS_PUBLIC_API(void)
JS_SetGCZeal(JSContext *cx, uint8 zeal);
#endif

JS_END_EXTERN_C

/************************************************************************/

#ifdef __cplusplus

/*
 * Spanky new C++ API
 */

namespace js {

struct NullTag {
    explicit NullTag() {}
};

struct UndefinedTag {
    explicit UndefinedTag() {}
};

struct Int32Tag {
    explicit Int32Tag(int32 i32) : i32(i32) {}
    int32 i32;
};

struct DoubleTag {
    explicit DoubleTag(double dbl) : dbl(dbl) {}
    double dbl;
};

struct NumberTag {
    explicit NumberTag(double dbl) : dbl(dbl) {}
    double dbl;
};

struct StringTag {
    explicit StringTag(JSString *str) : str(str) {}
    JSString *str;
};

struct FunObjTag {
    explicit FunObjTag(JSObject &obj) : obj(obj) {}
    JSObject &obj;
};

struct FunObjOrNull {
    explicit FunObjOrNull(JSObject *obj) : obj(obj) {}
    JSObject *obj;
};

struct FunObjOrUndefinedTag {
    explicit FunObjOrUndefinedTag(JSObject *obj) : obj(obj) {}
    JSObject *obj;
};

struct NonFunObjTag {
    explicit NonFunObjTag(JSObject &obj) : obj(obj) {}
    JSObject &obj;
};

struct NonFunObjOrNullTag {
    explicit NonFunObjOrNullTag(JSObject *obj) : obj(obj) {}
    JSObject *obj;
};

struct ObjectTag {
    explicit ObjectTag(JSObject &obj) : obj(obj) {}
    JSObject &obj;
};

struct ObjectOrNullTag {
    explicit ObjectOrNullTag(JSObject *obj) : obj(obj) {}
    JSObject *obj;
};

struct BooleanTag {
    explicit BooleanTag(bool boo) : boo(boo) {}
    bool boo;
};

struct PrivateVoidPtrTag {
    explicit PrivateVoidPtrTag(void *ptr) : ptr(ptr) {}
    void *ptr;
};

/*
 * While there is a single representation for values, there are two declared
 * types for dealing with these values: jsval and js::Value. jsval allows a
 * high-degree of source compatibility with the old word-sized boxed value
 * representation. js::Value is a new C++-only type and more accurately
 * reflects the current, unboxed value representation. As these two types are
 * layout-compatible, pointers to jsval and js::Value are interchangeable and
 * may be cast safely and canonically using js::Jsvalify and js::asValue.
 */
class Value
{
    /*
     * Generally, we'd like to keep the exact representation encapsulated so
     * that it may be tweaked in the future. Engine internals that need to
     * break this encapsulation should be listed as friends below. Also see
     * uses of public jsval members in jsapi.h/jspubtd.h.
     */
    friend class PrimitiveValue;

  protected:
    /* Type masks */

        template <int I> class T {};

    void staticAssertions() {
        JS_STATIC_ASSERT(sizeof(JSValueMask16) == 2);
        JS_STATIC_ASSERT(JSVAL_NANBOX_PATTERN == 0xFFFF);
        JS_STATIC_ASSERT(sizeof(JSValueMask32) == 4);
        JS_STATIC_ASSERT(JSVAL_MASK32_CLEAR == 0xFFFF0000);
        JS_STATIC_ASSERT(sizeof(JSBool) == 4);
        JS_STATIC_ASSERT(sizeof(JSWhyMagic) <= 4);
        JS_STATIC_ASSERT(sizeof(((jsval_layout *)0)->s.payload) == 4);
        JS_STATIC_ASSERT(sizeof(jsval) == 8);
    }

    jsval_layout data;

  public:
    /* Constructors */

    /* Value's default constructor leaves Value undefined */
    Value() {}

    /* Construct a Value of a single type */

    Value(NullTag)                     { setNull(); }
    Value(UndefinedTag)                { setUndefined(); }
    Value(Int32Tag arg)                { setInt32(arg.i32); }
    Value(DoubleTag arg)               { setDouble(arg.dbl); }
    Value(StringTag arg)               { setString(arg.str); }
    Value(FunObjTag arg)               { setFunObj(arg.obj); }
    Value(NonFunObjTag arg)            { setNonFunObj(arg.obj); }
    Value(BooleanTag arg)              { setBoolean(arg.boo); }
    Value(JSWhyMagic arg)              { setMagic(arg); }

    /* Construct a Value of a type dynamically chosen from a set of types */

    Value(FunObjOrNull arg)            { setFunObjOrNull(arg.obj); }
    Value(FunObjOrUndefinedTag arg)    { setFunObjOrUndefined(arg.obj); }
    Value(NonFunObjOrNullTag arg)      { setNonFunObjOrNull(arg.obj); }
    inline Value(NumberTag arg);
    inline Value(ObjectTag arg);
    inline Value(ObjectOrNullTag arg);

    /* Change to a Value of a single type */

    void setNull() {
        data.asBits = JSVAL_NULL;
    }

    void setUndefined() {
        data.asBits = JSVAL_VOID;
    }

    void setInt32(int32 i) {
        data = INT32_TO_JSVAL_IMPL(i);
    }

    int32 &asInt32Ref() {
        JS_ASSERT(isInt32());
        return data.s.payload.i32;
    }

    void setDouble(double d) {
        ASSERT_DOUBLE_ALIGN();
        data = DOUBLE_TO_JSVAL_IMPL(d);
    }

    double &asDoubleRef() {
        ASSERT_DOUBLE_ALIGN();
        JS_ASSERT(isDouble());
        return data.asDouble;
    }

    void setString(JSString *str) {
        data = STRING_TO_JSVAL_IMPL(str);
    }

    void setFunObj(JSObject &arg) {
        JS_ASSERT(JS_ObjectIsFunction(NULL, &arg));
        data = OBJECT_TO_JSVAL_IMPL(JSVAL_MASK32_FUNOBJ, &arg);
    }

    void setNonFunObj(JSObject &arg) {
        JS_ASSERT(!JS_ObjectIsFunction(NULL, &arg));
        data = OBJECT_TO_JSVAL_IMPL(JSVAL_MASK32_NONFUNOBJ, &arg);
    }

    void setBoolean(bool b) {
        data = BOOLEAN_TO_JSVAL_IMPL(b);
    }

    void setMagic(JSWhyMagic why) {
        data = MAGIC_TO_JSVAL_IMPL(why);
    }

    /* Change to a Value of a type dynamically chosen from a set of types */

    void setNumber(uint32 ui) {
        if (ui > JSVAL_INT_MAX)
            data = DOUBLE_TO_JSVAL_IMPL(ui);
        else
            data = INT32_TO_JSVAL_IMPL((int32)ui);
    }

    inline void setNumber(double d);

    void setFunObjOrNull(JSObject *arg) {
        JS_ASSERT_IF(arg, JS_ObjectIsFunction(NULL, arg));
        JSValueMask32 mask = arg ? JSVAL_MASK32_FUNOBJ : JSVAL_MASK32_NULL;
        data = OBJECT_TO_JSVAL_IMPL(mask, arg);
    }

    void setFunObjOrUndefined(JSObject *arg) {
        JS_ASSERT_IF(arg, JS_ObjectIsFunction(NULL, arg));
        JSValueMask32 mask = arg ? JSVAL_MASK32_FUNOBJ : JSVAL_MASK32_UNDEFINED;
        data = OBJECT_TO_JSVAL_IMPL(mask, arg);
    }

    void setNonFunObjOrNull(JSObject *arg) {
        JS_ASSERT_IF(arg, !JS_ObjectIsFunction(NULL, arg));
        JSValueMask32 mask = arg ? JSVAL_MASK32_NONFUNOBJ : JSVAL_MASK32_NULL;
        data = OBJECT_TO_JSVAL_IMPL(mask, arg);
    }

    inline void setObject(JSObject &arg);
    inline void setObjectOrNull(JSObject *arg);

    /* Query a Value's type */

    bool isUndefined() const {
        return JSVAL_IS_UNDEFINED_IMPL(data);
    }

    bool isNull() const {
        return JSVAL_IS_NULL_IMPL(data);
    }

    bool isNullOrUndefined() const {
        return JSVAL_IS_SINGLETON_IMPL(data);
    }

    bool isInt32() const {
        return data.s.mask32 == JSVAL_MASK32_INT32;
    }

    bool isInt32(int32 i32) const {
        return JSVAL_IS_SPECIFIC_INT32_IMPL(data, i32);
    }

    bool isDouble() const {
        return JSVAL_IS_DOUBLE_IMPL(data);
    }

    bool isNumber() const {
        return JSVAL_IS_NUMBER_IMPL(data);
    }

    bool isString() const {
        return data.s.mask32 == JSVAL_MASK32_STRING;
    }

    bool isNonFunObj() const {
        return data.s.mask32 == JSVAL_MASK32_NONFUNOBJ;
    }

    bool isFunObj() const {
        return data.s.mask32 == JSVAL_MASK32_FUNOBJ;
    }

    bool isObject() const {
        return JSVAL_IS_OBJECT_IMPL(data);
    }

    bool isPrimitive() const {
        return JSVAL_IS_PRIMITIVE_IMPL(data);
    }

    bool isObjectOrNull() const {
        return JSVAL_IS_OBJECT_OR_NULL_IMPL(data);
    }

    bool isGCThing() const {
        return JSVAL_IS_GCTHING_IMPL(data);
    }

    bool isBoolean() const {
        return data.s.mask32 == JSVAL_MASK32_BOOLEAN;
    }

    bool isTrue() const {
        return JSVAL_IS_SPECIFIC_BOOLEAN(data, true);
    }

    bool isFalse() const {
        return JSVAL_IS_SPECIFIC_BOOLEAN(data, false);
    }

    bool isMagic() const {
        return data.s.mask32 == JSVAL_MASK32_MAGIC;
    }

    bool isMagic(JSWhyMagic why) const {
        JS_ASSERT_IF(isMagic(), data.s.payload.why == why);
        return isMagic();
    }

    int32 traceKind() const {
        JS_ASSERT(isGCThing());
        return JSVAL_TRACE_KIND_IMPL(data);
    }

#ifdef DEBUG
    JSWhyMagic whyMagic() const {
        JS_ASSERT(isMagic());
        return data.s.payload.why;
    }
#endif

    /* Comparison */

    bool operator==(const Value &rhs) const {
        return data.asBits == rhs.data.asBits;
    }

    bool operator!=(const Value &rhs) const {
        return data.asBits != rhs.data.asBits;
    }

    friend bool SamePrimitiveTypeOrBothObjects(const Value &lhs, const Value &rhs) {
        return JSVAL_SAME_PRIMITIVE_TYPE_OR_BOTH_OBJECTS_IMPL(lhs.data, rhs.data);
    }

    friend bool BothInt32(const Value &lhs, const Value &rhs) {
        return JSVAL_BOTH_INT32_IMPL(lhs.data, rhs.data);
    }

    friend bool BothString(const Value &lhs, const Value &rhs) {
        return JSVAL_BOTH_STRING_IMPL(lhs.data, rhs.data);
    }

    /* Extract a Value's payload */

    int32 asInt32() const {
        JS_ASSERT(isInt32());
        return data.s.payload.i32;
    }

    double asDouble() const {
        JS_ASSERT(isDouble());
        ASSERT_DOUBLE_ALIGN();
        return data.asDouble;
    }

    double asNumber() const {
        JS_ASSERT(isNumber());
        return isDouble() ? asDouble() : double(asInt32());
    }

    JSString *asString() const {
        JS_ASSERT(isString());
        return JSVAL_TO_STRING_IMPL(data);
    }

    JSObject &asNonFunObj() const {
        JS_ASSERT(isNonFunObj());
        return *JSVAL_TO_OBJECT_IMPL(data);
    }

    JSObject &asFunObj() const {
        JS_ASSERT(isFunObj());
        return *JSVAL_TO_OBJECT_IMPL(data);
    }

    JSObject &asObject() const {
        JS_ASSERT(isObject());
        return *JSVAL_TO_OBJECT_IMPL(data);
    }

    JSObject *asObjectOrNull() const {
        JS_ASSERT(isObjectOrNull());
        return JSVAL_TO_OBJECT_IMPL(data);
    }

    void *asGCThing() const {
        JS_ASSERT(isGCThing());
        return JSVAL_TO_GCTHING_IMPL(data);
    }

    bool asBoolean() const {
        JS_ASSERT(isBoolean());
        return data.s.payload.boo;
    }

    uint32 asRawUint32() const {
        JS_ASSERT(!isDouble());
        return data.s.payload.u32;
    }

    /* Swap two Values */

    void swap(Value &rhs) {
        jsval_layout tmp = data;
        data = rhs.data;
        rhs.data = tmp;
    }

    /*
     * Private API
     *
     * Private setters/getters allow the caller to read/write arbitrary types
     * that fit in the 64-bit payload. It is the caller's responsibility, after
     * storing to a value with setPrivateX to only read with getPrivateX.
     * Privates values are given a valid type of Int32Tag and are thus GC-safe.
     */

    Value(PrivateVoidPtrTag arg) {
        setPrivateVoidPtr(arg.ptr);
    }

    void setPrivateVoidPtr(void *ptr) {
		data = PRIVATE_TO_JSVAL_IMPL(ptr);
    }

    void *asPrivateVoidPtr() const {
		JS_ASSERT(isDouble());
		return JSVAL_TO_PRIVATE_IMPL(data);
    }

    void setPrivateUint32(uint32 u) {
		setInt32((int32)u);
    }

    uint32 asPrivateUint32() const {
		return (uint32)asInt32();
    }

    uint32 &asPrivateUint32Ref() {
		JS_ASSERT(isInt32());
		return data.s.payload.u32;
    }
} VALUE_ALIGNMENT;

/*
 * As asserted above, js::Value and jsval are layout equivalent. To provide
 * widespread casting, the following safe casts are provided.
 */
static inline jsval *        Jsvalify(Value *v)        { return (jsval *)v; }
static inline const jsval *  Jsvalify(const Value *v)  { return (const jsval *)v; }
static inline jsval &        Jsvalify(Value &v)        { return (jsval &)v; }
static inline const jsval &  Jsvalify(const Value &v)  { return (const jsval &)v; }
static inline Value *        Valueify(jsval *v)        { return (Value *)v; }
static inline const Value *  Valueify(const jsval *v)  { return (const Value *)v; }
static inline Value **       Valueify(jsval **v)       { return (Value **)v; }
static inline Value &        Valueify(jsval &v)        { return (Value &)v; }
static inline const Value &  Valueify(const jsval &v)  { return (const Value &)v; }

/* Convenience inlines. */
static inline Value undefinedValue() { return UndefinedTag(); }
static inline Value nullValue()      { return NullTag(); }

/*
 * js::Class is layout compatible and thus 
 */
struct Class {
    const char          *name;
    uint32              flags;

    /* Mandatory non-null function pointer members. */
    PropertyOp          addProperty;
    PropertyOp          delProperty;
    PropertyOp          getProperty;
    PropertyOp          setProperty;
    JSEnumerateOp       enumerate;
    JSResolveOp         resolve;
    ConvertOp           convert;
    JSFinalizeOp        finalize;

    /* Optionally non-null members start here. */
    GetObjectOps        getObjectOps;
    CheckAccessOp       checkAccess;
    Native              call;
    Native              construct;
    JSXDRObjectOp       xdrObject;
    HasInstanceOp       hasInstance;
    JSMarkOp            mark;
    JSReserveSlotsOp    reserveSlots;
};

JS_STATIC_ASSERT(sizeof(JSClass) == sizeof(Class));

struct ExtendedClass {
    Class               base;
    EqualityOp          equality;
    JSObjectOp          outerObject;
    JSObjectOp          innerObject;
    JSIteratorOp        iteratorObject;
    JSObjectOp          wrappedObject;          /* NB: infallible, null
                                                   returns are treated as
                                                   the original object */
    void                (*reserved0)(void);
    void                (*reserved1)(void);
    void                (*reserved2)(void);
};

JS_STATIC_ASSERT(sizeof(JSExtendedClass) == sizeof(ExtendedClass));

static JS_ALWAYS_INLINE JSClass *         Jsvalify(Class *c)           { return (JSClass *)c; }
static JS_ALWAYS_INLINE Class *           Valueify(JSClass *c)         { return (Class *)c; }
static JS_ALWAYS_INLINE JSExtendedClass * Jsvalify(ExtendedClass *c)   { return (JSExtendedClass *)c; }
static JS_ALWAYS_INLINE ExtendedClass *   Valueify(JSExtendedClass *c) { return (ExtendedClass *)c; }

static const PropertyOp PropertyStub     = (PropertyOp)JS_PropertyStub;
static const JSEnumerateOp EnumerateStub = JS_EnumerateStub;
static const JSResolveOp ResolveStub     = JS_ResolveStub;
static const ConvertOp ConvertStub       = (ConvertOp)JS_ConvertStub;
static const JSFinalizeOp FinalizeStub   = JS_FinalizeStub;

} /* namespace js */
#endif  /* __cplusplus */

#endif /* jsapi_h___ */
