/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/******************************************************************

 iLBC Speech Coder ANSI-C Source Code

 WebRtcIlbcfix_FilteredCbVecs.h

******************************************************************/

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_FILTERED_CB_VECS_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_FILTERED_CB_VECS_H_

#include "defines.h"

/*----------------------------------------------------------------*
 *  Construct an additional codebook vector by filtering the
 *  initial codebook buffer. This vector is then used to expand
 *  the codebook with an additional section.
 *---------------------------------------------------------------*/

void WebRtcIlbcfix_FilteredCbVecs(
    WebRtc_Word16 *cbvectors, /* (o) Codebook vector for the higher section */
    WebRtc_Word16 *CBmem,  /* (i) Codebook memory that is filtered to create a
                                           second CB section */
    int lMem,  /* (i) Length of codebook memory */
    WebRtc_Word16 samples    /* (i) Number of samples to filter */
                                  );

#endif
