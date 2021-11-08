#if defined (__GNUC__) && ! defined (__INTEL_COMPILER)
#include <x86intrin.h>
// x86intrin.h includes header files for whatever instruction
// sets are specified on the compiler command line, such as: xopintrin.h, fma4intrin.h
#else
#include <immintrin.h> // MS version of immintrin.h covers AVX, AVX2 and FMA3
#endif // __GNUC__

#if !defined(__FMA__)
// Assume that all processors that have AVX2 also have FMA3
#if defined (__GNUC__) && ! defined (__INTEL_COMPILER) && ! defined (__INTEL_LLVM_COMPILER) && ! defined (__clang__)
// Prevent error message in g++ when using FMA intrinsics with avx2:
#pragma message "It is recommended to specify also option -mfma when using -mavx2 or higher"
#else
#define __FMA__  1
#endif
#endif
// FMA3 instruction set
#if defined (__FMA__) && (defined(__GNUC__) || defined(__clang__) || defined(__INTEL_LLVM_COMPILER))  && ! defined (__INTEL_COMPILER) 
#include <fmaintrin.h>
#endif // __FMA__ 

#ifndef _mm256_loadu2_m128i
#define _mm256_loadu2_m128i(/* __m128i const* */ hiaddr, \
                            /* __m128i const* */ loaddr) \
    _mm256_set_m128i(_mm_loadu_si128(hiaddr), _mm_loadu_si128(loaddr))
#endif

#include "SADFunctions_avx2.h"
#include <map>
#include <tuple>

#include <stdint.h>
#include "def.h"

// 64x (16-64)
// 48x (12x64)
// 32x (8-64)
// 16x (1-64)
template<int nBlkWidth, int nBlkHeight>
unsigned int Sad10_avx2(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch)
{
#if 0
  // check AVX2 result against C
  unsigned int result2 = Sad_AVX2_C<nBlkWidth, nBlkHeight, pixel_t>(pSrc, nSrcPitch, pRef, nRefPitch);
#endif

  if constexpr((nBlkWidth < 8)) {
    assert("AVX2 not supported for uint16_t with BlockSize<8");
    return 0;
  }

  __m256i zero = _mm256_setzero_si256();
  __m256i sum = _mm256_setzero_si256();

  constexpr bool two_16byte_rows = (nBlkWidth == 8);
  if constexpr(two_16byte_rows && nBlkHeight == 1) {
    assert("AVX2 not supported BlockHeight==1 for uint16_t with BlockSize=8");
    return 0;
  }
  constexpr bool one_cycle = (sizeof(uint16_t) * nBlkWidth) <= 32;
  constexpr bool unroll_by2 = !two_16byte_rows && nBlkHeight >= 2; // unroll by 4: slower

  constexpr int lane_count = (two_16byte_rows || nBlkWidth <= 16) ? 1 : (nBlkWidth / 16); // for 64 bit: 4 lanes
  // 10 bit version
  // a uint16 can hold maximum sum of 64 differences.
  // over nBlkHeight "X" we sum up after each "blkHeight_safe" height
  // one lane here: ymm register holding 16 pixels
  // 1x lane  16 pixels: 64   height ok (one difference for each pixel row) -> basically everything is ok, no width over 64 yet
  // 2x lanes 32 pixels: 32   height ok -> 32 ok, 64 not
  // 3x lanes 48 pixels: 21.3 height ok -> 12 ok, 24,48,64 not
  // 4x lanes 64 pixels: 16   height ok -> 16 ok, 32, 48 not

  constexpr int blkHeight_max_safe = 64 / lane_count;
  constexpr int blkHeight_safe = (blkHeight_max_safe >= nBlkHeight) ? nBlkHeight :
    (nBlkHeight % 16 == 0 && blkHeight_max_safe >= 16) ? 16 :
    (nBlkHeight % 8 == 0 && blkHeight_max_safe >= 8) ? 8 :
    (nBlkHeight % 4 == 0 && blkHeight_max_safe >= 4) ? 4 : 2;
  constexpr int blkHeight_outer = nBlkHeight / blkHeight_safe; // if safe
  
  assert(blkHeight_outer * blkHeight_safe == nBlkHeight);

  constexpr int vert_inc = (two_16byte_rows || unroll_by2) ? 2 : 1;
  
  for (int y_outer = 0; y_outer < blkHeight_outer; y_outer++) {
    __m256i sumw = _mm256_setzero_si256();
    for (int y = 0; y < blkHeight_safe; y += vert_inc)
    {
      if (two_16byte_rows) { // 8xN pixels
        __m256i src1, src2;
        // two 16 byte rows at a time
        src1 = _mm256_loadu2_m128i((const __m128i *) (pSrc + nSrcPitch), (const __m128i *) (pSrc));
        src2 = _mm256_loadu2_m128i((const __m128i *) (pRef + nRefPitch), (const __m128i *) (pRef));
        sumw = _mm256_add_epi16(sumw, _mm256_abs_epi16(_mm256_sub_epi16(src1, src2)));
      }
      else if (one_cycle) { // no x loop, 16xN pixels
        __m256i src1, src2;
        src1 = _mm256_loadu_si256((__m256i *) (pSrc));
        src2 = _mm256_loadu_si256((__m256i *) (pRef));
        sumw = _mm256_add_epi16(sumw, _mm256_abs_epi16(_mm256_sub_epi16(src1, src2)));
        if (unroll_by2) {
          src1 = _mm256_loadu_si256((__m256i *) (pSrc + nSrcPitch));
          src2 = _mm256_loadu_si256((__m256i *) (pRef + nRefPitch));
          sumw = _mm256_add_epi16(sumw, _mm256_abs_epi16(_mm256_sub_epi16(src1, src2)));
        }
      }
      else if constexpr((nBlkWidth * sizeof(uint16_t)) % 32 == 0) {
        // 16*2 bytes yes,
        // 24*2 bytes not supported, one 256bit and one 128bit lane
        // 32*2 bytes yes
        // 48*2 bytes yes
        // 64*2 bytes yes
        for (int x = 0; x < nBlkWidth * sizeof(uint16_t); x += 32)
        {
          __m256i src1, src2;
          src1 = _mm256_loadu_si256((__m256i *) (pSrc + x));
          src2 = _mm256_loadu_si256((__m256i *) (pRef + x));
          sumw = _mm256_add_epi16(sumw, _mm256_abs_epi16(_mm256_sub_epi16(src1, src2)));
          if (unroll_by2) {
            src1 = _mm256_loadu_si256((__m256i *) (pSrc + x + nSrcPitch));
            src2 = _mm256_loadu_si256((__m256i *) (pRef + x + nRefPitch));
            sumw = _mm256_add_epi16(sumw, _mm256_abs_epi16(_mm256_sub_epi16(src1, src2)));
          }
        }
      }
      else {
        assert(0);
      }
      pSrc += nSrcPitch * vert_inc;
      pRef += nRefPitch * vert_inc;
    }
    // sum up 8 words
    sum = _mm256_add_epi32(sum, _mm256_add_epi32(_mm256_unpacklo_epi16(sumw, zero), _mm256_unpackhi_epi16(sumw, zero)));
  }

  unsigned int result;
  sum = _mm256_hadd_epi32(sum, sum);
  sum = _mm256_hadd_epi32(sum, sum);
  __m128i sum128 = _mm_add_epi32(_mm256_castsi256_si128(sum), _mm256_extractf128_si256(sum, 1));
  result = _mm_cvtsi128_si32(sum128);

#if 0
  // check AVX2 result against C
  if (result != result2) {
    result = result2;
  }
#endif

  _mm256_zeroupper();
  /* Use VZEROUPPER to avoid the penalty of switching from AVX to SSE */

  return result;
}


// integer 256 with SSE2-like function from AVX2
// above width==16 for uint16_t and width==32 for uint8_t
template<int nBlkWidth, int nBlkHeight, typename pixel_t>
unsigned int Sad16_avx2(const uint8_t *pSrc, int nSrcPitch,const uint8_t *pRef, int nRefPitch)
{
#if 0
  // check AVX2 result against C
  unsigned int result2 = Sad_AVX2_C<nBlkWidth, nBlkHeight, pixel_t>(pSrc, nSrcPitch, pRef, nRefPitch);
#endif

  if constexpr((sizeof(pixel_t) == 2 && nBlkWidth < 8) || (sizeof(pixel_t) == 1 && nBlkWidth <= 16)) {
    assert("AVX2 not supported for uint16_t with BlockSize<8 or uint8_t with blocksize<16");
    return 0;
  }

  __m256i zero = _mm256_setzero_si256();
  __m256i sum = _mm256_setzero_si256(); // 2x or 4x int is probably enough for 32x32

  constexpr bool two_16byte_rows = (sizeof(pixel_t) == 2 && nBlkWidth == 8) || (sizeof(pixel_t) == 1 && nBlkWidth == 16);
  if constexpr(two_16byte_rows && nBlkHeight==1) {
    assert("AVX2 not supported BlockHeight==1 for uint16_t with BlockSize=8 or uint8_t with blocksize<16");
    return 0;
  }
  constexpr bool one_cycle = (sizeof(pixel_t) * nBlkWidth) <= 32;
  constexpr bool unroll_by2 = !two_16byte_rows && nBlkHeight>=2; // unroll by 4: slower

  for (int y = 0; y < nBlkHeight; y+= ((two_16byte_rows || unroll_by2) ? 2 : 1))
  {
    if (two_16byte_rows) {
      __m256i src1, src2;
      // two 16 byte rows at a time
      src1 = _mm256_loadu2_m128i((const __m128i *) (pSrc + nSrcPitch), (const __m128i *) (pSrc));
      src2 = _mm256_loadu2_m128i((const __m128i *) (pRef + nRefPitch), (const __m128i *) (pRef));
      if constexpr(sizeof(pixel_t) == 1) {
        sum = _mm256_add_epi32(sum, _mm256_sad_epu8(src1, src2));
        // result in four 32 bit areas at each 64 bytes
      }
      else {
        // we have 16 words
        __m256i greater_t = _mm256_subs_epu16(src1, src2); // unsigned sub with saturation
        __m256i smaller_t = _mm256_subs_epu16(src2, src1);
        __m256i absdiff = _mm256_or_si256(greater_t, smaller_t); //abs(s1-s2)  == (satsub(s1,s2) | satsub(s2,s1))
                                                                 // 16 x uint16 absolute differences
        sum = _mm256_add_epi32(sum, _mm256_unpacklo_epi16(absdiff, zero));
        sum = _mm256_add_epi32(sum, _mm256_unpackhi_epi16(absdiff, zero));
        // sum1_32, sum2_32, sum3_32, sum4_32 .. sum8_32
      }
    }
    else if (one_cycle) { // no x loop
      __m256i src1, src2;
      src1 = _mm256_loadu_si256((__m256i *) (pSrc));
      src2 = _mm256_loadu_si256((__m256i *) (pRef));
      if constexpr(sizeof(pixel_t) == 1) {
        sum = _mm256_add_epi32(sum, _mm256_sad_epu8(src1, src2));
        // result in four 32 bit areas at each 64 bytes
      }
      else {
        // we have 16 words
        __m256i greater_t = _mm256_subs_epu16(src1, src2); // unsigned sub with saturation
        __m256i smaller_t = _mm256_subs_epu16(src2, src1);
        __m256i absdiff = _mm256_or_si256(greater_t, smaller_t); //abs(s1-s2)  == (satsub(s1,s2) | satsub(s2,s1))
                                                                 // 16 x uint16 absolute differences
        sum = _mm256_add_epi32(sum, _mm256_unpacklo_epi16(absdiff, zero));
        sum = _mm256_add_epi32(sum, _mm256_unpackhi_epi16(absdiff, zero));
        // sum1_32, sum2_32, sum3_32, sum4_32 .. sum8_32
      }
      if (unroll_by2) {
        src1 = _mm256_loadu_si256((__m256i *) (pSrc+nSrcPitch));
        src2 = _mm256_loadu_si256((__m256i *) (pRef+nRefPitch));
        if constexpr(sizeof(pixel_t) == 1) {
          sum = _mm256_add_epi32(sum, _mm256_sad_epu8(src1, src2));
          // result in four 32 bit areas at each 64 bytes
        }
        else {
          // we have 16 words
          __m256i greater_t = _mm256_subs_epu16(src1, src2); // unsigned sub with saturation
          __m256i smaller_t = _mm256_subs_epu16(src2, src1);
          __m256i absdiff = _mm256_or_si256(greater_t, smaller_t); //abs(s1-s2)  == (satsub(s1,s2) | satsub(s2,s1))
                                                                   // 16 x uint16 absolute differences
          sum = _mm256_add_epi32(sum, _mm256_unpacklo_epi16(absdiff, zero));
          sum = _mm256_add_epi32(sum, _mm256_unpackhi_epi16(absdiff, zero));
          // sum1_32, sum2_32, sum3_32, sum4_32 .. sum8_32
        }
      }
    }
    else if constexpr((nBlkWidth * sizeof(pixel_t)) % 32 == 0) {
      // 16*2 bytes yes,
      // 24*2 bytes no
      // 32*2 bytes yes
      // 48*2 bytes yes
      // 64*2 bytes yes
      for (int x = 0; x < nBlkWidth * sizeof(pixel_t); x += 32)
      {
        __m256i src1, src2;
        src1 = _mm256_loadu_si256((__m256i *) (pSrc + x));
        src2 = _mm256_loadu_si256((__m256i *) (pRef + x));
        if constexpr(sizeof(pixel_t) == 1) {
          sum = _mm256_add_epi32(sum, _mm256_sad_epu8(src1, src2));
          // result in four 32 bit areas at each 64 bytes
        }
        else {
          // we have 16 words
          __m256i greater_t = _mm256_subs_epu16(src1, src2); // unsigned sub with saturation
          __m256i smaller_t = _mm256_subs_epu16(src2, src1);
          __m256i absdiff = _mm256_or_si256(greater_t, smaller_t); //abs(s1-s2)  == (satsub(s1,s2) | satsub(s2,s1))
                                                                   // 16 x uint16 absolute differences
          sum = _mm256_add_epi32(sum, _mm256_unpacklo_epi16(absdiff, zero));
          sum = _mm256_add_epi32(sum, _mm256_unpackhi_epi16(absdiff, zero));
          // sum1_32, sum2_32, sum3_32, sum4_32 .. sum8_32
        }
        if (unroll_by2) {
          src1 = _mm256_loadu_si256((__m256i *) (pSrc + x + nSrcPitch));
          src2 = _mm256_loadu_si256((__m256i *) (pRef + x + nRefPitch));
          if constexpr(sizeof(pixel_t) == 1) {
            sum = _mm256_add_epi32(sum, _mm256_sad_epu8(src1, src2));
            // result in four 32 bit areas at each 64 bytes
          }
          else {
            // we have 16 words
            __m256i greater_t = _mm256_subs_epu16(src1, src2); // unsigned sub with saturation
            __m256i smaller_t = _mm256_subs_epu16(src2, src1);
            __m256i absdiff = _mm256_or_si256(greater_t, smaller_t); //abs(s1-s2)  == (satsub(s1,s2) | satsub(s2,s1))
                                                                     // 16 x uint16 absolute differences
            sum = _mm256_add_epi32(sum, _mm256_unpacklo_epi16(absdiff, zero));
            sum = _mm256_add_epi32(sum, _mm256_unpackhi_epi16(absdiff, zero));
            // sum1_32, sum2_32, sum3_32, sum4_32 .. sum8_32
          }
        }
      }
    }
    else {
      assert(0);
    }
    if constexpr(two_16byte_rows || unroll_by2) {
      pSrc += nSrcPitch * 2;
      pRef += nRefPitch * 2;
    }
    else {
      pSrc += nSrcPitch;
      pRef += nRefPitch;
    }
  }

  unsigned int result;
  if constexpr (sizeof(pixel_t) == 2)
  {
    sum = _mm256_hadd_epi32(sum, sum);
    sum = _mm256_hadd_epi32(sum, sum);
    __m128i sum128 = _mm_add_epi32(_mm256_castsi256_si128(sum), _mm256_extractf128_si256(sum, 1));
    result = _mm_cvtsi128_si32(sum128);
  }
  else {
    // add the low 128 bit to the high 128 bit
    __m128i sum128 = _mm_add_epi32(_mm256_castsi256_si128(sum), _mm256_extractf128_si256(sum, 1));
    // a0+a4+a2+a6, 0, a1+a5+a3+a7, 0,

    __m128i sum_hi = _mm_unpackhi_epi64(sum128, _mm_setzero_si128()); // a1+a5+a3+a7
    sum128 = _mm_add_epi32(sum128, sum_hi); // a0+a4+a2+a6 + a1+a5+a3+a7

    result = _mm_cvtsi128_si32(sum128);
  }

#if 0
  // check AVX2 result against C
  if (result != result2) {
    result = result2;
  }
#endif

  _mm256_zeroupper();
  /* Use VZEROUPPER to avoid the penalty of switching from AVX to SSE */

  return result;
}

// Instantiate
// match with SADFunctions.cpp
#define MAKE_SAD_FN(x, y) template unsigned int Sad16_avx2<x, y, uint16_t>(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch); \
                          template unsigned int Sad10_avx2<x, y>(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch);
MAKE_SAD_FN(64, 64)
MAKE_SAD_FN(64, 48)
MAKE_SAD_FN(64, 32)
MAKE_SAD_FN(64, 16)
MAKE_SAD_FN(48, 64)
MAKE_SAD_FN(48, 48)
MAKE_SAD_FN(48, 24)
MAKE_SAD_FN(48, 12)
MAKE_SAD_FN(32, 64)
MAKE_SAD_FN(32, 32)
MAKE_SAD_FN(32, 24)
MAKE_SAD_FN(32, 16)
MAKE_SAD_FN(32, 8)
// MAKE_SAD_FN(24, 32) // 24*2 is not mod 32 bytes
MAKE_SAD_FN(16, 64)
MAKE_SAD_FN(16, 32)
MAKE_SAD_FN(16, 16)
MAKE_SAD_FN(16, 12)
MAKE_SAD_FN(16, 8)
MAKE_SAD_FN(16, 4)
MAKE_SAD_FN(16, 2)
MAKE_SAD_FN(16, 1) // 32 bytes with height=1 is OK for AVX2
//MAKE_SAD_FN(12, 16) 12*2 not mod 32 bytes
/* not optimal
MAKE_SAD_FN(8, 32)
MAKE_SAD_FN(8, 16)
MAKE_SAD_FN(8, 8)
MAKE_SAD_FN(8, 4)
MAKE_SAD_FN(8, 2)
*/
// MAKE_SAD_FN(8, 1) // 16 bytes with height=1 not supported for AVX2
//MAKE_SAD_FN(4, 8)
//MAKE_SAD_FN(4, 4)
//MAKE_SAD_FN(4, 2)
//MAKE_SAD_FN(4, 1)  // 8 bytes with height=1 not supported for SSE2
//MAKE_SAD_FN(2, 4)  // 2 pixels 4 bytes not supported with SSE2
//MAKE_SAD_FN(2, 2)
//MAKE_SAD_FN(2, 1)
#undef MAKE_SAD_FN


