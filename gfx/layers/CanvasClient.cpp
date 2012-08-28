/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CanvasClient.h"
#include "TextureClient.h"
#include "BasicCanvasLayer.h"
#include "mozilla/layers/ShadowLayers.h"
#include "SharedTextureImage.h"
#include "nsXULAppAPI.h"

namespace mozilla {
namespace layers {

CanvasClientTexture::CanvasClientTexture(ShadowLayerForwarder* aLayerForwarder,
                                         ShadowableLayer* aLayer,
                                         TextureFlags aFlags)
{
  mTextureClient = static_cast<TextureClientShmem*>(
    aLayerForwarder->CreateTextureClientFor(IMAGE_TEXTURE, IMAGE_TEXTURE, aLayer, aFlags, true).drop());
}

void
CanvasClientTexture::Update(gfx::IntSize aSize, BasicCanvasLayer* aLayer)
{
  if (!mTextureClient) {
    return;
  }

  bool isOpaque = (aLayer->GetContentFlags() & Layer::CONTENT_OPAQUE);
  gfxASurface::gfxContentType contentType = isOpaque
                                              ? gfxASurface::CONTENT_COLOR
                                              : gfxASurface::CONTENT_COLOR_ALPHA;
  mTextureClient->EnsureTextureClient(aSize, contentType);

  gfxASurface* surface = mTextureClient->GetSurface();
  static_cast<BasicCanvasLayer*>(aLayer)->UpdateSurface(surface, nullptr);
  mTextureClient->Unlock();
}

SharedImage
CanvasClientTexture::GetAsSharedImage()
{
  return SharedImage(mTextureClient->Descriptor());
}

void
CanvasClientTexture::SetBuffer(const TextureIdentifier& aTextureIdentifier,
                               const SharedImage& aBuffer)
{
  SharedImage::Type type = aBuffer.type();

  if (type != SharedImage::TSurfaceDescriptor) {
    mTextureClient->Descriptor() = SurfaceDescriptor();
    return;
  }

  mTextureClient->Descriptor() = aBuffer.get_SurfaceDescriptor();
}


CanvasClientShared::CanvasClientShared(ShadowLayerForwarder* aLayerForwarder,
                                       ShadowableLayer* aLayer, 
                                       TextureFlags aFlags)
  : mDescriptor(0)
{
  // we need to create a TextureHost, even though we don't use a texture client
  aLayerForwarder->CreateTextureClientFor(IMAGE_SHARED, IMAGE_SHARED, aLayer, true, aFlags);
}

CanvasClientShared::~CanvasClientShared()
{
  SharedTextureDescriptor handle = mDescriptor.get_SharedTextureDescriptor();
  if (mGL && handle.handle()) {
    mGL->ReleaseSharedHandle(handle.shareType(), handle.handle());
  }
}

void
CanvasClientShared::Update(gfx::IntSize aSize, BasicCanvasLayer* aLayer)
{
  NS_ASSERTION(aLayer->mGLContext, "CanvasClientShared should only be used with GL canvases");
  mGL = aLayer->mGLContext;
  TextureImage::TextureShareType flags;
  // if process type is default, then it is single-process (non-e10s)
  if (XRE_GetProcessType() == GeckoProcessType_Default)
    flags = TextureImage::ThreadShared;
  else
    flags = TextureImage::ProcessShared;

  SharedTextureHandle handle = 0;
  if (mDescriptor.type() == SurfaceDescriptor::TSharedTextureDescriptor) {
    handle = mDescriptor.get_SharedTextureDescriptor().handle();
  } else {
    handle = mGL->CreateSharedHandle(flags);
    if (!handle) {
      return;
    }
    mDescriptor = SharedTextureDescriptor(flags, handle, nsIntSize(aSize.width, aSize.height), false);
  }

  mGL->MakeCurrent();
  mGL->UpdateSharedHandle(flags, handle);

  // Move SharedTextureHandle ownership to ShadowLayer
  mDescriptor = SurfaceDescriptor();
}

SharedImage
CanvasClientShared::GetAsSharedImage()
{
  return SharedImage(mDescriptor);
}

void
CanvasClientShared::SetBuffer(const TextureIdentifier& aTextureIdentifier,
                              const SharedImage& aBuffer)
{
  mDescriptor = aBuffer.get_SurfaceDescriptor();
}

}
}
