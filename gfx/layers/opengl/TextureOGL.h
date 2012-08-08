/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_TEXTUREOGL_H
#define MOZILLA_GFX_TEXTUREOGL_H

#include "CompositorOGL.h"
#include "TiledThebesLayerOGL.h"

namespace mozilla {

namespace layers {

//TODO[nrc] unbreak CompositorOGL::DrawQuad and effects now that textures are a bit more heavyweight
// The image things should be textures and inherit from TextureOGL
// what about UpdateTexture?
// Not sure how the graph should look though
// what about YUVImage?


class TextureOGL : public Texture
{
  // TODO: Make a better version of TextureOGL.
public:
  virtual GLuint GetTextureHandle()
  {
    return mTextureHandle;
  }

  //TODO[nrc] where do these get set?
  //TODO[nrc] will UpdateTexture work with the other kinds of textures?

  GLenum mFormat;
  GLenum mInternalFormat;
  GLenum mType;
  PRUint32 mPixelSize;
  gfx::IntSize mSize;
  GLuint mTextureHandle;

  virtual void
    UpdateTexture(const nsIntRegion& aRegion, PRInt8 *aData, PRUint32 aStride) MOZ_OVERRIDE;

};

// TODO[nrc] move to ImageLayer ?
class ImageTextureOGL : public Image
{
public:
  ImageTextureOGL(const SurfaceDescriptor& aSurface, bool aForceSingleTile);

  virtual ImageTextureType GetType() { return IMAGE_OGL; }

  virtual void UpdateImage(const SharedImage& aImage);

  virtual void Composite(Compositor* aCompositor,
                         EffectChain& aEffectChain,
                         float aOpacity,
                         const gfx::Matrix4x4* aTransform,
                         const nsIntPoint& aOffset,
                         const gfx::Filter aFilter);

  GLuint GetTextureHandle()
  {
    return mTexImage->GetTextureID();
  }

private:
  gfx::IntSize mSize;
  GLenum mWrapMode;
  nsRefPtr<TextureImage> mTexImage;
};

class ImageTextureOGLShared : public Image
{
public:
  ImageTextureOGLShared(CompositorOGL* aCompositorOGL, const SharedTextureDescriptor& aTexture);

  ~ImageTextureOGLShared()
  {
    mCompositorOGL->gl()->ReleaseSharedHandle(mShareType, mSharedHandle);
  }

  virtual ImageTextureType GetType() { return IMAGE_OGL_SHARED; }


  virtual void UpdateImage(const SharedImage& aImage);

  virtual void Composite(Compositor* aCompositor,
                         EffectChain& aEffectChain,
                         float aOpacity,
                         const gfx::Matrix4x4* aTransform,
                         const nsIntPoint& aOffset,
                         const gfx::Filter aFilter);

  GLuint GetTextureHandle()
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


//TODO[nrc] not sure if this is the best place for this
// or even if it should exist in this form
// used for YUV textures
class TextureOGLRaw
{
public:
  ~TextureOGLRaw()
  {
    mTexture.Release();
  }

  GLuint GetTextureHandle()
  {
    return mTexture.GetTextureID();
  }

  void Init(CompositorOGL* aCompositor, const gfxIntSize& aSize)
  {
    mSize = aSize;

    if (!mTexture.IsAllocated()) {
      mTexture.Allocate(gl());
    }

    NS_ASSERTION(mTexture.IsAllocated(),
                 "Texture allocation failed!");

    aCompositor->gl()->MakeCurrent();
    aCompositor->SetClamping(mTexture.GetTextureID());
  }

  void Upload(CompositorOGL* aCompositor, gfxASurface aSurface)
  {
    //TODO[nrc] I don't see why we need a new image surface here, but should check
    /*nsRefPtr<gfxASurface> surf = new gfxImageSurface(aData.mYChannel,
                                                     mSize,
                                                     aData.mYStride,
                                                     gfxASurface::ImageFormatA8);*/
    aCompositor->gl->UploadSurfaceToTexture(aSurface,
                                            nsIntRect(0, 0, mSize.width, mSize.height),
                                            mTexture.GetTextureID(),
                                            true);
  }

private:
  GLTexture mTexture;
  gfxIntSize mSize;
};


}
}

#endif /* MOZILLA_GFX_TEXTUREOGL_H */
