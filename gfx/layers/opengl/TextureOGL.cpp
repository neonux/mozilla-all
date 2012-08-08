/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextureOGL.h"
#include "ipc/AutoOpenSurface.h"

namespace mozilla {
namespace layers {


static void
MakeTextureIfNeeded(GLContext* gl, GLuint& aTexture)
{
  if (aTexture != 0)
    return;

  gl->fGenTextures(1, &aTexture);

  gl->fBindTexture(LOCAL_GL_TEXTURE_2D, aTexture);

  gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MIN_FILTER, LOCAL_GL_LINEAR);
  gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MAG_FILTER, LOCAL_GL_LINEAR);
  gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_S, LOCAL_GL_CLAMP_TO_EDGE);
  gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_T, LOCAL_GL_CLAMP_TO_EDGE);
}

ImageSourceOGL::ImageSourceOGL(CompositorOGL* aCompositorOGL, const SurfaceDescriptor& aSurface, bool aForceSingleTile)
: mForceSingleTile(aForceSingleTile)
, ATextureOGL(aCompositorOGL)
{
  AutoOpenSurface autoSurf(OPEN_READ_ONLY, aSurface);
  mSize = autoSurf.Size();
  mTexImage = mCompositorOGL->gl()->CreateTextureImage(nsIntSize(mSize.width, mSize.height),
                                                       autoSurf.ContentType(),
                                                       LOCAL_GL_CLAMP_TO_EDGE,
                                                       mForceSingleTile
                                                       ? TextureImage::ForceSingleTile
                                                       : TextureImage::NoFlags);
}

void
ImageSourceOGL::Composite(EffectChain& aEffectChain,
                          float aOpacity,
                          const gfx::Matrix4x4& aTransform,
                          const gfx::Point& aOffset,
                          const gfx::Filter aFilter)
{
  NS_ASSERTION(mTexImage->GetContentType() != gfxASurface::CONTENT_ALPHA,
               "Image layer has alpha image");

  mWrapMode = mTexImage->GetWrapMode();

  if (mTexImage->GetShaderProgramType() == gl::BGRXLayerProgramType) {
    EffectBGRX* effect = new EffectBGRX(this, true, aFilter);
    aEffectChain.mEffects[EFFECT_BGRX] = effect;
  } else if (mTexImage->GetShaderProgramType() == gl::BGRALayerProgramType) {
    EffectBGRA* effect = new EffectBGRA(this, true, aFilter);
    aEffectChain.mEffects[EFFECT_BGRA] = effect;
  } else {
    NS_RUNTIMEABORT("Shader type not yet supported");
  }

  mTexImage->SetFilter(gfx::ThebesFilter(aFilter));
  mTexImage->BeginTileIteration();

  do {
    mSize = nsIntSize(mTexImage->GetTileRect().width,
                         mTexImage->GetTileRect().height);
    gfx::Rect rect(mTexImage->GetTileRect().x, mTexImage->GetTileRect().y,
                   mTexImage->GetTileRect().width, mTexImage->GetTileRect().height);
    gfx::Rect sourceRect(0, 0, mTexImage->GetTileRect().width,
                         mTexImage->GetTileRect().height);
    mCompositorOGL->DrawQuad(rect, &sourceRect, nullptr, aEffectChain,
                          aOpacity, aTransform, aOffset);
  } while (mTexImage->NextTile());
}

void
ImageSourceOGL::UpdateImage(const SharedImage& aImage)
{
  SurfaceDescriptor surface = aImage.get_SurfaceDescriptor();

  AutoOpenSurface surf(OPEN_READ_ONLY, surface);
  gfxIntSize size = surf.Size();

  NS_ASSERTION(mTexImage, "mTextImage should never be null");
  if (mSize != size ||
      mTexImage->GetContentType() != surf.ContentType()) {
    mSize = size;
    mTexImage = mCompositorOGL->gl()->CreateTextureImage(nsIntSize(mSize.width, mSize.height),
                                                         surf.ContentType(),
                                                         LOCAL_GL_CLAMP_TO_EDGE,
                                                         mForceSingleTile
                                                           ? TextureImage::ForceSingleTile
                                                           : TextureImage::NoFlags);
  }

  // XXX this is always just ridiculously slow
  nsIntRegion updateRegion(nsIntRect(0, 0, size.width, size.height));
  mTexImage->DirectUpdate(surf.Get(), updateRegion);
}

ImageSourceOGLShared::ImageSourceOGLShared(CompositorOGL* aCompositorOGL, const SharedTextureDescriptor& aTexture)
  : ATextureOGL(aCompositorOGL)
  , mSize(aTexture.size())
  , mSharedHandle(aTexture.handle())
  , mShareType(aTexture.shareType())
  , mInverted(aTexture.inverted())
{
}

void
ImageSourceOGLShared::Composite(EffectChain& aEffectChain,
                                float aOpacity,
                                const gfx::Matrix4x4& aTransform,
                                const gfx::Point& aOffset,
                                const gfx::Filter aFilter)
{
  GLContext::SharedHandleDetails handleDetails;
  if (!mCompositorOGL->gl()->GetSharedHandleDetails(mShareType, mSharedHandle, handleDetails)) {
    NS_ERROR("Failed to get shared handle details");
    return;
  }

  if (handleDetails.mProgramType == gl::RGBALayerProgramType) {
    EffectRGBA* effect = new EffectRGBA(this, true, aFilter, mInverted);
    aEffectChain.mEffects[EFFECT_RGBA] = effect;
  } else if (handleDetails.mProgramType == gl::RGBALayerExternalProgramType) {
    gfx::Matrix4x4 textureTransform;
    LayerManagerOGL::ToMatrix4x4(handleDetails.mTextureTransform, textureTransform);
    EffectRGBAExternal* effect = new EffectRGBAExternal(this, textureTransform, true, aFilter, mInverted);
    aEffectChain.mEffects[EFFECT_RGBA_EXTERNAL] = effect;
  } else {
    NS_RUNTIMEABORT("Shader type not yet supported");
  }

  gfx::Rect rect(0, 0, mSize.width, mSize.height);

  MakeTextureIfNeeded(mCompositorOGL->gl(), mTextureHandle);

  // TODO: Call AttachSharedHandle from CompositorOGL, not here.
  mCompositorOGL->gl()->fBindTexture(handleDetails.mTarget, mTextureHandle);
  if (!mCompositorOGL->gl()->AttachSharedHandle(mShareType, mSharedHandle)) {
    NS_ERROR("Failed to bind shared texture handle");
    return;
  }

  mCompositorOGL->DrawQuad(rect, nullptr, nullptr, aEffectChain,
                           aOpacity, aTransform, aOffset);

  // TODO:: Call this from CompositorOGL, not here.
  mCompositorOGL->gl()->DetachSharedHandle(mShareType, mSharedHandle);
}

void
ImageSourceOGLShared::UpdateImage(const SharedImage& aImage)
{
  SurfaceDescriptor surface = aImage.get_SurfaceDescriptor();
  SharedTextureDescriptor texture = surface.get_SharedTextureDescriptor();

  SharedTextureHandle newHandle = texture.handle();
  mSize = texture.size();
  mInverted = texture.inverted();

  if (newHandle != mSharedHandle) {
    mCompositorOGL->gl()->ReleaseSharedHandle(mShareType, mSharedHandle);
    mSharedHandle = newHandle;
  }

  mShareType = texture.shareType();
}

static PRUint32
DataOffset(PRUint32 aStride, PRUint32 aPixelSize, const nsIntPoint &aPoint)
{
  unsigned int data = aPoint.y * aStride;
  data += aPoint.x * aPixelSize;
  return data;
}

void
TextureOGL::UpdateTexture(const nsIntRegion& aRegion, PRInt8 *aData, PRUint32 aStride)
{
  mCompositorOGL->gl()->fBindTexture(LOCAL_GL_TEXTURE_2D, mTextureHandle);
  if (!mCompositorOGL->SupportsPartialTextureUpdate() ||
      (aRegion.IsEqual(nsIntRect(0, 0, mSize.width, mSize.height)))) {
    mCompositorOGL->gl()->TexImage2D(LOCAL_GL_TEXTURE_2D, 0, mInternalFormat,
                                     mSize.width, mSize.height, aStride, mPixelSize,
                                     0, mFormat, mType, aData);
  } else {
    nsIntRegionRectIterator iter(aRegion);
    const nsIntRect *iterRect;

    nsIntPoint topLeft = aRegion.GetBounds().TopLeft();

    while ((iterRect = iter.Next())) {
      // The inital data pointer is at the top left point of the region's
      // bounding rectangle. We need to find the offset of this rect
      // within the region and adjust the data pointer accordingly.
      PRInt8 *rectData = aData + DataOffset(aStride, mPixelSize, iterRect->TopLeft() - topLeft);
      mCompositorOGL->gl()->TexSubImage2D(LOCAL_GL_TEXTURE_2D,
                                          0,
                                          iterRect->x,
                                          iterRect->y,
                                          iterRect->width,
                                          iterRect->height,
                                          aStride,
                                          mPixelSize,
                                          mFormat,
                                          mType,
                                          rectData);
    }
  }
}

} /* layers */
} /* mozilla */
