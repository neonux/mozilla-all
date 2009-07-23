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
 * oggplay_tools.c
 *
 * Shane Stephens <shane.stephens@annodex.net>
 * Michael Martin
 */


#include "oggplay_private.h"
#include <string.h>

ogg_int64_t
oggplay_sys_time_in_ms(void) {
#ifdef WIN32
  FILETIME ft;
  GetSystemTimeAsFileTime(&ft);
  return ((ogg_int64_t)ft.dwHighDateTime << 32 | ft.dwLowDateTime) / 10000;
#else
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (ogg_int64_t)tv.tv_sec * 1000 + (ogg_int64_t)tv.tv_usec / 1000;
#endif
}

void
oggplay_millisleep(long ms) {
#ifdef WIN32
  Sleep(ms);
#elif defined(OS2)
  DosSleep(ms);
#else
  struct timespec ts = {0, (ogg_int64_t)ms * 1000000LL};
  nanosleep(&ts, NULL);
#endif
}


