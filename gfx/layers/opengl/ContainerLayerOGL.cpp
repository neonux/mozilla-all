/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ContainerLayerOGL.h"
#include "gfxUtils.h"
#include "Compositor.h"

namespace mozilla {
namespace layers {

template<class Container>
static void
ContainerInsertAfter(Container* aContainer, Layer* aChild, Layer* aAfter)
{
  aChild->SetParent(aContainer);
  if (!aAfter) {
    Layer *oldFirstChild = aContainer->GetFirstChild();
    aContainer->mFirstChild = aChild;
    aChild->SetNextSibling(oldFirstChild);
    aChild->SetPrevSibling(nullptr);
    if (oldFirstChild) {
      oldFirstChild->SetPrevSibling(aChild);
    } else {
      aContainer->mLastChild = aChild;
    }
    NS_ADDREF(aChild);
    aContainer->DidInsertChild(aChild);
    return;
  }
  for (Layer *child = aContainer->GetFirstChild(); 
       child; child = child->GetNextSibling()) {
    if (aAfter == child) {
      Layer *oldNextSibling = child->GetNextSibling();
      child->SetNextSibling(aChild);
      aChild->SetNextSibling(oldNextSibling);
      if (oldNextSibling) {
        oldNextSibling->SetPrevSibling(aChild);
      } else {
        aContainer->mLastChild = aChild;
      }
      aChild->SetPrevSibling(child);
      NS_ADDREF(aChild);
      aContainer->DidInsertChild(aChild);
      return;
    }
  }
  NS_WARNING("Failed to find aAfter layer!");
}

template<class Container>
static void
ContainerRemoveChild(Container* aContainer, Layer* aChild)
{
  if (aContainer->GetFirstChild() == aChild) {
    aContainer->mFirstChild = aContainer->GetFirstChild()->GetNextSibling();
    if (aContainer->mFirstChild) {
      aContainer->mFirstChild->SetPrevSibling(nullptr);
    } else {
      aContainer->mLastChild = nullptr;
    }
    aChild->SetNextSibling(nullptr);
    aChild->SetPrevSibling(nullptr);
    aChild->SetParent(nullptr);
    aContainer->DidRemoveChild(aChild);
    NS_RELEASE(aChild);
    return;
  }
  Layer *lastChild = nullptr;
  for (Layer *child = aContainer->GetFirstChild(); child; 
       child = child->GetNextSibling()) {
    if (child == aChild) {
      // We're sure this is not our first child. So lastChild != NULL.
      lastChild->SetNextSibling(child->GetNextSibling());
      if (child->GetNextSibling()) {
        child->GetNextSibling()->SetPrevSibling(lastChild);
      } else {
        aContainer->mLastChild = lastChild;
      }
      child->SetNextSibling(nullptr);
      child->SetPrevSibling(nullptr);
      child->SetParent(nullptr);
      aContainer->DidRemoveChild(aChild);
      NS_RELEASE(aChild);
      return;
    }
    lastChild = child;
  }
}

template<class Container>
static void
ContainerDestroy(Container* aContainer)
 {
  if (!aContainer->mDestroyed) {
    while (aContainer->mFirstChild) {
      aContainer->GetFirstChildOGL()->Destroy();
      aContainer->RemoveChild(aContainer->mFirstChild);
    }
    aContainer->mDestroyed = true;
  }
}

template<class Container>
static void
ContainerCleanupResources(Container* aContainer)
{
  for (Layer* l = aContainer->GetFirstChild(); l; l = l->GetNextSibling()) {
    LayerOGL* layerToRender = static_cast<LayerOGL*>(l->ImplData());
    layerToRender->CleanupResources();
  }
}

static inline LayerOGL*
GetNextSibling(LayerOGL* aLayer)
{
   Layer* layer = aLayer->GetLayer()->GetNextSibling();
   return layer ? static_cast<LayerOGL*>(layer->
                                         ImplData())
                 : nullptr;
}

static bool
HasOpaqueAncestorLayer(Layer* aLayer)
{
  for (Layer* l = aLayer->GetParent(); l; l = l->GetParent()) {
    if (l->GetContentFlags() & Layer::CONTENT_OPAQUE)
      return true;
  }
  return false;
}

template<class Container>
static void
ContainerRender(Container* aContainer,
                Surface* aPreviousSurface,
                const nsIntPoint& aOffset,
                LayerManagerOGL* aManager,
                const nsIntRect& aClipRect)
{
  /**
   * Setup our temporary surface for rendering the contents of this container.
   */
  GLuint containerSurface;
  RefPtr<Surface> surface;

  nsIntPoint childOffset(aOffset);
  nsIntRect visibleRect = aContainer->GetEffectiveVisibleRegion().GetBounds();

  nsIntRect cachedScissor = aClipRect;
  aContainer->mSupportsComponentAlphaChildren = false;

  float opacity = aContainer->GetEffectiveOpacity();
  bool needsSurface = aContainer->UseIntermediateSurface();
  if (needsSurface) {
    SurfaceInitMode mode = INIT_MODE_CLEAR;
    bool surfaceCopyNeeded = false;
    gfx::IntRect surfaceRect = gfx::IntRect(visibleRect.x, visibleRect.y, visibleRect.width,
                                            visibleRect.height);
    if (aContainer->GetEffectiveVisibleRegion().GetNumRects() == 1 && 
        (aContainer->GetContentFlags() & Layer::CONTENT_OPAQUE))
    {
      // don't need a background, we're going to paint all opaque stuff
      aContainer->mSupportsComponentAlphaChildren = true;
      mode = INIT_MODE_NONE;
    } else {
      const gfx3DMatrix& transform3D = aContainer->GetEffectiveTransform();
      gfxMatrix transform;
      // If we have an opaque ancestor layer, then we can be sure that
      // all the pixels we draw into are either opaque already or will be
      // covered by something opaque. Otherwise copying up the background is
      // not safe.
      if (HasOpaqueAncestorLayer(aContainer) &&
          transform3D.Is2D(&transform) && !transform.HasNonIntegerTranslation()) {
        surfaceCopyNeeded = true;
        surfaceRect.x += transform.x0;
        surfaceRect.y += transform.y0;
        aContainer->mSupportsComponentAlphaChildren = true;
      }
    }

    aContainer->gl()->PushViewportRect();
    surfaceRect -= gfx::IntPoint(childOffset.x, childOffset.y);
    if (surfaceCopyNeeded) {
      surface = aManager->GetCompositor()->CreateSurfaceFromSurface(surfaceRect, aPreviousSurface);
    } else {
      surface = aManager->GetCompositor()->CreateSurface(surfaceRect, mode);
    }
    aManager->GetCompositor()->SetSurfaceTarget(surface);
    childOffset.x = visibleRect.x;
    childOffset.y = visibleRect.y;
  } else {
    surface = aPreviousSurface;
    aContainer->mSupportsComponentAlphaChildren = (aContainer->GetContentFlags() & Layer::CONTENT_OPAQUE) ||
      (aContainer->GetParent() && aContainer->GetParent()->SupportsComponentAlphaChildren());
  }

  nsAutoTArray<Layer*, 12> children;
  aContainer->SortChildrenBy3DZOrder(children);

  /**
   * Render this container's contents.
   */
  for (PRUint32 i = 0; i < children.Length(); i++) {
    LayerOGL* layerToRender = static_cast<LayerOGL*>(children.ElementAt(i)->ImplData());

    if (layerToRender->GetLayer()->GetEffectiveVisibleRegion().IsEmpty()) {
      continue;
    }

    nsIntRect scissorRect = layerToRender->GetLayer()->
        CalculateScissorRect(cachedScissor, &aManager->GetWorldTransform());
    if (scissorRect.IsEmpty()) {
      continue;
    }

    layerToRender->RenderLayer(childOffset, scissorRect, surface);
    aContainer->gl()->MakeCurrent();
  }


  if (needsSurface) {
    // Unbind the current surface and rebind the previous one.
    aManager->GetCompositor()->SetSurfaceTarget(aPreviousSurface);
#ifdef MOZ_DUMP_PAINTING
    /* This needs to be re-written to use the Compositor API (which will likely require
     * additions to the Compositor API).
     */
    /*
    if (gfxUtils::sDumpPainting) {
      nsRefPtr<gfxImageSurface> surf = 
        aContainer->gl()->GetTexImage(containerSurface, true, aManager->GetFBOLayerProgramType());

      WriteSnapshotToDumpFile(aContainer, surf);
      */
    }
#endif

    // TODO: Handle this in the Compositor.
    // Restore the viewport
    aContainer->gl()->PopViewportRect();
    nsIntRect viewport = aContainer->gl()->ViewportRect();
    aManager->SetupPipeline(viewport.width, viewport.height);

    MaskType maskType = MaskNone;
    if (aContainer->GetMaskLayer()) {
      if (!aContainer->GetTransform().CanDraw2D()) {
        maskType = Mask3d;
      } else {
        maskType = Mask2d;
      }
    }

    // TODO: Handle mask layers.
    EffectChain effectChain;
    RefPtr<Effect> effect = new EffectSurface(surface);
    effectChain.mEffects[EFFECT_SURFACE] = effect;
    gfx::Matrix4x4 transform;
    aManager->ToMatrix4x4(aContainer->GetEffectiveTransform(), transform);

    gfx::Rect rect(visibleRect.x, visibleRect.y, visibleRect.width, visibleRect.height);
    aManager->GetCompositor()->DrawQuad(rect, nullptr, nullptr, effectChain, opacity,
                                        transform, gfx::Point(aOffset.x, aOffset.y));

  }
}

ContainerLayerOGL::ContainerLayerOGL(LayerManagerOGL *aManager)
  : ContainerLayer(aManager, NULL)
  , LayerOGL(aManager)
{
  mImplData = static_cast<LayerOGL*>(this);
}

ContainerLayerOGL::~ContainerLayerOGL()
{
  Destroy();
}

void
ContainerLayerOGL::InsertAfter(Layer* aChild, Layer* aAfter)
{
  ContainerInsertAfter(this, aChild, aAfter);
}

void
ContainerLayerOGL::RemoveChild(Layer *aChild)
{
  ContainerRemoveChild(this, aChild);
}

void
ContainerLayerOGL::Destroy()
{
  ContainerDestroy(this);
}

LayerOGL*
ContainerLayerOGL::GetFirstChildOGL()
{
  if (!mFirstChild) {
    return nullptr;
  }
  return static_cast<LayerOGL*>(mFirstChild->ImplData());
}

void
ContainerLayerOGL::RenderLayer(const nsIntPoint& aOffset,
                               const nsIntRect& aClipRect,
                               Surface* aPreviousSurface)
{
  ContainerRender(this, aPreviousSurface, aOffset, mOGLManager, aClipRect);
}

void
ContainerLayerOGL::CleanupResources()
{
  ContainerCleanupResources(this);
}

ShadowContainerLayerOGL::ShadowContainerLayerOGL(LayerManagerOGL *aManager)
  : ShadowContainerLayer(aManager, NULL)
  , LayerOGL(aManager)
{
  mImplData = static_cast<LayerOGL*>(this);
}
 
ShadowContainerLayerOGL::~ShadowContainerLayerOGL()
{
  // We don't Destroy() on destruction here because this destructor
  // can be called after remote content has crashed, and it may not be
  // safe to free the IPC resources of our children.  Those resources
  // are automatically cleaned up by IPDL-generated code.
  //
  // In the common case of normal shutdown, either
  // LayerManagerOGL::Destroy(), a parent
  // *ContainerLayerOGL::Destroy(), or Disconnect() will trigger
  // cleanup of our resources.
  while (mFirstChild) {
    ContainerRemoveChild(this, mFirstChild);
  }
}

void
ShadowContainerLayerOGL::InsertAfter(Layer* aChild, Layer* aAfter)
{
  ContainerInsertAfter(this, aChild, aAfter);
}

void
ShadowContainerLayerOGL::RemoveChild(Layer *aChild)
{
  ContainerRemoveChild(this, aChild);
}

void
ShadowContainerLayerOGL::Destroy()
{
  ContainerDestroy(this);
}

LayerOGL*
ShadowContainerLayerOGL::GetFirstChildOGL()
{
  if (!mFirstChild) {
    return nullptr;
   }
  return static_cast<LayerOGL*>(mFirstChild->ImplData());
}

void
ShadowContainerLayerOGL::RenderLayer(const nsIntPoint& aOffset,
                                     const nsIntRect& aClipRect,
                                     Surface* aPreviousSurface)
{
  ContainerRender(this, aPreviousSurface, aOffset, mOGLManager, aClipRect);
}

void
ShadowContainerLayerOGL::CleanupResources()
{
  ContainerCleanupResources(this);
}

ShadowRefLayerOGL::ShadowRefLayerOGL(LayerManagerOGL* aManager)
  : ShadowRefLayer(aManager, NULL)
  , LayerOGL(aManager)
{
  mImplData = static_cast<LayerOGL*>(this);
}

ShadowRefLayerOGL::~ShadowRefLayerOGL()
{
  Destroy();
}

void
ShadowRefLayerOGL::Destroy()
{
  MOZ_ASSERT(!mFirstChild);
  mDestroyed = true;
}

LayerOGL*
ShadowRefLayerOGL::GetFirstChildOGL()
{
  if (!mFirstChild) {
    return nullptr;
   }
  return static_cast<LayerOGL*>(mFirstChild->ImplData());
}

void
ShadowRefLayerOGL::RenderLayer(const nsIntPoint& aOffset,
                               const nsIntRect& aClipRect,
                               Surface* aPreviousSurface)
{
  ContainerRender(this, aPreviousSurface, aOffset, mOGLManager, aClipRect);
}

void
ShadowRefLayerOGL::CleanupResources()
{
}

} /* layers */
} /* mozilla */
