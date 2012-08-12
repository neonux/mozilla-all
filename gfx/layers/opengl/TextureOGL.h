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

class DrawableTextureHostOGL : public DrawableTextureHost
{
  virtual TextureIdentifier GetIdentifierForProcess(base::ProcessHandle* aProcess) MOZ_OVERRIDE
  {
    return TextureIdentifier();
  }

  virtual void PrepareForRendering() MOZ_OVERRIDE {}

  virtual void
    UpdateTexture(const nsIntRegion& aRegion, PRInt8 *aData, PRUint32 aStride) MOZ_OVERRIDE {}
};

class ATextureOGL : public Texture
{
public:
  virtual GLuint GetTextureHandle() = 0;

  gfx::IntSize GetSize()
  {
    return mSize;
  }


  GLenum GetWrapMode()
  {
    return mWrapMode;
  }

  void SetWrapMode(GLenum aWrapMode)
  {
    mWrapMode = aWrapMode;
  }

  //TODO[nrc] will UpdateTexture work with the other kinds of textures?
  //default = no op
  virtual void
    UpdateTexture(const nsIntRegion& aRegion, PRInt8 *aData, PRUint32 aStride) {}

protected:
  ATextureOGL(CompositorOGL* aCompositorOGL)
    : mCompositorOGL(aCompositorOGL)
    , mWrapMode(LOCAL_GL_REPEAT)
  {}

  ATextureOGL(CompositorOGL* aCompositorOGL, gfx::IntSize aSize)
    : mCompositorOGL(aCompositorOGL)
    , mSize(aSize)
    , mWrapMode(LOCAL_GL_REPEAT)
  {}

  RefPtr<CompositorOGL> mCompositorOGL;
  gfx::IntSize mSize;
  GLenum mWrapMode;
};

class TextureOGL : public ATextureOGL
{
public:
  TextureOGL(CompositorOGL* aCompositorOGL, GLuint aTextureHandle, const gfx::IntSize& aSize)
    : ATextureOGL(aCompositorOGL, aSize)
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
  PRUint32 mPixelSize;
};

class ImageSourceOGL : public ImageSource, public ATextureOGL
{
public:
  ImageSourceOGL(CompositorOGL* aCompositorOGL);

  virtual ImageSourceType GetType() { return IMAGE_TEXTURE; }

  virtual void UpdateImage(const SharedImage& aImage);

  virtual void Composite(EffectChain& aEffectChain,
                         float aOpacity,
                         const gfx::Matrix4x4& aTransform,
                         const gfx::Point& aOffset,
                         const gfx::Filter aFilter);

  virtual void BindTexture(GLuint aTextureUnit)
  {
    mTexImage->BindTextureAndApplyFilter(aTextureUnit);

    NS_ASSERTION(mTexImage->GetContentType() == gfxASurface::CONTENT_ALPHA,
                 "OpenGL mask layers must be backed by alpha surfaces");
  }

  virtual GLuint GetTextureHandle()
  {
    return mTexImage->GetTextureID();
  }

  void SetForceSingleTile(bool aForceSingleTile)
  {
    mForceSingleTile = aForceSingleTile;
  }

private:
  nsRefPtr<TextureImage> mTexImage;
  bool mForceSingleTile;
};

class ImageSourceOGLShared : public ImageSource, public ATextureOGL
{
public:
  ImageSourceOGLShared(CompositorOGL* aCompositorOGL);

  ~ImageSourceOGLShared()
  {
    mCompositorOGL->gl()->ReleaseSharedHandle(mShareType, mSharedHandle);
  }

  virtual ImageSourceType GetType() { return IMAGE_SHARED; }


  virtual void UpdateImage(const SharedImage& aImage);

  virtual void Composite(EffectChain& aEffectChain,
                         float aOpacity,
                         const gfx::Matrix4x4& aTransform,
                         const gfx::Point& aOffset,
                         const gfx::Filter aFilter);

  virtual GLuint GetTextureHandle()
  {
    return mTextureHandle;
  }

private:
  bool mInverted;
  GLuint mTextureHandle;
  gl::SharedTextureHandle mSharedHandle;
  gl::TextureImage::TextureShareType mShareType;
};


class TextureOGLRaw : public ATextureOGL
{
public:
  TextureOGLRaw()
    : ATextureOGL(nullptr)
  {}

  ~TextureOGLRaw()
  {
    mTexture.Release();
  }

  virtual GLuint GetTextureHandle()
  {
    return mTexture.GetTextureID();
  }

  void Ensure(CompositorOGL* aCompositorOGL, const gfxIntSize& aSize)
  {
    mSize = gfx::IntSize(aSize.width, aSize.height);
    mCompositorOGL = aCompositorOGL;


    if (!mTexture.IsAllocated()) {
      mTexture.Allocate(mCompositorOGL->gl());

      NS_ASSERTION(mTexture.IsAllocated(),
                   "Texture allocation failed!");

      mCompositorOGL->gl()->MakeCurrent();
      mCompositorOGL->SetClamping(mTexture.GetTextureID());
    }
  }

  void Upload(gfxASurface* aSurface)
  {
    //TODO[nrc] I don't see why we need a new image surface here, but should check
    /*nsRefPtr<gfxASurface> surf = new gfxImageSurface(aData.mYChannel,
                                                     mSize,
                                                     aData.mYStride,
                                                     gfxASurface::ImageFormatA8);*/
    GLuint textureId = mTexture.GetTextureID();
    mCompositorOGL->gl()->UploadSurfaceToTexture(aSurface,
                                                 nsIntRect(0, 0, mSize.width, mSize.height),
                                                 textureId,
                                                 true);
    NS_ASSERTION(textureId == mTexture.GetTextureID(), "texture handle id changed");
  }

private:
  GLTexture mTexture;
};


}
}

#endif /* MOZILLA_GFX_TEXTUREOGL_H */
