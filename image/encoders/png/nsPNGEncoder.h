/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Attributes.h"
#include "mozilla/ReentrantMonitor.h"

#include "imgIEncoder.h"

#include "nsCOMPtr.h"

#include <png.h>

#define NS_PNGENCODER_CID \
{ /* 38d1592e-b81e-432b-86f8-471878bbfe07 */         \
     0x38d1592e,                                     \
     0xb81e,                                         \
     0x432b,                                         \
    {0x86, 0xf8, 0x47, 0x18, 0x78, 0xbb, 0xfe, 0x07} \
}

// Provides PNG encoding functionality. Use InitFromData() to do the
// encoding. See that function definition for encoding options.

class nsPNGEncoder MOZ_FINAL : public imgIEncoder
{
  typedef mozilla::ReentrantMonitor ReentrantMonitor;
public:
  NS_DECL_ISUPPORTS
  NS_DECL_IMGIENCODER
  NS_DECL_NSIINPUTSTREAM
  NS_DECL_NSIASYNCINPUTSTREAM

  nsPNGEncoder();
  ~nsPNGEncoder();

protected:
  nsresult ParseOptions(const nsAString& aOptions,
                        bool* useTransparency,
                        bool* skipFirstFrame,
                        PRUint32* numAnimatedFrames,
                        PRUint32* numIterations,
                        PRUint32* frameDispose,
                        PRUint32* frameBlend,
                        PRUint32* frameDelay,
                        PRUint32* offsetX,
                        PRUint32* offsetY);
  void ConvertHostARGBRow(const PRUint8* aSrc, PRUint8* aDest,
                          PRUint32 aPixelWidth, bool aUseTransparency);
  void StripAlpha(const PRUint8* aSrc, PRUint8* aDest,
                  PRUint32 aPixelWidth);
  static void WarningCallback(png_structp png_ptr, png_const_charp warning_msg);
  static void ErrorCallback(png_structp png_ptr, png_const_charp error_msg);
  static void WriteCallback(png_structp png, png_bytep data, png_size_t size);
  void NotifyListener();

  png_struct* mPNG;
  png_info* mPNGinfo;

  bool mIsAnimation;
  bool mFinished;

  // image buffer
  PRUint8* mImageBuffer;
  PRUint32 mImageBufferSize;
  PRUint32 mImageBufferUsed;

  PRUint32 mImageBufferReadPoint;

  nsCOMPtr<nsIInputStreamCallback> mCallback;
  nsCOMPtr<nsIEventTarget> mCallbackTarget;
  PRUint32 mNotifyThreshold;

  /*
    nsPNGEncoder is designed to allow one thread to pump data into it while another
    reads from it.  We lock to ensure that the buffer remains append-only while
    we read from it (that it is not realloced) and to ensure that only one thread
    dispatches a callback for each call to AsyncWait.
   */
  ReentrantMonitor mReentrantMonitor;
};
