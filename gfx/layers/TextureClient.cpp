/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextureClient.h"
#include "ImageClient.h"
#include "CanvasClient.h"
#include "mozilla/layers/ShadowLayers.h"
#include "SharedTextureImage.h"

namespace mozilla {
namespace layers {



TextureClientShmem::TextureClientShmem(ShadowLayerForwarder* aLayerForwarder, ImageHostType aImageType)
  : TextureClient(aLayerForwarder, aImageType)
  , mSurface(nullptr)
  , mSurfaceAsImage(nullptr)
{
  mIdentifier.mTextureType = IMAGE_SHMEM;
  mIdentifier.mDescriptor = 0;
}

TextureClientShmem::~TextureClientShmem()
{
  //TODO[nrc] do I need to tell the host I've died?
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

    if (!mLayerForwarder->AllocBuffer(gfxIntSize(mSize.width, mSize.height), mContentType, &mDescriptor))
      NS_RUNTIMEABORT("creating SurfaceDescriptor failed!");
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
CompositingFactory::CreateImageClient(const TextureHostType &aHostType,
                                      const ImageHostType& aImageHostType,
                                      ShadowLayerForwarder* aLayerForwarder,
                                      ShadowableLayer* aLayer,
                                      TextureFlags aFlags)
{
  //TODO[nrc] remove this
  if (aFlags & CanvasFlag) {
    if (aImageHostType == IMAGE_SHMEM) {
      return new CanvasClientTexture(aLayerForwarder, aLayer, aFlags);
    } else if (aImageHostType == IMAGE_SHARED_WITH_BUFFER) {
      if (aHostType == HOST_GL) {
        return new CanvasClientShared(aLayerForwarder, aLayer, aFlags);
      }
      return new CanvasClientTexture(aLayerForwarder, aLayer, aFlags);
    } else {
      return nullptr;
    }
  }

  RefPtr<ImageClient> result = nullptr;
  switch (aImageHostType) {
  case IMAGE_SHARED:
    if (aHostType == HOST_GL) {
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
    if (aHostType == HOST_GL) {
      result = new ImageClientTexture(aLayerForwarder, aLayer, aFlags);
    }
    break;
  case IMAGE_SHMEM: //TODO[nrc] SHMEM or Texture?
  case IMAGE_UNKNOWN:
    return result.forget();    
  }

  NS_ASSERTION(result, "Failed to create ImageClient");

  return result.forget();
}

/* static */ TemporaryRef<TextureClient>
CompositingFactory::CreateTextureClient(const TextureHostType &aHostType,
                                        const ImageHostType& aTextureHostType,
                                        const ImageHostType& aImageHostType,
                                        ShadowLayerForwarder* aLayerForwarder,
                                        bool aStrict /* = false */)
{
  RefPtr<TextureClient> result = nullptr;
  switch (aTextureHostType) {
  case IMAGE_SHARED:
    if (aHostType == HOST_GL) {
      result = new TextureClientShared(aLayerForwarder, aImageHostType);
    }
    break;
  case IMAGE_SHMEM:
    if (aHostType == HOST_GL) {
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