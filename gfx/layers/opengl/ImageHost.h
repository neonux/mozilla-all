/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_IMAGEHOST_H
#define MOZILLA_GFX_IMAGEHOST_H

#include "BufferHost.h"
#include "TextureOGL.h"

namespace mozilla {
namespace layers {

class ImageHost : public BufferHost
{
public:
  virtual const SharedImage* UpdateImage(const TextureIdentifier& aTextureIdentifier,
                                         const SharedImage& aImage) = 0;

  virtual void SetPictureRect(const nsIntRect& aPictureRect) {}

protected:
  ImageHost(Compositor* aCompositor)
    : mCompositor(aCompositor)
  {
  }

  RefPtr<Compositor> mCompositor;
};


class ImageHostTexture : public ImageHost
{
public:
  ImageHostTexture(Compositor* aCompositor);

  virtual BufferType GetType() { return BUFFER_TEXTURE; }

  virtual const SharedImage* UpdateImage(const TextureIdentifier& aTextureIdentifier,
                                   const SharedImage& aImage);

  virtual void Composite(EffectChain& aEffectChain,
                         float aOpacity,
                         const gfx::Matrix4x4& aTransform,
                         const gfx::Point& aOffset,
                         const gfx::Filter& aFilter,
                         const gfx::Rect& aClipRect,
                         const nsIntRegion* aVisibleRegion = nullptr);

  virtual void AddTextureHost(const TextureIdentifier& aTextureIdentifier, TextureHost* aTextureHost);

private:
  RefPtr<TextureHost> mTextureHost;
};

class ImageHostShared : public ImageHost
{
public:
  ImageHostShared(Compositor* aCompositor);

  virtual BufferType GetType() { return BUFFER_SHARED; }


  virtual const SharedImage* UpdateImage(const TextureIdentifier& aTextureIdentifier,
                                         const SharedImage& aImage);

  virtual void Composite(EffectChain& aEffectChain,
                         float aOpacity,
                         const gfx::Matrix4x4& aTransform,
                         const gfx::Point& aOffset,
                         const gfx::Filter& aFilter,
                         const gfx::Rect& aClipRect,
                         const nsIntRegion* aVisibleRegion = nullptr);

  virtual void AddTextureHost(const TextureIdentifier& aTextureIdentifier, TextureHost* aTextureHost);

private:
  RefPtr<TextureHost> mTextureHost;
};

class YUVImageHost : public ImageHost
{
public:
  YUVImageHost(Compositor* aCompositor)
    : ImageHost(aCompositor)
  {
  }

  ~YUVImageHost();

  virtual BufferType GetType() { return BUFFER_YUV; }

  // aImage contains all three plains, we could also send them seperately and 
  // update mTextures one at a time
  virtual const SharedImage* UpdateImage(const TextureIdentifier& aTextureIdentifier,
                                         const SharedImage& aImage);

  virtual void Composite(EffectChain& aEffectChain,
                         float aOpacity,
                         const gfx::Matrix4x4& aTransform,
                         const gfx::Point& aOffset,
                         const gfx::Filter& aFilter,
                         const gfx::Rect& aClipRect,
                         const nsIntRegion* aVisibleRegion = nullptr);

  virtual void AddTextureHost(const TextureIdentifier& aTextureIdentifier, TextureHost* aTextureHost);

  virtual void SetPictureRect(const nsIntRect& aPictureRect)
  {
    mPictureRect = aPictureRect;
  }

private:
  RefPtr<TextureHost> mTextures[3];
  nsIntRect mPictureRect;
};


}
}
#endif
