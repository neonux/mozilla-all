/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextureOGL.h"

namespace mozilla {
namespace layers {

static PRUint32
DataOffset(PRUint32 aStride, PRUint32 aPixelSize, const nsIntPoint &aPoint)
{
  unsigned int data = aPoint.y * aStride;
  data += aPoint.x * aPixelSize;
  return data;
}

void
TextureOGL::UpdateTexture(const nsIntRegion& aRegion, PRInt8 *aData, PRUint32 aStride)
{
  if (!mCompositorOGL->SupportsPartialTextureUpdate() ||
      (aRegion.IsEqual(nsIntRect(0, 0, mSize.width, mSize.height)))) {
    mCompositorOGL->gl()->TexImage2D(LOCAL_GL_TEXTURE_2D, 0, mInternalFormat,
                                     mSize.width, mSize.height, aStride, mPixelSize,
                                     0, mFormat, mType, aData);
  } else {
    nsIntRegionRectIterator iter(aRegion);
    const nsIntRect *iterRect;

    nsIntPoint topLeft = aRegion.GetBounds().TopLeft();

    while ((iterRect = iter.Next())) {
      // The inital data pointer is at the top left point of the region's
      // bounding rectangle. We need to find the offset of this rect
      // within the region and adjust the data pointer accordingly.
      PRInt8 *rectData = aData + DataOffset(aStride, mPixelSize, iterRect->TopLeft() - topLeft);
      mCompositorOGL->gl()->TexSubImage2D(LOCAL_GL_TEXTURE_2D,
                                          0,
                                          iterRect->x,
                                          iterRect->y,
                                          iterRect->width,
                                          iterRect->height,
                                          aStride,
                                          mPixelSize,
                                          mFormat,
                                          mType,
                                          rectData);
    }
  }
}

} /* layers */
} /* mozilla */
