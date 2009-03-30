/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
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
#include "nsDebug.h"
#include "nsMediaStream.h"
#include "nsMediaDecoder.h"
#include "nsNetUtil.h"
#include "nsAutoLock.h"
#include "nsThreadUtils.h"
#include "nsIFile.h"
#include "nsIFileChannel.h"
#include "nsIHttpChannel.h"
#include "nsISeekableStream.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIRequestObserver.h"
#include "nsIStreamListener.h"
#include "nsIScriptSecurityManager.h"
#include "nsChannelToPipeListener.h"
#include "nsCrossSiteListenerProxy.h"
#include "nsHTMLMediaElement.h"
#include "nsIDocument.h"

class nsDefaultStreamStrategy : public nsMediaStream
{
public:
  nsDefaultStreamStrategy(nsMediaDecoder* aDecoder, nsIChannel* aChannel, nsIURI* aURI) :
    nsMediaStream(aDecoder, aChannel, aURI),
    mPosition(0)
  {
  }
  
  virtual nsresult Open(nsIStreamListener** aStreamListener);
  virtual nsresult Close();
  virtual nsresult Read(char* aBuffer, PRUint32 aCount, PRUint32* aBytes);
  virtual nsresult Seek(PRInt32 aWhence, PRInt64 aOffset);
  virtual PRInt64  Tell();
  virtual void     Cancel();
  virtual nsIPrincipal* GetCurrentPrincipal();
  virtual void     Suspend();
  virtual void     Resume();

private:
  // Listener attached to channel to constantly download the
  // media data asynchronously and store it in the pipe. The 
  // data is obtainable via the mPipeInput member. Use on 
  // main thread only.
  nsCOMPtr<nsChannelToPipeListener> mListener;

  // Input stream for the media data currently downloaded 
  // and stored in the pipe. This can be used from any thread.
  nsCOMPtr<nsIInputStream>  mPipeInput;

  // Current seek position. Need to compute this manually because
  // the underlying channel may not offer this information.
  PRInt64 mPosition;
};

nsresult nsDefaultStreamStrategy::Open(nsIStreamListener** aStreamListener)
{
  if (aStreamListener) {
    *aStreamListener = nsnull;
  }

  mListener = new nsChannelToPipeListener(mDecoder);
  NS_ENSURE_TRUE(mListener, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv = mListener->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIStreamListener> listener = do_QueryInterface(mListener);

  if (aStreamListener) {
    *aStreamListener = mListener;
    NS_ADDREF(mListener);
  } else {
    // Ensure that if we're loading cross domain, that the server is sending
    // an authorizing Access-Control header.
    nsHTMLMediaElement* element = mDecoder->GetMediaElement();
    NS_ENSURE_TRUE(element, NS_ERROR_FAILURE);
    if (element->ShouldCheckAllowOrigin()) {
      listener = new nsCrossSiteListenerProxy(mListener,
                                              element->NodePrincipal(),
                                              mChannel, 
                                              PR_FALSE,
                                              &rv);
      NS_ENSURE_TRUE(listener, NS_ERROR_OUT_OF_MEMORY);
      NS_ENSURE_SUCCESS(rv, rv);
    } else {
      // Ensure that we never load a local file from some page on a 
      // web server.
      rv = nsContentUtils::GetSecurityManager()->
             CheckLoadURIWithPrincipal(element->NodePrincipal(),
                                       mURI,
                                       nsIScriptSecurityManager::STANDARD);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    rv = mChannel->AsyncOpen(listener, nsnull);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = mListener->GetInputStream(getter_AddRefs(mPipeInput));
  NS_ENSURE_SUCCESS(rv, rv);

  mPosition = 0;

  return NS_OK;
}

nsresult nsDefaultStreamStrategy::Close()
{
  nsAutoLock lock(mLock);
  if (mChannel) {
    mChannel->Cancel(NS_BINDING_ABORTED);
    mChannel = nsnull;
  }
  if (mPipeInput) {
    mPipeInput->Close();
    mPipeInput = nsnull;
  }
  mListener = nsnull;
  return NS_OK;
}

nsresult nsDefaultStreamStrategy::Read(char* aBuffer, PRUint32 aCount, PRUint32* aBytes)
{
  // The read request pulls from the pipe, not the channels input
  // stream. This allows calling from any thread as the pipe is
  // threadsafe.
  nsAutoLock lock(mLock);
  if (!mPipeInput)
    return NS_ERROR_FAILURE;

  nsresult rv = mPipeInput->Read(aBuffer, aCount, aBytes);
  NS_ENSURE_SUCCESS(rv, rv);
  mPosition += *aBytes;

  return NS_OK;
}

nsresult nsDefaultStreamStrategy::Seek(PRInt32 aWhence, PRInt64 aOffset) 
{
  // Default streams cannot be seeked
  return NS_ERROR_FAILURE;
}

PRInt64 nsDefaultStreamStrategy::Tell()
{
  return mPosition;
}

void nsDefaultStreamStrategy::Cancel()
{
  if (mListener)
    mListener->Cancel();
}

nsIPrincipal* nsDefaultStreamStrategy::GetCurrentPrincipal()
{
  if (!mListener)
    return nsnull;

  return mListener->GetCurrentPrincipal();
}

void nsDefaultStreamStrategy::Suspend()
{
  mChannel->Suspend();
}

void nsDefaultStreamStrategy::Resume()
{
  mChannel->Resume();
}

class nsFileStreamStrategy : public nsMediaStream
{
public:
  nsFileStreamStrategy(nsMediaDecoder* aDecoder, nsIChannel* aChannel, nsIURI* aURI) :
    nsMediaStream(aDecoder, aChannel, aURI)
  {
  }
  
  virtual nsresult Open(nsIStreamListener** aStreamListener);
  virtual nsresult Close();
  virtual nsresult Read(char* aBuffer, PRUint32 aCount, PRUint32* aBytes);
  virtual nsresult Seek(PRInt32 aWhence, PRInt64 aOffset);
  virtual PRInt64  Tell();
  virtual nsIPrincipal* GetCurrentPrincipal();
  virtual void     Suspend();
  virtual void     Resume();

private:
  // Seekable stream interface to file. This can be used from any
  // thread.
  nsCOMPtr<nsISeekableStream> mSeekable;

  // Input stream for the media data. This can be used from any
  // thread.
  nsCOMPtr<nsIInputStream>  mInput;

  // Security Principal
  nsCOMPtr<nsIPrincipal> mPrincipal;
};

class LoadedEvent : public nsRunnable 
{
public:
  LoadedEvent(nsMediaDecoder* aDecoder, PRInt64 aOffset, PRInt64 aSize) :
    mOffset(aOffset), mSize(aSize), mDecoder(aDecoder)
  {
    MOZ_COUNT_CTOR(LoadedEvent);
  }
  ~LoadedEvent()
  {
    MOZ_COUNT_DTOR(LoadedEvent);
  }

  NS_IMETHOD Run() {
    if (mOffset >= 0) {
      mDecoder->NotifyDownloadSeeked(mOffset);
    }
    if (mSize > 0) {
      mDecoder->NotifyBytesDownloaded(mSize);
    }
    mDecoder->NotifyDownloadEnded(NS_OK);
    return NS_OK;
  }

private:
  PRInt64                  mOffset;
  PRInt64                  mSize;
  nsRefPtr<nsMediaDecoder> mDecoder;
};

nsresult nsFileStreamStrategy::Open(nsIStreamListener** aStreamListener)
{
  if (aStreamListener) {
    *aStreamListener = nsnull;
  }

  nsresult rv;
  if (aStreamListener) {
    // The channel is already open. We need a synchronous stream that
    // implements nsISeekableStream, so we have to find the underlying
    // file and reopen it
    nsCOMPtr<nsIFileChannel> fc(do_QueryInterface(mChannel));
    if (!fc)
      return NS_ERROR_UNEXPECTED;

    nsCOMPtr<nsIFile> file; 
    rv = fc->GetFile(getter_AddRefs(file));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = NS_NewLocalFileInputStream(getter_AddRefs(mInput), file);
  } else {
    // Ensure that we never load a local file from some page on a 
    // web server.
    nsHTMLMediaElement* element = mDecoder->GetMediaElement();
    NS_ENSURE_TRUE(element, NS_ERROR_FAILURE);

    rv = nsContentUtils::GetSecurityManager()->
           CheckLoadURIWithPrincipal(element->NodePrincipal(),
                                     mURI,
                                     nsIScriptSecurityManager::STANDARD);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mChannel->Open(getter_AddRefs(mInput));
  }
  NS_ENSURE_SUCCESS(rv, rv);

  mSeekable = do_QueryInterface(mInput);
  if (!mSeekable) {
    // XXX The file may just be a .url or similar
    // shortcut that points to a Web site. We need to fix this by
    // doing an async open and waiting until we locate the real resource,
    // then using that (if it's still a file!).
    return NS_ERROR_FAILURE;
  }

  /* Get our principal */
  nsCOMPtr<nsIScriptSecurityManager> secMan =
    do_GetService("@mozilla.org/scriptsecuritymanager;1");
  if (secMan) {
    rv = secMan->GetChannelPrincipal(mChannel,
                                     getter_AddRefs(mPrincipal));
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  // Get the file size and inform the decoder. Only files up to 4GB are
  // supported here.
  PRUint32 size;
  rv = mInput->Available(&size);
  if (NS_SUCCEEDED(rv)) {
    mDecoder->SetTotalBytes(size);
  }

  // This must happen before we return from this function, we can't
  // defer it to the LoadedEvent because that would allow reads from
  // the stream to complete before this notification is sent.
  mDecoder->NotifyBytesDownloaded(size);

  nsCOMPtr<nsIRunnable> event = new LoadedEvent(mDecoder, -1, 0);
  NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
  return NS_OK;
}

nsresult nsFileStreamStrategy::Close()
{
  nsAutoLock lock(mLock);
  if (mChannel) {
    mChannel->Cancel(NS_BINDING_ABORTED);
    mChannel = nsnull;
    mInput = nsnull;
    mSeekable = nsnull;
  }

  return NS_OK;
}

nsresult nsFileStreamStrategy::Read(char* aBuffer, PRUint32 aCount, PRUint32* aBytes)
{
  nsAutoLock lock(mLock);
  if (!mInput)
    return NS_ERROR_FAILURE;
  return mInput->Read(aBuffer, aCount, aBytes);
}

nsresult nsFileStreamStrategy::Seek(PRInt32 aWhence, PRInt64 aOffset) 
{  
  PRUint32 size = 0;
  PRInt64 absoluteOffset = 0;
  nsresult rv;
  {
    nsAutoLock lock(mLock);
    if (!mSeekable)
      return NS_ERROR_FAILURE;
    rv = mSeekable->Seek(aWhence, aOffset);
    if (NS_SUCCEEDED(rv)) {
      mSeekable->Tell(&absoluteOffset);
    }
    mInput->Available(&size);
  }

  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIRunnable> event = new LoadedEvent(mDecoder, absoluteOffset, size);
    // Synchronous dispatch to ensure the decoder is notified before our caller
    // proceeds and reads occur.
    NS_DispatchToMainThread(event, NS_DISPATCH_SYNC);
  }

  return rv;
}

PRInt64 nsFileStreamStrategy::Tell()
{
  nsAutoLock lock(mLock);
  if (!mSeekable)
    return 0;

  PRInt64 offset = 0;
  mSeekable->Tell(&offset);
  return offset;
}

nsIPrincipal* nsFileStreamStrategy::GetCurrentPrincipal()
{
  return mPrincipal;
}

void nsFileStreamStrategy::Suspend()
{
  mChannel->Suspend();
}

void nsFileStreamStrategy::Resume()
{
  mChannel->Resume();
}

class nsHttpStreamStrategy : public nsMediaStream
{
public:
  nsHttpStreamStrategy(nsMediaDecoder* aDecoder, nsIChannel* aChannel, nsIURI* aURI) :
    nsMediaStream(aDecoder, aChannel, aURI),
    mPosition(0),
    mAtEOF(PR_FALSE),
    mCancelled(PR_FALSE)
  {
  }
  
  virtual nsresult Open(nsIStreamListener** aListener);
  virtual nsresult Close();
  virtual nsresult Read(char* aBuffer, PRUint32 aCount, PRUint32* aBytes);
  virtual nsresult Seek(PRInt32 aWhence, PRInt64 aOffset);
  virtual PRInt64  Tell();
  virtual void     Cancel();
  virtual nsIPrincipal* GetCurrentPrincipal();
  virtual void     Suspend();
  virtual void     Resume();

  // Return PR_TRUE if the stream has been cancelled.
  PRBool IsCancelled() const;

  // This must be called on the main thread only, and at a time when the
  // strategy is not reading from the current channel/stream. It's primary
  // purpose is to be called from a Seek to reset to the new byte range
  // request HTTP channel.
  nsresult OpenInternal(nsIChannel* aChannel, PRInt64 aOffset);

  // Opens the HTTP channel, using a byte range request to start at aOffset.
  nsresult OpenInternal(nsIStreamListener **aStreamListener, PRInt64 aOffset);

private:
  // Listener attached to channel to constantly download the
  // media data asynchronously and store it in the pipe. The 
  // data is obtainable via the mPipeInput member. Use on 
  // main thread only.
  nsCOMPtr<nsChannelToPipeListener> mListener;

  // Input stream for the media data currently downloaded 
  // and stored in the pipe. This can be used from any thread.
  nsCOMPtr<nsIInputStream>  mPipeInput;

  // Current seek position. Need to compute this manually due to
  // seeking with byte range requests meaning the position in the pipe
  // is not valid. This is initially set on the main thread during the
  // Open call. After that it is read and written by a single thread
  // only (the thread that calls the read/seek operations).
  PRInt64 mPosition;

  // PR_TRUE if we are positioned at the end of the file.
  // This is written and read from a single thread only (the thread that
  // calls the read/seek operations).
  PRPackedBool mAtEOF;

  // PR_TRUE if the media stream requested this strategy is cancelled.
  // This is read and written on the main thread only.
  PRPackedBool mCancelled;
};

nsresult nsHttpStreamStrategy::Open(nsIStreamListener **aStreamListener)
{
  return OpenInternal(aStreamListener, 0);
}

nsresult nsHttpStreamStrategy::OpenInternal(nsIChannel* aChannel,
                                            PRInt64 aOffset)
{
  nsAutoLock lock(mLock);
  mChannel = aChannel;
  return OpenInternal(static_cast<nsIStreamListener**>(nsnull), aOffset);
}

nsresult nsHttpStreamStrategy::OpenInternal(nsIStreamListener **aStreamListener,
                                            PRInt64 aOffset)
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");
  NS_ENSURE_TRUE(mChannel, NS_ERROR_NULL_POINTER);

  if (aStreamListener) {
    *aStreamListener = nsnull;
  }

  mListener = new nsChannelToPipeListener(mDecoder, aOffset != 0);
  NS_ENSURE_TRUE(mListener, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv = mListener->Init();
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsCOMPtr<nsIStreamListener> listener = do_QueryInterface(mListener);

  if (aStreamListener) {
    *aStreamListener = mListener;
    NS_ADDREF(*aStreamListener);
  } else {
    // Ensure that if we're loading cross domain, that the server is sending
    // an authorizing Access-Control header.
    nsHTMLMediaElement* element = mDecoder->GetMediaElement();
    NS_ENSURE_TRUE(element, NS_ERROR_FAILURE);
    if (element->ShouldCheckAllowOrigin()) {
      listener = new nsCrossSiteListenerProxy(mListener,
                                              element->NodePrincipal(),
                                              mChannel, 
                                              PR_FALSE,
                                              &rv);
      NS_ENSURE_TRUE(listener, NS_ERROR_OUT_OF_MEMORY);
      NS_ENSURE_SUCCESS(rv, rv);
    } else {
      rv = nsContentUtils::GetSecurityManager()->
             CheckLoadURIWithPrincipal(element->NodePrincipal(),
                                       mURI,
                                       nsIScriptSecurityManager::STANDARD);
      NS_ENSURE_SUCCESS(rv, rv);

    }
    // Use a byte range request from the start of the resource.
    // This enables us to detect if the stream supports byte range
    // requests, and therefore seeking, early.
    nsCOMPtr<nsIHttpChannel> hc = do_QueryInterface(mChannel);
    if (hc) {
      nsCAutoString rangeString("bytes=");
      rangeString.AppendInt(aOffset);
      rangeString.Append("-");
      hc->SetRequestHeader(NS_LITERAL_CSTRING("Range"), rangeString, PR_FALSE);
    }
 
    rv = mChannel->AsyncOpen(listener, nsnull);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  
  rv = mListener->GetInputStream(getter_AddRefs(mPipeInput));
  NS_ENSURE_SUCCESS(rv, rv);

  mDecoder->NotifyDownloadSeeked(aOffset);

  return NS_OK;
}

nsresult nsHttpStreamStrategy::Close()
{
  NS_ASSERTION(NS_IsMainThread(), "Only call on main thread");
  nsAutoLock lock(mLock);
  if (mChannel) {
    mChannel->Cancel(NS_BINDING_ABORTED);
    mChannel = nsnull;
  }
  if (mPipeInput) {
    mPipeInput->Close();
    mPipeInput = nsnull;
  }
  mListener = nsnull;
  return NS_OK;
}

nsresult nsHttpStreamStrategy::Read(char* aBuffer, PRUint32 aCount, PRUint32* aBytes)
{
  // The read request pulls from the pipe, not the channels input
  // stream. This allows calling from any thread as the pipe is
  // threadsafe.
  nsAutoLock lock(mLock);
  if (!mPipeInput)
    return NS_ERROR_FAILURE;

  // If Cancel() is called then the read will fail with an error so we
  // can bail out of the blocking call.
  nsresult rv = mPipeInput->Read(aBuffer, aCount, aBytes);
  NS_ENSURE_SUCCESS(rv, rv);
  mPosition += *aBytes;

  return rv;
}

class nsByteRangeEvent : public nsRunnable 
{
public:
  nsByteRangeEvent(nsHttpStreamStrategy* aStrategy, 
                   nsIURI* aURI, 
                   PRInt64 aOffset) :
    mStrategy(aStrategy),
    mURI(aURI),
    mOffset(aOffset),
    mResult(NS_OK)
  {
    MOZ_COUNT_CTOR(nsByteRangeEvent);
  }

  ~nsByteRangeEvent()
  {
    MOZ_COUNT_DTOR(nsByteRangeEvent);
  }

  nsresult GetResult()
  {
    return mResult;
  }

  NS_IMETHOD Run() {
    // This event runs in the main thread. The same
    // thread as that which can block waiting for the
    // decode event to complete when the stream
    // is cancelled. Check to see if we are cancelled
    // in case this event is processed after the cancel flag
    // which would otherwise cause the listener to be recreated.
    if (mStrategy->IsCancelled()) {
      mResult = NS_ERROR_FAILURE;
      return NS_OK;
    }

    nsCOMPtr<nsIChannel> channel;
    mStrategy->Close();
    mResult = NS_NewChannel(getter_AddRefs(channel),
                            mURI,
                            nsnull,
                            nsnull,
                            nsnull,
                            nsIRequest::LOAD_NORMAL);
    NS_ENSURE_SUCCESS(mResult, mResult);
    mResult = mStrategy->OpenInternal(channel, mOffset);
    return NS_OK;
  }

private:
  nsHttpStreamStrategy* mStrategy;
  nsMediaDecoder* mDecoder;
  nsIURI* mURI;
  PRInt64 mOffset;
  nsresult mResult;
};

nsresult nsHttpStreamStrategy::Seek(PRInt32 aWhence, PRInt64 aOffset)
{
  PRInt64 totalBytes = mDecoder->GetStatistics().mTotalBytes;
  {
    nsAutoLock lock(mLock);
    if (!mChannel || !mPipeInput) 
      return NS_ERROR_FAILURE;

    // When seeking liboggz will first seek to the end of the file to
    // obtain the length of the file. It immediately does a 'tell' to
    // get the position and reseeks somewhere else. This traps the seek
    // to end of file and sets mAtEOF. Tell() looks for this flag being
    // set and returns the content length.
    if(aWhence == nsISeekableStream::NS_SEEK_END && aOffset == 0) {
      if (totalBytes == -1)
        return NS_ERROR_FAILURE;
      
      mAtEOF = PR_TRUE;
      return NS_OK;
    }
    else {
      mAtEOF = PR_FALSE;
    }

    // Handle cases of aWhence not being NS_SEEK_SET by converting to
    // NS_SEEK_SET
    switch (aWhence) {
    case nsISeekableStream::NS_SEEK_END: {
      if (totalBytes == -1)
        return NS_ERROR_FAILURE;
      
      aOffset += totalBytes; 
      aWhence = nsISeekableStream::NS_SEEK_SET;
      break;
    }
    case nsISeekableStream::NS_SEEK_CUR: {
      aOffset += mPosition;
      aWhence = nsISeekableStream::NS_SEEK_SET;
      break;
    }
    default:
      // Do nothing, we are NS_SEEK_SET 
      break;
    };
    
    // If we are already at the correct position, do nothing
    if (aOffset == mPosition) {
      return NS_OK;
    }

    // If we are seeking to a byterange that we already have buffered in
    // the listener then move to that and avoid the need to send a byte
    // range request.
    PRInt32 bytesAhead = aOffset - mPosition;
    PRUint32 available = 0;
    nsresult rv = mPipeInput->Available(&available);
    PRInt32 diff = available - PRUint32(bytesAhead);

    // If we have enough buffered data to satisfy the seek request
    // just do a read and discard the data.
    // If the seek request is for a small amount ahead (defined by
    // SEEK_VS_READ_THRESHOLD) then also perform a read and discard
    // instead of doing a byte range request. This improves the speed
    // of the seeks quite a bit.
    if (NS_SUCCEEDED(rv) && bytesAhead > 0 && diff > -SEEK_VS_READ_THRESHOLD) {
      nsAutoArrayPtr<char> data(new char[bytesAhead]);
      if (!data)
        return NS_ERROR_OUT_OF_MEMORY;
      // Read until the read cursor reaches new seek point. If Cancel() is
      // called then the read will fail with an error so we can bail out of
      // the blocking call.
      PRInt32 bytesRead = 0;
      PRUint32 bytes = 0;
      do {
        nsresult rv = mPipeInput->Read(data.get(),
                                       (bytesAhead-bytesRead),
                                       &bytes);
        NS_ENSURE_SUCCESS(rv, rv);
        NS_ENSURE_TRUE(bytes != 0, NS_ERROR_FAILURE); // Tried to read past EOF.
        mPosition += bytes;
        bytesRead += bytes;
      } while (bytesRead != bytesAhead);

      // We don't need to notify the decoder here that we seeked, just that
      // we read ahead. In fact, we mustn't tell the decoder that we seeked,
      // since the seek notification might race with the "data downloaded"
      // notification after the data was written into the pipe, so that the
      // seek notification happens *first*, hopelessly confusing the
      // decoder.
      mDecoder->NotifyBytesConsumed(bytesRead);
      return rv;
    }
  }

  // Don't acquire mLock in this scope as we do a synchronous call to the main thread
  // which would deadlock if that thread is calling Close().
  nsCOMPtr<nsByteRangeEvent> event = new nsByteRangeEvent(this, mURI, aOffset);
  NS_DispatchToMainThread(event, NS_DISPATCH_SYNC);

  // If the sync request fails, or a call to Cancel() is made during the request,
  // don't update the position and return the error.
  nsresult rv = event->GetResult();
  if (NS_SUCCEEDED(rv)) {
    mPosition = aOffset;
  }

  return rv;
}

PRInt64 nsHttpStreamStrategy::Tell()
{
  // Handle the case of a seek to EOF by liboggz
  // (See Seek for details)
  return mAtEOF ? mDecoder->GetStatistics().mTotalBytes : mPosition;
}

void nsHttpStreamStrategy::Cancel()
{
  mCancelled = PR_TRUE;
  if (mListener)
    mListener->Cancel();
}

PRBool nsHttpStreamStrategy::IsCancelled() const
{
  return mCancelled;
}

nsIPrincipal* nsHttpStreamStrategy::GetCurrentPrincipal()
{
  if (!mListener)
    return nsnull;

  return mListener->GetCurrentPrincipal();
}

void nsHttpStreamStrategy::Suspend()
{
  mChannel->Suspend();
}

void nsHttpStreamStrategy::Resume()
{
  mChannel->Resume();
}

nsresult
nsMediaStream::Open(nsMediaDecoder* aDecoder, nsIURI* aURI,
                    nsIChannel* aChannel, nsMediaStream** aStream,
                    nsIStreamListener** aListener)
{
  NS_ASSERTION(NS_IsMainThread(), 
	       "nsMediaStream::Open called on non-main thread");

  nsCOMPtr<nsIChannel> channel;
  if (aChannel) {
    channel = aChannel;
  } else {
    nsresult rv = NS_NewChannel(getter_AddRefs(channel), 
                                aURI, 
                                nsnull,
                                nsnull,
                                nsnull,
                                nsIRequest::LOAD_NORMAL);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsMediaStream* stream;
  nsCOMPtr<nsIHttpChannel> hc = do_QueryInterface(channel);
  if (hc) {
    stream = new nsHttpStreamStrategy(aDecoder, channel, aURI);
  } else {
    nsCOMPtr<nsIFileChannel> fc = do_QueryInterface(channel);
    if (fc) {
      stream = new nsFileStreamStrategy(aDecoder, channel, aURI);
    } else {
      stream = new nsDefaultStreamStrategy(aDecoder, channel, aURI);
    }
  }
  if (!stream)
    return NS_ERROR_OUT_OF_MEMORY;

  *aStream = stream;
  return stream->Open(aListener);
}
