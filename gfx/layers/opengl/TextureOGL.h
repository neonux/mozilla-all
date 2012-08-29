/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_TEXTUREOGL_H
#define MOZILLA_GFX_TEXTUREOGL_H

#include "ImageLayerOGL.h"
#include "CompositorOGL.h"

namespace mozilla {
namespace layers {

class TextureImageAsTextureHost;
class TextureHostOGLShared;

// not really a TextureHost, but otherwise we have a screwey inheritance hierarchy
class ATextureOGL : public TextureHost
{
public:
  virtual GLuint GetTextureHandle() = 0;
  virtual gfx::IntSize GetSize() = 0;
  virtual GLenum GetWrapMode() = 0;
  virtual void SetWrapMode(GLenum aWrapMode) = 0;

  //TODO[nrc] will UpdateTexture work with the other kinds of textures?
  //default = no op
  virtual void
    UpdateTexture(const nsIntRegion& aRegion, PRInt8 *aData, PRUint32 aStride) MOZ_OVERRIDE {}

protected:
  ATextureOGL() {}
};

//TODO[nrc] kill this once I sort out TextureOGL
class CTextureOGL : public ATextureOGL
{
public:
  virtual GLuint GetTextureHandle() = 0;

  virtual gfx::IntSize GetSize()
  {
    return mSize;
  }

  virtual GLenum GetWrapMode()
  {
    return mWrapMode;
  }

  virtual void SetWrapMode(GLenum aWrapMode)
  {
    mWrapMode = aWrapMode;
  }

protected:
  CTextureOGL()
    : mWrapMode(LOCAL_GL_REPEAT)
  {}

  CTextureOGL(gfx::IntSize aSize)
    : mSize(aSize)
    , mWrapMode(LOCAL_GL_REPEAT)
  {}

  gfx::IntSize mSize;
  GLenum mWrapMode;
};



class TextureOGL : public CTextureOGL
{
public:
  TextureOGL(GLContext* aGL, GLuint aTextureHandle, const gfx::IntSize& aSize)
    : CTextureOGL(aSize)
    , mGL(aGL)
    , mTextureHandle(aTextureHandle)
  {}

  virtual GLuint GetTextureHandle()
  {
    return mTextureHandle;
  }

  // TODO: Remove this once Textures are properly able to manage their own
  // handles.
  void SetTextureHandle(GLuint aTextureHandle)
  {
    mTextureHandle = aTextureHandle;
  }

  // TODO: Remove this once Textures are properly able to manager their own
  // sizes.
  void SetSize(const gfx::IntSize& aSize)
  {
    mSize = aSize;
  }

  virtual void
    UpdateTexture(const nsIntRegion& aRegion, PRInt8 *aData, PRUint32 aStride) MOZ_OVERRIDE;
  void UpdateTexture(PRInt8 *aData, PRUint32 aStride);

  void SetProperties(GLenum aFormat,
                     GLenum aInternalFormat,
                     GLenum aType,
                     PRUint32 aPixelSize)
  {
    mFormat = aFormat;
    mInternalFormat = aInternalFormat;
    mType = aType;
    mPixelSize = aPixelSize;
  }

private:
  GLuint mTextureHandle;
  GLenum mFormat;
  GLenum mInternalFormat;
  GLenum mType;
  nsRefPtr<GLContext> mGL;
  uint32_t mPixelSize;
};

//TODO[nrc] the ImageHosts are tied to a specific TextureHosts, I should fix that, maybe
// which means that these ImageHosts are GL specific because their TextureHosts are GL
// specific

class ImageHostTexture : public ImageHost
{
public:
  ImageHostTexture(Compositor* aCompositor);

  virtual ImageHostType GetType() { return IMAGE_TEXTURE; }

  virtual const SharedImage* UpdateImage(const TextureIdentifier& aTextureIdentifier,
                                   const SharedImage& aImage);

  virtual void Composite(EffectChain& aEffectChain,
                         float aOpacity,
                         const gfx::Matrix4x4& aTransform,
                         const gfx::Point& aOffset,
                         const gfx::Filter& aFilter,
                         const gfx::Rect& aClipRect,
                         const nsIntRegion* aVisibleRegion = nullptr);

  virtual void AddTextureHost(const TextureIdentifier& aTextureIdentifier, TextureHost* aTextureHost);

private:
  RefPtr<TextureImageAsTextureHost> mTextureHost;
  RefPtr<Compositor> mCompositor;
};

class ImageHostShared : public ImageHost
{
public:
  ImageHostShared(Compositor* aCompositor);

  virtual ImageHostType GetType() { return IMAGE_SHARED; }


  virtual const SharedImage* UpdateImage(const TextureIdentifier& aTextureIdentifier,
                                         const SharedImage& aImage);

  virtual void Composite(EffectChain& aEffectChain,
                         float aOpacity,
                         const gfx::Matrix4x4& aTransform,
                         const gfx::Point& aOffset,
                         const gfx::Filter& aFilter,
                         const gfx::Rect& aClipRect,
                         const nsIntRegion* aVisibleRegion = nullptr);

  virtual void AddTextureHost(const TextureIdentifier& aTextureIdentifier, TextureHost* aTextureHost);

private:
  RefPtr<TextureHostOGLShared> mTextureHost;
  RefPtr<Compositor> mCompositor;
};

class BasicBufferOGL;
class SurfaceBufferOGL;

//thin TextureHost wrapper around a TextureImage
class TextureImageAsTextureHost : public ATextureOGL
{
public:
  // XXX returns the size of the current tile being composited to DrawQuad, I think this is a bit of a hack
  virtual gfx::IntSize GetSize()
  {
    return mSize;
  }

  void SetSize(const gfx::IntSize& aSize)
  {
    mSize = aSize;
  }

  virtual GLuint GetTextureHandle()
  {
    return mTexImage->GetTextureID();
  }

  virtual GLenum GetWrapMode()
  {
    return mTexImage->mWrapMode;
  }

  virtual void SetWrapMode(GLenum aWrapMode)
  {
    mTexImage->mWrapMode = aWrapMode;
  }

  virtual const SharedImage* Update(const SharedImage& aImage);
  virtual void Update(gfxASurface* aSurface, nsIntRegion& aRegion);
  virtual Effect* Lock(const gfx::Filter& aFilter);

  // used when this is a double buffered host (TODO[nrc] should that make it a separate class?)
  bool UpdateDirect(const SurfaceDescriptor& aBuffer);

  void SetFilter(const gfx::Filter& aFilter) { mTexImage->SetFilter(gfx::ThebesFilter(aFilter)); }
  void BeginTileIteration() { mTexImage->BeginTileIteration(); }
  nsIntRect GetTileRect() { return mTexImage->GetTileRect(); }
  size_t GetTileCount() { return mTexImage->GetTileCount(); }
  bool NextTile() { return mTexImage->NextTile(); }
  bool InUpdate() { return mTexImage->InUpdate(); }
  void EndUpdate() { mTexImage->EndUpdate(); }
  gfxASurface* BeginUpdate(nsIntRegion& aRegion) { return mTexImage->BeginUpdate(aRegion); }

private:
  TextureImageAsTextureHost(GLContext* aGL)
    : mGL(aGL)
    , mTexImage(nullptr)
  {}

  GLContext* mGL;
  nsRefPtr<TextureImage> mTexImage;
  gfx::IntSize mSize;

  friend class CompositorOGL;
  // YUCK! this is because BasicBufferOGL and SurfaceBufferOGL use this class like a texture image
  // below methods are meant only for BasicBufferOGL and SurfaceBufferOGL
  friend class BasicBufferOGL;
  friend class SurfaceBufferOGL;

  TextureImage::ContentType GetContentType() { return mTexImage->GetContentType(); }
  TextureImage* GetTextureImage() { return mTexImage; }

  // TODO[nrc] comment
  // constructor for using without the TextureHost capabilites (probably a bad idea)
  TextureImageAsTextureHost(TextureImage* aTexImage, GLContext* aGL)
    : mGL(aGL)
    , mTexImage(aTexImage)
  {}
};

class TextureHostOGLShared : public ATextureOGL
{
public:
  ~TextureHostOGLShared()
  {
    mGL->MakeCurrent();
    mGL->ReleaseSharedHandle(mShareType, mSharedHandle);
    if (mTextureHandle) {
      mGL->fDeleteTextures(1, &mTextureHandle);
    }
  }

  virtual gfx::IntSize GetSize()
  {
    return mSize;
  }

  void SetSize(const gfx::IntSize& aSize)
  {
    mSize = aSize;
  }

  virtual GLuint GetTextureHandle()
  {
    return mTextureHandle;
  }

  virtual GLenum GetWrapMode()
  {
    return LOCAL_GL_REPEAT;
  }

  virtual void SetWrapMode(GLenum aWrapMode) {}

  virtual const SharedImage* Update(const SharedImage& aImage);
  virtual Effect* Lock(const gfx::Filter& aFilter);
  virtual void Unlock();

protected:
  TextureHostOGLShared(GLContext* aGL)
    : mGL(aGL)
  {}

  bool mInverted;
  GLContext* mGL;
  GLuint mTextureHandle;
  gfx::IntSize mSize;
  gl::SharedTextureHandle mSharedHandle;
  gl::TextureImage::TextureShareType mShareType;

  friend class CompositorOGL;
};

class TextureHostOGLSharedWithBuffer : public TextureHostOGLShared
{
public:
  ~TextureHostOGLSharedWithBuffer()
  {}

  virtual const SharedImage* Update(const SharedImage& aImage);
  virtual Effect* Lock(const gfx::Filter& aFilter);
  virtual void Unlock();

protected:
  TextureHostOGLSharedWithBuffer(GLContext* aGL)
    : TextureHostOGLShared(aGL)
  {}

  SharedImage mBuffer;

  friend class CompositorOGL;
};

class GLTextureAsTextureHost : public CTextureOGL
{
public:
  GLTextureAsTextureHost(GLContext* aGL)
    : CTextureOGL()
    , mGL(aGL)
  {}

  ~GLTextureAsTextureHost()
  {
    mTexture.Release();
  }

  virtual GLuint GetTextureHandle()
  {
    return mTexture.GetTextureID();
  }

  const SharedImage* Update(const SharedImage& aImage);

private:
  nsRefPtr<GLContext> mGL;
  GLTexture mTexture;
};


}
}

#endif /* MOZILLA_GFX_TEXTUREOGL_H */
