/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextureClient.h"
#include "ImageClient.h"
#include "CanvasClient.h"
#include "ThebesBufferClient.h"
#include "mozilla/layers/ShadowLayers.h"
#include "SharedTextureImage.h"

namespace mozilla {
namespace layers {



TextureClientShmem::TextureClientShmem(ShadowLayerForwarder* aLayerForwarder, ImageHostType aImageType)
  : TextureClient(aLayerForwarder, aImageType)
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
                                             ImageHostType aImageType)
  : TextureClientShared(aLayerForwarder, aImageType)
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

/* static */ ImageHostType
CompositingFactory::TypeForImage(Image* aImage) {
  if (!aImage) {
    return IMAGE_UNKNOWN;
  }

  if (aImage->GetFormat() == Image::SHARED_TEXTURE) {
    return IMAGE_SHARED;
  }
  if (aImage->GetFormat() == Image::PLANAR_YCBCR) {
    return IMAGE_YUV;
  }

  return IMAGE_TEXTURE;
}

/* static */ TemporaryRef<ImageClient>
CompositingFactory::CreateImageClient(LayersBackend aParentBackend,
                                      ImageHostType aImageHostType,
                                      ShadowLayerForwarder* aLayerForwarder,
                                      ShadowableLayer* aLayer,
                                      TextureFlags aFlags)
{
  RefPtr<ImageClient> result = nullptr;
  switch (aImageHostType) {
  case IMAGE_SHARED:
    if (aParentBackend == LAYERS_OPENGL) {
      result = new ImageClientShared(aLayerForwarder, aLayer, aFlags);
    }
    break;
  case IMAGE_YUV:
    if (aLayer->AsLayer()->Manager()->IsCompositingCheap()) {
      result = new ImageClientYUV(aLayerForwarder, aLayer, aFlags);
      break;
    }
    // fall through to IMAGE_TEXTURE
  case IMAGE_TEXTURE:
    if (aParentBackend == LAYERS_OPENGL) {
      result = new ImageClientTexture(aLayerForwarder, aLayer, aFlags);
    }
    break;
  case IMAGE_UNKNOWN:
    return result.forget();    
  }

  NS_ASSERTION(result, "Failed to create ImageClient");

  return result.forget();
}

/* static */ TemporaryRef<CanvasClient>
CompositingFactory::CreateCanvasClient(LayersBackend aParentBackend,
                                       ImageHostType aImageHostType,
                                       ShadowLayerForwarder* aLayerForwarder,
                                       ShadowableLayer* aLayer,
                                       TextureFlags aFlags)
{
  if (aImageHostType == IMAGE_TEXTURE) {
    return new CanvasClientTexture(aLayerForwarder, aLayer, aFlags);
  }
  if (aImageHostType == IMAGE_SHARED) {
    if (aParentBackend == LAYERS_OPENGL) {
      return new CanvasClientShared(aLayerForwarder, aLayer, aFlags);
    }
    return new CanvasClientTexture(aLayerForwarder, aLayer, aFlags);
  }
  return nullptr;
}

/* static */ TemporaryRef<ContentClient>
CompositingFactory::CreateContentClient(LayersBackend aParentBackend,
                                        ImageHostType aImageHostType,
                                        ShadowLayerForwarder* aLayerForwarder,
                                        ShadowableLayer* aLayer,
                                        TextureFlags aFlags)
{
  if (aParentBackend != LAYERS_OPENGL) {
    return nullptr;
  }
  if (aImageHostType == IMAGE_THEBES) {
    return new ContentClientTexture(aLayerForwarder, aLayer, aFlags);
  }
  if (aImageHostType == IMAGE_DIRECT) {
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
                                        ImageHostType aImageHostType,
                                        ShadowLayerForwarder* aLayerForwarder,
                                        bool aStrict /* = false */)
{
  RefPtr<TextureClient> result = nullptr;
  switch (aTextureHostType) {
  case TEXTURE_SHARED_GL:
    if (aParentBackend == LAYERS_OPENGL) {
      result = new TextureClientSharedGL(aLayerForwarder, aImageHostType);
    }
    break;
  case TEXTURE_SHARED:
    if (aParentBackend == LAYERS_OPENGL) {
      result = new TextureClientShared(aLayerForwarder, aImageHostType);
    }
    break;
  case TEXTURE_SHMEM:
    if (aParentBackend == LAYERS_OPENGL) {
      result = new TextureClientShmem(aLayerForwarder, aImageHostType);
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