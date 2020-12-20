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

#include <emmintrin.h> // SSE2
#include <pmmintrin.h> // SSE3
#include <tmmintrin.h> // SSSE3
#include <smmintrin.h> // SSE4

#include <stdint.h>
#include "include/avs/config.h"


template<int nBlkHeight>
#if defined(GCC) || defined(CLANG)
__attribute__((__target__("ssse3")))
#endif 
unsigned int Sad10_ssse3_4xN(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch)
{
  // 10 bit version
  // a uint16 can hold maximum sum of 64 differences. We are good till 4x128 (but max is 64 atm)
  assert(nBlkHeight <= 128);
  __m128i zero = _mm_setzero_si128();
  __m128i sumw = _mm_setzero_si128(); // word sized sums

  constexpr int vert_inc = (nBlkHeight % 2) == 0 ? 2 : 1;

  for (int y = 0; y < nBlkHeight; y += vert_inc)
  {
    // 1st row 4 pixels 8 bytes
    if constexpr(vert_inc == 1) {
      auto src1 = _mm_loadl_epi64((__m128i *) (pSrc));
      auto src2 = _mm_loadl_epi64((__m128i *) (pRef));
      __m128i absdiff = _mm_abs_epi16(_mm_sub_epi16(src1, src2));
      sumw = _mm_add_epi16(sumw, absdiff);
    }

    if constexpr(vert_inc >= 2) {
      auto src1 = _mm_loadl_epi64((__m128i *) (pSrc));
      auto src2 = _mm_loadl_epi64((__m128i *) (pRef));
      // 2nd row 4 pixels 8 bytes
      auto src1_next = _mm_loadl_epi64((__m128i *) (pSrc + nSrcPitch * 1));
      auto src2_next = _mm_loadl_epi64((__m128i *) (pRef + nRefPitch * 1));
      __m128i absdiff = _mm_abs_epi16(_mm_sub_epi16(_mm_unpacklo_epi16(src1, src1_next), _mm_unpacklo_epi16(src2, src2_next)));
      sumw = _mm_add_epi16(sumw, absdiff);
    }
    pSrc += nSrcPitch * vert_inc;
    pRef += nRefPitch * vert_inc;
  }
  // sum up 8 words
  __m128i sum = _mm_add_epi32(_mm_unpacklo_epi16(sumw, zero), _mm_unpackhi_epi16(sumw, zero));
  // sum up 4 integers
  sum = _mm_hadd_epi32(sum, sum);
  sum = _mm_hadd_epi32(sum, sum);
  unsigned int result = _mm_cvtsi128_si32(sum);

  return result;
}

template<int nBlkHeight>
#if defined(GCC) || defined(CLANG)
__attribute__((__target__("sse2")))
#endif 
unsigned int Sad16_sse2_4xN(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch)
{
  __m128i zero = _mm_setzero_si128();
  __m128i sum = _mm_setzero_si128();

  constexpr int vert_inc = (nBlkHeight % 2) == 1 ? 1 : 2;

  for (int y = 0; y < nBlkHeight; y += vert_inc)
  {
    // 4 pixels: 8 bytes
    auto src1 = _mm_loadl_epi64((__m128i *) (pSrc));
    auto src2 = _mm_loadl_epi64((__m128i *) (pRef)); // lower 8 bytes
    if constexpr(vert_inc == 2) {
      auto src1_h = _mm_loadl_epi64((__m128i *) (pSrc + nSrcPitch));
      auto src2_h = _mm_loadl_epi64((__m128i *) (pRef + nRefPitch));
      src1 = _mm_castps_si128(_mm_movelh_ps(_mm_castsi128_ps(src1), _mm_castsi128_ps(src1_h))); // lower 64 bits from src2 low 64 bits, high 64 bits from src4b low 64 bits
      src2 = _mm_castps_si128(_mm_movelh_ps(_mm_castsi128_ps(src2), _mm_castsi128_ps(src2_h))); // lower 64 bits from src2 low 64 bits, high 64 bits from src4b low 64 bits
    }

    __m128i greater_t = _mm_subs_epu16(src1, src2); // unsigned sub with saturation
    __m128i smaller_t = _mm_subs_epu16(src2, src1);
    __m128i absdiff = _mm_or_si128(greater_t, smaller_t); //abs(s1-s2)  == (satsub(s1,s2) | satsub(s2,s1))
    sum = _mm_add_epi32(sum, _mm_unpacklo_epi16(absdiff, zero));
    if constexpr(vert_inc == 2) {
      sum = _mm_add_epi32(sum, _mm_unpackhi_epi16(absdiff, zero));
    }

    pSrc += nSrcPitch * vert_inc;
    pRef += nRefPitch * vert_inc;
  }
  // we have 4 integers for sum: a0 a1 a2 a3
  __m128i a0_a1 = _mm_unpacklo_epi32(sum, zero); // a0 0 a1 0
  __m128i a2_a3 = _mm_unpackhi_epi32(sum, zero); // a2 0 a3 0
  sum = _mm_add_epi32(a0_a1, a2_a3); // a0+a2, 0, a1+a3, 0
                                     // sum here: two 32 bit partial result: sum1 0 sum2 0
  __m128i sum_hi = _mm_castps_si128(_mm_movehl_ps(_mm_castsi128_ps(sum), _mm_castsi128_ps(sum)));
  // __m128i sum_hi  _mm_unpackhi_epi64(sum, zero); // a1 + a3. 2 dwords right 
  sum = _mm_add_epi32(sum, sum_hi);  // a0 + a2 + a1 + a3
  unsigned int result = _mm_cvtsi128_si32(sum);

  return result;
}

// min: SSSE3
template<int nBlkHeight>
#if defined(GCC) || defined(CLANG)
__attribute__((__target__("ssse3")))
#endif
unsigned int Sad10_ssse3_6xN(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch)
{
  // 10 bit version
  // a uint16 can hold maximum sum of 64 differences. We are good till 6x64
  assert(nBlkHeight <= 64);
  __m128i zero = _mm_setzero_si128();
  __m128i sumw = _mm_setzero_si128(); // word sized sums
  __m128i mask6_16bit = _mm_set_epi16(0, 0, -1, -1, -1, -1, -1, -1); // -1: 0xFFFF

  const int vert_inc = (nBlkHeight % 2) == 0 ? 2 : 1;

  for (int y = 0; y < nBlkHeight; y += vert_inc)
  {
    // 6 pixels: 12 bytes 8+4, source is aligned
    auto src1 = _mm_load_si128((__m128i *) (pSrc)); // full 16 byte safe
    src1 = _mm_and_si128(src1, mask6_16bit);

    auto src2 = _mm_loadl_epi64((__m128i *) (pRef)); // lower 8 bytes
    auto srcRest32 = _mm_load_ss(reinterpret_cast<const float *>(pRef + 8)); // upper 4 bytes
    src2 = _mm_castps_si128(_mm_movelh_ps(_mm_castsi128_ps(src2), srcRest32)); // lower 64 bits from src2 low 64 bits, high 64 bits from src4b low 64 bits

    __m128i absdiff = _mm_abs_epi16(_mm_sub_epi16(src1, src2));
    sumw = _mm_add_epi16(sumw, absdiff);

    if constexpr(vert_inc >= 2) {
      // 2nd row 6 pixels: 12 bytes 8+4, source is aligned
      src1 = _mm_load_si128((__m128i *) (pSrc + nSrcPitch * 1)); // safe to read
      src1 = _mm_and_si128(src1, mask6_16bit);

      src2 = _mm_loadl_epi64((__m128i *) (pRef + nRefPitch * 1));
      srcRest32 = _mm_load_ss(reinterpret_cast<const float *>(pRef + nRefPitch * 1 + 8));
      src2 = _mm_castps_si128(_mm_movelh_ps(_mm_castsi128_ps(src2), srcRest32)); // lower 64 bits from src2 low 64 bits, high 64 bits from src4b low 64 bits

      absdiff = _mm_abs_epi16(_mm_sub_epi16(src1, src2));
      sumw = _mm_add_epi16(sumw, absdiff);
    }
    pSrc += nSrcPitch * vert_inc;
    pRef += nRefPitch * vert_inc;
  }
  // sum up 8 words
  __m128i sum = _mm_add_epi32(_mm_unpacklo_epi16(sumw, zero), _mm_unpackhi_epi16(sumw, zero));
  // sum up 4 integers
  sum = _mm_hadd_epi32(sum, sum);
  sum = _mm_hadd_epi32(sum, sum);
  unsigned int result = _mm_cvtsi128_si32(sum);

  return result;
}

template<int nBlkHeight>
#if defined(GCC) || defined(CLANG)
__attribute__((__target__("sse2")))
#endif
unsigned int Sad16_sse2_6xN(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch)
{
  __m128i zero = _mm_setzero_si128();
  __m128i sum = _mm_setzero_si128();
  __m128i mask6_16bit = _mm_set_epi16(0, 0, -1, -1, -1, -1, -1, -1); // -1: 0xFFFF

  for (int y = 0; y < nBlkHeight; y++)
  {
    // 6 pixels: 12 bytes 8+4, source is aligned
    auto src1 = _mm_load_si128((__m128i *) (pSrc));
    src1 = _mm_and_si128(src1, mask6_16bit);

    auto src2 = _mm_loadl_epi64((__m128i *) (pRef)); // lower 8 bytes
    auto srcRest32 = _mm_load_ss(reinterpret_cast<const float *>(pRef + 8)); // upper 4 bytes
    src2 = _mm_castps_si128(_mm_movelh_ps(_mm_castsi128_ps(src2), srcRest32)); // lower 64 bits from src2 low 64 bits, high 64 bits from src4b low 64 bits

    __m128i greater_t = _mm_subs_epu16(src1, src2); // unsigned sub with saturation
    __m128i smaller_t = _mm_subs_epu16(src2, src1);
    __m128i absdiff = _mm_or_si128(greater_t, smaller_t); //abs(s1-s2)  == (satsub(s1,s2) | satsub(s2,s1))
    sum = _mm_add_epi32(sum, _mm_unpacklo_epi16(absdiff, zero));
    sum = _mm_add_epi32(sum, _mm_unpackhi_epi16(absdiff, zero));

    pSrc += nSrcPitch;
    pRef += nRefPitch;
  }
  // at 16 bits: we have 4 integers for sum: a0 a1 a2 a3
  __m128i a0_a1 = _mm_unpacklo_epi32(sum, zero); // a0 0 a1 0
  __m128i a2_a3 = _mm_unpackhi_epi32(sum, zero); // a2 0 a3 0
  sum = _mm_add_epi32(a0_a1, a2_a3); // a0+a2, 0, a1+a3, 0
                                     // sum here: two 32 bit partial result: sum1 0 sum2 0
  __m128i sum_hi = _mm_castps_si128(_mm_movehl_ps(_mm_castsi128_ps(sum), _mm_castsi128_ps(sum)));
  // __m128i sum_hi  _mm_unpackhi_epi64(sum, zero); // a1 + a3. 2 dwords right 
  sum = _mm_add_epi32(sum, sum_hi);  // a0 + a2 + a1 + a3
  unsigned int result = _mm_cvtsi128_si32(sum);

  return result;
}



// min: SSSE3
template<int nBlkHeight>
#if defined(GCC) || defined(CLANG)
__attribute__((__target__("ssse3")))
#endif 
unsigned int Sad10_ssse3_8xN(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch)
{
  // 10 bit version
  // a uint16 can hold maximum sum of 64 differences. We are good till 8x64
  assert(nBlkHeight <= 64);
  __m128i zero = _mm_setzero_si128();
  __m128i sumw = _mm_setzero_si128(); // word sized sums

  const int vert_inc = (nBlkHeight % 2) == 0 ? 2 : 1;
  // looking at the VS2017 asm code for 8x8, unroll 4 is slower due to saving and restoring extra 3 registers used for indexing
  // we stick at unroll 2 at most

  for (int y = 0; y < nBlkHeight; y += vert_inc)
  {
    // 1st row 8 pixels 16 bytes
    auto src1 = _mm_load_si128((__m128i *) (pSrc));
    auto src2 = _mm_loadu_si128((__m128i *) (pRef));
    __m128i absdiff = _mm_abs_epi16(_mm_sub_epi16(src1, src2));
    sumw = _mm_add_epi16(sumw, absdiff);

    if constexpr(vert_inc >= 2) {
      // 2nd row 8 pixels 16 bytes
      src1 = _mm_load_si128((__m128i *) (pSrc + nSrcPitch * 1));
      src2 = _mm_loadu_si128((__m128i *) (pRef + nRefPitch * 1));
      absdiff = _mm_abs_epi16(_mm_sub_epi16(src1, src2));
      sumw = _mm_add_epi16(sumw, absdiff);
    }
    pSrc += nSrcPitch * vert_inc;
    pRef += nRefPitch * vert_inc;
  }
  // sum up 8 words
  __m128i sum = _mm_add_epi32(_mm_unpacklo_epi16(sumw, zero), _mm_unpackhi_epi16(sumw, zero));
  // sum up 4 integers
  sum = _mm_hadd_epi32(sum, sum);
  sum = _mm_hadd_epi32(sum, sum);
  unsigned int result = _mm_cvtsi128_si32(sum);

  return result;
}


template<int nBlkHeight>
#if defined(GCC) || defined(CLANG)
__attribute__((__target__("sse2")))
#endif
unsigned int Sad16_sse2_8xN(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch)
{
  __m128i zero = _mm_setzero_si128();
  __m128i sum = _mm_setzero_si128();

  const int vert_inc = (nBlkHeight % 2) == 1 ? 1 : 2;

  for (int y = 0; y < nBlkHeight; y += vert_inc)
  {
    // 1st row 8 pixels 16 bytes
    auto src1 = _mm_load_si128((__m128i *) (pSrc));
    auto src2 = _mm_loadu_si128((__m128i *) (pRef));
    __m128i greater_t = _mm_subs_epu16(src1, src2); // unsigned sub with saturation
    __m128i smaller_t = _mm_subs_epu16(src2, src1);
    __m128i absdiff = _mm_or_si128(greater_t, smaller_t); //abs(s1-s2)  == (satsub(s1,s2) | satsub(s2,s1))
                                                          // 8 x uint16 absolute differences
    sum = _mm_add_epi32(sum, _mm_unpacklo_epi16(absdiff, zero));
    sum = _mm_add_epi32(sum, _mm_unpackhi_epi16(absdiff, zero));

    if constexpr(vert_inc == 2) {
      // 2nd row 8 pixels 16 bytes
      src1 = _mm_load_si128((__m128i *) (pSrc + nSrcPitch * 1));
      src2 = _mm_loadu_si128((__m128i *) (pRef + nRefPitch * 1));
      greater_t = _mm_subs_epu16(src1, src2); // unsigned sub with saturation
      smaller_t = _mm_subs_epu16(src2, src1);
      absdiff = _mm_or_si128(greater_t, smaller_t); //abs(s1-s2)  == (satsub(s1,s2) | satsub(s2,s1))
      sum = _mm_add_epi32(sum, _mm_unpacklo_epi16(absdiff, zero));
      sum = _mm_add_epi32(sum, _mm_unpackhi_epi16(absdiff, zero));
    }
    pSrc += nSrcPitch * vert_inc;
    pRef += nRefPitch * vert_inc;
  }
  // at 16 bits: we have 4 integers for sum: a0 a1 a2 a3
  __m128i a0_a1 = _mm_unpacklo_epi32(sum, zero); // a0 0 a1 0
  __m128i a2_a3 = _mm_unpackhi_epi32(sum, zero); // a2 0 a3 0
  sum = _mm_add_epi32(a0_a1, a2_a3); // a0+a2, 0, a1+a3, 0
                                     // sum here: two 32 bit partial result: sum1 0 sum2 0
  __m128i sum_hi = _mm_castps_si128(_mm_movehl_ps(_mm_castsi128_ps(sum), _mm_castsi128_ps(sum)));
  // __m128i sum_hi  _mm_unpackhi_epi64(sum, zero); // a1 + a3. 2 dwords right 
  sum = _mm_add_epi32(sum, sum_hi);  // a0 + a2 + a1 + a3
  unsigned int result = _mm_cvtsi128_si32(sum);

  return result;
}

// min: SSSE3
template<int nBlkHeight>
#if defined(GCC) || defined(CLANG)
__attribute__((__target__("ssse3")))
#endif
unsigned int Sad10_ssse3_12xN(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch)
{
  // 10 bit version
  // a uint16 can hold maximum sum of 64 differences. We are good till 16x32
  assert(nBlkHeight <= 64);
  __m128i zero = _mm_setzero_si128();
  __m128i sum = _mm_setzero_si128(); // outer sum

  constexpr int vert_inc = (nBlkHeight % 2) == 0 ? 2 : 1;

  // over a nBlkHeight we sum up after each 8 height (limit would be a bit less, this is the rule for blkSize==16)
  // 12x16, 12x24, 12x48
  constexpr int lane_count = 2; // though we use oly half of the 2nd lane
  constexpr int blkHeight_max_safe = 64 / lane_count;
  constexpr int blkHeight_safe = (blkHeight_max_safe >= nBlkHeight) ? nBlkHeight :
    (nBlkHeight % 16 == 0 && blkHeight_max_safe >= 16) ? 16 :
    (nBlkHeight % 8 == 0 && blkHeight_max_safe >= 8) ? 8 :
    (nBlkHeight % 4 == 0 && blkHeight_max_safe >= 4) ? 4 : 2;
  constexpr int blkHeight_outer = nBlkHeight / blkHeight_safe; // if safe
  assert(blkHeight_outer * blkHeight_safe == nBlkHeight);

  for (int y_outer = 0; y_outer < blkHeight_outer; y_outer++) {
    __m128i sumw = _mm_setzero_si128(); // word sized sums
    for (int y = 0; y < blkHeight_safe; y += vert_inc)
    {
      // BlkSizeX==12: 8+4 pixels, 16+8 bytes
      auto src1 = _mm_load_si128((__m128i *) (pSrc));
      auto src2 = _mm_loadu_si128((__m128i *) (pRef));
      __m128i absdiff = _mm_abs_epi16(_mm_sub_epi16(src1, src2));
      sumw = _mm_add_epi16(sumw, absdiff);
      // 2nd 4 pixels
      src1 = _mm_loadl_epi64((__m128i *) (pSrc + 16));
      src2 = _mm_loadl_epi64((__m128i *) (pRef + 16));
      absdiff = _mm_abs_epi16(_mm_sub_epi16(src1, src2));
      sumw = _mm_add_epi16(sumw, absdiff);

      if constexpr(vert_inc >= 2) {
        // 2nd row 8 pixels 16 bytes
        src1 = _mm_load_si128((__m128i *) (pSrc + nSrcPitch * 1));
        src2 = _mm_loadu_si128((__m128i *) (pRef + nRefPitch * 1));
        absdiff = _mm_abs_epi16(_mm_sub_epi16(src1, src2));
        sumw = _mm_add_epi16(sumw, absdiff);

        src1 = _mm_loadl_epi64((__m128i *) (pSrc + nSrcPitch * 1 + 16));
        src2 = _mm_loadl_epi64((__m128i *) (pRef + nRefPitch * 1 + 16));
        absdiff = _mm_abs_epi16(_mm_sub_epi16(src1, src2));
        sumw = _mm_add_epi16(sumw, absdiff);
      }
      pSrc += nSrcPitch * vert_inc;
      pRef += nRefPitch * vert_inc;
    }
    // sum up 8 words
    sum = _mm_add_epi32(sum, _mm_add_epi32(_mm_unpacklo_epi16(sumw, zero), _mm_unpackhi_epi16(sumw, zero)));
  }
  // sum up 4 integers
  sum = _mm_hadd_epi32(sum, sum);
  sum = _mm_hadd_epi32(sum, sum);
  unsigned int result = _mm_cvtsi128_si32(sum);

  return result;
}

template<int nBlkHeight>
#if defined(GCC) || defined(CLANG)
__attribute__((__target__("sse2")))
#endif
unsigned int Sad16_sse2_12xN(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch)
{
  __m128i zero = _mm_setzero_si128();
  __m128i sum = _mm_setzero_si128(); // 2x or 4x int is probably enough for 32x32

  for (int y = 0; y < nBlkHeight; y += 1)
  {
    // BlkSizeX==12: 8+4 pixels, 16+8 bytes
    auto src1 = _mm_load_si128((__m128i *) (pSrc));
    auto src2 = _mm_loadu_si128((__m128i *) (pRef));
    __m128i greater_t = _mm_subs_epu16(src1, src2); // unsigned sub with saturation
    __m128i smaller_t = _mm_subs_epu16(src2, src1);
    __m128i absdiff = _mm_or_si128(greater_t, smaller_t); //abs(s1-s2)  == (satsub(s1,s2) | satsub(s2,s1))
                                                          // 8 x uint16 absolute differences
    sum = _mm_add_epi32(sum, _mm_unpacklo_epi16(absdiff, zero));
    sum = _mm_add_epi32(sum, _mm_unpackhi_epi16(absdiff, zero));
    // 2nd 4 pixels
    src1 = _mm_loadl_epi64((__m128i *) (pSrc + 16)); // zeros upper 64 bits
    src2 = _mm_loadl_epi64((__m128i *) (pRef + 16));
    greater_t = _mm_subs_epu16(src1, src2); // unsigned sub with saturation
    smaller_t = _mm_subs_epu16(src2, src1);
    absdiff = _mm_or_si128(greater_t, smaller_t); //abs(s1-s2)  == (satsub(s1,s2) | satsub(s2,s1))
    sum = _mm_add_epi32(sum, _mm_unpacklo_epi16(absdiff, zero));
    sum = _mm_add_epi32(sum, _mm_unpackhi_epi16(absdiff, zero));
    // sum1_32, sum2_32, sum3_32, sum4_32
    pSrc += nSrcPitch;
    pRef += nRefPitch;
  }
  // at 16 bits: we have 4 integers for sum: a0 a1 a2 a3
  __m128i a0_a1 = _mm_unpacklo_epi32(sum, zero); // a0 0 a1 0
  __m128i a2_a3 = _mm_unpackhi_epi32(sum, zero); // a2 0 a3 0
  sum = _mm_add_epi32(a0_a1, a2_a3); // a0+a2, 0, a1+a3, 0
                                     // sum here: two 32 bit partial result: sum1 0 sum2 0
  __m128i sum_hi = _mm_castps_si128(_mm_movehl_ps(_mm_castsi128_ps(sum), _mm_castsi128_ps(sum)));
  // __m128i sum_hi  _mm_unpackhi_epi64(sum, zero); // a1 + a3. 2 dwords right 
  sum = _mm_add_epi32(sum, sum_hi);  // a0 + a2 + a1 + a3

  unsigned int result = _mm_cvtsi128_si32(sum);

  return result;
}

// min: SSSE3
template<int nBlkHeight>
#if defined(GCC) || defined(CLANG)
__attribute__((__target__("ssse3")))
#endif
unsigned int Sad10_ssse3_16xN(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch)
{
  // 10 bit version
  // a uint16 can hold maximum sum of 64 differences. We are good till 16x32
  assert(nBlkHeight <= 64);
  __m128i zero = _mm_setzero_si128();
  __m128i sum = _mm_setzero_si128(); // outer sum

  constexpr int vert_inc = (nBlkHeight % 2) == 0 ? 2 : 1;

  constexpr int lane_count = 2; // 16 pixels
  constexpr int blkHeight_max_safe = 64 / lane_count;
  constexpr int blkHeight_safe = (blkHeight_max_safe >= nBlkHeight) ? nBlkHeight :
    (nBlkHeight % 16 == 0 && blkHeight_max_safe >= 16) ? 16 :
    (nBlkHeight % 8 == 0 && blkHeight_max_safe >= 8) ? 8 :
    (nBlkHeight % 4 == 0 && blkHeight_max_safe >= 4) ? 4 : 2;
  constexpr int blkHeight_outer = nBlkHeight / blkHeight_safe; // if safe
  assert(blkHeight_outer * blkHeight_safe == nBlkHeight);

  for (int y_outer = 0; y_outer < blkHeight_outer; y_outer++) {
    __m128i sumw = _mm_setzero_si128(); // word sized sums
    for (int y = 0; y < blkHeight_safe; y += vert_inc)
    {
      // 1st row 8 pixels 16 bytes
      auto src1 = _mm_load_si128((__m128i *) (pSrc));
      auto src2 = _mm_loadu_si128((__m128i *) (pRef));
      __m128i absdiff = _mm_abs_epi16(_mm_sub_epi16(src1, src2));
      sumw = _mm_add_epi16(sumw, absdiff);

      src1 = _mm_load_si128((__m128i *) (pSrc + 16));
      src2 = _mm_loadu_si128((__m128i *) (pRef + 16));
      absdiff = _mm_abs_epi16(_mm_sub_epi16(src1, src2));
      sumw = _mm_add_epi16(sumw, absdiff);

      if constexpr(vert_inc >= 2) {
        // 2nd row 8 pixels 16 bytes
        src1 = _mm_load_si128((__m128i *) (pSrc + nSrcPitch * 1));
        src2 = _mm_loadu_si128((__m128i *) (pRef + nRefPitch * 1));
        absdiff = _mm_abs_epi16(_mm_sub_epi16(src1, src2));
        sumw = _mm_add_epi16(sumw, absdiff);

        src1 = _mm_load_si128((__m128i *) (pSrc + nSrcPitch * 1 + 16));
        src2 = _mm_loadu_si128((__m128i *) (pRef + nRefPitch * 1 + 16));
        absdiff = _mm_abs_epi16(_mm_sub_epi16(src1, src2));
        sumw = _mm_add_epi16(sumw, absdiff);
      }
      pSrc += nSrcPitch * vert_inc;
      pRef += nRefPitch * vert_inc;
    }
    // sum up 8 words
    sum = _mm_add_epi32(sum, _mm_add_epi32(_mm_unpacklo_epi16(sumw, zero), _mm_unpackhi_epi16(sumw, zero)));
  }
  // sum up 4 integers
  sum = _mm_hadd_epi32(sum, sum);
  sum = _mm_hadd_epi32(sum, sum);
  unsigned int result = _mm_cvtsi128_si32(sum);

  return result;
}


template<int nBlkHeight>
#if defined(GCC) || defined(CLANG)
__attribute__((__target__("sse2")))
#endif
unsigned int Sad16_sse2_16xN(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch)
{
  __m128i zero = _mm_setzero_si128();
  __m128i sum = _mm_setzero_si128();

  constexpr int vert_inc = (nBlkHeight % 2) == 0 ? 2 : 1;

  for (int y = 0; y < nBlkHeight; y += vert_inc)
  {
    // 16 pixels: 2x8 pixels = 2x16 bytes
    // 2nd 8 pixels
    auto src1 = _mm_load_si128((__m128i *) (pSrc));
    auto src2 = _mm_loadu_si128((__m128i *) (pRef));
    __m128i greater_t = _mm_subs_epu16(src1, src2); // unsigned sub with saturation
    __m128i smaller_t = _mm_subs_epu16(src2, src1);
    __m128i absdiff = _mm_or_si128(greater_t, smaller_t); //abs(s1-s2)  == (satsub(s1,s2) | satsub(s2,s1))
                                                          // 8 x uint16 absolute differences
    sum = _mm_add_epi32(sum, _mm_unpacklo_epi16(absdiff, zero));
    sum = _mm_add_epi32(sum, _mm_unpackhi_epi16(absdiff, zero));
    // 2nd 8 pixels
    src1 = _mm_load_si128((__m128i *) (pSrc + 16));
    src2 = _mm_loadu_si128((__m128i *) (pRef + 16));
    greater_t = _mm_subs_epu16(src1, src2); // unsigned sub with saturation
    smaller_t = _mm_subs_epu16(src2, src1);
    absdiff = _mm_or_si128(greater_t, smaller_t); //abs(s1-s2)  == (satsub(s1,s2) | satsub(s2,s1))
    sum = _mm_add_epi32(sum, _mm_unpacklo_epi16(absdiff, zero));
    sum = _mm_add_epi32(sum, _mm_unpackhi_epi16(absdiff, zero));
    if constexpr(vert_inc >= 2) {
      // 16 pixels: 2x8 pixels = 2x16 bytes
      // 2nd 8 pixels
      src1 = _mm_load_si128((__m128i *) (pSrc + nSrcPitch*1));
      src2 = _mm_loadu_si128((__m128i *) (pRef + nRefPitch * 1));
      greater_t = _mm_subs_epu16(src1, src2); // unsigned sub with saturation
      smaller_t = _mm_subs_epu16(src2, src1);
      absdiff = _mm_or_si128(greater_t, smaller_t); //abs(s1-s2)  == (satsub(s1,s2) | satsub(s2,s1))
                                                            // 8 x uint16 absolute differences
      sum = _mm_add_epi32(sum, _mm_unpacklo_epi16(absdiff, zero));
      sum = _mm_add_epi32(sum, _mm_unpackhi_epi16(absdiff, zero));
      // 2nd 8 pixels
      src1 = _mm_load_si128((__m128i *) (pSrc + nSrcPitch * 1 + 16));
      src2 = _mm_loadu_si128((__m128i *) (pRef + nRefPitch * 1 + 16));
      greater_t = _mm_subs_epu16(src1, src2); // unsigned sub with saturation
      smaller_t = _mm_subs_epu16(src2, src1);
      absdiff = _mm_or_si128(greater_t, smaller_t); //abs(s1-s2)  == (satsub(s1,s2) | satsub(s2,s1))
      sum = _mm_add_epi32(sum, _mm_unpacklo_epi16(absdiff, zero));
      sum = _mm_add_epi32(sum, _mm_unpackhi_epi16(absdiff, zero));
    }
    // sum1_32, sum2_32, sum3_32, sum4_32
    pSrc += nSrcPitch * vert_inc;
    pRef += nRefPitch * vert_inc;
  }
  // at 16 bits: we have 4 integers for sum: a0 a1 a2 a3
  __m128i a0_a1 = _mm_unpacklo_epi32(sum, zero); // a0 0 a1 0
  __m128i a2_a3 = _mm_unpackhi_epi32(sum, zero); // a2 0 a3 0
  sum = _mm_add_epi32(a0_a1, a2_a3); // a0+a2, 0, a1+a3, 0
                                     // sum here: two 32 bit partial result: sum1 0 sum2 0
  __m128i sum_hi = _mm_castps_si128(_mm_movehl_ps(_mm_castsi128_ps(sum), _mm_castsi128_ps(sum)));
  // __m128i sum_hi  _mm_unpackhi_epi64(sum, zero); // a1 + a3. 2 dwords right 
  sum = _mm_add_epi32(sum, sum_hi);  // a0 + a2 + a1 + a3

  unsigned int result = _mm_cvtsi128_si32(sum);

  return result;
}

// min: SSSE3
template<int nBlkHeight>
#if defined(GCC) || defined(CLANG)
__attribute__((__target__("ssse3")))
#endif 
unsigned int Sad10_ssse3_24xN(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch)
{
  // 10 bit version
  // a uint16 can hold maximum sum of 64 differences. We are good till approx 24x21 (3 lanes add together, 64/3 = 21)
  assert(nBlkHeight <= 64);
  __m128i zero = _mm_setzero_si128();
  __m128i sum = _mm_setzero_si128(); // outer sum

  constexpr int vert_inc = (nBlkHeight % 2) == 0 ? 2 : 1;

  constexpr int lane_count = 3; // 24 pixels
  constexpr int blkHeight_max_safe = 64 / lane_count;
  constexpr int blkHeight_safe = (blkHeight_max_safe >= nBlkHeight) ? nBlkHeight :
    (nBlkHeight % 16 == 0 && blkHeight_max_safe >= 16) ? 16 :
    (nBlkHeight % 8 == 0 && blkHeight_max_safe >= 8) ? 8 :
    (nBlkHeight % 4 == 0 && blkHeight_max_safe >= 4) ? 4 : 2;
  constexpr int blkHeight_outer = nBlkHeight / blkHeight_safe; // if safe
  assert(blkHeight_outer * blkHeight_safe == nBlkHeight);

  for (int y_outer = 0; y_outer < blkHeight_outer; y_outer++) {
    __m128i sumw = _mm_setzero_si128(); // word sized sums
    for (int y = 0; y < blkHeight_safe; y += vert_inc)
    {
      // 1st row 8 pixels 16 bytes
      auto src1 = _mm_load_si128((__m128i *) (pSrc));
      auto src2 = _mm_loadu_si128((__m128i *) (pRef));
      __m128i absdiff = _mm_abs_epi16(_mm_sub_epi16(src1, src2));
      sumw = _mm_add_epi16(sumw, absdiff);
      // 2nd group
      src1 = _mm_load_si128((__m128i *) (pSrc + 16));
      src2 = _mm_loadu_si128((__m128i *) (pRef + 16));
      absdiff = _mm_abs_epi16(_mm_sub_epi16(src1, src2));
      sumw = _mm_add_epi16(sumw, absdiff);
      // 3rd group
      src1 = _mm_load_si128((__m128i *) (pSrc + 32));
      src2 = _mm_loadu_si128((__m128i *) (pRef + 32));
      absdiff = _mm_abs_epi16(_mm_sub_epi16(src1, src2));
      sumw = _mm_add_epi16(sumw, absdiff);

      if constexpr(vert_inc >= 2) {
        // 2nd row 8 pixels 16 bytes
        src1 = _mm_load_si128((__m128i *) (pSrc + nSrcPitch * 1));
        src2 = _mm_loadu_si128((__m128i *) (pRef + nRefPitch * 1));
        absdiff = _mm_abs_epi16(_mm_sub_epi16(src1, src2));
        sumw = _mm_add_epi16(sumw, absdiff);
        // 2nd group
        src1 = _mm_load_si128((__m128i *) (pSrc + nSrcPitch * 1 + 16));
        src2 = _mm_loadu_si128((__m128i *) (pRef + nRefPitch * 1 + 16));
        absdiff = _mm_abs_epi16(_mm_sub_epi16(src1, src2));
        sumw = _mm_add_epi16(sumw, absdiff);
        // 3rd group
        src1 = _mm_load_si128((__m128i *) (pSrc + nSrcPitch * 1 + 32));
        src2 = _mm_loadu_si128((__m128i *) (pRef + nRefPitch * 1 + 32));
        absdiff = _mm_abs_epi16(_mm_sub_epi16(src1, src2));
        sumw = _mm_add_epi16(sumw, absdiff);
      }
      pSrc += nSrcPitch * vert_inc;
      pRef += nRefPitch * vert_inc;
    }
    // sum up 8 words
    sum = _mm_add_epi32(sum, _mm_add_epi32(_mm_unpacklo_epi16(sumw, zero), _mm_unpackhi_epi16(sumw, zero)));
  }
  // sum up 4 integers
  sum = _mm_hadd_epi32(sum, sum);
  sum = _mm_hadd_epi32(sum, sum);
  unsigned int result = _mm_cvtsi128_si32(sum);

  return result;
}

template<int nBlkHeight>
#if defined(GCC) || defined(CLANG)
__attribute__((__target__("sse2")))
#endif
unsigned int Sad16_sse2_24xN(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch)
{
  __m128i zero = _mm_setzero_si128();
  __m128i sum = _mm_setzero_si128();

  for (int y = 0; y < nBlkHeight; y++)
  {
    auto src1 = _mm_load_si128((__m128i *) (pSrc));
    auto src2 = _mm_loadu_si128((__m128i *) (pRef));
    __m128i greater_t = _mm_subs_epu16(src1, src2); // unsigned sub with saturation
    __m128i smaller_t = _mm_subs_epu16(src2, src1);
    __m128i absdiff = _mm_or_si128(greater_t, smaller_t); //abs(s1-s2)  == (satsub(s1,s2) | satsub(s2,s1))
                                                          // 8 x uint16 absolute differences
    sum = _mm_add_epi32(sum, _mm_unpacklo_epi16(absdiff, zero));
    sum = _mm_add_epi32(sum, _mm_unpackhi_epi16(absdiff, zero));
    // 2nd 8 pixels
    src1 = _mm_load_si128((__m128i *) (pSrc + 16));
    src2 = _mm_loadu_si128((__m128i *) (pRef + 16));
    greater_t = _mm_subs_epu16(src1, src2); // unsigned sub with saturation
    smaller_t = _mm_subs_epu16(src2, src1);
    absdiff = _mm_or_si128(greater_t, smaller_t); //abs(s1-s2)  == (satsub(s1,s2) | satsub(s2,s1))
    sum = _mm_add_epi32(sum, _mm_unpacklo_epi16(absdiff, zero));
    sum = _mm_add_epi32(sum, _mm_unpackhi_epi16(absdiff, zero));
    // 3rd 8 pixels
    src1 = _mm_load_si128((__m128i *) (pSrc + 32));
    src2 = _mm_loadu_si128((__m128i *) (pRef + 32));
    greater_t = _mm_subs_epu16(src1, src2); // unsigned sub with saturation
    smaller_t = _mm_subs_epu16(src2, src1);
    absdiff = _mm_or_si128(greater_t, smaller_t); //abs(s1-s2)  == (satsub(s1,s2) | satsub(s2,s1))
    sum = _mm_add_epi32(sum, _mm_unpacklo_epi16(absdiff, zero));
    sum = _mm_add_epi32(sum, _mm_unpackhi_epi16(absdiff, zero));
    // sum1_32, sum2_32, sum3_32, sum4_32
    pSrc += nSrcPitch;
    pRef += nRefPitch;
  }
  /*__m128i sum1 = _mm_hadd_epi32(sum, sum);
  sum = _mm_hadd_epi32(sum1, sum1);*/
  // at 16 bits: we have 4 integers for sum: a0 a1 a2 a3
  __m128i a0_a1 = _mm_unpacklo_epi32(sum, zero); // a0 0 a1 0
  __m128i a2_a3 = _mm_unpackhi_epi32(sum, zero); // a2 0 a3 0
  sum = _mm_add_epi32(a0_a1, a2_a3); // a0+a2, 0, a1+a3, 0
                                     // sum here: two 32 bit partial result: sum1 0 sum2 0
  __m128i sum_hi = _mm_castps_si128(_mm_movehl_ps(_mm_castsi128_ps(sum), _mm_castsi128_ps(sum)));
  // __m128i sum_hi  _mm_unpackhi_epi64(sum, zero); // a1 + a3. 2 dwords right 
  sum = _mm_add_epi32(sum, sum_hi);  // a0 + a2 + a1 + a3

  unsigned int result = _mm_cvtsi128_si32(sum);

  return result;
}

// min: SSSE3
template<int nBlkHeight>
#if defined(GCC) || defined(CLANG)
__attribute__((__target__("ssse3")))
#endif
unsigned int Sad10_ssse3_32xN(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch)
{
  // 10 bit version
  // a uint16 can hold maximum sum of 64 differences.
  assert(nBlkHeight <= 64);
  __m128i zero = _mm_setzero_si128();
  __m128i sum = _mm_setzero_si128(); // outer sum

  constexpr int vert_inc = (nBlkHeight % 2) == 0 ? 2 : 1;

  constexpr int lane_count = 4; // 32 pixels
  constexpr int blkHeight_max_safe = 64 / lane_count;
  constexpr int blkHeight_safe = (blkHeight_max_safe >= nBlkHeight) ? nBlkHeight :
    (nBlkHeight % 16 == 0 && blkHeight_max_safe >= 16) ? 16 :
    (nBlkHeight % 8 == 0 && blkHeight_max_safe >= 8) ? 8 :
    (nBlkHeight % 4 == 0 && blkHeight_max_safe >= 4) ? 4 : 2;
  constexpr int blkHeight_outer = nBlkHeight / blkHeight_safe; // if safe
  assert(blkHeight_outer * blkHeight_safe == nBlkHeight);

  for (int y_outer = 0; y_outer < blkHeight_outer; y_outer++) {
    __m128i sumw = _mm_setzero_si128(); // word sized sums
    for (int y = 0; y < blkHeight_safe; y += vert_inc)
    {
      // 1st row 8 pixels 16 bytes
      auto src1 = _mm_load_si128((__m128i *) (pSrc));
      auto src2 = _mm_loadu_si128((__m128i *) (pRef));
      __m128i absdiff = _mm_abs_epi16(_mm_sub_epi16(src1, src2));
      sumw = _mm_add_epi16(sumw, absdiff);
      // 2nd group
      src1 = _mm_load_si128((__m128i *) (pSrc + 16));
      src2 = _mm_loadu_si128((__m128i *) (pRef + 16));
      absdiff = _mm_abs_epi16(_mm_sub_epi16(src1, src2));
      sumw = _mm_add_epi16(sumw, absdiff);
      // 3rd group
      src1 = _mm_load_si128((__m128i *) (pSrc + 32));
      src2 = _mm_loadu_si128((__m128i *) (pRef + 32));
      absdiff = _mm_abs_epi16(_mm_sub_epi16(src1, src2));
      sumw = _mm_add_epi16(sumw, absdiff);
      // 4th group
      src1 = _mm_load_si128((__m128i *) (pSrc + 48));
      src2 = _mm_loadu_si128((__m128i *) (pRef + 48));
      absdiff = _mm_abs_epi16(_mm_sub_epi16(src1, src2));
      sumw = _mm_add_epi16(sumw, absdiff);

      if constexpr(vert_inc >= 2) {
        // 2nd row 8 pixels 16 bytes
        src1 = _mm_load_si128((__m128i *) (pSrc + nSrcPitch * 1));
        src2 = _mm_loadu_si128((__m128i *) (pRef + nRefPitch * 1));
        absdiff = _mm_abs_epi16(_mm_sub_epi16(src1, src2));
        sumw = _mm_add_epi16(sumw, absdiff);
        // 2nd group
        src1 = _mm_load_si128((__m128i *) (pSrc + nSrcPitch * 1 + 16));
        src2 = _mm_loadu_si128((__m128i *) (pRef + nRefPitch * 1 + 16));
        absdiff = _mm_abs_epi16(_mm_sub_epi16(src1, src2));
        sumw = _mm_add_epi16(sumw, absdiff);
        // 3rd group
        src1 = _mm_load_si128((__m128i *) (pSrc + nSrcPitch * 1 + 32));
        src2 = _mm_loadu_si128((__m128i *) (pRef + nSrcPitch * 1 + 32));
        absdiff = _mm_abs_epi16(_mm_sub_epi16(src1, src2));
        sumw = _mm_add_epi16(sumw, absdiff);
        // 4th group
        src1 = _mm_load_si128((__m128i *) (pSrc + nSrcPitch * 1 + 48));
        src2 = _mm_loadu_si128((__m128i *) (pRef + nSrcPitch * 1 + 48));
        absdiff = _mm_abs_epi16(_mm_sub_epi16(src1, src2));
        sumw = _mm_add_epi16(sumw, absdiff);
      }
      pSrc += nSrcPitch * vert_inc;
      pRef += nRefPitch * vert_inc;
    }
    // sum up 8 words
    sum = _mm_add_epi32(sum, _mm_add_epi32(_mm_unpacklo_epi16(sumw, zero), _mm_unpackhi_epi16(sumw, zero)));
  }
  // sum up 4 integers
  sum = _mm_hadd_epi32(sum, sum);
  sum = _mm_hadd_epi32(sum, sum);
  unsigned int result = _mm_cvtsi128_si32(sum);

  return result;
}

template<int nBlkHeight>
#if defined(GCC) || defined(CLANG)
__attribute__((__target__("sse2")))
#endif
unsigned int Sad16_sse2_32xN(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch)
{
  __m128i zero = _mm_setzero_si128();
  __m128i sum = _mm_setzero_si128();

  for (int y = 0; y < nBlkHeight; y++)
  {
    auto src1 = _mm_load_si128((__m128i *) (pSrc));
    auto src2 = _mm_loadu_si128((__m128i *) (pRef));
    __m128i greater_t = _mm_subs_epu16(src1, src2); // unsigned sub with saturation
    __m128i smaller_t = _mm_subs_epu16(src2, src1);
    __m128i absdiff = _mm_or_si128(greater_t, smaller_t); //abs(s1-s2)  == (satsub(s1,s2) | satsub(s2,s1))
    sum = _mm_add_epi32(sum, _mm_unpacklo_epi16(absdiff, zero));
    sum = _mm_add_epi32(sum, _mm_unpackhi_epi16(absdiff, zero));
    // 2nd 8
    src1 = _mm_load_si128((__m128i *) (pSrc + 16));
    src2 = _mm_loadu_si128((__m128i *) (pRef + 16));
    greater_t = _mm_subs_epu16(src1, src2); // unsigned sub with saturation
    smaller_t = _mm_subs_epu16(src2, src1);
    absdiff = _mm_or_si128(greater_t, smaller_t); //abs(s1-s2)  == (satsub(s1,s2) | satsub(s2,s1))
    sum = _mm_add_epi32(sum, _mm_unpacklo_epi16(absdiff, zero));
    sum = _mm_add_epi32(sum, _mm_unpackhi_epi16(absdiff, zero));
    // 3rd 8
    src1 = _mm_load_si128((__m128i *) (pSrc + 32));
    src2 = _mm_loadu_si128((__m128i *) (pRef + 32));
    greater_t = _mm_subs_epu16(src1, src2); // unsigned sub with saturation
    smaller_t = _mm_subs_epu16(src2, src1);
    absdiff = _mm_or_si128(greater_t, smaller_t); //abs(s1-s2)  == (satsub(s1,s2) | satsub(s2,s1))
    sum = _mm_add_epi32(sum, _mm_unpacklo_epi16(absdiff, zero));
    sum = _mm_add_epi32(sum, _mm_unpackhi_epi16(absdiff, zero));
    // 4th 8
    src1 = _mm_load_si128((__m128i *) (pSrc + 32 + 16));
    src2 = _mm_loadu_si128((__m128i *) (pRef + 32 + 16));
    greater_t = _mm_subs_epu16(src1, src2); // unsigned sub with saturation
    smaller_t = _mm_subs_epu16(src2, src1);
    absdiff = _mm_or_si128(greater_t, smaller_t); //abs(s1-s2)  == (satsub(s1,s2) | satsub(s2,s1))
    sum = _mm_add_epi32(sum, _mm_unpacklo_epi16(absdiff, zero));
    sum = _mm_add_epi32(sum, _mm_unpackhi_epi16(absdiff, zero));
    // unroll#2: not any faster. Too many XMM registers are used and prolog/epilog (saving and restoring them) takes a lot of time.
    // sum1_32, sum2_32, sum3_32, sum4_32
    pSrc += nSrcPitch;
    pRef += nRefPitch;
  }
  // at 16 bits: we have 4 integers for sum: a0 a1 a2 a3
  __m128i a0_a1 = _mm_unpacklo_epi32(sum, zero); // a0 0 a1 0
  __m128i a2_a3 = _mm_unpackhi_epi32(sum, zero); // a2 0 a3 0
  sum = _mm_add_epi32(a0_a1, a2_a3); // a0+a2, 0, a1+a3, 0
                                     // sum here: two 32 bit partial result: sum1 0 sum2 0
  __m128i sum_hi = _mm_unpackhi_epi64(sum, zero); // a1 + a3. 2 dwords right 
  sum = _mm_add_epi32(sum, sum_hi);  // a0 + a2 + a1 + a3

  unsigned int result = _mm_cvtsi128_si32(sum);

  return result;
}

// min: SSSE3
template<int nBlkHeight>
#if defined(GCC) || defined(CLANG)
__attribute__((__target__("ssse3")))
#endif
unsigned int Sad10_ssse3_48xN(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch)
{
  // 10 bit version
  // a uint16 can hold maximum sum of 64 differences.
  assert(nBlkHeight <= 64);
  __m128i zero = _mm_setzero_si128();
  __m128i sum = _mm_setzero_si128(); // outer sum

  constexpr int vert_inc = (nBlkHeight % 2) == 0 ? 2 : 1;

  constexpr int lane_count = 6; // 48 pixels
  constexpr int blkHeight_max_safe = 64 / lane_count;
  constexpr int blkHeight_safe = (blkHeight_max_safe >= nBlkHeight) ? nBlkHeight :
    (nBlkHeight % 16 == 0 && blkHeight_max_safe >= 16) ? 16 :
    (nBlkHeight % 8 == 0 && blkHeight_max_safe >= 8) ? 8 :
    (nBlkHeight % 4 == 0 && blkHeight_max_safe >= 4) ? 4 : 2;
  constexpr int blkHeight_outer = nBlkHeight / blkHeight_safe; // if safe
  assert(blkHeight_outer * blkHeight_safe == nBlkHeight);

  for (int y_outer = 0; y_outer < blkHeight_outer; y_outer++) {
    __m128i sumw = _mm_setzero_si128(); // word sized sums
    for (int y = 0; y < blkHeight_safe; y += vert_inc)
    {
      // BlockSizeX==48: 3x(8+8) pixels cycles, 3x32 bytes
      for (int x = 0; x < 48 * 2; x += 32)
      {
        // 1st row 8 pixels 16 bytes
        auto src1 = _mm_load_si128((__m128i *) (pSrc + x));
        auto src2 = _mm_loadu_si128((__m128i *) (pRef + x));
        __m128i absdiff = _mm_abs_epi16(_mm_sub_epi16(src1, src2));
        sumw = _mm_add_epi16(sumw, absdiff);
        // 2nd group
        src1 = _mm_load_si128((__m128i *) (pSrc + x + 16));
        src2 = _mm_loadu_si128((__m128i *) (pRef + x + 16));
        absdiff = _mm_abs_epi16(_mm_sub_epi16(src1, src2));
        sumw = _mm_add_epi16(sumw, absdiff);

        if constexpr(vert_inc >= 2) {
          // 2nd row 8 pixels 16 bytes
          src1 = _mm_load_si128((__m128i *) (pSrc + nSrcPitch * 1 + x));
          src2 = _mm_loadu_si128((__m128i *) (pRef + nRefPitch * 1 + x));
          __m128i absdiff = _mm_abs_epi16(_mm_sub_epi16(src1, src2));
          sumw = _mm_add_epi16(sumw, absdiff);
          // 2nd group
          src1 = _mm_load_si128((__m128i *) (pSrc + nSrcPitch * 1 + x + 16));
          src2 = _mm_loadu_si128((__m128i *) (pRef + nRefPitch * 1 + x + 16));
          absdiff = _mm_abs_epi16(_mm_sub_epi16(src1, src2));
          sumw = _mm_add_epi16(sumw, absdiff);
        }
      }
      pSrc += nSrcPitch * vert_inc;
      pRef += nRefPitch * vert_inc;
    }
    // sum up 8 words
    sum = _mm_add_epi32(sum, _mm_add_epi32(_mm_unpacklo_epi16(sumw, zero), _mm_unpackhi_epi16(sumw, zero)));
  }
  // sum up 4 integers
  sum = _mm_hadd_epi32(sum, sum);
  sum = _mm_hadd_epi32(sum, sum);
  unsigned int result = _mm_cvtsi128_si32(sum);

  return result;
}

template<int nBlkHeight>
#if defined(GCC) || defined(CLANG)
__attribute__((__target__("sse2")))
#endif
unsigned int Sad16_sse2_48xN(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch)
{
  __m128i zero = _mm_setzero_si128();
  __m128i sum = _mm_setzero_si128();

  for (int y = 0; y < nBlkHeight; y++)
  {
    // BlockSizeX==48: 3x(8+8) pixels cycles, 3x32 bytes
    for (int x = 0; x < 48 * 2; x += 32)
    {
      // 1st 8 pixels
      auto src1 = _mm_load_si128((__m128i *) (pSrc + x));
      auto src2 = _mm_loadu_si128((__m128i *) (pRef + x));
      __m128i greater_t = _mm_subs_epu16(src1, src2); // unsigned sub with saturation
      __m128i smaller_t = _mm_subs_epu16(src2, src1);
      __m128i absdiff = _mm_or_si128(greater_t, smaller_t); //abs(s1-s2)  == (satsub(s1,s2) | satsub(s2,s1))
      sum = _mm_add_epi32(sum, _mm_unpacklo_epi16(absdiff, zero));
      sum = _mm_add_epi32(sum, _mm_unpackhi_epi16(absdiff, zero));
      // 2nd 8 pixels
      src1 = _mm_load_si128((__m128i *) (pSrc + x + 16));
      src2 = _mm_loadu_si128((__m128i *) (pRef + x + 16));
      greater_t = _mm_subs_epu16(src1, src2); // unsigned sub with saturation
      smaller_t = _mm_subs_epu16(src2, src1);
      absdiff = _mm_or_si128(greater_t, smaller_t); //abs(s1-s2)  == (satsub(s1,s2) | satsub(s2,s1))
      sum = _mm_add_epi32(sum, _mm_unpacklo_epi16(absdiff, zero));
      sum = _mm_add_epi32(sum, _mm_unpackhi_epi16(absdiff, zero));
      // sum1_32, sum2_32, sum3_32, sum4_32
    }
    pSrc += nSrcPitch;
    pRef += nRefPitch;
  }
  // at 16 bits: we have 4 integers for sum: a0 a1 a2 a3
  __m128i a0_a1 = _mm_unpacklo_epi32(sum, zero); // a0 0 a1 0
  __m128i a2_a3 = _mm_unpackhi_epi32(sum, zero); // a2 0 a3 0
  sum = _mm_add_epi32(a0_a1, a2_a3); // a0+a2, 0, a1+a3, 0
                                     // sum here: two 32 bit partial result: sum1 0 sum2 0
  __m128i sum_hi = _mm_unpackhi_epi64(sum, zero); // a1 + a3. 2 dwords right 
  sum = _mm_add_epi32(sum, sum_hi);  // a0 + a2 + a1 + a3

  unsigned int result = _mm_cvtsi128_si32(sum);

  return result;
}

// min: SSSE3
template<int nBlkHeight>
#if defined(GCC) || defined(CLANG)
__attribute__((__target__("ssse3")))
#endif 
unsigned int Sad10_ssse3_64xN(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch)
{
  // 10 bit version
  // a uint16 can hold maximum sum of 64 differences.
  assert(nBlkHeight <= 64);
  __m128i zero = _mm_setzero_si128();
  __m128i sum = _mm_setzero_si128(); // outer sum

  constexpr int vert_inc = (nBlkHeight % 2) == 0 ? 2 : 1;

  constexpr int lane_count = 8; // 64 pixels
  constexpr int blkHeight_max_safe = 64 / lane_count;
  constexpr int blkHeight_safe = (blkHeight_max_safe >= nBlkHeight) ? nBlkHeight :
    (nBlkHeight % 16 == 0 && blkHeight_max_safe >= 16) ? 16 :
    (nBlkHeight % 8 == 0 && blkHeight_max_safe >= 8) ? 8 :
    (nBlkHeight % 4 == 0 && blkHeight_max_safe >= 4) ? 4 : 2;
  constexpr int blkHeight_outer = nBlkHeight / blkHeight_safe; // if safe
  assert(blkHeight_outer * blkHeight_safe == nBlkHeight);

  for (int y_outer = 0; y_outer < blkHeight_outer; y_outer++) {
    __m128i sumw = _mm_setzero_si128(); // word sized sums
    for (int y = 0; y < blkHeight_safe; y += vert_inc)
    {
      // BlockSizeX==48: 3x(8+8) pixels cycles, 3x32 bytes
      for (int x = 0; x < 48 * 2; x += 32)
      {
        // 1st row 8 pixels 16 bytes
        auto src1 = _mm_load_si128((__m128i *) (pSrc + x));
        auto src2 = _mm_loadu_si128((__m128i *) (pRef + x));
        __m128i absdiff = _mm_abs_epi16(_mm_sub_epi16(src1, src2));
        sumw = _mm_add_epi16(sumw, absdiff);
        // 2nd group
        src1 = _mm_load_si128((__m128i *) (pSrc + x + 16));
        src2 = _mm_loadu_si128((__m128i *) (pRef + x + 16));
        absdiff = _mm_abs_epi16(_mm_sub_epi16(src1, src2));
        sumw = _mm_add_epi16(sumw, absdiff);

        if constexpr(vert_inc >= 2) {
          // 2nd row 8 pixels 16 bytes
          src1 = _mm_load_si128((__m128i *) (pSrc + nSrcPitch * 1 + x));
          src2 = _mm_loadu_si128((__m128i *) (pRef + nRefPitch * 1 + x));
          absdiff = _mm_abs_epi16(_mm_sub_epi16(src1, src2));
          sumw = _mm_add_epi16(sumw, absdiff);
          // 2nd group
          src1 = _mm_load_si128((__m128i *) (pSrc + nSrcPitch * 1 + x + 16));
          src2 = _mm_loadu_si128((__m128i *) (pRef + nRefPitch * 1 + x + 16));
          absdiff = _mm_abs_epi16(_mm_sub_epi16(src1, src2));
          sumw = _mm_add_epi16(sumw, absdiff);
        }
    }
      pSrc += nSrcPitch * vert_inc;
      pRef += nRefPitch * vert_inc;
  }
    // sum up 8 words
    sum = _mm_add_epi32(sum, _mm_add_epi32(_mm_unpacklo_epi16(sumw, zero), _mm_unpackhi_epi16(sumw, zero)));
}
  // sum up 4 integers
  sum = _mm_hadd_epi32(sum, sum);
  sum = _mm_hadd_epi32(sum, sum);
  unsigned int result = _mm_cvtsi128_si32(sum);

  return result;
}

template<int nBlkHeight>
#if defined(GCC) || defined(CLANG)
__attribute__((__target__("sse2")))
#endif
unsigned int Sad16_sse2_64xN(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch)
{
  __m128i zero = _mm_setzero_si128();
  __m128i sum = _mm_setzero_si128();

  for (int y = 0; y < nBlkHeight; y++)
  {
    // BlockSizeX==64: 2x16 pixels cycles, 2x64 bytes
    for (int x = 0; x < 64 * 2; x += 32)
    {
      // 1st 8
      auto src1 = _mm_load_si128((__m128i *) (pSrc + x));
      auto src2 = _mm_loadu_si128((__m128i *) (pRef + x));
      __m128i greater_t = _mm_subs_epu16(src1, src2); // unsigned sub with saturation
      __m128i smaller_t = _mm_subs_epu16(src2, src1);
      __m128i absdiff = _mm_or_si128(greater_t, smaller_t); //abs(s1-s2)  == (satsub(s1,s2) | satsub(s2,s1))
      sum = _mm_add_epi32(sum, _mm_unpacklo_epi16(absdiff, zero));
      sum = _mm_add_epi32(sum, _mm_unpackhi_epi16(absdiff, zero));
      // 2nd 8
      src1 = _mm_load_si128((__m128i *) (pSrc + x + 16));
      src2 = _mm_loadu_si128((__m128i *) (pRef + x + 16));
      greater_t = _mm_subs_epu16(src1, src2); // unsigned sub with saturation
      smaller_t = _mm_subs_epu16(src2, src1);
      absdiff = _mm_or_si128(greater_t, smaller_t); //abs(s1-s2)  == (satsub(s1,s2) | satsub(s2,s1))
      sum = _mm_add_epi32(sum, _mm_unpacklo_epi16(absdiff, zero));
      sum = _mm_add_epi32(sum, _mm_unpackhi_epi16(absdiff, zero));
    }
    pSrc += nSrcPitch;
    pRef += nRefPitch;
  }
  /*
  __m128i sum1 = _mm_hadd_epi32(sum, sum);
  sum = _mm_hadd_epi32(sum1, sum1);
  */
  // at 16 bits: we have 4 integers for sum: a0 a1 a2 a3
  __m128i a0_a1 = _mm_unpacklo_epi32(sum, zero); // a0 0 a1 0
  __m128i a2_a3 = _mm_unpackhi_epi32(sum, zero); // a2 0 a3 0
  sum = _mm_add_epi32(a0_a1, a2_a3); // a0+a2, 0, a1+a3, 0
                                     // sum here: two 32 bit partial result: sum1 0 sum2 0
  __m128i sum_hi = _mm_unpackhi_epi64(sum, zero); // a1 + a3. 2 dwords right 
  sum = _mm_add_epi32(sum, sum_hi);  // a0 + a2 + a1 + a3

  unsigned int result = _mm_cvtsi128_si32(sum);

  return result;
}
