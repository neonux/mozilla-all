/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
#include "WebGLContext.h"

#include "nsIConsoleService.h"
#include "nsIPrefService.h"
#include "nsServiceManagerUtils.h"
#include "nsIClassInfoImpl.h"
#include "nsContentUtils.h"
#include "nsIXPConnect.h"
#include "nsDOMError.h"

#include "gfxContext.h"
#include "gfxPattern.h"

#include "CanvasUtils.h"
#include "NativeJSContext.h"

using namespace mozilla;

nsresult NS_NewCanvasRenderingContextWebGL(nsICanvasRenderingContextWebGL** aResult);

nsresult
NS_NewCanvasRenderingContextWebGL(nsICanvasRenderingContextWebGL** aResult)
{
    nsICanvasRenderingContextWebGL* ctx = new WebGLContext();
    if (!ctx)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aResult = ctx);
    return NS_OK;
}

WebGLContext::WebGLContext()
    : gl(nsnull), mCanvasElement(nsnull), mGLPbuffer(nsnull), mWidth(0), mHeight(0),
      mInvalidated(PR_FALSE), mActiveTexture(0)
{
    mMapBuffers.Init();
    mMapTextures.Init();
    mMapPrograms.Init();
    mMapShaders.Init();
    mMapFramebuffers.Init();
    mMapRenderbuffers.Init();
}

WebGLContext::~WebGLContext()
{
}

void
WebGLContext::Invalidate()
{
    if (!mCanvasElement)
        return;

    if (mInvalidated)
        return;

    mInvalidated = true;
    mCanvasElement->InvalidateFrame();
}

//
// nsICanvasRenderingContextInternal
//

NS_IMETHODIMP
WebGLContext::SetCanvasElement(nsICanvasElement* aParentCanvas)
{
    nsresult rv;

    if (aParentCanvas == nsnull) {
        // we get this on shutdown; we should do some more cleanup here,
        // but instead we just let our destructor do it.
        return NS_OK;
    }

    if (!SafeToCreateCanvas3DContext(aParentCanvas))
        return NS_ERROR_FAILURE;

    //
    // Let's find our prefs
    //
    nsCOMPtr<nsIPrefService> prefService = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool forceSoftware = PR_FALSE;

    nsCOMPtr<nsIPrefBranch> prefBranch;
    rv = prefService->GetBranch("webgl.", getter_AddRefs(prefBranch));
    if (NS_SUCCEEDED(rv)) {
        PRBool val;

        rv = prefBranch->GetBoolPref("software_render", &val);
        if (NS_SUCCEEDED(rv))
            forceSoftware = val;
    }

    LogMessage("Canvas 3D: creating PBuffer...");

    if (!forceSoftware) {
#if defined(WINCE)
        mGLPbuffer = new nsGLPbufferEGL();
#elif defined(XP_WIN)
        mGLPbuffer = new nsGLPbufferWGL();
#elif defined(XP_UNIX) && defined(MOZ_X11)
        mGLPbuffer = new nsGLPbufferGLX();
#elif defined(XP_MACOSX)
        mGLPbuffer = new nsGLPbufferCGL();
#else
        mGLPbuffer = nsnull;
#endif

        if (mGLPbuffer && !mGLPbuffer->Init(this))
            mGLPbuffer = nsnull;
    }

    if (!mGLPbuffer) {
        mGLPbuffer = new nsGLPbufferOSMESA();
        if (!mGLPbuffer->Init(this))
            mGLPbuffer = nsnull;
    }

    if (!mGLPbuffer)
        return NS_ERROR_FAILURE;

    gl = mGLPbuffer->GL();

    if (!ValidateGL()) {
        // XXX over here we need to destroy mGLPbuffer and create a mesa buffer

        LogMessage("Canvas 3D: Couldn't validate OpenGL implementation; is everything needed present?");
        return NS_ERROR_FAILURE;
    }

    mCanvasElement = aParentCanvas;
    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::SetDimensions(PRInt32 width, PRInt32 height)
{
    if (mWidth == width && mHeight == height)
        return NS_OK;

    if (!mGLPbuffer->Resize(width, height)) {
        LogMessage("mGLPbuffer->Resize failed");
        return NS_ERROR_FAILURE;
    }

    LogMessage("Canvas 3D: ready");

    mWidth = width;
    mHeight = height;

    // Make sure that we clear this out, otherwise
    // we'll end up displaying random memory
#if 0
    int err = glGetError();
    if (err) {
        printf ("error before MakeContextCurrent! 0x%04x\n", err);
    }
#endif

    MakeContextCurrent();
    gl->fViewport(0, 0, mWidth, mHeight);
    gl->fClearColor(0, 0, 0, 0);
    gl->fClear(LOCAL_GL_COLOR_BUFFER_BIT | LOCAL_GL_DEPTH_BUFFER_BIT | LOCAL_GL_STENCIL_BUFFER_BIT);

#if 0
    err = glGetError();
    if (err) {
        printf ("error after MakeContextCurrent! 0x%04x\n", err);
    }
#endif

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::Render(gfxContext *ctx, gfxPattern::GraphicsFilter f)
{
    nsresult rv = NS_OK;

    if (mInvalidated) {
        mGLPbuffer->SwapBuffers();
        mInvalidated = PR_FALSE;
    }

    if (!mGLPbuffer)
        return NS_OK;

    // use GL Drawing if we can get a target GL context; otherwise
    // go through the fallback path.
#ifdef HAVE_GL_DRAWING
    if (mCanvasElement->GLWidgetBeginDrawing()) {
        glClearColor(0.0, 0.0, 0.0, 0.0);
        glClear(GL_COLOR_BUFFER_BIT);

        int bwidth = mGLPbuffer->Width();
        int bheight = mGLPbuffer->Height();

        GLuint tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_RECTANGLE_EXT, tex);

        CGLError err =
            CGLTexImagePBuffer(CGLGetCurrentContext(),
                               ((nsGLPbufferCGL*)mGLPbuffer)->GetCGLPbuffer(),
                               GL_BACK);
        if (err) {
            fprintf (stderr, "CGLTexImagePBuffer failed: %d\n", err);
            glDeleteTextures(1, &tex);
            return NS_OK;
        }

        glEnable(GL_TEXTURE_RECTANGLE_EXT);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        //glFrustum(-halfWidth, halfWidth, halfHeight, -halfHeight, 1.0, 100000.0);
        glOrtho(0, bwidth, bheight, 0, -0.5, 10.0);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        glBegin(GL_QUADS);

        /* Note that the texture needs a y-flip */
        glTexCoord2f(0.0, bheight);
        glVertex3f(0.0, 0.0, 0.0);

        glTexCoord2f(bwidth, bheight);
        glVertex3f(bwidth, 0.0, 0.0);

        glTexCoord2f(bwidth, 0);
        glVertex3f(bwidth, bheight, 0.0);

        glTexCoord2f(0.0, 0);
        glVertex3f(0.0, bheight, 0.0);

        glEnd();

        glDisable(GL_TEXTURE_RECTANGLE_EXT);
        glDeleteTextures(1, &tex);

        mCanvasElement->GLWidgetSwapBuffers();
        mCanvasElement->GLWidgetEndDrawing();
    } else
#endif
    {
        nsRefPtr<gfxASurface> surf = mGLPbuffer->ThebesSurface();
        if (!surf)
            return NS_OK;

        // XXX we can optimize this on win32 at least, by creating an upside-down
        // DIB.
        nsRefPtr<gfxPattern> pat = new gfxPattern(surf);
        gfxMatrix m;
        m.Translate(gfxPoint(0.0, mGLPbuffer->Height()));
        m.Scale(1.0, -1.0);
        pat->SetMatrix(m);

        // XXX I don't want to use PixelSnapped here, but layout doesn't guarantee
        // pixel alignment for this stuff!
        ctx->NewPath();
        ctx->PixelSnappedRectangleAndSetPattern(gfxRect(0, 0, mWidth, mHeight), pat);
        ctx->Fill();
    }

    return rv;
}

NS_IMETHODIMP
WebGLContext::GetInputStream(const char* aMimeType,
                             const PRUnichar* aEncoderOptions,
                             nsIInputStream **aStream)
{
    return NS_ERROR_FAILURE;

    // XXX fix this
#if 0
    if (!mGLPbuffer ||
        !mGLPbuffer->ThebesSurface())
        return NS_ERROR_FAILURE;

    nsresult rv;
    const char encoderPrefix[] = "@mozilla.org/image/encoder;2?type=";
    nsAutoArrayPtr<char> conid(new (std::nothrow) char[strlen(encoderPrefix) + strlen(aMimeType) + 1]);

    if (!conid)
        return NS_ERROR_OUT_OF_MEMORY;

    strcpy(conid, encoderPrefix);
    strcat(conid, aMimeType);

    nsCOMPtr<imgIEncoder> encoder = do_CreateInstance(conid);
    if (!encoder)
        return NS_ERROR_FAILURE;

    nsAutoArrayPtr<PRUint8> imageBuffer(new (std::nothrow) PRUint8[mWidth * mHeight * 4]);
    if (!imageBuffer)
        return NS_ERROR_OUT_OF_MEMORY;

    nsRefPtr<gfxImageSurface> imgsurf = new gfxImageSurface(imageBuffer.get(),
                                                            gfxIntSize(mWidth, mHeight),
                                                            mWidth * 4,
                                                            gfxASurface::ImageFormatARGB32);

    if (!imgsurf || imgsurf->CairoStatus())
        return NS_ERROR_FAILURE;

    nsRefPtr<gfxContext> ctx = new gfxContext(imgsurf);

    if (!ctx || ctx->HasError())
        return NS_ERROR_FAILURE;

    nsRefPtr<gfxASurface> surf = mGLPbuffer->ThebesSurface();
    nsRefPtr<gfxPattern> pat = CanvasGLThebes::CreatePattern(surf);
    gfxMatrix m;
    m.Translate(gfxPoint(0.0, mGLPbuffer->Height()));
    m.Scale(1.0, -1.0);
    pat->SetMatrix(m);

    // XXX I don't want to use PixelSnapped here, but layout doesn't guarantee
    // pixel alignment for this stuff!
    ctx->NewPath();
    ctx->PixelSnappedRectangleAndSetPattern(gfxRect(0, 0, mWidth, mHeight), pat);
    ctx->SetOperator(gfxContext::OPERATOR_SOURCE);
    ctx->Fill();

    rv = encoder->InitFromData(imageBuffer.get(),
                               mWidth * mHeight * 4, mWidth, mHeight, mWidth * 4,
                               imgIEncoder::INPUT_FORMAT_HOSTARGB,
                               nsDependentString(aEncoderOptions));
    NS_ENSURE_SUCCESS(rv, rv);

    return CallQueryInterface(encoder, aStream);
#endif
}

NS_IMETHODIMP
WebGLContext::GetThebesSurface(gfxASurface **surface)
{
    if (!mGLPbuffer) {
        *surface = nsnull;
        return NS_ERROR_NOT_AVAILABLE;
    }

    *surface = mGLPbuffer->ThebesSurface();
    NS_IF_ADDREF(*surface);
    return NS_OK;
}


//
// XPCOM goop
//

NS_IMPL_ADDREF(WebGLContext)
NS_IMPL_RELEASE(WebGLContext)

NS_INTERFACE_MAP_BEGIN(WebGLContext)
  NS_INTERFACE_MAP_ENTRY(nsICanvasRenderingContextWebGL)
  NS_INTERFACE_MAP_ENTRY(nsICanvasRenderingContextInternal)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsICanvasRenderingContextWebGL)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(CanvasRenderingContextWebGL)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebGLBuffer)
NS_IMPL_RELEASE(WebGLBuffer)

NS_INTERFACE_MAP_BEGIN(WebGLBuffer)
  NS_INTERFACE_MAP_ENTRY(nsIWebGLBuffer)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(WebGLBuffer)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebGLTexture)
NS_IMPL_RELEASE(WebGLTexture)

NS_INTERFACE_MAP_BEGIN(WebGLTexture)
  NS_INTERFACE_MAP_ENTRY(nsIWebGLTexture)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(WebGLTexture)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebGLProgram)
NS_IMPL_RELEASE(WebGLProgram)

NS_INTERFACE_MAP_BEGIN(WebGLProgram)
  NS_INTERFACE_MAP_ENTRY(nsIWebGLProgram)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(WebGLProgram)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebGLShader)
NS_IMPL_RELEASE(WebGLShader)

NS_INTERFACE_MAP_BEGIN(WebGLShader)
  NS_INTERFACE_MAP_ENTRY(nsIWebGLShader)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(WebGLShader)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebGLFramebuffer)
NS_IMPL_RELEASE(WebGLFramebuffer)

NS_INTERFACE_MAP_BEGIN(WebGLFramebuffer)
  NS_INTERFACE_MAP_ENTRY(nsIWebGLFramebuffer)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(WebGLFramebuffer)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebGLRenderbuffer)
NS_IMPL_RELEASE(WebGLRenderbuffer)

NS_INTERFACE_MAP_BEGIN(WebGLRenderbuffer)
  NS_INTERFACE_MAP_ENTRY(nsIWebGLRenderbuffer)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(WebGLRenderbuffer)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebGLArrayBuffer)
NS_IMPL_RELEASE(WebGLArrayBuffer)

NS_INTERFACE_MAP_BEGIN(WebGLArrayBuffer)
  NS_INTERFACE_MAP_ENTRY(nsICanvasArrayBuffer)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsICanvasArrayBuffer)
  NS_INTERFACE_MAP_ENTRY(nsIJSNativeInitializer)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(CanvasArrayBuffer)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebGLFloatArray)
NS_IMPL_RELEASE(WebGLFloatArray)

NS_INTERFACE_MAP_BEGIN(WebGLFloatArray)
  NS_INTERFACE_MAP_ENTRY(nsICanvasArray)
  NS_INTERFACE_MAP_ENTRY(nsICanvasFloatArray)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsICanvasFloatArray)
  NS_INTERFACE_MAP_ENTRY(nsIJSNativeInitializer)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(CanvasFloatArray)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebGLByteArray)
NS_IMPL_RELEASE(WebGLByteArray)

NS_INTERFACE_MAP_BEGIN(WebGLByteArray)
  NS_INTERFACE_MAP_ENTRY(nsICanvasArray)
  NS_INTERFACE_MAP_ENTRY(nsICanvasByteArray)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsICanvasByteArray)
  NS_INTERFACE_MAP_ENTRY(nsIJSNativeInitializer)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(CanvasByteArray)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebGLUnsignedByteArray)
NS_IMPL_RELEASE(WebGLUnsignedByteArray)

NS_INTERFACE_MAP_BEGIN(WebGLUnsignedByteArray)
  NS_INTERFACE_MAP_ENTRY(nsICanvasArray)
  NS_INTERFACE_MAP_ENTRY(nsICanvasUnsignedByteArray)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsICanvasUnsignedByteArray)
  NS_INTERFACE_MAP_ENTRY(nsIJSNativeInitializer)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(CanvasUnsignedByteArray)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebGLShortArray)
NS_IMPL_RELEASE(WebGLShortArray)

NS_INTERFACE_MAP_BEGIN(WebGLShortArray)
  NS_INTERFACE_MAP_ENTRY(nsICanvasArray)
  NS_INTERFACE_MAP_ENTRY(nsICanvasShortArray)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsICanvasShortArray)
  NS_INTERFACE_MAP_ENTRY(nsIJSNativeInitializer)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(CanvasShortArray)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebGLUnsignedShortArray)
NS_IMPL_RELEASE(WebGLUnsignedShortArray)

NS_INTERFACE_MAP_BEGIN(WebGLUnsignedShortArray)
  NS_INTERFACE_MAP_ENTRY(nsICanvasArray)
  NS_INTERFACE_MAP_ENTRY(nsICanvasUnsignedShortArray)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsICanvasUnsignedShortArray)
  NS_INTERFACE_MAP_ENTRY(nsIJSNativeInitializer)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(CanvasUnsignedShortArray)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebGLIntArray)
NS_IMPL_RELEASE(WebGLIntArray)

NS_INTERFACE_MAP_BEGIN(WebGLIntArray)
  NS_INTERFACE_MAP_ENTRY(nsICanvasArray)
  NS_INTERFACE_MAP_ENTRY(nsICanvasIntArray)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsICanvasIntArray)
  NS_INTERFACE_MAP_ENTRY(nsIJSNativeInitializer)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(CanvasIntArray)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebGLUnsignedIntArray)
NS_IMPL_RELEASE(WebGLUnsignedIntArray)

NS_INTERFACE_MAP_BEGIN(WebGLUnsignedIntArray)
  NS_INTERFACE_MAP_ENTRY(nsICanvasArray)
  NS_INTERFACE_MAP_ENTRY(nsICanvasUnsignedIntArray)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsICanvasUnsignedIntArray)
  NS_INTERFACE_MAP_ENTRY(nsIJSNativeInitializer)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(CanvasUnsignedIntArray)
NS_INTERFACE_MAP_END


nsresult
NS_NewCanvasArrayBuffer(nsISupports **aResult)
{
    nsICanvasArrayBuffer *wgab = new WebGLArrayBuffer();
    if (!wgab)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aResult = wgab);
    return NS_OK;
}

WebGLArrayBuffer::WebGLArrayBuffer(PRUint32 length)
{
    Prepare(LOCAL_GL_BYTE, 1, length);
}

NS_IMETHODIMP
WebGLArrayBuffer::Initialize(nsISupports *owner,
                            JSContext *cx,
                            JSObject *obj,
                            PRUint32 argc,
                            jsval *argv)
{
    if (JSVAL_IS_NUMBER(argv[0])) {
        PRUint32 length;
        ::JS_ValueToECMAUint32(cx, argv[0], &length);
        if (length == 0)
            return NS_ERROR_FAILURE;
        Prepare(LOCAL_GL_BYTE, 1, length);
    } else {
        return NS_ERROR_DOM_SYNTAX_ERR;
    }

    return NS_OK;
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

nsresult
NS_NewCanvasFloatArray(nsISupports **aResult)
{
    nsICanvasFloatArray *wgfa = new WebGLFloatArray();
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
        PRUint32 length;
        ::JS_ValueToECMAUint32(cx, argv[0], &length);
        mBuffer = new WebGLArrayBuffer();
        mBuffer->Prepare(LOCAL_GL_FLOAT, 1, length);
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
            nsCOMPtr<nsICanvasArrayBuffer> canvasObj;
            nsresult rv;
            rv = nsContentUtils::XPConnect()->WrapJS(cx, arrayObj, NS_GET_IID(nsICanvasArrayBuffer), getter_AddRefs(canvasObj));
            if (NS_FAILED(rv) || !canvasObj) {
                return NS_ERROR_SYNTAX_ERR;
            }

            mBuffer = canvasObj->GetNativeArrayBuffer();

            if (byteOffset % sizeof(float)) {
                return NS_ERROR_FAILURE;
            }

            if ((byteOffset + (length * sizeof(float))) > mBuffer->capacity) {
                return NS_ERROR_FAILURE;
            }

            if ((mBuffer->capacity - byteOffset) % sizeof(float)) {
                return NS_ERROR_FAILURE;
            }

            if (length > 0)
                mLength = length;
            else
                mLength = (mBuffer->capacity - byteOffset) / sizeof(float);
        }
    }

    return NS_OK;
}

/* readonly attribute nsICanvasArrayBuffer buffer; */
NS_IMETHODIMP WebGLFloatArray::GetBuffer(nsICanvasArrayBuffer **aBuffer)
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

/* nsICanvasArray slice (in unsigned long offset, in unsigned long length); */
NS_IMETHODIMP WebGLFloatArray::Slice(PRUint32 offset, PRUint32 length, nsICanvasArray **retval)
{
    if (length == 0) 
        return NS_ERROR_FAILURE;

    if (offset + length > mBuffer->capacity)
        return NS_ERROR_FAILURE;

    nsICanvasArray *wga = new WebGLFloatArray(mBuffer, offset, length);
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

        PRUint32 index;
        ::JS_ValueToECMAUint32(js.ctx, js.argv[0], &index);

        jsdouble value;
        ::JS_ValueToNumber(js.ctx, js.argv[1], &value);

        if (index >= mLength)
            return NS_ERROR_FAILURE;

        Set(index, value);
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

nsresult
NS_NewCanvasByteArray(nsISupports **aResult)
{
    nsICanvasByteArray *wgba = new WebGLByteArray();
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
        PRUint32 length;
        ::JS_ValueToECMAUint32(cx, argv[0], &length);
        mBuffer = new WebGLArrayBuffer();
        mBuffer->Prepare(LOCAL_GL_BYTE, 1, length);
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
            nsCOMPtr<nsICanvasArrayBuffer> canvasObj;
            nsresult rv;
            rv = nsContentUtils::XPConnect()->WrapJS(cx, arrayObj, NS_GET_IID(nsICanvasArrayBuffer), getter_AddRefs(canvasObj));
            if (NS_FAILED(rv) || !canvasObj) {
                return NS_ERROR_SYNTAX_ERR;
            }

            mBuffer = canvasObj->GetNativeArrayBuffer();

            if ((byteOffset + length) > mBuffer->capacity) {
                return NS_ERROR_FAILURE;
            }

            if (length > 0)
                mLength = length;
            else
                mLength = (mBuffer->capacity - byteOffset);
        }
    }

    return NS_OK;
}

/* readonly attribute nsICanvasArrayBuffer buffer; */
NS_IMETHODIMP WebGLByteArray::GetBuffer(nsICanvasArrayBuffer **aBuffer)
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

/* nsICanvasArray slice (in unsigned long offset, in unsigned long length); */
NS_IMETHODIMP WebGLByteArray::Slice(PRUint32 offset, PRUint32 length, nsICanvasArray **retval)
{
    if (length == 0) 
        return NS_ERROR_FAILURE;

    if (offset + length > mBuffer->capacity)
        return NS_ERROR_FAILURE;

    nsICanvasArray *wga = new WebGLByteArray(mBuffer, offset, length);
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

        PRUint32 index;
        ::JS_ValueToECMAUint32(js.ctx, js.argv[0], &index);

        char value;
        ::JS_ValueToECMAUint32(js.ctx, js.argv[1], (PRUint32 *) &value);

        if (index >= mLength)
            return NS_ERROR_FAILURE;

        Set(index, value);
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

nsresult
NS_NewCanvasUnsignedByteArray(nsISupports **aResult)
{
    nsICanvasUnsignedByteArray *wguba = new WebGLUnsignedByteArray();
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
        PRUint32 length;
        ::JS_ValueToECMAUint32(cx, argv[0], &length);
        mBuffer = new WebGLArrayBuffer();
        mBuffer->Prepare(LOCAL_GL_UNSIGNED_BYTE, 1, length);
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
            nsCOMPtr<nsICanvasArrayBuffer> canvasObj;
            nsresult rv;
            rv = nsContentUtils::XPConnect()->WrapJS(cx, arrayObj, NS_GET_IID(nsICanvasArrayBuffer), getter_AddRefs(canvasObj));
            if (NS_FAILED(rv) || !canvasObj) {
                return NS_ERROR_SYNTAX_ERR;
            }

            mBuffer = canvasObj->GetNativeArrayBuffer();

            if ((byteOffset + length) > mBuffer->capacity) {
                return NS_ERROR_FAILURE;
            }

            if (length > 0)
                mLength = length;
            else
                mLength = (mBuffer->capacity - byteOffset);
        }
    }

    return NS_OK;
}

/* readonly attribute nsICanvasArrayBuffer buffer; */
NS_IMETHODIMP WebGLUnsignedByteArray::GetBuffer(nsICanvasArrayBuffer **aBuffer)
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

/* nsICanvasArray slice (in unsigned long offset, in unsigned long length); */
NS_IMETHODIMP WebGLUnsignedByteArray::Slice(PRUint32 offset, PRUint32 length, nsICanvasArray **retval)
{
    if (length == 0) 
        return NS_ERROR_FAILURE;

    if (offset + length > mBuffer->capacity)
        return NS_ERROR_FAILURE;

    nsICanvasArray *wga = new WebGLUnsignedByteArray(mBuffer, offset, length);
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

        PRUint32 index;
        ::JS_ValueToECMAUint32(js.ctx, js.argv[0], &index);

        unsigned char value;
        ::JS_ValueToECMAUint32(js.ctx, js.argv[1], (PRUint32 *) &value);

        if (index >= mLength)
            return NS_ERROR_FAILURE;

        Set(index, value);
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

nsresult
NS_NewCanvasShortArray(nsISupports **aResult)
{
    nsICanvasShortArray *wgsa = new WebGLShortArray();
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
        PRUint32 length;
        ::JS_ValueToECMAUint32(cx, argv[0], &length);
        mBuffer = new WebGLArrayBuffer();
        mBuffer->Prepare(LOCAL_GL_SHORT, 1, length);
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
            nsCOMPtr<nsICanvasArrayBuffer> canvasObj;
            nsresult rv;
            rv = nsContentUtils::XPConnect()->WrapJS(cx, arrayObj, NS_GET_IID(nsICanvasArrayBuffer), getter_AddRefs(canvasObj));
            if (NS_FAILED(rv) || !canvasObj) {
                return NS_ERROR_SYNTAX_ERR;
            }

            mBuffer = canvasObj->GetNativeArrayBuffer();

            if (byteOffset % sizeof(short)) {
                return NS_ERROR_FAILURE;
            }

            if ((byteOffset + (length * sizeof(short))) > mBuffer->capacity) {
                return NS_ERROR_FAILURE;
            }

            if ((mBuffer->capacity - byteOffset) % sizeof(short)) {
                return NS_ERROR_FAILURE;
            }

            if (length > 0)
                mLength = length;
            else
                mLength = (mBuffer->capacity - byteOffset) / sizeof(short);
        }
    }

    return NS_OK;
}

/* readonly attribute nsICanvasArrayBuffer buffer; */
NS_IMETHODIMP WebGLShortArray::GetBuffer(nsICanvasArrayBuffer * *aBuffer)
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

/* nsICanvasArray slice (in unsigned long offset, in unsigned long length); */
NS_IMETHODIMP WebGLShortArray::Slice(PRUint32 offset, PRUint32 length, nsICanvasArray **retval)
{
    if (length == 0) 
        return NS_ERROR_FAILURE;

    if (offset + length > mBuffer->capacity)
        return NS_ERROR_FAILURE;

    nsICanvasArray *wga = new WebGLShortArray(mBuffer, offset, length);
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

        PRUint32 index;
        ::JS_ValueToECMAUint32(js.ctx, js.argv[0], &index);

        short value;
        ::JS_ValueToECMAUint32(js.ctx, js.argv[1], (PRUint32 *) &value);

        if (index >= mLength)
            return NS_ERROR_FAILURE;

        Set(index, value);
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


nsresult
NS_NewCanvasUnsignedShortArray(nsISupports **aResult)
{
    nsICanvasUnsignedShortArray *wgusa = new WebGLUnsignedShortArray();
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
        PRUint32 length;
        ::JS_ValueToECMAUint32(cx, argv[0], &length);
        mBuffer = new WebGLArrayBuffer();
        mBuffer->Prepare(LOCAL_GL_UNSIGNED_SHORT, 1, length);
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
            nsCOMPtr<nsICanvasArrayBuffer> canvasObj;
            nsresult rv;
            rv = nsContentUtils::XPConnect()->WrapJS(cx, arrayObj, NS_GET_IID(nsICanvasArrayBuffer), getter_AddRefs(canvasObj));
            if (NS_FAILED(rv) || !canvasObj) {
                return NS_ERROR_SYNTAX_ERR;
            }

            mBuffer = canvasObj->GetNativeArrayBuffer();

            if (byteOffset % sizeof(short)) {
                return NS_ERROR_FAILURE;
            }

            if ((byteOffset + (length * sizeof(short))) > mBuffer->capacity) {
                return NS_ERROR_FAILURE;
            }

            if ((mBuffer->capacity - byteOffset) % sizeof(short)) {
                return NS_ERROR_FAILURE;
            }

            if (length > 0)
                mLength = length;
            else
                mLength = (mBuffer->capacity - byteOffset) / sizeof(short);
        }
    }

    return NS_OK;
}

/* readonly attribute nsICanvasArrayBuffer buffer; */
NS_IMETHODIMP WebGLUnsignedShortArray::GetBuffer(nsICanvasArrayBuffer * *aBuffer)
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

/* nsICanvasArray slice (in unsigned long offset, in unsigned long length); */
NS_IMETHODIMP WebGLUnsignedShortArray::Slice(PRUint32 offset, PRUint32 length, nsICanvasArray **retval)
{
    if (length == 0) 
        return NS_ERROR_FAILURE;

    if (offset + length > mBuffer->capacity)
        return NS_ERROR_FAILURE;

    nsICanvasArray *wga = new WebGLUnsignedShortArray(mBuffer, offset, length);
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

        PRUint32 index;
        ::JS_ValueToECMAUint32(js.ctx, js.argv[0], &index);

        unsigned short value;
        ::JS_ValueToECMAUint32(js.ctx, js.argv[1], (PRUint32 *) &value);

        if (index >= mLength)
            return NS_ERROR_FAILURE;

        Set(index, value);
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

nsresult
NS_NewCanvasIntArray(nsISupports **aResult)
{
    nsICanvasIntArray *wgia = new WebGLIntArray();
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
        PRUint32 length;
        ::JS_ValueToECMAUint32(cx, argv[0], &length);
        mBuffer = new WebGLArrayBuffer();
        mBuffer->Prepare(LOCAL_GL_INT, 1, length);
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
            nsCOMPtr<nsICanvasArrayBuffer> canvasObj;
            nsresult rv;
            rv = nsContentUtils::XPConnect()->WrapJS(cx, arrayObj, NS_GET_IID(nsICanvasArrayBuffer), getter_AddRefs(canvasObj));
            if (NS_FAILED(rv) || !canvasObj) {
                return NS_ERROR_SYNTAX_ERR;
            }

            mBuffer = canvasObj->GetNativeArrayBuffer();

            if (byteOffset % sizeof(int)) {
                return NS_ERROR_FAILURE;
            }

            if ((byteOffset + (length * sizeof(int))) > mBuffer->capacity) {
                return NS_ERROR_FAILURE;
            }

            if ((mBuffer->capacity - byteOffset) % sizeof(int)) {
                return NS_ERROR_FAILURE;
            }

            if (length > 0)
                mLength = length;
            else
                mLength = (mBuffer->capacity - byteOffset) / sizeof(int);
        }
    }

    return NS_OK;
}

/* readonly attribute nsICanvasArrayBuffer buffer; */
NS_IMETHODIMP WebGLIntArray::GetBuffer(nsICanvasArrayBuffer * *aBuffer)
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

/* nsICanvasArray slice (in unsigned long offset, in unsigned long length); */
NS_IMETHODIMP WebGLIntArray::Slice(PRUint32 offset, PRUint32 length, nsICanvasArray **retval)
{
    if (length == 0) 
        return NS_ERROR_FAILURE;

    if (offset + length > mBuffer->capacity)
        return NS_ERROR_FAILURE;

    nsICanvasArray *wga = new WebGLIntArray(mBuffer, offset, length);
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

        PRUint32 index;
        ::JS_ValueToECMAUint32(js.ctx, js.argv[0], &index);

        PRInt32 value;
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

nsresult
NS_NewCanvasUnsignedIntArray(nsISupports **aResult)
{
    nsICanvasUnsignedIntArray *wguia = new WebGLUnsignedIntArray();
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
        PRUint32 length;
        ::JS_ValueToECMAUint32(cx, argv[0], &length);
        mBuffer = new WebGLArrayBuffer();
        mBuffer->Prepare(LOCAL_GL_UNSIGNED_INT, 1, length);
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
            nsCOMPtr<nsICanvasArrayBuffer> canvasObj;
            nsresult rv;
            rv = nsContentUtils::XPConnect()->WrapJS(cx, arrayObj, NS_GET_IID(nsICanvasArrayBuffer), getter_AddRefs(canvasObj));
            if (NS_FAILED(rv) || !canvasObj) {
                return NS_ERROR_SYNTAX_ERR;
            }

            mBuffer = canvasObj->GetNativeArrayBuffer();

            if (byteOffset % sizeof(int)) {
                return NS_ERROR_FAILURE;
            }

            if ((byteOffset + (length * sizeof(int))) > mBuffer->capacity) {
                return NS_ERROR_FAILURE;
            }

            if ((mBuffer->capacity - byteOffset) % sizeof(int)) {
                return NS_ERROR_FAILURE;
            }

            if (length > 0)
                mLength = length;
            else
                mLength = (mBuffer->capacity - byteOffset) / sizeof(int);
        }
    }

    return NS_OK;
}

/* readonly attribute nsICanvasArrayBuffer buffer; */
NS_IMETHODIMP WebGLUnsignedIntArray::GetBuffer(nsICanvasArrayBuffer * *aBuffer)
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

/* nsICanvasArray slice (in unsigned long offset, in unsigned long length); */
NS_IMETHODIMP WebGLUnsignedIntArray::Slice(PRUint32 offset, PRUint32 length, nsICanvasArray **retval)
{
    if (length == 0) 
        return NS_ERROR_FAILURE;

    if (offset + length > mBuffer->capacity)
        return NS_ERROR_FAILURE;

    nsICanvasArray *wga = new WebGLUnsignedIntArray(mBuffer, offset, length);
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

        PRUint32 index;
        ::JS_ValueToECMAUint32(js.ctx, js.argv[0], &index);

        PRUint32 value;
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

/* [noscript] attribute GLuint name; */
NS_IMETHODIMP WebGLTexture::GetName(GLuint *aName)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP WebGLTexture::SetName(GLuint aName)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* [noscript] attribute GLuint name; */
NS_IMETHODIMP WebGLBuffer::GetName(GLuint *aName)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP WebGLBuffer::SetName(GLuint aName)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* [noscript] attribute GLuint name; */
NS_IMETHODIMP WebGLProgram::GetName(GLuint *aName)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP WebGLProgram::SetName(GLuint aName)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* [noscript] attribute GLuint name; */
NS_IMETHODIMP WebGLShader::GetName(GLuint *aName)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP WebGLShader::SetName(GLuint aName)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* [noscript] attribute GLuint name; */
NS_IMETHODIMP WebGLFramebuffer::GetName(GLuint *aName)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP WebGLFramebuffer::SetName(GLuint aName)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* [noscript] attribute GLuint name; */
NS_IMETHODIMP WebGLRenderbuffer::GetName(GLuint *aName)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP WebGLRenderbuffer::SetName(GLuint aName)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
