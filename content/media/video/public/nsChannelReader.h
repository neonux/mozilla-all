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
#if !defined(nsChannelReader_h___)
#define nsChannelReader_h___

#include "nsCOMPtr.h"
#include "nsIChannel.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIRequestObserver.h"
#include "nsIStreamListener.h"
#include "nsIPrincipal.h"

#include "oggplay/oggplay.h"

class nsOggDecoder;

/* 
   Reads all data on the input stream of a channel and
   writes it to a pipe. This allows a seperate thread to
   read data from a channel running on the main thread
*/
class nsChannelToPipeListener : public nsIStreamListener
{
  // ISupports
  NS_DECL_ISUPPORTS

  // IRequestObserver
  NS_DECL_NSIREQUESTOBSERVER

  // IStreamListener
  NS_DECL_NSISTREAMLISTENER

  public:
  nsChannelToPipeListener(nsOggDecoder* aDecoder);
  nsresult Init();
  void GetInputStream(nsIInputStream** aStream);
  void Stop();
  double BytesPerSecond() const;

  nsIPrincipal* GetCurrentPrincipal();

private:
  nsCOMPtr<nsIInputStream> mInput;
  nsCOMPtr<nsIOutputStream> mOutput;
  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsOggDecoder* mDecoder;

  // Interval when download started. Used in
  // computing bytes per second download rate.
  PRIntervalTime mIntervalStart;

  // Interval when last downloaded bytes occurred. Used in computer
  // bytes per second download rate.
  PRIntervalTime mIntervalEnd;

  // Total bytes transferred so far
  PRInt64 mTotalBytes;
};

class nsChannelReader : public OggPlayReader
{
public:
  nsChannelReader();
  nsresult Init(nsOggDecoder* aDecoder, nsIURI* aURI);
  PRUint32 Available();
  OggPlayErrorCode initialise(int aBlock);
  OggPlayErrorCode destroy();
  size_t io_read(char* aBuffer, size_t aCount);
  int io_seek(long aOffset, int aWhence);
  long io_tell();  

  // Return average number of bytes per second that the 
  // download of the media resource is achieving.
  double BytesPerSecond() const;

  // return the principal that we saved in GetRequest
  nsIPrincipal* GetCurrentPrincipal();

public:
  nsCOMPtr<nsIChannel>  mChannel;
  nsCOMPtr<nsIInputStream>  mInput;
  nsCOMPtr<nsChannelToPipeListener> mListener;
  unsigned long mCurrentPosition;
};

#endif
