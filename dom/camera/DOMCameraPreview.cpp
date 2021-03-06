/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"
#include "Layers.h"
#include "VideoUtils.h"
#include "DOMCameraPreview.h"
#include "CameraCommon.h"

using namespace mozilla;

/**
 * 'PreviewControl' is a helper class that dispatches preview control
 * events from the main thread.
 *
 * NS_NewRunnableMethod() can't be used because it AddRef()s the method's
 * object, which can't be done off the main thread for cycle collection
 * participants.
 *
 * Before using this class, 'aDOMPreview' must be appropriately AddRef()ed.
 */
class PreviewControl : public nsRunnable
{
public:
  enum {
    START,
    STOP,
    STARTED,
    STOPPED
  };
  PreviewControl(DOMCameraPreview* aDOMPreview, uint32_t aControl)
    : mDOMPreview(aDOMPreview)
    , mControl(aControl)
  { }

  NS_IMETHOD Run()
  {
    NS_ASSERTION(NS_IsMainThread(), "PreviewControl not run on main thread");

    switch (mControl) {
      case START:
        mDOMPreview->Start();
        break;

      case STOP:
        mDOMPreview->Stop();
        break;

      case STARTED:
        mDOMPreview->SetStateStarted();
        break;

      case STOPPED:
        mDOMPreview->SetStateStopped();
        break;

      default:
        DOM_CAMERA_LOGE("PreviewControl: invalid control %d\n", mControl);
        break;
    }

    return NS_OK;
  }

protected:
  /**
   * This must be a raw pointer because this class is not created on the
   * main thread, and DOMCameraPreview is not threadsafe.  Prior to
   * issuing a preview control event, the caller must ensure that
   * mDOMPreview will not disappear.
   */
  DOMCameraPreview* mDOMPreview;
  uint32_t mControl;
};

class DOMCameraPreviewListener : public MediaStreamListener
{
public:
  DOMCameraPreviewListener(DOMCameraPreview* aDOMPreview) :
    mDOMPreview(aDOMPreview)
  {
    DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
  }

  ~DOMCameraPreviewListener()
  {
    DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
  }

  void NotifyConsumptionChanged(MediaStreamGraph* aGraph, Consumption aConsuming)
  {
    const char* state;

    DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);

    switch (aConsuming) {
      case NOT_CONSUMED:
        state = "not consuming";
        break;

      case CONSUMED:
        state = "consuming";
        break;

      default:
        state = "unknown";
        break;
    }

    DOM_CAMERA_LOGA("camera viewfinder is %s\n", state);
    nsCOMPtr<nsIRunnable> previewControl;

    switch (aConsuming) {
      case NOT_CONSUMED:
        previewControl = new PreviewControl(mDOMPreview, PreviewControl::STOP);
        break;

      case CONSUMED:
        previewControl = new PreviewControl(mDOMPreview, PreviewControl::START);
        break;

      default:
        return;
    }

    nsresult rv = NS_DispatchToMainThread(previewControl);
    if (NS_FAILED(rv)) {
      DOM_CAMERA_LOGE("Failed to dispatch preview control (%d)!\n", rv);
    }
  }

protected:
  // Raw pointer; if we exist, 'mDOMPreview' exists as well
  DOMCameraPreview* mDOMPreview;
};

DOMCameraPreview::DOMCameraPreview(ICameraControl* aCameraControl, uint32_t aWidth, uint32_t aHeight, uint32_t aFrameRate)
  : nsDOMMediaStream()
  , mState(STOPPED)
  , mWidth(aWidth)
  , mHeight(aHeight)
  , mFramesPerSecond(aFrameRate)
  , mFrameCount(0)
  , mCameraControl(aCameraControl)
{
  DOM_CAMERA_LOGT("%s:%d : this=%p : mWidth=%d, mHeight=%d, mFramesPerSecond=%d\n", __func__, __LINE__, this, mWidth, mHeight, mFramesPerSecond);

  mImageContainer = LayerManager::CreateImageContainer();
  MediaStreamGraph* gm = MediaStreamGraph::GetInstance();
  mStream = gm->CreateInputStream(this);
  mInput = GetStream()->AsSourceStream();

  mListener = new DOMCameraPreviewListener(this);
  mInput->AddListener(mListener);

  mInput->AddTrack(TRACK_VIDEO, mFramesPerSecond, 0, new VideoSegment());
  mInput->AdvanceKnownTracksTime(MEDIA_TIME_MAX);
}

DOMCameraPreview::~DOMCameraPreview()
{
  DOM_CAMERA_LOGT("%s:%d : this=%p, mListener=%p\n", __func__, __LINE__, this, mListener);
  mInput->RemoveListener(mListener);
}

bool
DOMCameraPreview::HaveEnoughBuffered()
{
  return mInput->HaveEnoughBuffered(TRACK_VIDEO);
}

bool
DOMCameraPreview::ReceiveFrame(void* aBuffer, ImageFormat aFormat, FrameBuilder aBuilder)
{
  DOM_CAMERA_LOGT("%s:%d : this=%p\n", __func__, __LINE__, this);
  if (!aBuffer || !aBuilder) {
    return false;
  }
  if (mState != STARTED) {
    return false;
  }

  ImageFormat format = aFormat;
  nsRefPtr<Image> image = mImageContainer->CreateImage(&format, 1);
  aBuilder(image, aBuffer, mWidth, mHeight);

  // AppendFrame() takes over image's reference
  mVideoSegment.AppendFrame(image.forget(), 1, gfxIntSize(mWidth, mHeight));
  mInput->AppendToTrack(TRACK_VIDEO, &mVideoSegment);
  return true;
}

void
DOMCameraPreview::Start()
{
  NS_ASSERTION(NS_IsMainThread(), "Start() not called from main thread");
  if (mState != STOPPED) {
    return;
  }

  DOM_CAMERA_LOGI("Starting preview stream\n");

  /**
   * Add a reference to ourselves to make sure we stay alive while
   * the preview is running, as the CameraControlImpl object holds a
   * weak reference to us.
   *
   * This reference is removed in SetStateStopped().
   */
  NS_ADDREF_THIS();
  mState = STARTING;
  mCameraControl->StartPreview(this);
}

void
DOMCameraPreview::SetStateStarted()
{
  NS_ASSERTION(NS_IsMainThread(), "SetStateStarted() not called from main thread");

  mState = STARTED;
  DOM_CAMERA_LOGI("Preview stream started\n");
}

void
DOMCameraPreview::Started()
{
  if (mState != STARTING) {
    return;
  }

  DOM_CAMERA_LOGI("Dispatching preview stream started\n");
  nsCOMPtr<nsIRunnable> started = new PreviewControl(this, PreviewControl::STARTED);
  nsresult rv = NS_DispatchToMainThread(started);
  if (NS_FAILED(rv)) {
    DOM_CAMERA_LOGE("failed to set statrted state (%d), POTENTIAL MEMORY LEAK!\n", rv);
  }
}

void
DOMCameraPreview::Stop()
{
  NS_ASSERTION(NS_IsMainThread(), "Stop() not called from main thread");
  if (mState != STARTED) {
    return;
  }

  DOM_CAMERA_LOGI("Stopping preview stream\n");
  mState = STOPPING;
  mCameraControl->StopPreview();
  mInput->EndTrack(TRACK_VIDEO);
  mInput->Finish();
}

void
DOMCameraPreview::SetStateStopped()
{
  NS_ASSERTION(NS_IsMainThread(), "SetStateStopped() not called from main thread");

  mState = STOPPED;
  DOM_CAMERA_LOGI("Preview stream stopped\n");

  /**
   * Only remove the reference added in Start() once the preview
   * has stopped completely.
   */
  NS_RELEASE_THIS();
}

void
DOMCameraPreview::Stopped(bool aForced)
{
  if (mState != STOPPING && !aForced) {
    return;
  }

  DOM_CAMERA_LOGI("Dispatching preview stream stopped\n");
  nsCOMPtr<nsIRunnable> stopped = new PreviewControl(this, PreviewControl::STOPPED);
  nsresult rv = NS_DispatchToMainThread(stopped);
  if (NS_FAILED(rv)) {
    DOM_CAMERA_LOGE("failed to decrement reference count (%d), MEMORY LEAK!\n", rv);
  }
}

void
DOMCameraPreview::Error()
{
  DOM_CAMERA_LOGE("Error occurred changing preview state!\n");
  Stopped(true);
}
