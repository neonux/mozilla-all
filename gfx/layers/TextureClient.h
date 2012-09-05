/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_TEXTURECLIENT_H
#define MOZILLA_GFX_TEXTURECLIENT_H

#include "mozilla/layers/LayersSurfaces.h"
#include "gfxASurface.h"
#include "Compositor.h"
#include "mozilla/layers/ShadowLayers.h"

//TODO[nrc] rename this file maybe? CompositorClient (also make CompositorHost?)

namespace mozilla {

namespace gl {
  class GLContext;
}

namespace layers {

/* This class allows texture clients to draw into textures through Azure or
 * thebes and applies locking semantics to allow GPU or CPU level
 * synchronization.
 */
class TextureClient : public RefCounted<TextureClient>
{
public:
  virtual ~TextureClient() {}
  /* This will return an identifier that can be sent accross a process or
   * thread boundary and used to construct a DrawableTextureHost object
   * which can then be used as a texture for rendering by a compatible
   * compositor. This texture should have been created with the
   * TextureHostIdentifier specified by the compositor that this identifier
   * is to be used with. If the process is identical to the current process
   * this may return the same object and will only be thread safe.
   */
  //virtual TextureIdentifier GetIdentifierForProcess(/*base::ProcessHandle* aProcess*/) = 0; //TODO[nrc]

  virtual const TextureIdentifier& GetIdentifier()
  {
    return mIdentifier;
  }
  
  void SetDescriptor(PRUint32 aDescriptor)
  {
    mIdentifier.mDescriptor = aDescriptor;
  }

  /* This requests a DrawTarget to draw into the current texture. Once the
   * user is finished with the DrawTarget it should call Unlock.
   */
  virtual TemporaryRef<gfx::DrawTarget> LockDT() { return nullptr; } 

  /* This requests a gfxContext to draw into the current texture. Once the
   * user is finished with the gfxContext it should call Unlock.
   */
  virtual already_AddRefed<gfxContext> LockContext()  { return nullptr; }

  //TODO[nrc] comments
  virtual gfxImageSurface* LockImageSurface() { return nullptr; }
  virtual gfxASurface* LockSurface() { return nullptr; }
  virtual SharedTextureHandle LockHandle(GLContext* aGL, TextureImage::TextureShareType aFlags) { return 0; }
  virtual void EnsureTextureClient(gfx::IntSize aSize, gfxASurface::gfxContentType aType) = 0;

  /* This unlocks the current DrawableTexture and allows the host to composite
   * it directly.
   */
  virtual void Unlock() {}

  void SetDescriptor(const SurfaceDescriptor& aDescriptor)
  {
    mDescriptor = aDescriptor;
  }

  SharedImage GetAsSharedImage()
  {
    return SharedImage(mDescriptor);
  }

protected:
  TextureClient(ShadowLayerForwarder* aLayerForwarder, ImageHostType aImageType)
    : mLayerForwarder(aLayerForwarder)
  {
    mIdentifier.mImageType = aImageType;
  }

  ShadowLayerForwarder* mLayerForwarder;
  SurfaceDescriptor mDescriptor;
  TextureIdentifier mIdentifier;
};

class TextureClientShmem : public TextureClient
{
public:
  virtual ~TextureClientShmem();

  virtual already_AddRefed<gfxContext> LockContext();
  virtual gfxImageSurface* LockImageSurface();
  virtual gfxASurface* LockSurface() { return GetSurface(); }
  virtual void Unlock();
  virtual void EnsureTextureClient(gfx::IntSize aSize, gfxASurface::gfxContentType aType);
private:
  TextureClientShmem(ShadowLayerForwarder* aLayerForwarder, ImageHostType aImageType);

  gfxASurface* GetSurface();

  nsRefPtr<gfxASurface> mSurface;
  nsRefPtr<gfxImageSurface> mSurfaceAsImage;

  gfxASurface::gfxContentType mContentType;
  gfx::IntSize mSize;

  friend class CompositingFactory;
};

// this class is just a place holder really
class TextureClientShared : public TextureClient
{
public:
  virtual ~TextureClientShared();

  virtual void EnsureTextureClient(gfx::IntSize aSize, gfxASurface::gfxContentType aType) {}

protected:
  TextureClientShared(ShadowLayerForwarder* aLayerForwarder, ImageHostType aImageType)
    : TextureClient(aLayerForwarder, aImageType)
  {
    mIdentifier.mTextureType = TEXTURE_SHARED;
  }

  friend class CompositingFactory;
};

class TextureClientSharedGL : public TextureClientShared
{
public:
  virtual ~TextureClientSharedGL();
  virtual void EnsureTextureClient(gfx::IntSize aSize, gfxASurface::gfxContentType aType);
  virtual SharedTextureHandle LockHandle(GLContext* aGL, TextureImage::TextureShareType aFlags);
  virtual void Unlock();
protected:
  TextureClientSharedGL(ShadowLayerForwarder* aLayerForwarder, ImageHostType aImageType);

  GLContext* mGL;
  gfx::IntSize mSize;

  friend class CompositingFactory;
};

}
}
#endif
