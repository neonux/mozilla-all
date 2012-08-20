/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextureClient.h"
#include "mozilla/layers/ShadowLayers.h"
#include "BasicLayers.h"
#include "SharedTextureImage.h"

namespace mozilla {
namespace layers {

class TextureClientTexture;

class ImageClientTexture : public ImageClient
{
public:
  ImageClientTexture(ShadowLayerForwarder* aLayerForwarder,
                     ShadowableLayer* aLayer);
  ~ImageClientTexture();

  virtual SharedImage GetAsSharedImage();
  virtual bool UpdateImage(ImageContainer* aContainer, ImageLayer* aLayer);
  virtual void SetBuffer(const SharedImage& aBuffer);

private:
  RefPtr<TextureClientTexture> mTextureClient;
};

class ImageClientShared : public ImageClient
{
public:
  ImageClientShared(ShadowLayerForwarder* aLayerForwarder,
                    ShadowableLayer* aLayer);
  ~ImageClientShared();

  virtual SharedImage GetAsSharedImage();
  virtual bool UpdateImage(ImageContainer* aContainer, ImageLayer* aLayer);

  virtual void SetBuffer(const SharedImage& aBuffer) {}

private:
  SurfaceDescriptor mDescriptor;
};

class ImageClientYUV : public ImageClient
{
public:
  ImageClientYUV(ShadowLayerForwarder* aLayerForwarder,
                 ShadowableLayer* aLayer);
  ~ImageClientYUV();

  virtual SharedImage GetAsSharedImage();
  virtual bool UpdateImage(ImageContainer* aContainer, ImageLayer* aLayer);
  virtual void SetBuffer(const SharedImage& aBuffer);


private:
  RefPtr<TextureClientTexture> mTextureClientY;
  RefPtr<TextureClientTexture> mTextureClientU;
  RefPtr<TextureClientTexture> mTextureClientV;
  nsIntRect mPictureRect;
};

//TODO[nrc] better name? At the moment it is just an impl class, but maybe it is specific to SurfaceDescriptor
class TextureClientTexture : public TextureClient
{
public:
  ~TextureClientTexture();

  virtual TextureIdentifier GetIdentifier() { return TextureIdentifier(); } //TODO[nrc]
  virtual TemporaryRef<gfx::DrawTarget> LockDT() { return nullptr; } //TODO[nrc]
  virtual already_AddRefed<gfxContext> LockContext();
  virtual gfxImageSurface* LockImageSurface();
  virtual void Unlock();
  virtual void EnsureTextureClient(gfx::IntSize aSize, gfxASurface::gfxContentType aType);
private:
  gfxASurface* GetSurface();

  TextureClientTexture(ShadowLayerForwarder* aLayerForwarder);

  nsRefPtr<gfxASurface> mSurface;
  nsRefPtr<gfxImageSurface> mSurfaceAsImage;

  gfxASurface::gfxContentType mContentType;
  SurfaceDescriptor mDescriptor;
  gfx::IntSize mSize;

  friend class ImageClientTexture;
  friend class ImageClientYUV;
  friend class CompositingFactory;
};



TextureClientTexture::TextureClientTexture(ShadowLayerForwarder* aLayerForwarder)
  : TextureClient(aLayerForwarder)
  , mSurface(nullptr)
  , mSurfaceAsImage(nullptr)
{
}

TextureClientTexture::~TextureClientTexture()
{
  //TODO[nrc] do I need to tell the host I've died?
  if (IsSurfaceDescriptorValid(mDescriptor)) {

    mLayerForwarder->DestroySharedSurface(&mDescriptor);
  }
}

void
TextureClientTexture::EnsureTextureClient(gfx::IntSize aSize, gfxASurface::gfxContentType aContentType)
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
TextureClientTexture::LockContext()
{
  nsRefPtr<gfxContext> result = new gfxContext(GetSurface());
  return result.forget();
}

gfxASurface*
TextureClientTexture::GetSurface()
{
  if (!mSurface) {
    mSurface = ShadowLayerForwarder::OpenDescriptor(OPEN_READ_WRITE, mDescriptor);
  }
  
  return mSurface.get();
}

void
TextureClientTexture::Unlock()
{
  mSurface = nullptr;
  mSurfaceAsImage = nullptr;

  ShadowLayerForwarder::CloseDescriptor(mDescriptor);
}

gfxImageSurface*
TextureClientTexture::LockImageSurface()
{
  if (!mSurfaceAsImage) {
    mSurfaceAsImage = GetSurface()->GetAsImageSurface();
  }

  return mSurfaceAsImage.get();
}


ImageClientTexture::ImageClientTexture(ShadowLayerForwarder* aLayerForwarder,
                                       ShadowableLayer* aLayer)
{
  mTextureClient = static_cast<TextureClientTexture*>(aLayerForwarder->CreateTextureClientFor(IMAGE_TEXTURE, aLayer, true).drop());
}

ImageClientTexture::~ImageClientTexture()
{
  //TODO[nrc] do I need to tell the host I've died?
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

  ImageSourceType type = CompositingFactory::TypeForImage(autoLock.GetImage());
  if (type != IMAGE_TEXTURE) {
    return type == IMAGE_UNKNOWN;
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

SharedImage
ImageClientTexture::GetAsSharedImage()
{
  return SharedImage(mTextureClient->mDescriptor);
}

void
ImageClientTexture::SetBuffer(const SharedImage& aBuffer)
{
  SharedImage::Type type = aBuffer.type();

  if (type != SharedImage::TSurfaceDescriptor) {
    mTextureClient->mDescriptor = SurfaceDescriptor();
    return;
  }

  mTextureClient->mDescriptor = aBuffer.get_SurfaceDescriptor();
}


ImageClientShared::ImageClientShared(ShadowLayerForwarder* aLayerForwarder,
                                     ShadowableLayer* aLayer)
{
}

ImageClientShared::~ImageClientShared()
{
  //TODO[nrc] do I need to tell the host I've died?
}

bool
ImageClientShared::UpdateImage(ImageContainer* aContainer, ImageLayer* aLayer)
{
  gfxASurface* dontCare = nullptr;
  AutoLockImage autoLock(aContainer, &dontCare);
  Image *image = autoLock.GetImage();
  ImageSourceType type = CompositingFactory::TypeForImage(autoLock.GetImage());
  if (type != IMAGE_SHARED) {
    return type == IMAGE_UNKNOWN;
  }

  SharedTextureImage* sharedImage = static_cast<SharedTextureImage*>(image);
  const SharedTextureImage::Data *data = sharedImage->GetData();

  SharedTextureDescriptor texture(data->mShareType, data->mHandle, data->mSize, data->mInverted);
  mDescriptor = SurfaceDescriptor(texture);

  return true;
}

SharedImage
ImageClientShared::GetAsSharedImage()
{
  return SharedImage(mDescriptor);
}


ImageClientYUV::ImageClientYUV(ShadowLayerForwarder* aLayerForwarder,
                               ShadowableLayer* aLayer)
{
  mTextureClientY = static_cast<TextureClientTexture*>(aLayerForwarder->CreateTextureClientFor(IMAGE_TEXTURE, aLayer, true).drop());
  mTextureClientU = static_cast<TextureClientTexture*>(aLayerForwarder->CreateTextureClientFor(IMAGE_TEXTURE, aLayer, true).drop());
  mTextureClientV = static_cast<TextureClientTexture*>(aLayerForwarder->CreateTextureClientFor(IMAGE_TEXTURE, aLayer, true).drop());
}

ImageClientYUV::~ImageClientYUV()
{
  //TODO[nrc] do I need to tell the host I've died?
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
  ImageSourceType type = CompositingFactory::TypeForImage(autoLock.GetImage());
  if (type != IMAGE_YUV) {
    return type == IMAGE_UNKNOWN;
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

SharedImage
ImageClientYUV::GetAsSharedImage()
{
  return YUVImage(mTextureClientY->mDescriptor,
                  mTextureClientU->mDescriptor,
                  mTextureClientV->mDescriptor,
                  mPictureRect);
}

void
ImageClientYUV::SetBuffer(const SharedImage& aBuffer)
{
  SharedImage::Type type = aBuffer.type();

  if (type != SharedImage::TSurfaceDescriptor) {
    mTextureClientY->mDescriptor = SurfaceDescriptor();
    mTextureClientU->mDescriptor = SurfaceDescriptor();
    mTextureClientV->mDescriptor = SurfaceDescriptor();
    return;
  }

  const YUVImage& yuv = aBuffer.get_YUVImage();
  mTextureClientY->mDescriptor = yuv.Ydata();
  mTextureClientU->mDescriptor = yuv.Udata();
  mTextureClientV->mDescriptor = yuv.Vdata();
}


/* static */ PRUint32 CompositingFactory::sId = 0;

/* static */ ImageSourceType
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
                                      const ImageSourceType& aImageSourceType,
                                      ShadowLayerForwarder* aLayerForwarder,
                                      ShadowableLayer* aLayer)
{
  //TODO[nrc]
  RefPtr<ImageClient> result = nullptr;
  switch (aHostType) {
  case HOST_D3D10:
    break;
  case HOST_GL:
    break;
  case HOST_SHMEM:
    break;
  }
  switch (aImageSourceType) {
  case IMAGE_UNKNOWN:
    return result.forget();    
  case IMAGE_SHARED:
    if (aHostType == HOST_GL) {
      result = new ImageClientShared(aLayerForwarder, aLayer);
    }
    break;
  case IMAGE_YUV:
    if (aLayer->AsLayer()->Manager()->IsCompositingCheap()) {
      result = new ImageClientYUV(aLayerForwarder, aLayer);
      break;
    }
    // fall through to IMAGE_TEXTURE
  case IMAGE_TEXTURE:
    if (aHostType == HOST_GL) {
      result = new ImageClientTexture(aLayerForwarder, aLayer);
    }
    break;
  case IMAGE_SHMEM:
    break;
  }

  NS_ASSERTION(result, "Failed to create ImageClient");

  return result.forget();
}

/* static */ TemporaryRef<TextureClient>
CompositingFactory::CreateTextureClient(const TextureHostType &aHostType,
                                        const ImageSourceType& aImageSourceType,
                                        ShadowLayerForwarder* aLayerForwarder,
                                        bool aStrict /* = false */)
{
  RefPtr<TextureClient> result = nullptr;
  switch (aImageSourceType) {
  case IMAGE_UNKNOWN:
  case IMAGE_SHARED:
    return result.forget();
  case IMAGE_YUV:
  case IMAGE_TEXTURE:
    if (aHostType == HOST_GL) {
      result = new TextureClientTexture(aLayerForwarder);
    }
    break;
  case IMAGE_SHMEM:
    break; //TODO[nrc]
  }

  NS_ASSERTION(result, "Failed to create ImageClient");

  return result.forget();
}

}
}