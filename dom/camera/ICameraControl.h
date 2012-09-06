/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_CAMERA_ICAMERACONTROL_H
#define DOM_CAMERA_ICAMERACONTROL_H

#include "base/basictypes.h"
#include "prtypes.h"
#include "jsapi.h"
#include "nsIDOMCameraManager.h"
#include "DictionaryHelpers.h"
#include "CameraCommon.h"

namespace mozilla {

using namespace dom;

class DOMCameraPreview;

class ICameraControl
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ICameraControl)

  virtual nsresult GetPreviewStream(CameraSize aSize, nsICameraPreviewStreamCallback* onSuccess, nsICameraErrorCallback* onError) = 0;
  virtual nsresult StartPreview(DOMCameraPreview* aDOMPreview) = 0;
  virtual void StopPreview() = 0;
  virtual nsresult AutoFocus(nsICameraAutoFocusCallback* onSuccess, nsICameraErrorCallback* onError) = 0;
  virtual nsresult TakePicture(CameraSize aSize, int32_t aRotation, const nsAString& aFileFormat, CameraPosition aPosition, nsICameraTakePictureCallback* onSuccess, nsICameraErrorCallback* onError) = 0;
  virtual nsresult StartRecording(CameraSize aSize, nsICameraStartRecordingCallback* onSuccess, nsICameraErrorCallback* onError) = 0;
  virtual nsresult StopRecording() = 0;

  virtual nsresult Set(uint32_t aKey, const nsAString& aValue) = 0;
  virtual nsresult Get(uint32_t aKey, nsAString& aValue) = 0;
  virtual nsresult Set(uint32_t aKey, double aValue) = 0;
  virtual nsresult Get(uint32_t aKey, double* aValue) = 0;
  virtual nsresult Set(JSContext* aCx, uint32_t aKey, const JS::Value& aValue, uint32_t aLimit) = 0;
  virtual nsresult Get(JSContext* aCx, uint32_t aKey, JS::Value* aValue) = 0;
  virtual nsresult SetFocusAreas(JSContext* aCx, const JS::Value& aValue) = 0;
  virtual nsresult SetMeteringAreas(JSContext* aCx, const JS::Value& aValue) = 0;

  virtual const char* GetParameter(const char* aKey) = 0;
  virtual const char* GetParameterConstChar(uint32_t aKey) = 0;
  virtual double GetParameterDouble(uint32_t aKey) = 0;
  virtual void GetParameter(uint32_t aKey, nsTArray<CameraRegion>& aRegions) = 0;
  virtual void SetParameter(const char* aKey, const char* aValue) = 0;
  virtual void SetParameter(uint32_t aKey, const char* aValue) = 0;
  virtual void SetParameter(uint32_t aKey, double aValue) = 0;
  virtual void SetParameter(uint32_t aKey, const nsTArray<CameraRegion>& aRegions) = 0;

protected:
  virtual ~ICameraControl() { }
};

} // namespace mozilla

#endif // DOM_CAMERA_ICAMERACONTROL_H
