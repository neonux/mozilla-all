/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * ilbc.h
 *
 * This header file contains all of the API's for iLBC.
 *
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_INTERFACE_ILBC_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_INTERFACE_ILBC_H_

/*
 * Define the fixpoint numeric formats
 */

#include "typedefs.h"

/*
 * Solution to support multiple instances
 * Customer has to cast instance to proper type
 */

typedef struct iLBC_encinst_t_ iLBC_encinst_t;

typedef struct iLBC_decinst_t_ iLBC_decinst_t;

/*
 * Comfort noise constants
 */

#define ILBC_SPEECH 1
#define ILBC_CNG  2

#ifdef __cplusplus
extern "C" {
#endif

  /****************************************************************************
   * WebRtcIlbcfix_XxxAssign(...)
   *
   * These functions assigns the encoder/decoder instance to the specified
   * memory location
   *
   * Input:
   *      - XXX_xxxinst       : Pointer to created instance that should be
   *                            assigned
   *      - ILBCXXX_inst_Addr : Pointer to the desired memory space
   *      - size              : The size that this structure occupies (in Word16)
   *
   * Return value             :  0 - Ok
   *                            -1 - Error
   */

  WebRtc_Word16 WebRtcIlbcfix_EncoderAssign(iLBC_encinst_t **iLBC_encinst,
					    WebRtc_Word16 *ILBCENC_inst_Addr,
					    WebRtc_Word16 *size);
  WebRtc_Word16 WebRtcIlbcfix_DecoderAssign(iLBC_decinst_t **iLBC_decinst,
					    WebRtc_Word16 *ILBCDEC_inst_Addr,
					    WebRtc_Word16 *size);


  /****************************************************************************
   * WebRtcIlbcfix_XxxAssign(...)
   *
   * These functions create a instance to the specified structure
   *
   * Input:
   *      - XXX_inst          : Pointer to created instance that should be created
   *
   * Return value             :  0 - Ok
   *                            -1 - Error
   */

  WebRtc_Word16 WebRtcIlbcfix_EncoderCreate(iLBC_encinst_t **iLBC_encinst);
  WebRtc_Word16 WebRtcIlbcfix_DecoderCreate(iLBC_decinst_t **iLBC_decinst);

  /****************************************************************************
   * WebRtcIlbcfix_XxxFree(...)
   *
   * These functions frees the dynamic memory of a specified instance
   *
   * Input:
   *      - XXX_inst          : Pointer to created instance that should be freed
   *
   * Return value             :  0 - Ok
   *                            -1 - Error
   */

  WebRtc_Word16 WebRtcIlbcfix_EncoderFree(iLBC_encinst_t *iLBC_encinst);
  WebRtc_Word16 WebRtcIlbcfix_DecoderFree(iLBC_decinst_t *iLBC_decinst);


  /****************************************************************************
   * WebRtcIlbcfix_EncoderInit(...)
   *
   * This function initializes a iLBC instance
   *
   * Input:
   *      - iLBCenc_inst      : iLBC instance, i.e. the user that should receive
   *                            be initialized
   *      - frameLen          : The frame length of the codec 20/30 (ms)
   *
   * Return value             :  0 - Ok
   *                            -1 - Error
   */

  WebRtc_Word16 WebRtcIlbcfix_EncoderInit(iLBC_encinst_t *iLBCenc_inst,
					  WebRtc_Word16 frameLen);

  /****************************************************************************
   * WebRtcIlbcfix_Encode(...)
   *
   * This function encodes one iLBC frame. Input speech length has be a
   * multiple of the frame length.
   *
   * Input:
   *      - iLBCenc_inst      : iLBC instance, i.e. the user that should encode
   *                            a package
   *      - speechIn          : Input speech vector
   *      - len               : Samples in speechIn (160, 240, 320 or 480)
   *
   * Output:
   *  - encoded               : The encoded data vector
   *
   * Return value             : >0 - Length (in bytes) of coded data
   *                            -1 - Error
   */

  WebRtc_Word16 WebRtcIlbcfix_Encode(iLBC_encinst_t *iLBCenc_inst,
				     WebRtc_Word16 *speechIn,
				     WebRtc_Word16 len,
				     WebRtc_Word16 *encoded);

  /****************************************************************************
   * WebRtcIlbcfix_DecoderInit(...)
   *
   * This function initializes a iLBC instance with either 20 or 30 ms frames
   * Alternatively the WebRtcIlbcfix_DecoderInit_XXms can be used. Then it's
   * not needed to specify the frame length with a variable.
   *
   * Input:
   *      - iLBC_decinst_t    : iLBC instance, i.e. the user that should receive
   *                            be initialized
   *      - frameLen          : The frame length of the codec 20/30 (ms)
   *
   * Return value             :  0 - Ok
   *                            -1 - Error
   */

  WebRtc_Word16 WebRtcIlbcfix_DecoderInit(iLBC_decinst_t *iLBCdec_inst,
					  WebRtc_Word16 frameLen);
  WebRtc_Word16 WebRtcIlbcfix_DecoderInit20Ms(iLBC_decinst_t *iLBCdec_inst);
  WebRtc_Word16 WebRtcIlbcfix_Decoderinit30Ms(iLBC_decinst_t *iLBCdec_inst);

  /****************************************************************************
   * WebRtcIlbcfix_Decode(...)
   *
   * This function decodes a packet with iLBC frame(s). Output speech length
   * will be a multiple of 160 or 240 samples ((160 or 240)*frames/packet).
   *
   * Input:
   *      - iLBCdec_inst      : iLBC instance, i.e. the user that should decode
   *                            a packet
   *      - encoded           : Encoded iLBC frame(s)
   *      - len               : Bytes in encoded vector
   *
   * Output:
   *      - decoded           : The decoded vector
   *      - speechType        : 1 normal, 2 CNG
   *
   * Return value             : >0 - Samples in decoded vector
   *                            -1 - Error
   */

  WebRtc_Word16 WebRtcIlbcfix_Decode(iLBC_decinst_t *iLBCdec_inst,
				     WebRtc_Word16* encoded,
				     WebRtc_Word16 len,
				     WebRtc_Word16 *decoded,
				     WebRtc_Word16 *speechType);
  WebRtc_Word16 WebRtcIlbcfix_Decode20Ms(iLBC_decinst_t *iLBCdec_inst,
					 WebRtc_Word16 *encoded,
					 WebRtc_Word16 len,
					 WebRtc_Word16 *decoded,
					 WebRtc_Word16 *speechType);
  WebRtc_Word16 WebRtcIlbcfix_Decode30Ms(iLBC_decinst_t *iLBCdec_inst,
					 WebRtc_Word16 *encoded,
					 WebRtc_Word16 len,
					 WebRtc_Word16 *decoded,
					 WebRtc_Word16 *speechType);

  /****************************************************************************
   * WebRtcIlbcfix_DecodePlc(...)
   *
   * This function conducts PLC for iLBC frame(s). Output speech length
   * will be a multiple of 160 or 240 samples.
   *
   * Input:
   *      - iLBCdec_inst      : iLBC instance, i.e. the user that should perform
   *                            a PLC
   *      - noOfLostFrames    : Number of PLC frames to produce
   *
   * Output:
   *      - decoded           : The "decoded" vector
   *
   * Return value             : >0 - Samples in decoded PLC vector
   *                            -1 - Error
   */

  WebRtc_Word16 WebRtcIlbcfix_DecodePlc(iLBC_decinst_t *iLBCdec_inst,
					WebRtc_Word16 *decoded,
					WebRtc_Word16 noOfLostFrames);

  /****************************************************************************
   * WebRtcIlbcfix_NetEqPlc(...)
   *
   * This function updates the decoder when a packet loss has occured, but it
   * does not produce any PLC data. Function can be used if another PLC method
   * is used (i.e NetEq).
   *
   * Input:
   *      - iLBCdec_inst      : iLBC instance that should be updated
   *      - noOfLostFrames    : Number of lost frames
   *
   * Output:
   *      - decoded           : The "decoded" vector (nothing in this case)
   *
   * Return value             : >0 - Samples in decoded PLC vector
   *                            -1 - Error
   */

  WebRtc_Word16 WebRtcIlbcfix_NetEqPlc(iLBC_decinst_t *iLBCdec_inst,
				       WebRtc_Word16 *decoded,
				       WebRtc_Word16 noOfLostFrames);

  /****************************************************************************
   * WebRtcIlbcfix_version(...)
   *
   * This function returns the version number of iLBC
   *
   * Output:
   *      - version           : Version number of iLBC (maximum 20 char)
   */

  void WebRtcIlbcfix_version(char *version);

#ifdef __cplusplus
}
#endif

#endif
