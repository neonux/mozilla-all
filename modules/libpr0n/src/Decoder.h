/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010.
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bobby Holley <bobbyholley@gmail.com>
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

#ifndef MOZILLA_IMAGELIB_DECODER_H_
#define MOZILLA_IMAGELIB_DECODER_H_

#include "RasterImage.h"

#include "imgIDecoderObserver.h"
#include "imgIDecoder.h"

namespace mozilla {
namespace imagelib {

// XXX - We temporarily inherit imgIDecoder
class Decoder : public imgIDecoder
{
public:

  // XXX - We should get rid of these when westop inheriting imgIDecoder
  NS_DECL_ISUPPORTS
  NS_DECL_IMGIDECODER

  Decoder();
  virtual ~Decoder();

  /**
   * XXX - These methods will stop returning nsresults in a later patch.
   */

  /**
   * Initialize an image decoder.
   *
   * @param aContainer The image container to decode to.
   * @param aObserver The observer for decode notification events.
   *
   * Notifications Sent: TODO
   */
  nsresult Init(RasterImage* aImage, imgIDecoderObserver* aObserver);

  /**
   * Writes data to the decoder.
   *
   * @param aBuffer buffer containing the data to be written
   * @param aCount the number of bytes to write
   *
   * Any errors are reported by setting the appropriate state on the decoder.
   *
   * Notifications Sent: TODO
   */
  // This is commented out while we inherit imgIDecoder because the signature
  // is the same as the one in the idl.
  // nsresult Write(const char* aBuffer, PRUint32 aCount);

  /**
   * Informs the decoder that all the data has been written.
   *
   * Notifications Sent: TODO
   */
  nsresult Finish();

  /**
   * Shuts down the decoder.
   *
   * Notifications Sent: None
   *
   * XXX - These flags will go away later in the patch stack
   */
  nsresult Shutdown(PRUint32 aFlags);

  // We're not COM-y, so we don't get refcounts by default
  // XXX - This is uncommented in a later patch when we stop inheriting imgIDecoder
  // NS_INLINE_DECL_REFCOUNTING(Decoder)

  /*
   * State.
   */

  // If we're doing a "size decode", we more or less pass through the image
  // data, stopping only to scoop out the image dimensions. A size decode
  // must be enabled by SetSizeDecode() _before_calling Init().
  bool IsSizeDecode() { return mSizeDecode; };
  void SetSizeDecode(bool aSizeDecode)
  {
    NS_ABORT_IF_FALSE(!mInitialized, "Can't set size decode after Init()!");
    mSizeDecode = aSizeDecode;
  }

protected:

  /*
   * Internal hooks. Decoder implementations may override these and
   * only these methods.
   */
  virtual nsresult InitInternal();
  virtual nsresult WriteInternal(const char* aBuffer, PRUint32 aCount);
  virtual nsresult FinishInternal();
  virtual nsresult ShutdownInternal(PRUint32 aFlags);

  /*
   * Progress notifications.
   */

  // Called by decoders when they determine the size of the image. Informs
  // the image of its size and sends notifications.
  void PostSize(PRInt32 aWidth, PRInt32 aHeight);


  /*
   * Member variables.
   *
   * XXX - Some of these become private later in the patch stack.
   */
  nsRefPtr<RasterImage> mImage;
  nsCOMPtr<imgIDecoderObserver> mObserver;

  bool mInitialized;
  bool mSizeDecode;
};

} // namespace imagelib
} // namespace mozilla

#endif // MOZILLA_IMAGELIB_DECODER_H_
