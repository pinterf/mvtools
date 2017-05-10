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

SADFunction* get_sad_function(int BlockX, int BlockY, int pixelsize, arch_t arch);

SADFunction* get_satd_function(int BlockX, int BlockY, int pixelsize, arch_t arch);

//x264_pixel_satd_##blksizex##x##blksizey##_sse2
/*
// add 4 floats horizontally.
// sse3 (not ssse3)
float hsum_ps_sse3(__m128 v) {
  __m128 shuf = _mm_movehdup_ps(v);        // broadcast elements 3,1 to 2,0
  __m128 sums = _mm_add_ps(v, shuf);
  shuf        = _mm_movehl_ps(shuf, sums); // high half -> low half
  sums        = _mm_add_ss(sums, shuf);
  return        _mm_cvtss_f32(sums);
  
  or ssse3
  v = _mm_hadd_ps(v, v);
  v = _mm_hadd_ps(v, v);
  or
  const __m128 t = _mm_add_ps(v, _mm_movehl_ps(v, v));
  const __m128 sum = _mm_add_ss(t, _mm_shuffle_ps(t, t, 1));
}
*/
// 170505
static unsigned int SadDummy(const uint8_t *, int , const uint8_t *, int )
{
  return 0;
}

// above width==4 for uint16_t and width==8 for uint8_t
template<int nBlkWidth, int nBlkHeight, typename pixel_t>
unsigned int Sad16_sse2(const uint8_t *pSrc, int nSrcPitch,const uint8_t *pRef, int nRefPitch)
{
#if 0
  // check result against C
  unsigned int result2 = Sad16_C<nBlkWidth, nBlkHeight, pixel_t>(pSrc, nSrcPitch, pRef, nRefPitch);
#endif

  __m128i zero = _mm_setzero_si128();
  __m128i sum = _mm_setzero_si128(); // 2x or 4x int is probably enough for 32x32
  
  const bool two_8byte_rows = (sizeof(pixel_t) == 2 && nBlkWidth <= 4) || (sizeof(pixel_t) == 1 && nBlkWidth <= 8);
  const bool one_cycle = (sizeof(pixel_t) * nBlkWidth) == 16;
  const bool unroll_by2 = !two_8byte_rows && nBlkHeight>=2; // unroll by 4: slower

    for (int y = 0; y < nBlkHeight; y += (two_8byte_rows || unroll_by2) ? 2 : 1)
    {
      if (two_8byte_rows) { // no x cycle
        __m128i src1, src2;
        // (8 bytes or 4 words) * 2 rows
#if 0
        src1 = _mm_or_si128(_mm_loadl_epi64((__m128i *) (pSrc)), _mm_slli_si128(_mm_loadl_epi64((__m128i *) (pSrc + nSrcPitch)), 8));
        src2 = _mm_or_si128(_mm_loadl_epi64((__m128i *) (pRef)), _mm_slli_si128(_mm_loadl_epi64((__m128i *) (pRef + nRefPitch)), 8));
#else
        // 16.12.01 unpack
        if (sizeof(pixel_t) == 1) {
          src1 = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i *) (pSrc)), _mm_loadl_epi64((__m128i *) (pSrc + nSrcPitch)));
          src2 = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i *) (pRef)), _mm_loadl_epi64((__m128i *) (pRef + nRefPitch)));
        }
        else if (sizeof(pixel_t) == 2) {
          src1 = _mm_unpacklo_epi16(_mm_loadl_epi64((__m128i *) (pSrc)), _mm_loadl_epi64((__m128i *) (pSrc + nSrcPitch)));
          src2 = _mm_unpacklo_epi16(_mm_loadl_epi64((__m128i *) (pRef)), _mm_loadl_epi64((__m128i *) (pRef + nRefPitch)));
        }
#endif
        if (sizeof(pixel_t) == 1) {
          // this is uint_16 specific, but will test on uint8_t against external .asm SAD functions)
          sum = _mm_add_epi32(sum, _mm_sad_epu8(src1, src2)); // yihhaaa, existing SIMD   sum1_32, 0, sum2_32, 0
                                                              // result in two 32 bit areas at the upper and lower 64 bytes
        }
        else {
          __m128i greater_t = _mm_subs_epu16(src1, src2); // unsigned sub with saturation
          __m128i smaller_t = _mm_subs_epu16(src2, src1);
          __m128i absdiff = _mm_or_si128(greater_t, smaller_t); //abs(s1-s2)  == (satsub(s1,s2) | satsub(s2,s1))
          // 8 x uint16 absolute differences
          sum = _mm_add_epi32(sum, _mm_unpacklo_epi16(absdiff, zero));
          sum = _mm_add_epi32(sum, _mm_unpackhi_epi16(absdiff, zero));
          // sum1_32, sum2_32, sum3_32, sum4_32
        }
      }
      else if (one_cycle)
      {
        __m128i src1, src2;
        src1 = _mm_load_si128((__m128i *) (pSrc)); // no x
        src2 = _mm_loadu_si128((__m128i *) (pRef));
        if (sizeof(pixel_t) == 1) {
          // this is uint_16 specific, but will test on uint8_t against external .asm SAD functions)
          sum = _mm_add_epi32(sum, _mm_sad_epu8(src1, src2)); // yihhaaa, existing SIMD   sum1_32, 0, sum2_32, 0
                                                              // result in two 32 bit areas at the upper and lower 64 bytes
        }
        else {
          __m128i greater_t = _mm_subs_epu16(src1, src2); // unsigned sub with saturation
          __m128i smaller_t = _mm_subs_epu16(src2, src1);
          __m128i absdiff = _mm_or_si128(greater_t, smaller_t); //abs(s1-s2)  == (satsub(s1,s2) | satsub(s2,s1))
                                                                // 8 x uint16 absolute differences
          sum = _mm_add_epi32(sum, _mm_unpacklo_epi16(absdiff, zero));
          sum = _mm_add_epi32(sum, _mm_unpackhi_epi16(absdiff, zero));
          // sum1_32, sum2_32, sum3_32, sum4_32
        }
        if (unroll_by2) {
          // unroll#2
          src1 = _mm_load_si128((__m128i *) (pSrc+nSrcPitch)); // no x
          src2 = _mm_loadu_si128((__m128i *) (pRef+nRefPitch));
          if (sizeof(pixel_t) == 1) {
            // this is uint_16 specific, but will test on uint8_t against external .asm SAD functions)
            sum = _mm_add_epi32(sum, _mm_sad_epu8(src1, src2)); // yihhaaa, existing SIMD   sum1_32, 0, sum2_32, 0
                                                                // result in two 32 bit areas at the upper and lower 64 bytes
          }
          else {
            __m128i greater_t = _mm_subs_epu16(src1, src2); // unsigned sub with saturation
            __m128i smaller_t = _mm_subs_epu16(src2, src1);
            __m128i absdiff = _mm_or_si128(greater_t, smaller_t); //abs(s1-s2)  == (satsub(s1,s2) | satsub(s2,s1))
                                                                  // 8 x uint16 absolute differences
            sum = _mm_add_epi32(sum, _mm_unpacklo_epi16(absdiff, zero));
            sum = _mm_add_epi32(sum, _mm_unpackhi_epi16(absdiff, zero));
            // sum1_32, sum2_32, sum3_32, sum4_32
          }
        }
      }
      else if ((nBlkWidth * sizeof(pixel_t)) % 16 == 0) {
        for (int x = 0; x < nBlkWidth * sizeof(pixel_t); x += 16)
        {
          __m128i src1, src2;
          src1 = _mm_load_si128((__m128i *) (pSrc + x));
          src2 = _mm_loadu_si128((__m128i *) (pRef + x));
          if (sizeof(pixel_t) == 1) {
            // this is uint_16 specific, but will test on uint8_t against external .asm SAD functions)
            sum = _mm_add_epi32(sum, _mm_sad_epu8(src1, src2)); // yihhaaa, existing SIMD   sum1_32, 0, sum2_32, 0
            // result in two 32 bit areas at the upper and lower 64 bytes
          }
          else {
            __m128i greater_t = _mm_subs_epu16(src1, src2); // unsigned sub with saturation
            __m128i smaller_t = _mm_subs_epu16(src2, src1);
            __m128i absdiff = _mm_or_si128(greater_t, smaller_t); //abs(s1-s2)  == (satsub(s1,s2) | satsub(s2,s1))
            // 8 x uint16 absolute differences
            sum = _mm_add_epi32(sum, _mm_unpacklo_epi16(absdiff, zero));
            sum = _mm_add_epi32(sum, _mm_unpackhi_epi16(absdiff, zero));
            // sum1_32, sum2_32, sum3_32, sum4_32
          }
          // sum += SADABS(reinterpret_cast<const pixel_t *>(pSrc)[x] - reinterpret_cast<const pixel_t *>(pRef)[x]);
          if (unroll_by2)
          {
            // unroll#2
            src1 = _mm_load_si128((__m128i *) (pSrc + nSrcPitch + x));
            src2 = _mm_loadu_si128((__m128i *) (pRef + nRefPitch + x));
            if (sizeof(pixel_t) == 1) {
              // this is uint_16 specific, but will test on uint8_t against external .asm SAD functions)
              sum = _mm_add_epi32(sum, _mm_sad_epu8(src1, src2)); // yihhaaa, existing SIMD   sum1_32, 0, sum2_32, 0
                                                                  // result in two 32 bit areas at the upper and lower 64 bytes
            }
            else {
              __m128i greater_t = _mm_subs_epu16(src1, src2); // unsigned sub with saturation
              __m128i smaller_t = _mm_subs_epu16(src2, src1);
              __m128i absdiff = _mm_or_si128(greater_t, smaller_t); //abs(s1-s2)  == (satsub(s1,s2) | satsub(s2,s1))
                                                                    // 8 x uint16 absolute differences
              sum = _mm_add_epi32(sum, _mm_unpacklo_epi16(absdiff, zero));
              sum = _mm_add_epi32(sum, _mm_unpackhi_epi16(absdiff, zero));
              // sum1_32, sum2_32, sum3_32, sum4_32
            }
          }
        }
      }
      else if ((nBlkWidth * sizeof(pixel_t)) % 8 == 0) {
        // e.g. 12x16 uint16
        for (int x = 0; x < nBlkWidth * sizeof(pixel_t); x += 8)
        {
          __m128i src1, src2;
          src1 = _mm_loadl_epi64((__m128i *) (pSrc + x));
          src2 = _mm_loadl_epi64((__m128i *) (pRef + x));
          if (sizeof(pixel_t) == 1) {
            // this is uint_16 specific, but will test on uint8_t against external .asm SAD functions)
            sum = _mm_add_epi32(sum, _mm_sad_epu8(src1, src2)); // yihhaaa, existing SIMD   sum1_32, 0, sum2_32, 0
                                                                // result in two 32 bit areas at the upper and lower 64 bytes
          }
          else {
            __m128i greater_t = _mm_subs_epu16(src1, src2); // unsigned sub with saturation
            __m128i smaller_t = _mm_subs_epu16(src2, src1);
            __m128i absdiff = _mm_or_si128(greater_t, smaller_t); //abs(s1-s2)  == (satsub(s1,s2) | satsub(s2,s1))
                                                                  // 8 x uint16 absolute differences
            sum = _mm_add_epi32(sum, _mm_unpacklo_epi16(absdiff, zero));
            // sum1_32, sum2_32, sum3_32, sum4_32
          }
          // sum += SADABS(reinterpret_cast<const pixel_t *>(pSrc)[x] - reinterpret_cast<const pixel_t *>(pRef)[x]);
          if (unroll_by2)
          {
            // unroll#2
            src1 = _mm_loadl_epi64((__m128i *) (pSrc + nSrcPitch + x));
            src2 = _mm_loadl_epi64((__m128i *) (pRef + nRefPitch + x));
            if (sizeof(pixel_t) == 1) {
              // this is uint_16 specific, but will test on uint8_t against external .asm SAD functions)
              sum = _mm_add_epi32(sum, _mm_sad_epu8(src1, src2)); // yihhaaa, existing SIMD   sum1_32, 0, sum2_32, 0
                                                                  // result in two 32 bit areas at the upper and lower 64 bytes
            }
            else {
              __m128i greater_t = _mm_subs_epu16(src1, src2); // unsigned sub with saturation
              __m128i smaller_t = _mm_subs_epu16(src2, src1);
              __m128i absdiff = _mm_or_si128(greater_t, smaller_t); //abs(s1-s2)  == (satsub(s1,s2) | satsub(s2,s1))
                                                                    // 8 x uint16 absolute differences
              sum = _mm_add_epi32(sum, _mm_unpacklo_epi16(absdiff, zero));
              // sum1_32, sum2_32, sum3_32, sum4_32
            }
          }
        }
      }
      else {
        assert(0);
      }
      if (two_8byte_rows || unroll_by2) {
        pSrc += nSrcPitch * 2;
        pRef += nRefPitch * 2;
      }
      else {
        pSrc += nSrcPitch;
        pRef += nRefPitch;
      }
    }
  /*
                                              [Low64, Hi64]
  _mm_unpacklo_epi64(_mm_setzero_si128(), x)  [0, x0]
  _mm_unpackhi_epi64(_mm_setzero_si128(), x)  [0, x1]
  _mm_move_epi64(x)                           [x0, 0]
  _mm_unpackhi_epi64(x, _mm_setzero_si128())  [x1, 0]
  */
  if(sizeof(pixel_t) == 2) {
    // at 16 bits: we have 4 integers for sum: a0 a1 a2 a3
    __m128i a0_a1 = _mm_unpacklo_epi32(sum, zero); // a0 0 a1 0
    __m128i a2_a3 = _mm_unpackhi_epi32(sum, zero); // a2 0 a3 0
    sum = _mm_add_epi32( a0_a1, a2_a3 ); // a0+a2, 0, a1+a3, 0
    // hadd: shower
  }
  // sum here: two 32 bit partial result: sum1 0 sum2 0
  __m128i sum_hi = _mm_unpackhi_epi64(sum, zero); // a1 + a3. 2 dwords right 
  sum = _mm_add_epi32(sum, sum_hi);  // a0 + a2 + a1 + a3
  
  unsigned int result = _mm_cvtsi128_si32(sum);

#if 0
  // check result against C
  if (result != result2) {
    result = result2;
  }
#endif

  return result;
} // end of SSE2 Sad16


#define MK_CFUNC(functionname) extern "C" unsigned int __cdecl functionname (const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch)

#define SAD_ISSE(blsizex, blsizey) extern "C" unsigned int __cdecl Sad##blsizex##x##blsizey##_iSSE(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch)
//Sad16x16_iSSE( x,y can be: 32 16 8 4 2
// used only for *1 or *2
SAD_ISSE(32,32);
SAD_ISSE(16,32);
SAD_ISSE(32,16);
SAD_ISSE(32,8);
SAD_ISSE(16,16);
SAD_ISSE(16,8);
SAD_ISSE(16,4);
SAD_ISSE(16,2);
SAD_ISSE(16,1);
SAD_ISSE(8,16);
SAD_ISSE(8,8);
SAD_ISSE(8,4);
SAD_ISSE(8,2);
SAD_ISSE(8,1);
SAD_ISSE(4,8);
SAD_ISSE(4,4);
SAD_ISSE(4,2);
SAD_ISSE(2,4);
SAD_ISSE(2,2);



#undef SAD_ISSE
/* included from x264/x265 */
#define SAD_x264(blsizex, blsizey, type) extern "C" unsigned int __cdecl x264_pixel_sad_##blsizex##x##blsizey##_##type##(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch)

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
SAD_x264(24, 32, sse2);

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
SAD_x264(12, 16, sse2);

// 8*x
SAD_x264(8, 32, sse2);
SAD_x264(8, 16, sse2);
//SAD_x264(8, 16, mmx2);
SAD_x264(8, 8, mmx2);
SAD_x264(8, 4, mmx2);

// 4*x
SAD_x264(4, 16, mmx2);
SAD_x264(4, 8, mmx2);
SAD_x264(4, 4, mmx2);
#undef SAD_x264
/*
//parameter is function name
MK_CFUNC(x264_pixel_sad_16x16_sse2); //non optimized cache access, for AMD?
MK_CFUNC(x264_pixel_sad_16x8_sse2);	 //non optimized cache access, for AMD?
MK_CFUNC(x264_pixel_sad_16x16_sse3); //LDDQU Pentium4E (Core1?), not for Core2!
MK_CFUNC(x264_pixel_sad_16x8_sse3);  //LDDQU Pentium4E (Core1?), not for Core2!
*/
/*MK_CFUNC(x264_pixel_sad_16x16_cache64_sse2);//core2 optimized
MK_CFUNC(x264_pixel_sad_16x8_cache64_sse2);//core2 optimized
MK_CFUNC(x264_pixel_sad_16x16_cache64_ssse3);//core2 optimized
MK_CFUNC(x264_pixel_sad_16x8_cache64_ssse3); //core2 optimized
*/

//MK_CFUNC(x264_pixel_sad_16x16_cache32_mmx2);
//MK_CFUNC(x264_pixel_sad_16x8_cache32_mmx2);
/*
MK_CFUNC(x264_pixel_sad_16x16_cache64_mmx2);
MK_CFUNC(x264_pixel_sad_16x8_cache64_mmx2);
MK_CFUNC(x264_pixel_sad_8x16_cache32_mmx2);
MK_CFUNC(x264_pixel_sad_8x8_cache32_mmx2);
MK_CFUNC(x264_pixel_sad_8x4_cache32_mmx2);
MK_CFUNC(x264_pixel_sad_8x16_cache64_mmx2);
MK_CFUNC(x264_pixel_sad_8x8_cache64_mmx2);
MK_CFUNC(x264_pixel_sad_8x4_cache64_mmx2);
*/
//1.9.5.3: added ssd & SATD (TSchniede)
/* alternative to SAD - SSD: squared sum of differences, VERY sensitive to noise */
#if 0
MK_CFUNC(x264_pixel_ssd_16x16_mmx);
MK_CFUNC(x264_pixel_ssd_16x8_mmx);
MK_CFUNC(x264_pixel_ssd_8x16_mmx);
MK_CFUNC(x264_pixel_ssd_8x8_mmx);
MK_CFUNC(x264_pixel_ssd_8x4_mmx);
MK_CFUNC(x264_pixel_ssd_4x8_mmx);
MK_CFUNC(x264_pixel_ssd_4x4_mmx);
#endif

/* SATD: Sum of Absolute Transformed Differences, more sensitive to noise, frequency domain based - replacement to dct/SAD */
#define SATD_SSE(blsizex, blsizey, type) extern "C" unsigned int __cdecl x264_pixel_satd_##blsizex##x##blsizey##_##type##(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch)
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
SATD_SSE(32, 64, sse2);
SATD_SSE(32, 32, sse2);
SATD_SSE(32, 24, sse2);
SATD_SSE(32, 16, sse2);
SATD_SSE(32,  8, sse2);
//SATD_SSE(32,  4, sse2); no such
SATD_SSE(24, 32, sse2);
SATD_SSE(16, 64, sse2);
SATD_SSE(16, 32, sse2);
SATD_SSE(16, 16, sse2);
SATD_SSE(16, 12, sse2);
SATD_SSE(16,  8, sse2);
SATD_SSE(16,  4, sse2);
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
SATD_SSE(32, 64, sse4);
SATD_SSE(32, 32, sse4);
SATD_SSE(32, 24, sse4);
SATD_SSE(32, 16, sse4);
SATD_SSE(32, 8 , sse4);
// SATD_SSE(32, 4 , sse4); no such
SATD_SSE(24, 32, sse4);
SATD_SSE(16, 64, sse4);
SATD_SSE(16, 32, sse4);
SATD_SSE(16, 16, sse4);
SATD_SSE(16, 12, sse4);
SATD_SSE(16,  8, sse4);
SATD_SSE(16,  4, sse4);
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
//SATD_SSE(64, 8, avx); no such
SATD_SSE(48, 64, avx);
SATD_SSE(32, 64, avx);
SATD_SSE(32, 32, avx);
SATD_SSE(32, 24, avx);
SATD_SSE(32, 16, avx);
SATD_SSE(32,  8, avx);
// SATD_SSE(32,  4, avx); no such
SATD_SSE(24, 32, avx);
SATD_SSE(16, 64, avx);
SATD_SSE(16, 32, avx);
SATD_SSE(16, 16, avx);
SATD_SSE(16, 12, avx);
SATD_SSE(16,  8, avx);
SATD_SSE(16,  4, avx);
SATD_SSE(12, 16, avx);
SATD_SSE( 8, 32, avx);
SATD_SSE( 8, 16, avx);
SATD_SSE( 8,  8, avx);
SATD_SSE( 8,  4, avx);

// no x4 versions
//SATD_SSE(64, 64, avx2); no such
//SATD_SSE(64, 32, avx2); no such
//SATD_SSE(64, 16, avx2); no such
//SATD_SSE(64, 8, avx2); no such
#ifdef _M_X64
SATD_SSE(32, 64, avx2);
SATD_SSE(32, 32, avx2);
SATD_SSE(32, 16, avx2);
SATD_SSE(32, 8, avx2);
#endif
#ifdef _M_X64
SATD_SSE(16, 64, avx2); // only in x64
SATD_SSE(16, 32, avx2); // only in x64
#endif
SATD_SSE(16, 16, avx2);
SATD_SSE(16,  8, avx2);
//SATD_SSE( 8, 32, avx2); no such
SATD_SSE( 8, 16, avx2);
SATD_SSE( 8,  8, avx2);

/*
SATD_SSE(32, 32, ssse3);
SATD_SSE(32, 16, ssse3);
SATD_SSE(16, 16, ssse3);
SATD_SSE(16,  8, ssse3);
SATD_SSE( 8, 16, ssse3);
SATD_SSE( 8,  8, ssse3);
SATD_SSE( 8,  4, ssse3);
*/
#undef SATD_SSE

//dummy for testing and deactivate SAD
//MK_CFUNC(SadDummy);
#undef MK_CFUNC


#endif
