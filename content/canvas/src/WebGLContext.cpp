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
 *   Vladimir Vukicevic <vladimir@pobox.com> (original author)
 *   Mark Steele <mwsteele@gmail.com>
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

#include "GLContextProvider.h"

using namespace mozilla;
using namespace mozilla::gl;

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
    : mCanvasElement(nsnull),
      gl(nsnull),
      mWidth(0), mHeight(0),
      mGeneration(0),
      mInvalidated(PR_FALSE),
      mActiveTexture(0),
      mSynthesizedGLError(LOCAL_GL_NO_ERROR)
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

/* readonly attribute nsIDOMHTMLCanvasElement canvas; */
NS_IMETHODIMP
WebGLContext::GetCanvas(nsIDOMHTMLCanvasElement **canvas)
{
    if (mCanvasElement == nsnull) {
        *canvas = nsnull;
        return NS_OK;
    }

    NS_ADDREF(*canvas = static_cast<nsIDOMHTMLCanvasElement*>(mCanvasElement));
    return NS_OK;
}

//
// nsICanvasRenderingContextInternal
//

NS_IMETHODIMP
WebGLContext::SetCanvasElement(nsHTMLCanvasElement* aParentCanvas)
{
    if (aParentCanvas == nsnull) {
        // we get this on shutdown; we should do some more cleanup here,
        // but instead we just let our destructor do it.
        mCanvasElement = nsnull;
        return NS_OK;
    }

    if (!SafeToCreateCanvas3DContext(aParentCanvas))
        return NS_ERROR_FAILURE;

    mCanvasElement = aParentCanvas;

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::SetDimensions(PRInt32 width, PRInt32 height)
{
    // If incrementing the generation would cause overflow,
    // don't allow it.  Allowing this would allow us to use
    // resource handles created from older context generations.
    if (mGeneration + 1 == 0)
        return NS_ERROR_FAILURE;

    if (mWidth == width && mHeight == height)
        return NS_OK;

    if (gl) {
        // hey we already have something
        if (gl->Resize(gfxIntSize(width, height))) {

            mWidth = width;
            mHeight = height;

            gl->fViewport(0, 0, mWidth, mHeight);
            gl->fClearColor(0, 0, 0, 0);
            gl->fClear(LOCAL_GL_COLOR_BUFFER_BIT | LOCAL_GL_DEPTH_BUFFER_BIT | LOCAL_GL_STENCIL_BUFFER_BIT);

            // great success!
            return NS_OK;
        }
    }

    LogMessage("Canvas 3D: creating PBuffer...");

    GLContextProvider::ContextFormat format(GLContextProvider::ContextFormat::BasicRGBA32);
    format.depth = 16;
    format.minDepth = 1;

    gl = gl::sGLContextProvider.CreatePBuffer(gfxIntSize(width, height), format);

    if (!gl) {
        LogMessage("Canvas 3D: can't get a native PBuffer, trying OSMesa...");
        gl = gl::GLContextProviderOSMesa::CreatePBuffer(gfxIntSize(width, height), format);
        if (!gl) {
            LogMessage("Canvas 3D: can't create a OSMesa pseudo-PBuffer.");
            return NS_ERROR_FAILURE;
        }
    }

    // We just blew away all the resources by creating a new context; reset everything,
    // and let ValidateGL set up the correct dimensions again.

    mActiveTexture = 0;
    mSynthesizedGLError = LOCAL_GL_NO_ERROR;

    mAttribBuffers.Clear();

    mUniformTextures.Clear();
    mBound2DTextures.Clear();
    mBoundCubeMapTextures.Clear();

    mBoundArrayBuffer = nsnull;
    mBoundElementArrayBuffer = nsnull;
    mCurrentProgram = nsnull;

    mFramebufferColorAttachments.Clear();
    mFramebufferDepthAttachment = nsnull;
    mFramebufferStencilAttachment = nsnull;

    mBoundFramebuffer = nsnull;
    mBoundRenderbuffer = nsnull;

    mMapTextures.Clear();
    mMapBuffers.Clear();
    mMapPrograms.Clear();
    mMapShaders.Clear();
    mMapFramebuffers.Clear();
    mMapRenderbuffers.Clear();

    // Now check the GL implementation, checking limits along the way
    if (!ValidateGL()) {
        LogMessage("Canvas 3D: Couldn't validate OpenGL implementation; is everything needed present?");
        return NS_ERROR_FAILURE;
    }

    LogMessage("Canvas 3D: ready");

    mWidth = width;
    mHeight = height;

    // increment the generation number
    mGeneration++;

    MakeContextCurrent();

    // Make sure that we clear this out, otherwise
    // we'll end up displaying random memory
    gl->fViewport(0, 0, mWidth, mHeight);
    gl->fClearColor(0, 0, 0, 0);
    gl->fClear(LOCAL_GL_COLOR_BUFFER_BIT | LOCAL_GL_DEPTH_BUFFER_BIT | LOCAL_GL_STENCIL_BUFFER_BIT);

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::Render(gfxContext *ctx, gfxPattern::GraphicsFilter f)
{
    if (!gl)
        return NS_OK;

    nsRefPtr<gfxImageSurface> surf = new gfxImageSurface(gfxIntSize(mWidth, mHeight),
                                                         gfxASurface::ImageFormatARGB32);
    if (surf->CairoStatus() != 0)
        return NS_ERROR_FAILURE;

    MakeContextCurrent();
    gl->fReadPixels(0, 0, mWidth, mHeight,
                    LOCAL_GL_BGRA,
                    LOCAL_GL_UNSIGNED_INT_8_8_8_8_REV,
                    surf->Data());

    gfxUtils::PremultiplyImageSurface(surf);

    nsRefPtr<gfxPattern> pat = new gfxPattern(surf);
    pat->SetFilter(f);

    ctx->NewPath();
    ctx->PixelSnappedRectangleAndSetPattern(gfxRect(0, 0, mWidth, mHeight), pat);
    ctx->Fill();

    return NS_OK;
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
    return NS_ERROR_NOT_AVAILABLE;
}

already_AddRefed<layers::CanvasLayer>
WebGLContext::GetCanvasLayer(LayerManager *manager)
{
    nsRefPtr<CanvasLayer> canvasLayer = manager->CreateCanvasLayer();
    if (!canvasLayer) {
        NS_WARNING("CreateCanvasLayer returned null!");
        return nsnull;
    }

    CanvasLayer::Data data;

    // the gl context may either provide a native PBuffer, in which case we want to initialize
    // data with the gl context directly, or may provide a surface to which it renders (this is the case
    // of OSMesa contexts), in which case we want to initialize data with that surface.

    void* native_pbuffer = gl->GetNativeData(gl::GLContext::NativePBuffer);
    void* native_surface = gl->GetNativeData(gl::GLContext::NativeImageSurface);

    if (native_pbuffer) {
        data.mGLContext = gl.get();
    }
    else if (native_surface) {
        data.mSurface = static_cast<gfxASurface*>(native_surface);
    }
    else {
        NS_WARNING("The GLContext has neither a native PBuffer nor a native surface!");
        return nsnull;
    }

    data.mSize = nsIntSize(mWidth, mHeight);
    data.mGLBufferIsPremultiplied = PR_FALSE;

    canvasLayer->Initialize(data);
    // once we support GL context attributes, we'll set the right thing here
    canvasLayer->SetIsOpaqueContent(PR_FALSE);
    canvasLayer->Updated(nsIntRect(0, 0, mWidth, mHeight));

    mInvalidated = PR_FALSE;

    return canvasLayer.forget().get();
}


//
// XPCOM goop
//

NS_IMPL_ADDREF(WebGLContext)
NS_IMPL_RELEASE(WebGLContext)

DOMCI_DATA(CanvasRenderingContextWebGL, WebGLContext)

NS_INTERFACE_MAP_BEGIN(WebGLContext)
  NS_INTERFACE_MAP_ENTRY(nsICanvasRenderingContextWebGL)
  NS_INTERFACE_MAP_ENTRY(nsICanvasRenderingContextInternal)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsICanvasRenderingContextWebGL)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(CanvasRenderingContextWebGL)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebGLBuffer)
NS_IMPL_RELEASE(WebGLBuffer)

DOMCI_DATA(WebGLBuffer, WebGLBuffer)

NS_INTERFACE_MAP_BEGIN(WebGLBuffer)
  NS_INTERFACE_MAP_ENTRY(WebGLBuffer)
  NS_INTERFACE_MAP_ENTRY(nsIWebGLBuffer)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(WebGLBuffer)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebGLTexture)
NS_IMPL_RELEASE(WebGLTexture)

DOMCI_DATA(WebGLTexture, WebGLTexture)

NS_INTERFACE_MAP_BEGIN(WebGLTexture)
  NS_INTERFACE_MAP_ENTRY(WebGLTexture)
  NS_INTERFACE_MAP_ENTRY(nsIWebGLTexture)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(WebGLTexture)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebGLProgram)
NS_IMPL_RELEASE(WebGLProgram)

DOMCI_DATA(WebGLProgram, WebGLProgram)

NS_INTERFACE_MAP_BEGIN(WebGLProgram)
  NS_INTERFACE_MAP_ENTRY(WebGLProgram)
  NS_INTERFACE_MAP_ENTRY(nsIWebGLProgram)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(WebGLProgram)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebGLShader)
NS_IMPL_RELEASE(WebGLShader)

DOMCI_DATA(WebGLShader, WebGLShader)

NS_INTERFACE_MAP_BEGIN(WebGLShader)
  NS_INTERFACE_MAP_ENTRY(WebGLShader)
  NS_INTERFACE_MAP_ENTRY(nsIWebGLShader)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(WebGLShader)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebGLFramebuffer)
NS_IMPL_RELEASE(WebGLFramebuffer)

DOMCI_DATA(WebGLFramebuffer, WebGLFramebuffer)

NS_INTERFACE_MAP_BEGIN(WebGLFramebuffer)
  NS_INTERFACE_MAP_ENTRY(WebGLFramebuffer)
  NS_INTERFACE_MAP_ENTRY(nsIWebGLFramebuffer)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(WebGLFramebuffer)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebGLRenderbuffer)
NS_IMPL_RELEASE(WebGLRenderbuffer)

DOMCI_DATA(WebGLRenderbuffer, WebGLRenderbuffer)

NS_INTERFACE_MAP_BEGIN(WebGLRenderbuffer)
  NS_INTERFACE_MAP_ENTRY(WebGLRenderbuffer)
  NS_INTERFACE_MAP_ENTRY(nsIWebGLRenderbuffer)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(WebGLRenderbuffer)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebGLUniformLocation)
NS_IMPL_RELEASE(WebGLUniformLocation)

DOMCI_DATA(WebGLUniformLocation, WebGLUniformLocation)

NS_INTERFACE_MAP_BEGIN(WebGLUniformLocation)
  NS_INTERFACE_MAP_ENTRY(WebGLUniformLocation)
  NS_INTERFACE_MAP_ENTRY(nsIWebGLUniformLocation)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(WebGLUniformLocation)
NS_INTERFACE_MAP_END

#define NAME_NOT_SUPPORTED(base) \
NS_IMETHODIMP base::GetName(WebGLuint *aName) \
{ return NS_ERROR_NOT_IMPLEMENTED; } \
NS_IMETHODIMP base::SetName(WebGLuint aName) \
{ return NS_ERROR_NOT_IMPLEMENTED; }

NAME_NOT_SUPPORTED(WebGLTexture)
NAME_NOT_SUPPORTED(WebGLBuffer)
NAME_NOT_SUPPORTED(WebGLProgram)
NAME_NOT_SUPPORTED(WebGLShader)
NAME_NOT_SUPPORTED(WebGLFramebuffer)
NAME_NOT_SUPPORTED(WebGLRenderbuffer)

/* [noscript] attribute WebGLint location; */
NS_IMETHODIMP WebGLUniformLocation::GetLocation(WebGLint *aLocation)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP WebGLUniformLocation::SetLocation(WebGLint aLocation)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
