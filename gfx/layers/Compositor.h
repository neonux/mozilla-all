/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_COMPOSITOR_H
#define MOZILLA_GFX_COMPOSITOR_H

#include "mozilla/layers/ImageContainerParent.h"
#include "mozilla/RefPtr.h"
#include "mozilla/gfx/Rect.h"
#include "mozilla/gfx/Matrix.h"
#include "nsAutoPtr.h"
#include "nsRegion.h"

class gfxContext;
class nsIWidget;

namespace mozilla {

namespace gfx {
class DrawTarget;
}

namespace layers {

enum TextureFormat
{
  TEXTUREFORMAT_BGRX32,
  TEXTUREFORMAT_BGRA32,
  TEXTUREFORMAT_BGR16,
  TEXTUREFORMAT_Y8
};

enum ImageSourceType
{
  IMAGE_YUV,
  IMAGE_OGL_SHARED,
  IMAGE_OGL
};

class Compositor;
struct EffectChain;

class Texture : public RefCounted<Texture>
{
public:
  /* aRegion is the region of the Texture to upload to. aData is a pointer to the
   * top-left of the bound of the region to be uploaded. If the compositor that
   * created this texture does not support partial texture upload, aRegion must be
   * equal to this Texture's rect.
   */
  virtual void
    UpdateTexture(const nsIntRegion& aRegion, PRInt8 *aData, PRUint32 aStride) = 0;

  virtual ~Texture() {}
};

class ImageSource : public RefCounted<ImageSource>
{
public:
  virtual ImageSourceType GetType() = 0;

  virtual void UpdateImage(const SharedImage& aImage) = 0;

  virtual void Composite(Compositor* aCompositor,
                         EffectChain& aEffectChain,
                         float aOpacity,
                         const gfx::Matrix4x4* aTransform,
                         const gfx::Point& aOffset,
                         const gfx::Filter aFilter) = 0;

};


/* This can be used as an offscreen rendering target by the compositor, and
 * subsequently can be used as a source by the compositor.
 */
class Surface : public RefCounted<Surface>
{
};

enum SurfaceInitMode
{
  INIT_MODE_NONE,
  INIT_MODE_CLEAR,
  INIT_MODE_COPY
};

enum EffectTypes
{
  EFFECT_BGRX,
  EFFECT_RGBX,
  EFFECT_BGRA,
  EFFECT_RGBA,
  EFFECT_RGBA_EXTERNAL,
  EFFECT_YCBCR,
  EFFECT_COMPONENT_ALPHA,
  EFFECT_SOLID_COLOR,
  EFFECT_MASK,
  EFFECT_SURFACE,
  EFFECT_MAX
};

struct Effect : public RefCounted<Effect>
{
  Effect(uint32_t aType) : mType(aType) {}

  uint32_t mType;
};

struct EffectMask : public Effect
{
  EffectMask(Texture *aMaskTexture,
             const gfx::Matrix4x4 &aMaskTransform)
    : Effect(EFFECT_MASK), mMaskTexture(aMaskTexture)
    , mMaskTransform(aMaskTransform)
  {}

  RefPtr<Texture> mMaskTexture;
  gfx::Matrix4x4 mMaskTransform;
};

struct EffectSurface : public Effect
{
  EffectSurface(Surface *aSurface)
    : Effect(EFFECT_SURFACE), mSurface(aSurface)
  {}

  RefPtr<Surface> mSurface;
};

struct EffectBGRX : public Effect
{
  EffectBGRX(Texture *aBGRXTexture,
             bool aPremultiplied,
             mozilla::gfx::Filter aFilter,
             bool aFlipped = false)
    : Effect(EFFECT_BGRX), mBGRXTexture(aBGRXTexture)
    , mPremultiplied(aPremultiplied), mFilter(aFilter)
    , mFlipped(aFlipped)
  {}

  RefPtr<Texture> mBGRXTexture;
  bool mPremultiplied;
  mozilla::gfx::Filter mFilter;
  bool mFlipped;
};

struct EffectRGBX : public Effect
{
  EffectRGBX(Texture *aRGBXTexture,
             bool aPremultiplied,
             mozilla::gfx::Filter aFilter,
             bool aFlipped = false)
    : Effect(EFFECT_RGBX), mRGBXTexture(aRGBXTexture)
    , mPremultiplied(aPremultiplied), mFilter(aFilter)
    , mFlipped(aFlipped)
  {}

  RefPtr<Texture> mRGBXTexture;
  bool mPremultiplied;
  mozilla::gfx::Filter mFilter;
  bool mFlipped;
};

struct EffectBGRA : public Effect
{
  EffectBGRA(Texture *aBGRATexture,
             bool aPremultiplied,
             mozilla::gfx::Filter aFilter,
             bool aFlipped = false)
    : Effect(EFFECT_BGRA), mBGRATexture(aBGRATexture)
    , mPremultiplied(aPremultiplied), mFilter(aFilter)
    , mFlipped(aFlipped)
  {}

  RefPtr<Texture> mBGRATexture;
  bool mPremultiplied;
  mozilla::gfx::Filter mFilter;
  bool mFlipped;
};

struct EffectRGBA : public Effect
{
  EffectRGBA(Texture *aRGBATexture,
             bool aPremultiplied,
             mozilla::gfx::Filter aFilter,
             bool aFlipped = false)
    : Effect(EFFECT_RGBA), mRGBATexture(aRGBATexture)
    , mPremultiplied(aPremultiplied), mFilter(aFilter)
    , mFlipped(aFlipped)
  {}

  RefPtr<Texture> mRGBATexture;
  bool mPremultiplied;
  mozilla::gfx::Filter mFilter;
  bool mFlipped;
};

struct EffectRGBAExternal : public Effect
{
  EffectRGBAExternal(Texture *aRGBATexture,
                     const gfx::Matrix4x4 &aTextureTransform,
                     bool aPremultiplied,
                     mozilla::gfx::Filter aFilter,
                     bool aFlipped = false)
    : Effect(EFFECT_RGBA_EXTERNAL), mRGBATexture(aRGBATexture)
    , mTextureTransform(aTextureTransform), mPremultiplied(aPremultiplied)
    , mFilter(aFilter), mFlipped(aFlipped)
  {}

  RefPtr<Texture> mRGBATexture;
  gfx::Matrix4x4 mTextureTransform;
  bool mPremultiplied;
  mozilla::gfx::Filter mFilter;
  bool mFlipped;
};

struct EffectYCbCr : public Effect
{
  EffectYCbCr(Texture *aY, Texture *aCb, Texture *aCr,
              mozilla::gfx::Filter aFilter)
    : Effect(EFFECT_YCBCR), mY(aY), mCb(aCb), mCr(aCr)
    , mFilter(aFilter)
  {}

  RefPtr<Texture> mY;
  RefPtr<Texture> mCb;
  RefPtr<Texture> mCr;
  mozilla::gfx::Filter mFilter;
};

struct EffectComponentAlpha : public Effect
{
  EffectComponentAlpha(Texture *aOnWhite, Texture *aOnBlack)
    : Effect(EFFECT_COMPONENT_ALPHA), mOnWhite(aOnWhite), mOnBlack(aOnBlack)
  {}

  RefPtr<Texture> mOnWhite;
  RefPtr<Texture> mOnBlack;
};

struct EffectSolidColor : public Effect
{
  EffectSolidColor(const mozilla::gfx::Color &aColor)
    : Effect(EFFECT_SOLID_COLOR), mColor(aColor)
  {}

  mozilla::gfx::Color mColor;
};

struct EffectChain
{
  // todo - define valid grammar
  Effect* mEffects[EFFECT_MAX];

  EffectChain()
  {
    memset(mEffects, 0, EFFECT_MAX * sizeof(Effect*));
  }
};

class Compositor : public RefCounted<Compositor>
{
public:

  /* This creates a texture based on an in-memory bitmap.
   */
  virtual TemporaryRef<Texture>
    CreateTextureForData(const gfx::IntSize &aSize, PRInt8 *aData, PRUint32 aStride,
                         TextureFormat aFormat) = 0;

  /* This creates a Surface that can be used as a rendering target by this
   * compositor.
   */
  virtual TemporaryRef<Surface> CreateSurface(const gfx::IntRect &aRect,
                                              SurfaceInitMode aInit) = 0;

  /* This creates a Surface that can be used as a rendering target by this compositor,
   * and initializes this surface by copying from the given surface.
   */
  virtual TemporaryRef<Surface> CreateSurfaceFromSurface(const gfx::IntRect &aRect,
                                                         const Surface *aSource);

  /* Sets the given surface as the target for subsequent calls to DrawQuad.
   */
  virtual void SetSurfaceTarget(Surface *aSurface) = 0;

  /* Sets the screen as the target for subsequent calls to DrawQuad.
   */
  virtual void RemoveSurfaceTarget() = 0;

  /* This tells the compositor to actually draw a quad, where the area is
   * specified in userspace, and the source rectangle is the area of the
   * currently set textures to sample from. This area may not refer directly
   * to pixels depending on the effect.
   */
  virtual void DrawQuad(const gfx::Rect &aRect, const gfx::Rect *aSourceRect,
                        const gfx::Rect *aClipRect, const EffectChain &aEffectChain,
                        gfx::Float aOpacity, const gfx::Matrix4x4 &aTransform,
                        const gfx::Point &aOffset) = 0;

  /* Flush the current frame to the screen.
   */
  virtual void EndFrame();

  /* Whether textures created by this compositor can receive partial updates.
   */
  virtual bool SupportsPartialTextureUpdate() = 0;

#ifdef MOZ_DUMP_PAINTING
  virtual const char* Name() const =0;
#endif // MOZ_DUMP_PAINTING

  virtual ~Compositor() {}
};

class Factory
{
  static TemporaryRef<Compositor> CreateCompositorForWidget(nsIWidget *aWidget);
};

//TODO[nrc] move the code out of the header file
template <class TextureImpl, class CompositorImpl>
class YUVImageSource : public ImageSource
{
public:
  YUVImageSource(const YUVImage& aImage, CompositorImpl* aCompositor)
  {
    mCompositor = aCompositor;

    AutoOpenSurface surfY(OPEN_READ_ONLY, aImage.Ydata());
    AutoOpenSurface surfU(OPEN_READ_ONLY, aImage.Udata());

    mTextures[0].Init(aCompositor, surfY.Size());
    mTextures[1].Init(aCompositor, surfU.Size());
    mTextures[2].Init(aCompositor, surfU.Size());
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

    mTextures[0].Init(mCompositor, asurfY.Size());
    mTextures[1].Init(mCompositor, asurfU.Size());
    mTextures[2].Init(mCompositor, asurfV.Size());

    mTextures[0].Upload(mCompositor, asurfY.GetAsImage());
    mTextures[1].Upload(mCompositor, asurfU.GetAsImage());
    mTextures[2].Upload(mCompositor, asurfV.GetAsImage());
  }

  virtual void Composite(Compositor* aCompositor,
                         EffectChain& aEffectChain,
                         float aOpacity,
                         const gfx::Matrix4x4* aTransform,
                         const gfx::Point& aOffset,
                         const gfx::Filter aFilter)
  {
    EffectYCbCr* effect = new EffectYCbCr(&mTextures[0], &mTextures[1], &mTextures[2], aFilter);
    aEffectChain.mEffects[EFFECT_YCBCR] = effect;
    gfx::Rect rect(0, 0, mPictureRect.width, mPictureRect.height);
    gfx::Rect sourceRect(mPictureRect.x, mPictureRect.y, mPictureRect.width, mPictureRect.height);
    aCompositor->DrawQuad(rect, &sourceRect, nullptr, aEffectChain, aOpacity, *aTransform, aOffset);
  }

private:
  TextureImpl mTextures[3];
  RefPtr<CompositorImpl> mCompositor;
  nsIntRect mPictureRect;
};


}
}

#endif /* MOZILLA_GFX_COMPOSITOR_H */
