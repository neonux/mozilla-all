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
  mTextureClient = aLayerForwarder->CreateTextureClientFor(TEXTURE_SHMEM, IMAGE_TEXTURE, aLayer, aFlags, true);
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

  gfxASurface* surface = mTextureClient->LockSurface();
  static_cast<BasicCanvasLayer*>(aLayer)->UpdateSurface(surface, nullptr);
  mTextureClient->Unlock();
}

void
CanvasClient::SetBuffer(const TextureIdentifier& aTextureIdentifier,
                        const SharedImage& aBuffer)
{
  if (aBuffer.type() != SharedImage::TSurfaceDescriptor) {
    mTextureClient->SetDescriptor(SurfaceDescriptor());
    return;
  }

  mTextureClient->SetDescriptor(aBuffer.get_SurfaceDescriptor());
}


CanvasClientShared::CanvasClientShared(ShadowLayerForwarder* aLayerForwarder,
                                       ShadowableLayer* aLayer, 
                                       TextureFlags aFlags)
{
  mTextureClient = aLayerForwarder->CreateTextureClientFor(TEXTURE_SHARED_GL, IMAGE_SHARED, aLayer, true, aFlags);
}

void
CanvasClientShared::Update(gfx::IntSize aSize, BasicCanvasLayer* aLayer)
{
  if (!mTextureClient) {
    return;
  }

  NS_ASSERTION(aLayer->mGLContext, "CanvasClientShared should only be used with GL canvases");

  // the content type won't be used
  mTextureClient->EnsureTextureClient(aSize, gfxASurface::CONTENT_COLOR);

  TextureImage::TextureShareType flags;
  // if process type is default, then it is single-process (non-e10s)
  if (XRE_GetProcessType() == GeckoProcessType_Default)
    flags = TextureImage::ThreadShared;
  else
    flags = TextureImage::ProcessShared;

  SharedTextureHandle handle = mTextureClient->LockHandle(aLayer->mGLContext, flags);

  if (handle) {
    aLayer->mGLContext->MakeCurrent();
    aLayer->mGLContext->UpdateSharedHandle(flags, handle);
  }

  mTextureClient->Unlock();
}

}
}
