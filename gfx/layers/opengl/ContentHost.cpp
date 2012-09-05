/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ipc/AutoOpenSurface.h"
#include "TextureOGL.h"
#include "mozilla/layers/ShadowLayers.h"
#include "ContentHost.h"

namespace mozilla {
namespace layers {

class ContentHost : public AContentHost, protected CompositingThebesLayerBuffer
{
public:
  typedef uint32_t UpdateFlags;
  static const UpdateFlags RESET_BUFFER = 0x1;
  static const UpdateFlags UPDATE_SUCCESS = 0x2;
  static const UpdateFlags UPDATE_FAIL = 0x4;
  static const UpdateFlags UPDATE_NOSWAP = 0x8;

  ContentHost(Compositor* aCompositor)
    : CompositingThebesLayerBuffer(aCompositor)
  {
    mInitialised = false;
  }

  ~ContentHost()
  {
    Reset();
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

  virtual void AddTextureHost(const TextureIdentifier& aTextureIdentifier, TextureHost* aTextureHost)
  {
    //TODO[nrc]
  }

  // CompositingThebesLayerBuffer implementation
  virtual PaintState BeginPaint(ContentType aContentType, PRUint32) {
    NS_RUNTIMEABORT("can't BeginPaint for a shadow layer");
    return PaintState();
  }

protected:
  virtual nsIntPoint GetOriginOffset() {
    return mBufferRect.TopLeft() - mBufferRotation;
  }

  void Reset() 
  {
    mInitialised = false;
    if (IsSurfaceDescriptorValid(mBufferDescriptor)) {
      mDeAllocator->DestroySharedSurface(&mBufferDescriptor);
    }
  }

  nsIntRect mBufferRect;
  nsIntPoint mBufferRotation;
  //TODO[nrc] should be in TextureHost
  SurfaceDescriptor mBufferDescriptor;
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

  virtual ImageHostType GetType() { return IMAGE_DIRECT; }

  virtual UpdateFlags UpdateThebes(const TextureIdentifier& aTextureIdentifier,
                                   const ThebesBuffer& aNewBack,
                                   const nsIntRegion& aUpdated,
                                   OptionalThebesBuffer* aNewFront);
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

  virtual ImageHostType GetType() { return IMAGE_THEBES; }

  virtual UpdateFlags UpdateThebes(const TextureIdentifier& aTextureIdentifier,
                                   const ThebesBuffer& aNewBack,
                                   const nsIntRegion& aUpdated,
                                   OptionalThebesBuffer* aNewFront);
};


ContentHost::UpdateFlags
ContentHostTexture::UpdateThebes(const TextureIdentifier& aTextureIdentifier,
                                 const ThebesBuffer& aNewFront,
                                 const nsIntRegion& aUpdated,
                                 OptionalThebesBuffer* aNewBack)
{
  AutoOpenSurface surface(OPEN_READ_ONLY, aNewFront.buffer());
  gfxASurface* updated = surface.Get();

  // updated is in screen coordinates. Convert it to buffer coordinates.
  nsIntRegion destRegion(aUpdated);
  destRegion.MoveBy(-aNewFront.rect().TopLeft());

  // Correct for rotation
  destRegion.MoveBy(aNewFront.rotation());
  gfxIntSize size = updated->GetSize();
  nsIntRect destBounds = destRegion.GetBounds();
  destRegion.MoveBy((destBounds.x >= size.width) ? -size.width : 0,
                    (destBounds.y >= size.height) ? -size.height : 0);

  // There's code to make sure that updated regions don't cross rotation
  // boundaries, so assert here that this is the case
  NS_ASSERTION(((destBounds.x % size.width) + destBounds.width <= size.width) &&
               ((destBounds.y % size.height) + destBounds.height <= size.height),
               "Updated region lies across rotation boundaries!");

  // NB: this gfxContext must not escape EndUpdate() below
  mTextureHost->Update(updated, destRegion);
  mInitialised = true;

  mBufferRect = aNewFront.rect();
  mBufferRotation = aNewFront.rotation();

  *aNewBack = aNewFront;
  return UPDATE_NOSWAP;
}

ContentHost::UpdateFlags
ContentHostDirect::UpdateThebes(const TextureIdentifier& aTextureIdentifier,
                                const ThebesBuffer& aNewBack,
                                const nsIntRegion& aUpdated,
                                OptionalThebesBuffer* aNewFront)
{
  UpdateFlags result = 0;
  if (IsSurfaceDescriptorValid(mBufferDescriptor)) {
    AutoOpenSurface currentFront(OPEN_READ_ONLY, mBufferDescriptor);
    AutoOpenSurface newFront(OPEN_READ_ONLY, aNewBack.buffer());
    if (currentFront.Size() != newFront.Size()) {
      // The buffer changed size making us obsolete.
      Reset();
      result |= RESET_BUFFER;
    }
  }

  if (IsSurfaceDescriptorValid(mBufferDescriptor)) {
    ThebesBuffer newFront;
    newFront.buffer() = mBufferDescriptor;
    newFront.rect() = mBufferRect;
    newFront.rotation() = mBufferRotation;
    *aNewFront = newFront;
    result |= UPDATE_SUCCESS;
  } else {
    *aNewFront = null_t();
    result |= UPDATE_FAIL;
  }

  bool success = mTextureHost->UpdateDirect(aNewBack.buffer());
  mBufferDescriptor = aNewBack.buffer();
  mBufferRect = aNewBack.rect();
  mBufferRotation = aNewBack.rotation();

  mInitialised = success;

  return result;
}

}
}