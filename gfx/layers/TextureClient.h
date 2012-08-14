/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_TEXTURECLIENT_H
#define MOZILLA_GFX_TEXTURECLIENT_H

#include "mozilla/layers/LayersSurfaces.h"
#include "gfxASurface.h"
#include "Compositor.h"

//TODO[nrc] rename this file maybe? CompositorClient (also make CompositorHost?)

namespace mozilla {
namespace layers {

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
  //virtual TextureIdentifier GetIdentifierForProcess(/*base::ProcessHandle* aProcess*/) = 0; //TODO[nrc]

  //TODO[nrc] will this even work?
  virtual TextureIdentifier GetIdentifier() = 0;

  //TODO[nrc] comment
  virtual void EnsureTextureClient(gfx::IntSize aSize, gfxASurface::gfxContentType aType);

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

  //TODO[nrc] comment; could this have a better name?
  virtual SharedImage GetAsSharedImage() = 0;
};

//TODO[nrc] comments
//specialised for non-shared textures
class TextureClientTexture : public TextureClient
{
public:
  TextureClientTexture(PRUint32 aId, ShadowLayerForwarder* aLayerForwarder);
  ~TextureClientTexture();

  virtual TextureIdentifier GetIdentifier();
  virtual void EnsureTextureClient(gfx::IntSize aSize, gfxASurface::gfxContentType aType);
  virtual TemporaryRef<gfx::DrawTarget> LockDT();
  virtual already_AddRefed<gfxContext> LockContext();
  virtual void Unlock();
  virtual SharedImage GetAsSharedImage();

private:
  //TODO[nrc] can we hoist up some of these fields
  //TODO[nrc] change this to a layer manager and a ref ptr
  ShadowLayerForwarder* mLayerForwarder;
  nsRefPtr<gfxASurface> mSurface;
  gfxASurface::gfxContentType mType;
  SurfaceDescriptor mDescriptor;
  TextureIdentifier mIdentifier;
  gfx::IntSize mSize;
};

}
}
#endif
