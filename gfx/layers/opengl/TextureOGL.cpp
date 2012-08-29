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

  gl->fActiveTexture(LOCAL_GL_TEXTURE0);
  gl->fBindTexture(LOCAL_GL_TEXTURE_2D, aTexture);

  gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MIN_FILTER, LOCAL_GL_LINEAR);
  gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MAG_FILTER, LOCAL_GL_LINEAR);
  gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_S, LOCAL_GL_CLAMP_TO_EDGE);
  gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_T, LOCAL_GL_CLAMP_TO_EDGE);
}

static TextureImage::Flags FlagsToGLFlags(TextureFlags aFlags)
{
  uint32_t result = TextureImage::NoFlags;
  
  if (aFlags & UseNearestFilter)
    result |= TextureImage::UseNearestFilter;
  if (aFlags & NeedsYFlip)
    result |= TextureImage::NeedsYFlip;
  if (aFlags & ForceSingleTile)
    result |= TextureImage::ForceSingleTile;

  return static_cast<TextureImage::Flags>(result);
}

GLenum
WrapMode(GLContext *aGl, bool aAllowRepeat)
{
  if (aAllowRepeat &&
      (aGl->IsExtensionSupported(GLContext::ARB_texture_non_power_of_two) ||
       aGl->IsExtensionSupported(GLContext::OES_texture_npot))) {
    return LOCAL_GL_REPEAT;
  }
  return LOCAL_GL_CLAMP_TO_EDGE;
}


const SharedImage*
TextureImageAsTextureHost::Update(const SharedImage& aImage)
{
  SurfaceDescriptor surface = aImage.get_SurfaceDescriptor();

  AutoOpenSurface surf(OPEN_READ_ONLY, surface);
  nsIntSize size = surf.Size();

  if (!mTexImage ||
      mTexImage->mSize != size ||
      mTexImage->GetContentType() != surf.ContentType()) {
    mTexImage = mGL->CreateTextureImage(size,
                                        surf.ContentType(),
                                        WrapMode(mGL, false), //TODO[nrc] probably need to find where AllowRepeat comes from - Image no, Thebes yes
                                        FlagsToGLFlags(mFlags));
  }

  // XXX this is always just ridiculously slow
  nsIntRegion updateRegion(nsIntRect(0, 0, size.width, size.height));
  mTexImage->DirectUpdate(surf.Get(), updateRegion);

  return &aImage;
}

void
TextureImageAsTextureHost::Update(gfxASurface* aSurface, nsIntRegion& aRegion)
{
  if (!mTexImage ||
      mTexImage->mSize != aSurface->GetSize() ||
      mTexImage->GetContentType() != aSurface->GetContentType()) {
    mTexImage = mGL->CreateTextureImage(aSurface->GetSize(),
                                        aSurface->GetContentType(),
                                        WrapMode(mGL, true), //TODO[nrc] AllowRepeat
                                        FlagsToGLFlags(mFlags));
  }

  mTexImage->DirectUpdate(aSurface, aRegion);
}

bool
TextureImageAsTextureHost::UpdateDirect(const SurfaceDescriptor& aBuffer)
{
  nsRefPtr<TextureImage> texImage =
      ShadowLayerManager::OpenDescriptorForDirectTexturing(
        mGL, aBuffer, WrapMode(mGL, true)); //TODO[nrc] AllowRepeat

  if (!texImage) {
    NS_WARNING("Could not create texture for direct texturing");
    return false;
  }

  mTexImage = texImage;
  return true;
}

Effect*
TextureImageAsTextureHost::Lock(const gfx::Filter& aFilter)
{
  NS_ASSERTION(mTexImage->GetContentType() != gfxASurface::CONTENT_ALPHA,
               "Image layer has alpha image");

  if (mTexImage->GetShaderProgramType() == gl::BGRXLayerProgramType) {
    return new EffectBGRX(this, true, aFilter, mFlags & NeedsYFlip);
  } else if (mTexImage->GetShaderProgramType() == gl::BGRALayerProgramType) {
    return new EffectBGRA(this, true, aFilter, mFlags & NeedsYFlip);
  } else {
    NS_RUNTIMEABORT("Shader type not yet supported");
    return nullptr;
  }
}


ImageHostTexture::ImageHostTexture(Compositor* aCompositor)
  : mTextureHost(nullptr)
  , mCompositor(aCompositor)
{
}

const SharedImage*
ImageHostTexture::UpdateImage(const TextureIdentifier& aTextureIdentifier,
                              const SharedImage& aImage)
{
  return mTextureHost->Update(aImage);
}

void
ImageHostTexture::Composite(EffectChain& aEffectChain,
                            float aOpacity,
                            const gfx::Matrix4x4& aTransform,
                            const gfx::Point& aOffset,
                            const gfx::Filter& aFilter,
                            const gfx::Rect& aClipRect,
                            const nsIntRegion* aVisibleRegion)
{
  //TODO[nrc] surely we don't need to set the filter twice?
  if (Effect* effect = mTextureHost->Lock(aFilter)) {
    aEffectChain.mEffects[effect->mType] = effect;
  } else {
    return;
  }
  mTextureHost->SetFilter(aFilter);
  mTextureHost->BeginTileIteration();

  do {
    nsIntRect tileRect = mTextureHost->GetTileRect();
    mTextureHost->SetSize(gfx::IntSize(tileRect.width, tileRect.height));
    gfx::Rect rect(tileRect.x, tileRect.y, tileRect.width, tileRect.height);
    gfx::Rect sourceRect(0, 0, tileRect.width, tileRect.height);
    mCompositor->DrawQuad(rect, &sourceRect, &aClipRect, aEffectChain,
                          aOpacity, aTransform, aOffset);
  } while (mTextureHost->NextTile());

  mTextureHost->Unlock();
}

void
ImageHostTexture::AddTextureHost(const TextureIdentifier& aTextureIdentifier, TextureHost* aTextureHost)
{
  NS_ASSERTION(aTextureIdentifier.mImageType == IMAGE_SHMEM &&
               aTextureIdentifier.mTextureType == IMAGE_SHMEM,
               "ImageHostType mismatch.");
  mTextureHost = static_cast<TextureImageAsTextureHost*>(aTextureHost);
}


const SharedImage*
TextureHostOGLShared::Update(const SharedImage& aImage)
{
  NS_ASSERTION(aImage.type() == SurfaceDescriptor::TSharedTextureDescriptor,
               "Invalid descriptor");

  SurfaceDescriptor surface = aImage.get_SurfaceDescriptor();
  SharedTextureDescriptor texture = surface.get_SharedTextureDescriptor();

  SharedTextureHandle newHandle = texture.handle();
  nsIntSize size = texture.size();
  mSize = gfx::IntSize(size.width, size.height);
  mInverted = texture.inverted();
  mShareType = texture.shareType();

  if (mSharedHandle &&
      newHandle != mSharedHandle) {
    mGL->ReleaseSharedHandle(mShareType, mSharedHandle);
  }
  mSharedHandle = newHandle;

  return &aImage;
}

Effect*
TextureHostOGLShared::Lock(const gfx::Filter& aFilter)
{
  GLContext::SharedHandleDetails handleDetails;
  if (!mGL->GetSharedHandleDetails(mShareType, mSharedHandle, handleDetails)) {
    NS_ERROR("Failed to get shared handle details");
    return nullptr;
  }

  //TODO[nrc] can we do this in update instead of here?
  MakeTextureIfNeeded(mGL, mTextureHandle);

  mGL->fBindTexture(handleDetails.mTarget, mTextureHandle);
  if (!mGL->AttachSharedHandle(mShareType, mSharedHandle)) {
    NS_ERROR("Failed to bind shared texture handle");
    return nullptr;
  }

  if (handleDetails.mProgramType == gl::RGBALayerProgramType) {
    return new EffectRGBA(this, true, aFilter, mInverted);
  } else if (handleDetails.mProgramType == gl::RGBALayerExternalProgramType) {
    gfx::Matrix4x4 textureTransform;
    LayerManagerOGL::ToMatrix4x4(handleDetails.mTextureTransform, textureTransform);
    return new EffectRGBAExternal(this, textureTransform, true, aFilter, mInverted);
  } else {
    NS_RUNTIMEABORT("Shader type not yet supported");
    return nullptr;
  }
}

void
TextureHostOGLShared::Unlock()
{
  mGL->DetachSharedHandle(mShareType, mSharedHandle);
  mGL->fBindTexture(LOCAL_GL_TEXTURE_2D, 0);
}

//TODO[nrc] we can surely share more code with TextureHostOGLShared
const SharedImage*
TextureHostOGLSharedWithBuffer::Update(const SharedImage& aImage)
{
  NS_ASSERTION(aImage.type() == SurfaceDescriptor::TSharedTextureDescriptor,
               "Invalid descriptor");

  //TODO[nrc] do I even need this?
  //if (mBuffer.type() != SurfaceDescriptor::TSharedTextureDescriptor) {
  //  mBuffer = SharedTextureDescriptor(TextureImage::ThreadShared, 0, nsIntSize(0, 0), false);
  //}

  SurfaceDescriptor surface = aImage.get_SurfaceDescriptor();
  SharedTextureDescriptor texture = surface.get_SharedTextureDescriptor();

  SharedTextureHandle newHandle = texture.handle();
  nsIntSize size = texture.size();
  mSize = gfx::IntSize(size.width, size.height);
  if (texture.inverted()) {
    mFlags |= NeedsYFlip;
  }
  mShareType = texture.shareType();

  if (mSharedHandle &&
      newHandle != mSharedHandle) {
    mGL->ReleaseSharedHandle(mShareType, mSharedHandle);
  }
  mSharedHandle = newHandle;

  mBuffer = SharedImage(surface);
  return &mBuffer;
}

Effect*
TextureHostOGLSharedWithBuffer::Lock(const gfx::Filter& aFilter)
{
  GLContext::SharedHandleDetails handleDetails;
  if (!mGL->GetSharedHandleDetails(mShareType, mSharedHandle, handleDetails)) {
    NS_ERROR("Failed to get shared handle details");
    return nullptr;
  }

  //TODO[nrc] can we do this in update instead of here?
  MakeTextureIfNeeded(mGL, mTextureHandle);

  mGL->fActiveTexture(LOCAL_GL_TEXTURE0);
  mGL->fBindTexture(handleDetails.mTarget, mTextureHandle);
  if (!mGL->AttachSharedHandle(mShareType, mSharedHandle)) {
    NS_ERROR("Failed to bind shared texture handle");
    return nullptr;
  }

  if (mFlags & UseOpaqueSurface) {
    return new EffectRGBX(this, true, aFilter, mFlags & NeedsYFlip);
  } else if (handleDetails.mProgramType == gl::RGBALayerProgramType) {
    return new EffectRGBA(this, true, aFilter, mFlags & NeedsYFlip);
  } else if (handleDetails.mProgramType == gl::RGBALayerExternalProgramType) {
    gfx::Matrix4x4 textureTransform;
    LayerManagerOGL::ToMatrix4x4(handleDetails.mTextureTransform, textureTransform);
    return new EffectRGBAExternal(this, textureTransform, true, aFilter, mFlags & NeedsYFlip);
  } else {
    NS_RUNTIMEABORT("Shader type not yet supported");
    return nullptr;
  }
}

void
TextureHostOGLSharedWithBuffer::Unlock()
{
  mGL->DetachSharedHandle(mShareType, mSharedHandle);
  mGL->fBindTexture(LOCAL_GL_TEXTURE_2D, 0);
}


ImageHostShared::ImageHostShared(Compositor* aCompositor)
  : mTextureHost(nullptr)
  , mCompositor(aCompositor)
{
}

const SharedImage*
ImageHostShared::UpdateImage(const TextureIdentifier& aTextureIdentifier,
                             const SharedImage& aImage)
{
  return mTextureHost->Update(aImage);
}

void
ImageHostShared::Composite(EffectChain& aEffectChain,
                           float aOpacity,
                           const gfx::Matrix4x4& aTransform,
                           const gfx::Point& aOffset,
                           const gfx::Filter& aFilter,
                           const gfx::Rect& aClipRect,
                           const nsIntRegion* aVisibleRegion)
{
  if (Effect* effect = mTextureHost->Lock(aFilter)) {
    aEffectChain.mEffects[effect->mType] = effect;
  } else {
    return;
  }

  gfx::Rect rect(0, 0, mTextureHost->GetSize().width, mTextureHost->GetSize().height);
  mCompositor->DrawQuad(rect, nullptr, &aClipRect, aEffectChain,
                        aOpacity, aTransform, aOffset);

  mTextureHost->Unlock();
}

void
ImageHostShared::AddTextureHost(const TextureIdentifier& aTextureIdentifier, TextureHost* aTextureHost)
{
  NS_ASSERTION(aTextureIdentifier.mImageType == IMAGE_SHARED &&
               aTextureIdentifier.mTextureType == IMAGE_SHARED,
               "ImageHostType mismatch.");
  mTextureHost = static_cast<TextureHostOGLShared*>(aTextureHost);
}

static PRUint32
DataOffset(PRUint32 aStride, PRUint32 aPixelSize, const nsIntPoint &aPoint)
{
  unsigned int data = aPoint.y * aStride;
  data += aPoint.x * aPixelSize;
  return data;
}

void
TextureOGL::UpdateTexture(PRInt8 *aData, PRUint32 aStride)
{
  mGL->fBindTexture(LOCAL_GL_TEXTURE_2D, mTextureHandle);
  mGL->TexImage2D(LOCAL_GL_TEXTURE_2D, 0, mInternalFormat,
                  mSize.width, mSize.height, aStride, mPixelSize,
                  0, mFormat, mType, aData);
}

void
TextureOGL::UpdateTexture(const nsIntRegion& aRegion, PRInt8 *aData, PRUint32 aStride)
{
  mGL->fBindTexture(LOCAL_GL_TEXTURE_2D, mTextureHandle);
  if (!mGL->CanUploadSubTextures() ||
      (aRegion.IsEqual(nsIntRect(0, 0, mSize.width, mSize.height)))) {
    mGL->TexImage2D(LOCAL_GL_TEXTURE_2D, 0, mInternalFormat,
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
      mGL->TexSubImage2D(LOCAL_GL_TEXTURE_2D,
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

const SharedImage*
GLTextureAsTextureHost::Update(const SharedImage& aImage)
{
  AutoOpenSurface surf(OPEN_READ_ONLY, aImage.get_SurfaceDescriptor());
    
  mSize = gfx::IntSize(surf.Size().width, surf.Size().height);

  if (!mTexture.IsAllocated()) {
    mTexture.Allocate(mGL);

    NS_ASSERTION(mTexture.IsAllocated(),
                  "Texture allocation failed!");

    mGL->MakeCurrent();
    mGL->fBindTexture(LOCAL_GL_TEXTURE_2D, mTexture.GetTextureID());
    mGL->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_S, LOCAL_GL_CLAMP_TO_EDGE);
    mGL->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_T, LOCAL_GL_CLAMP_TO_EDGE);
  }

  //TODO[nrc] I don't see why we need a new image surface here, but should check
  /*nsRefPtr<gfxASurface> surf = new gfxImageSurface(aData.mYChannel,
                                                    mSize,
                                                    aData.mYStride,
                                                    gfxASurface::ImageFormatA8);*/
  GLuint textureId = mTexture.GetTextureID();
  mGL->UploadSurfaceToTexture(surf.GetAsImage(),
                              nsIntRect(0, 0, mSize.width, mSize.height),
                              textureId,
                              true);
  NS_ASSERTION(textureId == mTexture.GetTextureID(), "texture handle id changed");

  return &aImage;
}

} /* layers */
} /* mozilla */
