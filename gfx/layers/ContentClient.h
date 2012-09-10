/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_CONTENTCLIENT_H
#define MOZILLA_GFX_CONTENTCLIENT_H

#include "mozilla/layers/LayersSurfaces.h"
#include "BufferClient.h"
#include "TextureClient.h"
#include "BasicBuffers.h"
#include "mozilla/Attributes.h"

namespace mozilla {
namespace layers {

class BasicLayerManager;

class ContentClient : public BufferClient, protected ThebesLayerBuffer
{
public:
  ContentClient()
    : ThebesLayerBuffer(ContainsVisibleBounds)
  {}
  virtual ~ContentClient() {}

  typedef ThebesLayerBuffer::PaintState PaintState;
  typedef ThebesLayerBuffer::ContentType ContentType;

  virtual void Clear() { ThebesLayerBuffer::Clear(); }
  virtual PaintState BeginPaint(ThebesLayer* aLayer, ContentType aContentType,
                                PRUint32 aFlags)
  { return ThebesLayerBuffer::BeginPaint(aLayer, aContentType, aFlags); }
  virtual void DrawTo(ThebesLayer* aLayer, gfxContext* aTarget, float aOpacity,
                      gfxASurface* aMask, const gfxMatrix* aMaskTransform)
  { ThebesLayerBuffer::DrawTo(aLayer, aTarget, aOpacity, aMask, aMaskTransform); }

  // Sync front/back buffers content
  // After executing, the new back buffer has the same (interesting) pixels as the
  // new front buffer, and mValidRegion et al. are correct wrt the new
  // back buffer (i.e. as they were for the old back buffer)
  virtual void SyncFrontBufferToBackBuffer() {}

  virtual void SetBackBufferAndAttrs(const TextureIdentifier& aTextureIdentifier,
                                     const OptionalThebesBuffer& aBuffer,
                                     const nsIntRegion& aValidRegion,
                                     const OptionalThebesBuffer& aReadOnlyFrontBuffer,
                                     const nsIntRegion& aFrontUpdatedRegion,
                                     nsIntRegion& aLayerValidRegion) {}

  //TODO[nrc] comment for MapBuffer/UnmapBuffer
  /**
   * When BasicThebesLayerBuffer is used with layers that hold
   * SurfaceDescriptor, this buffer only has a valid gfxASurface in
   * the scope of an AutoOpenSurface for that SurfaceDescriptor.  That
   * is, it's sort of a "virtual buffer" that's only mapped and
   * unmapped within the scope of AutoOpenSurface.  None of the
   * underlying buffer attributes (rect, rotation) are affected by
   * mapping/unmapping.
   *
   * These helpers just exist to provide more descriptive names of the
   * map/unmap process.
   */
  virtual void BeginPaint() {}
  virtual void EndPaint() {}
};

// thin wrapper around BasicThebesLayerBuffer, for on-mtc
class ContentClientBasic : public ContentClient
{
public:
  ContentClientBasic(BasicLayerManager* aManager);

  virtual already_AddRefed<gfxASurface> CreateBuffer(ContentType aType,
                                                     const nsIntSize& aSize,
                                                     PRUint32 aFlags);
private:
  nsRefPtr<BasicLayerManager> mManager;
};

class ContentClientRemote : public ContentClient
{
  using ThebesLayerBuffer::BufferRect;
  using ThebesLayerBuffer::BufferRotation;
public:
  ContentClientRemote(ShadowLayerForwarder* aLayerForwarder,
                      ShadowableLayer* aLayer,
                      TextureFlags aFlags)
    : mLayerForwarder(aLayerForwarder)
    , mLayer(aLayer)
    , mTextureClient(nullptr)
  {}

  virtual already_AddRefed<gfxASurface> CreateBuffer(ContentType aType,
                                                     const nsIntSize& aSize,
                                                     PRUint32 aFlags);

  virtual void BeginPaint();
  virtual void EndPaint();

  virtual BufferType GetType() = 0;

  virtual void Updated(ShadowableLayer* aLayer,
                       const nsIntRegion& aRegionToDraw,
                       const nsIntRegion& aVisibleRegion,
                       bool aDidSelfCopy)
  {
    nsIntRegion updatedRegion = GetUpdatedRegion(aRegionToDraw,
                                                 aVisibleRegion,
                                                 aDidSelfCopy);

    NS_ASSERTION(mTextureClient, "No texture client?!");
    mTextureClient->UpdatedRegion(aLayer,
                                  updatedRegion,
                                  BufferRect(),
                                  BufferRotation());
  }

protected:
  /**
   * Swap out the old backing buffer for |aBuffer| and attributes.
   */
  void SetBackingBuffer(gfxASurface* aBuffer,
                        const nsIntRect& aRect, const nsIntPoint& aRotation);

  virtual nsIntRegion GetUpdatedRegion(const nsIntRegion& aRegionToDraw,
                                       const nsIntRegion& aVisibleRegion,
                                       bool aDidSelfCopy);

  ShadowLayerForwarder* mLayerForwarder;
  ShadowableLayer* mLayer;

  RefPtr<TextureClient> mTextureClient;
  // keep a record of texture clients we have created and need to keep around, then unlock
  nsTArray<RefPtr<TextureClient>> mOldTextures;

  bool mIsNewBuffer;
};

class ContentClientDirect : public ContentClientRemote
{
public:
  ContentClientDirect(ShadowLayerForwarder* aLayerForwarder,
                      ShadowableLayer* aLayer,
                      TextureFlags aFlags)
    : ContentClientRemote(aLayerForwarder, aLayer, aFlags)
  {}

  virtual already_AddRefed<gfxASurface> CreateBuffer(ContentType aType,
                                                     const nsIntSize& aSize,
                                                     PRUint32 aFlags)
  {
    return ContentClientRemote::CreateBuffer(aType, aSize, aFlags);
  }

  virtual void SetBackBufferAndAttrs(const TextureIdentifier& aTextureIdentifier,
                                     const OptionalThebesBuffer& aBuffer,
                                     const nsIntRegion& aValidRegion,
                                     const OptionalThebesBuffer& aReadOnlyFrontBuffer,
                                     const nsIntRegion& aFrontUpdatedRegion,
                                     nsIntRegion& aLayerValidRegion);

  virtual void SyncFrontBufferToBackBuffer();

  virtual BufferType GetType() { return BUFFER_DIRECT; }

private:
  ContentClientDirect(gfxASurface* aBuffer,
                      const nsIntRect& aRect, const nsIntPoint& aRotation)
    // The size policy doesn't really matter here; this constructor is
    // intended to be used for creating temporaries
    : ContentClientRemote(nullptr, nullptr, NoFlags)
  {
    SetBuffer(aBuffer, aRect, aRotation);
  }

  void SetBackingBufferAndUpdateFrom(gfxASurface* aBuffer,
                                     gfxASurface* aSource,
                                     const nsIntRect& aRect,
                                     const nsIntPoint& aRotation,
                                     const nsIntRegion& aUpdateRegion);

  bool mFrontAndBackBufferDiffer;
  OptionalThebesBuffer mROFrontBuffer;
  nsIntRegion mFrontUpdatedRegion;
};

class ContentClientTexture : public ContentClientRemote
{
public:
  ContentClientTexture(ShadowLayerForwarder* aLayerForwarder,
                       ShadowableLayer* aLayer,
                       TextureFlags aFlags)
    : ContentClientRemote(aLayerForwarder, aLayer, aFlags)
  {}

  virtual void SetBackBufferAndAttrs(const TextureIdentifier& aTextureIdentifier,
                                     const OptionalThebesBuffer& aBuffer,
                                     const nsIntRegion& aValidRegion,
                                     const OptionalThebesBuffer& aReadOnlyFrontBuffer,
                                     const nsIntRegion& aFrontUpdatedRegion,
                                     nsIntRegion& aLayerValidRegion);

  virtual void SyncFrontBufferToBackBuffer(); 

  virtual BufferType GetType() { return BUFFER_TEXTURE; }
};

}
}

#endif
