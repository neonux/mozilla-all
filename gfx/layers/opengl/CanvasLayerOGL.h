/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_CANVASLAYEROGL_H
#define GFX_CANVASLAYEROGL_H


#include "LayerManagerOGL.h"
#include "gfxASurface.h"
//#include "ImageHost.h"
#if defined(MOZ_WIDGET_GTK2) && !defined(MOZ_PLATFORM_MAEMO)
#include "GLXLibrary.h"
#include "mozilla/X11Util.h"
#endif

namespace mozilla {
namespace layers {

class ImageHost;

class THEBES_API CanvasLayerOGL :
  public CanvasLayer,
  public LayerOGL
{
public:
  CanvasLayerOGL(LayerManagerOGL *aManager)
    : CanvasLayer(aManager, NULL),
      LayerOGL(aManager),
      mTexture(0),
      mDelayedUpdates(false)
#if defined(MOZ_WIDGET_GTK2) && !defined(MOZ_PLATFORM_MAEMO)
      ,mPixmap(0)
#endif
  { 
      mImplData = static_cast<LayerOGL*>(this);
  }
  ~CanvasLayerOGL() { Destroy(); }

  // CanvasLayer implementation
  virtual void Initialize(const Data& aData);

  // LayerOGL implementation
  virtual void Destroy();
  virtual Layer* GetLayer() { return this; }
  virtual void RenderLayer(const nsIntPoint& aOffset,
                           const nsIntRect& aClipRect,
                           Surface* aPreviousSurface = nullptr);
  virtual void CleanupResources();

protected:
  void UpdateSurface();

  nsRefPtr<gfxASurface> mCanvasSurface;
  nsRefPtr<GLContext> mCanvasGLContext;
  gl::ShaderProgramType mLayerProgram;
  RefPtr<gfx::DrawTarget> mDrawTarget;

  GLuint mTexture;

  bool mDelayedUpdates;
  bool mGLBufferIsPremultiplied;
  bool mNeedsYFlip;
#if defined(MOZ_WIDGET_GTK2) && !defined(MOZ_PLATFORM_MAEMO)
  GLXPixmap mPixmap;
#endif

  nsRefPtr<gfxImageSurface> mCachedTempSurface;
  gfxIntSize mCachedSize;
  gfxImageFormat mCachedFormat;

  gfxImageSurface* GetTempSurface(const gfxIntSize& aSize, const gfxImageFormat aFormat)
  {
    if (!mCachedTempSurface ||
        aSize.width != mCachedSize.width ||
        aSize.height != mCachedSize.height ||
        aFormat != mCachedFormat)
    {
      mCachedTempSurface = new gfxImageSurface(aSize, aFormat);
      mCachedSize = aSize;
      mCachedFormat = aFormat;
    }

    return mCachedTempSurface;
  }

  void DiscardTempSurface()
  {
    mCachedTempSurface = nullptr;
  }
};

// NB: eventually we'll have separate shadow canvas2d and shadow
// canvas3d layers, but currently they look the same from the
// perspective of the compositor process
class ShadowCanvasLayerOGL : public ShadowCanvasLayer,
                             public LayerOGL
{
  typedef gl::TextureImage TextureImage;

public:
  ShadowCanvasLayerOGL(LayerManagerOGL* aManager);
  virtual ~ShadowCanvasLayerOGL();

  // CanvasLayer impl
  virtual void Initialize(const Data& aData)
  {
    NS_RUNTIMEABORT("Incompatibe surface type");
  }

  // This isn't meaningful for shadow canvas.
  virtual void Updated(const nsIntRect&) {}

  virtual void SetAllocator(ISurfaceDeAllocator* aAllocator) {}

  // ShadowCanvasLayer impl
  virtual void Swap(const SharedImage& aNewFront,
                    bool needYFlip,
                    SharedImage* aNewBack)
  {
    NS_ERROR("Should never be called");
  }

  virtual void AddTextureHost(const TextureIdentifier& aTextureIdentifier, TextureHost* aTextureHost);

  virtual void SwapTexture(const TextureIdentifier& aTextureIdentifier,
                           const SharedImage& aFront,
                           SharedImage* aNewBack);

  virtual void Disconnect()
  {
    Destroy();
  }

  // LayerOGL impl
  void Destroy();
  Layer* GetLayer();
  virtual void RenderLayer(const nsIntPoint& aOffset,
                           const nsIntRect& aClipRect,
                           Surface* aPreviousSurface = nullptr);

  virtual void CleanupResources();

private:
  void EnsureImageHost(BufferType aHostType);

  RefPtr<ImageHost> mImageHost;
};

} /* layers */
} /* mozilla */
#endif /* GFX_IMAGELAYEROGL_H */
