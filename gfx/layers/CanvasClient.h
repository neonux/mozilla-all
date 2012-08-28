/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_CANVASCLIENT_H
#define MOZILLA_GFX_CANVASCLIENT_H

#include "ImageClient.h"

namespace mozilla {

namespace gl {
class GLContext;
}

namespace layers {

class BasicCanvasLayer;
class TextureClientShmem;

//TODO[nrc] shouldn't need to extend ImageClient
//don't need Updateiamge
class CanvasClient : public ImageClient
{
public:
  virtual ~CanvasClient() {}
  //TODO[nrc] comments
  virtual SharedImage GetAsSharedImage() = 0;

  // returns false if this is the wrong kind of ImageClient for aContainer
  // note returning true does not necessarily imply success
  virtual void Update(gfx::IntSize aSize, BasicCanvasLayer* aLayer) = 0;

  virtual void SetBuffer(const TextureIdentifier& aTextureIdentifier,
                         const SharedImage& aBuffer) = 0;
  virtual bool UpdateImage(ImageContainer* aContainer, ImageLayer* aLayer) {return true;}
};

class CanvasClientTexture : public CanvasClient
{
public:
  CanvasClientTexture(ShadowLayerForwarder* aLayerForwarder,
                      ShadowableLayer* aLayer,
                      TextureFlags aFlags);
  virtual ~CanvasClientTexture() {}

  virtual SharedImage GetAsSharedImage();
  virtual void Update(gfx::IntSize aSize, BasicCanvasLayer* aLayer);
  virtual void SetBuffer(const TextureIdentifier& aTextureIdentifier,
                         const SharedImage& aBuffer);

private:
  RefPtr<TextureClientShmem> mTextureClient;
};

class CanvasClientShared : public CanvasClient
{
public:
  CanvasClientShared(ShadowLayerForwarder* aLayerForwarder,
                     ShadowableLayer* aLayer,
                     TextureFlags aFlags);
  virtual ~CanvasClientShared();

  virtual SharedImage GetAsSharedImage();
  virtual void Update(gfx::IntSize aSize, BasicCanvasLayer* aLayer);

  virtual void SetBuffer(const TextureIdentifier& aTextureIdentifier,
                         const SharedImage& aBuffer);

private:
  SurfaceDescriptor mDescriptor;
  nsRefPtr<gl::GLContext> mGL;
};

}
}

#endif
