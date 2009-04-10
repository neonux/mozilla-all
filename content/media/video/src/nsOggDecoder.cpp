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
#include <limits>
#include "prlog.h"
#include "prmem.h"
#include "nsIFrame.h"
#include "nsIDocument.h"
#include "nsThreadUtils.h"
#include "nsIDOMHTMLMediaElement.h"
#include "nsNetUtil.h"
#include "nsAudioStream.h"
#include "nsChannelReader.h"
#include "nsHTMLVideoElement.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsAutoLock.h"
#include "nsTArray.h"
#include "nsNetUtil.h"
#include "nsOggDecoder.h"

/* 
   The maximum height and width of the video. Used for
   sanitizing the memory allocation of the RGB buffer
*/
#define MAX_VIDEO_WIDTH  2000
#define MAX_VIDEO_HEIGHT 2000

// The number of entries in oggplay buffer list. This value
// is the one used by the oggplay examples.
#define OGGPLAY_BUFFER_SIZE 20

// The number of frames to read before audio callback is called.
// This value is the one used by the oggplay examples.
#define OGGPLAY_FRAMES_PER_CALLBACK 2048

// Offset into Ogg buffer containing audio information. This value
// is the one used by the oggplay examples.
#define OGGPLAY_AUDIO_OFFSET 250L

// Wait this number of seconds when buffering, then leave and play
// as best as we can if the required amount of data hasn't been
// retrieved.
#define BUFFERING_WAIT 15

// The amount of data to retrieve during buffering is computed based
// on the download rate. BUFFERING_MIN_RATE is the minimum download
// rate to be used in that calculation to help avoid constant buffering
// attempts at a time when the average download rate has not stabilised.
#define BUFFERING_MIN_RATE 50000
#define BUFFERING_RATE(x) ((x)< BUFFERING_MIN_RATE ? BUFFERING_MIN_RATE : (x))

// The number of seconds of buffer data before buffering happens
// based on current playback rate.
#define BUFFERING_SECONDS_LOW_WATER_MARK 1

/* 
  All reading (including seeks) from the nsMediaStream are done on the
  decoding thread. The decoder thread is informed before closing that
  the stream is about to close via the Shutdown
  event. oggplay_prepare_for_close is called before sending the
  shutdown event to tell liboggplay to shutdown.

  This call results in oggplay internally not calling any
  read/write/seek/tell methods, and returns a value that results in
  stopping the decoder thread.

  oggplay_close is called in the destructor which results in the media
  stream being closed. This is how the nsMediaStream contract that no
  read/seeking must occur during or after Close is called is enforced.

  This object keeps pointers to the nsOggDecoder and nsChannelReader
  objects.  Since the lifetime of nsOggDecodeStateMachine is
  controlled by nsOggDecoder it will never have a stale reference to
  these objects. The reader is destroyed by the call to oggplay_close
  which is done in the destructor so again this will never be a stale
  reference.

  All internal state is synchronised via the decoder monitor. NotifyAll
  on the monitor is called when the state of the state machine is changed
  by the main thread. The following changes to state cause a notify:

    mState and data related to that state changed (mSeekTime, etc)
    Ogg Metadata Loaded
    First Frame Loaded  
    Frame decoded    
    
  See nsOggDecoder.h for more details.
*/
class nsOggDecodeStateMachine : public nsRunnable
{
public:
  // Object to hold the decoded data from a frame
  class FrameData {
  public:
    FrameData() :
      mVideoHeader(nsnull),
      mVideoWidth(0),
      mVideoHeight(0),
      mUVWidth(0),
      mUVHeight(0),
      mDecodedFrameTime(0.0),
      mTime(0.0)
    {
      MOZ_COUNT_CTOR(FrameData);
    }

    ~FrameData()
    {
      MOZ_COUNT_DTOR(FrameData);

      if (mVideoHeader) {
        oggplay_callback_info_unlock_item(mVideoHeader);
      }
    }

    // Write the audio data from the frame to the Audio stream.
    void Write(nsAudioStream* aStream)
    {
      PRUint32 length = mAudioData.Length();
      if (length == 0)
        return;

      aStream->Write(mAudioData.Elements(), length);
    }

    void SetVideoHeader(OggPlayDataHeader* aVideoHeader)
    {
      NS_ABORT_IF_FALSE(!mVideoHeader, "Frame already owns a video header");
      mVideoHeader = aVideoHeader;
      oggplay_callback_info_lock_item(mVideoHeader);
    }

    // The position in the stream where this frame ended, in bytes
    PRInt64 mEndStreamPosition;
    OggPlayDataHeader* mVideoHeader;
    nsTArray<float> mAudioData;
    int mVideoWidth;
    int mVideoHeight;
    int mUVWidth;
    int mUVHeight;
    float mDecodedFrameTime;
    float mTime;
    OggPlayStreamInfo mState;
  };

  // A queue of decoded video frames. 
  class FrameQueue
  {
  public:
    FrameQueue() :
      mHead(0),
      mTail(0),
      mEmpty(PR_TRUE)
    {
    }

    void Push(FrameData* frame)
    {
      NS_ASSERTION(!IsFull(), "FrameQueue is full");
      mQueue[mTail] = frame;
      mTail = (mTail+1) % OGGPLAY_BUFFER_SIZE;
      mEmpty = PR_FALSE;
    }

    FrameData* Peek()
    {
      NS_ASSERTION(!mEmpty, "FrameQueue is empty");

      return mQueue[mHead];
    }

    FrameData* Pop()
    {
      NS_ASSERTION(!mEmpty, "FrameQueue is empty");

      FrameData* result = mQueue[mHead];
      mHead = (mHead + 1) % OGGPLAY_BUFFER_SIZE;
      mEmpty = mHead == mTail;
      return result;
    }

    PRBool IsEmpty() const
    {
      return mEmpty;
    }

    PRBool IsFull() const
    {
      return !mEmpty && mHead == mTail;
    }

  private:
    FrameData* mQueue[OGGPLAY_BUFFER_SIZE];
    PRInt32 mHead;
    PRInt32 mTail;
    PRPackedBool mEmpty;
  };

  // Enumeration for the valid states
  enum State {
    DECODER_STATE_DECODING_METADATA,
    DECODER_STATE_DECODING_FIRSTFRAME,
    DECODER_STATE_DECODING,
    DECODER_STATE_SEEKING,
    DECODER_STATE_BUFFERING,
    DECODER_STATE_COMPLETED,
    DECODER_STATE_SHUTDOWN
  };

  nsOggDecodeStateMachine(nsOggDecoder* aDecoder);
  ~nsOggDecodeStateMachine();

  // Cause state transitions. These methods obtain the decoder monitor
  // to synchronise the change of state, and to notify other threads
  // that the state has changed.
  void Shutdown();
  void Decode();
  void Seek(float aTime);

  NS_IMETHOD Run();

  PRBool HasAudio()
  {
    NS_ASSERTION(mState > DECODER_STATE_DECODING_METADATA, "HasAudio() called during invalid state");
    //  NS_ASSERTION(PR_InMonitor(mDecoder->GetMonitor()), "HasAudio() called without acquiring decoder monitor");
    return mAudioTrack != -1;
  }

  // Decode one frame of data, returning the OggPlay error code. Must
  // be called only when the current state > DECODING_METADATA. The decode 
  // monitor MUST NOT be locked during this call since it can take a long
  // time. liboggplay internally handles locking.
  // Any return value apart from those below is mean decoding cannot continue.
  // E_OGGPLAY_CONTINUE       = One frame decoded and put in buffer list
  // E_OGGPLAY_USER_INTERRUPT = One frame decoded, buffer list is now full
  // E_OGGPLAY_TIMEOUT        = No frames decoded, timed out
  OggPlayErrorCode DecodeFrame();

  // Returns the next decoded frame of data. The caller is responsible
  // for freeing the memory returned. This function must be called
  // only when the current state > DECODING_METADATA. The decode
  // monitor lock does not need to be locked during this call since
  // liboggplay internally handles locking.
  FrameData* NextFrame();

  // Play a frame of decoded video. The decode monitor is obtained
  // internally by this method for synchronisation.
  void PlayFrame();

  // Play the video data from the given frame. The decode monitor
  // must be locked when calling this method.
  void PlayVideo(FrameData* aFrame);

  // Play the audio data from the given frame. The decode monitor must
  // be locked when calling this method.
  void PlayAudio(FrameData* aFrame);

  // Called from the main thread to get the current frame time. The decoder
  // monitor must be obtained before calling this.
  float GetCurrentTime();

  // Called from the main thread to get the duration. The decoder monitor
  // must be obtained before calling this. It is in units of milliseconds.
  PRInt64 GetDuration();

  // Called from the main thread to set the duration of the media resource
  // if it is able to be obtained via HTTP headers. The decoder monitor
  // must be obtained before calling this.
  void SetDuration(PRInt64 aDuration);

  // Called from the main thread to set whether the media resource can
  // be seeked. The decoder monitor must be obtained before calling this.
  void SetSeekable(PRBool aSeekable);

  // Set the audio volume. The decoder monitor must be obtained before
  // calling this.
  void SetVolume(float aVolume);

  // Clear the flag indicating that a playback position change event
  // is currently queued. This is called from the main thread and must
  // be called with the decode monitor held.
  void ClearPositionChangeFlag();

  // Must be called with the decode monitor held. Can be called by main
  // thread.
  PRBool HaveNextFrameData() const {
    return !mDecodedFrames.IsEmpty() &&
      (mState == DECODER_STATE_DECODING ||
       mState == DECODER_STATE_COMPLETED);
  }

  // Must be called with the decode monitor held. Can be called by main
  // thread.
  PRBool IsBuffering() const {
    return mState == nsOggDecodeStateMachine::DECODER_STATE_BUFFERING;
  }

protected:
  // Convert the OggPlay frame information into a format used by Gecko
  // (RGB for video, float for sound, etc).The decoder monitor must be
  // acquired in the scope of calls to these functions. They must be
  // called only when the current state > DECODING_METADATA.
  void HandleVideoData(FrameData* aFrame, int aTrackNum, OggPlayDataHeader* aVideoHeader);
  void HandleAudioData(FrameData* aFrame, OggPlayAudioData* aAudioData, int aSize);

  // These methods can only be called on the decoding thread.
  void LoadOggHeaders(nsChannelReader* aReader);

  // Initializes and opens the audio stream. Called from the decode
  // thread only. Must be called with the decode monitor held.
  void OpenAudioStream();

  // Closes and releases resources used by the audio stream. Called
  // from the decode thread only. Must be called with the decode
  // monitor held.
  void CloseAudioStream();

  // Start playback of audio, either by opening or resuming the audio
  // stream. Must be called with the decode monitor held.
  void StartAudio();

  // Stop playback of audio, either by closing or pausing the audio
  // stream. Must be called with the decode monitor held.
  void StopAudio();

  // Start playback of media. Must be called with the decode monitor held.
  void StartPlayback();

  // Stop playback of media. Must be called with the decode monitor held.
  void StopPlayback();

  // Update the playback position. This can result in a timeupdate event
  // and an invalidate of the frame being dispatched asynchronously if
  // there is no such event currently queued.
  // Only called on the decoder thread. Must be called with
  // the decode monitor held.
  void UpdatePlaybackPosition(float aTime);

private:
  // *****
  // The follow fields are only accessed by the decoder thread
  // *****

  // The decoder object that created this state machine. The decoder
  // always outlives us since it controls our lifetime.
  nsOggDecoder* mDecoder;

  // The OggPlay handle. Synchronisation of calls to oggplay functions
  // are handled by liboggplay. We control the lifetime of this
  // object, destroying it in our destructor.
  OggPlay* mPlayer;

  // Frame data containing decoded video/audio for the frame the
  // current frame and the previous frame. Always accessed with monitor
  // held. Written only via the decoder thread, but can be tested on
  // main thread via HaveNextFrameData.
  FrameQueue mDecodedFrames;

  // The time that playback started from the system clock. This is used
  // for synchronising frames.  It is reset after a seek as the mTime member
  // of FrameData is reset to start at 0 from the first frame after a seek.
  // Accessed only via the decoder thread.
  PRIntervalTime mPlayStartTime;

  // The time that playback was most recently paused, either via
  // buffering or pause. This is used to compute mPauseDuration for
  // a/v sync adjustments.  Accessed only via the decoder thread.
  PRIntervalTime mPauseStartTime;

  // The total time that has been spent in completed pauses (via
  // 'pause' or buffering). This is used to adjust for these
  // pauses when computing a/v synchronisation. Accessed only via the
  // decoder thread.
  PRIntervalTime mPauseDuration;

  // PR_TRUE if the media is playing and the decoder has started
  // the sound and adjusted the sync time for pauses. PR_FALSE
  // if the media is paused and the decoder has stopped the sound
  // and adjusted the sync time for pauses. Accessed only via the
  // decoder thread.
  PRPackedBool mPlaying;

  // Number of seconds of data video/audio data held in a frame.
  // Accessed only via the decoder thread.
  float mCallbackPeriod;

  // Video data. These are initially set when the metadata is loaded.
  // They are only accessed from the decoder thread.
  PRInt32 mVideoTrack;
  float   mFramerate;

  // Audio data. These are initially set when the metadata is loaded.
  // They are only accessed from the decoder thread.
  PRInt32 mAudioRate;
  PRInt32 mAudioChannels;
  PRInt32 mAudioTrack;

  // Time that buffering started. Used for buffering timeout and only
  // accessed in the decoder thread.
  PRIntervalTime mBufferingStart;

  // Download position where we should stop buffering. Only
  // accessed in the decoder thread.
  PRInt64 mBufferingEndOffset;

  // The time value of the last decoded video frame. Used for
  // computing the sleep period between frames for a/v sync.
  // Read/Write from the decode thread only.
  float mLastFrameTime;

  // The decoder position of the end of the last decoded video frame.
  // Read/Write from the decode thread only.
  PRInt64 mLastFramePosition;

  // *****
  // The follow fields are accessed by the decoder thread or
  // the main thread.
  // *****

  // The decoder monitor must be obtained before modifying this state.
  // NotifyAll on the monitor must be called when the state is changed by
  // the main thread so the decoder thread can wake up.
  State mState;

  // Position to seek to when the seek state transition occurs. The
  // decoder monitor lock must be obtained before reading or writing
  // this value.
  float mSeekTime;

  // The audio stream resource. Used on the decode thread and the
  // main thread (Via the SetVolume call). Synchronisation via
  // mDecoder monitor.
  nsAutoPtr<nsAudioStream> mAudioStream;

  // The time of the current frame in seconds. This is referenced from
  // 0.0 which is the initial start of the stream. Set by the decode
  // thread, and read-only from the main thread to get the current
  // time value. Synchronised via decoder monitor.
  float mCurrentFrameTime;

  // Volume of playback. 0.0 = muted. 1.0 = full volume. Read/Written
  // from the decode and main threads. Synchronised via decoder
  // monitor.
  float mVolume;

  // Duration of the media resource. It is accessed from the decoder and main
  // threads. Synchronised via decoder monitor. It is in units of
  // milliseconds.
  PRInt64 mDuration;

  // PR_TRUE if the media resource can be seeked. Accessed from the decoder
  // and main threads. Synchronised via decoder monitor.
  PRPackedBool mSeekable;

  // PR_TRUE if an event to notify about a change in the playback
  // position has been queued, but not yet run. It is set to PR_FALSE when
  // the event is run. This allows coalescing of these events as they can be
  // produced many times per second. Synchronised via decoder monitor.
  PRPackedBool mPositionChangeQueued;
};

nsOggDecodeStateMachine::nsOggDecodeStateMachine(nsOggDecoder* aDecoder) :
  mDecoder(aDecoder),
  mPlayer(0),
  mPlayStartTime(0),
  mPauseStartTime(0),
  mPauseDuration(0),
  mPlaying(PR_FALSE),
  mCallbackPeriod(1.0),
  mVideoTrack(-1),
  mFramerate(0.0),
  mAudioRate(0),
  mAudioChannels(0),
  mAudioTrack(-1),
  mBufferingStart(0),
  mBufferingEndOffset(0),
  mLastFrameTime(0),
  mLastFramePosition(-1),
  mState(DECODER_STATE_DECODING_METADATA),
  mSeekTime(0.0),
  mCurrentFrameTime(0.0),
  mVolume(1.0),
  mDuration(-1),
  mSeekable(PR_TRUE),
  mPositionChangeQueued(PR_FALSE)
{
}

nsOggDecodeStateMachine::~nsOggDecodeStateMachine()
{
  while (!mDecodedFrames.IsEmpty()) {
    delete mDecodedFrames.Pop();
  }
  oggplay_close(mPlayer);
}

OggPlayErrorCode nsOggDecodeStateMachine::DecodeFrame()
{
  NS_ASSERTION(mState > DECODER_STATE_DECODING_METADATA, "DecodeFrame() called during invalid state");
  return oggplay_step_decoding(mPlayer);
}

nsOggDecodeStateMachine::FrameData* nsOggDecodeStateMachine::NextFrame()
{
  NS_ASSERTION(mState > DECODER_STATE_DECODING_METADATA, "NextFrame() called during invalid state");
  OggPlayCallbackInfo** info = oggplay_buffer_retrieve_next(mPlayer);
  if (!info)
    return nsnull;

  FrameData* frame = new FrameData();
  if (!frame) {
    return nsnull;
  }

  frame->mTime = mLastFrameTime;
  frame->mEndStreamPosition = mDecoder->mDecoderPosition;
  mLastFrameTime += mCallbackPeriod;

  if (mLastFramePosition >= 0) {
    NS_ASSERTION(frame->mEndStreamPosition >= mLastFramePosition,
                 "Playback positions must not decrease without an intervening reset");
    mDecoder->mPlaybackStatistics.Start(frame->mTime*PR_TicksPerSecond());
    mDecoder->mPlaybackStatistics.AddBytes(frame->mEndStreamPosition - mLastFramePosition);
    mDecoder->mPlaybackStatistics.Stop(mLastFrameTime*PR_TicksPerSecond());
    mDecoder->UpdatePlaybackRate();
  }
  mLastFramePosition = frame->mEndStreamPosition;

  int num_tracks = oggplay_get_num_tracks(mPlayer);
  float audioTime = 0.0;
  float videoTime = 0.0;

  if (mVideoTrack != -1 &&
      num_tracks > mVideoTrack &&
      oggplay_callback_info_get_type(info[mVideoTrack]) == OGGPLAY_YUV_VIDEO) {
    OggPlayDataHeader** headers = oggplay_callback_info_get_headers(info[mVideoTrack]);
    videoTime = ((float)oggplay_callback_info_get_presentation_time(headers[0]))/1000.0;
    HandleVideoData(frame, mVideoTrack, headers[0]);
  }

  if (mAudioTrack != -1 &&
      num_tracks > mAudioTrack &&
      oggplay_callback_info_get_type(info[mAudioTrack]) == OGGPLAY_FLOATS_AUDIO) {
    OggPlayDataHeader** headers = oggplay_callback_info_get_headers(info[mAudioTrack]);
    audioTime = ((float)oggplay_callback_info_get_presentation_time(headers[0]))/1000.0;
    int required = oggplay_callback_info_get_required(info[mAudioTrack]);
    for (int j = 0; j < required; ++j) {
      int size = oggplay_callback_info_get_record_size(headers[j]);
      OggPlayAudioData* audio_data = oggplay_callback_info_get_audio_data(headers[j]);
      HandleAudioData(frame, audio_data, size);
    }
  }

  // Pick one stream to act as the reference track to indicate if the
  // stream has ended, seeked, etc.
  if (mVideoTrack >= 0 )
    frame->mState = oggplay_callback_info_get_stream_info(info[mVideoTrack]);
  else if (mAudioTrack >= 0)
    frame->mState = oggplay_callback_info_get_stream_info(info[mAudioTrack]);
  else
    frame->mState = OGGPLAY_STREAM_UNINITIALISED;

  frame->mDecodedFrameTime = mVideoTrack == -1 ? audioTime : videoTime;

  oggplay_buffer_release(mPlayer, info);
  return frame;
}

void nsOggDecodeStateMachine::HandleVideoData(FrameData* aFrame, int aTrackNum, OggPlayDataHeader* aVideoHeader) {
  if (!aVideoHeader)
    return;

  int y_width;
  int y_height;
  oggplay_get_video_y_size(mPlayer, aTrackNum, &y_width, &y_height);
  int uv_width;
  int uv_height;
  oggplay_get_video_uv_size(mPlayer, aTrackNum, &uv_width, &uv_height);

  if (y_width >= MAX_VIDEO_WIDTH || y_height >= MAX_VIDEO_HEIGHT) {
    return;
  }

  aFrame->mVideoWidth = y_width;
  aFrame->mVideoHeight = y_height;
  aFrame->mUVWidth = uv_width;
  aFrame->mUVHeight = uv_height;
  aFrame->SetVideoHeader(aVideoHeader);
}

void nsOggDecodeStateMachine::HandleAudioData(FrameData* aFrame, OggPlayAudioData* aAudioData, int aSize) {
  // 'aSize' is number of samples. Multiply by number of channels to
  // get the actual number of floats being sent.
  int size = aSize * mAudioChannels;

  aFrame->mAudioData.AppendElements(reinterpret_cast<float*>(aAudioData), size);
}

void nsOggDecodeStateMachine::PlayFrame() {
  // Play a frame of video and/or audio data.
  // If we are playing we open the audio stream if needed
  // If there are decoded frames in the queue a single frame
  // is popped off and played if it is time for that frame
  // to display. 
  // If it is not time yet to display the frame, we either
  // continue decoding frames, or wait until it is time for
  // the frame to display if the queue is full.
  //
  // If the decode state is not PLAYING then we just exit
  // so we can continue decoding frames. If the queue is
  // full we wait for a state change.
  nsAutoMonitor mon(mDecoder->GetMonitor());

  if (mDecoder->GetState() == nsOggDecoder::PLAY_STATE_PLAYING) {
    if (!mPlaying) {
      StartPlayback();
    }

    if (!mDecodedFrames.IsEmpty()) {
      FrameData* frame = mDecodedFrames.Peek();
      if (frame->mState == OGGPLAY_STREAM_JUST_SEEKED) {
        // After returning from a seek all mTime members of
        // FrameData start again from a time position of 0.
        // Reset the play start time.
        mPlayStartTime = PR_IntervalNow();
        mPauseDuration = 0;
        frame->mState = OGGPLAY_STREAM_INITIALISED;
      }

      double time = (PR_IntervalToMilliseconds(PR_IntervalNow()-mPlayStartTime-mPauseDuration)/1000.0);
      if (time >= frame->mTime) {
        // Audio for the current frame is played, but video for the next frame
        // is displayed, to account for lag from the time the audio is written
        // to when it is played. This will go away when we move to a/v sync
        // using the audio hardware clock.
        PlayAudio(frame);
        mDecodedFrames.Pop();
        PlayVideo(mDecodedFrames.IsEmpty() ? frame : mDecodedFrames.Peek());
        mDecoder->mPlaybackPosition = frame->mEndStreamPosition;
        UpdatePlaybackPosition(frame->mDecodedFrameTime);
        delete frame;
      }
      else {
        // If the queue of decoded frame is full then we wait for the
        // approximate time until the next frame. 
        if (mDecodedFrames.IsFull()) {
          mon.Wait(PR_MillisecondsToInterval(PRInt64((frame->mTime - time)*1000)));
          if (mState == DECODER_STATE_SHUTDOWN) {
            return;
          }
        }
      }
    }
  }
  else {
    if (mPlaying) {
      StopPlayback();
    }

    if (mDecodedFrames.IsFull() && mState == DECODER_STATE_DECODING) {
      mon.Wait();
      if (mState == DECODER_STATE_SHUTDOWN) {
        return;
      }
    }
  }
}

void nsOggDecodeStateMachine::PlayVideo(FrameData* aFrame)
{
  //  NS_ASSERTION(PR_InMonitor(mDecoder->GetMonitor()), "PlayVideo() called without acquiring decoder monitor");
  if (aFrame && aFrame->mVideoHeader) {
    OggPlayVideoData* videoData = oggplay_callback_info_get_video_data(aFrame->mVideoHeader);

    OggPlayYUVChannels yuv;
    yuv.ptry = videoData->y;
    yuv.ptru = videoData->u;
    yuv.ptrv = videoData->v;
    yuv.uv_width = aFrame->mUVWidth;
    yuv.uv_height = aFrame->mUVHeight;
    yuv.y_width = aFrame->mVideoWidth;
    yuv.y_height = aFrame->mVideoHeight;

    size_t size = aFrame->mVideoWidth * aFrame->mVideoHeight * 4;
    nsAutoArrayPtr<unsigned char> buffer(new unsigned char[size]);
    if (!buffer)
      return;

    OggPlayRGBChannels rgb;
    rgb.ptro = buffer;
    rgb.rgb_width = aFrame->mVideoWidth;
    rgb.rgb_height = aFrame->mVideoHeight;

    oggplay_yuv2bgra(&yuv, &rgb);

    mDecoder->SetRGBData(aFrame->mVideoWidth, aFrame->mVideoHeight, mFramerate, buffer.forget());
  }
}

void nsOggDecodeStateMachine::PlayAudio(FrameData* aFrame)
{
  //  NS_ASSERTION(PR_InMonitor(mDecoder->GetMonitor()), "PlayAudio() called without acquiring decoder monitor");
  if (!mAudioStream)
    return;

  aFrame->Write(mAudioStream);
}

void nsOggDecodeStateMachine::OpenAudioStream()
{
  //  NS_ASSERTION(PR_InMonitor(mDecoder->GetMonitor()), "OpenAudioStream() called without acquiring decoder monitor");
  mAudioStream = new nsAudioStream();
  if (!mAudioStream) {
    LOG(PR_LOG_ERROR, ("Could not create audio stream"));
  }
  else {
    mAudioStream->Init(mAudioChannels, mAudioRate, nsAudioStream::FORMAT_FLOAT32);
    mAudioStream->SetVolume(mVolume);
  }
}

void nsOggDecodeStateMachine::CloseAudioStream()
{
  //  NS_ASSERTION(PR_InMonitor(mDecoder->GetMonitor()), "CloseAudioStream() called without acquiring decoder monitor");
  if (mAudioStream) {
    mAudioStream->Shutdown();
    mAudioStream = nsnull;
  }
}

void nsOggDecodeStateMachine::StartAudio()
{
  //  NS_ASSERTION(PR_InMonitor(mDecoder->GetMonitor()), "StartAudio() called without acquiring decoder monitor");
  if (HasAudio()) {
    OpenAudioStream();
  }
}

void nsOggDecodeStateMachine::StopAudio()
{
  //  NS_ASSERTION(PR_InMonitor(mDecoder->GetMonitor()), "StopAudio() called without acquiring decoder monitor");
  if (HasAudio()) {
    CloseAudioStream();
  }
}

void nsOggDecodeStateMachine::StartPlayback()
{
  //  NS_ASSERTION(PR_InMonitor(mDecoder->GetMonitor()), "StartPlayback() called without acquiring decoder monitor");
  StartAudio();
  mPlaying = PR_TRUE;

  // If this is the very first play, then set the initial start time
  if (mPlayStartTime == 0) {
    mPlayStartTime = PR_IntervalNow();
  }

  // If we have been paused previously, then compute duration spent paused
  if (mPauseStartTime != 0) {
    mPauseDuration += PR_IntervalNow() - mPauseStartTime;
  }
}

void nsOggDecodeStateMachine::StopPlayback()
{
  //  NS_ASSERTION(PR_InMonitor(mDecoder->GetMonitor()), "StopPlayback() called without acquiring decoder monitor");
  StopAudio();
  mPlaying = PR_FALSE;
  mPauseStartTime = PR_IntervalNow();
}

void nsOggDecodeStateMachine::UpdatePlaybackPosition(float aTime)
{
  //  NS_ASSERTION(PR_InMonitor(mDecoder->GetMonitor()), "UpdatePlaybackPosition() called without acquiring decoder monitor");
  mCurrentFrameTime = aTime;
  if (!mPositionChangeQueued) {
    mPositionChangeQueued = PR_TRUE;
    nsCOMPtr<nsIRunnable> event =
      NS_NEW_RUNNABLE_METHOD(nsOggDecoder, mDecoder, PlaybackPositionChanged);
    NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
  }
}

void nsOggDecodeStateMachine::ClearPositionChangeFlag()
{
  //  NS_ASSERTION(PR_InMonitor(mDecoder->GetMonitor()), "ClearPositionChangeFlag() called without acquiring decoder monitor");
  mPositionChangeQueued = PR_FALSE;
}

void nsOggDecodeStateMachine::SetVolume(float volume)
{
  //  NS_ASSERTION(PR_InMonitor(mDecoder->GetMonitor()), "SetVolume() called without acquiring decoder monitor");
  if (mAudioStream) {
    mAudioStream->SetVolume(volume);
  }

  mVolume = volume;
}

float nsOggDecodeStateMachine::GetCurrentTime()
{
  //  NS_ASSERTION(PR_InMonitor(mDecoder->GetMonitor()), "GetCurrentTime() called without acquiring decoder monitor");
  return mCurrentFrameTime;
}

PRInt64 nsOggDecodeStateMachine::GetDuration()
{
  //  NS_ASSERTION(PR_InMonitor(mDecoder->GetMonitor()), "GetDuration() called without acquiring decoder monitor");
  return mDuration;
}

void nsOggDecodeStateMachine::SetDuration(PRInt64 aDuration)
{
   //  NS_ASSERTION(PR_InMonitor(mDecoder->GetMonitor()), "SetDuration() called without acquiring decoder monitor");
  mDuration = aDuration;
}

void nsOggDecodeStateMachine::SetSeekable(PRBool aSeekable)
{
   //  NS_ASSERTION(PR_InMonitor(mDecoder->GetMonitor()), "SetSeekable() called without acquiring decoder monitor");
  mSeekable = aSeekable;
}

void nsOggDecodeStateMachine::Shutdown()
{
  // oggplay_prepare_for_close cannot be undone. Once called, the
  // mPlayer object cannot decode any more frames. Once we've entered
  // the shutdown state here there's no going back.
  nsAutoMonitor mon(mDecoder->GetMonitor());
  if (mPlayer) {
    oggplay_prepare_for_close(mPlayer);
  }
  LOG(PR_LOG_DEBUG, ("Changed state to SHUTDOWN"));
  mState = DECODER_STATE_SHUTDOWN;
  mon.NotifyAll();
}

void nsOggDecodeStateMachine::Decode()
{
  // When asked to decode, switch to decoding only if
  // we are currently buffering.
  nsAutoMonitor mon(mDecoder->GetMonitor());
  if (mState == DECODER_STATE_BUFFERING) {
    LOG(PR_LOG_DEBUG, ("Changed state from BUFFERING to DECODING"));
    mState = DECODER_STATE_DECODING;
  }
}

void nsOggDecodeStateMachine::Seek(float aTime)
{
  nsAutoMonitor mon(mDecoder->GetMonitor());
  mSeekTime = aTime;
  LOG(PR_LOG_DEBUG, ("Changed state to SEEKING (to %f)", aTime));
  mState = DECODER_STATE_SEEKING;
}

nsresult nsOggDecodeStateMachine::Run()
{
  nsChannelReader* reader = mDecoder->GetReader();
  NS_ENSURE_TRUE(reader, NS_ERROR_NULL_POINTER);
  while (PR_TRUE) {
   nsAutoMonitor mon(mDecoder->GetMonitor());
   switch(mState) {
    case DECODER_STATE_SHUTDOWN:
      if (mPlaying) {
        StopPlayback();
      }
      return NS_OK;

    case DECODER_STATE_DECODING_METADATA:
      mon.Exit();
      LoadOggHeaders(reader);
      mon.Enter();
      
      if (mState == DECODER_STATE_DECODING_METADATA) {
        LOG(PR_LOG_DEBUG, ("Changed state from DECODING_METADATA to DECODING_FIRSTFRAME"));
        mState = DECODER_STATE_DECODING_FIRSTFRAME;
      }
      break;

    case DECODER_STATE_DECODING_FIRSTFRAME:
      {
        OggPlayErrorCode r;
        do {
          mon.Exit();
          r = DecodeFrame();
          mon.Enter();
        } while (mState != DECODER_STATE_SHUTDOWN && r == E_OGGPLAY_TIMEOUT);

        if (mState == DECODER_STATE_SHUTDOWN)
          continue;

        mLastFrameTime = 0;
        FrameData* frame = NextFrame();
        if (frame) {
          mDecodedFrames.Push(frame);
          mDecoder->mPlaybackPosition = frame->mEndStreamPosition;
          UpdatePlaybackPosition(frame->mDecodedFrameTime);
          PlayVideo(frame);
        }

        nsCOMPtr<nsIRunnable> event =
          NS_NEW_RUNNABLE_METHOD(nsOggDecoder, mDecoder, FirstFrameLoaded);
        NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);

        if (mState == DECODER_STATE_DECODING_FIRSTFRAME) {
          LOG(PR_LOG_DEBUG, ("Changed state from DECODING_FIRSTFRAME to DECODING"));
          mState = DECODER_STATE_DECODING;
        }
      }
      break;

    case DECODER_STATE_DECODING:
      {
        PRBool bufferExhausted = PR_FALSE;

        if (!mDecodedFrames.IsFull()) {
          PRInt64 initialDownloadPosition =
            mDecoder->mReader->Stream()->GetCachedDataEnd(mDecoder->mDecoderPosition);

          mon.Exit();
          OggPlayErrorCode r = DecodeFrame();
          mon.Enter();

          // Check whether decoding that frame required us to read data
          // that wasn't available at the start of the frame. That means
          // we should probably start buffering.
          bufferExhausted =
            mDecoder->mDecoderPosition > initialDownloadPosition;

          if (mState != DECODER_STATE_DECODING)
            continue;

          // Get the decoded frame and store it in our queue of decoded frames
          FrameData* frame = NextFrame();
          if (frame) {
            mDecodedFrames.Push(frame);
          }

          if (r != E_OGGPLAY_CONTINUE &&
              r != E_OGGPLAY_USER_INTERRUPT &&
              r != E_OGGPLAY_TIMEOUT)  {
            LOG(PR_LOG_DEBUG, ("Changed state from DECODING to COMPLETED"));
            mState = DECODER_STATE_COMPLETED;
          }
        }

        // Show at least the first frame if we're not playing
        // so we have a poster frame on initial load and after seek.
        if (!mPlaying && !mDecodedFrames.IsEmpty()) {
          PlayVideo(mDecodedFrames.Peek());
        }

        if (bufferExhausted && mState == DECODER_STATE_DECODING &&
            mDecoder->GetState() == nsOggDecoder::PLAY_STATE_PLAYING &&
            !mDecoder->mReader->Stream()->IsDataCachedToEndOfStream(mDecoder->mDecoderPosition) &&
            !mDecoder->mReader->Stream()->IsSuspendedByCache()) {
          // There is at most one frame in the queue and there's
          // more data to load. Let's buffer to make sure we can play a
          // decent amount of video in the future.
          if (mPlaying) {
            StopPlayback();
          }

          // We need to tell the element that buffering has started.
          // We can't just directly send an asynchronous runnable that
          // eventually fires the "waiting" event. The problem is that
          // there might be pending main-thread events, such as "data
          // received" notifications, that mean we're not actually still
          // buffering by the time this runnable executes. So instead
          // we just trigger UpdateReadyStateForData; when it runs, it
          // will check the current state and decide whether to tell
          // the element we're buffering or not.
          nsCOMPtr<nsIRunnable> event = 
            NS_NEW_RUNNABLE_METHOD(nsOggDecoder, mDecoder, UpdateReadyStateForData);
          NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);

          mBufferingStart = PR_IntervalNow();
          PRPackedBool reliable;
          double playbackRate = mDecoder->ComputePlaybackRate(&reliable);
          mBufferingEndOffset = mDecoder->mDecoderPosition +
              BUFFERING_RATE(playbackRate) * BUFFERING_WAIT;
          mState = DECODER_STATE_BUFFERING;
          LOG(PR_LOG_DEBUG, ("Changed state from DECODING to BUFFERING"));
        } else {
          PlayFrame();
        }
      }
      break;

    case DECODER_STATE_SEEKING:
      {
        // During the seek, don't have a lock on the decoder state,
        // otherwise long seek operations can block the main thread.
        // The events dispatched to the main thread are SYNC calls.
        // These calls are made outside of the decode monitor lock so
        // it is safe for the main thread to makes calls that acquire
        // the lock since it won't deadlock. We check the state when
        // acquiring the lock again in case shutdown has occurred
        // during the time when we didn't have the lock.
        float seekTime = mSeekTime;
        mDecoder->StopProgressUpdates();
        mon.Exit();
        nsCOMPtr<nsIRunnable> startEvent = 
          NS_NEW_RUNNABLE_METHOD(nsOggDecoder, mDecoder, SeekingStarted);
        NS_DispatchToMainThread(startEvent, NS_DISPATCH_SYNC);
        
        LOG(PR_LOG_DEBUG, ("Entering oggplay_seek(%f)", seekTime));
        oggplay_seek(mPlayer, ogg_int64_t(seekTime * 1000));
        LOG(PR_LOG_DEBUG, ("Leaving oggplay_seek"));

        // Reactivate all tracks. Liboggplay deactivates tracks when it
        // reads to the end of stream, but they must be reactivated in order
        // to start reading from them again.
        for (int i = 0; i < oggplay_get_num_tracks(mPlayer); ++i) {
         if (oggplay_set_track_active(mPlayer, i) < 0)  {
            LOG(PR_LOG_ERROR, ("Could not set track %d active", i));
          }
        }

        mon.Enter();
        mDecoder->StartProgressUpdates();
        mLastFramePosition = mDecoder->mPlaybackPosition;
        if (mState == DECODER_STATE_SHUTDOWN)
          continue;

        // Remove all frames decoded prior to seek from the queue
        while (!mDecodedFrames.IsEmpty()) {
          delete mDecodedFrames.Pop();
        }

        OggPlayErrorCode r;
        do {
          mon.Exit();
          r = DecodeFrame();
          mon.Enter();
        } while (mState != DECODER_STATE_SHUTDOWN && r == E_OGGPLAY_TIMEOUT);

        if (mState == DECODER_STATE_SHUTDOWN)
          continue;

        mLastFrameTime = 0;
        FrameData* frame = NextFrame();
        NS_ASSERTION(frame != nsnull, "No frame after seek!");
        if (frame) {
          mDecodedFrames.Push(frame);
          UpdatePlaybackPosition(frame->mDecodedFrameTime);
          PlayVideo(frame);
        }
        mon.Exit();
        nsCOMPtr<nsIRunnable> stopEvent = 
          NS_NEW_RUNNABLE_METHOD(nsOggDecoder, mDecoder, SeekingStopped);
        NS_DispatchToMainThread(stopEvent, NS_DISPATCH_SYNC);        
        mon.Enter();

        if (mState == DECODER_STATE_SEEKING && mSeekTime == seekTime) {
          LOG(PR_LOG_DEBUG, ("Changed state from SEEKING (to %f) to DECODING", seekTime));
          mState = DECODER_STATE_DECODING;
        }
      }
      break;

    case DECODER_STATE_BUFFERING:
      {
        PRIntervalTime now = PR_IntervalNow();
        if ((PR_IntervalToMilliseconds(now - mBufferingStart) < BUFFERING_WAIT*1000) &&
            mDecoder->mReader->Stream()->GetCachedDataEnd(mDecoder->mDecoderPosition) < mBufferingEndOffset &&
            !mDecoder->mReader->Stream()->IsDataCachedToEndOfStream(mDecoder->mDecoderPosition) &&
            !mDecoder->mReader->Stream()->IsSuspendedByCache()) {
          LOG(PR_LOG_DEBUG, 
              ("In buffering: buffering data until %d bytes available or %d milliseconds", 
               PRUint32(mBufferingEndOffset - mDecoder->mReader->Stream()->GetCachedDataEnd(mDecoder->mDecoderPosition)),
               BUFFERING_WAIT*1000 - (PR_IntervalToMilliseconds(now - mBufferingStart))));
          mon.Wait(PR_MillisecondsToInterval(1000));
          if (mState == DECODER_STATE_SHUTDOWN)
            continue;
        } else {
          LOG(PR_LOG_DEBUG, ("Changed state from BUFFERING to DECODING"));
          mState = DECODER_STATE_DECODING;
        }

        if (mState != DECODER_STATE_BUFFERING) {
          nsCOMPtr<nsIRunnable> event = 
            NS_NEW_RUNNABLE_METHOD(nsOggDecoder, mDecoder, UpdateReadyStateForData);
          NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
          if (mDecoder->GetState() == nsOggDecoder::PLAY_STATE_PLAYING) {
            if (!mPlaying) {
              StartPlayback();
            }
          }
        }

        break;
      }

    case DECODER_STATE_COMPLETED:
      {
        while (mState == DECODER_STATE_COMPLETED &&
               !mDecodedFrames.IsEmpty()) {
          PlayFrame();
          if (mState != DECODER_STATE_SHUTDOWN) {
            // Wait for the time of one frame so we don't tight loop
            // and we need to release the monitor so timeupdate and
            // invalidate's on the main thread can occur.
            mon.Wait(PR_MillisecondsToInterval(PRInt64(mCallbackPeriod*1000)));
          }
        }

        if (mState != DECODER_STATE_COMPLETED)
          continue;

        if (mAudioStream) {
          mon.Exit();
          LOG(PR_LOG_DEBUG, ("Begin nsAudioStream::Drain"));
          mAudioStream->Drain();
          LOG(PR_LOG_DEBUG, ("End nsAudioStream::Drain"));
          mon.Enter();

          // After the drain call the audio stream is unusable. Close it so that
          // next time audio is used a new stream is created. The StopPlayback
          // call also resets the playing flag so audio is restarted correctly.
          StopPlayback();

          if (mState != DECODER_STATE_COMPLETED)
            continue;
        }

        nsCOMPtr<nsIRunnable> event =
          NS_NEW_RUNNABLE_METHOD(nsOggDecoder, mDecoder, PlaybackEnded);
        NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
        do {
          mon.Wait();
        } while (mState == DECODER_STATE_COMPLETED);
      }
      break;
    }
  }

  return NS_OK;
}

void nsOggDecodeStateMachine::LoadOggHeaders(nsChannelReader* aReader) 
{
  LOG(PR_LOG_DEBUG, ("Loading Ogg Headers"));
  mPlayer = oggplay_open_with_reader(aReader);
  if (mPlayer) {
    LOG(PR_LOG_DEBUG, ("There are %d tracks", oggplay_get_num_tracks(mPlayer)));

    for (int i = 0; i < oggplay_get_num_tracks(mPlayer); ++i) {
      LOG(PR_LOG_DEBUG, ("Tracks %d: %s", i, oggplay_get_track_typename(mPlayer, i)));
      if (mVideoTrack == -1 && oggplay_get_track_type(mPlayer, i) == OGGZ_CONTENT_THEORA) {
        oggplay_set_callback_num_frames(mPlayer, i, 1);
        mVideoTrack = i;

        int fpsd, fpsn;
        oggplay_get_video_fps(mPlayer, i, &fpsd, &fpsn);
        mFramerate = fpsd == 0 ? 0.0 : float(fpsn)/float(fpsd);
        mCallbackPeriod = 1.0 / mFramerate;
        LOG(PR_LOG_DEBUG, ("Frame rate: %f", mFramerate));

        int y_width;
        int y_height;
        oggplay_get_video_y_size(mPlayer, i, &y_width, &y_height);
        mDecoder->SetRGBData(y_width, y_height, mFramerate, nsnull);
      }
      else if (mAudioTrack == -1 && oggplay_get_track_type(mPlayer, i) == OGGZ_CONTENT_VORBIS) {
        mAudioTrack = i;
        oggplay_set_offset(mPlayer, i, OGGPLAY_AUDIO_OFFSET);
        oggplay_get_audio_samplerate(mPlayer, i, &mAudioRate);
        oggplay_get_audio_channels(mPlayer, i, &mAudioChannels);
        LOG(PR_LOG_DEBUG, ("samplerate: %d, channels: %d", mAudioRate, mAudioChannels));
      }
 
      if (oggplay_set_track_active(mPlayer, i) < 0)  {
        LOG(PR_LOG_ERROR, ("Could not set track %d active", i));
      }
    }

    if (mVideoTrack == -1) {
      oggplay_set_callback_num_frames(mPlayer, mAudioTrack, OGGPLAY_FRAMES_PER_CALLBACK);
      mCallbackPeriod = 1.0 / (float(mAudioRate) / OGGPLAY_FRAMES_PER_CALLBACK);
    }
    LOG(PR_LOG_DEBUG, ("Callback Period: %f", mCallbackPeriod));

    oggplay_use_buffer(mPlayer, OGGPLAY_BUFFER_SIZE);

    // Get the duration from the Ogg file. We only do this if the
    // content length of the resource is known as we need to seek
    // to the end of the file to get the last time field. We also
    // only do this if the resource is seekable and if we haven't
    // already obtained the duration via an HTTP header.
    {
      nsAutoMonitor mon(mDecoder->GetMonitor());
      if (mState != DECODER_STATE_SHUTDOWN &&
          aReader->Stream()->GetLength() >= 0 &&
          mSeekable &&
          mDuration == -1) {
        mDecoder->StopProgressUpdates();
        // Don't hold the monitor during the duration
        // call as it can issue seek requests
        // and blocks until these are completed.
        mon.Exit();
        PRInt64 d = oggplay_get_duration(mPlayer);
        oggplay_seek(mPlayer, 0);
        mon.Enter();
        mDuration = d;
        mDecoder->StartProgressUpdates();
        mDecoder->UpdatePlaybackRate();
      }
      if (mState == DECODER_STATE_SHUTDOWN)
        return;
    }

    // Inform the element that we've loaded the Ogg metadata
    nsCOMPtr<nsIRunnable> metadataLoadedEvent = 
      NS_NEW_RUNNABLE_METHOD(nsOggDecoder, mDecoder, MetadataLoaded); 
    
    NS_DispatchToMainThread(metadataLoadedEvent, NS_DISPATCH_NORMAL);
  }
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsOggDecoder, nsIObserver)

void nsOggDecoder::Pause() 
{
  nsAutoMonitor mon(mMonitor);
  if (mPlayState == PLAY_STATE_SEEKING) {
    mNextState = PLAY_STATE_PAUSED;
    return;
  }

  ChangeState(PLAY_STATE_PAUSED);
}

void nsOggDecoder::SetVolume(float volume)
{
  nsAutoMonitor mon(mMonitor);
  mInitialVolume = volume;

  if (mDecodeStateMachine) {
    mDecodeStateMachine->SetVolume(volume);
  }
}

float nsOggDecoder::GetDuration()
{
  if (mDuration >= 0) {
     return static_cast<float>(mDuration) / 1000.0;
  }

  return std::numeric_limits<float>::quiet_NaN();
}

nsOggDecoder::nsOggDecoder() :
  nsMediaDecoder(),
  mDecoderPosition(0),
  mPlaybackPosition(0),
  mCurrentTime(0.0),
  mInitialVolume(0.0),
  mRequestedSeekTime(-1.0),
  mDuration(-1),
  mNotifyOnShutdown(PR_FALSE),
  mSeekable(PR_TRUE),
  mReader(nsnull),
  mMonitor(nsnull),
  mPlayState(PLAY_STATE_PAUSED),
  mNextState(PLAY_STATE_PAUSED),
  mResourceLoaded(PR_FALSE),
  mIgnoreProgressData(PR_FALSE)
{
  MOZ_COUNT_CTOR(nsOggDecoder);
}

PRBool nsOggDecoder::Init(nsHTMLMediaElement* aElement)
{
  mMonitor = nsAutoMonitor::NewMonitor("media.decoder");
  return mMonitor && nsMediaDecoder::Init(aElement);
}

void nsOggDecoder::Shutdown()
{
  mShuttingDown = PR_TRUE;

  ChangeState(PLAY_STATE_SHUTDOWN);
  nsMediaDecoder::Shutdown();

  Stop();
}

nsOggDecoder::~nsOggDecoder()
{
  MOZ_COUNT_DTOR(nsOggDecoder);
  nsAutoMonitor::DestroyMonitor(mMonitor);
}

nsresult nsOggDecoder::Load(nsIURI* aURI, nsIChannel* aChannel,
                            nsIStreamListener** aStreamListener)
{
  // Reset Stop guard flag flag, else shutdown won't occur properly when
  // reusing decoder.
  mStopping = PR_FALSE;

  // Reset progress member variables
  mDecoderPosition = 0;
  mPlaybackPosition = 0;
  mResourceLoaded = PR_FALSE;

  NS_ASSERTION(!mReader, "Didn't shutdown properly!");
  NS_ASSERTION(!mDecodeStateMachine, "Didn't shutdown properly!");
  NS_ASSERTION(!mDecodeThread, "Didn't shutdown properly!");

  if (aStreamListener) {
    *aStreamListener = nsnull;
  }

  if (aURI) {
    NS_ASSERTION(!aStreamListener, "No listener should be requested here");
    mURI = aURI;
  } else {
    NS_ASSERTION(aChannel, "Either a URI or a channel is required");
    NS_ASSERTION(aStreamListener, "A listener should be requested here");

    // If the channel was redirected, we want the post-redirect URI;
    // but if the URI scheme was expanded, say from chrome: to jar:file:,
    // we want the original URI.
    nsresult rv = NS_GetFinalChannelURI(aChannel, getter_AddRefs(mURI));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  RegisterShutdownObserver();

  mReader = new nsChannelReader();
  NS_ENSURE_TRUE(mReader, NS_ERROR_OUT_OF_MEMORY);

  {
    nsAutoMonitor mon(mMonitor);
    // Hold the lock while we do this to set proper lock ordering
    // expectations for dynamic deadlock detectors: decoder lock(s)
    // should be grabbed before the cache lock
    nsresult rv = mReader->Init(this, mURI, aChannel, aStreamListener);
    if (NS_FAILED(rv)) {
      // Free the failed-to-initialize reader so we don't try to use it.
      mReader = nsnull;
      return rv;
    }
  }

  nsresult rv = NS_NewThread(getter_AddRefs(mDecodeThread));
  NS_ENSURE_SUCCESS(rv, rv);

  mDecodeStateMachine = new nsOggDecodeStateMachine(this);
  {
    nsAutoMonitor mon(mMonitor);
    mDecodeStateMachine->SetSeekable(mSeekable);
  }

  ChangeState(PLAY_STATE_LOADING);

  return mDecodeThread->Dispatch(mDecodeStateMachine, NS_DISPATCH_NORMAL);
}

nsresult nsOggDecoder::Play()
{
  nsAutoMonitor mon(mMonitor);
  if (mPlayState == PLAY_STATE_SEEKING) {
    mNextState = PLAY_STATE_PLAYING;
    return NS_OK;
  }
  if (mPlayState == PLAY_STATE_ENDED)
    return Seek(0);

  ChangeState(PLAY_STATE_PLAYING);

  return NS_OK;
}

nsresult nsOggDecoder::Seek(float aTime)
{
  nsAutoMonitor mon(mMonitor);

  if (aTime < 0.0)
    return NS_ERROR_FAILURE;

  mRequestedSeekTime = aTime;

  // If we are already in the seeking state, then setting mRequestedSeekTime
  // above will result in the new seek occurring when the current seek
  // completes.
  if (mPlayState != PLAY_STATE_SEEKING) {
    if (mPlayState == PLAY_STATE_ENDED) {
      mNextState = PLAY_STATE_PLAYING;
    } else {
      mNextState = mPlayState;
    }
    ChangeState(PLAY_STATE_SEEKING);
  }

  return NS_OK;
}

nsresult nsOggDecoder::PlaybackRateChanged()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

// Postpones destruction of nsOggDecoder's objects, so they can be safely
// performed later, when events can't interfere.
class nsDestroyStateMachine : public nsRunnable {
public:
  nsDestroyStateMachine(nsOggDecoder *aDecoder,
                        nsOggDecodeStateMachine *aMachine,
                        nsChannelReader *aReader,
                        nsIThread *aThread)
  : mDecoder(aDecoder),
    mDecodeStateMachine(aMachine),
    mReader(aReader),
    mDecodeThread(aThread)
  {
  }

  NS_IMETHOD Run() {
    NS_ASSERTION(NS_IsMainThread(), "Should be called on main thread");
    // The decode thread must die before the state machine can die.
    // The state machine must die before the reader.
    // The state machine must die before the decoder.
    if (mDecodeThread)
      mDecodeThread->Shutdown();
    mDecodeThread = nsnull;
    mDecodeStateMachine = nsnull;
    mReader = nsnull;
    mDecoder = nsnull;
    return NS_OK;
  }

private:
  nsRefPtr<nsOggDecoder> mDecoder;
  nsCOMPtr<nsOggDecodeStateMachine> mDecodeStateMachine;
  nsAutoPtr<nsChannelReader> mReader;
  nsCOMPtr<nsIThread> mDecodeThread;
};

void nsOggDecoder::Stop()
{
  NS_ASSERTION(NS_IsMainThread(), 
               "nsOggDecoder::Stop called on non-main thread");  
  
  if (mStopping)
    return;

  mStopping = PR_TRUE;

  ChangeState(PLAY_STATE_ENDED);

  StopProgress();

  // Force any outstanding seek and byterange requests to complete
  // to prevent shutdown from deadlocking.
  mReader->Stream()->Close();

  // Shutdown must be on called the mDecodeStateMachine before deleting.
  // This is required to ensure that the state machine isn't running
  // in the thread and using internal objects when it is deleted.
  if (mDecodeStateMachine) {
    mDecodeStateMachine->Shutdown();
  }

  // mDecodeThread holds a ref to mDecodeStateMachine, so we can't destroy
  // mDecodeStateMachine until mDecodeThread is destroyed. We can't destroy
  // mReader until mDecodeStateMachine is destroyed because mDecodeStateMachine
  // uses mReader in its destructor. In addition, it's unsafe to Shutdown() the
  // decode thread here, as nsIThread::Shutdown() may run events, such as JS
  // event handlers, which could kick off a new Load().
  // mDecodeStateMachine::Run() may also be holding a reference to the decoder
  // in an event runner object on its stack, so the decoder must outlive the
  // state machine, else we may destroy the decoder on a non-main thread,
  // and its monitor doesn't like that. So we need to create a new event which
  // holds references the decoder, reader, thread, and state machine, and
  // releases them safely later on the main thread when events can't interfere.
  // See bug 468721.
  nsCOMPtr<nsIRunnable> event = new nsDestroyStateMachine(this,
                                                          mDecodeStateMachine,
                                                          mReader.forget(),
                                                          mDecodeThread);
  NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);

  // Null data fields. They can be reinitialized in future Load()s safely now.
  mDecodeThread = nsnull;
  mDecodeStateMachine = nsnull;
  UnregisterShutdownObserver();
}

float nsOggDecoder::GetCurrentTime()
{
  return mCurrentTime;
}

void nsOggDecoder::GetCurrentURI(nsIURI** aURI)
{
  NS_IF_ADDREF(*aURI = mURI);
}

already_AddRefed<nsIPrincipal> nsOggDecoder::GetCurrentPrincipal()
{
  if (!mReader)
    return nsnull;
  return mReader->Stream()->GetCurrentPrincipal();
}

void nsOggDecoder::MetadataLoaded()
{
  if (mShuttingDown)
    return;

  // Only inform the element of MetadataLoaded if not doing a load() in order
  // to fulfill a seek, otherwise we'll get multiple metadataloaded events.
  PRBool notifyElement = PR_TRUE;
  {
    nsAutoMonitor mon(mMonitor);
    mDuration = mDecodeStateMachine ? mDecodeStateMachine->GetDuration() : -1;
    notifyElement = mNextState != PLAY_STATE_SEEKING;
  }

  if (mElement && notifyElement) {
    // Make sure the element and the frame (if any) are told about
    // our new size.
    Invalidate();
    mElement->MetadataLoaded();
  }

  if (!mResourceLoaded) {
    StartProgress();
  }
  else if (mElement)
  {
    // Resource was loaded during metadata loading, when progress
    // events are being ignored. Fire the final progress event.
    mElement->DispatchAsyncProgressEvent(NS_LITERAL_STRING("progress"));
  }
}

void nsOggDecoder::FirstFrameLoaded()
{
  if (mShuttingDown)
    return;
 
  // Only inform the element of FirstFrameLoaded if not doing a load() in order
  // to fulfill a seek, otherwise we'll get multiple loadedfirstframe events.
  PRBool notifyElement = PR_TRUE;
  {
    nsAutoMonitor mon(mMonitor);
    notifyElement = mNextState != PLAY_STATE_SEEKING;
  }  

  if (mElement && notifyElement) {
    mElement->FirstFrameLoaded();
  }

  // The element can run javascript via events
  // before reaching here, so only change the 
  // state if we're still set to the original
  // loading state.
  nsAutoMonitor mon(mMonitor);
  if (mPlayState == PLAY_STATE_LOADING) {
    if (mRequestedSeekTime >= 0.0) {
      ChangeState(PLAY_STATE_SEEKING);
    }
    else {
      ChangeState(mNextState);
    }
  }

  if (!mResourceLoaded && mReader &&
      mReader->Stream()->IsDataCachedToEndOfStream(mDecoderPosition)) {
    ResourceLoaded();
  }
}

void nsOggDecoder::ResourceLoaded()
{
  // Don't handle ResourceLoaded if we are shutting down, or if
  // we need to ignore progress data due to seeking (in the case
  // that the seek results in reaching end of file, we get a bogus call
  // to ResourceLoaded).
  if (mShuttingDown)
    return;

  {
    // If we are seeking or loading then the resource loaded notification we get
    // should be ignored, since it represents the end of the seek request.
    nsAutoMonitor mon(mMonitor);
    if (mIgnoreProgressData || mResourceLoaded || mPlayState == PLAY_STATE_LOADING)
      return;

    Progress(PR_FALSE);

    mResourceLoaded = PR_TRUE;
    StopProgress();
  }

  // Ensure the final progress event gets fired
  if (mElement) {
    mElement->DispatchAsyncProgressEvent(NS_LITERAL_STRING("progress"));
    mElement->ResourceLoaded();
  }
}

void nsOggDecoder::NetworkError()
{
  if (mStopping || mShuttingDown)
    return;

  if (mElement)
    mElement->NetworkError();
  Stop();
}

PRBool nsOggDecoder::IsSeeking() const
{
  return mPlayState == PLAY_STATE_SEEKING || mNextState == PLAY_STATE_SEEKING;
}

PRBool nsOggDecoder::IsEnded() const
{
  return mPlayState == PLAY_STATE_ENDED || mPlayState == PLAY_STATE_SHUTDOWN;
}

void nsOggDecoder::PlaybackEnded()
{
  if (mShuttingDown || mPlayState == nsOggDecoder::PLAY_STATE_SEEKING)
    return;

  ChangeState(PLAY_STATE_ENDED);

  if (mElement)  {
    mElement->PlaybackEnded();
  }
}

NS_IMETHODIMP nsOggDecoder::Observe(nsISupports *aSubjet,
                                      const char *aTopic,
                                      const PRUnichar *someData)
{
  if (strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0) {
    Shutdown();
  }

  return NS_OK;
}

nsMediaDecoder::Statistics
nsOggDecoder::GetStatistics()
{
  Statistics result;

  nsAutoMonitor mon(mMonitor);
  if (mReader) {
    result.mDownloadRate = 
      mReader->Stream()->GetDownloadRate(&result.mDownloadRateReliable);
    result.mDownloadPosition =
      mReader->Stream()->GetCachedDataEnd(mDecoderPosition);
    result.mTotalBytes = mReader->Stream()->GetLength();
    result.mPlaybackRate = ComputePlaybackRate(&result.mPlaybackRateReliable);
    result.mDecoderPosition = mDecoderPosition;
    result.mPlaybackPosition = mPlaybackPosition;
  } else {
    result.mDownloadRate = 0;
    result.mDownloadRateReliable = PR_TRUE;
    result.mPlaybackRate = 0;
    result.mPlaybackRateReliable = PR_TRUE;
    result.mDecoderPosition = 0;
    result.mPlaybackPosition = 0;
    result.mDownloadPosition = 0;
    result.mTotalBytes = 0;
  }

  return result;
}

double nsOggDecoder::ComputePlaybackRate(PRPackedBool* aReliable)
{
  PRInt64 length = mReader ? mReader->Stream()->GetLength() : -1;
  if (mDuration >= 0 && length >= 0) {
    *aReliable = PR_TRUE;
    return double(length)*1000.0/mDuration;
  }
  return mPlaybackStatistics.GetRateAtLastStop(aReliable);
}

void nsOggDecoder::UpdatePlaybackRate()
{
  if (!mReader)
    return;
  PRPackedBool reliable;
  PRUint32 rate = PRUint32(ComputePlaybackRate(&reliable));
  if (!reliable) {
    // Set a minimum rate of 10,000 bytes per second ... sometimes we just
    // don't have good data
    rate = PR_MAX(rate, 10000);
  }
  mReader->Stream()->SetPlaybackRate(rate);
}

void nsOggDecoder::NotifySuspendedStatusChanged()
{
  NS_ASSERTION(NS_IsMainThread(), 
               "nsOggDecoder::NotifyDownloadSuspended called on non-main thread");
  if (!mReader)
    return;
  if (mReader->Stream()->IsSuspendedByCache() && mElement) {
    // if this is an autoplay element, we need to kick off its autoplaying
    // now so we consume data and hopefully free up cache space
    mElement->NotifyAutoplayDataReady();
  }
}

void nsOggDecoder::NotifyBytesDownloaded()
{
  NS_ASSERTION(NS_IsMainThread(),
               "nsOggDecoder::NotifyBytesDownloaded called on non-main thread");   
  UpdateReadyStateForData();
}

void nsOggDecoder::NotifyDownloadEnded(nsresult aStatus)
{
  if (aStatus == NS_BINDING_ABORTED)
    return;

  {
    nsAutoMonitor mon(mMonitor);
    UpdatePlaybackRate();
  }

  if (NS_SUCCEEDED(aStatus)) {
    ResourceLoaded();
  } else if (aStatus != NS_BASE_STREAM_CLOSED) {
    NetworkError();
  }
  UpdateReadyStateForData();
}

void nsOggDecoder::NotifyBytesConsumed(PRInt64 aBytes)
{
  nsAutoMonitor mon(mMonitor);
  if (!mIgnoreProgressData) {
    mDecoderPosition += aBytes;
  }
}

void nsOggDecoder::UpdateReadyStateForData()
{
  if (!mElement || mShuttingDown || !mDecodeStateMachine)
    return;

  nsHTMLMediaElement::NextFrameStatus frameStatus;
  {
    nsAutoMonitor mon(mMonitor);
    if (mDecodeStateMachine->HaveNextFrameData()) {
      frameStatus = nsHTMLMediaElement::NEXT_FRAME_AVAILABLE;
    } else if (mDecodeStateMachine->IsBuffering()) {
      frameStatus = nsHTMLMediaElement::NEXT_FRAME_UNAVAILABLE_BUFFERING;
    } else {
      frameStatus = nsHTMLMediaElement::NEXT_FRAME_UNAVAILABLE;
    }
  }
  mElement->UpdateReadyStateForData(frameStatus);
}

void nsOggDecoder::SeekingStopped()
{
  if (mShuttingDown)
    return;

  {
    nsAutoMonitor mon(mMonitor);

    // An additional seek was requested while the current seek was
    // in operation.
    if (mRequestedSeekTime >= 0.0)
      ChangeState(PLAY_STATE_SEEKING);
    else
      ChangeState(mNextState);
  }

  if (mElement) {
    mElement->SeekCompleted();
    UpdateReadyStateForData();
  }
}

void nsOggDecoder::SeekingStarted()
{
  if (mShuttingDown)
    return;

  if (mElement) {
    mElement->SeekStarted();
  }
}

void nsOggDecoder::RegisterShutdownObserver()
{
  if (!mNotifyOnShutdown) {
    nsCOMPtr<nsIObserverService> observerService =
      do_GetService("@mozilla.org/observer-service;1");
    if (observerService) {
      mNotifyOnShutdown = 
        NS_SUCCEEDED(observerService->AddObserver(this, 
                                                  NS_XPCOM_SHUTDOWN_OBSERVER_ID, 
                                                  PR_FALSE));
    }
    else {
      NS_WARNING("Could not get an observer service. Video decoding events may not shutdown cleanly.");
    }
  }
}

void nsOggDecoder::UnregisterShutdownObserver()
{
  if (mNotifyOnShutdown) {
    nsCOMPtr<nsIObserverService> observerService =
      do_GetService("@mozilla.org/observer-service;1");
    if (observerService) {
      mNotifyOnShutdown = PR_FALSE;
      observerService->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
    }
  }
}

void nsOggDecoder::ChangeState(PlayState aState)
{
  NS_ASSERTION(NS_IsMainThread(), 
               "nsOggDecoder::ChangeState called on non-main thread");   
  nsAutoMonitor mon(mMonitor);

  if (mNextState == aState) {
    mNextState = PLAY_STATE_PAUSED;
  }

  if (mPlayState == PLAY_STATE_SHUTDOWN) {
    mon.NotifyAll();
    return;
  }

  mPlayState = aState;
  switch (aState) {
  case PLAY_STATE_PAUSED:
    /* No action needed */
    break;
  case PLAY_STATE_PLAYING:
    mDecodeStateMachine->Decode();
    break;
  case PLAY_STATE_SEEKING:
    mDecodeStateMachine->Seek(mRequestedSeekTime);
    mRequestedSeekTime = -1.0;
    break;
  case PLAY_STATE_LOADING:
    /* No action needed */
    break;
  case PLAY_STATE_START:
    /* No action needed */
    break;
  case PLAY_STATE_ENDED:
    /* No action needed */
    break;
  case PLAY_STATE_SHUTDOWN:
    /* No action needed */
    break;
  }
  mon.NotifyAll();
}

void nsOggDecoder::PlaybackPositionChanged()
{
  if (mShuttingDown)
    return;

  float lastTime = mCurrentTime;

  // Control the scope of the monitor so it is not
  // held while the timeupdate and the invalidate is run.
  {
    nsAutoMonitor mon(mMonitor);

    if (mDecodeStateMachine) {
      mCurrentTime = mDecodeStateMachine->GetCurrentTime();
      mDecodeStateMachine->ClearPositionChangeFlag();
    }
  }

  // Invalidate the frame so any video data is displayed.
  // Do this before the timeupdate event so that if that
  // event runs JavaScript that queries the media size, the
  // frame has reflowed and the size updated beforehand.
  Invalidate();

  if (mElement && lastTime != mCurrentTime) {
    mElement->DispatchSimpleEvent(NS_LITERAL_STRING("timeupdate"));
  }
}

void nsOggDecoder::SetDuration(PRInt64 aDuration)
{
  mDuration = aDuration;
  if (mDecodeStateMachine) {
    nsAutoMonitor mon(mMonitor);
    mDecodeStateMachine->SetDuration(mDuration);

    if (mReader) {
      mReader->SetDuration(mDuration);
    }
    UpdatePlaybackRate();
  }
}

void nsOggDecoder::SetSeekable(PRBool aSeekable)
{
  mSeekable = aSeekable;
  if (mDecodeStateMachine) {
    nsAutoMonitor mon(mMonitor);
    mDecodeStateMachine->SetSeekable(aSeekable);
  }
}

PRBool nsOggDecoder::GetSeekable()
{
  return mSeekable;
}

void nsOggDecoder::Suspend()
{
  if (mReader) {
    mReader->Stream()->Suspend();
  }
}

void nsOggDecoder::Resume()
{
  if (mReader) {
    mReader->Stream()->Resume();
  }
}

void nsOggDecoder::StopProgressUpdates()
{
  mIgnoreProgressData = PR_TRUE;
  if (mReader) {
    mReader->Stream()->SetReadMode(nsMediaCacheStream::MODE_METADATA);
  }
}

void nsOggDecoder::StartProgressUpdates()
{
  mIgnoreProgressData = PR_FALSE;
  if (mReader) {
    mReader->Stream()->SetReadMode(nsMediaCacheStream::MODE_PLAYBACK);
    mDecoderPosition = mPlaybackPosition = mReader->Stream()->Tell();
  }
}

void nsOggDecoder::MoveLoadsToBackground()
{
  if (mReader && mReader->Stream()) {
    mReader->Stream()->MoveLoadsToBackground();
  }
}

