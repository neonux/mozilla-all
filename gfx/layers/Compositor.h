/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_COMPOSITOR_H
#define MOZILLA_GFX_COMPOSITOR_H

#include "mozilla/RefPtr.h"
#include "mozilla/gfx/Rect.h"
#include "mozilla/gfx/Matrix.h"
#include "gfxMatrix.h"
#include "nsAutoPtr.h"
#include "nsRegion.h"

//TODO: I'm pretty sure we don't want to do this
//this is just for the definition of HANDLE, used to typedef ProcessHandle
//#ifdef OS_WIN
//#include <windows.h>
//#endif

class gfxContext;
class nsIWidget;

/*namespace base {
#if defined(OS_WIN)
typedef HANDLE int ProcessHandle;
#elif defined(OS_POSIX)
typedef pid_t ProcessHandle;
#endif
}*/

namespace mozilla {

namespace gfx {
class DrawTarget;
}

namespace layers {

class Compositor;
struct EffectChain;
class SharedImage;

enum TextureFormat
{
  TEXTUREFORMAT_BGRX32,
  TEXTUREFORMAT_BGRA32,
  TEXTUREFORMAT_BGR16,
  TEXTUREFORMAT_Y8
};


enum ImageSourceType
{
  IMAGE_YUV, //TODO[nrc] do we need backend texture info? I don't think so, should be covered by TextureHostType (maybe)
  IMAGE_SHARED,
  IMAGE_TEXTURE,
  IMAGE_SHMEM
};

//TODO[nrc] should ImageSourceType be part of TextureHostType? No, but we need both or something?
//maybe TextureType - one of GL, D3D19, etc. used with IMAGE_TEXTURE, IMAGE_YUV

enum TextureHostType
{
  HOST_D3D10,
  HOST_GL,
  HOST_SHMEM  //[nrc] for software composition
};

//TODO[nrc] comment
// goes Compositor to ShadowLayerForwarder on LayerManager init
struct TextureHostIdentifier
{
  TextureHostType mType;
  PRInt32 mMaxTextureSize;
};

//TODO[nrc] comment
// goes LayerManager to Compositor on TextureClient creation
struct TextureIdentifier
{
  ImageSourceType mType;
  PRUint32 mDescriptor;
};

/*static bool operator==(const TextureHostIdentifier& aLeft,const TextureHostIdentifier& aRight)
{
  return aLeft.mType == aRight.mType &&
         aLeft.mMaxTextureSize == aRight.mMaxTextureSize;
}*/
static bool operator==(const TextureIdentifier& aLeft, const TextureIdentifier& aRight)
{
  return aLeft.mType == aRight.mType &&
         aLeft.mDescriptor == aRight.mDescriptor;
}

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

//TODO[nrc] merge with TextureHost
class ImageSource : public RefCounted<ImageSource>
{
public:
  virtual ImageSourceType GetType() = 0;

  virtual void UpdateImage(const SharedImage& aImage) = 0;

  virtual void Composite(EffectChain& aEffectChain,
                         float aOpacity,
                         const gfx::Matrix4x4& aTransform,
                         const gfx::Point& aOffset,
                         const gfx::Filter aFilter,
                         const gfx::Rect& aClipRect) = 0;

  //TODO[nrc] fix the dependency on GL stuff!
  typedef unsigned int GLuint;
  virtual void BindTexture(GLuint aTextureUnit)
  {
    NS_ERROR("BindTexture not implemented for this ImageSource");
  }
};

class TextureHost : public Texture
{
public:
  /* This will return an identifier that can be sent accross a process or
   * thread boundary and used to construct a DrawableTextureClient object
   * which can then be used for rendering. If the process is identical to the
   * current process this may return the same object and will only be thread
   * safe.
   */
  virtual TextureIdentifier GetIdentifierForProcess(/*base::ProcessHandle* aProcess*/) = 0; //TODO[nrc]

  /* Perform any precomputation (e.g. texture upload) that needs to happen to the
   * texture before rendering.
   */
  virtual void PrepareForRendering() = 0;
};

/* This class allows texture clients to draw into textures through Azure or
 * thebes and applies locking semantics to allow GPU or CPU level
 * synchronization.
 */
class TextureClient : public RefCounted<TextureClient>
{
public:
  /* This will return an identifier that can be sent accross a process or
   * thread boundary and used to construct a DrawableTextureHost object
   * which can then be used as a texture for rendering by a compatible
   * compositor. This texture should have been created with the
   * TextureHostIdentifier specified by the compositor that this identifier
   * is to be used with. If the process is identical to the current process
   * this may return the same object and will only be thread safe.
   */
  virtual TextureIdentifier GetIdentifierForProcess(/*base::ProcessHandle* aProcess*/) = 0; //TODO[nrc]

  //TODO[nrc] will this even work?
  virtual TextureIdentifier GetIdentifier() = 0;

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
  /* Request a texture host identifier that may be used for creating textures
   * accross process or thread boundaries that are compatible with this
   * compositor.
   */
  virtual TextureHostIdentifier
    GetTextureHostIdentifier() = 0;

  /* This creates a texture based on an in-memory bitmap.
   */
  virtual TemporaryRef<Texture>
    CreateTextureForData(const gfx::IntSize &aSize, PRInt8 *aData, PRUint32 aStride,
                         TextureFormat aFormat) = 0;

  /**
   * TODO[nrc] comment
   */
  virtual TemporaryRef<TextureHost>
    CreateTextureHost(const TextureIdentifier &aIdentifier) = 0;

  /**
   * TODO[nrc] comment, name
   */
  virtual TemporaryRef<ImageSource> 
    CreateImageSourceForSharedImage(ImageSourceType aType) = 0;

  /* This creates a Surface that can be used as a rendering target by this
   * compositor.
   */
  virtual TemporaryRef<Surface> CreateSurface(const gfx::IntRect &aRect,
                                              SurfaceInitMode aInit) = 0;

  /* This creates a Surface that can be used as a rendering target by this compositor,
   * and initializes this surface by copying from the given surface. If the given surface
   * is nullptr, the screen frame in progress is used as the source.
   */
  virtual TemporaryRef<Surface> CreateSurfaceFromSurface(const gfx::IntRect &aRect,
                                                         const Surface *aSource) = 0;

  /* Sets the given surface as the target for subsequent calls to DrawQuad.
   * Passing nullptr as aSurface sets the screen as the target.
   */
  virtual void SetSurfaceTarget(Surface *aSurface) = 0;


  /* This tells the compositor to actually draw a quad, where the area is
   * specified in userspace, and the source rectangle is the area of the
   * currently set textures to sample from. This area may not refer directly
   * to pixels depending on the effect.
   */
  virtual void DrawQuad(const gfx::Rect &aRect, const gfx::Rect *aSourceRect,
                        const gfx::Rect *aClipRect, const EffectChain &aEffectChain,
                        gfx::Float aOpacity, const gfx::Matrix4x4 &aTransform,
                        const gfx::Point &aOffset) = 0;

  /* Start a new frame. If aClipRectIn is null, sets *aClipRectOut to the screen dimensions. 
   */
  virtual void BeginFrame(const gfx::Rect *aClipRectIn, const gfxMatrix& aTransform,
                          gfx::Rect *aClipRectOut = nullptr) = 0;

  /* Flush the current frame to the screen.
   */
  virtual void EndFrame() = 0;

  /* Whether textures created by this compositor can receive partial updates.
   */
  virtual bool SupportsPartialTextureUpdate() = 0;

#ifdef MOZ_DUMP_PAINTING
  virtual const char* Name() const = 0;
#endif // MOZ_DUMP_PAINTING

  virtual ~Compositor() {}
};

class CompositingFactory
{
public:
  // TODO[nrc] comment
  static TemporaryRef<TextureClient> CreateTextureClient(const TextureHostType &aHostType, const ImageSourceType& aImageSourceType);

  static TemporaryRef<Compositor> CreateCompositorForWidget(nsIWidget *aWidget);
};


}
}

#endif /* MOZILLA_GFX_COMPOSITOR_H */
