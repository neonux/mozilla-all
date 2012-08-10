/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_IMAGESOURCE_H
#define MOZILLA_GFX_IMAGESOURCE_H

#include "ipc/AutoOpenSurface.h"
#include "Compositor.h"

namespace mozilla {
namespace layers {

template <class TextureImpl, class CompositorImpl>
class YUVImageSource : public ImageSource
{
public:
  YUVImageSource(CompositorImpl* aCompositor)
  {
    mCompositor = aCompositor;
  }

  ~YUVImageSource();

  virtual ImageSourceType GetType() { return IMAGE_YUV; }

  virtual void UpdateImage(const SharedImage& aImage)
  {
    const YUVImage& yuv = aImage.get_YUVImage();
    mPictureRect = yuv.picture();

    AutoOpenSurface asurfY(OPEN_READ_ONLY, yuv.Ydata());
    AutoOpenSurface asurfU(OPEN_READ_ONLY, yuv.Udata());
    AutoOpenSurface asurfV(OPEN_READ_ONLY, yuv.Vdata());

    mTextures[0].Ensure(mCompositor, asurfY.Size());
    mTextures[1].Ensure(mCompositor, asurfU.Size());
    mTextures[2].Ensure(mCompositor, asurfV.Size());

    mTextures[0].Upload(asurfY.GetAsImage());
    mTextures[1].Upload(asurfU.GetAsImage());
    mTextures[2].Upload(asurfV.GetAsImage());
  }

  virtual void Composite(EffectChain& aEffectChain,
                         float aOpacity,
                         const gfx::Matrix4x4& aTransform,
                         const gfx::Point& aOffset,
                         const gfx::Filter aFilter)
  {
    EffectYCbCr* effect = new EffectYCbCr(&mTextures[0], &mTextures[1], &mTextures[2], aFilter);
    aEffectChain.mEffects[EFFECT_YCBCR] = effect;
    gfx::Rect rect(0, 0, mPictureRect.width, mPictureRect.height);
    gfx::Rect sourceRect(mPictureRect.x, mPictureRect.y, mPictureRect.width, mPictureRect.height);
    mCompositor->DrawQuad(rect, &sourceRect, nullptr, aEffectChain, aOpacity, aTransform, aOffset);
  }

private:
  TextureImpl mTextures[3];
  RefPtr<CompositorImpl> mCompositor;
  nsIntRect mPictureRect;
};

//TODO[nrc] we should be able to get rid of this - the ImageSource should be pulled out of the compositor
static ImageSourceType
ImageSourceTypeForSharedImage(const SharedImage& aImage)
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