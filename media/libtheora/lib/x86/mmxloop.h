#if !defined(_x86_mmxloop_H)
# define _x86_mmxloop_H (1)
# include <stddef.h>
# include "x86int.h"

#if defined(OC_X86_ASM)

/*On entry, mm0={a0,...,a7}, mm1={b0,...,b7}, mm2={c0,...,c7}, mm3={d0,...d7}.
  On exit, mm1={b0+lflim(R_0,L),...,b7+lflim(R_7,L)} and
   mm2={c0-lflim(R_0,L),...,c7-lflim(R_7,L)}; mm0 and mm3 are clobbered.*/
#define OC_LOOP_FILTER8_MMX \
 "#OC_LOOP_FILTER8_MMX\n\t" \
 /*mm7=0*/ \
 "pxor %%mm7,%%mm7\n\t" \
 /*mm6:mm0={a0,...,a7}*/ \
 "movq %%mm0,%%mm6\n\t" \
 "punpcklbw %%mm7,%%mm0\n\t" \
 "punpckhbw %%mm7,%%mm6\n\t" \
 /*mm3:mm5={d0,...,d7}*/ \
 "movq %%mm3,%%mm5\n\t" \
 "punpcklbw %%mm7,%%mm3\n\t" \
 "punpckhbw %%mm7,%%mm5\n\t" \
 /*mm6:mm0={a0-d0,...,a7-d7}*/ \
 "psubw %%mm3,%%mm0\n\t" \
 "psubw %%mm5,%%mm6\n\t" \
 /*mm3:mm1={b0,...,b7}*/ \
 "movq %%mm1,%%mm3\n\t" \
 "punpcklbw %%mm7,%%mm1\n\t" \
 "movq %%mm2,%%mm4\n\t" \
 "punpckhbw %%mm7,%%mm3\n\t" \
 /*mm5:mm4={c0,...,c7}*/ \
 "movq %%mm2,%%mm5\n\t" \
 "punpcklbw %%mm7,%%mm4\n\t" \
 "punpckhbw %%mm7,%%mm5\n\t" \
 /*mm7={3}x4 \
   mm5:mm4={c0-b0,...,c7-b7}*/ \
 "pcmpeqw %%mm7,%%mm7\n\t" \
 "psubw %%mm1,%%mm4\n\t" \
 "psrlw $14,%%mm7\n\t" \
 "psubw %%mm3,%%mm5\n\t" \
 /*Scale by 3.*/ \
 "pmullw %%mm7,%%mm4\n\t" \
 "pmullw %%mm7,%%mm5\n\t" \
 /*mm7={4}x4 \
   mm5:mm4=f={a0-d0+3*(c0-b0),...,a7-d7+3*(c7-b7)}*/ \
 "psrlw $1,%%mm7\n\t" \
 "paddw %%mm0,%%mm4\n\t" \
 "psllw $2,%%mm7\n\t" \
 "movq (%[ll]),%%mm0\n\t" \
 "paddw %%mm6,%%mm5\n\t" \
 /*R_i has the range [-127,128], so we compute -R_i instead. \
   mm4=-R_i=-(f+4>>3)=0xFF^(f-4>>3)*/ \
 "psubw %%mm7,%%mm4\n\t" \
 "psubw %%mm7,%%mm5\n\t" \
 "psraw $3,%%mm4\n\t" \
 "psraw $3,%%mm5\n\t" \
 "pcmpeqb %%mm7,%%mm7\n\t" \
 "packsswb %%mm5,%%mm4\n\t" \
 "pxor %%mm6,%%mm6\n\t" \
 "pxor %%mm7,%%mm4\n\t" \
 "packuswb %%mm3,%%mm1\n\t" \
 /*Now compute lflim of -mm4 cf. Section 7.10 of the sepc.*/ \
 /*There's no unsigned byte+signed byte with unsigned saturation op code, so \
    we have to split things by sign (the other option is to work in 16 bits, \
    but working in 8 bits gives much better parallelism). \
   We compute abs(R_i), but save a mask of which terms were negative in mm6. \
   Then we compute mm4=abs(lflim(R_i,L))=min(abs(R_i),max(2*L-abs(R_i),0)). \
   Finally, we split mm4 into positive and negative pieces using the mask in \
    mm6, and add and subtract them as appropriate.*/ \
 /*mm4=abs(-R_i)*/ \
 /*mm7=255-2*L*/ \
 "pcmpgtb %%mm4,%%mm6\n\t" \
 "psubb %%mm0,%%mm7\n\t" \
 "pxor %%mm6,%%mm4\n\t" \
 "psubb %%mm0,%%mm7\n\t" \
 "psubb %%mm6,%%mm4\n\t" \
 /*mm7=255-max(2*L-abs(R_i),0)*/ \
 "paddusb %%mm4,%%mm7\n\t" \
 /*mm4=min(abs(R_i),max(2*L-abs(R_i),0))*/ \
 "paddusb %%mm7,%%mm4\n\t" \
 "psubusb %%mm7,%%mm4\n\t" \
 /*Now split mm4 by the original sign of -R_i.*/ \
 "movq %%mm4,%%mm5\n\t" \
 "pand %%mm6,%%mm4\n\t" \
 "pandn %%mm5,%%mm6\n\t" \
 /*mm1={b0+lflim(R_0,L),...,b7+lflim(R_7,L)}*/ \
 /*mm2={c0-lflim(R_0,L),...,c7-lflim(R_7,L)}*/ \
 "paddusb %%mm4,%%mm1\n\t" \
 "psubusb %%mm4,%%mm2\n\t" \
 "psubusb %%mm6,%%mm1\n\t" \
 "paddusb %%mm6,%%mm2\n\t" \

#define OC_LOOP_FILTER_V_MMX(_pix,_ystride,_ll) \
  do{ \
    ptrdiff_t ystride3__; \
    __asm__ __volatile__( \
      /*mm0={a0,...,a7}*/ \
      "movq (%[pix]),%%mm0\n\t" \
      /*ystride3=_ystride*3*/ \
      "lea (%[ystride],%[ystride],2),%[ystride3]\n\t" \
      /*mm3={d0,...,d7}*/ \
      "movq (%[pix],%[ystride3]),%%mm3\n\t" \
      /*mm1={b0,...,b7}*/ \
      "movq (%[pix],%[ystride]),%%mm1\n\t" \
      /*mm2={c0,...,c7}*/ \
      "movq (%[pix],%[ystride],2),%%mm2\n\t" \
      OC_LOOP_FILTER8_MMX \
      /*Write it back out.*/ \
      "movq %%mm1,(%[pix],%[ystride])\n\t" \
      "movq %%mm2,(%[pix],%[ystride],2)\n\t" \
      :[ystride3]"=&r"(ystride3__) \
      :[pix]"r"(_pix-_ystride*2),[ystride]"r"((ptrdiff_t)(_ystride)), \
       [ll]"r"(_ll) \
      :"memory" \
    ); \
  } \
  while(0)

#define OC_LOOP_FILTER_H_MMX(_pix,_ystride,_ll) \
  do{ \
    unsigned char *pix__; \
    ptrdiff_t      ystride3__; \
    ptrdiff_t      d__; \
    pix__=(_pix)-2; \
    __asm__ __volatile__( \
      /*x x x x d0 c0 b0 a0*/ \
      "movd (%[pix]),%%mm0\n\t" \
      /*x x x x d1 c1 b1 a1*/ \
      "movd (%[pix],%[ystride]),%%mm1\n\t" \
      /*ystride3=_ystride*3*/ \
      "lea (%[ystride],%[ystride],2),%[ystride3]\n\t" \
      /*x x x x d2 c2 b2 a2*/ \
      "movd (%[pix],%[ystride],2),%%mm2\n\t" \
      /*x x x x d3 c3 b3 a3*/ \
      "lea (%[pix],%[ystride],4),%[d]\n\t" \
      "movd (%[pix],%[ystride3]),%%mm3\n\t" \
      /*x x x x d4 c4 b4 a4*/ \
      "movd (%[d]),%%mm4\n\t" \
      /*x x x x d5 c5 b5 a5*/ \
      "movd (%[d],%[ystride]),%%mm5\n\t" \
      /*x x x x d6 c6 b6 a6*/ \
      "movd (%[d],%[ystride],2),%%mm6\n\t" \
      /*x x x x d7 c7 b7 a7*/ \
      "movd (%[d],%[ystride3]),%%mm7\n\t" \
      /*mm0=d1 d0 c1 c0 b1 b0 a1 a0*/ \
      "punpcklbw %%mm1,%%mm0\n\t" \
      /*mm2=d3 d2 c3 c2 b3 b2 a3 a2*/ \
      "punpcklbw %%mm3,%%mm2\n\t" \
      /*mm3=d1 d0 c1 c0 b1 b0 a1 a0*/ \
      "movq %%mm0,%%mm3\n\t" \
      /*mm0=b3 b2 b1 b0 a3 a2 a1 a0*/ \
      "punpcklwd %%mm2,%%mm0\n\t" \
      /*mm3=d3 d2 d1 d0 c3 c2 c1 c0*/ \
      "punpckhwd %%mm2,%%mm3\n\t" \
      /*mm1=b3 b2 b1 b0 a3 a2 a1 a0*/ \
      "movq %%mm0,%%mm1\n\t" \
      /*mm4=d5 d4 c5 c4 b5 b4 a5 a4*/ \
      "punpcklbw %%mm5,%%mm4\n\t" \
      /*mm6=d7 d6 c7 c6 b7 b6 a7 a6*/ \
      "punpcklbw %%mm7,%%mm6\n\t" \
      /*mm5=d5 d4 c5 c4 b5 b4 a5 a4*/ \
      "movq %%mm4,%%mm5\n\t" \
      /*mm4=b7 b6 b5 b4 a7 a6 a5 a4*/ \
      "punpcklwd %%mm6,%%mm4\n\t" \
      /*mm5=d7 d6 d5 d4 c7 c6 c5 c4*/ \
      "punpckhwd %%mm6,%%mm5\n\t" \
      /*mm2=d3 d2 d1 d0 c3 c2 c1 c0*/ \
      "movq %%mm3,%%mm2\n\t" \
      /*mm0=a7 a6 a5 a4 a3 a2 a1 a0*/ \
      "punpckldq %%mm4,%%mm0\n\t" \
      /*mm1=b7 b6 b5 b4 b3 b2 b1 b0*/ \
      "punpckhdq %%mm4,%%mm1\n\t" \
      /*mm2=c7 c6 c5 c4 c3 c2 c1 c0*/ \
      "punpckldq %%mm5,%%mm2\n\t" \
      /*mm3=d7 d6 d5 d4 d3 d2 d1 d0*/ \
      "punpckhdq %%mm5,%%mm3\n\t" \
      OC_LOOP_FILTER8_MMX \
      /*mm2={b0+R_0'',...,b7+R_7''}*/ \
      "movq %%mm1,%%mm0\n\t" \
      /*mm1={b0+R_0'',c0-R_0'',...,b3+R_3'',c3-R_3''}*/ \
      "punpcklbw %%mm2,%%mm1\n\t" \
      /*mm2={b4+R_4'',c4-R_4'',...,b7+R_7'',c7-R_7''}*/ \
      "punpckhbw %%mm2,%%mm0\n\t" \
      /*[d]=c1 b1 c0 b0*/ \
      "movd %%mm1,%[d]\n\t" \
      "movw %w[d],1(%[pix])\n\t" \
      "psrlq $32,%%mm1\n\t" \
      "shr $16,%[d]\n\t" \
      "movw %w[d],1(%[pix],%[ystride])\n\t" \
      /*[d]=c3 b3 c2 b2*/ \
      "movd %%mm1,%[d]\n\t" \
      "movw %w[d],1(%[pix],%[ystride],2)\n\t" \
      "shr $16,%[d]\n\t" \
      "movw %w[d],1(%[pix],%[ystride3])\n\t" \
      "lea (%[pix],%[ystride],4),%[pix]\n\t" \
      /*[d]=c5 b5 c4 b4*/ \
      "movd %%mm0,%[d]\n\t" \
      "movw %w[d],1(%[pix])\n\t" \
      "psrlq $32,%%mm0\n\t" \
      "shr $16,%[d]\n\t" \
      "movw %w[d],1(%[pix],%[ystride])\n\t" \
      /*[d]=c7 b7 c6 b6*/ \
      "movd %%mm0,%[d]\n\t" \
      "movw %w[d],1(%[pix],%[ystride],2)\n\t" \
      "shr $16,%[d]\n\t" \
      "movw %w[d],1(%[pix],%[ystride3])\n\t" \
      :[pix]"+r"(pix__),[ystride3]"=&r"(ystride3__),[d]"=&r"(d__) \
      :[ystride]"r"((ptrdiff_t)(_ystride)),[ll]"r"(_ll) \
      :"memory" \
    ); \
  } \
  while(0)

# endif
#endif
