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

//TODO[nrc] TextureOGL and Texture are only used by CreateTextureForData,
// which is not used anywhere, so what are they for?
class TextureOGL : public Texture
{
public:
  TextureOGL(GLContext* aGL, GLuint aTextureHandle, const gfx::IntSize& aSize)
    : mGL(aGL)
    , mTextureHandle(aTextureHandle)
    , mSize(aSize)
  {}

  virtual GLuint GetTextureHandle()
  {
    return mTextureHandle;
  }

  virtual gfx::IntSize GetSize()
  {
    return mSize;
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
  gfx::IntSize mSize;
};

//TODO[nrc] kill this once I sort out TextureOGL
class TextureHostOGL : public TextureHost
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
  TextureHostOGL()
    : mWrapMode(LOCAL_GL_REPEAT)
  {}

  TextureHostOGL(gfx::IntSize aSize)
    : mSize(aSize)
    , mWrapMode(LOCAL_GL_REPEAT)
  {}

  gfx::IntSize mSize;
  GLenum mWrapMode;
};

class BasicBufferOGL;
class SurfaceBufferOGL;

//thin TextureHost wrapper around a TextureImage
class TextureImageAsTextureHost : public TextureHostOGL, public TileIterator
{
public:
  virtual gfx::IntSize GetSize()
  {
    NS_ASSERTION(mSize == gfx::IntSize(mTexImage->mSize.width, mTexImage->mSize.height),
                 "mSize not synced with mTexImage");
    return mSize;
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

  virtual TileIterator* GetAsTileIterator() { return this; }
  virtual Effect* Lock(const gfx::Filter& aFilter);


  void SetFilter(const gfx::Filter& aFilter) { mTexImage->SetFilter(gfx::ThebesFilter(aFilter)); }
  virtual void BeginTileIteration() { mTexImage->BeginTileIteration(); }
  virtual nsIntRect GetTileRect() { return mTexImage->GetTileRect(); }
  virtual size_t GetTileCount() { return mTexImage->GetTileCount(); }
  virtual bool NextTile() { return mTexImage->NextTile(); }
  //bool InUpdate() { return mTexImage->InUpdate(); }
  //void EndUpdate() { mTexImage->EndUpdate(); }
  //gfxASurface* BeginUpdate(nsIntRegion& aRegion) { return mTexImage->BeginUpdate(aRegion); }

protected:
  TextureImageAsTextureHost(GLContext* aGL)
    : mGL(aGL)
    , mTexImage(nullptr)
  {}

  GLContext* mGL;
  nsRefPtr<TextureImage> mTexImage;

  friend class CompositorOGL;
  // YUCK! this is because BasicBufferOGL and SurfaceBufferOGL use this class like a texture image
  // below method is meant only for ContentHost
  friend class ContentHost;
  friend class ThebesLayerBufferOGL;
  TextureImage* GetTextureImage() { return mTexImage; }

  // TODO[nrc] comment
  // constructor for using without the TextureHost capabilites (probably a bad idea)
  TextureImageAsTextureHost(TextureImage* aTexImage, GLContext* aGL)
    : mGL(aGL)
    , mTexImage(aTexImage)
  {}
};

class TextureImageAsTextureHostWithBuffer : public TextureImageAsTextureHost
{
public:
  ~TextureImageAsTextureHostWithBuffer();

  virtual bool Update(const SurfaceDescriptor& aNewBuffer,
                      SurfaceDescriptor* aOldBuffer);
  /**
   * Set deallocator for data recieved from IPC protocol
   * We should be able to set allocator right before swap call
   * that is why allowed multiple call with the same Allocator
   */
  virtual void SetDeAllocator(ISurfaceDeAllocator* aDeAllocator)
  {
    NS_ASSERTION(!mDeAllocator || mDeAllocator == aDeAllocator, "Stomping allocator?");
    mDeAllocator = aDeAllocator;
  }

  // returns true if the buffer was reset
  bool EnsureBuffer(nsIntSize aSize);

protected:
  TextureImageAsTextureHostWithBuffer(GLContext* aGL)
    : TextureImageAsTextureHost(aGL)
  {}

  ISurfaceDeAllocator* mDeAllocator;
  SurfaceDescriptor mBufferDescriptor;

  friend class CompositorOGL;
};

class TextureHostOGLShared : public TextureHostOGL
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

  //TODO[nrc] delete
  void SetSize(const gfx::IntSize& aSize)
  {
    mSize = aSize;
  }

  virtual GLuint GetTextureHandle()
  {
    return mTextureHandle;
  }

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
  gl::SharedTextureHandle mSharedHandle;
  gl::TextureImage::TextureShareType mShareType;

  friend class CompositorOGL;
};

class TextureHostOGLSharedWithBuffer : public TextureHostOGLShared
{
public:
  //TODO[nrc] don't we need to de-allocate?
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

class GLTextureAsTextureHost : public TextureHostOGL
{
public:
  GLTextureAsTextureHost(GLContext* aGL)
    : TextureHostOGL()
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
