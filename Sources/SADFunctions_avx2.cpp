#include "SADFunctions_avx2.h"
//#include "overlap.h"
#include <map>
#include <tuple>
#include <immintrin.h>
#include <stdint.h>
#include "def.h"

// integer 256 with SSE2-like function from AVX2
// above width==16 for uint16_t and width==32 for uint8_t
template<int nBlkWidth, int nBlkHeight, typename pixel_t>
unsigned int Sad16_avx2(const uint8_t *pSrc, int nSrcPitch,const uint8_t *pRef, int nRefPitch)
{
#if 0
  // check AVX2 result against C
  unsigned int result2 = Sad_AVX2_C<nBlkWidth, nBlkHeight, pixel_t>(pSrc, nSrcPitch, pRef, nRefPitch);
#endif

  if ((sizeof(pixel_t) == 2 && nBlkWidth < 8) || (sizeof(pixel_t) == 1 && nBlkWidth <= 16)) {
    assert("AVX2 not supported for uint16_t with BlockSize<8 or uint8_t with blocksize<16");
    return 0;
  }

  __m256i zero = _mm256_setzero_si256();
  __m256i sum = _mm256_setzero_si256(); // 2x or 4x int is probably enough for 32x32

  bool two_16byte_rows = (sizeof(pixel_t) == 2 && nBlkWidth == 8) || (sizeof(pixel_t) == 1 && nBlkWidth == 16);
  if (two_16byte_rows && nBlkHeight==1) {
    assert("AVX2 not supported BlockHeight==1 for uint16_t with BlockSize=8 or uint8_t with blocksize<16");
    return 0;
  }
  const bool one_cycle = (sizeof(pixel_t) * nBlkWidth) <= 32;
  const bool unroll_by2 = !two_16byte_rows && nBlkHeight>=2; // unroll by 4: slower

  for (int y = 0; y < nBlkHeight; y+= ((two_16byte_rows || unroll_by2) ? 2 : 1))
  {
    if (two_16byte_rows) {
      __m256i src1, src2;
      // two 16 byte rows at a time
      src1 = _mm256_loadu2_m128i((const __m128i *) (pSrc + nSrcPitch), (const __m128i *) (pSrc));
      src2 = _mm256_loadu2_m128i((const __m128i *) (pRef + nRefPitch), (const __m128i *) (pRef));
      if (sizeof(pixel_t) == 1) {
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
      if (sizeof(pixel_t) == 1) {
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
        if (sizeof(pixel_t) == 1) {
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
    else if ((nBlkWidth * sizeof(pixel_t)) % 32 == 0) {
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
        if (sizeof(pixel_t) == 1) {
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
          if (sizeof(pixel_t) == 1) {
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
    if (two_16byte_rows || unroll_by2) {
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
  __m256i a03;
  __m256i a47;
  if(sizeof(pixel_t) == 2) {
    // at 16 bits: we have 8 integers for sum: a0 a1 a2 a3 a4 a5 a6 a7
    a03 = _mm256_unpacklo_epi32(sum, zero); // a0 0 a1 0 a2 0 a3 0
    a47 = _mm256_unpackhi_epi32(sum, zero); // a4 0 a5 0 a6 0 a7 0
    a03 = _mm256_add_epi32(a03, a47); // a0+a4,0, a1+a5, 0, a2+a6, 0, a3+a7, 0
                                      // sum here: sum1 0 sum2 0 sum3 0 sum4 0, as in uint8_t case
  }
  if(sizeof(pixel_t) == 1) {
    a03 = sum;
  }

  // add the low 128 bit to the high 128 bit
  __m128i sum128 = _mm_add_epi32(_mm256_castsi256_si128(a03),_mm256_extractf128_si256(a03, 1));
  // a0+a4+a2+a6, 0, a1+a5+a3+a7, 0,

  __m128i sum_hi = _mm_unpackhi_epi64(sum128, _mm_setzero_si128()); // a1+a5+a3+a7
  sum128 = _mm_add_epi32(sum128, sum_hi); // a0+a4+a2+a6 + a1+a5+a3+a7

  unsigned int result = _mm_cvtsi128_si32(sum128);

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

