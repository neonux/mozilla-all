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

/**
 * YUV to RGB conversion using x86 CPU extensions
 */

#if defined(_MSC_VER)
#include "yuv2rgb_x86_vs.h" 
#elif defined(__GNUC__)
#include "yuv2rgb_x86.h" 
#endif

typedef union
{
	long long               q[2];
	unsigned long long	uq[2]; 	
	int                     d[4]; 
	unsigned int            ud[4];
	short                   w[8];
	unsigned short          uw[8];
	char                    b[16];
	unsigned char           ub[16];
	float                   s[4];
} ATTR_ALIGN(16) simd_t;

#define UV_128 0x0080008000800080LL 
#define Y_16 0x1010101010101010LL
#define Y_Co 0x253f253f253f253fLL
#define GU_Co 0xf37df37df37df37dLL
#define GV_Co 0xe5fce5fce5fce5fcLL
#define BU_Co 0x4093409340934093LL
#define RV_Co 0x3312331233123312LL
#define Y_MASK 0x00ff00ff00ff00ffLL 
#define ALFA 0xffffffffffffffffLL 

/**
 * coefficients and constants for yuv to rgb SIMD conversion
 */
static const simd_t simd_table[9] = {
	{{UV_128, UV_128}},
	{{GU_Co, GU_Co}},
	{{GV_Co, GV_Co}},
	{{BU_Co, BU_Co}},
	{{RV_Co, RV_Co}},
	{{Y_16, Y_16}},
	{{Y_Co, Y_Co}},
	{{Y_MASK, Y_MASK}},
	{{ALFA, ALFA}}
};

/**
 *  the conversion functions using MMX instructions 
 */

/* template for the MMX conversion functions */
#define YUV_CONVERT_MMX(FUNC, CONVERT) YUV_CONVERT(FUNC, CONVERT, 8, 32, 8, 4)
#define CLEANUP emms()
#define OUT_RGBA_32 OUTPUT_RGBA_32(movq, mm, 8, 16, 24)
#define OUT_ARGB_32 OUTPUT_ARGB_32(movq, mm, 8, 16, 24)
#define OUT_BGRA_32 OUTPUT_BGRA_32(movq, mm, 8, 16, 24)
#define MOVNTQ MMX_MOVNTQ

/* yuv420 -> */
#define CONVERT(OUTPUT_FUNC) LOAD_YUV_PLANAR_2(movq, mm) \
			     YUV_2_RGB(movq, mm) 	\
			     OUTPUT_FUNC

YUV_CONVERT_MMX(yuv420_to_rgba_mmx, CONVERT(OUT_RGBA_32))
YUV_CONVERT_MMX(yuv420_to_bgra_mmx, CONVERT(OUT_BGRA_32)) 
YUV_CONVERT_MMX(yuv420_to_argb_mmx, CONVERT(OUT_ARGB_32)) 
#undef CONVERT

#undef CLEANUP
#undef OUT_RGBA_32
#undef OUT_ARGB_32
#undef OUT_BGRA_32
#undef MOVNTQ

/**
 *  the conversion functions using SSE2 instructions 
 */

/* template for the SSE2 conversion functions */
#define YUV_CONVERT_SSE2(FUNC, CONVERT) YUV_CONVERT(FUNC, CONVERT, 16, 64, 16, 8)
#define OUT_RGBA_32 OUTPUT_RGBA_32(movdqa, xmm, 16, 32, 48)
#define OUT_ARGB_32 OUTPUT_ARGB_32(movdqa, xmm, 16, 32, 48)
#define OUT_BGRA_32 OUTPUT_BGRA_32(movdqa, xmm, 16, 32, 48)
#define MOVNTQ SSE2_MOVNTQ
#define CLEANUP

/* yuv420 -> */
#define CONVERT(OUTPUT_FUNC) LOAD_YUV_PLANAR_2(movdqu, xmm) \
       			     YUV_2_RGB(movdqa, xmm)	\
			     OUTPUT_FUNC

YUV_CONVERT_SSE2(yuv420_to_rgba_sse2, CONVERT(OUT_RGBA_32))
YUV_CONVERT_SSE2(yuv420_to_bgra_sse2, CONVERT(OUT_BGRA_32))
YUV_CONVERT_SSE2(yuv420_to_argb_sse2, CONVERT(OUT_ARGB_32)) 
#undef CONVERT

#undef OUT_RGBA_32
#undef OUT_ARGB_32
#undef OUT_BGRA_32
#undef MOVNTQ
#undef CLEANUP 

