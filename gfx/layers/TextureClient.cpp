/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextureClient.h"
#include "ImageClient.h"
#include "CanvasClient.h"
#include "ContentClient.h"
#include "mozilla/layers/ShadowLayers.h"
#include "SharedTextureImage.h"

namespace mozilla {
namespace layers {


void
TextureClient::Updated(ShadowableLayer* aLayer)
{
  mLayerForwarder->UpdateTexture(aLayer, mIdentifier, SharedImage(mDescriptor));
}

void
TextureClient::UpdatedRegion(ShadowableLayer* aLayer,
                             const nsIntRegion& aUpdatedRegion,
                             const nsIntRect& aBufferRect,
                             const nsIntPoint& aBufferRotation)
{
  mLayerForwarder->UpdateTextureRegion(aLayer,
                                       mIdentifier,
                                       ThebesBuffer(mDescriptor, aBufferRect, aBufferRotation),
                                       aUpdatedRegion);

}


TextureClientShmem::TextureClientShmem(ShadowLayerForwarder* aLayerForwarder, BufferType aBufferType)
  : TextureClient(aLayerForwarder, aBufferType)
  , mSurface(nullptr)
  , mSurfaceAsImage(nullptr)
{
  mIdentifier.mTextureType = TEXTURE_SHMEM;
  mIdentifier.mDescriptor = 0;
}

TextureClientShmem::~TextureClientShmem()
{
  //TODO[nrc] do I need to tell the host I've died?
  //yes
  //mLayerForwarder->DestroyedThebesBuffer(mLayerForwarder->Hold(this),
  //                                mBackBuffer);
  if (mSurface) {
    mSurface = nullptr;
    ShadowLayerForwarder::CloseDescriptor(mDescriptor);
  }
  if (IsSurfaceDescriptorValid(mDescriptor)) {
    mLayerForwarder->DestroySharedSurface(&mDescriptor);
  }
}

void
TextureClientShmem::EnsureTextureClient(gfx::IntSize aSize, gfxASurface::gfxContentType aContentType)
{
  if (aSize != mSize ||
      aContentType != mContentType ||
      !IsSurfaceDescriptorValid(mDescriptor)) {
    if (IsSurfaceDescriptorValid(mDescriptor)) {
      mLayerForwarder->DestroySharedSurface(&mDescriptor);
    }
    mContentType = aContentType;
    mSize = aSize;

    if (!mLayerForwarder->AllocBuffer(gfxIntSize(mSize.width, mSize.height), mContentType, &mDescriptor)) {
      NS_RUNTIMEABORT("creating SurfaceDescriptor failed!");
    }
  }
}

already_AddRefed<gfxContext>
TextureClientShmem::LockContext()
{
  nsRefPtr<gfxContext> result = new gfxContext(GetSurface());
  return result.forget();
}

gfxASurface*
TextureClientShmem::GetSurface()
{
  if (!mSurface) {
    mSurface = ShadowLayerForwarder::OpenDescriptor(OPEN_READ_WRITE, mDescriptor);
  }
  
  return mSurface.get();
}

void
TextureClientShmem::Unlock()
{
  mSurface = nullptr;
  mSurfaceAsImage = nullptr;

  ShadowLayerForwarder::CloseDescriptor(mDescriptor);
}

gfxImageSurface*
TextureClientShmem::LockImageSurface()
{
  if (!mSurfaceAsImage) {
    mSurfaceAsImage = GetSurface()->GetAsImageSurface();
  }

  return mSurfaceAsImage.get();
}


TextureClientShared::~TextureClientShared()
{
  if (IsSurfaceDescriptorValid(mDescriptor)) {
    mLayerForwarder->DestroySharedSurface(&mDescriptor);
  }
}

TextureClientSharedGL::TextureClientSharedGL(ShadowLayerForwarder* aLayerForwarder,
                                             BufferType aBufferType)
  : TextureClientShared(aLayerForwarder, aBufferType)
{
  mIdentifier.mTextureType = TEXTURE_SHARED_GL;
}

TextureClientSharedGL::~TextureClientSharedGL()
{
  SharedTextureDescriptor handle = mDescriptor.get_SharedTextureDescriptor();
  if (mGL && handle.handle()) {
    mGL->ReleaseSharedHandle(handle.shareType(), handle.handle());
  }
}


void
TextureClientSharedGL::EnsureTextureClient(gfx::IntSize aSize, gfxASurface::gfxContentType aContentType)
{
  mSize = aSize;
}

SharedTextureHandle
TextureClientSharedGL::LockHandle(GLContext* aGL, TextureImage::TextureShareType aFlags)
{
  mGL = aGL;

  SharedTextureHandle handle = 0;
  if (mDescriptor.type() == SurfaceDescriptor::TSharedTextureDescriptor) {
    handle = mDescriptor.get_SharedTextureDescriptor().handle();
  } else {
    handle = mGL->CreateSharedHandle(aFlags);
    if (!handle) {
      return 0;
    }
    mDescriptor = SharedTextureDescriptor(aFlags, handle, nsIntSize(mSize.width, mSize.height), false);
  }

  return handle;
}

void
TextureClientSharedGL::Unlock()
{
  // Move SharedTextureHandle ownership to ShadowLayer
  mDescriptor = SurfaceDescriptor();
}

/* static */ BufferType
CompositingFactory::TypeForImage(Image* aImage) {
  if (!aImage) {
    return BUFFER_UNKNOWN;
  }

  if (aImage->GetFormat() == Image::SHARED_TEXTURE) {
    return BUFFER_SHARED;
  }
  if (aImage->GetFormat() == Image::PLANAR_YCBCR) {
    return BUFFER_YUV;
  }

  return BUFFER_TEXTURE;
}

/* static */ TemporaryRef<ImageClient>
CompositingFactory::CreateImageClient(LayersBackend aParentBackend,
                                      BufferType aBufferHostType,
                                      ShadowLayerForwarder* aLayerForwarder,
                                      ShadowableLayer* aLayer,
                                      TextureFlags aFlags)
{
  RefPtr<ImageClient> result = nullptr;
  switch (aBufferHostType) {
  case BUFFER_SHARED:
    if (aParentBackend == LAYERS_OPENGL) {
      result = new ImageClientShared(aLayerForwarder, aLayer, aFlags);
    }
    break;
  case BUFFER_YUV:
    if (aLayer->AsLayer()->Manager()->IsCompositingCheap()) {
      result = new ImageClientYUV(aLayerForwarder, aLayer, aFlags);
      break;
    }
    // fall through to BUFFER_TEXTURE
  case BUFFER_TEXTURE:
    if (aParentBackend == LAYERS_OPENGL) {
      result = new ImageClientTexture(aLayerForwarder, aLayer, aFlags);
    }
    break;
  case BUFFER_UNKNOWN:
    return nullptr;    
  }

  NS_ASSERTION(result, "Failed to create ImageClient");

  return result.forget();
}

/* static */ TemporaryRef<CanvasClient>
CompositingFactory::CreateCanvasClient(LayersBackend aParentBackend,
                                       BufferType aBufferHostType,
                                       ShadowLayerForwarder* aLayerForwarder,
                                       ShadowableLayer* aLayer,
                                       TextureFlags aFlags)
{
  if (aBufferHostType == BUFFER_TEXTURE) {
    return new CanvasClientTexture(aLayerForwarder, aLayer, aFlags);
  }
  if (aBufferHostType == BUFFER_SHARED) {
    if (aParentBackend == LAYERS_OPENGL) {
      return new CanvasClientShared(aLayerForwarder, aLayer, aFlags);
    }
    return new CanvasClientTexture(aLayerForwarder, aLayer, aFlags);
  }
  return nullptr;
}

/* static */ TemporaryRef<ContentClient>
CompositingFactory::CreateContentClient(LayersBackend aParentBackend,
                                        BufferType aBufferHostType,
                                        ShadowLayerForwarder* aLayerForwarder,
                                        ShadowableLayer* aLayer,
                                        TextureFlags aFlags)
{
  if (aParentBackend != LAYERS_OPENGL) {
    return nullptr;
  }
  if (aBufferHostType == BUFFER_THEBES) {
    return new ContentClientTexture(aLayerForwarder, aLayer, aFlags);
  }
  if (aBufferHostType == BUFFER_DIRECT) {
    if (ShadowLayerManager::SupportsDirectTexturing()) {
      return new ContentClientDirect(aLayerForwarder, aLayer, aFlags);
    }
    return new ContentClientTexture(aLayerForwarder, aLayer, aFlags);
  }
  return nullptr;
}

/* static */ TemporaryRef<TextureClient>
CompositingFactory::CreateTextureClient(LayersBackend aParentBackend,
                                        TextureHostType aTextureHostType,
                                        BufferType aBufferHostType,
                                        ShadowLayerForwarder* aLayerForwarder,
                                        bool aStrict /* = false */)
{
  RefPtr<TextureClient> result = nullptr;
  switch (aTextureHostType) {
  case TEXTURE_SHARED_GL:
    if (aParentBackend == LAYERS_OPENGL) {
      result = new TextureClientSharedGL(aLayerForwarder, aBufferHostType);
    }
    break;
  case TEXTURE_SHARED:
    if (aParentBackend == LAYERS_OPENGL) {
      result = new TextureClientShared(aLayerForwarder, aBufferHostType);
    }
    break;
  case TEXTURE_SHMEM:
    if (aParentBackend == LAYERS_OPENGL) {
      result = new TextureClientShmem(aLayerForwarder, aBufferHostType);
    }
    break;
  default:
    return result.forget();
  }

  NS_ASSERTION(result, "Failed to create ImageClient");

  return result.forget();
}

}
}