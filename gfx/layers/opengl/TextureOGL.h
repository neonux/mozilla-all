/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_TEXTUREOGL_H
#define MOZILLA_GFX_TEXTUREOGL_H

#include "CompositorOGL.h"
#include "TiledThebesLayerOGL.h"

namespace mozilla {

namespace layers {

class TextureOGL : public Texture
{
  // TODO: Make a better version of TextureOGL.
  // TODO: Release the GL texture on destruction.
public:
  GLuint mTextureHandle;
  gfx::IntSize mSize;
  GLenum mFormat;
  GLenum mInternalFormat;
  GLenum mType;
  PRUint32 mPixelSize;
  RefPtr<CompositorOGL> mCompositorOGL;
  GLenum mWrapMode;

  virtual void
    UpdateTexture(const nsIntRegion& aRegion, PRInt8 *aData, PRUint32 aStride) MOZ_OVERRIDE;
};


}
}

#endif /* MOZILLA_GFX_TEXTUREOGL_H */
