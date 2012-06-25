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

 WebRtcIlbcfix_HpOutput.h

******************************************************************/

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_HP_OUTPUT_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_HP_OUTPUT_H_

#include "defines.h"

void WebRtcIlbcfix_HpOutput(
    WebRtc_Word16 *signal,     /* (i/o) signal vector */
    WebRtc_Word16 *ba,      /* (i)   B- and A-coefficients (2:nd order)
                               {b[0] b[1] b[2] -a[1] -a[2]} a[0]
                               is assumed to be 1.0 */
    WebRtc_Word16 *y,      /* (i/o) Filter state yhi[n-1] ylow[n-1]
                              yhi[n-2] ylow[n-2] */
    WebRtc_Word16 *x,      /* (i/o) Filter state x[n-1] x[n-2] */
    WebRtc_Word16 len);      /* (i)   Number of samples to filter */

#endif
