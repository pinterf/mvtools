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

#include "def.h"
#include "types.h"
#include <stdint.h>
#include <cassert>
#include "emmintrin.h"

#ifndef USE_SAD_ASM
// alternative to the external asm sad function. is slower a bit
template<int nBlkWidth, int nBlkHeight>
#if defined(GCC) || defined(CLANG)
__attribute__((__target__("sse2")))
#endif 
unsigned int Sad_sse2(const uint8_t* pSrc, int nSrcPitch8, const uint8_t* pRef, int nRefPitch8)
{
  __m128i zero = _mm_setzero_si128();

  const int nSrcPitch = nSrcPitch8;
  const int nRefPitch = nRefPitch8;

  constexpr int vert_inc = ((nBlkWidth % 16 != 0 && nBlkWidth % 8 == 0)) && (nBlkHeight % 8) == 0 ? 8 : nBlkHeight % 4 == 0 ? 4 : nBlkHeight % 2 == 0 ? 2 : 1;
  // unrolled 8, 4, 2 option; 8 implemented only for wmod8

  __m128i acc = _mm_setzero_si128();
  for (int y = 0; y < nBlkHeight; y += vert_inc)
  {
    for (int x = 0; x < nBlkWidth / 16 * 16; x += 16) {
      auto dst1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(pRef + x));
      auto src1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(pSrc + x));
      auto sad1 = _mm_sad_epu8(dst1, src1);
      acc = _mm_add_epi32(acc, sad1);
      if constexpr (vert_inc >= 2) {
        dst1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(pRef + x + nRefPitch));
        src1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(pSrc + x + nSrcPitch));
        sad1 = _mm_sad_epu8(dst1, src1);
        acc = _mm_add_epi32(acc, sad1);
      }
      if constexpr (vert_inc >= 4) {
        dst1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(pRef + x + nRefPitch * 2));
        src1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(pSrc + x + nSrcPitch * 2));
        sad1 = _mm_sad_epu8(dst1, src1);
        acc = _mm_add_epi32(acc, sad1);
        dst1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(pRef + x + nRefPitch * 3));
        src1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(pSrc + x + nSrcPitch * 3));
        sad1 = _mm_sad_epu8(dst1, src1);
        acc = _mm_add_epi32(acc, sad1);
      }
    }
    if constexpr (nBlkWidth % 16 != 0 && nBlkWidth % 8 == 0) {
      for (int x = nBlkWidth / 16 * 16; x < nBlkWidth / 8 * 8; x += 8) {
        auto dst1 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(pRef + x));
        auto src1 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(pSrc + x));
        if constexpr (vert_inc >= 2) {
          dst1 = _mm_unpacklo_epi8(dst1, _mm_loadl_epi64(reinterpret_cast<const __m128i*>(pRef + x + nRefPitch)));
          src1 = _mm_unpacklo_epi8(src1, _mm_loadl_epi64(reinterpret_cast<const __m128i*>(pSrc + x + nSrcPitch)));
        }
        auto sad1 = _mm_sad_epu8(dst1, src1);
        acc = _mm_add_epi32(acc, sad1);

        if constexpr (vert_inc >= 4) {
          // unroll 4
          dst1 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(pRef + x + nRefPitch * 2));
          src1 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(pSrc + x + nSrcPitch * 2));
          dst1 = _mm_unpacklo_epi8(dst1, _mm_loadl_epi64(reinterpret_cast<const __m128i*>(pRef + x + nRefPitch * 3)));
          src1 = _mm_unpacklo_epi8(src1, _mm_loadl_epi64(reinterpret_cast<const __m128i*>(pSrc + x + nSrcPitch * 3)));
          sad1 = _mm_sad_epu8(dst1, src1);
          acc = _mm_add_epi32(acc, sad1);
        }

        if constexpr (vert_inc >= 8) {
          // unroll 8
          dst1 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(pRef + x + nRefPitch * 4));
          src1 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(pSrc + x + nSrcPitch * 4));
          dst1 = _mm_unpacklo_epi8(dst1, _mm_loadl_epi64(reinterpret_cast<const __m128i*>(pRef + x + nRefPitch * 5)));
          src1 = _mm_unpacklo_epi8(src1, _mm_loadl_epi64(reinterpret_cast<const __m128i*>(pSrc + x + nSrcPitch * 5)));
          sad1 = _mm_sad_epu8(dst1, src1);
          acc = _mm_add_epi32(acc, sad1);

          dst1 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(pRef + x + nRefPitch * 6));
          src1 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(pSrc + x + nSrcPitch * 6));
          dst1 = _mm_unpacklo_epi8(dst1, _mm_loadl_epi64(reinterpret_cast<const __m128i*>(pRef + x + nRefPitch * 7)));
          src1 = _mm_unpacklo_epi8(src1, _mm_loadl_epi64(reinterpret_cast<const __m128i*>(pSrc + x + nSrcPitch * 7)));
          sad1 = _mm_sad_epu8(dst1, src1);
          acc = _mm_add_epi32(acc, sad1);
        }
      }
    }
    if constexpr (nBlkWidth % 8 != 0 && nBlkWidth % 4 == 0) {
      for (int x = nBlkWidth / 8 * 8; x < nBlkWidth / 4 * 4; x += 4) {
        auto dst1 = _mm_cvtsi32_si128(*reinterpret_cast<const uint32_t*>(pRef + x));
        auto src1 = _mm_cvtsi32_si128(*reinterpret_cast<const uint32_t*>(pSrc + x));
        if constexpr (vert_inc >= 2) {
          dst1 = _mm_unpacklo_epi8(dst1, _mm_cvtsi32_si128(*reinterpret_cast<const uint32_t*>(pRef + x + nRefPitch)));
          src1 = _mm_unpacklo_epi8(src1, _mm_cvtsi32_si128(*reinterpret_cast<const uint32_t*>(pSrc + x + nSrcPitch)));
        }
        auto sad1 = _mm_sad_epu8(dst1, src1);
        acc = _mm_add_epi32(acc, sad1);

        if constexpr (vert_inc >= 4) {
          dst1 = _mm_cvtsi32_si128(*reinterpret_cast<const uint32_t*>(pRef + x + nRefPitch * 2));
          src1 = _mm_cvtsi32_si128(*reinterpret_cast<const uint32_t*>(pSrc + x + nSrcPitch * 2));
          dst1 = _mm_unpacklo_epi8(dst1, _mm_cvtsi32_si128(*reinterpret_cast<const uint32_t*>(pRef + x + nRefPitch * 3)));
          src1 = _mm_unpacklo_epi8(src1, _mm_cvtsi32_si128(*reinterpret_cast<const uint32_t*>(pSrc + x + nSrcPitch * 3)));
          sad1 = _mm_sad_epu8(dst1, src1);
          acc = _mm_add_epi32(acc, sad1);
        }
      }
    }
    if constexpr (nBlkWidth % 4 != 0 && nBlkWidth % 2 == 0) {
      for (int x = nBlkWidth / 4 * 4; x < nBlkWidth / 2 * 2; x += 2) {
        auto dst1 = _mm_cvtsi32_si128(*reinterpret_cast<const uint16_t*>(pRef + x));
        auto src1 = _mm_cvtsi32_si128(*reinterpret_cast<const uint16_t*>(pSrc + x));
        auto sad1 = _mm_sad_epu8(dst1, src1);
        acc = _mm_add_epi32(acc, sad1);
        if constexpr (vert_inc >= 2) {
          dst1 = _mm_cvtsi32_si128(*reinterpret_cast<const uint16_t*>(pRef + x + nRefPitch));
          src1 = _mm_cvtsi32_si128(*reinterpret_cast<const uint16_t*>(pSrc + x + nSrcPitch));
          sad1 = _mm_sad_epu8(dst1, src1);
          acc = _mm_add_epi32(acc, sad1);
        }
        if constexpr (vert_inc >= 4) {
          dst1 = _mm_cvtsi32_si128(*reinterpret_cast<const uint16_t*>(pRef + x + nRefPitch * 2));
          src1 = _mm_cvtsi32_si128(*reinterpret_cast<const uint16_t*>(pSrc + x + nSrcPitch * 2));
          sad1 = _mm_sad_epu8(dst1, src1);
          acc = _mm_add_epi32(acc, sad1);
          dst1 = _mm_cvtsi32_si128(*reinterpret_cast<const uint16_t*>(pRef + x + nRefPitch * 3));
          src1 = _mm_cvtsi32_si128(*reinterpret_cast<const uint16_t*>(pSrc + x + nSrcPitch * 3));
          sad1 = _mm_sad_epu8(dst1, src1);
          acc = _mm_add_epi32(acc, sad1);
        }
      }
    }
    pRef += nRefPitch * vert_inc;
    pSrc += nSrcPitch * vert_inc;
  }
  __m128i upper = _mm_castps_si128(_mm_movehl_ps(_mm_setzero_ps(), _mm_castsi128_ps(acc)));
  acc = _mm_add_epi32(acc, upper);
  auto result = _mm_cvtsi128_si32(acc);
  return result;
}
#endif

 // SAD routines are same for SADFunctions.h and SADFunctions_avx.cpp
 // put it into a common h
#include "SADFunctions16.h"

SADFunction* get_sad_function(int BlockX, int BlockY, int bits_per_pixel, arch_t arch);
SADFunction* get_satd_function(int BlockX, int BlockY, int pixelsize, arch_t arch);

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

#ifdef USE_SAD_ASM
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
#endif
// 1.9.5.3: added ssd & SATD (TSchniede)
// alternative to SAD - SSD: squared sum of differences, VERY sensitive to noise
// ssd did not go live. sample: x264_pixel_ssd_16x16_mmx

#ifdef USE_SAD_ASM
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
#endif


#endif
