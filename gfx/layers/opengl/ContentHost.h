/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_CONTENTHOST_H
#define GFX_CONTENTHOST_H

#include "ThebesLayerBuffer.h"
#include "Compositor.h"

namespace mozilla {
namespace layers {

class TextureImageAsTextureHost;
class ThebesBuffer;
class OptionalThebesBuffer;

class CompositingThebesLayerBuffer
{
  NS_INLINE_DECL_REFCOUNTING(CompositingThebesLayerBuffer)
public:
  typedef ThebesLayerBuffer::ContentType ContentType;
  typedef ThebesLayerBuffer::PaintState PaintState;

  CompositingThebesLayerBuffer(Compositor* aCompositor)
    : mCompositor(aCompositor)
    , mPaintWillResample(false)
    , mTextureHost(nullptr)
    , mTextureHostOnWhite(nullptr)
    , mInitialised(true)
  {}
  virtual ~CompositingThebesLayerBuffer() {}

  virtual PaintState BeginPaint(ContentType aContentType,
                                PRUint32 aFlags) = 0;

  void Composite(EffectChain& aEffectChain,
                 float aOpacity,
                 const gfx::Matrix4x4& aTransform,
                 const gfx::Point& aOffset,
                 const gfx::Filter& aFilter,
                 const gfx::Rect& aClipRect,
                 const nsIntRegion* aVisibleRegion = nullptr);

  void SetPaintWillResample(bool aResample) { mPaintWillResample = aResample; }

protected:
  virtual nsIntPoint GetOriginOffset() = 0;

  bool PaintWillResample() { return mPaintWillResample; }

  bool mPaintWillResample;
  RefPtr<Compositor> mCompositor;
  RefPtr<TextureImageAsTextureHost> mTextureHost;
  RefPtr<TextureImageAsTextureHost> mTextureHostOnWhite;
  bool mInitialised;
};

class AContentHost : public ImageHost
{
public:
  typedef uint32_t UpdateFlags;
  static const UpdateFlags RESET_BUFFER = 0x1;
  static const UpdateFlags UPDATE_SUCCESS = 0x2;
  static const UpdateFlags UPDATE_FAIL = 0x4;
  static const UpdateFlags UPDATE_NOSWAP = 0x8;

  virtual const SharedImage* UpdateImage(const TextureIdentifier& aTextureIdentifier,
                                         const SharedImage& aImage)
  {
    //TODO[nrc]
    NS_ERROR("Shouldn't call UpdateImage on a Thebes buffer");
    return nullptr;
  }

  //TODO[nrc] comment, maybe move to ImageHost?
  virtual UpdateFlags UpdateThebes(const TextureIdentifier& aTextureIdentifier,
                                   const ThebesBuffer& aNewBack,
                                   const nsIntRegion& aUpdated,
                                   OptionalThebesBuffer* aNewFront) = 0;

  virtual void Composite(EffectChain& aEffectChain,
                         float aOpacity,
                         const gfx::Matrix4x4& aTransform,
                         const gfx::Point& aOffset,
                         const gfx::Filter& aFilter,
                         const gfx::Rect& aClipRect,
                         const nsIntRegion* aVisibleRegion = nullptr) = 0;

  virtual void AddTextureHost(const TextureIdentifier& aTextureIdentifier, TextureHost* aTextureHost) = 0;
};

}
}

#endif