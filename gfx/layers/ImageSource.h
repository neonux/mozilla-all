/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//TODO[nrc] rename the file
#ifndef MOZILLA_GFX_IMAGEHOST_H
#define MOZILLA_GFX_IMAGEHOST_H

#include "ipc/AutoOpenSurface.h"
#include "Compositor.h"

namespace mozilla {
namespace layers {

class YUVImageHost : public ImageHost
{
public:
  YUVImageHost(Compositor* aCompositor)
  {
    mCompositor = aCompositor;
  }

  ~YUVImageHost();

  virtual ImageHostType GetType() { return IMAGE_YUV; }

  // aImage contains all three plains, we could also send them seperately and 
  // update mTextures one at a time
  virtual void UpdateImage(const TextureIdentifier& aTextureIdentifier,
                           const SharedImage& aImage)
  {
    NS_ASSERTION(aTextureIdentifier.mImageType == IMAGE_YUV, "ImageHostType mismatch.");
    
    const YUVImage& yuv = aImage.get_YUVImage();
    mPictureRect = yuv.picture();

    mTextures[0]->Update(SurfaceDescriptor(yuv.Ydata()));
    mTextures[1]->Update(SurfaceDescriptor(yuv.Udata()));
    mTextures[2]->Update(SurfaceDescriptor(yuv.Vdata()));
  }

  virtual void Composite(EffectChain& aEffectChain,
                         float aOpacity,
                         const gfx::Matrix4x4& aTransform,
                         const gfx::Point& aOffset,
                         const gfx::Filter& aFilter,
                         const gfx::Rect& aClipRect)
  {
    mTextures[0]->Lock(aFilter);
    mTextures[1]->Lock(aFilter);
    mTextures[2]->Lock(aFilter);

    EffectYCbCr* effect = new EffectYCbCr(mTextures[0], mTextures[1], mTextures[2], aFilter);
    aEffectChain.mEffects[EFFECT_YCBCR] = effect;
    gfx::Rect rect(0, 0, mPictureRect.width, mPictureRect.height);
    gfx::Rect sourceRect(mPictureRect.x, mPictureRect.y, mPictureRect.width, mPictureRect.height);
    mCompositor->DrawQuad(rect, &sourceRect, &aClipRect, aEffectChain, aOpacity, aTransform, aOffset);

    mTextures[0]->Unlock();
    mTextures[1]->Unlock();
    mTextures[2]->Unlock();
  }

  virtual void AddTextureHost(const TextureIdentifier& aTextureIdentifier, TextureHost* aTextureHost)
  {
    NS_ASSERTION(aTextureIdentifier.mImageType == IMAGE_YUV, "ImageHostType mismatch.");
    mTextures[aTextureIdentifier.mDescriptor] = aTextureHost;
  }

private:
  RefPtr<TextureHost> mTextures[3];
  RefPtr<Compositor> mCompositor;
  nsIntRect mPictureRect;
};

//TODO[nrc] we should be able to get rid of this - the ImageHost should be pulled out of the compositor
static ImageHostType
ImageHostTypeForSharedImage(const SharedImage& aImage)
{
  if (aImage.type() == SharedImage::TSurfaceDescriptor) {
    SurfaceDescriptor surface = aImage.get_SurfaceDescriptor();
    if (surface.type() == SurfaceDescriptor::TSharedTextureDescriptor) {
      return IMAGE_SHARED;
    }

    return IMAGE_TEXTURE;
  }

  return IMAGE_YUV;
}

}
}

#endif
