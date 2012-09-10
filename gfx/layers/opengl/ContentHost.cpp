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

TextureImage*
ContentHost::GetTextureImage()
{
  return static_cast<TextureImageAsTextureHost*>(mTextureHost.get())->GetTextureImage();
}

void
ContentHostTexture::UpdateThebes(const TextureIdentifier& aTextureIdentifier,
                                 const ThebesBuffer& aNewFront,
                                 const nsIntRegion& aUpdated,
                                 OptionalThebesBuffer* aNewBack,
                                 const nsIntRegion& aOldValidRegionFront,
                                 const nsIntRegion& aOldValidRegionBack,
                                 OptionalThebesBuffer* aNewBackResult,
                                 nsIntRegion* aNewValidRegionFront,
                                 nsIntRegion* aUpdatedRegionBack)
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

  mTextureHost->Update(updated, destRegion);
  mInitialised = true;

  mBufferRect = aNewFront.rect();
  mBufferRotation = aNewFront.rotation();

  *aNewBack = aNewFront;
  *aNewValidRegionFront = aOldValidRegionBack;
  *aNewBackResult = null_t();
  aUpdatedRegionBack->SetEmpty();
}

void
ContentHostTexture::AddTextureHost(const TextureIdentifier& aTextureIdentifier, TextureHost* aTextureHost)
{
  NS_ASSERTION(aTextureIdentifier.mBufferType == BUFFER_THEBES &&
               aTextureIdentifier.mTextureType == TEXTURE_SHMEM,
               "BufferType mismatch.");
  mTextureHost = static_cast<TextureImageAsTextureHost*>(aTextureHost);
}

void
ContentHostDirect::UpdateThebes(const TextureIdentifier& aTextureIdentifier,
                                const ThebesBuffer& aNewBack,
                                const nsIntRegion& aUpdated,
                                OptionalThebesBuffer* aNewFront,
                                const nsIntRegion& aOldValidRegionFront,
                                const nsIntRegion& aOldValidRegionBack,
                                OptionalThebesBuffer* aNewBackResult,
                                nsIntRegion* aNewValidRegionFront,
                                nsIntRegion* aUpdatedRegionBack)
{
  mBufferRect = aNewBack.rect();
  mBufferRotation = aNewBack.rotation();

  if (!mTextureHost) {
    *aNewFront = null_t();
    mInitialised = false;

    aNewValidRegionFront->SetEmpty();
    *aNewBackResult = null_t();
    *aUpdatedRegionBack = aUpdated;
    return;
  }

  AutoOpenSurface newBack(OPEN_READ_ONLY, aNewBack.buffer());
  bool needsReset = static_cast<TextureImageAsTextureHostWithBuffer*>(mTextureHost.get())
    ->EnsureBuffer(newBack.Size());

  ThebesBuffer newFront;
  mInitialised = mTextureHost->Update(aNewBack.buffer(), &newFront.buffer());
  newFront.rect() = mBufferRect;
  newFront.rotation() = mBufferRotation;
  *aNewFront = newFront;

  // We have to invalidate the pixels painted into the new buffer.
  // They might overlap with our old pixels.
  aNewValidRegionFront->Sub(needsReset ? nsIntRegion() : aOldValidRegionFront, aUpdated);
  *aNewBackResult = newFront;
  *aUpdatedRegionBack = aUpdated;
}

void
ContentHostDirect::AddTextureHost(const TextureIdentifier& aTextureIdentifier, TextureHost* aTextureHost)
{
  NS_ASSERTION(aTextureIdentifier.mBufferType == BUFFER_DIRECT &&
               aTextureIdentifier.mTextureType == TEXTURE_SHMEM,
               "BufferType mismatch.");
  mTextureHost = static_cast<TextureImageAsTextureHostWithBuffer*>(aTextureHost);
}

}
}