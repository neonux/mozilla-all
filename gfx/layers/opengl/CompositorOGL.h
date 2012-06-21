/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_COMPOSITOROGL_H
#define MOZILLA_GFX_COMPOSITOROGL_H

#include "Compositor.h"

namespace mozilla {

namespace layers {

class CompositorOGL : public Compositor
{
  virtual TextureHostIdentifier GetTextureHostIdentifier() MOZ_OVERRIDE;

  virtual TemporaryRef<Texture>
    CreateTextureForData(const gfx::IntSize &aSize, PRInt8 *aData, PRUint32 aStride,
                         TextureFormat aFormat) MOZ_OVERRIDE;

  virtual TemporaryRef<DrawableTextureHost>
    CreateDrawableTexture(const TextureIdentifier &aIdentifier) MOZ_OVERRIDE;

  virtual void DrawQuad(const gfx::Rect &aRect, const gfx::Rect *aSourceRect,
                        const gfx::Rect *aClipRect, const EffectChain &aEffectChain,
                        const gfx::Matrix3x3 &aTransform) MOZ_OVERRIDE;
};

}
}

#endif /* MOZILLA_GFX_COMPOSITOROGL_H */
