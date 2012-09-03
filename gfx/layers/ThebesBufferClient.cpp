/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ThebesBufferClient.h"
#include "BasicThebesLayer.h"
#include "nsIWidget.h"
#include "gfxUtils.h"

namespace mozilla {
namespace layers {

//TODO[nrc] AutoBufferTracker comment
/**
 * AutoOpenBuffer is a helper that builds on top of AutoOpenSurface,
 * which we need to get a gfxASurface from a SurfaceDescriptor.  For
 * other layer types, simple lexical scoping of AutoOpenSurface is
 * easy.  For ThebesLayers, the lifetime of buffer mappings doesn't
 * exactly match simple lexical scopes, so naively putting
 * AutoOpenSurfaces on the stack doesn't always work.  We use this
 * helper to track openings instead.
 *
 * Any surface that's opened while painting this ThebesLayer will
 * notify this helper and register itself for unmapping.
 *
 * We ignore buffer destruction here because the shadow layers
 * protocol already ensures that destroyed buffers stay alive until
 * end-of-transaction.
 */

ContentClientBasic::ContentClientBasic(BasicLayerManager* aManager)
  : mManager(aManager)
{}


already_AddRefed<gfxASurface>
ContentClientBasic::CreateBuffer(ContentType aType, const nsIntSize& aSize, PRUint32 aFlags)
{
  nsRefPtr<gfxASurface> referenceSurface = GetBuffer();
  if (!referenceSurface) {
    gfxContext* defaultTarget = mManager->GetDefaultTarget();
    if (defaultTarget) {
      referenceSurface = defaultTarget->CurrentSurface();
    } else {
      nsIWidget* widget = mManager->GetRetainerWidget();
      if (widget) {
        referenceSurface = widget->GetThebesSurface();
      } else {
        referenceSurface = mManager->GetTarget()->CurrentSurface();
      }
    }
  }
  return referenceSurface->CreateSimilarSurface(
    aType, gfxIntSize(aSize.width, aSize.height));
}

already_AddRefed<gfxASurface>
ContentClientRemote::CreateBuffer(ContentType aType, const nsIntSize& aSize, PRUint32 aFlags)
{
  if (IsSurfaceDescriptorValid(mBackBuffer)) {
    //TODO[nrc] need to move to the TextureClient and send the message properly
    //mLayerForwarder->DestroyedThebesBuffer(mLayerForwarder->Hold(this),
    //                                mBackBuffer);
    mBackBuffer = SurfaceDescriptor();
  }

  AllocBackBuffer(aType, aSize);

  NS_ABORT_IF_FALSE(!mIsNewBuffer,
                    "Bad! Did we create a buffer twice without painting?");

  mIsNewBuffer = true;

  Maybe<AutoOpenSurface>* surface = mNewBuffers.AppendElement();
  surface->construct(OPEN_READ_WRITE, mBackBuffer);
  nsRefPtr<gfxASurface> buffer =  surface->ref().Get();

  return buffer.forget();
}

void
ContentClientRemote::BeginPaint()
{
  if (IsSurfaceDescriptorValid(mBackBuffer)) {
    mInitialBuffer.construct(OPEN_READ_WRITE, mBackBuffer);
    SetBuffer(mInitialBuffer.ref().Get());
  }
}

void
ContentClientRemote::EndPaint()
{
  SetBuffer(nullptr);
  mInitialBuffer.destroyIfConstructed();
  mNewBuffers.Clear();
}

nsIntRegion 
ContentClientRemote::GetUpdatedRegion(const nsIntRegion& aRegionToDraw,
                                      const nsIntRegion& aVisibleRegion,
                                      bool aDidSelfCopy)
{
  nsIntRegion updatedRegion;
  if (mIsNewBuffer || aDidSelfCopy) {
    // A buffer reallocation clears both buffers. The front buffer has all the
    // content by now, but the back buffer is still clear. Here, in effect, we
    // are saying to copy all of the pixels of the front buffer to the back.
    // Also when we self-copied in the buffer, the buffer space
    // changes and some changed buffer content isn't reflected in the
    // draw or invalidate region (on purpose!).  When this happens, we
    // need to read back the entire buffer too.
    updatedRegion = aVisibleRegion;
    mIsNewBuffer = false;
  } else {
    updatedRegion = aRegionToDraw;
  }

  NS_ASSERTION(BufferRect().Contains(aRegionToDraw.GetBounds()),
               "Update outside of buffer rect!");
  NS_ABORT_IF_FALSE(IsSurfaceDescriptorValid(mBackBuffer),
                    "should have a back buffer by now");

  return updatedRegion;
}

void
ContentClientRemote::AllocBackBuffer(ContentType aType,
                                     const nsIntSize& aSize)
{
  // This function must *not* open the buffer it allocates.
  if (!mLayerForwarder->AllocBuffer(gfxIntSize(aSize.width, aSize.height),
                                    aType,
                                    &mBackBuffer)) {
    enum { buflen = 256 };
    char buf[buflen];
    PR_snprintf(buf, buflen,
                "creating ThebesLayer 'back buffer' failed! width=%d, height=%d, type=%x",
                aSize.width, aSize.height, int(aType));
    NS_RUNTIMEABORT(buf);
  }
}

void
ContentClientDirect::SetBackBufferAndAttrs(const TextureIdentifier& aTextureIdentifier,
                                           const OptionalThebesBuffer& aBuffer,
                                           const nsIntRegion& aValidRegion,
                                           const OptionalThebesBuffer& aReadOnlyFrontBuffer,
                                           const nsIntRegion& aFrontUpdatedRegion,
                                           nsIntRegion& aLayerValidRegion)
{
  if (OptionalThebesBuffer::Tnull_t == aBuffer.type()) {
    mBackBuffer = SurfaceDescriptor();
  } else {
    mBackBuffer = aBuffer.get_ThebesBuffer().buffer();
  }

  MOZ_ASSERT(OptionalThebesBuffer::Tnull_t != aReadOnlyFrontBuffer.type());

  mFrontAndBackBufferDiffer = true;
  mROFrontBuffer = aReadOnlyFrontBuffer;
  mFrontUpdatedRegion = aFrontUpdatedRegion;
}

void
ContentClientTexture::SetBackBufferAndAttrs(const TextureIdentifier& aTextureIdentifier,
                                            const OptionalThebesBuffer& aBuffer,
                                            const nsIntRegion& aValidRegion,
                                            const OptionalThebesBuffer& aReadOnlyFrontBuffer,
                                            const nsIntRegion& aFrontUpdatedRegion,
                                            nsIntRegion& aLayerValidRegion)
{
  // We shouldn't get back a read-only ref to our old back buffer (the
  // parent's new front buffer).  If the parent is pushing updates
  // to a texture it owns, then we probably got back the same buffer
  // we pushed in the update and all is well.  If not, ...
  MOZ_ASSERT(OptionalThebesBuffer::Tnull_t == aReadOnlyFrontBuffer.type());

  if (OptionalThebesBuffer::Tnull_t == aBuffer.type()) {
    mBackBuffer = SurfaceDescriptor();
  } else {
    mBackBuffer = aBuffer.get_ThebesBuffer().buffer();
    gfxASurface* backBuffer = GetBuffer();
    if (!backBuffer) {
      Maybe<AutoOpenSurface> autoBackBuffer;
      autoBackBuffer.construct(OPEN_READ_WRITE, mBackBuffer);
      backBuffer = autoBackBuffer.ref().Get();
    }

    SetBuffer(backBuffer,
              aBuffer.get_ThebesBuffer().rect(), 
              aBuffer.get_ThebesBuffer().rotation());
  }
  mIsNewBuffer = false;
  aLayerValidRegion = aValidRegion;
}

void
ContentClientDirect::SyncFrontBufferToBackBuffer()
{
  if (!mFrontAndBackBufferDiffer) {
    return;
  }

  gfxASurface* backBuffer = GetBuffer();

  if (!IsSurfaceDescriptorValid(mBackBuffer)) {
    MOZ_ASSERT(!backBuffer);
    MOZ_ASSERT(mROFrontBuffer.type() == OptionalThebesBuffer::TThebesBuffer);
    const ThebesBuffer roFront = mROFrontBuffer.get_ThebesBuffer();
    AutoOpenSurface roFrontBuffer(OPEN_READ_ONLY, roFront.buffer());
    AllocBackBuffer(roFrontBuffer.ContentType(), roFrontBuffer.Size());
  }

  Maybe<AutoOpenSurface> autoBackBuffer;
  if (!backBuffer) {
    autoBackBuffer.construct(OPEN_READ_WRITE, mBackBuffer);
    backBuffer = autoBackBuffer.ref().Get();
  }

  MOZ_LAYERS_LOG(("BasicShadowableThebes(%p): reading back <x=%d,y=%d,w=%d,h=%d>",
                  this,
                  mFrontUpdatedRegion.GetBounds().x,
                  mFrontUpdatedRegion.GetBounds().y,
                  mFrontUpdatedRegion.GetBounds().width,
                  mFrontUpdatedRegion.GetBounds().height));

  const ThebesBuffer roFront = mROFrontBuffer.get_ThebesBuffer();
  AutoOpenSurface autoROFront(OPEN_READ_ONLY, roFront.buffer());
  SetBackingBufferAndUpdateFrom(backBuffer,
                                autoROFront.Get(),
                                roFront.rect(),
                                roFront.rotation(),
                                mFrontUpdatedRegion);
  mIsNewBuffer = false;
  mFrontAndBackBufferDiffer = false;
}

void
ContentClientTexture::SyncFrontBufferToBackBuffer()
{
  MOZ_ASSERT(IsSurfaceDescriptorValid(mBackBuffer));
  MOZ_ASSERT(GetBuffer());
  MOZ_ASSERT(!mIsNewBuffer);
}

void
ContentClientDirect::SetBackingBufferAndUpdateFrom(gfxASurface* aBuffer,
                                                   gfxASurface* aSource,
                                                   const nsIntRect& aRect,
                                                   const nsIntPoint& aRotation,
                                                   const nsIntRegion& aUpdateRegion)
{
  SetBackingBuffer(aBuffer, aRect, aRotation);
  nsRefPtr<gfxContext> destCtx =
    GetContextForQuadrantUpdate(aUpdateRegion.GetBounds());
  destCtx->SetOperator(gfxContext::OPERATOR_SOURCE);
  if (IsClippingCheap(destCtx, aUpdateRegion)) {
    gfxUtils::ClipToRegion(destCtx, aUpdateRegion);
  }

  ContentClientDirect srcBuffer(aSource, aRect, aRotation);
  srcBuffer.DrawBufferWithRotation(destCtx, 1.0, nullptr, nullptr);
}

void
ContentClientRemote::SetBackingBuffer(gfxASurface* aBuffer,
                                      const nsIntRect& aRect,
                                      const nsIntPoint& aRotation)
{
  gfxIntSize prevSize = gfxIntSize(BufferRect().width, BufferRect().height);
  gfxIntSize newSize = aBuffer->GetSize();
  NS_ABORT_IF_FALSE(newSize == prevSize,
                    "Swapped-in buffer size doesn't match old buffer's!");
  nsRefPtr<gfxASurface> oldBuffer;
  oldBuffer = SetBuffer(aBuffer, aRect, aRotation);
}


}
}
