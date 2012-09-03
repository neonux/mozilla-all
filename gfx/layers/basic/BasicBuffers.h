/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_BASICBUFFERS_H
#define GFX_BASICBUFFERS_H

#include "ipc/AutoOpenSurface.h"
#include "ipc/ShadowLayerChild.h"
#include "ThebesLayerBuffer.h"

namespace mozilla {
namespace layers {

//TODO[nrc] move to basicthebeslayer.cpp and kill this file

class ShadowThebesLayerBuffer : public ThebesLayerBuffer
{
  typedef ThebesLayerBuffer Base;

public:
  ShadowThebesLayerBuffer()
    : Base(ContainsVisibleBounds)
  {
    MOZ_COUNT_CTOR(ShadowThebesLayerBuffer);
  }

  ~ShadowThebesLayerBuffer()
  {
    MOZ_COUNT_DTOR(ShadowThebesLayerBuffer);
  }

  /**
   * Swap in the old "virtual buffer" (see above) attributes in aNew*
   * and return the old ones in aOld*.
   *
   * Swap() must only be called when the buffer is in its "unmapped"
   * state, that is the underlying gfxASurface is not available.  It
   * is expected that the owner of this buffer holds an unmapped
   * SurfaceDescriptor as the backing storage for this buffer.  That's
   * why no gfxASurface or SurfaceDescriptor parameters appear here.
   */
  void Swap(const nsIntRect& aNewRect, const nsIntPoint& aNewRotation,
            nsIntRect* aOldRect, nsIntPoint* aOldRotation)
  {
    *aOldRect = BufferRect();
    *aOldRotation = BufferRotation();

    nsRefPtr<gfxASurface> oldBuffer;
    oldBuffer = SetBuffer(nullptr, aNewRect, aNewRotation);
    MOZ_ASSERT(!oldBuffer);
  }

  /**
   * When BasicThebesLayerBuffer is used with layers that hold
   * SurfaceDescriptor, this buffer only has a valid gfxASurface in
   * the scope of an AutoOpenSurface for that SurfaceDescriptor.  That
   * is, it's sort of a "virtual buffer" that's only mapped an
   * unmapped within the scope of AutoOpenSurface.  None of the
   * underlying buffer attributes (rect, rotation) are affected by
   * mapping/unmapping.
   *
   * These helpers just exist to provide more descriptive names of the
   * map/unmap process.
   */
  void MapBuffer(gfxASurface* aBuffer)
  {
    SetBuffer(aBuffer);
  }
  void UnmapBuffer()
  {
    SetBuffer(nullptr);
  }
protected:
  virtual already_AddRefed<gfxASurface>
  CreateBuffer(ContentType, const nsIntSize&, PRUint32)
  {
    NS_RUNTIMEABORT("ShadowThebesLayer can't paint content");
    return nullptr;
  }
};

}
}

#endif
