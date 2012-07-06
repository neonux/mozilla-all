/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_TEXTUREOGL_H
#define MOZILLA_GFX_TEXTUREOGL_H

#include "Compositor.h"
#include "TiledThebesLayerOGL.h"

namespace mozilla {

namespace layers {

class TextureOGL : public Texture
{
  // TODO: Make a better version of TextureOGL.
  // TODO: Release the GL texture on destruction.
public:
  TiledTexture mTexture;
  gfx::IntSize mSize;
  GLenum format;
  GLenum internalFormat;
};

class DrawableTextureHostOGL : public DrawableTextureHost
{
  virtual TextureIdentifier GetIdentifierForProcess(base::ProcessHandle aProcess) MOZ_OVERRIDE;

  virtual void PrepareForRendering() MOZ_OVERRIDE;
};

}
}

#endif /* MOZILLA_GFX_TEXTUREOGL_H */
