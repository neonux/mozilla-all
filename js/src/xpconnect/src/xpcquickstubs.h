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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jason Orendorff <jorendorff@mozilla.com>
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

#ifndef xpcquickstubs_h___
#define xpcquickstubs_h___

/* xpcquickstubs.h - Support functions used only by quick stubs. */

class XPCCallContext;

#define XPC_QS_NULL_INDEX  ((size_t) -1)

struct xpc_qsPropertySpec {
    const char *name;
    JSPropertyOp getter;
    JSPropertyOp setter;
};

struct xpc_qsFunctionSpec {
    const char *name;
    JSFastNative native;
    uintN arity;
};

/** A table mapping interfaces to quick stubs. */
struct xpc_qsHashEntry {
    nsID iid;
    const xpc_qsPropertySpec *properties;
    const xpc_qsFunctionSpec *functions;
    // These last two fields index to other entries in the same table.
    // XPC_QS_NULL_ENTRY indicates there are no more entries in the chain.
    size_t parentInterface;
    size_t chain;
};

JSBool
xpc_qsDefineQuickStubs(JSContext *cx, JSObject *proto, uintN extraFlags,
                       PRUint32 ifacec, const nsIID **interfaces,
                       PRUint32 tableSize, const xpc_qsHashEntry *table);

/** Raise an exception on @a cx and return JS_FALSE. */
JSBool
xpc_qsThrow(JSContext *cx, nsresult rv);

/** Elaborately fail after an XPCOM method returned rv. */
JSBool
xpc_qsThrowGetterSetterFailed(JSContext *cx, nsresult rv,
                              XPCWrappedNative *wrapper, jsval memberId);

JSBool
xpc_qsThrowMethodFailed(JSContext *cx, nsresult rv,
                        XPCWrappedNative *wrapper, jsval *vp);

JSBool
xpc_qsThrowMethodFailedWithCcx(XPCCallContext &ccx, nsresult rv);

/** Elaborately fail after converting an argument fails. */
void
xpc_qsThrowBadArg(JSContext *cx, nsresult rv,
                  XPCWrappedNative *wrapper, jsval *vp, uintN paramnum);

void
xpc_qsThrowBadArgWithCcx(XPCCallContext &ccx, nsresult rv, uintN paramnum);

void
xpc_qsThrowBadSetterValue(JSContext *cx, nsresult rv,
                          XPCWrappedNative *wrapper, jsval propId);


/* Functions for converting values between COM and JS. */

inline JSBool
xpc_qsInt32ToJsval(JSContext *cx, PRInt32 i, jsval *rv)
{
    if(INT_FITS_IN_JSVAL(i))
    {
        *rv = INT_TO_JSVAL(i);
        return JS_TRUE;
    }
    return JS_NewDoubleValue(cx, i, rv);
}

inline JSBool
xpc_qsUint32ToJsval(JSContext *cx, PRUint32 u, jsval *rv)
{
    if(u <= JSVAL_INT_MAX)
    {
        *rv = INT_TO_JSVAL(u);
        return JS_TRUE;
    }
    return JS_NewDoubleValue(cx, u, rv);
}

#ifdef HAVE_LONG_LONG

#define INT64_TO_DOUBLE(i)      ((jsdouble) (i))
// Win32 can't handle uint64 to double conversion
#define UINT64_TO_DOUBLE(u)     ((jsdouble) (int64) (u))

#else

inline jsdouble
INT64_TO_DOUBLE(const int64 &v)
{
    jsdouble d;
    LL_L2D(d, v);
    return d;
}

// if !HAVE_LONG_LONG, then uint64 is a typedef of int64
#define UINT64_TO_DOUBLE INT64_TO_DOUBLE

#endif

inline JSBool
xpc_qsInt64ToJsval(JSContext *cx, PRInt64 i, jsval *rv)
{
    double d = INT64_TO_DOUBLE(i);
    return JS_NewNumberValue(cx, d, rv);
}

inline JSBool
xpc_qsUint64ToJsval(JSContext *cx, PRUint64 u, jsval *rv)
{
    double d = UINT64_TO_DOUBLE(u);
    return JS_NewNumberValue(cx, d, rv);
}


/* Classes for converting jsvals to string types. */

template <class S, class T>
class xpc_qsBasicString
{
public:
    typedef S interface_type;
    typedef T implementation_type;

    ~xpc_qsBasicString()
    {
        if (mValid)
            Ptr()->~implementation_type();
    }

    JSBool IsValid() { return mValid; }

    implementation_type *Ptr()
    {
        return reinterpret_cast<implementation_type *>(mBuf);
    }

    operator interface_type &()
    {
        return *Ptr();
    }

protected:
    /*
     * Neither field is initialized; that is left to the derived class
     * constructor. However, the destructor destroys the string object
     * stored in mBuf, if mValid is true.
     */
    void *mBuf[JS_HOWMANY(sizeof(implementation_type), sizeof(void *))];
    JSBool mValid;
};

/**
 * Class for converting a jsval to DOMString.
 *
 *     xpc_qsDOMString arg0(cx, &argv[0]);
 *     if (!arg0.IsValid())
 *         return JS_FALSE;
 *
 * The second argument to the constructor is an in-out parameter. It must
 * point to a rooted jsval, such as a JSNative argument or return value slot.
 * The value in the jsval on entry is converted to a string. The constructor
 * may overwrite that jsval with a string value, to protect the characters of
 * the string from garbage collection. The caller must leave the jsval alone
 * for the lifetime of the xpc_qsDOMString.
 */
class xpc_qsDOMString : public xpc_qsBasicString<nsAString, nsDependentString>
{
public:
    xpc_qsDOMString(JSContext *cx, jsval *pval);
};

/**
 * The same as xpc_qsDOMString, but with slightly different conversion behavior,
 * corresponding to the [astring] magic XPIDL annotation rather than [domstring].
 */
class xpc_qsAString : public xpc_qsBasicString<nsAString, nsDependentString>
{
public:
    xpc_qsAString(JSContext *cx, jsval *pval);
};

/**
 * Like xpc_qsDOMString and xpc_qsAString, but for XPIDL native types annotated
 * with [cstring] rather than [domstring] or [astring].
 */
class xpc_qsACString : public xpc_qsBasicString<nsACString, nsCString>
{
public:
    xpc_qsACString(JSContext *cx, jsval *pval);
};

/**
 * Convert a jsval to char*, returning JS_TRUE on success.
 *
 * @param cx
 *      A context.
 * @param pval
 *     In/out. *pval is the jsval to convert; the function may write to *pval,
 *     using it as a GC root (like xpc_qsDOMString's constructor).
 * @param pstr
 *     Out. On success *pstr receives the converted string or NULL if *pval is
 *     null or undefined. Unicode data is garbled as with JS_GetStringBytes.
 */
JSBool
xpc_qsJsvalToCharStr(JSContext *cx, jsval *pval, char **pstr);

JSBool
xpc_qsJsvalToWcharStr(JSContext *cx, jsval *pval, PRUnichar **pstr);


/** Convert an nsAString to jsval, returning JS_TRUE on success. */
JSBool
xpc_qsStringToJsval(JSContext *cx, const nsAString &str, jsval *rval);

JSBool
xpc_qsUnwrapThisImpl(JSContext *cx,
                     JSObject *obj,
                     const nsIID &iid,
                     void **ppThis,
                     XPCWrappedNative **ppWrapper);

/**
 * Search @a obj and its prototype chain for an XPCOM object that implements
 * the interface T.
 *
 * If an object implementing T is found, AddRef it, store the pointer in
 * @a *ppThis, store a pointer to the wrapper in @a *ppWrapper, and return
 * JS_TRUE. Otherwise, raise an exception on @a cx and return JS_FALSE.
 *
 * This does not consult inner objects. It does support XPConnect tear-offs
 * and it sees through XOWs, XPCNativeWrappers, and SafeJSObjectWrappers.
 *
 * Requires a request on @a cx.
 */
template <class T>
inline JSBool
xpc_qsUnwrapThis(JSContext *cx,
                 JSObject *obj,
                 T **ppThis,
                 XPCWrappedNative **ppWrapper)
{
    return xpc_qsUnwrapThisImpl(cx,
                                obj,
                                NS_GET_TEMPLATE_IID(T),
                                reinterpret_cast<void **>(ppThis),
                                ppWrapper);
}

JSBool
xpc_qsUnwrapThisFromCcxImpl(XPCCallContext &ccx,
                            const nsIID &iid,
                            void **ppThis);

/**
 * Alternate implementation of xpc_qsUnwrapThis using information already
 * present in the given XPCCallContext.
 */
template <class T>
inline JSBool
xpc_qsUnwrapThisFromCcx(XPCCallContext &ccx,
                        T **ppThis)
{
    return xpc_qsUnwrapThisFromCcxImpl(ccx, NS_GET_TEMPLATE_IID(T),
                                       reinterpret_cast<void **>(ppThis));
}

nsresult
xpc_qsUnwrapArgImpl(JSContext *cx, jsval v, const nsIID &iid, void **ppArg);

/** Convert a jsval to an XPCOM pointer. */
template <class T>
inline nsresult
xpc_qsUnwrapArg(JSContext *cx, jsval v, T **ppArg)
{
    return xpc_qsUnwrapArgImpl(cx, v, NS_GET_TEMPLATE_IID(T),
                               reinterpret_cast<void **>(ppArg));
}

/** Convert an XPCOM pointer to jsval. Return JS_TRUE on success. */
JSBool
xpc_qsXPCOMObjectToJsval(XPCCallContext &ccx,
                         nsISupports *p,
                         const nsIID &iid,
                         jsval *rval);

/**
 * Convert a variant to jsval. Return JS_TRUE on success.
 *
 * @a paramNum is used in error messages. XPConnect treats the return
 * value as a parameter in this regard.
 */
JSBool
xpc_qsVariantToJsval(XPCCallContext &ccx,
                     nsIVariant *p,
                     uintN paramNum,
                     jsval *rval);

/**
 * Use this as the setter for readonly attributes. (The IDL readonly
 * keyword does not map to JSPROP_READONLY. Semantic mismatch.)
 *
 * Always fails, with the same error as setting a property that has
 * JSPROP_GETTER but not JSPROP_SETTER.
 */
JSBool
xpc_qsReadOnlySetter(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

#ifdef DEBUG
void
xpc_qsAssertContextOK(JSContext *cx);

#define XPC_QS_ASSERT_CONTEXT_OK(cx) xpc_qsAssertContextOK(cx)
#else
#define XPC_QS_ASSERT_CONTEXT_OK(cx) ((void) 0)
#endif

#endif /* xpcquickstubs_h___ */
