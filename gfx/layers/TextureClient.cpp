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

class TextureClientShmem;

class ImageClientTexture : public ImageClient
{
public:
  ImageClientTexture(ShadowLayerForwarder* aLayerForwarder,
                     ShadowableLayer* aLayer);
  virtual ~ImageClientTexture();

  virtual SharedImage GetAsSharedImage();
  virtual bool UpdateImage(ImageContainer* aContainer, ImageLayer* aLayer);
  virtual void SetBuffer(const TextureIdentifier& aTextureIdentifier,
                         const SharedImage& aBuffer);

private:
  RefPtr<TextureClientShmem> mTextureClient;
};

class ImageClientShared : public ImageClient
{
public:
  ImageClientShared(ShadowLayerForwarder* aLayerForwarder,
                    ShadowableLayer* aLayer);
  virtual ~ImageClientShared();

  virtual SharedImage GetAsSharedImage();
  virtual bool UpdateImage(ImageContainer* aContainer, ImageLayer* aLayer);

  virtual void SetBuffer(const TextureIdentifier& aTextureIdentifier,
                         const SharedImage& aBuffer) {}

private:
  SurfaceDescriptor mDescriptor;
};

class ImageClientYUV : public ImageClient
{
public:
  ImageClientYUV(ShadowLayerForwarder* aLayerForwarder,
                 ShadowableLayer* aLayer);
  virtual ~ImageClientYUV();

  virtual SharedImage GetAsSharedImage();
  virtual bool UpdateImage(ImageContainer* aContainer, ImageLayer* aLayer);
  virtual void SetBuffer(const TextureIdentifier& aTextureIdentifier,
                         const SharedImage& aBuffer);

private:
  RefPtr<TextureClientShmem> mTextureClientY;
  RefPtr<TextureClientShmem> mTextureClientU;
  RefPtr<TextureClientShmem> mTextureClientV;
  nsIntRect mPictureRect;
};

class TextureClientShmem : public TextureClient
{
public:
  virtual ~TextureClientShmem();

  virtual TemporaryRef<gfx::DrawTarget> LockDT() { return nullptr; } //TODO[nrc]
  virtual already_AddRefed<gfxContext> LockContext();
  virtual gfxImageSurface* LockImageSurface();
  virtual void Unlock();
  virtual void EnsureTextureClient(gfx::IntSize aSize, gfxASurface::gfxContentType aType);
  // only exposed to ImageClients, not sure if this will work for other uses
  SurfaceDescriptor& Descriptor() { return mDescriptor; }
private:
  gfxASurface* GetSurface();

  TextureClientShmem(ShadowLayerForwarder* aLayerForwarder, ImageHostType aImageType);

  nsRefPtr<gfxASurface> mSurface;
  nsRefPtr<gfxImageSurface> mSurfaceAsImage;

  gfxASurface::gfxContentType mContentType;
  SurfaceDescriptor mDescriptor;
  gfx::IntSize mSize;

  friend class CompositingFactory;
};

// this class is just a place holder really
class TextureClientShared : public TextureClient
{
public:
  virtual ~TextureClientShared() {}

  virtual TemporaryRef<gfx::DrawTarget> LockDT() { return nullptr; } 
  virtual already_AddRefed<gfxContext> LockContext()  { return nullptr; }
  virtual gfxImageSurface* LockImageSurface() { return nullptr; }
  virtual void Unlock() {}
  virtual void EnsureTextureClient(gfx::IntSize aSize, gfxASurface::gfxContentType aType) {}
private:
  gfxASurface* GetSurface();

  TextureClientShared(ShadowLayerForwarder* aLayerForwarder, ImageHostType aImageType)
    : TextureClient(aLayerForwarder, aImageType)
  {
    mIdentifier.mTextureType = IMAGE_SHARED;
  }

  friend class CompositingFactory;
};



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


ImageClientTexture::ImageClientTexture(ShadowLayerForwarder* aLayerForwarder,
                                       ShadowableLayer* aLayer)
{
  mTextureClient = static_cast<TextureClientShmem*>(
    aLayerForwarder->CreateTextureClientFor(IMAGE_TEXTURE, IMAGE_TEXTURE, aLayer, true).drop());
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

  ImageHostType type = CompositingFactory::TypeForImage(autoLock.GetImage());
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
  return SharedImage(mTextureClient->Descriptor());
}

void
ImageClientTexture::SetBuffer(const TextureIdentifier& aTextureIdentifier,
                              const SharedImage& aBuffer)
{
  SharedImage::Type type = aBuffer.type();

  if (type != SharedImage::TSurfaceDescriptor) {
    mTextureClient->Descriptor() = SurfaceDescriptor();
    return;
  }

  mTextureClient->Descriptor() = aBuffer.get_SurfaceDescriptor();
}


ImageClientShared::ImageClientShared(ShadowLayerForwarder* aLayerForwarder,
                                     ShadowableLayer* aLayer)
{
  // we need to create a TextureHost, even though we don't use a texture client
  aLayerForwarder->CreateTextureClientFor(IMAGE_SHARED, IMAGE_SHARED, aLayer, true);
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
  ImageHostType type = CompositingFactory::TypeForImage(autoLock.GetImage());
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
  mTextureClientY = static_cast<TextureClientShmem*>(
    aLayerForwarder->CreateTextureClientFor(IMAGE_SHMEM, IMAGE_YUV, aLayer, true).drop());
  mTextureClientY->SetDescriptor(0);
  mTextureClientU = static_cast<TextureClientShmem*>(
    aLayerForwarder->CreateTextureClientFor(IMAGE_SHMEM, IMAGE_YUV, aLayer, true).drop());
  mTextureClientU->SetDescriptor(1);
  mTextureClientV = static_cast<TextureClientShmem*>(
    aLayerForwarder->CreateTextureClientFor(IMAGE_SHMEM, IMAGE_YUV, aLayer, true).drop());
  mTextureClientV->SetDescriptor(2);
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
  ImageHostType type = CompositingFactory::TypeForImage(autoLock.GetImage());
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
  return YUVImage(mTextureClientY->Descriptor(),
                  mTextureClientU->Descriptor(),
                  mTextureClientV->Descriptor(),
                  mPictureRect);
}

// we deal with all three textures at once with this kind of reply
void
ImageClientYUV::SetBuffer(const TextureIdentifier& aTextureIdentifier,
                          const SharedImage& aBuffer)
{
  SharedImage::Type type = aBuffer.type();

  if (type != SharedImage::TSurfaceDescriptor) {
    mTextureClientY->Descriptor() = SurfaceDescriptor();
    mTextureClientU->Descriptor() = SurfaceDescriptor();
    mTextureClientV->Descriptor() = SurfaceDescriptor();
    return;
  }

  const YUVImage& yuv = aBuffer.get_YUVImage();
  mTextureClientY->Descriptor() = yuv.Ydata();
  mTextureClientU->Descriptor() = yuv.Udata();
  mTextureClientV->Descriptor() = yuv.Vdata();
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
  switch (aImageHostType) {
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