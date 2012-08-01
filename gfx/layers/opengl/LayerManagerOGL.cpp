/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/PLayers.h"

/* This must occur *after* layers/PLayers.h to avoid typedefs conflicts. */
#include "mozilla/Util.h"

#include "LayerManagerOGL.h"
#include "ThebesLayerOGL.h"
#include "ContainerLayerOGL.h"
#include "ImageLayerOGL.h"
#include "ColorLayerOGL.h"
#include "CanvasLayerOGL.h"
#include "TiledThebesLayerOGL.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Preferences.h"
#include "TexturePoolOGL.h"

#include "gfxContext.h"
#include "gfxUtils.h"
#include "gfxPlatform.h"
#include "nsIWidget.h"

#include "GLContext.h"
#include "GLContextProvider.h"

#include "nsIServiceManager.h"
#include "nsIConsoleService.h"

#include "gfxCrashReporterUtils.h"

#include "sampler.h"

#ifdef MOZ_WIDGET_ANDROID
#include <android/log.h>
#endif

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;
using namespace mozilla::gl;


/*
Removing:
mGLContext
gl()
mWidget
mSurfaceSize
mIsRenderingToEGLSurface
shader stuff

world transform?
  SetupPipeline
transactions
  mRoot
  Render
    CopyToTarget
    mClippingRegion
    back buffer stuff
      SetupBackBuffer
*/


/**
 * LayerManagerOGL
 */
LayerManagerOGL::LayerManagerOGL(nsIWidget *aWidget, int aSurfaceWidth, int aSurfaceHeight,
                                 bool aIsRenderingToEGLSurface)
  : mBackBufferFBO(0)
  , mBackBufferTexture(0)
  , mBackBufferSize(-1, -1)
{
  mCompositor = new CompositorOGL(aWidget, aSurfaceWidth, aSurfaceHeight, aIsRenderingToEGLSurface);
}

void
LayerManagerOGL::Destroy()
{
  if (!mDestroyed) {
    if (mRoot) {
      RootLayer()->Destroy();
    }
    mRoot = nullptr;

    mCompositor->Destroy();

    mDestroyed = true;
  }
}


void
LayerManagerOGL::BeginTransaction()
{
}

void
LayerManagerOGL::BeginTransactionWithTarget(gfxContext *aTarget)
{
#ifdef MOZ_LAYERS_HAVE_LOG
  MOZ_LAYERS_LOG(("[----- BeginTransaction"));
  Log();
#endif

  if (mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return;
  }

  mTarget = aTarget;
}

bool
LayerManagerOGL::EndEmptyTransaction()
{
  if (!mRoot)
    return false;

  EndTransaction(nullptr, nullptr);
  return true;
}

void
LayerManagerOGL::EndTransaction(DrawThebesLayerCallback aCallback,
                                void* aCallbackData,
                                EndTransactionFlags aFlags)
{
#ifdef MOZ_LAYERS_HAVE_LOG
  MOZ_LAYERS_LOG(("  ----- (beginning paint)"));
  Log();
#endif

  if (mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return;
  }

  if (mRoot && !(aFlags & END_NO_IMMEDIATE_REDRAW)) {
    // The results of our drawing always go directly into a pixel buffer,
    // so we don't need to pass any global transform here.
    mRoot->ComputeEffectiveTransforms(gfx3DMatrix());

    mThebesLayerCallback = aCallback;
    mThebesLayerCallbackData = aCallbackData;

    Render();

    mThebesLayerCallback = nullptr;
    mThebesLayerCallbackData = nullptr;
  }

  mTarget = NULL;

#ifdef MOZ_LAYERS_HAVE_LOG
  Log();
  MOZ_LAYERS_LOG(("]----- EndTransaction"));
#endif
}

already_AddRefed<gfxASurface>
LayerManagerOGL::CreateOptimalMaskSurface(const gfxIntSize &aSize)
{
  return gfxPlatform::GetPlatform()->
    CreateOffscreenImageSurface(aSize, gfxASurface::CONTENT_ALPHA);
}

already_AddRefed<ThebesLayer>
LayerManagerOGL::CreateThebesLayer()
{
  if (mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nullptr;
  }

  nsRefPtr<ThebesLayer> layer = new ThebesLayerOGL(this);
  return layer.forget();
}

already_AddRefed<ContainerLayer>
LayerManagerOGL::CreateContainerLayer()
{
  if (mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nullptr;
  }

  nsRefPtr<ContainerLayer> layer = new ContainerLayerOGL(this);
  return layer.forget();
}

already_AddRefed<ImageLayer>
LayerManagerOGL::CreateImageLayer()
{
  if (mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nullptr;
  }

  nsRefPtr<ImageLayer> layer = new ImageLayerOGL(this);
  return layer.forget();
}

already_AddRefed<ColorLayer>
LayerManagerOGL::CreateColorLayer()
{
  if (mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nullptr;
  }

  nsRefPtr<ColorLayer> layer = new ColorLayerOGL(this);
  return layer.forget();
}

already_AddRefed<CanvasLayer>
LayerManagerOGL::CreateCanvasLayer()
{
  if (mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nullptr;
  }

  nsRefPtr<CanvasLayer> layer = new CanvasLayerOGL(this);
  return layer.forget();
}

LayerOGL*
LayerManagerOGL::RootLayer() const
{
  if (mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nullptr;
  }

  return static_cast<LayerOGL*>(mRoot->ImplData());
}


//TODO[nrc]
void
LayerManagerOGL::Render()
{
  SAMPLE_LABEL("LayerManagerOGL", "Render");
  if (mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return;
  }

  mCompositor->BeginFrame(mRoot->GetClipRect());

//  WorldTransformRect(rect);

  // We can't draw anything to something with no area
  // so just return
//  if (width == 0 || height == 0)
//    return;


#if MOZ_WIDGET_ANDROID
  TexturePoolOGL::Fill(gl());
#endif

  SetupBackBuffer(width, height);
//  SetupPipeline(width, height, ApplyWorldTransform);

  // If the Java compositor is being used, this clear will be done in
  // DrawWindowUnderlay. Make sure the bits used here match up with those used
  // in mobile/android/base/gfx/LayerRenderer.java
#ifndef MOZ_JAVA_COMPOSITOR
  mCompositor->mGLContext->fClearColor(0.0, 0.0, 0.0, 0.0);
  mCompositor->mGLContext->fClear(LOCAL_GL_COLOR_BUFFER_BIT | LOCAL_GL_DEPTH_BUFFER_BIT);
#endif

  // Allow widget to render a custom background.
  mWidget->DrawWindowUnderlay(this, rect);

  // Render our layers.
  RootLayer()->RenderLayer(mCompositor->mGLContext->IsDoubleBuffered() ? 0 : mBackBufferFBO,
                           nsIntPoint(0, 0));

  // Allow widget to render a custom foreground too.
  mWidget->DrawWindowOverlay(this, rect);

#ifdef MOZ_DUMP_PAINTING
  if (gfxUtils::sDumpPainting) {
    nsIntRect rect;
    if (mIsRenderingToEGLSurface) {
      rect = nsIntRect(0, 0, mSurfaceSize.width, mSurfaceSize.height);
    } else {
      mWidget->GetBounds(rect);
    }
    nsRefPtr<gfxASurface> surf = gfxPlatform::GetPlatform()->CreateOffscreenSurface(rect.Size(), gfxASurface::CONTENT_COLOR_ALPHA);
    nsRefPtr<gfxContext> ctx = new gfxContext(surf);
    CopyToTarget(ctx);

    WriteSnapshotToDumpFile(this, surf);
  }
#endif

  if (mTarget) {
    CopyToTarget(mTarget);
    mCompositor->mGLContext->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, 0);
    return;
  }

  if (sDrawFPS) {
    mFPS.DrawFPS(mCompositor->mGLContext, GetProgram(Copy2DProgramType));
  }

  mCompositor->EndFrame();

  mCompositor->mGLContext->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, 0);

  mCompositor->mGLContext->fActiveTexture(LOCAL_GL_TEXTURE0);

  ShaderProgramOGL *copyprog = GetProgram(Copy2DProgramType);

  if (mFBOTextureTarget == LOCAL_GL_TEXTURE_RECTANGLE_ARB) {
    copyprog = GetProgram(Copy2DRectProgramType);
  }

  mCompositor->mGLContext->fBindTexture(mFBOTextureTarget, mBackBufferTexture);

  copyprog->Activate();
  copyprog->SetTextureUnit(0);

  if (copyprog->GetTexCoordMultiplierUniformLocation() != -1) {
    copyprog->SetTexCoordMultiplier(width, height);
  }

  // we're going to use client-side vertex arrays for this.
  mCompositor->mGLContext->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, 0);

  // "COPY"
  mCompositor->mGLContext->fBlendFuncSeparate(LOCAL_GL_ONE, LOCAL_GL_ZERO,
                                 LOCAL_GL_ONE, LOCAL_GL_ZERO);

  // enable our vertex attribs; we'll call glVertexPointer below
  // to fill with the correct data.
  GLint vcattr = copyprog->AttribLocation(ShaderProgramOGL::VertexCoordAttrib);
  GLint tcattr = copyprog->AttribLocation(ShaderProgramOGL::TexCoordAttrib);

  mCompositor->mGLContext->fEnableVertexAttribArray(vcattr);
  mCompositor->mGLContext->fEnableVertexAttribArray(tcattr);

  const nsIntRect *r;
  nsIntRegionRectIterator iter(mClippingRegion);

  while ((r = iter.Next()) != nullptr) {
    nsIntRect cRect = *r; r = &cRect;
    WorldTransformRect(cRect);
    float left = (GLfloat)r->x / width;
    float right = (GLfloat)r->XMost() / width;
    float top = (GLfloat)r->y / height;
    float bottom = (GLfloat)r->YMost() / height;

    float vertices[] = { left * 2.0f - 1.0f,
                         -(top * 2.0f - 1.0f),
                         right * 2.0f - 1.0f,
                         -(top * 2.0f - 1.0f),
                         left * 2.0f - 1.0f,
                         -(bottom * 2.0f - 1.0f),
                         right * 2.0f - 1.0f,
                         -(bottom * 2.0f - 1.0f) };

    // Use inverted texture coordinates since our projection matrix also has a
    // flip and we need to cancel that out.
    float coords[] = { left, 1 - top,
                       right, 1 - top,
                       left, 1 - bottom,
                       right, 1 - bottom };

    mCompositor->mGLContext->fVertexAttribPointer(vcattr,
                                     2, LOCAL_GL_FLOAT,
                                     LOCAL_GL_FALSE,
                                     0, vertices);

    mCompositor->mGLContext->fVertexAttribPointer(tcattr,
                                     2, LOCAL_GL_FLOAT,
                                     LOCAL_GL_FALSE,
                                     0, coords);

    mCompositor->mGLContext->fDrawArrays(LOCAL_GL_TRIANGLE_STRIP, 0, 4);
  }

  mCompositor->mGLContext->fDisableVertexAttribArray(vcattr);
  mCompositor->mGLContext->fDisableVertexAttribArray(tcattr);

  mCompositor->mGLContext->fFlush();
  mCompositor->mGLContext->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, 0);
}

void
LayerManagerOGL::SetWorldTransform(const gfxMatrix& aMatrix)
{
  NS_ASSERTION(aMatrix.PreservesAxisAlignedRectangles(),
               "SetWorldTransform only accepts matrices that satisfy PreservesAxisAlignedRectangles");
  NS_ASSERTION(!aMatrix.HasNonIntegerScale(),
               "SetWorldTransform only accepts matrices with integer scale");

  mWorldMatrix = aMatrix;
}

gfxMatrix&
LayerManagerOGL::GetWorldTransform(void)
{
  return mWorldMatrix;
}

void
LayerManagerOGL::WorldTransformRect(nsIntRect& aRect)
{
  gfxRect grect(aRect.x, aRect.y, aRect.width, aRect.height);
  grect = mWorldMatrix.TransformBounds(grect);
  aRect.SetRect(grect.X(), grect.Y(), grect.Width(), grect.Height());
}

void
LayerManagerOGL::SetupPipeline(int aWidth, int aHeight, WorldTransforPolicy aTransformPolicy)
{
  mCompositor->SetupPipeline(aWidth, aHeight);

  // XXX: We keep track of whether the window size changed, so we could skip
  // this update if it hadn't changed since the last call. We will need to
  // track changes to aTransformPolicy and mWorldMatrix for this to work
  // though.

  //TODO[nrc]
  if (aTransformPolicy == ApplyWorldTransform) {
    viewMatrix = mWorldMatrix * viewMatrix;
  }
}

//TODO[nrc]
void
LayerManagerOGL::SetupBackBuffer(int aWidth, int aHeight)
{
  GLContext* ctxt = mCompositor->mGLContext;

  if (mCompositor->mGLContext->IsDoubleBuffered()) {
    ctxt->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, 0);
    return;
  }

  // Do we have a FBO of the right size already?
  if (mBackBufferSize.width == aWidth &&
      mBackBufferSize.height == aHeight)
  {
    ctxt->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, mBackBufferFBO);
    return;
  }

  // we already have a FBO, but we need to resize its texture.
  ctxt->fActiveTexture(LOCAL_GL_TEXTURE0);
  ctxt->fBindTexture(mFBOTextureTarget, mBackBufferTexture);
  ctxt->fTexImage2D(mFBOTextureTarget,
                          0,
                          LOCAL_GL_RGBA,
                          aWidth, aHeight,
                          0,
                          LOCAL_GL_RGBA,
                          LOCAL_GL_UNSIGNED_BYTE,
                          NULL);
  ctxt->fBindTexture(mFBOTextureTarget, 0);

  ctxt->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, mBackBufferFBO);
  ctxt->fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER,
                                    LOCAL_GL_COLOR_ATTACHMENT0,
                                    mFBOTextureTarget,
                                    mBackBufferTexture,
                                    0);

  GLenum result = ctxt->fCheckFramebufferStatus(LOCAL_GL_FRAMEBUFFER);
  if (result != LOCAL_GL_FRAMEBUFFER_COMPLETE) {
    nsCAutoString msg;
    msg.Append("Framebuffer not complete -- error 0x");
    msg.AppendInt(result, 16);
    NS_RUNTIMEABORT(msg.get());
  }

  mBackBufferSize.width = aWidth;
  mBackBufferSize.height = aHeight;
}

//TODO[nrc]
void
LayerManagerOGL::CopyToTarget(gfxContext *aTarget)
{
  nsIntRect rect;
  if (mIsRenderingToEGLSurface) {
    rect = nsIntRect(0, 0, mSurfaceSize.width, mSurfaceSize.height);
  } else {
    mWidget->GetBounds(rect);
  }
  GLint width = rect.width;
  GLint height = rect.height;

  if ((PRInt64(width) * PRInt64(height) * PRInt64(4)) > PR_INT32_MAX) {
    NS_ERROR("Widget size too big - integer overflow!");
    return;
  }

  nsRefPtr<gfxImageSurface> imageSurface =
    new gfxImageSurface(gfxIntSize(width, height),
                        gfxASurface::ImageFormatARGB32);

  mCompositor->mGLContext->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER,
                               mCompositor->mGLContext->IsDoubleBuffered() ? 0 : mBackBufferFBO);

  if (!mCompositor->mGLContext->IsGLES2()) {
    // GLES2 promises that binding to any custom FBO will attach
    // to GL_COLOR_ATTACHMENT0 attachment point.
    if (mCompositor->mGLContext->IsDoubleBuffered()) {
      mCompositor->mGLContext->fReadBuffer(LOCAL_GL_BACK);
    }
    else {
      mCompositor->mGLContext->fReadBuffer(LOCAL_GL_COLOR_ATTACHMENT0);
    }
  }

  NS_ASSERTION(imageSurface->Stride() == width * 4,
               "Image Surfaces being created with weird stride!");

  mCompositor->mGLContext->ReadPixelsIntoImageSurface(0, 0, width, height, imageSurface);

  aTarget->SetOperator(gfxContext::OPERATOR_SOURCE);
  aTarget->Scale(1.0, -1.0);
  aTarget->Translate(-gfxPoint(0.0, height));
  aTarget->SetSource(imageSurface);
  aTarget->Paint();
}


already_AddRefed<ShadowThebesLayer>
LayerManagerOGL::CreateShadowThebesLayer()
{
  if (LayerManagerOGL::mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nullptr;
  }
#ifdef FORCE_BASICTILEDTHEBESLAYER
  return nsRefPtr<ShadowThebesLayer>(new TiledThebesLayerOGL(this)).forget();
#else
  return nsRefPtr<ShadowThebesLayerOGL>(new ShadowThebesLayerOGL(this)).forget();
#endif
}

already_AddRefed<ShadowContainerLayer>
LayerManagerOGL::CreateShadowContainerLayer()
{
  if (LayerManagerOGL::mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nullptr;
  }
  return nsRefPtr<ShadowContainerLayerOGL>(new ShadowContainerLayerOGL(this)).forget();
}

already_AddRefed<ShadowImageLayer>
LayerManagerOGL::CreateShadowImageLayer()
{
  if (LayerManagerOGL::mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nullptr;
  }
  return nsRefPtr<ShadowImageLayerOGL>(new ShadowImageLayerOGL(this)).forget();
}

already_AddRefed<ShadowColorLayer>
LayerManagerOGL::CreateShadowColorLayer()
{
  if (LayerManagerOGL::mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nullptr;
  }
  return nsRefPtr<ShadowColorLayerOGL>(new ShadowColorLayerOGL(this)).forget();
}

already_AddRefed<ShadowCanvasLayer>
LayerManagerOGL::CreateShadowCanvasLayer()
{
  if (LayerManagerOGL::mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nullptr;
  }
  return nsRefPtr<ShadowCanvasLayerOGL>(new ShadowCanvasLayerOGL(this)).forget();
}

already_AddRefed<ShadowRefLayer>
LayerManagerOGL::CreateShadowRefLayer()
{
  if (LayerManagerOGL::mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nullptr;
  }
  return nsRefPtr<ShadowRefLayerOGL>(new ShadowRefLayerOGL(this)).forget();
}

} /* layers */
} /* mozilla */
