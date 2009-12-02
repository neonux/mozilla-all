/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 *   Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mark Steele <mwsteele@gmail.com>
 *   Vladimir Vukicevic <vladimir@pobox.com>
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

#include "WebGLArrays.h"

#include "NativeJSContext.h"

// TODO:
// XXX: fix overflow in integer mult in ::Set!
// XXX: get rid of code duplication, use inline helpers, templates, or macros
// XXX: Prepare/Zero in initializers is inefficient, we should really
//      just be doing calloc
// XXX: array Set() shouldn't call into the inner Set(), because that
//      repeats the length check and is probably not getting inlined
// write benchmarks

using namespace mozilla;

nsresult
NS_NewWebGLArrayBuffer(nsISupports **aResult)
{
    nsIWebGLArrayBuffer *wgab = new WebGLArrayBuffer();
    if (!wgab)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aResult = wgab);
    return NS_OK;
}

WebGLArrayBuffer::WebGLArrayBuffer(PRUint32 length)
{
    EnsureCapacity(PR_FALSE, length);
}

NS_IMETHODIMP
WebGLArrayBuffer::Initialize(nsISupports *owner,
                             JSContext *cx,
                             JSObject *obj,
                             PRUint32 argc,
                             jsval *argv)
{
    /* Constructor: WebGLArrayBuffer(n) */
    if (JSVAL_IS_NUMBER(argv[0])) {
        uint32 length;
        ::JS_ValueToECMAUint32(cx, argv[0], &length);
        if (length == 0)
            return NS_ERROR_FAILURE;

        EnsureCapacity(PR_FALSE, length);

        return NS_OK;
    }

    return NS_ERROR_DOM_SYNTAX_ERR;
}

/* readonly attribute unsigned long byteLength; */
NS_IMETHODIMP WebGLArrayBuffer::GetByteLength(PRUint32 *aByteLength)
{
    *aByteLength = capacity;
    return NS_OK;
}

/* [noscript, notxpcom] voidPtr GetNativeArrayBuffer (); */
NS_IMETHODIMP_(WebGLArrayBuffer *) WebGLArrayBuffer::GetNativeArrayBuffer()
{
    return this;
}

/* [noscript, notxpcom] voidPtr nativePointer (); */
NS_IMETHODIMP_(void *) WebGLArrayBuffer::NativePointer()
{
    return data;
}

/* [noscript, notxpcom] unsigned long nativeSize (); */
NS_IMETHODIMP_(PRUint32) WebGLArrayBuffer::NativeSize()
{
    return capacity;
}

/*
 * WebGLFloatArray
 */

nsresult
NS_NewWebGLFloatArray(nsISupports **aResult)
{
    nsIWebGLFloatArray *wgfa = new WebGLFloatArray();
    if (!wgfa)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aResult = wgfa);
    return NS_OK;
}

WebGLFloatArray::WebGLFloatArray(PRUint32 length)
    : mOffset(0), mLength(length)
{
    mBuffer = new WebGLArrayBuffer(length * sizeof(float));
}

WebGLFloatArray::WebGLFloatArray(WebGLArrayBuffer *buffer, PRUint32 offset, PRUint32 length)
    : mBuffer(buffer), mOffset(offset), mLength(length)
{
}

WebGLFloatArray::WebGLFloatArray(JSContext *cx, JSObject *arrayObj, jsuint arrayLen)
    : mOffset(0), mLength(arrayLen)
{
    mBuffer = new WebGLArrayBuffer();
    mBuffer->InitFromJSArray(LOCAL_GL_FLOAT, 1, cx, arrayObj, arrayLen);
}

NS_IMETHODIMP
WebGLFloatArray::Initialize(nsISupports *owner,
                            JSContext *cx,
                            JSObject *obj,
                            PRUint32 argc,
                            jsval *argv)
{
    if (JSVAL_IS_NUMBER(argv[0])) {
        uint32 length;
        ::JS_ValueToECMAUint32(cx, argv[0], &length);
        mBuffer = new WebGLArrayBuffer();
        mBuffer->Prepare(LOCAL_GL_FLOAT, 1, length);
        mBuffer->Zero();
        mLength = length;
    } else {
        JSObject *arrayObj;
        jsuint arrayLen;
        jsuint byteOffset = 0;
        jsuint length = 0;

        if (!::JS_ConvertArguments(cx, argc, argv, "o/uu", &arrayObj, &byteOffset, &length) ||
            arrayObj == NULL)
        {
            return NS_ERROR_DOM_SYNTAX_ERR;
        }

        if (::JS_IsArrayObject(cx, arrayObj) &&
            ::JS_GetArrayLength(cx, arrayObj, &arrayLen))
        {
            mBuffer = new WebGLArrayBuffer();
            mBuffer->InitFromJSArray(LOCAL_GL_FLOAT, 1, cx, arrayObj, arrayLen);
            mLength = arrayLen;
        } else {
            nsCOMPtr<nsIWebGLArrayBuffer> canvasObj;
            nsresult rv;
            rv = nsContentUtils::XPConnect()->WrapJS(cx, arrayObj, NS_GET_IID(nsIWebGLArrayBuffer), getter_AddRefs(canvasObj));
            if (NS_FAILED(rv) || !canvasObj) {
                return NS_ERROR_DOM_SYNTAX_ERR;
            }

            mBuffer = canvasObj->GetNativeArrayBuffer();

            if (byteOffset % sizeof(float))
                return NS_ERROR_FAILURE;

            if ((byteOffset + (length * sizeof(float))) > mBuffer->capacity)
                return NS_ERROR_FAILURE;

            if (length > 0) {
                mLength = length;
            } else {
                if ((mBuffer->capacity - byteOffset) % sizeof(float))
                    return NS_ERROR_FAILURE;

                mLength = (mBuffer->capacity - byteOffset) / sizeof(float);
            }
        }
    }

    return NS_OK;
}

/* readonly attribute nsIWebGLArrayBuffer buffer; */
NS_IMETHODIMP WebGLFloatArray::GetBuffer(nsIWebGLArrayBuffer **aBuffer)
{
    NS_ADDREF(*aBuffer = mBuffer);
    return NS_OK;
}

/* readonly attribute unsigned long byteOffset; */
NS_IMETHODIMP WebGLFloatArray::GetByteOffset(PRUint32 *aByteOffset)
{
    *aByteOffset = mOffset;
    return NS_OK;
}

/* readonly attribute unsigned long byteLength; */
NS_IMETHODIMP WebGLFloatArray::GetByteLength(PRUint32 *aByteLength)
{
    *aByteLength = mLength * sizeof(float);
    return NS_OK;
}

/* attribute unsigned long length; */
NS_IMETHODIMP WebGLFloatArray::GetLength(PRUint32 *aLength)
{
    *aLength = mLength;
    return NS_OK;
}

/* unsigned long alignedSizeInBytes (); */
NS_IMETHODIMP WebGLFloatArray::AlignedSizeInBytes(PRUint32 *retval)
{
    *retval = mBuffer->capacity;
    return NS_OK;
}

/* nsIWebGLArray slice (in unsigned long offset, in unsigned long length); */
NS_IMETHODIMP WebGLFloatArray::Slice(PRUint32 offset, PRUint32 length, nsIWebGLArray **retval)
{
    if (length == 0) 
        return NS_ERROR_FAILURE;

    if (offset + length > mBuffer->capacity)
        return NS_ERROR_FAILURE;

    nsIWebGLArray *wga = new WebGLFloatArray(mBuffer, offset, length);
    NS_ADDREF(*retval = wga);
    return NS_OK;
}

/* [IndexGetter] float get (in unsigned long index); */
NS_IMETHODIMP WebGLFloatArray::Get(PRUint32 index, float *retval)
{
    if (index >= mLength)
        return NS_ERROR_FAILURE;

    float *values = static_cast<float*>(mBuffer->data);
    *retval = values[index];

    return NS_OK;
}

void
WebGLFloatArray::Set(PRUint32 index, float value)
{
    if (index >= mLength)
        return;

    float *values = static_cast<float*>(mBuffer->data);
    values[index] = value;
}

/* void set (); */
NS_IMETHODIMP WebGLFloatArray::Set()
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    if (js.argc < 1 || js.argc > 2)
        return NS_ERROR_DOM_SYNTAX_ERR;

    if (JSVAL_IS_NUMBER(js.argv[0])) {
        if (js.argc != 2)
            return NS_ERROR_DOM_SYNTAX_ERR;

        uint32 index;
        ::JS_ValueToECMAUint32(js.ctx, js.argv[0], &index);

        jsdouble value;
        ::JS_ValueToNumber(js.ctx, js.argv[1], &value);

        if (index >= mLength)
            return NS_ERROR_FAILURE;

        Set(index, (float) value);
    } else {
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    return NS_OK;
}

NS_IMETHODIMP_(PRUint32) WebGLFloatArray::NativeType()
{
    return mBuffer->type;
}

/* [noscript, notxpcom] voidPtr nativePointer (); */
NS_IMETHODIMP_(void *) WebGLFloatArray::NativePointer()
{
    return mBuffer->data;
}

/* [noscript, notxpcom] unsigned long nativeSize (); */
NS_IMETHODIMP_(PRUint32) WebGLFloatArray::NativeSize()
{
    return mBuffer->capacity;
}

/* [noscript, notxpcom] unsigned long nativeElementSize (); */
NS_IMETHODIMP_(PRUint32) WebGLFloatArray::NativeElementSize()
{
    return mBuffer->ElementSize();
}

/* [noscript, notxpcom] unsigned long nativeCount (); */
NS_IMETHODIMP_(PRUint32) WebGLFloatArray::NativeCount()
{
    return mBuffer->length;
}

// nsIXPCScriptable
#define XPC_MAP_CLASSNAME WebGLFloatArray
#define XPC_MAP_QUOTED_CLASSNAME "WebGLFloatArray"
#define XPC_MAP_WANT_SETPROPERTY
#define XPC_MAP_WANT_GETPROPERTY
#define XPC_MAP_WANT_NEWRESOLVE
#define XPC_MAP_FLAGS nsIXPCScriptable::USE_JSSTUB_FOR_ADDPROPERTY
#include "xpc_map_end.h"

PRBool WebGLFloatArray::JSValToIndex(JSContext *cx, jsval id, PRUint32 *retval) {
    PRBool ok = PR_FALSE;
    PRUint32 index;

    if (JSVAL_IS_INT(id)) {
        index = JSVAL_TO_INT(id);
        ok = PR_TRUE;
    } else {
        ok = JS_ValueToECMAUint32(cx, id, &index);
    }

    if (!ok || index >= mLength)
        return PR_FALSE;

    *retval = index;
    return PR_TRUE;
}

NS_IMETHODIMP WebGLFloatArray::GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                                           JSObject * obj, jsval id, jsval * vp, PRBool *_retval)
{
    PRUint32 index;
    
    if (!JSValToIndex(cx, id, &index)) {
        *_retval = PR_FALSE;
        return NS_ERROR_INVALID_ARG;
    }

    float val;
    Get(index, &val);
    *_retval = JS_NewNumberValue(cx, val, vp);

    return NS_OK;
}

NS_IMETHODIMP WebGLFloatArray::SetProperty(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                       JSObject * obj, jsval id, jsval * vp, PRBool *_retval)
{
    PRUint32 index;
    float val;

    if (!JSValToIndex(cx, id, &index)) {
        *_retval = PR_FALSE;
        return NS_ERROR_INVALID_ARG;
    }

    PRBool ok = PR_FALSE;

    if (JSVAL_IS_INT(*vp)) {
        val = JSVAL_TO_INT(*vp);
        ok = PR_TRUE;
    } else {
        jsdouble dval;
        ok = JS_ValueToNumber(cx, *vp, &dval);
        val = dval;
    }

    if (!ok) {
        *_retval = PR_FALSE;
        return NS_ERROR_INVALID_ARG;
    }

    Set(index, val);
    return NS_OK;
}

NS_IMETHODIMP WebGLFloatArray::NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                      JSObject * obj, jsval id, PRUint32 flags, JSObject * *objp,
                      PRBool *_retval)
{
    PRUint32 index;
    PRBool ok = JSValToIndex(cx, id, &index);

    if (ok) {
        *_retval = PR_TRUE;
        *objp = obj;
    } else {
        *_retval = PR_FALSE;
        return NS_ERROR_FAILURE;
    }

    return NS_OK;
}

/*
 * WebGLByteArray
 */

nsresult
NS_NewWebGLByteArray(nsISupports **aResult)
{
    nsIWebGLByteArray *wgba = new WebGLByteArray();
    if (!wgba)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aResult = wgba);
    return NS_OK;
}

WebGLByteArray::WebGLByteArray(PRUint32 length)
    : mOffset(0), mLength(length)
{
    mBuffer = new WebGLArrayBuffer(length);
}

WebGLByteArray::WebGLByteArray(WebGLArrayBuffer *buffer, PRUint32 offset, PRUint32 length)
    : mBuffer(buffer), mOffset(offset), mLength(length)
{
}

WebGLByteArray::WebGLByteArray(JSContext *cx, JSObject *arrayObj, jsuint arrayLen)
    : mOffset(0), mLength(arrayLen)
{
    mBuffer = new WebGLArrayBuffer();
    mBuffer->InitFromJSArray(LOCAL_GL_UNSIGNED_BYTE, 1, cx, arrayObj, arrayLen);
}

NS_IMETHODIMP
WebGLByteArray::Initialize(nsISupports *owner,
                           JSContext *cx,
                           JSObject *obj,
                           PRUint32 argc,
                           jsval *argv)
{
    if (JSVAL_IS_NUMBER(argv[0])) {
        uint32 length;
        ::JS_ValueToECMAUint32(cx, argv[0], &length);
        mBuffer = new WebGLArrayBuffer();
        mBuffer->Prepare(LOCAL_GL_BYTE, 1, length);
        mBuffer->Zero();
        mLength = length;
    } else {
        JSObject *arrayObj;
        jsuint arrayLen;
        jsuint byteOffset = 0;
        jsuint length = 0;

        if (!::JS_ConvertArguments(cx, argc, argv, "o/uu", &arrayObj, &byteOffset, &length) ||
            arrayObj == NULL)
        {
            return NS_ERROR_DOM_SYNTAX_ERR;
        }

        if (::JS_IsArrayObject(cx, arrayObj) &&
            ::JS_GetArrayLength(cx, arrayObj, &arrayLen))
        {
            mBuffer = new WebGLArrayBuffer();
            mBuffer->InitFromJSArray(LOCAL_GL_UNSIGNED_BYTE, 1, cx, arrayObj, arrayLen);
            mLength = arrayLen;
        } else {
            nsCOMPtr<nsIWebGLArrayBuffer> canvasObj;
            nsresult rv;
            rv = nsContentUtils::XPConnect()->WrapJS(cx, arrayObj, NS_GET_IID(nsIWebGLArrayBuffer), getter_AddRefs(canvasObj));
            if (NS_FAILED(rv) || !canvasObj) {
                return NS_ERROR_DOM_SYNTAX_ERR;
            }

            mBuffer = canvasObj->GetNativeArrayBuffer();

            if ((byteOffset + length) > mBuffer->capacity)
                return NS_ERROR_FAILURE;

            if (length > 0)
                mLength = length;
            else
                mLength = (mBuffer->capacity - byteOffset);
        }
    }

    return NS_OK;
}

/* readonly attribute nsIWebGLArrayBuffer buffer; */
NS_IMETHODIMP WebGLByteArray::GetBuffer(nsIWebGLArrayBuffer **aBuffer)
{
    NS_ADDREF(*aBuffer = mBuffer);
    return NS_OK;
}

/* readonly attribute unsigned long byteOffset; */
NS_IMETHODIMP WebGLByteArray::GetByteOffset(PRUint32 *aByteOffset)
{
    *aByteOffset = mOffset;
    return NS_OK;
}

/* readonly attribute unsigned long byteLength; */
NS_IMETHODIMP WebGLByteArray::GetByteLength(PRUint32 *aByteLength)
{
    *aByteLength = mLength;
    return NS_OK;
}

/* attribute unsigned long length; */
NS_IMETHODIMP WebGLByteArray::GetLength(PRUint32 *aLength)
{
    *aLength = mLength;
    return NS_OK;
}

/* unsigned long alignedSizeInBytes (); */
NS_IMETHODIMP WebGLByteArray::AlignedSizeInBytes(PRUint32 *retval)
{
    *retval = mBuffer->capacity;
    return NS_OK;
}

/* nsIWebGLArray slice (in unsigned long offset, in unsigned long length); */
NS_IMETHODIMP WebGLByteArray::Slice(PRUint32 offset, PRUint32 length, nsIWebGLArray **retval)
{
    if (length == 0) 
        return NS_ERROR_FAILURE;

    if (offset + length > mBuffer->capacity)
        return NS_ERROR_FAILURE;

    nsIWebGLArray *wga = new WebGLByteArray(mBuffer, offset, length);
    NS_ADDREF(*retval = wga);
    return NS_OK;
}

/* [IndexGetter] long get (in unsigned long index); */
NS_IMETHODIMP WebGLByteArray::Get(PRUint32 index, PRInt32 *retval)
{
    if (index >= mLength)
        return NS_ERROR_FAILURE;

    char *values = static_cast<char*>(mBuffer->data);
    *retval = values[index];

    return NS_OK;
}

void
WebGLByteArray::Set(PRUint32 index, char value)
{
    if (index >= mLength)
        return;

    char *values = static_cast<char*>(mBuffer->data);
    values[index] = value;
}

/* void set (); */
NS_IMETHODIMP WebGLByteArray::Set()
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    if (js.argc < 1 || js.argc > 2)
        return NS_ERROR_DOM_SYNTAX_ERR;

    if (JSVAL_IS_NUMBER(js.argv[0])) {
        if (js.argc != 2)
            return NS_ERROR_DOM_SYNTAX_ERR;

        uint32 index;
        ::JS_ValueToECMAUint32(js.ctx, js.argv[0], &index);

        int32 value;
        ::JS_ValueToECMAInt32(js.ctx, js.argv[1], &value);

        if (index >= mLength)
            return NS_ERROR_FAILURE;

        Set(index, (char) value);
    } else {
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    return NS_OK;
}

NS_IMETHODIMP_(PRUint32) WebGLByteArray::NativeType()
{
    return mBuffer->type;
}

/* [noscript, notxpcom] voidPtr nativePointer (); */
NS_IMETHODIMP_(void *) WebGLByteArray::NativePointer()
{
    return mBuffer->data;
}

/* [noscript, notxpcom] unsigned long nativeSize (); */
NS_IMETHODIMP_(PRUint32) WebGLByteArray::NativeSize()
{
    return mBuffer->capacity;
}

/* [noscript, notxpcom] unsigned long nativeElementSize (); */
NS_IMETHODIMP_(PRUint32) WebGLByteArray::NativeElementSize()
{
    return mBuffer->ElementSize();
}

/* [noscript, notxpcom] unsigned long nativeCount (); */
NS_IMETHODIMP_(PRUint32) WebGLByteArray::NativeCount()
{
    return mBuffer->length;
}

// nsIXPCScriptable
#define XPC_MAP_CLASSNAME WebGLByteArray
#define XPC_MAP_QUOTED_CLASSNAME "WebGLByteArray"
#define XPC_MAP_WANT_SETPROPERTY
#define XPC_MAP_WANT_GETPROPERTY
#define XPC_MAP_WANT_NEWRESOLVE
#define XPC_MAP_FLAGS nsIXPCScriptable::USE_JSSTUB_FOR_ADDPROPERTY
#include "xpc_map_end.h"

PRBool WebGLByteArray::JSValToIndex(JSContext *cx, jsval id, PRUint32 *retval) {
    PRBool ok = PR_FALSE;
    PRUint32 index;

    if (JSVAL_IS_INT(id)) {
        index = JSVAL_TO_INT(id);
        ok = PR_TRUE;
    } else {
        ok = JS_ValueToECMAUint32(cx, id, &index);
    }

    if (!ok || index >= mLength)
        return PR_FALSE;

    *retval = index;
    return PR_TRUE;
}

NS_IMETHODIMP WebGLByteArray::GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                                          JSObject * obj, jsval id, jsval * vp, PRBool *_retval)
{
    PRUint32 index;
    
    if (!JSValToIndex(cx, id, &index)) {
        *_retval = PR_FALSE;
        return NS_ERROR_INVALID_ARG;
    }

    PRInt32 val;
    Get(index, &val);
    *_retval = JS_NewNumberValue(cx, val, vp);

    return NS_SUCCESS_I_DID_SOMETHING;
}

NS_IMETHODIMP WebGLByteArray::SetProperty(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                                          JSObject * obj, jsval id, jsval * vp, PRBool *_retval)
{
    PRUint32 index;
    PRInt32 val;

    if (!JSValToIndex(cx, id, &index)) {
        *_retval = PR_FALSE;
        return NS_ERROR_INVALID_ARG;
    }

    PRBool ok = PR_FALSE;

    if (JSVAL_IS_INT(*vp)) {
        val = JSVAL_TO_INT(*vp);
        ok = PR_TRUE;
    } else {
        ok = JS_ValueToECMAInt32(cx, *vp, &val);
    }

    if (!ok) {
        *_retval = PR_FALSE;
        return NS_ERROR_INVALID_ARG;
    }

    Set(index, val);
    return NS_SUCCESS_I_DID_SOMETHING;
}

NS_IMETHODIMP WebGLByteArray::NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                                         JSObject * obj, jsval id, PRUint32 flags, JSObject * *objp,
                                         PRBool *_retval)
{
    PRUint32 index;
    PRBool ok = JSValToIndex(cx, id, &index);

    if (ok) {
        *_retval = PR_TRUE;
        *objp = obj;
    } else {
        *_retval = PR_FALSE;
        return NS_ERROR_FAILURE;
    }

    return NS_OK;
}

/*
 * WebGLUnsignedByteArray
 */

nsresult
NS_NewWebGLUnsignedByteArray(nsISupports **aResult)
{
    nsIWebGLUnsignedByteArray *wguba = new WebGLUnsignedByteArray();
    if (!wguba)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aResult = wguba);
    return NS_OK;
}

WebGLUnsignedByteArray::WebGLUnsignedByteArray(PRUint32 length)
    : mOffset(0), mLength(length)
{
    mBuffer = new WebGLArrayBuffer(length);
}

WebGLUnsignedByteArray::WebGLUnsignedByteArray(WebGLArrayBuffer *buffer, PRUint32 offset, PRUint32 length)
    : mBuffer(buffer), mOffset(offset), mLength(length)
{
}

WebGLUnsignedByteArray::WebGLUnsignedByteArray(JSContext *cx, JSObject *arrayObj, jsuint arrayLen)
    : mOffset(0), mLength(arrayLen)
{
    mBuffer = new WebGLArrayBuffer();
    mBuffer->InitFromJSArray(LOCAL_GL_UNSIGNED_BYTE, 1, cx, arrayObj, arrayLen);
}

NS_IMETHODIMP
WebGLUnsignedByteArray::Initialize(nsISupports *owner,
                                   JSContext *cx,
                                   JSObject *obj,
                                   PRUint32 argc,
                                   jsval *argv)
{
    if (JSVAL_IS_NUMBER(argv[0])) {
        uint32 length;
        ::JS_ValueToECMAUint32(cx, argv[0], &length);
        mBuffer = new WebGLArrayBuffer();
        mBuffer->Prepare(LOCAL_GL_UNSIGNED_BYTE, 1, length);
        mBuffer->Zero();
        mLength = length;
    } else {
        JSObject *arrayObj;
        jsuint arrayLen;
        jsuint byteOffset = 0;
        jsuint length = 0;

        if (!::JS_ConvertArguments(cx, argc, argv, "o/uu", &arrayObj, &byteOffset, &length) ||
            arrayObj == NULL)
        {
            return NS_ERROR_DOM_SYNTAX_ERR;
        }

        if (::JS_IsArrayObject(cx, arrayObj) &&
            ::JS_GetArrayLength(cx, arrayObj, &arrayLen))
        {
            mBuffer = new WebGLArrayBuffer();
            mBuffer->InitFromJSArray(LOCAL_GL_UNSIGNED_BYTE, 1, cx, arrayObj, arrayLen);
            mLength = arrayLen;
        } else {
            nsCOMPtr<nsIWebGLArrayBuffer> canvasObj;
            nsresult rv;
            rv = nsContentUtils::XPConnect()->WrapJS(cx, arrayObj, NS_GET_IID(nsIWebGLArrayBuffer), getter_AddRefs(canvasObj));
            if (NS_FAILED(rv) || !canvasObj) {
                return NS_ERROR_DOM_SYNTAX_ERR;
            }

            mBuffer = canvasObj->GetNativeArrayBuffer();

            if ((byteOffset + length) > mBuffer->capacity)
                return NS_ERROR_FAILURE;

            if (length > 0)
                mLength = length;
            else
                mLength = (mBuffer->capacity - byteOffset);
        }
    }

    return NS_OK;
}

/* readonly attribute nsIWebGLArrayBuffer buffer; */
NS_IMETHODIMP WebGLUnsignedByteArray::GetBuffer(nsIWebGLArrayBuffer **aBuffer)
{
    NS_ADDREF(*aBuffer = mBuffer);
    return NS_OK;
}

/* readonly attribute unsigned long byteOffset; */
NS_IMETHODIMP WebGLUnsignedByteArray::GetByteOffset(PRUint32 *aByteOffset)
{
    *aByteOffset = mOffset;
    return NS_OK;
}

/* readonly attribute unsigned long byteLength; */
NS_IMETHODIMP WebGLUnsignedByteArray::GetByteLength(PRUint32 *aByteLength)
{
    *aByteLength = mLength;
    return NS_OK;
}

/* attribute unsigned long length; */
NS_IMETHODIMP WebGLUnsignedByteArray::GetLength(PRUint32 *aLength)
{
    *aLength = mLength;
    return NS_OK;
}

/* unsigned long alignedSizeInBytes (); */
NS_IMETHODIMP WebGLUnsignedByteArray::AlignedSizeInBytes(PRUint32 *retval)
{
    *retval = mBuffer->capacity;
    return NS_OK;
}

/* nsIWebGLArray slice (in unsigned long offset, in unsigned long length); */
NS_IMETHODIMP WebGLUnsignedByteArray::Slice(PRUint32 offset, PRUint32 length, nsIWebGLArray **retval)
{
    if (length == 0) 
        return NS_ERROR_FAILURE;

    if (offset + length > mBuffer->capacity)
        return NS_ERROR_FAILURE;

    nsIWebGLArray *wga = new WebGLUnsignedByteArray(mBuffer, offset, length);
    NS_ADDREF(*retval = wga);
    return NS_OK;
}

/* [IndexGetter] unsigned long get (in unsigned long index); */
NS_IMETHODIMP WebGLUnsignedByteArray::Get(PRUint32 index, PRUint32 *retval)
{
    if (index >= mLength)
        return NS_ERROR_FAILURE;

    unsigned char *values = static_cast<unsigned char*>(mBuffer->data);
    *retval = values[index];

    return NS_OK;
}

void
WebGLUnsignedByteArray::Set(PRUint32 index, unsigned char value)
{
    if (index >= mLength)
        return;

    unsigned char *values = static_cast<unsigned char*>(mBuffer->data);
    values[index] = value;
}

/* void set (); */
NS_IMETHODIMP WebGLUnsignedByteArray::Set()
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    if (js.argc < 1 || js.argc > 2)
        return NS_ERROR_DOM_SYNTAX_ERR;

    if (JSVAL_IS_NUMBER(js.argv[0])) {
        if (js.argc != 2)
            return NS_ERROR_DOM_SYNTAX_ERR;

        uint32 index;
        ::JS_ValueToECMAUint32(js.ctx, js.argv[0], &index);

        uint32 value;
        ::JS_ValueToECMAUint32(js.ctx, js.argv[1], &value);

        if (index >= mLength)
            return NS_ERROR_FAILURE;

        Set(index, (unsigned char) value);
    } else {
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    return NS_OK;
}

NS_IMETHODIMP_(PRUint32) WebGLUnsignedByteArray::NativeType()
{
    return mBuffer->type;
}

/* [noscript, notxpcom] voidPtr nativePointer (); */
NS_IMETHODIMP_(void *) WebGLUnsignedByteArray::NativePointer()
{
    return mBuffer->data;
}

/* [noscript, notxpcom] unsigned long nativeSize (); */
NS_IMETHODIMP_(PRUint32) WebGLUnsignedByteArray::NativeSize()
{
    return mBuffer->capacity;
}

/* [noscript, notxpcom] unsigned long nativeElementSize (); */
NS_IMETHODIMP_(PRUint32) WebGLUnsignedByteArray::NativeElementSize()
{
    return mBuffer->ElementSize();
}

/* [noscript, notxpcom] unsigned long nativeCount (); */
NS_IMETHODIMP_(PRUint32) WebGLUnsignedByteArray::NativeCount()
{
    return mBuffer->length;
}

// nsIXPCScriptable
#define XPC_MAP_CLASSNAME WebGLUnsignedByteArray
#define XPC_MAP_QUOTED_CLASSNAME "WebGLUnsignedByteArray"
#define XPC_MAP_WANT_SETPROPERTY
#define XPC_MAP_WANT_GETPROPERTY
#define XPC_MAP_WANT_NEWRESOLVE
#define XPC_MAP_FLAGS nsIXPCScriptable::USE_JSSTUB_FOR_ADDPROPERTY
#include "xpc_map_end.h"

PRBool WebGLUnsignedByteArray::JSValToIndex(JSContext *cx, jsval id, PRUint32 *retval) {
    PRBool ok = PR_FALSE;
    PRUint32 index;

    if (JSVAL_IS_INT(id)) {
        index = JSVAL_TO_INT(id);
        ok = PR_TRUE;
    } else {
        ok = JS_ValueToECMAUint32(cx, id, &index);
    }

    if (!ok || index >= mLength)
        return PR_FALSE;

    *retval = index;
    return PR_TRUE;
}

NS_IMETHODIMP WebGLUnsignedByteArray::GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                                                  JSObject * obj, jsval id, jsval * vp, PRBool *_retval)
{
    PRUint32 index;
    
    if (!JSValToIndex(cx, id, &index)) {
        *_retval = PR_FALSE;
        return NS_ERROR_INVALID_ARG;
    }

    PRUint32 val;
    Get(index, &val);
    *_retval = JS_NewNumberValue(cx, val, vp);

    return NS_OK;
}

NS_IMETHODIMP WebGLUnsignedByteArray::SetProperty(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                                                  JSObject * obj, jsval id, jsval * vp, PRBool *_retval)
{
    PRUint32 index;
    PRUint32 val;

    if (!JSValToIndex(cx, id, &index)) {
        *_retval = PR_FALSE;
        return NS_ERROR_INVALID_ARG;
    }

    PRBool ok = PR_FALSE;

    if (JSVAL_IS_INT(*vp)) {
        val = JSVAL_TO_INT(*vp);
        ok = PR_TRUE;
    } else {
        ok = JS_ValueToECMAUint32(cx, *vp, &val);
    }

    if (!ok) {
        *_retval = PR_FALSE;
        return NS_ERROR_INVALID_ARG;
    }

    Set(index, val);
    return NS_OK;
}

NS_IMETHODIMP WebGLUnsignedByteArray::NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                                                 JSObject * obj, jsval id, PRUint32 flags, JSObject * *objp,
                                                 PRBool *_retval)
{
    PRUint32 index;
    PRBool ok = JSValToIndex(cx, id, &index);

    if (ok) {
        *_retval = PR_TRUE;
        *objp = obj;
    } else {
        *_retval = PR_FALSE;
        return NS_ERROR_FAILURE;
    }

    return NS_OK;
}

/*
 * WebGLShortArray
 */

nsresult
NS_NewWebGLShortArray(nsISupports **aResult)
{
    nsIWebGLShortArray *wgsa = new WebGLShortArray();
    if (!wgsa)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aResult = wgsa);
    return NS_OK;
}

WebGLShortArray::WebGLShortArray(PRUint32 length)
    : mOffset(0), mLength(length)
{
    mBuffer = new WebGLArrayBuffer(length * sizeof(short));
}

WebGLShortArray::WebGLShortArray(WebGLArrayBuffer *buffer, PRUint32 offset, PRUint32 length)
    : mBuffer(buffer), mOffset(offset), mLength(length)
{
}

WebGLShortArray::WebGLShortArray(JSContext *cx, JSObject *arrayObj, jsuint arrayLen)
    : mOffset(0), mLength(arrayLen)
{
    mBuffer = new WebGLArrayBuffer();
    mBuffer->InitFromJSArray(LOCAL_GL_SHORT, 1, cx, arrayObj, arrayLen);
}

NS_IMETHODIMP
WebGLShortArray::Initialize(nsISupports *owner,
                            JSContext *cx,
                            JSObject *obj,
                            PRUint32 argc,
                            jsval *argv)
{
    if (JSVAL_IS_NUMBER(argv[0])) {
        uint32 length;
        ::JS_ValueToECMAUint32(cx, argv[0], &length);
        mBuffer = new WebGLArrayBuffer();
        mBuffer->Prepare(LOCAL_GL_SHORT, 1, length);
        mBuffer->Zero();
        mLength = length;
    } else {
        JSObject *arrayObj;
        jsuint arrayLen;
        jsuint byteOffset = 0;
        jsuint length = 0;

        if (!::JS_ConvertArguments(cx, argc, argv, "o/uu", &arrayObj, &byteOffset, &length) ||
            arrayObj == NULL)
        {
            return NS_ERROR_DOM_SYNTAX_ERR;
        }

        if (::JS_IsArrayObject(cx, arrayObj) &&
            ::JS_GetArrayLength(cx, arrayObj, &arrayLen))
        {
            mBuffer = new WebGLArrayBuffer();
            mBuffer->InitFromJSArray(LOCAL_GL_SHORT, 1, cx, arrayObj, arrayLen);
            mLength = arrayLen;
        } else {
            nsCOMPtr<nsIWebGLArrayBuffer> canvasObj;
            nsresult rv;
            rv = nsContentUtils::XPConnect()->WrapJS(cx, arrayObj, NS_GET_IID(nsIWebGLArrayBuffer), getter_AddRefs(canvasObj));
            if (NS_FAILED(rv) || !canvasObj) {
                return NS_ERROR_DOM_SYNTAX_ERR;
            }

            mBuffer = canvasObj->GetNativeArrayBuffer();

            if (byteOffset % sizeof(short))
                return NS_ERROR_FAILURE;

            if ((byteOffset + (length * sizeof(short))) > mBuffer->capacity)
                return NS_ERROR_FAILURE;

            if (length > 0) {
                mLength = length;
            } else {
                if ((mBuffer->capacity - byteOffset) % sizeof(short))
                    return NS_ERROR_FAILURE;

                mLength = (mBuffer->capacity - byteOffset) / sizeof(short);
            }
        }
    }

    return NS_OK;
}

/* readonly attribute nsIWebGLArrayBuffer buffer; */
NS_IMETHODIMP WebGLShortArray::GetBuffer(nsIWebGLArrayBuffer * *aBuffer)
{
    NS_ADDREF(*aBuffer = mBuffer);
    return NS_OK;
}

/* readonly attribute unsigned long byteOffset; */
NS_IMETHODIMP WebGLShortArray::GetByteOffset(PRUint32 *aByteOffset)
{
    *aByteOffset = mOffset;
    return NS_OK;
}

/* readonly attribute unsigned long byteLength; */
NS_IMETHODIMP WebGLShortArray::GetByteLength(PRUint32 *aByteLength)
{
    *aByteLength = mLength * sizeof(short);
    return NS_OK;
}

/* attribute unsigned long length; */
NS_IMETHODIMP WebGLShortArray::GetLength(PRUint32 *aLength)
{
    *aLength = mLength;
    return NS_OK;
}

/* unsigned long alignedSizeInBytes (); */
NS_IMETHODIMP WebGLShortArray::AlignedSizeInBytes(PRUint32 *retval)
{
    *retval = mBuffer->capacity;
    return NS_OK;
}

/* nsIWebGLArray slice (in unsigned long offset, in unsigned long length); */
NS_IMETHODIMP WebGLShortArray::Slice(PRUint32 offset, PRUint32 length, nsIWebGLArray **retval)
{
    if (length == 0) 
        return NS_ERROR_FAILURE;

    if (offset + length > mBuffer->capacity)
        return NS_ERROR_FAILURE;

    nsIWebGLArray *wga = new WebGLShortArray(mBuffer, offset, length);
    NS_ADDREF(*retval = wga);
    return NS_OK;
}

/* [IndexGetter] long get (in unsigned long index); */
NS_IMETHODIMP WebGLShortArray::Get(PRUint32 index, PRInt32 *retval)
{
    if (index >= mLength)
        return NS_ERROR_FAILURE;

    short *values = static_cast<short*>(mBuffer->data);
    *retval = values[index];

    return NS_OK;
}

void
WebGLShortArray::Set(PRUint32 index, short value)
{
    if (index >= mLength)
        return;

    short *values = static_cast<short*>(mBuffer->data);
    values[index] = value;
}

/* void set (); */
NS_IMETHODIMP WebGLShortArray::Set()
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    if (js.argc < 1 || js.argc > 2)
        return NS_ERROR_DOM_SYNTAX_ERR;

    if (JSVAL_IS_NUMBER(js.argv[0])) {
        if (js.argc != 2)
            return NS_ERROR_DOM_SYNTAX_ERR;

        uint32 index;
        ::JS_ValueToECMAUint32(js.ctx, js.argv[0], &index);

        int32 value;
        ::JS_ValueToECMAInt32(js.ctx, js.argv[1], &value);

        if (index >= mLength)
            return NS_ERROR_FAILURE;

        Set(index, (short) value);
    } else {
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    return NS_OK;
}

NS_IMETHODIMP_(PRUint32) WebGLShortArray::NativeType()
{
    return mBuffer->type;
}

/* [noscript, notxpcom] voidPtr nativePointer (); */
NS_IMETHODIMP_(void *) WebGLShortArray::NativePointer()
{
    return mBuffer->data;
}

/* [noscript, notxpcom] unsigned long nativeSize (); */
NS_IMETHODIMP_(PRUint32) WebGLShortArray::NativeSize()
{
    return mBuffer->capacity;
}

/* [noscript, notxpcom] unsigned long nativeElementSize (); */
NS_IMETHODIMP_(PRUint32) WebGLShortArray::NativeElementSize()
{
    return mBuffer->ElementSize();
}

/* [noscript, notxpcom] unsigned long nativeCount (); */
NS_IMETHODIMP_(PRUint32) WebGLShortArray::NativeCount()
{
    return mBuffer->length;
}

// nsIXPCScriptable
#define XPC_MAP_CLASSNAME WebGLShortArray
#define XPC_MAP_QUOTED_CLASSNAME "WebGLShortArray"
#define XPC_MAP_WANT_SETPROPERTY
#define XPC_MAP_WANT_GETPROPERTY
#define XPC_MAP_WANT_NEWRESOLVE
#define XPC_MAP_FLAGS nsIXPCScriptable::USE_JSSTUB_FOR_ADDPROPERTY
#include "xpc_map_end.h"

PRBool WebGLShortArray::JSValToIndex(JSContext *cx, jsval id, PRUint32 *retval) {
    PRBool ok = PR_FALSE;
    PRUint32 index;

    if (JSVAL_IS_INT(id)) {
        index = JSVAL_TO_INT(id);
        ok = PR_TRUE;
    } else {
        ok = JS_ValueToECMAUint32(cx, id, &index);
    }

    if (!ok || index >= mLength)
        return PR_FALSE;

    *retval = index;
    return PR_TRUE;
}

NS_IMETHODIMP WebGLShortArray::GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                                           JSObject * obj, jsval id, jsval * vp, PRBool *_retval)
{
    PRUint32 index;
    
    if (!JSValToIndex(cx, id, &index)) {
        *_retval = PR_FALSE;
        return NS_ERROR_INVALID_ARG;
    }

    PRInt32 val;
    Get(index, &val);
    *_retval = JS_NewNumberValue(cx, val, vp);

    return NS_OK;
}

NS_IMETHODIMP WebGLShortArray::SetProperty(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                                           JSObject * obj, jsval id, jsval * vp, PRBool *_retval)
{
    PRUint32 index;
    PRInt32 val;

    if (!JSValToIndex(cx, id, &index)) {
        *_retval = PR_FALSE;
        return NS_ERROR_INVALID_ARG;
    }

    PRBool ok = PR_FALSE;

    if (JSVAL_IS_INT(*vp)) {
        val = JSVAL_TO_INT(*vp);
        ok = PR_TRUE;
    } else {
        ok = JS_ValueToECMAInt32(cx, *vp, &val);
    }

    if (!ok) {
        *_retval = PR_FALSE;
        return NS_ERROR_INVALID_ARG;
    }

    Set(index, val);
    return NS_OK;
}

NS_IMETHODIMP WebGLShortArray::NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                                          JSObject * obj, jsval id, PRUint32 flags, JSObject * *objp,
                                          PRBool *_retval)
{
    PRUint32 index;
    PRBool ok = JSValToIndex(cx, id, &index);

    if (ok) {
        *_retval = PR_TRUE;
        *objp = obj;
    } else {
        *_retval = PR_FALSE;
        return NS_ERROR_FAILURE;
    }

    return NS_OK;
}

/*
 * WebGLUnsignedShortArray
 */

nsresult
NS_NewWebGLUnsignedShortArray(nsISupports **aResult)
{
    nsIWebGLUnsignedShortArray *wgusa = new WebGLUnsignedShortArray();
    if (!wgusa)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aResult = wgusa);
    return NS_OK;
}

WebGLUnsignedShortArray::WebGLUnsignedShortArray(PRUint32 length)
    : mOffset(0), mLength(length)
{
    mBuffer = new WebGLArrayBuffer(length * sizeof(short));
}

WebGLUnsignedShortArray::WebGLUnsignedShortArray(WebGLArrayBuffer *buffer, PRUint32 offset, PRUint32 length)
    : mBuffer(buffer), mOffset(offset), mLength(length)
{
}

WebGLUnsignedShortArray::WebGLUnsignedShortArray(JSContext *cx, JSObject *arrayObj, jsuint arrayLen)
    : mOffset(0), mLength(arrayLen)
{
    mBuffer = new WebGLArrayBuffer();
    mBuffer->InitFromJSArray(LOCAL_GL_UNSIGNED_SHORT, 1, cx, arrayObj, arrayLen);
}

NS_IMETHODIMP
WebGLUnsignedShortArray::Initialize(nsISupports *owner,
                                   JSContext *cx,
                                   JSObject *obj,
                                   PRUint32 argc,
                                   jsval *argv)
{
    if (JSVAL_IS_NUMBER(argv[0])) {
        uint32 length;
        ::JS_ValueToECMAUint32(cx, argv[0], &length);
        mBuffer = new WebGLArrayBuffer();
        mBuffer->Prepare(LOCAL_GL_UNSIGNED_SHORT, 1, length);
        mBuffer->Zero();
        mLength = length;
    } else {
        JSObject *arrayObj;
        jsuint arrayLen;
        jsuint byteOffset = 0;
        jsuint length = 0;

        if (!::JS_ConvertArguments(cx, argc, argv, "o/uu", &arrayObj, &byteOffset, &length) ||
            arrayObj == NULL)
        {
            return NS_ERROR_DOM_SYNTAX_ERR;
        }

        if (::JS_IsArrayObject(cx, arrayObj) &&
            ::JS_GetArrayLength(cx, arrayObj, &arrayLen))
        {
            mBuffer = new WebGLArrayBuffer();
            mBuffer->InitFromJSArray(LOCAL_GL_UNSIGNED_SHORT, 1, cx, arrayObj, arrayLen);
            mLength = arrayLen;
        } else {
            nsCOMPtr<nsIWebGLArrayBuffer> canvasObj;
            nsresult rv;
            rv = nsContentUtils::XPConnect()->WrapJS(cx, arrayObj, NS_GET_IID(nsIWebGLArrayBuffer), getter_AddRefs(canvasObj));
            if (NS_FAILED(rv) || !canvasObj) {
                return NS_ERROR_DOM_SYNTAX_ERR;
            }

            mBuffer = canvasObj->GetNativeArrayBuffer();

            if (byteOffset % sizeof(short))
                return NS_ERROR_FAILURE;

            if ((byteOffset + (length * sizeof(short))) > mBuffer->capacity)
                return NS_ERROR_FAILURE;

            if (length > 0) {
                mLength = length;
            } else {
                if ((mBuffer->capacity - byteOffset) % sizeof(short)) 
                    return NS_ERROR_FAILURE;

                mLength = (mBuffer->capacity - byteOffset) / sizeof(short);
            }
        }
    }

    return NS_OK;
}

/* readonly attribute nsIWebGLArrayBuffer buffer; */
NS_IMETHODIMP WebGLUnsignedShortArray::GetBuffer(nsIWebGLArrayBuffer * *aBuffer)
{
    NS_ADDREF(*aBuffer = mBuffer);
    return NS_OK;
}

/* readonly attribute unsigned long byteOffset; */
NS_IMETHODIMP WebGLUnsignedShortArray::GetByteOffset(PRUint32 *aByteOffset)
{
    *aByteOffset = mOffset;
    return NS_OK;
}

/* readonly attribute unsigned long byteLength; */
NS_IMETHODIMP WebGLUnsignedShortArray::GetByteLength(PRUint32 *aByteLength)
{
    *aByteLength = mLength * sizeof(short);
    return NS_OK;
}

/* attribute unsigned long length; */
NS_IMETHODIMP WebGLUnsignedShortArray::GetLength(PRUint32 *aLength)
{
    *aLength = mLength;
    return NS_OK;
}

/* unsigned long alignedSizeInBytes (); */
NS_IMETHODIMP WebGLUnsignedShortArray::AlignedSizeInBytes(PRUint32 *retval)
{
    *retval = mBuffer->capacity;
    return NS_OK;
}

/* nsIWebGLArray slice (in unsigned long offset, in unsigned long length); */
NS_IMETHODIMP WebGLUnsignedShortArray::Slice(PRUint32 offset, PRUint32 length, nsIWebGLArray **retval)
{
    if (length == 0) 
        return NS_ERROR_FAILURE;

    if (offset + length > mBuffer->capacity)
        return NS_ERROR_FAILURE;

    nsIWebGLArray *wga = new WebGLUnsignedShortArray(mBuffer, offset, length);
    NS_ADDREF(*retval = wga);
    return NS_OK;
}

/* [IndexGetter] unsigned long get (in unsigned long index); */
NS_IMETHODIMP WebGLUnsignedShortArray::Get(PRUint32 index, PRUint32 *retval)
{
    if (index >= mLength)
        return NS_ERROR_FAILURE;

    unsigned short *values = static_cast<unsigned short*>(mBuffer->data);
    *retval = values[index];

    return NS_OK;
}

void
WebGLUnsignedShortArray::Set(PRUint32 index, unsigned short value)
{
    if (index >= mLength)
        return;

    unsigned short *values = static_cast<unsigned short*>(mBuffer->data);
    values[index] = value;
}

/* void set (); */
NS_IMETHODIMP WebGLUnsignedShortArray::Set()
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    if (js.argc < 1 || js.argc > 2)
        return NS_ERROR_DOM_SYNTAX_ERR;

    if (JSVAL_IS_NUMBER(js.argv[0])) {
        if (js.argc != 2)
            return NS_ERROR_DOM_SYNTAX_ERR;

        uint32 index;
        ::JS_ValueToECMAUint32(js.ctx, js.argv[0], &index);

        uint32 value;
        ::JS_ValueToECMAUint32(js.ctx, js.argv[1], &value);

        if (index >= mLength)
            return NS_ERROR_FAILURE;

        Set(index, (unsigned short) value);
    } else {
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    return NS_OK;
}

NS_IMETHODIMP_(PRUint32) WebGLUnsignedShortArray::NativeType()
{
    return mBuffer->type;
}

/* [noscript, notxpcom] voidPtr nativePointer (); */
NS_IMETHODIMP_(void *) WebGLUnsignedShortArray::NativePointer()
{
    return mBuffer->data;
}

/* [noscript, notxpcom] unsigned long nativeSize (); */
NS_IMETHODIMP_(PRUint32) WebGLUnsignedShortArray::NativeSize()
{
    return mBuffer->capacity;
}

/* [noscript, notxpcom] unsigned long nativeElementSize (); */
NS_IMETHODIMP_(PRUint32) WebGLUnsignedShortArray::NativeElementSize()
{
    return mBuffer->ElementSize();
}

/* [noscript, notxpcom] unsigned long nativeCount (); */
NS_IMETHODIMP_(PRUint32) WebGLUnsignedShortArray::NativeCount()
{
    return mBuffer->length;
}

// nsIXPCScriptable
#define XPC_MAP_CLASSNAME WebGLUnsignedShortArray
#define XPC_MAP_QUOTED_CLASSNAME "WebGLUnsignedShortArray"
#define XPC_MAP_WANT_SETPROPERTY
#define XPC_MAP_WANT_GETPROPERTY
#define XPC_MAP_WANT_NEWRESOLVE
#define XPC_MAP_FLAGS nsIXPCScriptable::USE_JSSTUB_FOR_ADDPROPERTY
#include "xpc_map_end.h"

PRBool WebGLUnsignedShortArray::JSValToIndex(JSContext *cx, jsval id, PRUint32 *retval) {
    PRBool ok = PR_FALSE;
    PRUint32 index;

    if (JSVAL_IS_INT(id)) {
        index = JSVAL_TO_INT(id);
        ok = PR_TRUE;
    } else {
        ok = JS_ValueToECMAUint32(cx, id, &index);
    }

    if (!ok || index >= mLength)
        return PR_FALSE;

    *retval = index;
    return PR_TRUE;
}

NS_IMETHODIMP WebGLUnsignedShortArray::GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                                                   JSObject * obj, jsval id, jsval * vp, PRBool *_retval)
{
    PRUint32 index;
    
    if (!JSValToIndex(cx, id, &index)) {
        *_retval = PR_FALSE;
        return NS_ERROR_INVALID_ARG;
    }

    PRUint32 val;
    Get(index, &val);
    *_retval = JS_NewNumberValue(cx, val, vp);

    return NS_OK;
}

NS_IMETHODIMP WebGLUnsignedShortArray::SetProperty(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                                                   JSObject * obj, jsval id, jsval * vp, PRBool *_retval)
{
    PRUint32 index;
    PRUint32 val;

    if (!JSValToIndex(cx, id, &index)) {
        *_retval = PR_FALSE;
        return NS_ERROR_INVALID_ARG;
    }

    PRBool ok = PR_FALSE;

    if (JSVAL_IS_INT(*vp)) {
        val = JSVAL_TO_INT(*vp);
        ok = PR_TRUE;
    } else {
        ok = JS_ValueToECMAUint32(cx, *vp, &val);
    }

    if (!ok) {
        *_retval = PR_FALSE;
        return NS_ERROR_INVALID_ARG;
    }

    Set(index, val);
    return NS_OK;
}

NS_IMETHODIMP WebGLUnsignedShortArray::NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                                                  JSObject * obj, jsval id, PRUint32 flags, JSObject * *objp,
                                                  PRBool *_retval)
{
    PRUint32 index;
    PRBool ok = JSValToIndex(cx, id, &index);

    if (ok) {
        *_retval = PR_TRUE;
        *objp = obj;
    } else {
        *_retval = PR_FALSE;
        return NS_ERROR_FAILURE;
    }

    return NS_OK;
}

/*
 * WebGLIntArray
 */

nsresult
NS_NewWebGLIntArray(nsISupports **aResult)
{
    nsIWebGLIntArray *wgia = new WebGLIntArray();
    if (!wgia)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aResult = wgia);
    return NS_OK;
}

WebGLIntArray::WebGLIntArray(PRUint32 length)
    : mOffset(0), mLength(length)
{
    mBuffer = new WebGLArrayBuffer(length * sizeof(int));
}

WebGLIntArray::WebGLIntArray(WebGLArrayBuffer *buffer, PRUint32 offset, PRUint32 length)
    : mBuffer(buffer), mOffset(offset), mLength(length)
{
}

WebGLIntArray::WebGLIntArray(JSContext *cx, JSObject *arrayObj, jsuint arrayLen)
    : mOffset(0), mLength(arrayLen)
{
    mBuffer = new WebGLArrayBuffer();
    mBuffer->InitFromJSArray(LOCAL_GL_INT, 1, cx, arrayObj, arrayLen);
}

NS_IMETHODIMP
WebGLIntArray::Initialize(nsISupports *owner,
                          JSContext *cx,
                          JSObject *obj,
                          PRUint32 argc,
                          jsval *argv)
{
    if (JSVAL_IS_NUMBER(argv[0])) {
        uint32 length;
        ::JS_ValueToECMAUint32(cx, argv[0], &length);
        mBuffer = new WebGLArrayBuffer();
        mBuffer->Prepare(LOCAL_GL_INT, 1, length);
        mBuffer->Zero();
        mLength = length;
    } else {
        JSObject *arrayObj;
        jsuint arrayLen;
        jsuint byteOffset = 0;
        jsuint length = 0;

        if (!::JS_ConvertArguments(cx, argc, argv, "o/uu", &arrayObj, &byteOffset, &length) ||
            arrayObj == NULL)
        {
            return NS_ERROR_DOM_SYNTAX_ERR;
        }

        if (::JS_IsArrayObject(cx, arrayObj) &&
            ::JS_GetArrayLength(cx, arrayObj, &arrayLen))
        {
            mBuffer = new WebGLArrayBuffer();
            mBuffer->InitFromJSArray(LOCAL_GL_INT, 1, cx, arrayObj, arrayLen);
            mLength = arrayLen;
        } else {
            nsCOMPtr<nsIWebGLArrayBuffer> canvasObj;
            nsresult rv;
            rv = nsContentUtils::XPConnect()->WrapJS(cx, arrayObj, NS_GET_IID(nsIWebGLArrayBuffer), getter_AddRefs(canvasObj));
            if (NS_FAILED(rv) || !canvasObj) {
                return NS_ERROR_DOM_SYNTAX_ERR;
            }

            mBuffer = canvasObj->GetNativeArrayBuffer();

            if (byteOffset % sizeof(int))
                return NS_ERROR_FAILURE;

            if ((byteOffset + (length * sizeof(int))) > mBuffer->capacity)
                return NS_ERROR_FAILURE;

            if (length > 0) {
                mLength = length;
            } else {
                if ((mBuffer->capacity - byteOffset) % sizeof(int))
                    return NS_ERROR_FAILURE;

                mLength = (mBuffer->capacity - byteOffset) / sizeof(int);
            }
        }
    }

    return NS_OK;
}

/* readonly attribute nsIWebGLArrayBuffer buffer; */
NS_IMETHODIMP WebGLIntArray::GetBuffer(nsIWebGLArrayBuffer * *aBuffer)
{
    NS_ADDREF(*aBuffer = mBuffer);
    return NS_OK;
}

/* readonly attribute unsigned long byteOffset; */
NS_IMETHODIMP WebGLIntArray::GetByteOffset(PRUint32 *aByteOffset)
{
    *aByteOffset = mOffset;
    return NS_OK;
}

/* readonly attribute unsigned long byteLength; */
NS_IMETHODIMP WebGLIntArray::GetByteLength(PRUint32 *aByteLength)
{
    *aByteLength = mLength * sizeof(int);
    return NS_OK;
}

/* attribute unsigned long length; */
NS_IMETHODIMP WebGLIntArray::GetLength(PRUint32 *aLength)
{
    *aLength = mLength;
    return NS_OK;
}

/* unsigned long alignedSizeInBytes (); */
NS_IMETHODIMP WebGLIntArray::AlignedSizeInBytes(PRUint32 *retval)
{
    *retval = mBuffer->capacity;
    return NS_OK;
}

/* nsIWebGLArray slice (in unsigned long offset, in unsigned long length); */
NS_IMETHODIMP WebGLIntArray::Slice(PRUint32 offset, PRUint32 length, nsIWebGLArray **retval)
{
    if (length == 0) 
        return NS_ERROR_FAILURE;

    if (offset + length > mBuffer->capacity)
        return NS_ERROR_FAILURE;

    nsIWebGLArray *wga = new WebGLIntArray(mBuffer, offset, length);
    NS_ADDREF(*retval = wga);
    return NS_OK;
}

/* [IndexGetter] long get (in unsigned long index); */
NS_IMETHODIMP WebGLIntArray::Get(PRUint32 index, PRInt32 *retval)
{
    if (index >= mLength)
        return NS_ERROR_FAILURE;

    int *values = static_cast<int*>(mBuffer->data);
    *retval = values[index];

    return NS_OK;
}

void
WebGLIntArray::Set(PRUint32 index, int value)
{
    if (index >= mLength)
        return;

    int *values = static_cast<int*>(mBuffer->data);
    values[index] = value;
}

/* void set (); */
NS_IMETHODIMP WebGLIntArray::Set()
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    if (js.argc < 1 || js.argc > 2)
        return NS_ERROR_DOM_SYNTAX_ERR;

    if (JSVAL_IS_NUMBER(js.argv[0])) {
        if (js.argc != 2)
            return NS_ERROR_DOM_SYNTAX_ERR;

        uint32 index;
        ::JS_ValueToECMAUint32(js.ctx, js.argv[0], &index);

        int32 value;
        ::JS_ValueToECMAInt32(js.ctx, js.argv[1], &value);

        if (index >= mLength)
            return NS_ERROR_FAILURE;

        Set(index, value);
    } else {
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    return NS_OK;
}

NS_IMETHODIMP_(PRUint32) WebGLIntArray::NativeType()
{
    return mBuffer->type;
}

/* [noscript, notxpcom] voidPtr nativePointer (); */
NS_IMETHODIMP_(void *) WebGLIntArray::NativePointer()
{
    return mBuffer->data;
}

/* [noscript, notxpcom] unsigned long nativeSize (); */
NS_IMETHODIMP_(PRUint32) WebGLIntArray::NativeSize()
{
    return mBuffer->capacity;
}

/* [noscript, notxpcom] unsigned long nativeElementSize (); */
NS_IMETHODIMP_(PRUint32) WebGLIntArray::NativeElementSize()
{
    return mBuffer->ElementSize();
}

/* [noscript, notxpcom] unsigned long nativeCount (); */
NS_IMETHODIMP_(PRUint32) WebGLIntArray::NativeCount()
{
    return mBuffer->length;
}

// nsIXPCScriptable
#define XPC_MAP_CLASSNAME WebGLIntArray
#define XPC_MAP_QUOTED_CLASSNAME "WebGLIntArray"
#define XPC_MAP_WANT_SETPROPERTY
#define XPC_MAP_WANT_GETPROPERTY
#define XPC_MAP_WANT_NEWRESOLVE
#define XPC_MAP_FLAGS nsIXPCScriptable::USE_JSSTUB_FOR_ADDPROPERTY
#include "xpc_map_end.h"

PRBool WebGLIntArray::JSValToIndex(JSContext *cx, jsval id, PRUint32 *retval) {
    PRBool ok = PR_FALSE;
    PRUint32 index;

    if (JSVAL_IS_INT(id)) {
        index = JSVAL_TO_INT(id);
        ok = PR_TRUE;
    } else {
        ok = JS_ValueToECMAUint32(cx, id, &index);
    }

    if (!ok || index >= mLength)
        return PR_FALSE;

    *retval = index;
    return PR_TRUE;
}

NS_IMETHODIMP WebGLIntArray::GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                                         JSObject * obj, jsval id, jsval * vp, PRBool *_retval)
{
    PRUint32 index;
    
    if (!JSValToIndex(cx, id, &index)) {
        *_retval = PR_FALSE;
        return NS_ERROR_INVALID_ARG;
    }

    PRInt32 val;
    Get(index, &val);
    *_retval = JS_NewNumberValue(cx, val, vp);

    return NS_OK;
}

NS_IMETHODIMP WebGLIntArray::SetProperty(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                                         JSObject * obj, jsval id, jsval * vp, PRBool *_retval)
{
    PRUint32 index;
    PRInt32 val;

    if (!JSValToIndex(cx, id, &index)) {
        *_retval = PR_FALSE;
        return NS_ERROR_INVALID_ARG;
    }

    PRBool ok = PR_FALSE;

    if (JSVAL_IS_INT(*vp)) {
        val = JSVAL_TO_INT(*vp);
        ok = PR_TRUE;
    } else {
        ok = JS_ValueToECMAInt32(cx, *vp, &val);
    }

    if (!ok) {
        *_retval = PR_FALSE;
        return NS_ERROR_INVALID_ARG;
    }

    Set(index, val);
    return NS_OK;
}

NS_IMETHODIMP WebGLIntArray::NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                                        JSObject * obj, jsval id, PRUint32 flags, JSObject * *objp,
                                        PRBool *_retval)
{
    PRUint32 index;
    PRBool ok = JSValToIndex(cx, id, &index);

    if (ok) {
        *_retval = PR_TRUE;
        *objp = obj;
    } else {
        *_retval = PR_FALSE;
        return NS_ERROR_FAILURE;
    }

    return NS_OK;
}

/*
 * WebGLUnsignedIntArray
 */

nsresult
NS_NewWebGLUnsignedIntArray(nsISupports **aResult)
{
    nsIWebGLUnsignedIntArray *wguia = new WebGLUnsignedIntArray();
    if (!wguia)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aResult = wguia);
    return NS_OK;
}

WebGLUnsignedIntArray::WebGLUnsignedIntArray(PRUint32 length)
    : mOffset(0), mLength(length)
{
    mBuffer = new WebGLArrayBuffer(length * sizeof(int));
}

WebGLUnsignedIntArray::WebGLUnsignedIntArray(WebGLArrayBuffer *buffer, PRUint32 offset, PRUint32 length)
    : mBuffer(buffer), mOffset(offset), mLength(length)
{
}

WebGLUnsignedIntArray::WebGLUnsignedIntArray(JSContext *cx, JSObject *arrayObj, jsuint arrayLen)
    : mOffset(0), mLength(arrayLen)
{
    mBuffer = new WebGLArrayBuffer();
    mBuffer->InitFromJSArray(LOCAL_GL_UNSIGNED_INT, 1, cx, arrayObj, arrayLen);
}

NS_IMETHODIMP
WebGLUnsignedIntArray::Initialize(nsISupports *owner,
                                  JSContext *cx,
                                  JSObject *obj,
                                  PRUint32 argc,
                                  jsval *argv)
{
    if (JSVAL_IS_NUMBER(argv[0])) {
        uint32 length;
        ::JS_ValueToECMAUint32(cx, argv[0], &length);
        mBuffer = new WebGLArrayBuffer();
        mBuffer->Prepare(LOCAL_GL_UNSIGNED_INT, 1, length);
        mBuffer->Zero();
        mLength = length;
    } else {
        JSObject *arrayObj;
        jsuint arrayLen;
        jsuint byteOffset = 0;
        jsuint length = 0;

        if (!::JS_ConvertArguments(cx, argc, argv, "o/uu", &arrayObj, &byteOffset, &length) ||
            arrayObj == NULL)
        {
            return NS_ERROR_DOM_SYNTAX_ERR;
        }

        if (::JS_IsArrayObject(cx, arrayObj) &&
            ::JS_GetArrayLength(cx, arrayObj, &arrayLen))
        {
            mBuffer = new WebGLArrayBuffer();
            mBuffer->InitFromJSArray(LOCAL_GL_UNSIGNED_INT, 1, cx, arrayObj, arrayLen);
            mLength = arrayLen;
        } else {
            nsCOMPtr<nsIWebGLArrayBuffer> canvasObj;
            nsresult rv;
            rv = nsContentUtils::XPConnect()->WrapJS(cx, arrayObj, NS_GET_IID(nsIWebGLArrayBuffer), getter_AddRefs(canvasObj));
            if (NS_FAILED(rv) || !canvasObj) {
                return NS_ERROR_DOM_SYNTAX_ERR;
            }

            mBuffer = canvasObj->GetNativeArrayBuffer();

            if (byteOffset % sizeof(int))
                return NS_ERROR_FAILURE;

            if ((byteOffset + (length * sizeof(int))) > mBuffer->capacity)
                return NS_ERROR_FAILURE;

            if (length > 0) {
                mLength = length;
            } else {
                if ((mBuffer->capacity - byteOffset) % sizeof(int))
                    return NS_ERROR_FAILURE;

                mLength = (mBuffer->capacity - byteOffset) / sizeof(int);
            }
        }
    }

    return NS_OK;
}

/* readonly attribute nsIWebGLArrayBuffer buffer; */
NS_IMETHODIMP WebGLUnsignedIntArray::GetBuffer(nsIWebGLArrayBuffer * *aBuffer)
{
    NS_ADDREF(*aBuffer = mBuffer);
    return NS_OK;
}

/* readonly attribute unsigned long byteOffset; */
NS_IMETHODIMP WebGLUnsignedIntArray::GetByteOffset(PRUint32 *aByteOffset)
{
    *aByteOffset = mOffset;
    return NS_OK;
}

/* readonly attribute unsigned long byteLength; */
NS_IMETHODIMP WebGLUnsignedIntArray::GetByteLength(PRUint32 *aByteLength)
{
    *aByteLength = mLength * sizeof(int);
    return NS_OK;
}

/* attribute unsigned long length; */
NS_IMETHODIMP WebGLUnsignedIntArray::GetLength(PRUint32 *aLength)
{
    *aLength = mLength;
    return NS_OK;
}

/* unsigned long alignedSizeInBytes (); */
NS_IMETHODIMP WebGLUnsignedIntArray::AlignedSizeInBytes(PRUint32 *retval)
{
    *retval = mBuffer->capacity;
    return NS_OK;
}

/* nsIWebGLArray slice (in unsigned long offset, in unsigned long length); */
NS_IMETHODIMP WebGLUnsignedIntArray::Slice(PRUint32 offset, PRUint32 length, nsIWebGLArray **retval)
{
    if (length == 0) 
        return NS_ERROR_FAILURE;

    if (offset + length > mBuffer->capacity)
        return NS_ERROR_FAILURE;

    nsIWebGLArray *wga = new WebGLUnsignedIntArray(mBuffer, offset, length);
    NS_ADDREF(*retval = wga);
    return NS_OK;
}

/* [IndexGetter] unsigned long get (in unsigned long index); */
NS_IMETHODIMP WebGLUnsignedIntArray::Get(PRUint32 index, PRUint32 *retval)
{
    if (index >= mLength)
        return NS_ERROR_FAILURE;

    unsigned int *values = static_cast<unsigned int*>(mBuffer->data);
    *retval = values[index];

    return NS_OK;
}

void
WebGLUnsignedIntArray::Set(PRUint32 index, unsigned int value)
{
    if (index >= mLength)
        return;

    unsigned int *values = static_cast<unsigned int*>(mBuffer->data);
    values[index] = value;
}

/* void set (); */
NS_IMETHODIMP WebGLUnsignedIntArray::Set()
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    if (js.argc < 1 || js.argc > 2)
        return NS_ERROR_DOM_SYNTAX_ERR;

    if (JSVAL_IS_NUMBER(js.argv[0])) {
        if (js.argc != 2)
            return NS_ERROR_DOM_SYNTAX_ERR;

        uint32 index;
        ::JS_ValueToECMAUint32(js.ctx, js.argv[0], &index);

        uint32 value;
        ::JS_ValueToECMAUint32(js.ctx, js.argv[1], &value);

        if (index >= mLength)
            return NS_ERROR_FAILURE;

        Set(index, value);
    } else {
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    return NS_OK;
}

NS_IMETHODIMP_(PRUint32) WebGLUnsignedIntArray::NativeType()
{
    return mBuffer->type;
}

/* [noscript, notxpcom] voidPtr nativePointer (); */
NS_IMETHODIMP_(void *) WebGLUnsignedIntArray::NativePointer()
{
    return mBuffer->data;
}

/* [noscript, notxpcom] unsigned long nativeSize (); */
NS_IMETHODIMP_(PRUint32) WebGLUnsignedIntArray::NativeSize()
{
    return mBuffer->capacity;
}

/* [noscript, notxpcom] unsigned long nativeElementSize (); */
NS_IMETHODIMP_(PRUint32) WebGLUnsignedIntArray::NativeElementSize()
{
    return mBuffer->ElementSize();
}

/* [noscript, notxpcom] unsigned long nativeCount (); */
NS_IMETHODIMP_(PRUint32) WebGLUnsignedIntArray::NativeCount()
{
    return mBuffer->length;
}

// nsIXPCScriptable
#define XPC_MAP_CLASSNAME WebGLUnsignedIntArray
#define XPC_MAP_QUOTED_CLASSNAME "WebGLUnsignedIntArray"
#define XPC_MAP_WANT_SETPROPERTY
#define XPC_MAP_WANT_GETPROPERTY
#define XPC_MAP_WANT_NEWRESOLVE
#define XPC_MAP_FLAGS nsIXPCScriptable::USE_JSSTUB_FOR_ADDPROPERTY
#include "xpc_map_end.h"

PRBool WebGLUnsignedIntArray::JSValToIndex(JSContext *cx, jsval id, PRUint32 *retval) {
    PRBool ok = PR_FALSE;
    PRUint32 index;

    if (JSVAL_IS_INT(id)) {
        index = JSVAL_TO_INT(id);
        ok = PR_TRUE;
    } else {
        ok = JS_ValueToECMAUint32(cx, id, &index);
    }

    if (!ok || index >= mLength)
        return PR_FALSE;

    *retval = index;
    return PR_TRUE;
}

NS_IMETHODIMP WebGLUnsignedIntArray::GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                                                 JSObject * obj, jsval id, jsval * vp, PRBool *_retval)
{
    PRUint32 index;
    
    if (!JSValToIndex(cx, id, &index)) {
        *_retval = PR_FALSE;
        return NS_ERROR_INVALID_ARG;
    }

    PRUint32 val;
    Get(index, &val);
    *_retval = JS_NewNumberValue(cx, val, vp);

    return NS_OK;
}

NS_IMETHODIMP WebGLUnsignedIntArray::SetProperty(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                                                 JSObject * obj, jsval id, jsval * vp, PRBool *_retval)
{
    PRUint32 index;
    PRUint32 val;

    if (!JSValToIndex(cx, id, &index)) {
        *_retval = PR_FALSE;
        return NS_ERROR_INVALID_ARG;
    }

    PRBool ok = PR_FALSE;

    if (JSVAL_IS_INT(*vp)) {
        val = JSVAL_TO_INT(*vp);
        ok = PR_TRUE;
    } else {
        ok = JS_ValueToECMAUint32(cx, *vp, &val);
    }

    if (!ok) {
        *_retval = PR_FALSE;
        return NS_ERROR_INVALID_ARG;
    }

    Set(index, val);
    return NS_OK;
}

NS_IMETHODIMP WebGLUnsignedIntArray::NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                                                JSObject * obj, jsval id, PRUint32 flags, JSObject * *objp,
                                                PRBool *_retval)
{
    PRUint32 index;
    PRBool ok = JSValToIndex(cx, id, &index);

    if (ok) {
        *_retval = PR_TRUE;
        *objp = obj;
    } else {
        *_retval = PR_FALSE;
        return NS_ERROR_FAILURE;
    }

    return NS_OK;
}


/*
 * XPCOM AddRef/Release/QI
 */
NS_IMPL_ADDREF(WebGLArrayBuffer)
NS_IMPL_RELEASE(WebGLArrayBuffer)

NS_INTERFACE_MAP_BEGIN(WebGLArrayBuffer)
  NS_INTERFACE_MAP_ENTRY(nsIWebGLArrayBuffer)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIWebGLArrayBuffer)
  NS_INTERFACE_MAP_ENTRY(nsIJSNativeInitializer)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(WebGLArrayBuffer)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebGLFloatArray)
NS_IMPL_RELEASE(WebGLFloatArray)

NS_INTERFACE_MAP_BEGIN(WebGLFloatArray)
  NS_INTERFACE_MAP_ENTRY(nsIWebGLArray)
  NS_INTERFACE_MAP_ENTRY(nsIWebGLFloatArray)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIWebGLFloatArray)
  NS_INTERFACE_MAP_ENTRY(nsIJSNativeInitializer)
  NS_INTERFACE_MAP_ENTRY(nsIXPCScriptable)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(WebGLFloatArray)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebGLByteArray)
NS_IMPL_RELEASE(WebGLByteArray)

NS_INTERFACE_MAP_BEGIN(WebGLByteArray)
  NS_INTERFACE_MAP_ENTRY(nsIWebGLArray)
  NS_INTERFACE_MAP_ENTRY(nsIWebGLByteArray)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIWebGLByteArray)
  NS_INTERFACE_MAP_ENTRY(nsIJSNativeInitializer)
  NS_INTERFACE_MAP_ENTRY(nsIXPCScriptable)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(WebGLByteArray)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebGLUnsignedByteArray)
NS_IMPL_RELEASE(WebGLUnsignedByteArray)

NS_INTERFACE_MAP_BEGIN(WebGLUnsignedByteArray)
  NS_INTERFACE_MAP_ENTRY(nsIWebGLArray)
  NS_INTERFACE_MAP_ENTRY(nsIWebGLUnsignedByteArray)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIWebGLUnsignedByteArray)
  NS_INTERFACE_MAP_ENTRY(nsIJSNativeInitializer)
  NS_INTERFACE_MAP_ENTRY(nsIXPCScriptable)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(WebGLUnsignedByteArray)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebGLShortArray)
NS_IMPL_RELEASE(WebGLShortArray)

NS_INTERFACE_MAP_BEGIN(WebGLShortArray)
  NS_INTERFACE_MAP_ENTRY(nsIWebGLArray)
  NS_INTERFACE_MAP_ENTRY(nsIWebGLShortArray)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIWebGLShortArray)
  NS_INTERFACE_MAP_ENTRY(nsIJSNativeInitializer)
  NS_INTERFACE_MAP_ENTRY(nsIXPCScriptable)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(WebGLShortArray)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebGLUnsignedShortArray)
NS_IMPL_RELEASE(WebGLUnsignedShortArray)

NS_INTERFACE_MAP_BEGIN(WebGLUnsignedShortArray)
  NS_INTERFACE_MAP_ENTRY(nsIWebGLArray)
  NS_INTERFACE_MAP_ENTRY(nsIWebGLUnsignedShortArray)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIWebGLUnsignedShortArray)
  NS_INTERFACE_MAP_ENTRY(nsIJSNativeInitializer)
  NS_INTERFACE_MAP_ENTRY(nsIXPCScriptable)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(WebGLUnsignedShortArray)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebGLIntArray)
NS_IMPL_RELEASE(WebGLIntArray)

NS_INTERFACE_MAP_BEGIN(WebGLIntArray)
  NS_INTERFACE_MAP_ENTRY(nsIWebGLArray)
  NS_INTERFACE_MAP_ENTRY(nsIWebGLIntArray)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIWebGLIntArray)
  NS_INTERFACE_MAP_ENTRY(nsIJSNativeInitializer)
  NS_INTERFACE_MAP_ENTRY(nsIXPCScriptable)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(WebGLIntArray)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebGLUnsignedIntArray)
NS_IMPL_RELEASE(WebGLUnsignedIntArray)

NS_INTERFACE_MAP_BEGIN(WebGLUnsignedIntArray)
  NS_INTERFACE_MAP_ENTRY(nsIWebGLArray)
  NS_INTERFACE_MAP_ENTRY(nsIWebGLUnsignedIntArray)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIWebGLUnsignedIntArray)
  NS_INTERFACE_MAP_ENTRY(nsIJSNativeInitializer)
  NS_INTERFACE_MAP_ENTRY(nsIXPCScriptable)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(WebGLUnsignedIntArray)
NS_INTERFACE_MAP_END
