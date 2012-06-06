/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_SpdySession2_h
#define mozilla_net_SpdySession2_h

// SPDY as defined by
// http://www.chromium.org/spdy/spdy-protocol/spdy-protocol-draft2

#include "ASpdySession.h"
#include "nsClassHashtable.h"
#include "nsDataHashtable.h"
#include "nsDeque.h"
#include "nsHashKeys.h"
#include "zlib.h"
#include "mozilla/Attributes.h"

class nsHttpConnection;
class nsISocketTransport;

namespace mozilla { namespace net {

class SpdyStream2;

class SpdySession2 MOZ_FINAL : public ASpdySession
                             , public nsAHttpConnection
                             , public nsAHttpSegmentReader
                             , public nsAHttpSegmentWriter
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSAHTTPTRANSACTION
  NS_DECL_NSAHTTPCONNECTION(mConnection)
  NS_DECL_NSAHTTPSEGMENTREADER
  NS_DECL_NSAHTTPSEGMENTWRITER

  SpdySession2(nsAHttpTransaction *, nsISocketTransport *, PRInt32);
  ~SpdySession2();

  bool AddStream(nsAHttpTransaction *, PRInt32);
  bool CanReuse() { return !mShouldGoAway && !mClosed; }
  bool RoomForMoreStreams();

  // When the connection is active this is called every 1 second
  void ReadTimeoutTick(PRIntervalTime now);
  
  // Idle time represents time since "goodput".. e.g. a data or header frame
  PRIntervalTime IdleTime();

  PRUint32 RegisterStreamID(SpdyStream2 *);

  const static PRUint8 kFlag_Control   = 0x80;

  const static PRUint8 kFlag_Data_FIN  = 0x01;
  const static PRUint8 kFlag_Data_UNI  = 0x02;
  const static PRUint8 kFlag_Data_ZLIB = 0x02;
  
  // The protocol document for v2 specifies that the
  // highest value (3) is the highest priority, but in
  // reality 0 is the highest priority. 
  //
  // Draft 3 notes here https://sites.google.com/a/chromium.org/dev/spdy/spdy-protocol/
  // are the best guide to the mistake. Also see
  // GetLowestPriority() and GetHighestPriority() in spdy_framer.h of
  // chromium source.

  const static PRUint8 kPri00   = 0 << 6; // highest
  const static PRUint8 kPri01   = 1 << 6;
  const static PRUint8 kPri02   = 2 << 6;
  const static PRUint8 kPri03   = 3 << 6; // lowest

  enum
  {
    CONTROL_TYPE_FIRST = 0,
    CONTROL_TYPE_SYN_STREAM = 1,
    CONTROL_TYPE_SYN_REPLY = 2,
    CONTROL_TYPE_RST_STREAM = 3,
    CONTROL_TYPE_SETTINGS = 4,
    CONTROL_TYPE_NOOP = 5,
    CONTROL_TYPE_PING = 6,
    CONTROL_TYPE_GOAWAY = 7,
    CONTROL_TYPE_HEADERS = 8,
    CONTROL_TYPE_WINDOW_UPDATE = 9,               /* no longer in v2 */
    CONTROL_TYPE_LAST = 10
  };

  enum rstReason
  {
    RST_PROTOCOL_ERROR = 1,
    RST_INVALID_STREAM = 2,
    RST_REFUSED_STREAM = 3,
    RST_UNSUPPORTED_VERSION = 4,
    RST_CANCEL = 5,
    RST_INTERNAL_ERROR = 6,
    RST_FLOW_CONTROL_ERROR = 7,
    RST_BAD_ASSOC_STREAM = 8
  };

  enum
  {
    SETTINGS_TYPE_UPLOAD_BW = 1, // kb/s
    SETTINGS_TYPE_DOWNLOAD_BW = 2, // kb/s
    SETTINGS_TYPE_RTT = 3, // ms
    SETTINGS_TYPE_MAX_CONCURRENT = 4, // streams
    SETTINGS_TYPE_CWND = 5, // packets
    SETTINGS_TYPE_DOWNLOAD_RETRANS_RATE = 6, // percentage
    SETTINGS_TYPE_INITIAL_WINDOW = 7  // bytes. Not used in v2.
  };

  // This should be big enough to hold all of your control packets,
  // but if it needs to grow for huge headers it can do so dynamically.
  // About 1% of requests to SPDY google services seem to be > 1000
  // with all less than 2000.
  const static PRUint32 kDefaultBufferSize = 2048;

  // kDefaultQueueSize must be >= other queue size constants
  const static PRUint32 kDefaultQueueSize =  16384;
  const static PRUint32 kQueueMinimumCleanup = 8192;
  const static PRUint32 kQueueTailRoom    =  4096;
  const static PRUint32 kQueueReserved    =  1024;

  const static PRUint32 kDefaultMaxConcurrent = 100;
  const static PRUint32 kMaxStreamID = 0x7800000;
  
  // This is a sentinel for a deleted stream. It is not a valid
  // 31 bit stream ID.
  const static PRUint32 kDeadStreamID = 0xffffdead;
  
  static nsresult HandleSynStream(SpdySession2 *);
  static nsresult HandleSynReply(SpdySession2 *);
  static nsresult HandleRstStream(SpdySession2 *);
  static nsresult HandleSettings(SpdySession2 *);
  static nsresult HandleNoop(SpdySession2 *);
  static nsresult HandlePing(SpdySession2 *);
  static nsresult HandleGoAway(SpdySession2 *);
  static nsresult HandleHeaders(SpdySession2 *);
  static nsresult HandleWindowUpdate(SpdySession2 *);

  static void EnsureBuffer(nsAutoArrayPtr<char> &,
                           PRUint32, PRUint32, PRUint32 &);

  // For writing the SPDY data stream to LOG4
  static void LogIO(SpdySession2 *, SpdyStream2 *, const char *,
                    const char *, PRUint32);

  // an overload of nsAHttpConnection
  void TransactionHasDataToWrite(nsAHttpTransaction *);

  // a similar version for SpdyStream2
  void TransactionHasDataToWrite(SpdyStream2 *);

  // an overload of nsAHttpSegementReader
  virtual nsresult CommitToSegmentSize(PRUint32 size);
  
private:

  enum stateType {
    BUFFERING_FRAME_HEADER,
    BUFFERING_CONTROL_FRAME,
    PROCESSING_DATA_FRAME,
    DISCARDING_DATA_FRAME,
    PROCESSING_CONTROL_SYN_REPLY,
    PROCESSING_CONTROL_RST_STREAM
  };

  void        DeterminePingThreshold();
  nsresult    HandleSynReplyForValidStream();
  PRUint32    GetWriteQueueSize();
  void        ChangeDownstreamState(enum stateType);
  void        ResetDownstreamState();
  nsresult    DownstreamUncompress(char *, PRUint32);
  void        zlibInit();
  nsresult    FindHeader(nsCString, nsDependentCSubstring &);
  nsresult    ConvertHeaders(nsDependentCSubstring &,
                             nsDependentCSubstring &);
  void        GeneratePing(PRUint32);
  void        ClearPing(bool);
  void        GenerateRstStream(PRUint32, PRUint32);
  void        GenerateGoAway();
  void        CleanupStream(SpdyStream2 *, nsresult, rstReason);
  void        CloseStream(SpdyStream2 *, nsresult);

  void        SetWriteCallbacks();
  void        FlushOutputQueue();

  bool        RoomForMoreConcurrent();
  void        ActivateStream(SpdyStream2 *);
  void        ProcessPending();
  nsresult    SetInputFrameDataStream(PRUint32);
  bool        VerifyStream(SpdyStream2 *, PRUint32);
  void        SetNeedsCleanup();

  // a wrapper for all calls to the nshttpconnection level segment writer. Used
  // to track network I/O for timeout purposes
  nsresult   NetworkRead(nsAHttpSegmentWriter *, char *, PRUint32, PRUint32 *);
  
  static PLDHashOperator ShutdownEnumerator(nsAHttpTransaction *,
                                            nsAutoPtr<SpdyStream2> &,
                                            void *);

  // This is intended to be nsHttpConnectionMgr:nsHttpConnectionHandle taken
  // from the first transaction on this session. That object contains the
  // pointer to the real network-level nsHttpConnection object.
  nsRefPtr<nsAHttpConnection> mConnection;

  // The underlying socket transport object is needed to propogate some events
  nsISocketTransport         *mSocketTransport;

  // These are temporary state variables to hold the argument to
  // Read/WriteSegments so it can be accessed by On(read/write)segment
  // further up the stack.
  nsAHttpSegmentReader       *mSegmentReader;
  nsAHttpSegmentWriter       *mSegmentWriter;

  PRUint32          mSendingChunkSize;        /* the transmission chunk size */
  PRUint32          mNextStreamID;            /* 24 bits */
  PRUint32          mConcurrentHighWater;     /* max parallelism on session */

  stateType         mDownstreamState; /* in frame, between frames, etc..  */

  // Maintain 5 indexes - one by stream ID, one by transaction ptr,
  // one list of streams ready to write, one list of streams that are queued
  // due to max parallelism settings, and one list of streams
  // that must be given priority to write for window updates. The objects
  // are not ref counted - they get destroyed
  // by the nsClassHashtable implementation when they are removed from
  // there.
  nsDataHashtable<nsUint32HashKey, SpdyStream2 *>     mStreamIDHash;
  nsClassHashtable<nsPtrHashKey<nsAHttpTransaction>,
                   SpdyStream2>                       mStreamTransactionHash;
  nsDeque                                             mReadyForWrite;
  nsDeque                                             mQueuedStreams;

  // UrgentForWrite is meant to carry window updates. They were defined in
  // the v2 spec but apparently never implemented so are now scheduled to
  // be removed. But they will be reintroduced for v3, so we will leave
  // this queue in place to ease that transition.
  nsDeque           mUrgentForWrite;

  // Compression contexts for header transport using deflate.
  // SPDY compresses only HTTP headers and does not reset zlib in between
  // frames.
  z_stream            mDownstreamZlib;
  z_stream            mUpstreamZlib;

  // mInputFrameBuffer is used to store received control packets and the 8 bytes
  // of header on data packets
  PRUint32             mInputFrameBufferSize;
  PRUint32             mInputFrameBufferUsed;
  nsAutoArrayPtr<char> mInputFrameBuffer;
  
  // mInputFrameDataSize/Read are used for tracking the amount of data consumed
  // in a data frame. the data itself is not buffered in spdy
  // The frame size is mInputFrameDataSize + the constant 8 byte header
  PRUint32             mInputFrameDataSize;
  PRUint32             mInputFrameDataRead;
  bool                 mInputFrameDataLast; // This frame was marked FIN

  // When a frame has been received that is addressed to a particular stream
  // (e.g. a data frame after the stream-id has been decoded), this points
  // to the stream.
  SpdyStream2          *mInputFrameDataStream;
  
  // mNeedsCleanup is a state variable to defer cleanup of a closed stream
  // If needed, It is set in session::OnWriteSegments() and acted on and
  // cleared when the stack returns to session::WriteSegments(). The stream
  // cannot be destroyed directly out of OnWriteSegments because
  // stream::writeSegments() is on the stack at that time.
  SpdyStream2          *mNeedsCleanup;

  // The CONTROL_TYPE value for a control frame
  PRUint32             mFrameControlType;

  // This reason code in the last processed RESET frame
  PRUint32             mDownstreamRstReason;

  // These are used for decompressing downstream spdy response headers
  // This is done at the session level because sometimes the stream
  // has already been canceled but the decompression still must happen
  // to keep the zlib state correct for the next state of headers.
  PRUint32             mDecompressBufferSize;
  PRUint32             mDecompressBufferUsed;
  nsAutoArrayPtr<char> mDecompressBuffer;

  // for the conversion of downstream http headers into spdy formatted headers
  nsCString            mFlatHTTPResponseHeaders;
  PRUint32             mFlatHTTPResponseHeadersOut;

  // when set, the session will go away when it reaches 0 streams. This flag
  // is set when: the stream IDs are running out (at either the client or the
  // server), when DontReuse() is called, a RST that is not specific to a
  // particular stream is received, a GOAWAY frame has been received from
  // the server.
  bool                 mShouldGoAway;

  // the session has received a nsAHttpTransaction::Close()  call
  bool                 mClosed;

  // the session received a GoAway frame with a valid GoAwayID
  bool                 mCleanShutdown;

  // If a GoAway message was received this is the ID of the last valid
  // stream. 0 otherwise. (0 is never a valid stream id.)
  PRUint32             mGoAwayID;

  // The limit on number of concurrent streams for this session. Normally it
  // is basically unlimited, but the SETTINGS control message from the
  // server might bring it down.
  PRUint32             mMaxConcurrent;

  // The actual number of concurrent streams at this moment. Generally below
  // mMaxConcurrent, but the max can be lowered in real time to a value
  // below the current value
  PRUint32             mConcurrent;

  // The number of server initiated SYN-STREAMS, tracked for telemetry
  PRUint32             mServerPushedResources;

  // This is a output queue of bytes ready to be written to the SSL stream.
  // When that streams returns WOULD_BLOCK on direct write the bytes get
  // coalesced together here. This results in larger writes to the SSL layer.
  // The buffer is not dynamically grown to accomodate stream writes, but
  // does expand to accept infallible session wide frames like GoAway and RST.
  PRUint32             mOutputQueueSize;
  PRUint32             mOutputQueueUsed;
  PRUint32             mOutputQueueSent;
  nsAutoArrayPtr<char> mOutputQueueBuffer;

  PRIntervalTime       mPingThreshold;
  PRIntervalTime       mLastReadEpoch;     // used for ping timeouts
  PRIntervalTime       mLastDataReadEpoch; // used for IdleTime()
  PRIntervalTime       mPingSentEpoch;
  PRUint32             mNextPingID;
  bool                 mPingThresholdExperiment;
};

}} // namespace mozilla::net

#endif // mozilla_net_SpdySession2_h
