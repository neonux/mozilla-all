/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_IMAGECLIENT_H
#define MOZILLA_GFX_IMAGECLIENT_H

#include "mozilla/layers/LayersSurfaces.h"
#include "Compositor.h"
#include "TextureClient.h"

namespace mozilla {
namespace layers {

class ImageContainer;
class ImageLayer;

class ImageClient : public RefCounted<ImageClient>
{
public:
  virtual ~ImageClient() {}
  //TODO[nrc] comments

  // returns false if this is the wrong kind of ImageClient for aContainer
  // note returning true does not necessarily imply success
  virtual bool UpdateImage(ImageContainer* aContainer, ImageLayer* aLayer) = 0;

  virtual void SetBuffer(const TextureIdentifier& aTextureIdentifier,
                         const SharedImage& aBuffer) = 0;
};

class ImageClientTexture : public ImageClient
{
public:
  ImageClientTexture(ShadowLayerForwarder* aLayerForwarder,
                     ShadowableLayer* aLayer,
                     TextureFlags aFlags);
  virtual ~ImageClientTexture();

  virtual bool UpdateImage(ImageContainer* aContainer, ImageLayer* aLayer);
  virtual void SetBuffer(const TextureIdentifier& aTextureIdentifier,
                         const SharedImage& aBuffer);

private:
  RefPtr<TextureClient> mTextureClient;
};

class ImageClientShared : public ImageClient
{
public:
  ImageClientShared(ShadowLayerForwarder* aLayerForwarder,
                    ShadowableLayer* aLayer,
                    TextureFlags aFlags);
  virtual ~ImageClientShared();

  virtual bool UpdateImage(ImageContainer* aContainer, ImageLayer* aLayer);

  virtual void SetBuffer(const TextureIdentifier& aTextureIdentifier,
                         const SharedImage& aBuffer) {}

private:
  RefPtr<TextureClient> mTextureClient;
};

class ImageClientYUV : public ImageClient
{
public:
  ImageClientYUV(ShadowLayerForwarder* aLayerForwarder,
                 ShadowableLayer* aLayer,
                 TextureFlags aFlags);
  virtual ~ImageClientYUV();

  virtual bool UpdateImage(ImageContainer* aContainer, ImageLayer* aLayer);
  virtual void SetBuffer(const TextureIdentifier& aTextureIdentifier,
                         const SharedImage& aBuffer);

private:
  RefPtr<TextureClientShmem> mTextureClientY;
  RefPtr<TextureClientShmem> mTextureClientU;
  RefPtr<TextureClientShmem> mTextureClientV;
  nsIntRect mPictureRect;
};

}
}

#endif
