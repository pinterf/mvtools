#include "SADFunctions_avx2.h"
#include <map>
#include <tuple>
#include <immintrin.h>
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
  if constexpr(sizeof(pixel_t) == 2) {
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

