/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_BASICTHEBESLAYER_H
#define GFX_BASICTHEBESLAYER_H

#include "mozilla/layers/PLayersParent.h"
#include "BasicBuffers.h"
#include "BasicLayersImpl.h"
#include "ThebesBufferClient.h"

namespace mozilla {
namespace layers {

class BasicThebesLayer : public ThebesLayer, public BasicImplData {
public:
  typedef ThebesLayerBuffer::PaintState PaintState;
  typedef ThebesLayerBuffer::ContentType ContentType;

  BasicThebesLayer(BasicLayerManager* aLayerManager) :
    ThebesLayer(aLayerManager, static_cast<BasicImplData*>(this)),
    mContentClient(new ContentClientBasic(aLayerManager))
  {
    MOZ_COUNT_CTOR(BasicThebesLayer);
  }
  virtual ~BasicThebesLayer()
  {
    MOZ_COUNT_DTOR(BasicThebesLayer);
  }

  virtual void SetVisibleRegion(const nsIntRegion& aRegion)
  {
    NS_ASSERTION(BasicManager()->InConstruction(),
                 "Can only set properties in construction phase");
    ThebesLayer::SetVisibleRegion(aRegion);
  }
  virtual void InvalidateRegion(const nsIntRegion& aRegion)
  {
    NS_ASSERTION(BasicManager()->InConstruction(),
                 "Can only set properties in construction phase");
    mValidRegion.Sub(mValidRegion, aRegion);
  }

  virtual void PaintThebes(gfxContext* aContext,
                           Layer* aMaskLayer,
                           LayerManager::DrawThebesLayerCallback aCallback,
                           void* aCallbackData,
                           ReadbackProcessor* aReadback);

  virtual void ClearCachedResources() { mContentClient->Clear(); mValidRegion.SetEmpty(); }
  
  virtual void ComputeEffectiveTransforms(const gfx3DMatrix& aTransformToSurface)
  {
    if (!BasicManager()->IsRetained()) {
      // Don't do any snapping of our transform, since we're just going to
      // draw straight through without intermediate buffers.
      mEffectiveTransform = GetLocalTransform()*aTransformToSurface;
      if (gfxPoint(0,0) != mResidualTranslation) {
        mResidualTranslation = gfxPoint(0,0);
        mValidRegion.SetEmpty();
      }
      ComputeEffectiveTransformForMaskLayer(aTransformToSurface);
      return;
    }
    ThebesLayer::ComputeEffectiveTransforms(aTransformToSurface);
  }

  BasicLayerManager* BasicManager()
  {
    return static_cast<BasicLayerManager*>(mManager);
  }

protected:
  BasicThebesLayer(BasicLayerManager* aLayerManager, ContentClientBasic* aContentClient) :
    ThebesLayer(aLayerManager, static_cast<BasicImplData*>(this)),
    mContentClient(aContentClient)
  {
    MOZ_COUNT_CTOR(BasicThebesLayer);
  }

  virtual void
  PaintBuffer(gfxContext* aContext,
              const nsIntRegion& aRegionToDraw,
              const nsIntRegion& aExtendedRegionToDraw,
              const nsIntRegion& aRegionToInvalidate,
              bool aDidSelfCopy,
              LayerManager::DrawThebesLayerCallback aCallback,
              void* aCallbackData)
  {
    if (!aCallback) {
      BasicManager()->SetTransactionIncomplete();
      return;
    }
    aCallback(this, aContext, aExtendedRegionToDraw, aRegionToInvalidate,
              aCallbackData);
    // Everything that's visible has been validated. Do this instead of just
    // OR-ing with aRegionToDraw, since that can lead to a very complex region
    // here (OR doesn't automatically simplify to the simplest possible
    // representation of a region.)
    nsIntRegion tmp;
    tmp.Or(mVisibleRegion, aExtendedRegionToDraw);
    mValidRegion.Or(mValidRegion, tmp);
  }

  RefPtr<ContentClient> mContentClient;
};


class BasicShadowableThebesLayer : public BasicThebesLayer,
                                   public BasicShadowableLayer
{

  typedef BasicThebesLayer Base;

public:
  BasicShadowableThebesLayer(BasicShadowLayerManager* aManager)
    : BasicThebesLayer(aManager, nullptr)
  {
    MOZ_COUNT_CTOR(BasicShadowableThebesLayer);
  }
  virtual ~BasicShadowableThebesLayer()
  {
    DestroyBackBuffer();
    MOZ_COUNT_DTOR(BasicShadowableThebesLayer);
  }

  virtual void PaintThebes(gfxContext* aContext,
                           Layer* aMaskLayer,
                           LayerManager::DrawThebesLayerCallback aCallback,
                           void* aCallbackData,
                           ReadbackProcessor* aReadback);

  virtual void FillSpecificAttributes(SpecificLayerAttributes& aAttrs)
  {
    aAttrs = ThebesLayerAttributes(GetValidRegion());
  }

  virtual Layer* AsLayer() { return this; }
  virtual ShadowableLayer* AsShadowableLayer() { return this; }

  void SetBackBufferAndAttrs(const TextureIdentifier& aTextureIdentifier,
                             const OptionalThebesBuffer& aBuffer,
                             const nsIntRegion& aValidRegion,
                             const OptionalThebesBuffer& aReadOnlyFrontBuffer,
                             const nsIntRegion& aFrontUpdatedRegion)
  {
    mContentClient->SetBackBufferAndAttrs(aTextureIdentifier,
                                          aBuffer,
                                          aValidRegion,
                                          aReadOnlyFrontBuffer,
                                          aFrontUpdatedRegion,
                                          mValidRegion);
  }

  virtual void Disconnect();

  virtual BasicShadowableThebesLayer* AsThebes() { return this; }

private:
  BasicShadowLayerManager* BasicManager()
  {
    return static_cast<BasicShadowLayerManager*>(mManager);
  }

  virtual void
  PaintBuffer(gfxContext* aContext,
              const nsIntRegion& aRegionToDraw,
              const nsIntRegion& aExtendedRegionToDraw,
              const nsIntRegion& aRegionToInvalidate,
              bool aDidSelfCopy,
              LayerManager::DrawThebesLayerCallback aCallback,
              void* aCallbackData) MOZ_OVERRIDE;

  void DestroyBackBuffer()
  {
    mContentClient = nullptr;
  }
};

}
}

#endif
