/*
   Copyright (C) 2003 Commonwealth Scientific and Industrial Research
   Organisation (CSIRO) Australia

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   - Neither the name of CSIRO Australia nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
   PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE ORGANISATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*
 * oggplay_data.c
 *
 * Shane Stephens <shane.stephens@annodex.net>
 */

#include "oggplay_private.h"
#include "oggplay/oggplay_callback_info.h"

#include <stdlib.h>
#include <string.h>

#if HAVE_INTTYPES_H
#include <inttypes.h>
#else
#if LONG_MAX==2147483647L
#define PRId64 "lld"
#else
#define PRId64 "ld"
#endif
#endif

/*
 * the normal lifecycle for a frame is:
 *
 * (1) frame gets decoded and added to list with a locking value of 1
 * (2) frame gets delivered to user
 * (3) frame becomes out-of-date (its presentation time expires) and its
 *      lock is decremented
 * (4) frame is removed from list and freed
 *
 * This can be modified by:
 * (a) early consumption by user (user calls oggplay_mark_record_consumed)
 * (b) frame locking by user (user calls oggplay_mark_record_locked) and
 *     subsequent unlocking (user calls oggplay_mark_record_consumed)
 */

void
oggplay_data_initialise_list (OggPlayDecode *decode) {

  decode->data_list = decode->end_of_data_list = NULL;
  decode->untimed_data_list = NULL;

}

/*
 * helper function to append data packets to end of data_list
 */
void
oggplay_data_add_to_list_end(OggPlayDecode *decode, OggPlayDataHeader *data) {

  data->next = NULL;

  if (decode->data_list == NULL) {
    decode->data_list = data;
    decode->end_of_data_list = data;
  } else {
    decode->end_of_data_list->next = data;
    decode->end_of_data_list = data;
  }

}

/*
 * helper function to append data packets to front of data_list
 */
void
oggplay_data_add_to_list_front(OggPlayDecode *decode, OggPlayDataHeader *data) {
  if (decode->data_list == NULL) {
    decode->data_list = decode->end_of_data_list = data;
    data->next = NULL;
  } else {
    data->next = decode->data_list;
    decode->data_list = data;
  }
}

void
_print_list(char *name, OggPlayDataHeader *p) {
    printf("%s: ", name);
    for (; p != NULL; p = p->next) {
      printf("%"PRId64"[%d]", p->presentation_time >> 32, p->lock);
      if (p->next != NULL) printf("->");
    }
    printf("\n");
}


void
oggplay_data_add_to_list (OggPlayDecode *decode, OggPlayDataHeader *data) {

  /*
   * if this is a packet with an unknown display time, prepend it to
   * the untimed_data_list for later timestamping.
   */

  ogg_int64_t samples_in_next_in_list;

  //_print_list("before", decode->data_list);
  //_print_list("untimed before", decode->untimed_data_list);

  if (data->presentation_time == -1) {
    data->next = decode->untimed_data_list;
    decode->untimed_data_list = data;
  } else {
    /*
     * process the untimestamped data into the timestamped data list.
     *
     * First store any old data.
     */
    ogg_int64_t presentation_time         = data->presentation_time;
    samples_in_next_in_list               = data->samples_in_record;


    while (decode->untimed_data_list != NULL) {
      OggPlayDataHeader *untimed = decode->untimed_data_list;

      presentation_time -=
                samples_in_next_in_list * decode->granuleperiod;
      untimed->presentation_time = presentation_time;
      decode->untimed_data_list = untimed->next;
      samples_in_next_in_list = untimed->samples_in_record;

      if (untimed->presentation_time >= decode->player->presentation_time) {
        oggplay_data_add_to_list_front(decode, untimed);
      } else {
        oggplay_free(untimed);
      }

    }

    oggplay_data_add_to_list_end(decode, data);

    /*
     * if the StreamInfo is still at uninitialised, then this is the first
     * meaningful data packet!  StreamInfo will be updated to
     * OGGPLAY_STREAM_INITIALISED in oggplay_callback_info.c as part of the
     * callback process.
     */
    if (decode->stream_info == OGGPLAY_STREAM_UNINITIALISED) {
      decode->stream_info = OGGPLAY_STREAM_FIRST_DATA;
    }

  }

  //_print_list("after", decode->data_list);
  //_print_list("untimed after", decode->untimed_data_list);

}

void
oggplay_data_free_list(OggPlayDataHeader *list) {
  OggPlayDataHeader *p;

  while (list != NULL) {
    p = list;
    list = list->next;
    oggplay_free(p);
  }
}

void
oggplay_data_shutdown_list (OggPlayDecode *decode) {

  oggplay_data_free_list(decode->data_list);
  oggplay_data_free_list(decode->untimed_data_list);

}

/*
 * this function removes any displayed, unlocked frames from the list.
 *
 * this function also removes any undisplayed frames that are before the
 * global presentation time.
 */
void
oggplay_data_clean_list (OggPlayDecode *decode) {

  ogg_int64_t         target = decode->player->target;
  OggPlayDataHeader * header = decode->data_list;
  OggPlayDataHeader * p      = NULL;

  while (header != NULL) {
    if
    (
      header->lock == 0
      &&
      (
        (
          (header->presentation_time < (target + decode->offset))
          &&
          header->has_been_presented
        )
        ||
        (
          (header->presentation_time < decode->player->presentation_time)
        )
      )

    )
    {
      if (p == NULL) {
        decode->data_list = decode->data_list->next;
        if (decode->data_list == NULL)
          decode->end_of_data_list = NULL;
        oggplay_free (header);
        header = decode->data_list;
      } else {
        if (header->next == NULL)
          decode->end_of_data_list = p;
        p->next = header->next;
        oggplay_free (header);
        header = p->next;
      }
    } else {
      p = header;
      header = header->next;
    }
  }
}

void
oggplay_data_initialise_header (OggPlayDecode *decode,
                OggPlayDataHeader *header) {
  /*
   * the frame is not cleaned until its presentation time has passed.  We'll
   * check presentation times in oggplay_data_clean_list.
   */
  header->lock = 0;
  header->next = NULL;
  header->presentation_time = decode->current_loc;
  header->has_been_presented = 0;

}

void
oggplay_data_handle_audio_data (OggPlayDecode *decode, void *data,
      int samples, int samplesize) {

  int                   num_channels;
  OggPlayAudioRecord  * record = NULL;

  num_channels = ((OggPlayAudioDecode *)decode)->sound_info.channels;
  record = (OggPlayAudioRecord*)oggplay_calloc(sizeof(OggPlayAudioRecord) +
                  samples * samplesize * num_channels, 1);

  if (record == NULL)
    return;

  oggplay_data_initialise_header(decode, &(record->header));

  record->header.samples_in_record = samples;

  record->data = (void *)(record + 1);

  memcpy(record->data, data, samples * samplesize * num_channels);
  /*
  printf("[%f%f%f]\n", ((float *)record->data)[0], ((float *)record->data)[1],
                    ((float *)record->data)[2]);
  */
  oggplay_data_add_to_list(decode, &(record->header));
}

void
oggplay_data_handle_cmml_data(OggPlayDecode *decode, unsigned char *data,
                int size) {

  OggPlayTextRecord * record = NULL;

  record =
      (OggPlayTextRecord*)oggplay_calloc (sizeof(OggPlayTextRecord) + size + 1, 1);

  if (record == NULL)
    return;

  oggplay_data_initialise_header(decode, &(record->header));

  record->header.samples_in_record = 1;
  record->data = (char *)(record + 1);

  memcpy(record->data, data, size);
  record->data[size] = '\0';

  oggplay_data_add_to_list(decode, &(record->header));

}

void
oggplay_data_handle_theora_frame (OggPlayTheoraDecode *decode,
                                    yuv_buffer *buffer) {

  int                   size = sizeof (OggPlayVideoRecord);
  int                   i;
  unsigned char       * p;
  unsigned char       * q;
  unsigned char       * p2;
  unsigned char       * q2;
  OggPlayVideoRecord  * record;
  OggPlayVideoData    * data;

  if (buffer->y_stride < 0) {
    size -= buffer->y_stride * buffer->y_height;
    size -= buffer->uv_stride * buffer->uv_height * 2;
  } else {
    size += buffer->y_stride * buffer->y_height;
    size += buffer->uv_stride * buffer->uv_height * 2;
  }

  /*
   * we need to set the output strides to the input widths because we are
   * trying not to pass negative output stride issues on to the poor user.
   */
  record = (OggPlayVideoRecord*)oggplay_malloc (size);

  if (record == NULL)
    return;

  record->header.samples_in_record = 1;
  data = &(record->data);

  data->y = (unsigned char *)(record + 1);
  data->u = data->y + (decode->y_stride * decode->y_height);
  data->v = data->u + (decode->uv_stride * decode->uv_height);

  /*
   * *grumble* theora plays silly buggers with pointers so we need to do
   * a row-by-row copy (stride may be negative)
   */
  p = data->y;
  q = buffer->y;
  for (i = 0; i < decode->y_height; i++) {
    memcpy(p, q, decode->y_width);
    p += decode->y_width;
    q += buffer->y_stride;
  }

  p = data->u;
  q = buffer->u;
  p2 = data->v;
  q2 = buffer->v;
  for (i = 0; i < decode->uv_height; i++) {
    memcpy(p, q, decode->uv_width);
    memcpy(p2, q2, decode->uv_width);
    p += decode->uv_width;
    p2 += decode->uv_width;
    q += buffer->uv_stride;
    q2 += buffer->uv_stride;
  }

  /* if we're to send RGB video, convert here */
  if (decode->convert_to_rgb) {
    OggPlayYUVChannels      yuv;
    OggPlayRGBChannels      rgb;
    OggPlayOverlayRecord  * orecord;
    OggPlayOverlayData    * odata;

    yuv.ptry = data->y;
    yuv.ptru = data->u;
    yuv.ptrv = data->v;
    yuv.y_width = decode->y_width;
    yuv.y_height = decode->y_height;
    yuv.uv_width = decode->uv_width;
    yuv.uv_height = decode->uv_height;

    size = sizeof(OggPlayOverlayRecord) + decode->y_width * decode->y_height * 4;
    orecord = (OggPlayOverlayRecord*) oggplay_malloc (size);
    if (orecord) {
      oggplay_data_initialise_header((OggPlayDecode *)decode, &(orecord->header));
      orecord->header.samples_in_record = 1;
      odata = &(orecord->data);

      rgb.ptro = (unsigned char*)(orecord+1);
      rgb.rgb_width = yuv.y_width;
      rgb.rgb_height = yuv.y_height;

      oggplay_yuv2rgba(&yuv, &rgb);

//      odata->rgb = NULL;
//      odata->rgba = rgb.ptro;
      odata->rgb = rgb.ptro;
      odata->rgba = NULL;
      odata->width = rgb.rgb_width;
      odata->height = rgb.rgb_height;
      odata->stride = rgb.rgb_width*4;

      oggplay_free(record);
    
      oggplay_data_add_to_list((OggPlayDecode *)decode, &(orecord->header));
    }
  }
  else {
    oggplay_data_initialise_header((OggPlayDecode *)decode, &(record->header));
    oggplay_data_add_to_list((OggPlayDecode *)decode, &(record->header));
  }
}

#ifdef HAVE_KATE
void
oggplay_data_handle_kate_data(OggPlayKateDecode *decode, const kate_event *ev) {

  OggPlayTextRecord * record = NULL;

#ifdef HAVE_TIGER
  tiger_renderer_add_event(decode->tr, ev->ki, ev);

  if (decode->use_tiger) {
    /* if rendering with Tiger, we don't add an event, a synthetic one will be
       generated each "tick" with an updated tracker state */
  }
  else
#endif
  {
    record = (OggPlayTextRecord*)oggplay_calloc (sizeof(OggPlayTextRecord) + ev->len0, 1);
    if (!record)
      return;

    oggplay_data_initialise_header(&decode->decoder, &(record->header));

    //record->header.presentation_time = (ogg_int64_t)(ev->start_time*1000);
    record->header.samples_in_record = (ev->end_time-ev->start_time)*1000;
    record->data = (char *)(record + 1);

    memcpy(record->data, ev->text, ev->len0);

    oggplay_data_add_to_list(&decode->decoder, &(record->header));
  }
}
#endif

#ifdef HAVE_TIGER
void
oggplay_data_update_tiger(OggPlayKateDecode *decode, int active, ogg_int64_t presentation_time, OggPlayCallbackInfo *info) {

  OggPlayOverlayRecord  * record = NULL;
  OggPlayOverlayData    * data = NULL;
  size_t                size = sizeof (OggPlayOverlayRecord);
  int                   track = active && decode->use_tiger;
  kate_float            t = OGGPLAY_TIME_FP_TO_INT(presentation_time) / 1000.0f;

  if (!decode->init) return;

  if (track) {
    if (info) {
      if (info->required_records>0) {
        OggPlayDataHeader *header = info->records[0];
        data = (OggPlayOverlayData*)(header+1);
        if (decode->tr && data->rgb) {
          tiger_renderer_set_buffer(decode->tr, data->rgb, data->width, data->height, data->stride, 1);
        }
        else {
          /* we're supposed to overlay on a frame, but the available frame has no RGB buffer */
          /* fprintf(stderr,"no RGB buffer found for video frame\n"); */
          return;
        }
      }
      else {
        /* we're supposed to overlay on a frame, but there is no frame available */
        /* fprintf(stderr,"no video frame to overlay on\n"); */
        return;
      }
    }
    else {
      // TODO: some way of knowing the size of the video we'll be drawing onto, if any
      int width = decode->k.ki->original_canvas_width;
      int height = decode->k.ki->original_canvas_height;
      if (width <= 0 || height <= 0) {
        /* some default resolution if we're not overlaying onto a video and the canvas size is unknown */
        width = 640;
        height = 480;
      }
      size = sizeof (OggPlayOverlayRecord) + width*height*4;
      record = (OggPlayOverlayRecord*)oggplay_calloc (1, size);
      if (!record)
        return;

      record->header.samples_in_record = 1;
      data= &(record->data);
      oggplay_data_initialise_header((OggPlayDecode *)decode, &(record->header));

      data->rgba = (unsigned char*)(record+1);
      data->rgb = NULL;
      data->width = width;
      data->height = height;
      data->stride = width*4;

      if (decode->tr && data->rgba) {
        tiger_renderer_set_buffer(decode->tr, data->rgba, data->width, data->height, data->stride, 1);
      }

      oggplay_data_add_to_list(&decode->decoder, &(record->header));
      record->header.presentation_time=presentation_time;
    }
  }

  if (decode->tr) {
    tiger_renderer_update(decode->tr, t, track);
  }

  if (track) {
    /* buffer was either calloced, so already cleared, or already filled with video, so no clearing */
    if (decode->tr) {
      tiger_renderer_render(decode->tr);
    }
  }
}
#endif

