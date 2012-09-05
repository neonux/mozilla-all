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
ContentClientBasic::CreateBuffer(ContentType aType,
                                 const nsIntSize& aSize,
                                 PRUint32 aFlags)
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
ContentClientRemote::CreateBuffer(ContentType aType,
                                  const nsIntSize& aSize,
                                  PRUint32 aFlags)
{
  NS_ABORT_IF_FALSE(!mIsNewBuffer,
                    "Bad! Did we create a buffer twice without painting?");

  mIsNewBuffer = true;

  mOldTextures.AppendElement(mTextureClient);
  mTextureClient = static_cast<TextureClientShmem*>(
    mLayerForwarder->CreateTextureClientFor(TEXTURE_SHMEM, GetType(),
                                            mLayer, NoFlags, true).drop());
  mTextureClient->EnsureTextureClient(gfx::IntSize(aSize.width, aSize.height),
                                      aType);
  return mTextureClient->LockSurface();
}

void
ContentClientRemote::BeginPaint()
{
  if (mTextureClient) {
    SetBuffer(mTextureClient->LockSurface());
  }
}

void
ContentClientRemote::EndPaint()
{
  SetBuffer(nullptr);
  if (mTextureClient) {
    mTextureClient->Unlock();
  }
  mOldTextures.Clear();
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
  NS_ABORT_IF_FALSE(!mTextureClient, "should have a back buffer by now");

  return updatedRegion;
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

void
ContentClientDirect::SetBackBufferAndAttrs(const TextureIdentifier& aTextureIdentifier,
                                           const OptionalThebesBuffer& aBuffer,
                                           const nsIntRegion& aValidRegion,
                                           const OptionalThebesBuffer& aReadOnlyFrontBuffer,
                                           const nsIntRegion& aFrontUpdatedRegion,
                                           nsIntRegion& aLayerValidRegion)
{
  if (OptionalThebesBuffer::Tnull_t == aBuffer.type()) {
    mTextureClient->Descriptor() = SurfaceDescriptor();
  } else {
    mTextureClient->Descriptor() = aBuffer.get_ThebesBuffer().buffer();
  }

  MOZ_ASSERT(OptionalThebesBuffer::Tnull_t != aReadOnlyFrontBuffer.type());

  mFrontAndBackBufferDiffer = true;
  mROFrontBuffer = aReadOnlyFrontBuffer;
  mFrontUpdatedRegion = aFrontUpdatedRegion;
}

struct AutoTextureClient {
  AutoTextureClient()
    : mTexture(nullptr)
  {}
  ~AutoTextureClient()
  {
    if (mTexture) {
      mTexture->Unlock();
    }
  }
  gfxASurface* GetSurface(TextureClientShmem* aTexture)
  {
    mTexture = aTexture;
    return mTexture->LockSurface();
  }

private:
  TextureClientShmem* mTexture;
};

void
ContentClientDirect::SyncFrontBufferToBackBuffer()
{
  if (!mFrontAndBackBufferDiffer) {
    return;
  }

  gfxASurface* backBuffer = GetBuffer();

  if (!mTextureClient) {
    MOZ_ASSERT(!backBuffer);
    MOZ_ASSERT(mROFrontBuffer.type() == OptionalThebesBuffer::TThebesBuffer);
    const ThebesBuffer roFront = mROFrontBuffer.get_ThebesBuffer();
    AutoOpenSurface roFrontBuffer(OPEN_READ_ONLY, roFront.buffer());
    nsIntSize size = roFrontBuffer.Size();
    mTextureClient = static_cast<TextureClientShmem*>(
      mLayerForwarder->CreateTextureClientFor(TEXTURE_SHMEM, GetType(),
                                              mLayer, NoFlags, true).drop());
    mTextureClient->EnsureTextureClient(gfx::IntSize(size.width, size.height),
                                        roFrontBuffer.ContentType());
  }

  AutoTextureClient autoTexture;
  if (!backBuffer) {
    backBuffer = autoTexture.GetSurface(mTextureClient);
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
    mTextureClient->Descriptor() = SurfaceDescriptor();
  } else {
    mTextureClient->Descriptor() = aBuffer.get_ThebesBuffer().buffer();
    gfxASurface* backBuffer = GetBuffer();
    if (!backBuffer) {
      backBuffer = mTextureClient->LockSurface();
      mTextureClient->Unlock();
    }

    SetBuffer(backBuffer,
              aBuffer.get_ThebesBuffer().rect(),
              aBuffer.get_ThebesBuffer().rotation());
  }
  mIsNewBuffer = false;
  aLayerValidRegion = aValidRegion;
}

void
ContentClientTexture::SyncFrontBufferToBackBuffer()
{
  MOZ_ASSERT(mTextureClient);
  MOZ_ASSERT(GetBuffer());
  MOZ_ASSERT(!mIsNewBuffer);
}


}
}
