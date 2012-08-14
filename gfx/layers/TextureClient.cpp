/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextureClient.h"
#include "mozilla/layers/ShadowLayers.h"

namespace mozilla {
namespace layers {

TextureClientTexture::TextureClientTexture(PRUint32 aId, ShadowLayerForwarder* aLayerForwarder)
  : mLayerForwarder(aLayerForwarder)
  , mSurface(nullptr)
{
  mIdentifier.mType = IMAGE_TEXTURE;
  mIdentifier.mDescriptor = aId;
}

TextureClientTexture::~TextureClientTexture()
{
  //TODO[nrc] do I need to tell the host I've died?
  if (IsSurfaceDescriptorValid(mDescriptor)) {
    mLayerForwarder->DestroySharedSurface(&mDescriptor);
  }
}

TextureIdentifier
TextureClientTexture::GetIdentifier()
{
  return mIdentifier;
}

void
TextureClientTexture::EnsureTextureClient(gfx::IntSize aSize, gfxASurface::gfxContentType aType)
{
  if (aSize != mSize ||
      aType != mType ||
      !IsSurfaceDescriptorValid(mDescriptor)) {
    if (IsSurfaceDescriptorValid(mDescriptor)) {
      mLayerForwarder->DestroySharedSurface(&mDescriptor);
    }
    mType = aType;
    mSize = aSize;

    if (!mLayerForwarder->AllocBuffer(gfxIntSize(mSize.width, mSize.height), aType, &mDescriptor))
      NS_RUNTIMEABORT("creating SurfaceDescriptor failed!");
  }
}

TemporaryRef<gfx::DrawTarget>
TextureClientTexture::LockDT()
{
  //TODO[nrc]
  NS_ERROR("Not implemented");
  return nullptr;
}

already_AddRefed<gfxContext>
TextureClientTexture::LockContext()
{
  if (!mSurface) {
    mSurface = ShadowLayerForwarder::OpenDescriptor(OPEN_READ_WRITE, mDescriptor);
  }
  nsRefPtr<gfxContext> result = new gfxContext(mSurface.get());
  return result.forget();
}

void
TextureClientTexture::Unlock()
{
  mSurface = nullptr;
  ShadowLayerForwarder::CloseDescriptor(mDescriptor);
}

SharedImage
TextureClientTexture::GetAsSharedImage()
{
  return SharedImage(mDescriptor);
}

/* static */ PRUint32 CompositingFactory::sId = 0;

/* static */ TemporaryRef<TextureClient>
CompositingFactory::CreateTextureClient(const TextureHostType &aHostType,
                                        const ImageSourceType& aImageSourceType,
                                        ShadowLayerForwarder* aLayerForwarder)
{
  //TODO[nrc]
  RefPtr<TextureClient> result = nullptr;
  switch (aHostType) {
  case HOST_D3D10:
    break;
  case HOST_GL:
    break;
  case HOST_SHMEM:
    break;
  }
  switch (aImageSourceType) {
  case IMAGE_YUV:
    break;
  case IMAGE_SHARED:
    break;
  case IMAGE_TEXTURE:
    result = new TextureClientTexture(sId++, aLayerForwarder);
    break;
  case IMAGE_SHMEM:
    break;
  }

  NS_ASSERTION(result, "Failed to create TextureClient");

  return result.forget();
}

}
}