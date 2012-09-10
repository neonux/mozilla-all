/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_CONTENTHOST_H
#define GFX_CONTENTHOST_H

#include "ThebesLayerBuffer.h"
#include "BufferHost.h"

namespace mozilla {
namespace layers {

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
  //TODO[nrc] comment
  virtual TextureImage* GetTextureImage() = 0;
  virtual TextureImage* GetTextureImageOnWhite() = 0;
  virtual TemporaryRef<TextureHost> GetTextureHost() = 0;
  virtual TemporaryRef<TextureHost> GetTextureHostOnWhite() = 0;

  virtual nsIntPoint GetOriginOffset() = 0;

  bool PaintWillResample() { return mPaintWillResample; }

  bool mPaintWillResample;
  RefPtr<Compositor> mCompositor;
  bool mInitialised;
};

class AContentHost : public BufferHost
{
public:
  //TODO[nrc] comment
  virtual void UpdateThebes(const TextureIdentifier& aTextureIdentifier,
                            const ThebesBuffer& aNewBack,
                            const nsIntRegion& aUpdated,
                            OptionalThebesBuffer* aNewFront,
                            const nsIntRegion& aOldValidRegionFront,
                            const nsIntRegion& aOldValidRegionBack,
                            OptionalThebesBuffer* aNewBackResult,
                            nsIntRegion* aNewValidRegionFront,
                            nsIntRegion* aUpdatedRegionBack) = 0;

  virtual void SetDeAllocator(ISurfaceDeAllocator* aDeAllocator) {}
};

class ContentHost : public AContentHost, protected CompositingThebesLayerBuffer
{
public:

  ContentHost(Compositor* aCompositor)
    : CompositingThebesLayerBuffer(aCompositor)
  {
    mInitialised = false;
  }

  void Release() { AContentHost::Release(); }
  void AddRef() { AContentHost::AddRef(); }

  // AContentHost implementation
  virtual void Composite(EffectChain& aEffectChain,
                         float aOpacity,
                         const gfx::Matrix4x4& aTransform,
                         const gfx::Point& aOffset,
                         const gfx::Filter& aFilter,
                         const gfx::Rect& aClipRect,
                         const nsIntRegion* aVisibleRegion = nullptr)
  {
    CompositingThebesLayerBuffer::Composite(aEffectChain,
                                            aOpacity,
                                            aTransform,
                                            aOffset,
                                            aFilter,
                                            aClipRect,
                                            aVisibleRegion);
  }

  // CompositingThebesLayerBuffer implementation
  virtual PaintState BeginPaint(ContentType aContentType, PRUint32) {
    NS_RUNTIMEABORT("can't BeginPaint for a shadow layer");
    return PaintState();
  }

  virtual void SetDeAllocator(ISurfaceDeAllocator* aDeAllocator)
  {
    mTextureHost->SetDeAllocator(aDeAllocator);
  }

protected:
  virtual TextureImage* GetTextureImage();
  virtual TextureImage* GetTextureImageOnWhite() { return nullptr; }
  virtual TemporaryRef<TextureHost> GetTextureHost() { return mTextureHost; }
  virtual TemporaryRef<TextureHost> GetTextureHostOnWhite() { return nullptr; }

  virtual nsIntPoint GetOriginOffset() {
    return mBufferRect.TopLeft() - mBufferRotation;
  }

  RefPtr<TextureHost> mTextureHost;
  nsIntRect mBufferRect;
  nsIntPoint mBufferRotation;
};

// We can directly texture the drawn surface.  Use that as our new
// front buffer, and return our previous directly-textured surface
// to the renderer.
class ContentHostDirect : public ContentHost
{
public:
  ContentHostDirect(Compositor* aCompositor)
    : ContentHost(aCompositor)
  {}

  virtual BufferType GetType() { return BUFFER_DIRECT; }

  virtual void UpdateThebes(const TextureIdentifier& aTextureIdentifier,
                            const ThebesBuffer& aNewBack,
                            const nsIntRegion& aUpdated,
                            OptionalThebesBuffer* aNewFront,
                            const nsIntRegion& aOldValidRegionFront,
                            const nsIntRegion& aOldValidRegionBack,
                            OptionalThebesBuffer* aNewBackResult,
                            nsIntRegion* aNewValidRegionFront,
                            nsIntRegion* aUpdatedRegionBack);

  virtual void AddTextureHost(const TextureIdentifier& aTextureIdentifier, TextureHost* aTextureHost);
};

// We're using resources owned by our texture as the front buffer.
// Upload the changed region and then return the surface back to
// the renderer.
class ContentHostTexture : public ContentHost
{
public:
  ContentHostTexture(Compositor* aCompositor)
    : ContentHost(aCompositor)
  {}

  virtual BufferType GetType() { return BUFFER_THEBES; }

  virtual void UpdateThebes(const TextureIdentifier& aTextureIdentifier,
                            const ThebesBuffer& aNewBack,
                            const nsIntRegion& aUpdated,
                            OptionalThebesBuffer* aNewFront,
                            const nsIntRegion& aOldValidRegionFront,
                            const nsIntRegion& aOldValidRegionBack,
                            OptionalThebesBuffer* aNewBackResult,
                            nsIntRegion* aNewValidRegionFront,
                            nsIntRegion* aUpdatedRegionBack);

  virtual void AddTextureHost(const TextureIdentifier& aTextureIdentifier, TextureHost* aTextureHost);
};

}
}

#endif