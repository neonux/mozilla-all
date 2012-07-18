/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Layers.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace layers {

LayerTreeManager::LayerTreeManager()
  : mSnapEffectiveTransforms(false)
  , mRoot(NULL)
  , mCallback(NULL)
{
}

void
LayerTreeManager::BeginTransaction()
{
}

bool
LayerTreeManager::EndEmptyTransaction()
{
  return true;
}

void
LayerTreeManager::EndTransaction(DrawThebesLayerCallback aCallback,
                                 void* aCallbackData,
                                 EndTransactionFlags aFlags)
{
}

void
LayerTreeManager::SetRoot(Layer *aRoot)
{
  mRoot = aRoot;
}

already_AddRefed<gfxASurface>
LayerTreeManager::CreateOptimalSurface(const gfxIntSize &aSize,
                                       gfxASurface::gfxImageFormat imageFormat)
{
  return nsnull;
}

int32_t
LayerTreeManager::GetMaxTextureSize() const
{
  return INT32_MAX;
}

already_AddRefed<ThebesLayer>
LayerTreeManager::CreateThebesLayer()
{
  return new ThebesLayer(this);
}

already_AddRefed<ContainerLayer>
LayerTreeManager::CreateContainerLayer()
{
  return new ContainerLayer(this);
}

already_AddRefed<ColorLayer>
LayerTreeManager::CreateColorLayer()
{
  return new ColorLayer(this);
}

TemporaryRef<DrawTarget>
LayerTreeManager::CreateDrawTarget(const mozilla::gfx::IntSize &aSize,
                                   mozilla::gfx::SurfaceFormat aFormat)
{
  return NULL;
}

}
}
