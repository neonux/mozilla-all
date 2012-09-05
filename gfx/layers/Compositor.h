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
#include "LayersTypes.h"

//TODO[nrc] can we break up this header file into host and client parts?

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
struct Effect;
struct EffectChain;
class SharedImage;
class ShadowLayerForwarder;
class ShadowableLayer;
class TextureClient;
class ImageClient;
class CanvasClient;
class ContentClient;
class Image;
class ISurfaceDeAllocator;

enum TextureFormat
{
  TEXTUREFORMAT_BGRX32,
  TEXTUREFORMAT_BGRA32,
  TEXTUREFORMAT_BGR16,
  TEXTUREFORMAT_Y8
};


//TODO[nrc] rename this crap
//TODO[nrc] update E:\Firefox\gfx\ipc\glue\IPCMessageUtils.h
enum ImageHostType
{
  IMAGE_UNKNOWN,
  IMAGE_YUV,
  IMAGE_SHARED,
  IMAGE_TEXTURE,
  IMAGE_BRIDGE,
  IMAGE_THEBES,
  IMAGE_DIRECT
};

enum TextureHostType
{
  TEXTURE_UNKNOWN,
  TEXTURE_SHMEM,
  TEXTURE_SHARED,
  TEXTURE_SHARED_GL,
  TEXTURE_BRIDGE
};

typedef uint32_t TextureFlags;
const TextureFlags NoFlags            = 0x0;
const TextureFlags UseNearestFilter   = 0x1;
const TextureFlags NeedsYFlip         = 0x2;
const TextureFlags ForceSingleTile    = 0x4;
const TextureFlags UseOpaqueSurface   = 0x8;


//TODO[nrc] comment
// goes Compositor to ShadowLayerForwarder on LayerManager init
struct TextureHostIdentifier
{
  //TODO[nrc] add ImageHostType, use it
  LayersBackend mParentBackend;
  PRInt32 mMaxTextureSize;
};

//TODO[nrc] comment
// goes LayerManager to Compositor on TextureClient creation
struct TextureIdentifier
{
  ImageHostType mImageType;
  TextureHostType mTextureType;
  PRUint32 mDescriptor;
};

/*static bool operator==(const TextureHostIdentifier& aLeft,const TextureHostIdentifier& aRight)
{
  return aLeft.mType == aRight.mType &&
         aLeft.mMaxTextureSize == aRight.mMaxTextureSize;
}*/
static bool operator==(const TextureIdentifier& aLeft, const TextureIdentifier& aRight)
{
  return aLeft.mImageType == aRight.mImageType &&
         aLeft.mTextureType == aRight.mTextureType &&
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


//TODO[nrc] IPC interface is a bit of a mess:
// ImageClient -> SharedImage -> TextureHost
class TextureHost : public Texture
{
public:
  TextureHost() : mFlags(NoFlags) {}
  /* This will return an identifier that can be sent accross a process or
   * thread boundary and used to construct a DrawableTextureClient object
   * which can then be used for rendering. If the process is identical to the
   * current process this may return the same object and will only be thread
   * safe.
   */
  //virtual TextureIdentifier GetIdentifierForProcess(/*base::ProcessHandle* aProcess*/) = 0; //TODO[nrc]

  /* Perform any precomputation (e.g. texture upload) that needs to happen to the
   * texture before rendering.
   */
  //virtual void PrepareForRendering() {}

  //TODO[nrc] comments
  virtual const SharedImage* Update(const SharedImage& aImage) { return nullptr; }
  virtual Effect* Lock(const gfx::Filter& aFilter) { return nullptr; }
  virtual void Unlock() {}

  void SetFlags(TextureFlags aFlags) { mFlags = aFlags; }
  void AddFlag(TextureFlags aFlag) { mFlags |= aFlag; }
protected:
  TextureFlags mFlags;
};

class ImageHost : public RefCounted<ImageHost>
{
public:
  ImageHost()
    : mDeAllocator(nullptr)
  {}

  virtual ImageHostType GetType() = 0;

  virtual const SharedImage* UpdateImage(const TextureIdentifier& aTextureIdentifier,
                                         const SharedImage& aImage) = 0;

  virtual void Composite(EffectChain& aEffectChain,
                         float aOpacity,
                         const gfx::Matrix4x4& aTransform,
                         const gfx::Point& aOffset,
                         const gfx::Filter& aFilter,
                         const gfx::Rect& aClipRect,
                         const nsIntRegion* aVisibleRegion = nullptr) = 0;

  virtual void AddTextureHost(const TextureIdentifier& aTextureIdentifier, TextureHost* aTextureHost) = 0;

  /**
   * Set deallocator for data recieved from IPC protocol
   * We should be able to set allocator right before swap call
   * that is why allowed multiple call with the same Allocator
   */
  void SetDeAllocator(ISurfaceDeAllocator* aDeAllocator)
  {
    NS_ASSERTION(!mDeAllocator || mDeAllocator == aDeAllocator, "Stomping allocator?");
    mDeAllocator = aDeAllocator;
  }

protected:
  ISurfaceDeAllocator* mDeAllocator;
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
  EFFECT_RGB,
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

struct EffectRGB : public Effect
{
  EffectRGB(Texture *aRGBTexture,
             bool aPremultiplied,
             mozilla::gfx::Filter aFilter,
             bool aFlipped = false)
    : Effect(EFFECT_RGB), mRGBTexture(aRGBTexture)
    , mPremultiplied(aPremultiplied), mFilter(aFilter)
    , mFlipped(aFlipped)
  {}

  RefPtr<Texture> mRGBTexture;
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
    CreateTextureHost(const TextureIdentifier &aIdentifier, TextureFlags aFlags) = 0;

  /**
   * TODO[nrc] comment, name
   */
  virtual TemporaryRef<ImageHost> 
    CreateImageHost(ImageHostType aType) = 0;

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
  // TODO[nrc] enums shouldn't be const &
  static TemporaryRef<ImageClient> CreateImageClient(LayersBackend aBackendType,
                                                     ImageHostType aImageHostType,
                                                     ShadowLayerForwarder* aLayerForwarder,
                                                     ShadowableLayer* aLayer,
                                                     TextureFlags aFlags);
  static TemporaryRef<CanvasClient> CreateCanvasClient(LayersBackend aBackendType,
                                                       ImageHostType aImageHostType,
                                                       ShadowLayerForwarder* aLayerForwarder,
                                                       ShadowableLayer* aLayer,
                                                       TextureFlags aFlags);
  static TemporaryRef<ContentClient> CreateContentClient(LayersBackend aBackendType,
                                                         ImageHostType aImageHostType,
                                                         ShadowLayerForwarder* aLayerForwarder,
                                                         ShadowableLayer* aLayer,
                                                         TextureFlags aFlags);
  static TemporaryRef<TextureClient> CreateTextureClient(LayersBackend aBackendType,
                                                         TextureHostType aTextureHostType,
                                                         ImageHostType aImageHostType,
                                                         ShadowLayerForwarder* aLayerForwarder,
                                                         bool aStrict = false);

  static TemporaryRef<Compositor> CreateCompositorForWidget(nsIWidget *aWidget);
  static ImageHostType TypeForImage(Image* aImage);
};


}
}

#endif /* MOZILLA_GFX_COMPOSITOR_H */
