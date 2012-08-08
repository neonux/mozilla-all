/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextureOGL.h"

namespace mozilla {
namespace layers {

void
ImageTextureOGL::ImageTextureOGL(const SurfaceDescriptor& aSurface, bool aForceSingleTile)
: mForceSingleTile(aForceSingleTile)
{
  AutoOpenSurface autoSurf(OPEN_READ_ONLY, aSurface);
  mSize = autoSurf.Size();
  mTexImage = gl()->CreateTextureImage(nsIntSize(mSize.width, mSize.height),
                                        autoSurf.ContentType(),
                                        LOCAL_GL_CLAMP_TO_EDGE,
                                        mForceSingleTile
                                        ? TextureImage::ForceSingleTile
                                        : TextureImage::NoFlags);
}

void
ImageTextureOGL::Composite(Compositor* aCompositor,
                           EffectChain& aEffectChain,
                           float aOpacity,
                           const gfx::Matrix4x4* aTransform,
                           const gfx::Point& aOffset,
                           const gfx::Filter aFilter)
{
  NS_ASSERTION(mTexImage->GetContentType() != gfxASurface::CONTENT_ALPHA,
               "Image layer has alpha image");

  mWrapMode = mTexImage->GetWrapMode();

  if (mTexImage->GetShaderProgramType() == gl::BGRXLayerProgramType) {
    effect = new EffectBGRX(this, true, aFilter);
    aEffectChain.mEffects[EFFECT_BGRX] = effect;
  } else if (mTexImage->GetShaderProgramType() == gl::BGRALayerProgramType) {
    effect = new EffectBGRA(this, true, aFilter);
    aEffectChain.mEffects[EFFECT_BGRA] = effect;
  } else {
    NS_RUNTIMEABORT("Shader type not yet supported");
  }

  mTexImage->SetFilter(gfx::ThebesFilter(mFilter));
  mTexImage->BeginTileIteration();

  do {
    mSize = gfx::IntSize(mTexImage->GetTileRect().width,
                         mTexImage->GetTileRect().height);
    gfx::Rect rect(mTexImage->GetTileRect().x, mTexImage->GetTileRect().y,
                   mTexImage->GetTileRect().width, mTexImage->GetTileRect().height);
    gfx::Rect sourceRect(0, 0, mTexImage->GetTileRect().width,
                         mTexImage->GetTileRect().height);
    aCompositor->DrawQuad(rect, &sourceRect, nullptr, aEffectChain,
                          aOpacity, aTransform, aOffset);
  } while (mTexImage->NextTile());
}

void
ImageTextureOGL::UpdateImage(const SharedImage& aImage)
{
  SurfaceDescriptor surface = aNewFront.get_SurfaceDescriptor();

  AutoOpenSurface surf(OPEN_READ_ONLY, surface);
  gfxIntSize size = surf.Size();

  NS_ASSERTION(mTexImage, "mTextImage should never be null");
  if (mSize != size ||
      mTexImage->GetContentType() != surf.ContentType()) {
    mSize = size;
    mTexImage = gl()->CreateTextureImage(nsIntSize(mSize.width, mSize.height),
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

ImageTextureOGLShared::ImageTextureOGLShared(CompositorOGL* aCompositorOGL, const SharedTextureDescriptor& aTexture)
  : mCompositorOGL(aCompositorOGL)
  , mSize(aTexture.size())
  , mSharedHandle(aTexture.handle())
  , mShareType(aTexture.shareType())
  , mInverted(aTexture.inverted())
{
}

void
ImageTextureOGLShared::Composite(Compositor* aCompositor,
                                 EffectChain& aEffectChain,
                                 float aOpacity,
                                 const gfx::Matrix4x4* aTransform,
                                 const gfx::Point& aOffset,
                                 const gfx::Filter aFilter)
{
  GLContext::SharedHandleDetails handleDetails;
  if (!gl()->GetSharedHandleDetails(mShareType, mSharedHandle, handleDetails)) {
    NS_ERROR("Failed to get shared handle details");
    return;
  }

  mSize = gfx::IntSize(mSize.width, mSize.height);
  if (handleDetails.mProgramType == gl::RGBALayerProgramType) {
    effect = new EffectRGBA(this, true, aFilter, mInverted);
    aEffectChain.mEffects[EFFECT_RGBA] = effect;
  } else if (handleDetails.mProgramType == gl::RGBALayerExternalProgramType) {
    gfx::Matrix4x4 textureTransform;
    LayerManagerOGL::ToMatrix4x4(handleDetails.mTextureTransform, textureTransform);
    effect = new EffectRGBAExternal(this, textureTransform, true, aFilter, mInverted);
    aEffectChain.mEffects[EFFECT_RGBA_EXTERNAL] = effect;
  } else {
    NS_RUNTIMEABORT("Shader type not yet supported");
  }

  gfx::Rect rect(0, 0, mSize.width, mSize.height);

  MakeTextureIfNeeded(gl(), mTextureHandle);

  // TODO: Call AttachSharedHandle from CompositorOGL, not here.
  aCompositor->gl()->fBindTexture(handleDetails.mTarget, mTextureHandle);
  if (!aCompositor->gl()->AttachSharedHandle(mShareType, mSharedHandle)) {
    NS_ERROR("Failed to bind shared texture handle");
    return;
  }

  aCompositor->DrawQuad(rect, nullptr, nullptr, aEffectChain,
                        aOpacity, aTransform, aOffset);

  // TODO:: Call this from CompositorOGL, not here.
  aCompositor->gl()->DetachSharedHandle(mShareType, mSharedHandle);
}

void
ImageTextureOGLShared::UpdateImage(const SharedImage& aImage)
{
  SurfaceDescriptor surface = aNewFront.get_SurfaceDescriptor();
  SharedTextureDescriptor texture = surface.get_SharedTextureDescriptor();

  SharedTextureHandle newHandle = texture.handle();
  mSize = texture.size();
  mInverted = texture.inverted();

  if (newHandle != mSharedHandle) {
    mCompositor->gl()->ReleaseSharedHandle(mShareType, mSharedHandle);
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
