/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ipc/AutoOpenSurface.h"
#include "mozilla/layers/PLayers.h"
#include "mozilla/layers/ShadowLayers.h"

#include "gfxSharedImageSurface.h"

#include "CanvasLayerOGL.h"

#include "gfxImageSurface.h"
#include "gfxContext.h"
#include "GLContextProvider.h"
#include "gfxPlatform.h"

#ifdef XP_WIN
#include "gfxWindowsSurface.h"
#include "WGLLibrary.h"
#endif

#ifdef XP_MACOSX
#include <OpenGL/OpenGL.h>
#endif

#ifdef MOZ_X11
#include "gfxXlibSurface.h"
#endif

using namespace mozilla;
using namespace mozilla::layers;
using namespace mozilla::gl;

static void
MakeTextureIfNeeded(GLContext* gl, GLuint& aTexture)
{
  if (aTexture != 0)
    return;

  gl->fGenTextures(1, &aTexture);

  gl->fActiveTexture(LOCAL_GL_TEXTURE0);
  gl->fBindTexture(LOCAL_GL_TEXTURE_2D, aTexture);

  gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MIN_FILTER, LOCAL_GL_LINEAR);
  gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MAG_FILTER, LOCAL_GL_LINEAR);
  gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_S, LOCAL_GL_CLAMP_TO_EDGE);
  gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_T, LOCAL_GL_CLAMP_TO_EDGE);
}

void
CanvasLayerOGL::Destroy()
{
  if (!mDestroyed) {
    CleanupResources();
    mDestroyed = true;
  }
}

void
CanvasLayerOGL::Initialize(const Data& aData)
{
  NS_ASSERTION(mCanvasSurface == nullptr, "BasicCanvasLayer::Initialize called twice!");

  if (aData.mGLContext != nullptr &&
      aData.mSurface != nullptr)
  {
    NS_WARNING("CanvasLayerOGL can't have both surface and GLContext");
    return;
  }

  mOGLManager->MakeCurrent();

  if (aData.mDrawTarget) {
    mDrawTarget = aData.mDrawTarget;
    mCanvasSurface = gfxPlatform::GetPlatform()->GetThebesSurfaceForDrawTarget(mDrawTarget);
    mNeedsYFlip = false;
  } else if (aData.mSurface) {
    mCanvasSurface = aData.mSurface;
    mNeedsYFlip = false;
#if defined(MOZ_WIDGET_GTK2) && !defined(MOZ_PLATFORM_MAEMO)
    if (aData.mSurface->GetType() == gfxASurface::SurfaceTypeXlib) {
        gfxXlibSurface *xsurf = static_cast<gfxXlibSurface*>(aData.mSurface);
        mPixmap = xsurf->GetGLXPixmap();
        if (mPixmap) {
            if (aData.mSurface->GetContentType() == gfxASurface::CONTENT_COLOR_ALPHA) {
                mLayerProgram = gl::RGBALayerProgramType;
            } else {
                mLayerProgram = gl::RGBXLayerProgramType;
            }
            MakeTextureIfNeeded(gl(), mTexture);
        }
    }
#endif
  } else if (aData.mGLContext) {
    if (!aData.mGLContext->IsOffscreen()) {
      NS_WARNING("CanvasLayerOGL with a non-offscreen GL context given");
      return;
    }

    mCanvasGLContext = aData.mGLContext;
    mGLBufferIsPremultiplied = aData.mGLBufferIsPremultiplied;

    mNeedsYFlip = mCanvasGLContext->GetOffscreenTexture() != 0;
  } else {
    NS_WARNING("CanvasLayerOGL::Initialize called without surface or GL context!");
    return;
  }

  mBounds.SetRect(0, 0, aData.mSize.width, aData.mSize.height);
      
  // Check the maximum texture size supported by GL. glTexImage2D supports
  // images of up to 2 + GL_MAX_TEXTURE_SIZE
  GLint texSize = gl()->GetMaxTextureSize();
  if (mBounds.width > (2 + texSize) || mBounds.height > (2 + texSize)) {
    mDelayedUpdates = true;
    MakeTextureIfNeeded(gl(), mTexture);
    // This should only ever occur with 2d canvas, WebGL can't already have a texture
    // of this size can it?
    NS_ABORT_IF_FALSE(mCanvasSurface || mDrawTarget, 
                      "Invalid texture size when WebGL surface already exists at that size?");
  }
}

/**
 * Following UpdateSurface(), mTexture on context this->gl() should contain the data we want,
 * unless mDelayedUpdates is true because of a too-large surface.
 */
void
CanvasLayerOGL::UpdateSurface()
{
  if (!mDirty)
    return;
  mDirty = false;

  if (mDestroyed || mDelayedUpdates) {
    return;
  }

#if defined(MOZ_WIDGET_GTK2) && !defined(MOZ_PLATFORM_MAEMO)
  if (mPixmap) {
    return;
  }
#endif

  if (mCanvasGLContext &&
      mCanvasGLContext->GetContextType() == gl()->GetContextType())
  {
    DiscardTempSurface();

    // Can texture share, just make sure it's resolved first
    mCanvasGLContext->MakeCurrent();
    mCanvasGLContext->GuaranteeResolve();

    if (gl()->BindOffscreenNeedsTexture(mCanvasGLContext) &&
        mTexture == 0)
    {
      mOGLManager->MakeCurrent();
      MakeTextureIfNeeded(gl(), mTexture);
    }
  } else {
    nsRefPtr<gfxASurface> updatedAreaSurface;

    if (mCanvasSurface) {
      updatedAreaSurface = mCanvasSurface;
    } else if (mCanvasGLContext) {
      gfxIntSize size(mBounds.width, mBounds.height);
      nsRefPtr<gfxImageSurface> updatedAreaImageSurface =
        GetTempSurface(size, gfxASurface::ImageFormatARGB32);

      mCanvasGLContext->ReadPixelsIntoImageSurface(0, 0,
                                                   mBounds.width,
                                                   mBounds.height,
                                                   updatedAreaImageSurface);

      updatedAreaSurface = updatedAreaImageSurface;
    }

    mOGLManager->MakeCurrent();
    mLayerProgram = gl()->UploadSurfaceToTexture(updatedAreaSurface,
                                                 mBounds,
                                                 mTexture,
                                                 false,
                                                 nsIntPoint(0, 0));
  }
}

void
CanvasLayerOGL::RenderLayer(int aPreviousDestination,
                            const nsIntPoint& aOffset)
{
  UpdateSurface();
  FireDidTransactionCallback();

  mOGLManager->MakeCurrent();

  // XXX We're going to need a different program depending on if
  // mGLBufferIsPremultiplied is TRUE or not.  The RGBLayerProgram
  // assumes that it's true.

  gl()->fActiveTexture(LOCAL_GL_TEXTURE0);

  if (mTexture) {
    gl()->fBindTexture(LOCAL_GL_TEXTURE_2D, mTexture);
  }

  ShaderProgramOGL *program = nullptr;

  bool useGLContext = mCanvasGLContext &&
    mCanvasGLContext->GetContextType() == gl()->GetContextType();

  nsIntRect drawRect = mBounds;

  if (useGLContext) {
    gl()->BindTex2DOffscreen(mCanvasGLContext);
    program = mOGLManager->GetBasicLayerProgram(CanUseOpaqueSurface(),
                                                true,
                                                GetMaskLayer() ? Mask2d : MaskNone);
  } else if (mDelayedUpdates) {
    NS_ABORT_IF_FALSE(mCanvasSurface || mDrawTarget, "WebGL canvases should always be using full texture upload");
    
    drawRect.IntersectRect(drawRect, GetEffectiveVisibleRegion().GetBounds());

    mLayerProgram =
      gl()->UploadSurfaceToTexture(mCanvasSurface,
                                   nsIntRect(0, 0, drawRect.width, drawRect.height),
                                   mTexture,
                                   true,
                                   drawRect.TopLeft());
  }

  if (!program) {
    program = mOGLManager->GetProgram(mLayerProgram, GetMaskLayer());
  }

#if defined(MOZ_WIDGET_GTK2) && !defined(MOZ_PLATFORM_MAEMO)
  if (mPixmap && !mDelayedUpdates) {
    sGLXLibrary.BindTexImage(mPixmap);
  }
#endif

  gl()->ApplyFilterToBoundTexture(mFilter);

  program->Activate();
  program->SetLayerQuadRect(drawRect);
  program->SetLayerTransform(GetEffectiveTransform());
  program->SetLayerOpacity(GetEffectiveOpacity());
  program->SetRenderOffset(aOffset);
  program->SetTextureUnit(0);
  program->LoadMask(GetMaskLayer());

  if (gl()->CanUploadNonPowerOfTwo()) {
    mOGLManager->BindAndDrawQuad(program, mNeedsYFlip ? true : false);
  } else {
    mOGLManager->BindAndDrawQuadWithTextureRect(program, drawRect, drawRect.Size());
  }

#if defined(MOZ_WIDGET_GTK2) && !defined(MOZ_PLATFORM_MAEMO)
  if (mPixmap && !mDelayedUpdates) {
    sGLXLibrary.ReleaseTexImage(mPixmap);
  }
#endif

  if (useGLContext) {
    gl()->UnbindTex2DOffscreen(mCanvasGLContext);
  }
}

void
CanvasLayerOGL::CleanupResources()
{
  if (mTexture) {
    gl()->MakeCurrent();
    gl()->fDeleteTextures(1, &mTexture);
  }
}

static bool
IsValidSharedTexDescriptor(const SurfaceDescriptor& aDescriptor)
{
  return aDescriptor.type() == SurfaceDescriptor::TSharedTextureDescriptor;
}

ShadowCanvasLayerOGL::ShadowCanvasLayerOGL(LayerManagerOGL* aManager)
  : ShadowCanvasLayer(aManager, nullptr)
  , LayerOGL(aManager)
  , mNeedsYFlip(false)
  , mTexture(0)
{
  mImplData = static_cast<LayerOGL*>(this);
}
 
ShadowCanvasLayerOGL::~ShadowCanvasLayerOGL()
{}

void
ShadowCanvasLayerOGL::Initialize(const Data& aData)
{
  NS_RUNTIMEABORT("Incompatibe surface type");
}

void
ShadowCanvasLayerOGL::Init(const CanvasSurface& aNewFront, bool needYFlip)
{
  AutoOpenSurface autoSurf(OPEN_READ_ONLY, aNewFront);

  mNeedsYFlip = needYFlip;

  mTexImage = gl()->CreateTextureImage(autoSurf.Size(),
                                       autoSurf.ContentType(),
                                       LOCAL_GL_CLAMP_TO_EDGE,
                                       mNeedsYFlip ? TextureImage::NeedsYFlip : TextureImage::NoFlags);
}

void
ShadowCanvasLayerOGL::Swap(const CanvasSurface& aNewFront,
                           bool needYFlip,
                           CanvasSurface* aNewBack)
{
  if (mDestroyed) {
    *aNewBack = aNewFront;
    return;
  }

  if (IsValidSharedTexDescriptor(aNewFront)) {
    MakeTextureIfNeeded(gl(), mTexture);
    if (!IsValidSharedTexDescriptor(mFrontBufferDescriptor)) {
      mFrontBufferDescriptor = SharedTextureDescriptor(TextureImage::ThreadShared, 0, nsIntSize(0, 0), false);
    }
    *aNewBack = mFrontBufferDescriptor;
    mFrontBufferDescriptor = aNewFront;
    mNeedsYFlip = needYFlip;
  } else {
    AutoOpenSurface autoSurf(OPEN_READ_ONLY, aNewFront);
    gfxIntSize sz = autoSurf.Size();
    if (!mTexImage || mTexImage->GetSize() != sz ||
        mTexImage->GetContentType() != autoSurf.ContentType()) {
      Init(aNewFront, needYFlip);
    }
    nsIntRegion updateRegion(nsIntRect(0, 0, sz.width, sz.height));
    mTexImage->DirectUpdate(autoSurf.Get(), updateRegion);
    *aNewBack = aNewFront;
  }
}

void
ShadowCanvasLayerOGL::DestroyFrontBuffer()
{
  mTexImage = nullptr;
  if (mTexture) {
    gl()->MakeCurrent();
    gl()->fDeleteTextures(1, &mTexture);
  }
  if (IsValidSharedTexDescriptor(mFrontBufferDescriptor)) {
    SharedTextureDescriptor texDescriptor = mFrontBufferDescriptor.get_SharedTextureDescriptor();
    gl()->ReleaseSharedHandle(texDescriptor.shareType(), texDescriptor.handle());
    mFrontBufferDescriptor = SurfaceDescriptor();
  }
}

void
ShadowCanvasLayerOGL::Disconnect()
{
  Destroy();
}

void
ShadowCanvasLayerOGL::Destroy()
{
  if (!mDestroyed) {
    mDestroyed = true;
    DestroyFrontBuffer();
  }
}

Layer*
ShadowCanvasLayerOGL::GetLayer()
{
  return this;
}

void
ShadowCanvasLayerOGL::RenderLayer(int aPreviousFrameBuffer,
                                  const nsIntPoint& aOffset)
{
  if (!mTexImage && !IsValidSharedTexDescriptor(mFrontBufferDescriptor)) {
    return;
  }

  mOGLManager->MakeCurrent();

  gfxPattern::GraphicsFilter filter = mFilter;
#ifdef ANDROID
  // Bug 691354
  // Using the LINEAR filter we get unexplained artifacts.
  // Use NEAREST when no scaling is required.
  gfxMatrix matrix;
  bool is2D = GetEffectiveTransform().Is2D(&matrix);
  if (is2D && !matrix.HasNonTranslationOrFlip()) {
    filter = gfxPattern::FILTER_NEAREST;
  }
#endif

  // TODO: Fix texture handling.
  // TODO: Handle mask layers.

  EffectChain effectChain;
  RefPtr<Effect> effect;
  RefPtr<Effect> effectMask;

  gfx::Matrix4x4 transform;
  mOGLManager->ToMatrix4x4(GetEffectiveTransform(), transform);

  if (IsValidSharedTexDescriptor(mFrontBufferDescriptor)) {
    // Shared texture handle rendering path, single texture rendering
    SharedTextureDescriptor texDescriptor = mFrontBufferDescriptor.get_SharedTextureDescriptor();

    // TODO: This block needs to be moved to CompositorOGL.
    gl()->fActiveTexture(LOCAL_GL_TEXTURE0);
    gl()->fBindTexture(LOCAL_GL_TEXTURE_2D, mTexture);
    if (!gl()->AttachSharedHandle(texDescriptor.shareType(), texDescriptor.handle())) {
      NS_ERROR("Failed to attach shared texture handle");
      return;
    }

    RefPtr<TextureOGL> texture = new TextureOGL();
    texture->mTextureHandle = mTexture;
    texture->mSize = gfx::IntSize(texDescriptor.size().width, texDescriptor.size().height);

    if (CanUseOpaqueSurface()) {
      effect = new EffectRGBX(texture, true, gfx::ToFilter(filter), mNeedsYFlip);
      effectChain.mEffects[EFFECT_RGBX] = effect;
    } else {
      effect = new EffectRGBA(texture, true, gfx::ToFilter(filter), mNeedsYFlip);
      effectChain.mEffects[EFFECT_RGBA] = effect;
    } else {
      NS_RUNTIMEABORT("Shader type not yet supported");
    }

    gfx::Rect rect(0, 0, texDescriptor.size().width, texDescriptor.size().height);
    gfx::Point offset(aOffset.x, aOffset.y);

    mOGLManager->GetCompositor()->DrawQuad(rect, nullptr, nullptr, effectChain,
                                           GetEffectiveOpacity(), transform,
                                           offset);

    // TODO: This needs to be moved to CompositorOGL.
    gl()->DetachSharedHandle(texDescriptor.shareType(), texDescriptor.handle());
    gl()->fBindTexture(LOCAL_GL_TEXTURE_2D, 0);
  } else {
    RefPtr<TextureOGL> texture = new TextureOGL();
    texture->mWrapMode = mTexImage->GetWrapMode();
    if (mTexImage->GetShaderProgramType() == gl::BGRXLayerProgramType) {
      effect = new EffectBGRX(texture, true, gfx::ToFilter(mFilter), mNeedsYFlip);
      effectChain.mEffects[EFFECT_BGRX] = effect;
    } else if (mTexImage->GetShaderProgramType() == gl::BGRALayerProgramType) {
      effect = new EffectBGRA(texture, true, gfx::ToFilter(mFilter), mNeedsYFlip);
      effectChain.mEffects[EFFECT_BGRA] = effect;
    } else {
      NS_RUNTIMEABORT("Shader type not yet supported");
    }

    // Tiled texture image rendering path
    mTexImage->SetFilter(filter);
    mTexImage->BeginTileIteration();

    do {
      texture->mTextureHandle = mTexImage->GetTextureID();
      texture->mSize = gfx::IntSize(mTexImage->GetTileRect().width,
                                    mTexImage->GetTileRect().height);
      gfx::Rect rect(mTexImage->GetTileRect().x, mTexImage->GetTileRect().y,
                     mTexImage->GetTileRect().width, mTexImage->GetTileRect().height);
      gfx::Rect sourceRect(0, 0, mTexImage->GetTileRect().width,
                           mTexImage->GetTileRect().height);
      gfx::Point offset(aOffset.x, aOffset.y);
      mOGLManager->GetCompositor()->DrawQuad(rect, &sourceRect, nullptr, effectChain,
                                             GetEffectiveOpacity(), transform,
                                             offset);
    } while (mTexImage->NextTile());
  }
}

void
ShadowCanvasLayerOGL::CleanupResources()
{
  DestroyFrontBuffer();
}
