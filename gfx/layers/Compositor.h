/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_COMPOSITOR_H
#define MOZILLA_GFX_COMPOSITOR_H

#include "mozilla/RefPtr.h"
#include "mozilla/gfx/Rect.h"
#include "mozilla/gfx/Matrix.h"
#include "base/process.h"
#include "nsAutoPtr.h"

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

enum TextureHostType
{
  HOST_SHMEM,
  HOST_D3D10,
  HOST_GL
};

struct TextureHostIdentifier
{
  TextureHostType mType;
  void *mDescriptor;
  gfx::IntSize mMaxTextureSize;
};

struct TextureIdentifier
{
  TextureHostType mType;
  void *mDescriptor;
};

class Texture : public RefCounted<Texture>
{
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
  EFFECT_BGRA,
  EFFECT_YCBCR,
  EFFECT_COMPONENT_ALPHA,
  EFFECT_SOLID_COLOR,
  EFFECT_MASK,
  EFFECT_SURFACE,
  EFFECT_MAX
};

struct Effect
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
  EffectSurface(Surface *aSurface,
                bool aPremultiplied,
                mozilla::gfx::Filter aFilter)
    : Effect(EFFECT_SURFACE), mSurface(aSurface)
    , mPremultiplied(aPremultiplied), mFilter(aFilter)
  {}

  RefPtr<Surface> mSurface;
  bool mPremultiplied;
  mozilla::gfx::Filter mFilter;
};

struct EffectBGRX : public Effect
{
  EffectBGRX(Texture *aBGRXTexture,
             bool aPremultiplied,
             mozilla::gfx::Filter aFilter)
    : Effect(EFFECT_BGRX), mBGRXTexture(aBGRXTexture)
    , mPremultiplied(aPremultiplied), mFilter(aFilter)
  {}

  RefPtr<Texture> mBGRXTexture;
  bool mPremultiplied;
  mozilla::gfx::Filter mFilter;
};

struct EffectBGRA : public Effect
{
  EffectBGRA(Texture *aBGRATexture,
             bool aPremultiplied,
             mozilla::gfx::Filter aFilter)
    : Effect(EFFECT_BGRA), mBGRATexture(aBGRATexture)
    , mPremultiplied(aPremultiplied), mFilter(aFilter)
  {}

  RefPtr<Texture> mBGRATexture;
  bool mPremultiplied;
  mozilla::gfx::Filter mFilter;
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


class DrawableTextureHost : public Texture
{
  /* This will return an identifier that can be sent accross a process or
   * thread boundary and used to construct a DrawableTextureClient object
   * which can then be used for rendering. If the process is identical to the
   * current process this may return the same object and will only be thread
   * safe.
   */
  virtual TextureIdentifier GetIdentifierForProcess(base::ProcessHandle aProcess) = 0;

  /* Perform any precomputation (e.g. texture upload) that needs to happen to the
   * texture before rendering.
   */
  virtual void PrepareForRendering() = 0;
};

/* This class allows texture clients to draw into textures through Azure or
 * thebes and applies locking semantics to allow GPU or CPU level
 * synchronization.
 */
class DrawableTextureClient
{
  /* This will return an identifier that can be sent accross a process or
   * thread boundary and used to construct a DrawableTextureHost object
   * which can then be used as a texture for rendering by a compatible
   * compositor. This texture should have been created with the
   * TextureHostIdentifier specified by the compositor that this identifier
   * is to be used with. If the process is identical to the current process
   * this may return the same object and will only be thread safe.
   */
  virtual TextureIdentifier GetIdentifierForProcess(base::ProcessHandle aProcess) = 0;

  /* This requests a DrawTarget to draw into the current texture. Once the
   * user is finished with the DrawTarget it should call Unlock.
   */
  virtual TemporaryRef<gfx::DrawTarget> LockDT() = 0;

  /* This requests a gfxContext to draw into the current texture. Once the
   * user is finished with the gfxContext it should call Unlock.
   */
  virtual already_AddRefed<gfxContext> LockContext() = 0;

  /* This unlocks the current DrawableTexture and allows the host to composite
   * it directly.
   */
  virtual void Unlock() = 0;
};

class Compositor : public RefCounted<Compositor>
{
public:
  /* Request a texture host identifier that may be used for creating textures
   * accross process or thread boundaries that are compatible with this
   * compositor.
   */
  virtual TextureHostIdentifier
    GetTextureHostIdentifier() = 0;

  /* This creates an immutable texture based on an in-memory bitmap.
   */
  virtual TemporaryRef<Texture>
    CreateTextureForData(const gfx::IntSize &aSize, PRInt8 *aData, PRUint32 aStride,
                         TextureFormat aFormat) = 0;

  /* This creates a DrawableTexture that can be sent accross process or thread
   * boundaries to receive its content.
   */
  virtual TemporaryRef<DrawableTextureHost>
    CreateDrawableTexture(const TextureIdentifier &aIdentifier) = 0;

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
                        gfx::Float aOpacity, const gfx::Matrix4x4 &aTransform) = 0;

  /* Flush the current frame to the screen.
   */
  virtual void EndFrame();

  virtual ~Compositor() {}
};

class Factory
{
  static TemporaryRef<Compositor> CreateCompositorForWidget(nsIWidget *aWidget);

  /* This may be called by a Texture client to create a Texture which is
   * the host whose identifier is specified.
   */
  static TemporaryRef<DrawableTextureClient> CreateTextureClient(const TextureHostIdentifier &aIdentifier);
};

}
}

#endif /* MOZILLA_GFX_COMPOSITOR_H */
