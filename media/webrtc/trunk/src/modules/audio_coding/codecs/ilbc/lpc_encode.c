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

 WebRtcIlbcfix_LpcEncode.c

******************************************************************/

#include "defines.h"
#include "simple_lpc_analysis.h"
#include "simple_interpolate_lsf.h"
#include "simple_lsf_quant.h"
#include "lsf_check.h"
#include "constants.h"

/*----------------------------------------------------------------*
 *  lpc encoder
 *---------------------------------------------------------------*/

void WebRtcIlbcfix_LpcEncode(
    WebRtc_Word16 *syntdenum,  /* (i/o) synthesis filter coefficients
                                           before/after encoding */
    WebRtc_Word16 *weightdenum, /* (i/o) weighting denumerator coefficients
                                   before/after encoding */
    WebRtc_Word16 *lsf_index,  /* (o) lsf quantization index */
    WebRtc_Word16 *data,   /* (i) Speech to do LPC analysis on */
    iLBC_Enc_Inst_t *iLBCenc_inst
    /* (i/o) the encoder state structure */
                              ) {
  /* Stack based */
  WebRtc_Word16 lsf[LPC_FILTERORDER * LPC_N_MAX];
  WebRtc_Word16 lsfdeq[LPC_FILTERORDER * LPC_N_MAX];

  /* Calculate LSF's from the input speech */
  WebRtcIlbcfix_SimpleLpcAnalysis(lsf, data, iLBCenc_inst);

  /* Quantize the LSF's */
  WebRtcIlbcfix_SimpleLsfQ(lsfdeq, lsf_index, lsf, iLBCenc_inst->lpc_n);

  /* Stableize the LSF's if needed */
  WebRtcIlbcfix_LsfCheck(lsfdeq, LPC_FILTERORDER, iLBCenc_inst->lpc_n);

  /* Calculate the synthesis and weighting filter coefficients from
     the optimal LSF and the dequantized LSF */
  WebRtcIlbcfix_SimpleInterpolateLsf(syntdenum, weightdenum,
                                     lsf, lsfdeq, iLBCenc_inst->lsfold,
                                     iLBCenc_inst->lsfdeqold, LPC_FILTERORDER, iLBCenc_inst);

  return;
}
