/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Content App.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Benoit Girard <bgirard@mozilla.com>
 *   Ali Juma <ajuma@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef mozilla_layers_CompositorParent_h
#define mozilla_layers_CompositorParent_h

#include "mozilla/layers/PCompositorParent.h"
#include "mozilla/layers/PLayersParent.h"
#include "base/thread.h"
#include "ShadowLayersManager.h"

class nsIWidget;

namespace mozilla {
namespace layers {

class LayerManager;

// Represents (affine) transforms that are calculated from a content view.
struct ViewTransform {
  ViewTransform(nsIntPoint aTranslation = nsIntPoint(0, 0), float aXScale = 1, float aYScale = 1)
    : mTranslation(aTranslation)
    , mXScale(aXScale)
    , mYScale(aYScale)
  {}

  operator gfx3DMatrix() const
  {
    return
      gfx3DMatrix::ScalingMatrix(mXScale, mYScale, 1) *
      gfx3DMatrix::Translation(mTranslation.x, mTranslation.y, 0);
  }

  nsIntPoint mTranslation;
  float mXScale;
  float mYScale;
};

class CompositorParent : public PCompositorParent,
                         public ShadowLayersManager
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CompositorParent)
public:
  CompositorParent(nsIWidget* aWidget);
  virtual ~CompositorParent();

  virtual bool RecvStop() MOZ_OVERRIDE;

  virtual void ShadowLayersUpdated() MOZ_OVERRIDE;
  void Destroy();

  LayerManager* GetLayerManager() { return mLayerManager; }

  void SetTransformation(float aScale, nsIntPoint aScrollOffset);
  void AsyncRender();
  void ScheduleRenderOnCompositorThread(::base::Thread &aCompositorThread);

  void PauseComposition();
  void ResumeComposition();
  void SchedulePauseOnCompositorThread(::base::Thread &aCompositorThread);
  void ScheduleResumeOnCompositorThread(::base::Thread &aCompositorThread);

protected:
  virtual PLayersParent* AllocPLayers(const LayersBackend &backendType);
  virtual bool DeallocPLayers(PLayersParent* aLayers);

private:
  void ScheduleComposition();
  void Composite();
#ifdef OMTC_TEST_ASYNC_SCROLLING
  void TestScroll();
#endif
  void TransformShadowTree(Layer* aLayer, const ViewTransform& aTransform,
                           float aTempScaleDiffX = 1.0,
                           float aTempScaleDiffY = 1.0);

  // Platform specific functions
#ifdef MOZ_WIDGET_ANDROID
  /**
   * Register the compositor thread with the Java native thread.
   * This will replace the temporary compositor with the real
   * Gecko compositor thread.
   **/
  void RegisterCompositorWithJava();

  /**
   * Asks Java for the viewport position and updates the world transform
   * accordingly.
   */
  void RequestViewTransform();
#endif

  nsRefPtr<LayerManager> mLayerManager;
  bool mPaused;
  nsIWidget* mWidget;
  float mXScale;
  float mYScale;
  nsIntPoint mScrollOffset;

  DISALLOW_EVIL_CONSTRUCTORS(CompositorParent);
};

} // layers
} // mozilla

#endif // mozilla_layers_CompositorParent_h
