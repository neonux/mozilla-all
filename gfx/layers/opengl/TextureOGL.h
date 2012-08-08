/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_TEXTUREOGL_H
#define MOZILLA_GFX_TEXTUREOGL_H

#include "CompositorOGL.h"
#include "TiledThebesLayerOGL.h"
#include "ImageLayerOGL.h"

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
};

class TextureOGL : public ATextureOGL
{
  // TODO: Make a better version of TextureOGL.
public:
  virtual GLuint GetTextureHandle()
  {
    return mTextureHandle;
  }

  //TODO[nrc] where do these get set?
  GLenum mFormat;
  GLenum mInternalFormat;
  GLenum mType;
  PRUint32 mPixelSize;
  gfx::IntSize mSize;
  RefPtr<CompositorOGL> mCompositorOGL;

  virtual void
    UpdateTexture(const nsIntRegion& aRegion, PRInt8 *aData, PRUint32 aStride) MOZ_OVERRIDE;

//private:
  GLuint mTextureHandle;
};

class ImageSourceOGL : public ImageSource, public ATextureOGL
{
public:
  ImageSourceOGL(const SurfaceDescriptor& aSurface, bool aForceSingleTile);

  virtual ImageSourceType GetType() { return IMAGE_OGL; }

  virtual void UpdateImage(const SharedImage& aImage);

  virtual void Composite(Compositor* aCompositor,
                         EffectChain& aEffectChain,
                         float aOpacity,
                         const gfx::Matrix4x4* aTransform,
                         const gfx::Point& aOffset,
                         const gfx::Filter aFilter);

  virtual GLuint GetTextureHandle()
  {
    return mTexImage->GetTextureID();
  }

private:
  gfx::IntSize mSize;
  GLenum mWrapMode;
  nsRefPtr<TextureImage> mTexImage;
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

  virtual void Composite(Compositor* aCompositor,
                         EffectChain& aEffectChain,
                         float aOpacity,
                         const gfx::Matrix4x4* aTransform,
                         const gfx::Point& aOffset,
                         const gfx::Filter aFilter);

  virtual GLuint GetTextureHandle()
  {
    return mTextureHandle;
  }

private:
  RefPtr<CompositorOGL> mCompositorOGL;
  bool mInverted;
  GLuint mTextureHandle;
  gfx::IntSize mSize;
  gl::SharedTextureHandle mSharedHandle;
  gl::TextureImage::TextureShareType mShareType;
};


class TextureOGLRaw : public ATextureOGL
{
public:
  ~TextureOGLRaw()
  {
    mTexture.Release();
  }

  virtual GLuint GetTextureHandle()
  {
    return mTexture.GetTextureID();
  }

  void Init(CompositorOGL* aCompositor, const gfxIntSize& aSize)
  {
    mSize = aSize;

    if (!mTexture.IsAllocated()) {
      mTexture.Allocate(aCompositor->gl());
    }

    NS_ASSERTION(mTexture.IsAllocated(),
                 "Texture allocation failed!");

    aCompositor->gl()->MakeCurrent();
    aCompositor->SetClamping(mTexture.GetTextureID());
  }

  void Upload(CompositorOGL* aCompositor, gfxASurface* aSurface)
  {
    //TODO[nrc] I don't see why we need a new image surface here, but should check
    /*nsRefPtr<gfxASurface> surf = new gfxImageSurface(aData.mYChannel,
                                                     mSize,
                                                     aData.mYStride,
                                                     gfxASurface::ImageFormatA8);*/
    GLuint textureId = mTexture.GetTextureID();
    aCompositor->gl()->UploadSurfaceToTexture(aSurface,
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
