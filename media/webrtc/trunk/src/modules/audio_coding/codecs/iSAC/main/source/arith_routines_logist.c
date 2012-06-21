/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * arith_routines.h
 *
 * This file contains functions for arithmatically encoding and
 * decoding DFT coefficients.
 *
 */


#include "arith_routines.h"



static const WebRtc_Word32 kHistEdgesQ15[51] = {
  -327680, -314573, -301466, -288359, -275252, -262144, -249037, -235930, -222823, -209716,
  -196608, -183501, -170394, -157287, -144180, -131072, -117965, -104858, -91751, -78644,
  -65536, -52429, -39322, -26215, -13108,  0,  13107,  26214,  39321,  52428,
  65536,  78643,  91750,  104857,  117964,  131072,  144179,  157286,  170393,  183500,
  196608,  209715,  222822,  235929,  249036,  262144,  275251,  288358,  301465,  314572,
  327680};


static const int kCdfSlopeQ0[51] = {  /* Q0 */
  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
  5,  5,  13,  23,  47,  87,  154,  315,  700,  1088,
  2471,  6064,  14221,  21463,  36634,  36924,  19750,  13270,  5806,  2312,
  1095,  660,  316,  145,  86,  41,  32,  5,  5,  5,
  5,  5,  5,  5,  5,  5,  5,  5,  5,  2, 0};


static const int kCdfQ16[51] = {  /* Q16 */
  0,  2,  4,  6,  8,  10,  12,  14,  16,  18,
  20,  22,  24,  29,  38,  57,  92,  153,  279,  559,
  994,  1983,  4408,  10097,  18682,  33336,  48105,  56005,  61313,  63636,
  64560,  64998,  65262,  65389,  65447,  65481,  65497,  65510,  65512,  65514,
  65516,  65518,  65520,  65522,  65524,  65526,  65528,  65530,  65532,  65534,
  65535};



/* function to be converted to fixed point */
static __inline WebRtc_UWord32 piecewise(WebRtc_Word32 xinQ15) {

  WebRtc_Word32 ind, qtmp1, qtmp2, qtmp3;
  WebRtc_UWord32 tmpUW32;


  qtmp2 = xinQ15;

  if (qtmp2 < kHistEdgesQ15[0]) {
    qtmp2 = kHistEdgesQ15[0];
  }
  if (qtmp2 > kHistEdgesQ15[50]) {
    qtmp2 = kHistEdgesQ15[50];
  }

  qtmp1 = qtmp2 - kHistEdgesQ15[0];       /* Q15 - Q15 = Q15        */
  ind = (qtmp1 * 5) >> 16;              /* 2^16 / 5 = 0.4 in Q15  */
  /* Q15 -> Q0              */
  qtmp1 = qtmp2 - kHistEdgesQ15[ind];     /* Q15 - Q15 = Q15        */
  qtmp2 = kCdfSlopeQ0[ind] * qtmp1;      /* Q0 * Q15 = Q15         */
  qtmp3 = qtmp2>>15;                    /* Q15 -> Q0              */

  tmpUW32 = kCdfQ16[ind] + qtmp3;    /* Q0 + Q0 = Q0           */
  return tmpUW32;
}



int WebRtcIsac_EncLogisticMulti2(
    Bitstr *streamdata,      /* in-/output struct containing bitstream */
    WebRtc_Word16 *dataQ7,    /* input: data vector */
    const WebRtc_UWord16 *envQ8, /* input: side info vector defining the width of the pdf */
    const int N,       /* input: data vector length / 2 */
    const WebRtc_Word16 isSWB12kHz)
{
  WebRtc_UWord32 W_lower, W_upper;
  WebRtc_UWord32 W_upper_LSB, W_upper_MSB;
  WebRtc_UWord8 *stream_ptr;
  WebRtc_UWord8 *maxStreamPtr;
  WebRtc_UWord8 *stream_ptr_carry;
  WebRtc_UWord32 cdf_lo, cdf_hi;
  int k;

  /* point to beginning of stream buffer */
  stream_ptr = streamdata->stream + streamdata->stream_index;
  W_upper = streamdata->W_upper;

  maxStreamPtr = streamdata->stream + STREAM_SIZE_MAX_60 - 1;
  for (k = 0; k < N; k++)
  {
    /* compute cdf_lower and cdf_upper by evaluating the piecewise linear cdf */
    cdf_lo = piecewise((*dataQ7 - 64) * *envQ8);
    cdf_hi = piecewise((*dataQ7 + 64) * *envQ8);

    /* test and clip if probability gets too small */
    while (cdf_lo+1 >= cdf_hi) {
      /* clip */
      if (*dataQ7 > 0) {
        *dataQ7 -= 128;
        cdf_hi = cdf_lo;
        cdf_lo = piecewise((*dataQ7 - 64) * *envQ8);
      } else {
        *dataQ7 += 128;
        cdf_lo = cdf_hi;
        cdf_hi = piecewise((*dataQ7 + 64) * *envQ8);
      }
    }

    dataQ7++;
    // increment only once per 4 iterations for SWB-16kHz or WB
    // increment only once per 2 iterations for SWB-12kHz
    envQ8 += (isSWB12kHz)? (k & 1):((k & 1) & (k >> 1));


    /* update interval */
    W_upper_LSB = W_upper & 0x0000FFFF;
    W_upper_MSB = W_upper >> 16;
    W_lower = W_upper_MSB * cdf_lo;
    W_lower += (W_upper_LSB * cdf_lo) >> 16;
    W_upper = W_upper_MSB * cdf_hi;
    W_upper += (W_upper_LSB * cdf_hi) >> 16;

    /* shift interval such that it begins at zero */
    W_upper -= ++W_lower;

    /* add integer to bitstream */
    streamdata->streamval += W_lower;

    /* handle carry */
    if (streamdata->streamval < W_lower)
    {
      /* propagate carry */
      stream_ptr_carry = stream_ptr;
      while (!(++(*--stream_ptr_carry)));
    }

    /* renormalize interval, store most significant byte of streamval and update streamval */
    while ( !(W_upper & 0xFF000000) )      /* W_upper < 2^24 */
    {
      W_upper <<= 8;
      *stream_ptr++ = (WebRtc_UWord8) (streamdata->streamval >> 24);

      if(stream_ptr > maxStreamPtr)
      {
        return -ISAC_DISALLOWED_BITSTREAM_LENGTH;
      }
      streamdata->streamval <<= 8;
    }
  }

  /* calculate new stream_index */
  streamdata->stream_index = (int)(stream_ptr - streamdata->stream);
  streamdata->W_upper = W_upper;

  return 0;
}



int WebRtcIsac_DecLogisticMulti2(
    WebRtc_Word16 *dataQ7,       /* output: data vector */
    Bitstr *streamdata,      /* in-/output struct containing bitstream */
    const WebRtc_UWord16 *envQ8, /* input: side info vector defining the width of the pdf */
    const WebRtc_Word16 *ditherQ7,/* input: dither vector */
    const int N,         /* input: data vector length */
    const WebRtc_Word16 isSWB12kHz)
{
  WebRtc_UWord32    W_lower, W_upper;
  WebRtc_UWord32    W_tmp;
  WebRtc_UWord32    W_upper_LSB, W_upper_MSB;
  WebRtc_UWord32    streamval;
  const WebRtc_UWord8 *stream_ptr;
  WebRtc_UWord32    cdf_tmp;
  WebRtc_Word16     candQ7;
  int             k;

  stream_ptr = streamdata->stream + streamdata->stream_index;
  W_upper = streamdata->W_upper;
  if (streamdata->stream_index == 0)   /* first time decoder is called for this stream */
  {
    /* read first word from bytestream */
    streamval = *stream_ptr << 24;
    streamval |= *++stream_ptr << 16;
    streamval |= *++stream_ptr << 8;
    streamval |= *++stream_ptr;
  } else {
    streamval = streamdata->streamval;
  }


  for (k = 0; k < N; k++)
  {
    /* find the integer *data for which streamval lies in [W_lower+1, W_upper] */
    W_upper_LSB = W_upper & 0x0000FFFF;
    W_upper_MSB = W_upper >> 16;

    /* find first candidate by inverting the logistic cdf */
    candQ7 = - *ditherQ7 + 64;
    cdf_tmp = piecewise(candQ7 * *envQ8);

    W_tmp = W_upper_MSB * cdf_tmp;
    W_tmp += (W_upper_LSB * cdf_tmp) >> 16;
    if (streamval > W_tmp)
    {
      W_lower = W_tmp;
      candQ7 += 128;
      cdf_tmp = piecewise(candQ7 * *envQ8);

      W_tmp = W_upper_MSB * cdf_tmp;
      W_tmp += (W_upper_LSB * cdf_tmp) >> 16;
      while (streamval > W_tmp)
      {
        W_lower = W_tmp;
        candQ7 += 128;
        cdf_tmp = piecewise(candQ7 * *envQ8);

        W_tmp = W_upper_MSB * cdf_tmp;
        W_tmp += (W_upper_LSB * cdf_tmp) >> 16;

        /* error check */
        if (W_lower == W_tmp) return -1;
      }
      W_upper = W_tmp;

      /* another sample decoded */
      *dataQ7 = candQ7 - 64;
    }
    else
    {
      W_upper = W_tmp;
      candQ7 -= 128;
      cdf_tmp = piecewise(candQ7 * *envQ8);

      W_tmp = W_upper_MSB * cdf_tmp;
      W_tmp += (W_upper_LSB * cdf_tmp) >> 16;
      while ( !(streamval > W_tmp) )
      {
        W_upper = W_tmp;
        candQ7 -= 128;
        cdf_tmp = piecewise(candQ7 * *envQ8);

        W_tmp = W_upper_MSB * cdf_tmp;
        W_tmp += (W_upper_LSB * cdf_tmp) >> 16;

        /* error check */
        if (W_upper == W_tmp) return -1;
      }
      W_lower = W_tmp;

      /* another sample decoded */
      *dataQ7 = candQ7 + 64;
    }
    ditherQ7++;
    dataQ7++;
    // increment only once per 4 iterations for SWB-16kHz or WB
    // increment only once per 2 iterations for SWB-12kHz
    envQ8 += (isSWB12kHz)? (k & 1):((k & 1) & (k >> 1));

    /* shift interval to start at zero */
    W_upper -= ++W_lower;

    /* add integer to bitstream */
    streamval -= W_lower;

    /* renormalize interval and update streamval */
    while ( !(W_upper & 0xFF000000) )    /* W_upper < 2^24 */
    {
      /* read next byte from stream */
      streamval = (streamval << 8) | *++stream_ptr;
      W_upper <<= 8;
    }
  }

  streamdata->stream_index = (int)(stream_ptr - streamdata->stream);
  streamdata->W_upper = W_upper;
  streamdata->streamval = streamval;

  /* find number of bytes in original stream (determined by current interval width) */
  if ( W_upper > 0x01FFFFFF )
    return streamdata->stream_index - 2;
  else
    return streamdata->stream_index - 1;
}
