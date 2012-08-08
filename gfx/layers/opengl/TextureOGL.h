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

class ATextureOGL : public Texture
{
public:
  virtual GLuint GetTextureHandle() = 0;

  //TODO[nrc] will UpdateTexture work with the other kinds of textures?
  //default = no op
  virtual void
    UpdateTexture(const nsIntRegion& aRegion, PRInt8 *aData, PRUint32 aStride) {}

  //TODO[nrc] make protected
  RefPtr<CompositorOGL> mCompositorOGL;

protected:
  ATextureOGL(CompositorOGL* aCompositorOGL)
    : mCompositorOGL(aCompositorOGL)
  {}
};

class TextureOGL : public ATextureOGL
{
  // TODO: Make a better version of TextureOGL.
public:
  TextureOGL(CompositorOGL* aCompositorOGL)
    : ATextureOGL(aCompositorOGL)
  {}

  virtual GLuint GetTextureHandle()
  {
    return mTextureHandle;
  }

  //TODO[nrc] where do these get set?
  GLenum mFormat;
  GLenum mInternalFormat;
  GLenum mType;
  GLenum mWrapMode;
  PRUint32 mPixelSize;
  gfx::IntSize mSize;

  virtual void
    UpdateTexture(const nsIntRegion& aRegion, PRInt8 *aData, PRUint32 aStride) MOZ_OVERRIDE;

//private:
  GLuint mTextureHandle;
};

class ImageSourceOGL : public ImageSource, public ATextureOGL
{
public:
  ImageSourceOGL(CompositorOGL* aCompositorOGL, const SurfaceDescriptor& aSurface, bool aForceSingleTile);

  virtual ImageSourceType GetType() { return IMAGE_OGL; }

  virtual void UpdateImage(const SharedImage& aImage);

  virtual void Composite(EffectChain& aEffectChain,
                         float aOpacity,
                         const gfx::Matrix4x4& aTransform,
                         const gfx::Point& aOffset,
                         const gfx::Filter aFilter);

  virtual GLuint GetTextureHandle()
  {
    return mTexImage->GetTextureID();
  }

private:
  nsIntSize mSize;
  GLenum mWrapMode;
  nsRefPtr<TextureImage> mTexImage;
  bool mForceSingleTile;
};

class ImageSourceOGLShared : public ImageSource, public ATextureOGL
{
public:
  ImageSourceOGLShared(CompositorOGL* aCompositorOGL, const SharedTextureDescriptor& aTexture);

  ~ImageSourceOGLShared()
  {
    mCompositorOGL->gl()->ReleaseSharedHandle(mShareType, mSharedHandle);
  }

  virtual ImageSourceType GetType() { return IMAGE_OGL_SHARED; }


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
  nsIntSize mSize;
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

  void Init(CompositorOGL* aCompositorOGL, const gfxIntSize& aSize)
  {
    mSize = aSize;
    mCompositorOGL = aCompositorOGL;


    if (!mTexture.IsAllocated()) {
      mTexture.Allocate(mCompositorOGL->gl());
    }

    NS_ASSERTION(mTexture.IsAllocated(),
                 "Texture allocation failed!");

    mCompositorOGL->gl()->MakeCurrent();
    mCompositorOGL->SetClamping(mTexture.GetTextureID());
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
  gfxIntSize mSize;
};


}
}

#endif /* MOZILLA_GFX_TEXTUREOGL_H */
