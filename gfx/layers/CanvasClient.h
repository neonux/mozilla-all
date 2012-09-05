/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_CANVASCLIENT_H
#define MOZILLA_GFX_CANVASCLIENT_H

#include "ImageClient.h"

namespace mozilla {

namespace layers {

class BasicCanvasLayer;
class TextureClientShmem;

class CanvasClient : public RefCounted<CanvasClient>
{
public:
  virtual ~CanvasClient() {}

  // returns false if this is the wrong kind of ImageClient for aContainer
  // note returning true does not necessarily imply success
  virtual void Update(gfx::IntSize aSize, BasicCanvasLayer* aLayer) = 0;

  virtual void SetBuffer(const TextureIdentifier& aTextureIdentifier,
                         const SharedImage& aBuffer);
protected:
  RefPtr<TextureClient> mTextureClient;
};

class CanvasClientTexture : public CanvasClient
{
public:
  CanvasClientTexture(ShadowLayerForwarder* aLayerForwarder,
                      ShadowableLayer* aLayer,
                      TextureFlags aFlags);

  virtual void Update(gfx::IntSize aSize, BasicCanvasLayer* aLayer);
};

class CanvasClientShared : public CanvasClient
{
public:
  CanvasClientShared(ShadowLayerForwarder* aLayerForwarder,
                     ShadowableLayer* aLayer,
                     TextureFlags aFlags);

  virtual void Update(gfx::IntSize aSize, BasicCanvasLayer* aLayer);
};

}
}

#endif
