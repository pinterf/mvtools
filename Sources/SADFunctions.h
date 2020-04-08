// Functions that computes distances between blocks

// See legal notice in Copying.txt for more information

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA, or visit
// http://www.gnu.org/copyleft/gpl.html .

/*! \file SADFunctions.h
 *  \brief Calculate the sum of absolute differences between two blocks.
 *
 *	The sum of absolute differences between two blocks represents the distance
 *	between these two blocks. The lower this value, the closer the blocks will look like
 *  There are two versions, one in plain C, one in iSSE assembler.
 */

#ifndef __SAD_FUNC__
#define __SAD_FUNC__

#include "types.h"
#include <stdint.h>
#include <cassert>
#include "emmintrin.h"

 // SAD routined are same for SADFunctions.h and SADFunctions_avx.cpp
 // put it into a common h
#include "SADFunctions16.h"

SADFunction* get_sad_function(int BlockX, int BlockY, int bits_per_pixel, arch_t arch);
SADFunction* get_satd_function(int BlockX, int BlockY, int pixelsize, arch_t arch);

#define MK_CFUNC(functionname) extern "C" unsigned int __cdecl functionname (const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch)

#if 0
// test-test-test-failed
#define SAD16_FROM_X265
#ifdef SAD16_FROM_X265
#define SAD10_x264(blsizex, blsizey, type) extern "C" unsigned int __cdecl x264_10_pixel_sad_##blsizex##x##blsizey##_##type##(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch)
// experimental from sad16-a.asm
// arrgh problems: 
// - pitches are not byte level, but have 16bit granularity instead in the original asm. <-- biggest problem for using it out-of-box
// - subw, absw is used, which is 16 bit incompatible, internal sum limits, all stuff can be used safely for 10-12 bits max
// I will not differentiate by bit-depth, too much work a.t.m., it is excluded from Build, anyway
// check: sad16-a.asm preprocessor options (for x86): PREFIX;private_prefix=x264_10;ARCH_X86_64=0;BIT_DEPTH=10;HIGH_BIT_DEPTH=1
// BIT_DEPTH should be 10, HIGH_BIT_DEPTH set to 1. private_prefix: functions will name like that
// check: const16-a.asm preprocessor options (for x86): PREFIX;private_prefix=x264_10;ARCH_X86_64=0;BIT_DEPTH=10;HIGH_BIT_DEPTH=1
SAD10_x264(16, 16, sse2);
SAD10_x264(8, 8, sse2);
SAD10_x264(4, 4, sse2);
#undef SAD10_x264
#endif
#endif

/* included from x264/x265 */
// now the prefix is still x264 (see project properties preprocessor directives for sad-a.asm)
// to be changed
#define SAD_x264(blsizex, blsizey, type) extern "C" unsigned int __cdecl x264_pixel_sad_##blsizex##x##blsizey##_##type(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch)

// AVX2: Supported: 64x64, 64x48, 64x32, 64x16
SAD_x264(64, 64, avx2);
SAD_x264(64, 48, avx2);
SAD_x264(64, 32, avx2);
SAD_x264(64, 16, avx2);
// SSE2: Supported: 64x64, 64x48, 64x32, 64x16
SAD_x264(64, 64, sse2);
SAD_x264(64, 48, sse2);
SAD_x264(64, 32, sse2);
SAD_x264(64, 16, sse2);

// 48*x
SAD_x264(48, 64, avx2);
SAD_x264(48, 64, sse2);
SAD_x264(48, 48, avx2);
SAD_x264(48, 48, sse2);
SAD_x264(48, 24, avx2);
SAD_x264(48, 24, sse2);
SAD_x264(48, 12, avx2);
SAD_x264(48, 12, sse2);

// 32*x
// AVX2
SAD_x264(32, 64, avx2);
SAD_x264(32, 32, avx2);
SAD_x264(32, 24, avx2);
SAD_x264(32, 16, avx2);
SAD_x264(32, 8, avx2);
// SSE3
SAD_x264(32, 64, sse3);
SAD_x264(32, 32, sse3);
SAD_x264(32, 24, sse3);
SAD_x264(32, 16, sse3);
SAD_x264(32,  8, sse3);
// SSE2
SAD_x264(32, 64, sse2);
SAD_x264(32, 32, sse2);
SAD_x264(32, 24, sse2);
SAD_x264(32, 16, sse2);
SAD_x264(32,  8, sse2);

// 24*x
SAD_x264(24, 48, sse2);
SAD_x264(24, 32, sse2);
SAD_x264(24, 24, sse2);
SAD_x264(24, 12, sse2);
SAD_x264(24, 6, sse2);

// 16*x
// SSE3
SAD_x264(16, 64, sse3);
SAD_x264(16, 32, sse3);
SAD_x264(16, 16, sse3);
SAD_x264(16, 12, sse3);
SAD_x264(16, 8, sse3);
SAD_x264(16, 4, sse3);
// SSE2/mmx2
SAD_x264(16, 64, sse2);
SAD_x264(16, 32, sse2);
//SAD_x264(16, 16, mmx2);
SAD_x264(16, 16, sse2);
SAD_x264(16, 12, sse2);
//SAD_x264(16, 8, mmx2);
SAD_x264(16, 8, sse2);
SAD_x264(16, 4, sse2);

// 12*x
SAD_x264(12, 48, sse2);
SAD_x264(12, 24, sse2);
SAD_x264(12, 16, sse2);
SAD_x264(12, 12, sse2);
SAD_x264(12, 6, sse2);
SAD_x264(12, 3, sse2);

// 8*x
SAD_x264(8, 32, sse2);
SAD_x264(8, 16, sse2);
//SAD_x264(8, 16, mmx2);
SAD_x264(8, 8, sse2); // 20181125
SAD_x264(8, 8, mmx2);
SAD_x264(8, 4, sse2); // 20181125
SAD_x264(8, 4, mmx2);

// 6*x
SAD_x264(6, 24, sse2);
SAD_x264(6, 12, sse2);
SAD_x264(6, 6, sse2);

// 4*x
SAD_x264(4, 16, sse2); // 20181125 (not used)
SAD_x264(4, 16, mmx2);
SAD_x264(4, 8, sse2); // 20181125
SAD_x264(4, 8, mmx2);
SAD_x264(4, 4, sse2); // 20181125
SAD_x264(4, 4, mmx2);
#undef SAD_x264

// 1.9.5.3: added ssd & SATD (TSchniede)
// alternative to SAD - SSD: squared sum of differences, VERY sensitive to noise
// ssd did not go live. sample: x264_pixel_ssd_16x16_mmx

/* SATD: Sum of Absolute Transformed Differences, more sensitive to noise, frequency domain based - replacement to dct/SAD */
#define SATD_SSE(blsizex, blsizey, type) extern "C" unsigned int __cdecl x264_pixel_satd_##blsizex##x##blsizey##_##type(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch)
// Make extern functions from the satd pixel-a.asm
// See function selector "get_satd_function" in SadFunctions.cpp
// hard to find where they implemented, pixel-a.asm is macro-world :)
// find x264_pixel_satd_16x16_%1
// 32x32, 32x16, 32x8, 16x32 and 16x4 are not macroised in pixel-a.asm
// already instantiated in pixel-a.asm
// combinations of 4, 8, 16, 32
// todo: combinations of 

SATD_SSE(64, 64, sse2);
SATD_SSE(64, 48, sse2);
SATD_SSE(64, 32, sse2);
SATD_SSE(64, 16, sse2);
//SATD_SSE(64,  8, sse2); no such
SATD_SSE(48, 64, sse2);
SATD_SSE(48, 48, sse2);
SATD_SSE(48, 24, sse2);
SATD_SSE(48, 12, sse2);
SATD_SSE(32, 64, sse2);
SATD_SSE(32, 32, sse2);
SATD_SSE(32, 24, sse2);
SATD_SSE(32, 16, sse2);
SATD_SSE(32,  8, sse2);
//SATD_SSE(32,  4, sse2); no such
SATD_SSE(24, 48, sse2);
SATD_SSE(24, 32, sse2);
SATD_SSE(24, 24, sse2);
SATD_SSE(24, 12, sse2);
SATD_SSE(16, 64, sse2);
SATD_SSE(16, 32, sse2);
SATD_SSE(16, 16, sse2);
SATD_SSE(16, 12, sse2);
SATD_SSE(16,  8, sse2);
SATD_SSE(16,  4, sse2);
// no 12x12 or 12x24 from x265
SATD_SSE(12, 16, sse2);
SATD_SSE( 8, 32, sse2);
SATD_SSE( 8, 16, sse2);
SATD_SSE( 8,  8, sse2);
SATD_SSE( 8,  4, sse2);
#ifndef _M_X64
SATD_SSE( 4, 32, sse2); // simulated
#endif
SATD_SSE( 4, 16, sse2); // in 2014 this was not
SATD_SSE( 4,  8, sse2); // in 2014 this was already mmx2
SATD_SSE( 4,  4, mmx2); // in 2017 pixel-a.asm, only this one is mmx

SATD_SSE(64, 64, sse4);
SATD_SSE(64, 48, sse4);
SATD_SSE(64, 32, sse4);
SATD_SSE(64, 16, sse4);
//SATD_SSE(64,  8, sse4); no such
SATD_SSE(48, 64, sse4);
SATD_SSE(48, 48, sse4);
SATD_SSE(48, 24, sse4);
SATD_SSE(48, 12, sse4);

SATD_SSE(32, 64, sse4);
SATD_SSE(32, 32, sse4);
SATD_SSE(32, 24, sse4);
SATD_SSE(32, 16, sse4);
SATD_SSE(32, 8 , sse4);
// SATD_SSE(32, 4 , sse4); no such
SATD_SSE(24, 48, sse4);
SATD_SSE(24, 32, sse4);
SATD_SSE(24, 24, sse4);
SATD_SSE(24, 12, sse4);

SATD_SSE(16, 64, sse4);
SATD_SSE(16, 32, sse4);
SATD_SSE(16, 16, sse4);
SATD_SSE(16, 12, sse4);
SATD_SSE(16,  8, sse4);
SATD_SSE(16,  4, sse4);
// no 12x12 or 12x24 from x265
SATD_SSE(12, 16, sse4);
SATD_SSE( 8, 32, sse4);
SATD_SSE( 8, 16, sse4);
SATD_SSE( 8,  8, sse4);
SATD_SSE( 8,  4, sse4);
SATD_SSE( 4,  8, sse4);
SATD_SSE( 4,  4, sse4);

SATD_SSE(64, 64, avx);
SATD_SSE(64, 48, avx);
SATD_SSE(64, 32, avx);
SATD_SSE(64, 16, avx);
SATD_SSE(48, 64, avx);
SATD_SSE(48, 48, avx);
SATD_SSE(48, 24, avx);
SATD_SSE(48, 12, avx);

SATD_SSE(32, 64, avx);
SATD_SSE(32, 32, avx);
SATD_SSE(32, 24, avx);
SATD_SSE(32, 16, avx);
SATD_SSE(32,  8, avx);
// SATD_SSE(32,  4, avx); no such
SATD_SSE(24, 48, avx);
SATD_SSE(24, 32, avx);
SATD_SSE(24, 24, avx);
SATD_SSE(24, 12, avx);
SATD_SSE(16, 64, avx);
SATD_SSE(16, 32, avx);
SATD_SSE(16, 16, avx);
SATD_SSE(16, 12, avx);
SATD_SSE(16,  8, avx);
SATD_SSE(16,  4, avx);
// no 12x12 or 12x24 from x265
SATD_SSE(12, 16, avx);
SATD_SSE( 8, 32, avx);
SATD_SSE( 8, 16, avx);
SATD_SSE( 8,  8, avx);
SATD_SSE( 8,  4, avx);

//SATD_SSE(64, 64, avx2); no such
//SATD_SSE(64, 32, avx2); no such
//SATD_SSE(64, 16, avx2); no such
#ifdef _M_X64
SATD_SSE(64, 64, avx2);
SATD_SSE(64, 48, avx2);
SATD_SSE(64, 32, avx2);
SATD_SSE(64, 16, avx2);
SATD_SSE(48, 64, avx2);
SATD_SSE(48, 48, avx2);
SATD_SSE(48, 24, avx2);
SATD_SSE(48, 12, avx2);
SATD_SSE(32, 64, avx2);
SATD_SSE(32, 32, avx2);
SATD_SSE(32, 16, avx2);
SATD_SSE(32, 8, avx2);
SATD_SSE(16, 64, avx2);
SATD_SSE(16, 32, avx2);
#endif
SATD_SSE(16, 16, avx2);
SATD_SSE(16,  8, avx2);
SATD_SSE( 8, 16, avx2);
SATD_SSE( 8,  8, avx2);

#undef SATD_SSE

#undef MK_CFUNC


#endif
