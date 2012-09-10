/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_IMAGECLIENT_H
#define MOZILLA_GFX_IMAGECLIENT_H

#include "mozilla/layers/LayersSurfaces.h"
#include "BufferClient.h"
#include "TextureClient.h"

namespace mozilla {
namespace layers {

class ImageContainer;
class ImageLayer;

class ImageClient : public BufferClient
{
public:
  virtual ~ImageClient() {}
  //TODO[nrc] comments

  // returns false if this is the wrong kind of ImageClient for aContainer
  // note returning true does not necessarily imply success
  virtual bool UpdateImage(ImageContainer* aContainer, ImageLayer* aLayer) = 0;

  virtual void SetBuffer(const TextureIdentifier& aTextureIdentifier,
                         const SharedImage& aBuffer) = 0;

  virtual void Updated(ShadowableLayer* aLayer) = 0;
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

  virtual void Updated(ShadowableLayer* aLayer);
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

  virtual void Updated(ShadowableLayer* aLayer);
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

  virtual void Updated(ShadowableLayer* aLayer);
private:
  ShadowLayerForwarder* mLayerForwarder;
  RefPtr<TextureClient> mTextureClientY;
  RefPtr<TextureClient> mTextureClientU;
  RefPtr<TextureClient> mTextureClientV;
  nsIntRect mPictureRect;
};

}
}

#endif
