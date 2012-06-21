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

 define.h

******************************************************************/
#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_DEFINES_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_DEFINES_H_

#include "typedefs.h"
#include "signal_processing_library.h"
#include <string.h>

/* general codec settings */

#define FS       8000
#define BLOCKL_20MS     160
#define BLOCKL_30MS     240
#define BLOCKL_MAX     240
#define NSUB_20MS     4
#define NSUB_30MS     6
#define NSUB_MAX     6
#define NASUB_20MS     2
#define NASUB_30MS     4
#define NASUB_MAX     4
#define SUBL      40
#define STATE_LEN     80
#define STATE_SHORT_LEN_30MS  58
#define STATE_SHORT_LEN_20MS  57

/* LPC settings */

#define LPC_FILTERORDER    10
#define LPC_LOOKBACK    60
#define LPC_N_20MS     1
#define LPC_N_30MS     2
#define LPC_N_MAX     2
#define LPC_ASYMDIFF    20
#define LSF_NSPLIT     3
#define LSF_NUMBER_OF_STEPS   4
#define LPC_HALFORDER    5
#define COS_GRID_POINTS 60

/* cb settings */

#define CB_NSTAGES     3
#define CB_EXPAND     2
#define CB_MEML      147
#define CB_FILTERLEN    (2*4)
#define CB_HALFFILTERLEN   4
#define CB_RESRANGE     34
#define CB_MAXGAIN_FIXQ6   83 /* error = -0.24% */
#define CB_MAXGAIN_FIXQ14   21299

/* enhancer */

#define ENH_BLOCKL     80  /* block length */
#define ENH_BLOCKL_HALF    (ENH_BLOCKL/2)
#define ENH_HL      3  /* 2*ENH_HL+1 is number blocks
                                                                           in said second sequence */
#define ENH_SLOP     2  /* max difference estimated and
                                                                           correct pitch period */
#define ENH_PLOCSL     8  /* pitch-estimates and
                                                                           pitch-locations buffer length */
#define ENH_OVERHANG    2
#define ENH_UPS0     4  /* upsampling rate */
#define ENH_FL0      3  /* 2*FLO+1 is the length of each filter */
#define ENH_FLO_MULT2_PLUS1   7
#define ENH_VECTL     (ENH_BLOCKL+2*ENH_FL0)
#define ENH_CORRDIM     (2*ENH_SLOP+1)
#define ENH_NBLOCKS     (BLOCKL/ENH_BLOCKL)
#define ENH_NBLOCKS_EXTRA   5
#define ENH_NBLOCKS_TOT    8 /* ENH_NBLOCKS+ENH_NBLOCKS_EXTRA */
#define ENH_BUFL     (ENH_NBLOCKS_TOT)*ENH_BLOCKL
#define ENH_BUFL_FILTEROVERHEAD  3
#define ENH_A0      819   /* Q14 */
#define ENH_A0_MINUS_A0A0DIV4  848256041 /* Q34 */
#define ENH_A0DIV2     26843546 /* Q30 */

/* PLC */

/* Down sampling */

#define FILTERORDER_DS_PLUS1  7
#define DELAY_DS     3
#define FACTOR_DS     2

/* bit stream defs */

#define NO_OF_BYTES_20MS   38
#define NO_OF_BYTES_30MS   50
#define NO_OF_WORDS_20MS   19
#define NO_OF_WORDS_30MS   25
#define STATE_BITS     3
#define BYTE_LEN     8
#define ULP_CLASSES     3

/* help parameters */

#define TWO_PI_FIX     25736 /* Q12 */

/* Constants for codebook search and creation */

#define ST_MEM_L_TBL  85
#define MEM_LF_TBL  147


/* Struct for the bits */
typedef struct iLBC_bits_t_ {
  WebRtc_Word16 lsf[LSF_NSPLIT*LPC_N_MAX];
  WebRtc_Word16 cb_index[CB_NSTAGES*(NASUB_MAX+1)];  /* First CB_NSTAGES values contains extra CB index */
  WebRtc_Word16 gain_index[CB_NSTAGES*(NASUB_MAX+1)]; /* First CB_NSTAGES values contains extra CB gain */
  WebRtc_Word16 idxForMax;
  WebRtc_Word16 state_first;
  WebRtc_Word16 idxVec[STATE_SHORT_LEN_30MS];
  WebRtc_Word16 firstbits;
  WebRtc_Word16 startIdx;
} iLBC_bits;

/* type definition encoder instance */
typedef struct iLBC_Enc_Inst_t_ {

  /* flag for frame size mode */
  WebRtc_Word16 mode;

  /* basic parameters for different frame sizes */
  WebRtc_Word16 blockl;
  WebRtc_Word16 nsub;
  WebRtc_Word16 nasub;
  WebRtc_Word16 no_of_bytes, no_of_words;
  WebRtc_Word16 lpc_n;
  WebRtc_Word16 state_short_len;

  /* analysis filter state */
  WebRtc_Word16 anaMem[LPC_FILTERORDER];

  /* Fix-point old lsf parameters for interpolation */
  WebRtc_Word16 lsfold[LPC_FILTERORDER];
  WebRtc_Word16 lsfdeqold[LPC_FILTERORDER];

  /* signal buffer for LP analysis */
  WebRtc_Word16 lpc_buffer[LPC_LOOKBACK + BLOCKL_MAX];

  /* state of input HP filter */
  WebRtc_Word16 hpimemx[2];
  WebRtc_Word16 hpimemy[4];

#ifdef SPLIT_10MS
  WebRtc_Word16 weightdenumbuf[66];
  WebRtc_Word16 past_samples[160];
  WebRtc_UWord16 bytes[25];
  WebRtc_Word16 section;
  WebRtc_Word16 Nfor_flag;
  WebRtc_Word16 Nback_flag;
  WebRtc_Word16 start_pos;
  WebRtc_Word16 diff;
#endif

} iLBC_Enc_Inst_t;

/* type definition decoder instance */
typedef struct iLBC_Dec_Inst_t_ {

  /* flag for frame size mode */
  WebRtc_Word16 mode;

  /* basic parameters for different frame sizes */
  WebRtc_Word16 blockl;
  WebRtc_Word16 nsub;
  WebRtc_Word16 nasub;
  WebRtc_Word16 no_of_bytes, no_of_words;
  WebRtc_Word16 lpc_n;
  WebRtc_Word16 state_short_len;

  /* synthesis filter state */
  WebRtc_Word16 syntMem[LPC_FILTERORDER];

  /* old LSF for interpolation */
  WebRtc_Word16 lsfdeqold[LPC_FILTERORDER];

  /* pitch lag estimated in enhancer and used in PLC */
  int last_lag;

  /* PLC state information */
  int consPLICount, prev_enh_pl;
  WebRtc_Word16 perSquare;

  WebRtc_Word16 prevScale, prevPLI;
  WebRtc_Word16 prevLag, prevLpc[LPC_FILTERORDER+1];
  WebRtc_Word16 prevResidual[NSUB_MAX*SUBL];
  WebRtc_Word16 seed;

  /* previous synthesis filter parameters */

  WebRtc_Word16 old_syntdenum[(LPC_FILTERORDER + 1)*NSUB_MAX];

  /* state of output HP filter */
  WebRtc_Word16 hpimemx[2];
  WebRtc_Word16 hpimemy[4];

  /* enhancer state information */
  int use_enhancer;
  WebRtc_Word16 enh_buf[ENH_BUFL+ENH_BUFL_FILTEROVERHEAD];
  WebRtc_Word16 enh_period[ENH_NBLOCKS_TOT];

} iLBC_Dec_Inst_t;

#endif
