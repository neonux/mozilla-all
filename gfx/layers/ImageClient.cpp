/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextureClient.h"
#include "ImageClient.h"
#include "BasicLayers.h"
#include "mozilla/layers/ShadowLayers.h"
#include "SharedTextureImage.h"

namespace mozilla {
namespace layers {

ImageClientTexture::ImageClientTexture(ShadowLayerForwarder* aLayerForwarder,
                                       ShadowableLayer* aLayer,
                                       TextureFlags aFlags)
{
  mTextureClient = aLayerForwarder->CreateTextureClientFor(TEXTURE_SHMEM, BUFFER_TEXTURE, aLayer, aFlags, true);
}

bool
ImageClientTexture::UpdateImage(ImageContainer* aContainer, ImageLayer* aLayer)
{
  if (!mTextureClient) {
    return true;
  }

  nsRefPtr<gfxASurface> surface;
  AutoLockImage autoLock(aContainer, getter_AddRefs(surface));
  Image *image = autoLock.GetImage();

  BufferType type = CompositingFactory::TypeForImage(autoLock.GetImage());
  if (type != BUFFER_TEXTURE) {
    return type == BUFFER_UNKNOWN;
  }

  nsRefPtr<gfxPattern> pat = new gfxPattern(surface);
  if (!pat)
    return true;

  pat->SetFilter(aLayer->GetFilter());
  gfxMatrix mat = pat->GetMatrix();
  aLayer->ScaleMatrix(surface->GetSize(), mat);
  pat->SetMatrix(mat);

  gfxIntSize size = autoLock.GetSize();

  gfxASurface::gfxContentType contentType = gfxASurface::CONTENT_COLOR_ALPHA;
  bool isOpaque = (aLayer->GetContentFlags() & Layer::CONTENT_OPAQUE);
  if (surface) {
    contentType = surface->GetContentType();
  }
  if (contentType != gfxASurface::CONTENT_ALPHA &&
      isOpaque) {
    contentType = gfxASurface::CONTENT_COLOR;
  }
  mTextureClient->EnsureTextureClient(gfx::IntSize(size.width, size.height), contentType);

  nsRefPtr<gfxContext> tmpCtx = mTextureClient->LockContext();
  tmpCtx->SetOperator(gfxContext::OPERATOR_SOURCE);
  PaintContext(pat,
               nsIntRegion(nsIntRect(0, 0, size.width, size.height)),
               1.0, tmpCtx, nullptr);

  mTextureClient->Unlock();
  return true;
}


void
ImageClientTexture::SetBuffer(const TextureIdentifier& aTextureIdentifier,
                              const SharedImage& aBuffer)
{
  SharedImage::Type type = aBuffer.type();

  if (type != SharedImage::TSurfaceDescriptor) {
    mTextureClient->SetDescriptor(SurfaceDescriptor());
    return;
  }

  mTextureClient->SetDescriptor(aBuffer.get_SurfaceDescriptor());
}

void
ImageClientTexture::Updated(ShadowableLayer* aLayer)
{
  mTextureClient->Updated(aLayer);
}

ImageClientShared::ImageClientShared(ShadowLayerForwarder* aLayerForwarder,
                                     ShadowableLayer* aLayer, 
                                     TextureFlags aFlags)
{
  mTextureClient = aLayerForwarder->CreateTextureClientFor(TEXTURE_SHARED, BUFFER_SHARED, aLayer, true, aFlags);
}

bool
ImageClientShared::UpdateImage(ImageContainer* aContainer, ImageLayer* aLayer)
{
  gfxASurface* dontCare = nullptr;
  AutoLockImage autoLock(aContainer, &dontCare);
  Image *image = autoLock.GetImage();
  BufferType type = CompositingFactory::TypeForImage(autoLock.GetImage());
  if (type != BUFFER_SHARED) {
    return type == BUFFER_UNKNOWN;
  }

  SharedTextureImage* sharedImage = static_cast<SharedTextureImage*>(image);
  const SharedTextureImage::Data *data = sharedImage->GetData();

  SharedTextureDescriptor texture(data->mShareType, data->mHandle, data->mSize, data->mInverted);
  mTextureClient->SetDescriptor(SurfaceDescriptor(texture));

  return true;
}

void
ImageClientShared::Updated(ShadowableLayer* aLayer)
{
  mTextureClient->Updated(aLayer);
}


ImageClientYUV::ImageClientYUV(ShadowLayerForwarder* aLayerForwarder,
                               ShadowableLayer* aLayer,
                               TextureFlags aFlags)
  : mLayerForwarder(aLayerForwarder)
{
  mTextureClientY = static_cast<TextureClientShmem*>(
    aLayerForwarder->CreateTextureClientFor(TEXTURE_SHMEM, BUFFER_YUV, aLayer, true, aFlags).drop());
  mTextureClientY->SetDescriptor(0);
  mTextureClientU = static_cast<TextureClientShmem*>(
    aLayerForwarder->CreateTextureClientFor(TEXTURE_SHMEM, BUFFER_YUV, aLayer, true, aFlags).drop());
  mTextureClientU->SetDescriptor(1);
  mTextureClientV = static_cast<TextureClientShmem*>(
    aLayerForwarder->CreateTextureClientFor(TEXTURE_SHMEM, BUFFER_YUV, aLayer, true, aFlags).drop());
  mTextureClientV->SetDescriptor(2);
}

bool
ImageClientYUV::UpdateImage(ImageContainer* aContainer, ImageLayer* aLayer)
{
  if (!mTextureClientY ||
      !mTextureClientU ||
      !mTextureClientV) {
    return true;
  }

  gfxASurface* dontCare = nullptr;
  AutoLockImage autoLock(aContainer, &dontCare);

  Image *image = autoLock.GetImage();
  BufferType type = CompositingFactory::TypeForImage(autoLock.GetImage());
  if (type != BUFFER_YUV) {
    return type == BUFFER_UNKNOWN;
  }

  PlanarYCbCrImage *YCbCrImage = static_cast<PlanarYCbCrImage*>(image);
  const PlanarYCbCrImage::Data *data = YCbCrImage->GetData();
  NS_ASSERTION(data, "Must be able to retrieve yuv data from image!");

  gfxIntSize sizeY = data->mYSize;
  gfxIntSize sizeCbCr = data->mCbCrSize;
  mTextureClientY->EnsureTextureClient(gfx::IntSize(sizeY.width, sizeY.height),
                                       gfxASurface::CONTENT_ALPHA);
  mTextureClientU->EnsureTextureClient(gfx::IntSize(sizeCbCr.width, sizeCbCr.height),
                                       gfxASurface::CONTENT_ALPHA);
  mTextureClientV->EnsureTextureClient(gfx::IntSize(sizeCbCr.width, sizeCbCr.height),
                                       gfxASurface::CONTENT_ALPHA);

  gfxImageSurface* dy = mTextureClientY->LockImageSurface();
  for (int i = 0; i < data->mYSize.height; i++) {
    memcpy(dy->Data() + i * dy->Stride(),
           data->mYChannel + i * data->mYStride,
           data->mYSize.width);
  }
  mTextureClientY->Unlock();

  gfxImageSurface* du = mTextureClientU->LockImageSurface();
  gfxImageSurface* dv = mTextureClientV->LockImageSurface();
  for (int i = 0; i < data->mCbCrSize.height; i++) {
    memcpy(du->Data() + i * du->Stride(),
           data->mCbChannel + i * data->mCbCrStride,
           data->mCbCrSize.width);
    memcpy(dv->Data() + i * dv->Stride(),
           data->mCrChannel + i * data->mCbCrStride,
           data->mCbCrSize.width);
  }
  mTextureClientU->Unlock();
  mTextureClientV->Unlock();

  mPictureRect = data->GetPictureRect();
  return true;
}

void
ImageClientYUV::SetBuffer(const TextureIdentifier& aTextureIdentifier,
                          const SharedImage& aBuffer)
{
  SharedImage::Type type = aBuffer.type();

  TextureClient* textureClient;
  switch (aTextureIdentifier.mDescriptor) {
  case 0:
    textureClient = mTextureClientY;
    break;
  case 1:
    textureClient = mTextureClientY;
    break;
  case 2:
    textureClient = mTextureClientY;
    break;
  default:
    NS_WARNING("Bad texture identifier");
    return;
  }

  if (type != SharedImage::TSurfaceDescriptor) {
    textureClient->SetDescriptor(SurfaceDescriptor());
    return;
  }

  textureClient->SetDescriptor(aBuffer.get_SurfaceDescriptor());
}

void
ImageClientYUV::Updated(ShadowableLayer* aLayer)
{
  mTextureClientY->Updated(aLayer);
  mTextureClientU->Updated(aLayer);
  mTextureClientV->Updated(aLayer);
  mLayerForwarder->UpdatePictureRect(aLayer, mPictureRect);
}

}
}
