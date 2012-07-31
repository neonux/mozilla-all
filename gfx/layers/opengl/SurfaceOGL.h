/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_SURFACEOGL_H
#define MOZILLA_GFX_SURFACEOGL_H

#include "Compositor.h"

namespace mozilla {

namespace layers {

class SurfaceOGL : public Surface
{
  // TODO: Release the GL texture and FBO on destruction.
public:
  gfx::IntSize mSize;
  GLuint mTexture;
  GLuint mFBO;
};


}
}

#endif /* MOZILLA_GFX_SURFACEOGL_H */
