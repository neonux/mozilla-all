/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CompositorOGL.h"
#include "TextureOGL.h"

namespace mozilla {
namespace layers {

TextureHostIdentifier
CompositorOGL::GetTextureHostIdentifier()
{
  // TODO: Implement this.

  return TextureHostIdentifier();
}

TemporaryRef<Texture>
CompositorOGL::CreateTextureForData(const gfx::IntSize &aSize, PRInt8 *aData, PRUint32 aStride,
                                    TextureFormat aFormat)
{
  // TODO: Implement this.

  return new TextureOGL();
}

TemporaryRef<DrawableTextureHost>
CompositorOGL::CreateDrawableTexture(const TextureIdentifier &aIdentifier)
{
  // TODO: Implement this.

  return new DrawableTextureHostOGL();
}

void
CompositorOGL::DrawQuad(const gfx::Rect &aRect, const gfx::Rect *aSourceRect,
                        const gfx::Rect *aClipRect, const EffectChain &aEffectChain,
                        const gfx::Matrix3x3 &aTransform)
{
  // TODO: Implement this.
}

} /* layers */
} /* mozilla */
