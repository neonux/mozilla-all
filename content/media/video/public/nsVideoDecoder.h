/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: ML 1.1/GPL 2.0/LGPL 2.1
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
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is the Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Chris Double <chris.double@double.co.nz>
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
#if !defined(nsVideoDecoder_h___)
#define nsVideoDecoder_h___

#include "nsIObserver.h"
#include "nsSize.h"
#include "prlog.h"
#include "gfxContext.h"
#include "gfxRect.h"
#include "nsITimer.h"

#ifdef PR_LOGGING
extern PRLogModuleInfo* gVideoDecoderLog;
#define LOG(type, msg) PR_LOG(gVideoDecoderLog, type, msg)
#else
#define LOG(type, msg)
#endif

class nsHTMLMediaElement;

// All methods of nsVideoDecoder must be called from the main thread only
// with the exception of SetRGBData. The latter can be called from any thread.
class nsVideoDecoder : public nsIObserver
{
 public:
  nsVideoDecoder();
  virtual ~nsVideoDecoder() { }

  // Initialize the logging object
  static nsresult InitLogger();

  // Perform any initialization required for the decoder.
  // Return PR_TRUE on successful initialisation, PR_FALSE
  // on failure.
  virtual PRBool Init();

  
  // Return the current URI being played or downloaded.
  virtual void GetCurrentURI(nsIURI** aURI) = 0;

  // Return the time position in the video stream being
  // played measured in seconds.
  virtual float GetCurrentTime() = 0;

  // Seek to the time position in (seconds) from the start of the video.
  virtual nsresult Seek(float time) = 0;

  // Called by the element when the playback rate has been changed.
  // Adjust the speed of the playback, optionally with pitch correction,
  // when this is called.
  virtual nsresult PlaybackRateChanged() = 0;

  // Return the duration of the video in seconds.
  virtual float GetDuration() = 0;
  
  // Pause video playback.
  virtual void Pause() = 0;

  // Return the current audio volume that the video plays at. 
  // This is a value form 0 through to 1.0.
  virtual float GetVolume() = 0;

  // Set the audio volume. It should be a value from 0 to 1.0.
  virtual void SetVolume(float volume) = 0;

  // Returns the current video frame width and height.
  // If there is no video frame, returns the given default size.
  virtual nsIntSize GetVideoSize(nsIntSize defaultSize) = 0;

  // Return the current framerate of the video, in frames per second.
  virtual double GetVideoFramerate() = 0;

  // Start playback of a video. 'Load' must have previously been
  // called.
  virtual nsresult Play() = 0;

  // Stop playback of a video, and stop download of video stream.
  virtual void Stop() = 0;

  // Start downloading the video at the given URI. Decode
  // the downloaded data up to the point of the first frame
  // of data. 
  virtual nsresult Load(nsIURI* aURI) = 0;

  // Draw the latest video data. This is done
  // here instead of in nsVideoFrame so that the lock around the
  // RGB buffer doesn't have to be exposed publically.
  // The current video frame is drawn to fill aRect.
  // Called in the main thread only.
  virtual void Paint(gfxContext* aContext, const gfxRect& aRect);

  // Called when the video file has completed downloading.
  virtual void ResourceLoaded() = 0;

  // Return the current number of bytes loaded from the video file.
  // This is used for progress events.
  virtual PRUint32 GetBytesLoaded() = 0;

  // Return the size of the video file in bytes.
  // This is used for progress events.
  virtual PRUint32 GetTotalBytes() = 0;

  // Called when the HTML DOM element is bound.
  virtual void ElementAvailable(nsHTMLMediaElement* anElement);

  // Called when the HTML DOM element is unbound.
  virtual void ElementUnavailable();

  // Invalidate the frame.
  virtual void Invalidate();

  // Update progress information.
  virtual void Progress();

protected:
  // Cleanup internal data structures
  virtual void Shutdown();

  // Start invalidating the video frame at the interval required
  // by the specificed framerate (in frames per second).
  nsresult StartInvalidating(double aFramerate);

  // Stop invalidating the video frame
  void StopInvalidating();

  // Start timer to update download progress information.
  nsresult StartProgress();

  // Stop progress information timer.
  nsresult StopProgress();

  // Set the RGB width, height and framerate. The passed RGB buffer is
  // copied to the mRGB buffer. This also allocates the mRGB buffer if
  // needed.
  // This is the only nsVideoDecoder method that may be called 
  // from threads other than the main thread.
  void SetRGBData(PRInt32 aWidth, 
                  PRInt32 aHeight, 
                  double aFramerate, 
                  unsigned char* aRGBBuffer);

protected:
  // Timer used for invalidating the video 
  nsCOMPtr<nsITimer> mInvalidateTimer;

  // Timer used for updating progress events 
  nsCOMPtr<nsITimer> mProgressTimer;

  // The element is not reference counted. Instead the decoder is
  // notified when it is able to be used. It should only ever be
  // accessed from the main thread.
  nsHTMLMediaElement* mElement;

  // RGB data for last decoded frame of video data.
  // The size of the buffer is mRGBWidth*mRGBHeight*4 bytes and
  // contains bytes in RGBA format.
  nsAutoArrayPtr<unsigned char> mRGB;

  PRInt32 mRGBWidth;
  PRInt32 mRGBHeight;

  // Has our size changed since the last repaint?
  PRPackedBool mSizeChanged;

  // Lock around the video RGB, width and size data. This
  // is used in the decoder backend threads and the main thread
  // to ensure that repainting the video does not use these
  // values while they are out of sync (width changed but
  // not height yet, etc).
  // Backends that are updating the height, width or writing
  // to the RGB buffer must obtain this lock first to ensure that
  // the video element does not use video data or sizes that are
  // in the midst of being changed.
  PRLock* mVideoUpdateLock;

  // Framerate of video being displayed in the element
  // expressed in numbers of frames per second.
  double mFramerate;
};

#endif
