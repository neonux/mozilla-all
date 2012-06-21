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

struct Matrix3x3
{
  Float _11, _12, _13;
  Float _21, _22, _23;
  Float _31, _32, _33;
};

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
};

struct TextureIdentifier
{
  TextureHostType mType;
  void *mDescriptor;
};

class Texture : public RefCounted<Texture>
{
};

enum EffectTypes
{
  EFFECT_BGRX,
  EFFECT_BGRA,
  EFFECT_YCBCR,
  EFFECT_COMPONENT_ALPHA,
  EFFECT_SOLID_COLOR,
  EFFECT_MASK,
  EFFECT_MAX
};

struct Effect
{
  Effect(uint32_t aType) : mType(aType) {}

  uint32_t mType;
};

struct EffectMask : public Effect
{
  EffectMask(Texture *aMaskTexture)
    : Effect(EFFECT_MASK), mMaskTexture(aMaskTexture)
  {}

  RefPtr<Texture> mMaskTexture;
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
};


class DrawableTextureHost : public Texture
{
  /* This will return an identifier that can be sent accross a process or
   * thread boundary and used to construct a DrawableTextureClient object
   * which can then be used for rendering. If the process is identical to the
   * current process this may return the same object and will only be thread
   * safe.
   */
  TextureIdentifier GetIdentifierForProcess(base::ProcessHandle aProcess);
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
  TextureIdentifier GetIdentifierForProcess(base::ProcessHandle aProcess);

  /* This requests a DrawTarget to draw into the current texture. Once the
   * user is finished with the DrawTarget it should call Unlock.
   */
  TemporaryRef<gfx::DrawTarget> LockDT();

  /* This requests a gfxContext to draw into the current texture. Once the
   * user is finished with the gfxContext it should call Unlock.
   */
  already_AddRefed<gfxContext> LockContext();

  /* This unlocks the current DrawableTexture and allows the host to composite
   * it directly.
   */
  void Unlock();
};

class Compositor : RefCounted<Compositor>
{
  /* Request a texture host identifier that may be used for creating textures
   * accross process or thread boundaries that are compatible with this
   * compositor.
   */
  TextureHostIdentifier
    GetTextureHostIdentifier();

  /* This creates an immutable texture based on an in-memory bitmap.
   */
  TemporaryRef<Texture>
    CreateTextureForData(const gfx::IntSize &aSize, PRInt8 *aData, PRUint32 aStride,
                         TextureFormat aFormat);

  /* This creates a DrawableTexture that can be sent accross process or thread
   * boundaries to receive its content.
   */
  TemporaryRef<DrawableTextureHost>
    CreateDrawableTexture(const TextureIdentifier &aIdentifier);

  /* This tells the compositor to actually draw a quad, where the area is
   * specified in userspace, and the source rectangle is the area of the
   * currently set textures to sample from. This area may not refer directly
   * to pixels depending on the effect.
   */
  void DrawQuad(const gfx::Rect &aRect, const gfx::Rect *aSourceRect,
                const gfx::Rect *aClipRect, const EffectChain &aEffectChain,
                const gfx::Matrix3x3 &aTransform); 
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
