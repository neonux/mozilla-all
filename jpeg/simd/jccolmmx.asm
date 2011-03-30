;
; jccolmmx.asm - colorspace conversion (MMX)
;
; Copyright 2009 Pierre Ossman <ossman@cendio.se> for Cendio AB
; Copyright 2009 D. R. Commander
;
; Based on
; x86 SIMD extension for IJG JPEG library
; Copyright (C) 1999-2006, MIYASAKA Masaru.
; For conditions of distribution and use, see copyright notice in jsimdext.inc
;
; This file should be assembled with NASM (Netwide Assembler),
; can *not* be assembled with Microsoft's MASM or any compatible
; assembler (including Borland's Turbo Assembler).
; NASM is available from http://nasm.sourceforge.net/ or
; http://sourceforge.net/project/showfiles.php?group_id=6208
;
; [TAB8]

%include "jsimdext.inc"

; --------------------------------------------------------------------------

%define SCALEBITS	16

F_0_081	equ	 5329			; FIX(0.08131)
F_0_114	equ	 7471			; FIX(0.11400)
F_0_168	equ	11059			; FIX(0.16874)
F_0_250	equ	16384			; FIX(0.25000)
F_0_299	equ	19595			; FIX(0.29900)
F_0_331	equ	21709			; FIX(0.33126)
F_0_418	equ	27439			; FIX(0.41869)
F_0_587	equ	38470			; FIX(0.58700)
F_0_337	equ	(F_0_587 - F_0_250)	; FIX(0.58700) - FIX(0.25000)

; --------------------------------------------------------------------------
	SECTION	SEG_CONST

	alignz	16
	global	EXTN(jconst_rgb_ycc_convert_mmx)

EXTN(jconst_rgb_ycc_convert_mmx):

PW_F0299_F0337	times 2 dw  F_0_299, F_0_337
PW_F0114_F0250	times 2 dw  F_0_114, F_0_250
PW_MF016_MF033	times 2 dw -F_0_168,-F_0_331
PW_MF008_MF041	times 2 dw -F_0_081,-F_0_418
PD_ONEHALFM1_CJ	times 2 dd  (1 << (SCALEBITS-1)) - 1 + (CENTERJSAMPLE << SCALEBITS)
PD_ONEHALF	times 2 dd  (1 << (SCALEBITS-1))

	alignz	16

; --------------------------------------------------------------------------
%include "jcclrmmx.asm"

%undef RGB_RED
%undef RGB_GREEN
%undef RGB_BLUE
%undef RGB_PIXELSIZE
%define RGB_RED 0
%define RGB_GREEN 1
%define RGB_BLUE 2
%define RGB_PIXELSIZE 3
%define jsimd_rgb_ycc_convert_mmx jsimd_extrgb_ycc_convert_mmx
%include "jcclrmmx.asm"

%undef RGB_RED
%undef RGB_GREEN
%undef RGB_BLUE
%undef RGB_PIXELSIZE
%define RGB_RED 0
%define RGB_GREEN 1
%define RGB_BLUE 2
%define RGB_PIXELSIZE 4
%define jsimd_rgb_ycc_convert_mmx jsimd_extrgbx_ycc_convert_mmx
%include "jcclrmmx.asm"

%undef RGB_RED
%undef RGB_GREEN
%undef RGB_BLUE
%undef RGB_PIXELSIZE
%define RGB_RED 2
%define RGB_GREEN 1
%define RGB_BLUE 0
%define RGB_PIXELSIZE 3
%define jsimd_rgb_ycc_convert_mmx jsimd_extbgr_ycc_convert_mmx
%include "jcclrmmx.asm"

%undef RGB_RED
%undef RGB_GREEN
%undef RGB_BLUE
%undef RGB_PIXELSIZE
%define RGB_RED 2
%define RGB_GREEN 1
%define RGB_BLUE 0
%define RGB_PIXELSIZE 4
%define jsimd_rgb_ycc_convert_mmx jsimd_extbgrx_ycc_convert_mmx
%include "jcclrmmx.asm"

%undef RGB_RED
%undef RGB_GREEN
%undef RGB_BLUE
%undef RGB_PIXELSIZE
%define RGB_RED 3
%define RGB_GREEN 2
%define RGB_BLUE 1
%define RGB_PIXELSIZE 4
%define jsimd_rgb_ycc_convert_mmx jsimd_extxbgr_ycc_convert_mmx
%include "jcclrmmx.asm"

%undef RGB_RED
%undef RGB_GREEN
%undef RGB_BLUE
%undef RGB_PIXELSIZE
%define RGB_RED 1
%define RGB_GREEN 2
%define RGB_BLUE 3
%define RGB_PIXELSIZE 4
%define jsimd_rgb_ycc_convert_mmx jsimd_extxrgb_ycc_convert_mmx
%include "jcclrmmx.asm"
