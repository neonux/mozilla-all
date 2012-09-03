/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_ContentClient_H
#define MOZILLA_GFX_ContentClient_H

#include "mozilla/layers/LayersSurfaces.h"
#include "Compositor.h"
#include "TextureClient.h"
#include "BasicBuffers.h"
#include "mozilla/Attributes.h"

namespace mozilla {
namespace layers {

class BasicLayerManager;

class ContentClient : public RefCounted<ContentClient>, protected ThebesLayerBuffer
{
public:
  ContentClient()
    : ThebesLayerBuffer(ContainsVisibleBounds)
  {}
  virtual ~ContentClient() {}
  //TODO[nrc]
  //virtual SharedImage GetAsSharedImage() = 0;

  // returns false if this is the wrong kind of ImageClient for aContainer
  // note returning true does not necessarily imply success
  //virtual bool Update(ThebesLayer* aLayer) = 0;

  //virtual void SetBuffer(const TextureIdentifier& aTextureIdentifier,
  //                       const SharedImage& aBuffer) = 0;

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
  virtual void SyncFrontBufferToBackBuffer() = 0;

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

class ContentClientRemote : public ContentClient
{
  using ThebesLayerBuffer::BufferRect;
  using ThebesLayerBuffer::BufferRotation;
public:
  ContentClientRemote(ShadowLayerForwarder* aLayerForwarder)
    : mLayerForwarder(aLayerForwarder)
  {}

  ~ContentClientRemote()
  {
    if (IsSurfaceDescriptorValid(mBackBuffer)) {
      mLayerForwarder->DestroySharedSurface(&mBackBuffer);
    }
  }

  virtual already_AddRefed<gfxASurface> CreateBuffer(ContentType aType,
                                                     const nsIntSize& aSize,
                                                     PRUint32 aFlags);

  virtual void BeginPaint();
  virtual void EndPaint();

  virtual nsIntRegion GetUpdatedRegion(const nsIntRegion& aRegionToDraw,
                                       const nsIntRegion& aVisibleRegion,
                                       bool aDidSelfCopy);

  ThebesBuffer GetAsThebesBuffer()
  {
    return ThebesBuffer(mBackBuffer, BufferRect(), BufferRotation());
  }

protected:
  /**
   * Swap out the old backing buffer for |aBuffer| and attributes.
   */
  void SetBackingBuffer(gfxASurface* aBuffer,
                        const nsIntRect& aRect, const nsIntPoint& aRotation);

  // This function must *not* open the buffer it allocates.
  void AllocBackBuffer(ContentType aType, const nsIntSize& aSize);

  ShadowLayerForwarder* mLayerForwarder;

  // This describes the gfxASurface we hand to mBuffer.  We keep a
  // copy of the descriptor here so that we can call
  // DestroySharedSurface() on the descriptor.
  SurfaceDescriptor mBackBuffer;

  //TODO[nrc] comment, AutoBufferTracker things
  Maybe<AutoOpenSurface> mInitialBuffer;
  nsAutoTArray<Maybe<AutoOpenSurface>, 2> mNewBuffers;

  bool mIsNewBuffer;
};

//both have back buffers, only one (which?) has a font buffer

class ContentClientDirect : public ContentClientRemote
{
public:
  ContentClientDirect(ShadowLayerForwarder* aLayerForwarder)
    : ContentClientRemote(aLayerForwarder)
  {}

  virtual void SetBackBufferAndAttrs(const TextureIdentifier& aTextureIdentifier,
                                     const OptionalThebesBuffer& aBuffer,
                                     const nsIntRegion& aValidRegion,
                                     const OptionalThebesBuffer& aReadOnlyFrontBuffer,
                                     const nsIntRegion& aFrontUpdatedRegion,
                                     nsIntRegion& aLayerValidRegion);

  virtual void SyncFrontBufferToBackBuffer();

private:
  ContentClientDirect(gfxASurface* aBuffer,
                      const nsIntRect& aRect, const nsIntPoint& aRotation)
    // The size policy doesn't really matter here; this constructor is
    // intended to be used for creating temporaries
    : ContentClientRemote(nullptr)
  {
    SetBuffer(aBuffer, aRect, aRotation);
  }

  void SetBackingBufferAndUpdateFrom(
    gfxASurface* aBuffer,
    gfxASurface* aSource, const nsIntRect& aRect, const nsIntPoint& aRotation,
    const nsIntRegion& aUpdateRegion);

  bool mFrontAndBackBufferDiffer;
  OptionalThebesBuffer mROFrontBuffer;
  nsIntRegion mFrontUpdatedRegion;
};

class ContentClientTexture : public ContentClientRemote
{
public:
  ContentClientTexture(ShadowLayerForwarder* aLayerForwarder)
    : ContentClientRemote(aLayerForwarder)
  {}

  virtual void SetBackBufferAndAttrs(const TextureIdentifier& aTextureIdentifier,
                                     const OptionalThebesBuffer& aBuffer,
                                     const nsIntRegion& aValidRegion,
                                     const OptionalThebesBuffer& aReadOnlyFrontBuffer,
                                     const nsIntRegion& aFrontUpdatedRegion,
                                     nsIntRegion& aLayerValidRegion);

  virtual void SyncFrontBufferToBackBuffer(); 
};

// thin wrapper around BasicThebesLayerBuffer, for on-mtc
class ContentClientBasic : public ContentClient
{
public:
  ContentClientBasic(BasicLayerManager* aManager);

  virtual already_AddRefed<gfxASurface> CreateBuffer(ContentType aType, const nsIntSize& aSize, PRUint32 aFlags);

  virtual void SyncFrontBufferToBackBuffer() {}
private:
  nsRefPtr<BasicLayerManager> mManager;
};

}
}

#endif
