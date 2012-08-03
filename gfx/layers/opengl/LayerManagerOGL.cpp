/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/PLayers.h"

/* This must occur *after* layers/PLayers.h to avoid typedefs conflicts. */
#include "mozilla/Util.h"

#include "LayerManagerOGL.h"
#include "ThebesLayerOGL.h"
#include "ContainerLayerOGL.h"
#include "ImageLayerOGL.h"
#include "ColorLayerOGL.h"
#include "CanvasLayerOGL.h"
#include "TiledThebesLayerOGL.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Preferences.h"
#include "TexturePoolOGL.h"

#include "gfxContext.h"
#include "gfxUtils.h"
#include "gfxPlatform.h"
#include "nsIWidget.h"

#include "GLContext.h"
#include "GLContextProvider.h"

#include "nsIServiceManager.h"
#include "nsIConsoleService.h"

#include "gfxCrashReporterUtils.h"

#include "sampler.h"

#ifdef MOZ_WIDGET_ANDROID
#include <android/log.h>
#endif

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;
using namespace mozilla::gl;


/**
 * LayerManagerOGL
 */
LayerManagerOGL::LayerManagerOGL(nsIWidget *aWidget, int aSurfaceWidth, int aSurfaceHeight,
                                 bool aIsRenderingToEGLSurface)
{
  mCompositor = new CompositorOGL(aWidget, aSurfaceWidth, aSurfaceHeight, aIsRenderingToEGLSurface);
}

void
LayerManagerOGL::Destroy()
{
  if (!mDestroyed) {
    if (mRoot) {
      RootLayer()->Destroy();
    }
    mRoot = nullptr;

    mCompositor->Destroy();

    mDestroyed = true;
  }
}


void
LayerManagerOGL::BeginTransaction()
{
}

void
LayerManagerOGL::BeginTransactionWithTarget(gfxContext *aTarget)
{
#ifdef MOZ_LAYERS_HAVE_LOG
  MOZ_LAYERS_LOG(("[----- BeginTransaction"));
  Log();
#endif

  if (mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return;
  }

  mCompositor->SetTarget(aTarget);
}

bool
LayerManagerOGL::EndEmptyTransaction()
{
  if (!mRoot)
    return false;

  EndTransaction(nullptr, nullptr);
  return true;
}

void
LayerManagerOGL::EndTransaction(DrawThebesLayerCallback aCallback,
                                void* aCallbackData,
                                EndTransactionFlags aFlags)
{
#ifdef MOZ_LAYERS_HAVE_LOG
  MOZ_LAYERS_LOG(("  ----- (beginning paint)"));
  Log();
#endif

  if (mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return;
  }

  if (mRoot && !(aFlags & END_NO_IMMEDIATE_REDRAW)) {
    // The results of our drawing always go directly into a pixel buffer,
    // so we don't need to pass any global transform here.
    mRoot->ComputeEffectiveTransforms(gfx3DMatrix());

    mThebesLayerCallback = aCallback;
    mThebesLayerCallbackData = aCallbackData;

    Render();

    mThebesLayerCallback = nullptr;
    mThebesLayerCallbackData = nullptr;
  }

  mCompositor->SetTarget(nullptr);

#ifdef MOZ_LAYERS_HAVE_LOG
  Log();
  MOZ_LAYERS_LOG(("]----- EndTransaction"));
#endif
}

already_AddRefed<gfxASurface>
LayerManagerOGL::CreateOptimalMaskSurface(const gfxIntSize &aSize)
{
  return gfxPlatform::GetPlatform()->
    CreateOffscreenImageSurface(aSize, gfxASurface::CONTENT_ALPHA);
}

already_AddRefed<ThebesLayer>
LayerManagerOGL::CreateThebesLayer()
{
  if (mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nullptr;
  }

  nsRefPtr<ThebesLayer> layer = new ThebesLayerOGL(this);
  return layer.forget();
}

already_AddRefed<ContainerLayer>
LayerManagerOGL::CreateContainerLayer()
{
  if (mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nullptr;
  }

  nsRefPtr<ContainerLayer> layer = new ContainerLayerOGL(this);
  return layer.forget();
}

already_AddRefed<ImageLayer>
LayerManagerOGL::CreateImageLayer()
{
  if (mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nullptr;
  }

  nsRefPtr<ImageLayer> layer = new ImageLayerOGL(this);
  return layer.forget();
}

already_AddRefed<ColorLayer>
LayerManagerOGL::CreateColorLayer()
{
  if (mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nullptr;
  }

  nsRefPtr<ColorLayer> layer = new ColorLayerOGL(this);
  return layer.forget();
}

already_AddRefed<CanvasLayer>
LayerManagerOGL::CreateCanvasLayer()
{
  if (mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nullptr;
  }

  nsRefPtr<CanvasLayer> layer = new CanvasLayerOGL(this);
  return layer.forget();
}

LayerOGL*
LayerManagerOGL::RootLayer() const
{
  if (mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nullptr;
  }

  return static_cast<LayerOGL*>(mRoot->ImplData());
}

void
LayerManagerOGL::Render()
{
  SAMPLE_LABEL("LayerManagerOGL", "Render");
  if (mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return;
  }

  if (mRoot->GetClipRect()) {
    nsIntRect clipRect = *mRoot->GetClipRect();
    WorldTransformRect(clipRect);
    mCompositor->BeginFrame(&Rect(clipRect.x, clipRect.y, clipRect.width, clipRect.height), mWorldMatrix);
  } else {
    mCompositor->BeginFrame(nullptr, mWorldMatrix);
  }

  // Render our layers.
  RootLayer()->RenderLayer(0, nsIntPoint(0, 0));

  mCompositor->EndFrame();
}

void
LayerManagerOGL::SetWorldTransform(const gfxMatrix& aMatrix)
{
  NS_ASSERTION(aMatrix.PreservesAxisAlignedRectangles(),
               "SetWorldTransform only accepts matrices that satisfy PreservesAxisAlignedRectangles");
  NS_ASSERTION(!aMatrix.HasNonIntegerScale(),
               "SetWorldTransform only accepts matrices with integer scale");

  mWorldMatrix = aMatrix;
}

gfxMatrix&
LayerManagerOGL::GetWorldTransform(void)
{
  return mWorldMatrix;
}

void
LayerManagerOGL::WorldTransformRect(nsIntRect& aRect)
{
  gfxRect grect(aRect.x, aRect.y, aRect.width, aRect.height);
  grect = mWorldMatrix.TransformBounds(grect);
  aRect.SetRect(grect.X(), grect.Y(), grect.Width(), grect.Height());
}

already_AddRefed<ShadowThebesLayer>
LayerManagerOGL::CreateShadowThebesLayer()
{
  if (LayerManagerOGL::mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nullptr;
  }
#ifdef FORCE_BASICTILEDTHEBESLAYER
  return nsRefPtr<ShadowThebesLayer>(new TiledThebesLayerOGL(this)).forget();
#else
  return nsRefPtr<ShadowThebesLayerOGL>(new ShadowThebesLayerOGL(this)).forget();
#endif
}

already_AddRefed<ShadowContainerLayer>
LayerManagerOGL::CreateShadowContainerLayer()
{
  if (LayerManagerOGL::mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nullptr;
  }
  return nsRefPtr<ShadowContainerLayerOGL>(new ShadowContainerLayerOGL(this)).forget();
}

already_AddRefed<ShadowImageLayer>
LayerManagerOGL::CreateShadowImageLayer()
{
  if (LayerManagerOGL::mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nullptr;
  }
  return nsRefPtr<ShadowImageLayerOGL>(new ShadowImageLayerOGL(this)).forget();
}

already_AddRefed<ShadowColorLayer>
LayerManagerOGL::CreateShadowColorLayer()
{
  if (LayerManagerOGL::mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nullptr;
  }
  return nsRefPtr<ShadowColorLayerOGL>(new ShadowColorLayerOGL(this)).forget();
}

already_AddRefed<ShadowCanvasLayer>
LayerManagerOGL::CreateShadowCanvasLayer()
{
  if (LayerManagerOGL::mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nullptr;
  }
  return nsRefPtr<ShadowCanvasLayerOGL>(new ShadowCanvasLayerOGL(this)).forget();
}

already_AddRefed<ShadowRefLayer>
LayerManagerOGL::CreateShadowRefLayer()
{
  if (LayerManagerOGL::mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nullptr;
  }
  return nsRefPtr<ShadowRefLayerOGL>(new ShadowRefLayerOGL(this)).forget();
}

void
LayerManagerOGL::ToMatrix4x4(const gfx3DMatrix &aIn, gfx::Matrix4x4 &aOut)
{
  aOut._11 = aIn._11;
  aOut._12 = aIn._12;
  aOut._13 = aIn._13;
  aOut._14 = aIn._14;
  aOut._21 = aIn._21;
  aOut._22 = aIn._22;
  aOut._23 = aIn._23;
  aOut._24 = aIn._24;
  aOut._31 = aIn._31;
  aOut._32 = aIn._32;
  aOut._33 = aIn._33;
  aOut._34 = aIn._34;
  aOut._41 = aIn._41;
  aOut._42 = aIn._42;
  aOut._43 = aIn._43;
  aOut._44 = aIn._44;
}

} /* layers */
} /* mozilla */
