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

static unsigned int SadDummy(const uint8_t *, int , const uint8_t *, int )
{
  return 0;
}

  // SAD routined are same for SADFunctions.h and SADFunctions_avx.cpp
  // todo put it into a common hpp

template<int nBlkHeight>
unsigned int Sad16_sse2_4xN_sse2(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch)
{
#ifdef __AVX__
  _mm256_zeroupper(); // diff from main sse2
#endif

  __m128i zero = _mm_setzero_si128();
  __m128i sum = _mm_setzero_si128();

  const int vert_inc = (nBlkHeight % 2) == 1 ? 1 : 2;

  for (int y = 0; y < nBlkHeight; y += vert_inc)
  {
    // 4 pixels: 8 bytes
    auto src1 = _mm_loadl_epi64((__m128i *) (pSrc));
    auto src2 = _mm_loadl_epi64((__m128i *) (pRef)); // lower 8 bytes
    if (vert_inc == 2) {
      auto src1_h = _mm_loadl_epi64((__m128i *) (pSrc + nSrcPitch));
      auto src2_h = _mm_loadl_epi64((__m128i *) (pRef + nRefPitch));
      src1 = _mm_castps_si128(_mm_movelh_ps(_mm_castsi128_ps(src1), _mm_castsi128_ps(src1_h))); // lower 64 bits from src2 low 64 bits, high 64 bits from src4b low 64 bits
      src2 = _mm_castps_si128(_mm_movelh_ps(_mm_castsi128_ps(src2), _mm_castsi128_ps(src2_h))); // lower 64 bits from src2 low 64 bits, high 64 bits from src4b low 64 bits
    }

    __m128i greater_t = _mm_subs_epu16(src1, src2); // unsigned sub with saturation
    __m128i smaller_t = _mm_subs_epu16(src2, src1);
    __m128i absdiff = _mm_or_si128(greater_t, smaller_t); //abs(s1-s2)  == (satsub(s1,s2) | satsub(s2,s1))
    sum = _mm_add_epi32(sum, _mm_unpacklo_epi16(absdiff, zero));
    if (vert_inc == 2) {
      sum = _mm_add_epi32(sum, _mm_unpackhi_epi16(absdiff, zero));
    }

    pSrc += nSrcPitch * vert_inc;
    pRef += nRefPitch * vert_inc;
  }
  // we have 4 integers for sum: a0 a1 a2 a3
#ifdef __AVX__
  __m128i sum1 = _mm_hadd_epi32(sum, sum); // a0+a1, a2+a3, (a0+a1, a2+a3)
  sum = _mm_hadd_epi32(sum1, sum1); // a0+a1+a2+a3, (a0+a1+a2+a3,a0+a1+a2+a3,a0+a1+a2+a3)
#else
  __m128i a0_a1 = _mm_unpacklo_epi32(sum, zero); // a0 0 a1 0
  __m128i a2_a3 = _mm_unpackhi_epi32(sum, zero); // a2 0 a3 0
  sum = _mm_add_epi32(a0_a1, a2_a3); // a0+a2, 0, a1+a3, 0
                                     // sum here: two 32 bit partial result: sum1 0 sum2 0
  __m128i sum_hi = _mm_castps_si128(_mm_movehl_ps(_mm_castsi128_ps(sum), _mm_castsi128_ps(sum)));
  // __m128i sum_hi  _mm_unpackhi_epi64(sum, zero); // a1 + a3. 2 dwords right 
  sum = _mm_add_epi32(sum, sum_hi);  // a0 + a2 + a1 + a3
#endif
  unsigned int result = _mm_cvtsi128_si32(sum);

#ifdef __AVX__
  _mm256_zeroupper(); // diff from main sse2, paranoia, bacause here we have no ymm regs
#endif

  return result;
}

template<int nBlkHeight>
unsigned int Sad16_sse2_6xN_sse2(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch)
{
#ifdef __AVX__
  _mm256_zeroupper(); // diff from main sse2
#endif

  __m128i zero = _mm_setzero_si128();
  __m128i sum = _mm_setzero_si128();
  __m128i mask6_16bit = _mm_set_epi16(0, 0, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF);

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
#ifdef __AVX__
  __m128i sum1 = _mm_hadd_epi32(sum, sum);
  sum = _mm_hadd_epi32(sum1, sum1);
#else
  // at 16 bits: we have 4 integers for sum: a0 a1 a2 a3
  __m128i a0_a1 = _mm_unpacklo_epi32(sum, zero); // a0 0 a1 0
  __m128i a2_a3 = _mm_unpackhi_epi32(sum, zero); // a2 0 a3 0
  sum = _mm_add_epi32(a0_a1, a2_a3); // a0+a2, 0, a1+a3, 0
                                     // sum here: two 32 bit partial result: sum1 0 sum2 0
  __m128i sum_hi = _mm_castps_si128(_mm_movehl_ps(_mm_castsi128_ps(sum), _mm_castsi128_ps(sum)));
  // __m128i sum_hi  _mm_unpackhi_epi64(sum, zero); // a1 + a3. 2 dwords right 
  sum = _mm_add_epi32(sum, sum_hi);  // a0 + a2 + a1 + a3
#endif
  unsigned int result = _mm_cvtsi128_si32(sum);

#ifdef __AVX__
  _mm256_zeroupper(); // diff from main sse2, paranoia, bacause here we have no ymm regs
#endif

  return result;
}

template<int nBlkHeight>
unsigned int Sad16_sse2_8xN_sse2(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch)
{
#ifdef __AVX__
  _mm256_zeroupper(); // diff from main sse2
#endif

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

    if (vert_inc == 2) {
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
#ifdef __AVX__
  __m128i sum1 = _mm_hadd_epi32(sum, sum);
  sum = _mm_hadd_epi32(sum1, sum1);
#else
  // at 16 bits: we have 4 integers for sum: a0 a1 a2 a3
  __m128i a0_a1 = _mm_unpacklo_epi32(sum, zero); // a0 0 a1 0
  __m128i a2_a3 = _mm_unpackhi_epi32(sum, zero); // a2 0 a3 0
  sum = _mm_add_epi32(a0_a1, a2_a3); // a0+a2, 0, a1+a3, 0
                                     // sum here: two 32 bit partial result: sum1 0 sum2 0
  __m128i sum_hi = _mm_castps_si128(_mm_movehl_ps(_mm_castsi128_ps(sum), _mm_castsi128_ps(sum)));
  // __m128i sum_hi  _mm_unpackhi_epi64(sum, zero); // a1 + a3. 2 dwords right 
  sum = _mm_add_epi32(sum, sum_hi);  // a0 + a2 + a1 + a3
#endif
  unsigned int result = _mm_cvtsi128_si32(sum);

#ifdef __AVX__
  _mm256_zeroupper(); // diff from main sse2, paranoia, bacause here we have no ymm regs
#endif

  return result;
}

template<int nBlkHeight>
unsigned int Sad16_sse2_12xN_sse2(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch)
{
#ifdef __AVX__
  _mm256_zeroupper(); // diff from main sse2
#endif

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
#ifdef __AVX__ // avx or avx2
  __m128i sum1 = _mm_hadd_epi32(sum, sum);
  sum = _mm_hadd_epi32(sum1, sum1);
#else
  // at 16 bits: we have 4 integers for sum: a0 a1 a2 a3
  __m128i a0_a1 = _mm_unpacklo_epi32(sum, zero); // a0 0 a1 0
  __m128i a2_a3 = _mm_unpackhi_epi32(sum, zero); // a2 0 a3 0
  sum = _mm_add_epi32(a0_a1, a2_a3); // a0+a2, 0, a1+a3, 0
                                     // sum here: two 32 bit partial result: sum1 0 sum2 0
  __m128i sum_hi = _mm_castps_si128(_mm_movehl_ps(_mm_castsi128_ps(sum), _mm_castsi128_ps(sum)));
  // __m128i sum_hi  _mm_unpackhi_epi64(sum, zero); // a1 + a3. 2 dwords right 
  sum = _mm_add_epi32(sum, sum_hi);  // a0 + a2 + a1 + a3
#endif

  unsigned int result = _mm_cvtsi128_si32(sum);

#ifdef __AVX__
  _mm256_zeroupper(); // diff from main sse2, paranoia, bacause here we have no ymm regs
#endif

  return result;
}


template<int nBlkHeight>
unsigned int Sad16_sse2_16xN_sse2(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch)
{
#ifdef __AVX__
  _mm256_zeroupper(); // diff from main sse2
#endif

  __m128i zero = _mm_setzero_si128();
  __m128i sum = _mm_setzero_si128();

  for (int y = 0; y < nBlkHeight; y++)
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
    // sum1_32, sum2_32, sum3_32, sum4_32
    pSrc += nSrcPitch;
    pRef += nRefPitch;
  }
#ifdef __AVX__ // avx or avx2
  __m128i sum1 = _mm_hadd_epi32(sum, sum);
  sum = _mm_hadd_epi32(sum1, sum1);
#else
  // at 16 bits: we have 4 integers for sum: a0 a1 a2 a3
  __m128i a0_a1 = _mm_unpacklo_epi32(sum, zero); // a0 0 a1 0
  __m128i a2_a3 = _mm_unpackhi_epi32(sum, zero); // a2 0 a3 0
  sum = _mm_add_epi32(a0_a1, a2_a3); // a0+a2, 0, a1+a3, 0
                                     // sum here: two 32 bit partial result: sum1 0 sum2 0
  __m128i sum_hi = _mm_castps_si128(_mm_movehl_ps(_mm_castsi128_ps(sum), _mm_castsi128_ps(sum)));
  // __m128i sum_hi  _mm_unpackhi_epi64(sum, zero); // a1 + a3. 2 dwords right 
  sum = _mm_add_epi32(sum, sum_hi);  // a0 + a2 + a1 + a3
#endif

  unsigned int result = _mm_cvtsi128_si32(sum);

#ifdef __AVX__
  _mm256_zeroupper(); // diff from main sse2, paranoia, bacause here we have no ymm regs
#endif

  return result;
}


template<int nBlkHeight>
unsigned int Sad16_sse2_24xN_sse2(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch)
{
#ifdef __AVX__
  _mm256_zeroupper(); // diff from main sse2
#endif

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
#ifdef __AVX__ // avx or avx2
  __m128i sum1 = _mm_hadd_epi32(sum, sum);
  sum = _mm_hadd_epi32(sum1, sum1);
#else
  // at 16 bits: we have 4 integers for sum: a0 a1 a2 a3
  __m128i a0_a1 = _mm_unpacklo_epi32(sum, zero); // a0 0 a1 0
  __m128i a2_a3 = _mm_unpackhi_epi32(sum, zero); // a2 0 a3 0
  sum = _mm_add_epi32(a0_a1, a2_a3); // a0+a2, 0, a1+a3, 0
                                     // sum here: two 32 bit partial result: sum1 0 sum2 0
  __m128i sum_hi = _mm_castps_si128(_mm_movehl_ps(_mm_castsi128_ps(sum), _mm_castsi128_ps(sum)));
  // __m128i sum_hi  _mm_unpackhi_epi64(sum, zero); // a1 + a3. 2 dwords right 
  sum = _mm_add_epi32(sum, sum_hi);  // a0 + a2 + a1 + a3
#endif

  unsigned int result = _mm_cvtsi128_si32(sum);

#ifdef __AVX__
  _mm256_zeroupper(); // diff from main sse2, paranoia, bacause here we have no ymm regs
#endif

  return result;
}

template<int nBlkHeight>
unsigned int Sad16_sse2_32xN_sse2(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch)
{
#ifdef __AVX__
  _mm256_zeroupper(); // diff from main sse2
#endif

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
#ifdef __AVX__ // avx or avx2
  __m128i sum1 = _mm_hadd_epi32(sum, sum);
  sum = _mm_hadd_epi32(sum1, sum1);
#else
  // at 16 bits: we have 4 integers for sum: a0 a1 a2 a3
  __m128i a0_a1 = _mm_unpacklo_epi32(sum, zero); // a0 0 a1 0
  __m128i a2_a3 = _mm_unpackhi_epi32(sum, zero); // a2 0 a3 0
  sum = _mm_add_epi32(a0_a1, a2_a3); // a0+a2, 0, a1+a3, 0
                                     // sum here: two 32 bit partial result: sum1 0 sum2 0
  __m128i sum_hi = _mm_unpackhi_epi64(sum, zero); // a1 + a3. 2 dwords right 
  sum = _mm_add_epi32(sum, sum_hi);  // a0 + a2 + a1 + a3
#endif

  unsigned int result = _mm_cvtsi128_si32(sum);

#ifdef __AVX__
  _mm256_zeroupper(); // diff from main sse2, paranoia, bacause here we have no ymm regs
#endif

  return result;
}

template<int nBlkHeight>
unsigned int Sad16_sse2_48xN_sse2(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch)
{
#ifdef __AVX__
  _mm256_zeroupper(); // diff from main sse2
#endif

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
#ifdef __AVX__ // avx or avx2
  __m128i sum1 = _mm_hadd_epi32(sum, sum);
  sum = _mm_hadd_epi32(sum1, sum1);
#else
  // at 16 bits: we have 4 integers for sum: a0 a1 a2 a3
  __m128i a0_a1 = _mm_unpacklo_epi32(sum, zero); // a0 0 a1 0
  __m128i a2_a3 = _mm_unpackhi_epi32(sum, zero); // a2 0 a3 0
  sum = _mm_add_epi32(a0_a1, a2_a3); // a0+a2, 0, a1+a3, 0
                                     // sum here: two 32 bit partial result: sum1 0 sum2 0
  __m128i sum_hi = _mm_unpackhi_epi64(sum, zero); // a1 + a3. 2 dwords right 
  sum = _mm_add_epi32(sum, sum_hi);  // a0 + a2 + a1 + a3
#endif

  unsigned int result = _mm_cvtsi128_si32(sum);

#ifdef __AVX__
  _mm256_zeroupper(); // diff from main sse2, paranoia, bacause here we have no ymm regs
#endif

  return result;
}


template<int nBlkHeight>
unsigned int Sad16_sse2_64xN_sse2(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch)
{
#ifdef __AVX__
  _mm256_zeroupper(); // diff from main sse2
#endif

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
#ifdef __AVX__ // avx or avx2
  __m128i sum1 = _mm_hadd_epi32(sum, sum);
  sum = _mm_hadd_epi32(sum1, sum1);
#else
  // at 16 bits: we have 4 integers for sum: a0 a1 a2 a3
  __m128i a0_a1 = _mm_unpacklo_epi32(sum, zero); // a0 0 a1 0
  __m128i a2_a3 = _mm_unpackhi_epi32(sum, zero); // a2 0 a3 0
  sum = _mm_add_epi32(a0_a1, a2_a3); // a0+a2, 0, a1+a3, 0
                                     // sum here: two 32 bit partial result: sum1 0 sum2 0
  __m128i sum_hi = _mm_unpackhi_epi64(sum, zero); // a1 + a3. 2 dwords right 
  sum = _mm_add_epi32(sum, sum_hi);  // a0 + a2 + a1 + a3
#endif

  unsigned int result = _mm_cvtsi128_si32(sum);

#ifdef __AVX__
  _mm256_zeroupper(); // diff from main sse2, paranoia, bacause here we have no ymm regs
#endif

  return result;
}



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

#if 0
// test-test-test-failed
#ifdef SAD16_FROM_X265
#define SAD16_x264(blsizex, blsizey, type) extern "C" unsigned int __cdecl x264_16_pixel_sad_##blsizex##x##blsizey##_##type##(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch)
// experimental from sad16-a.asm
// arrgh problems: 
// - pitches are not byte level, but have 16bit granularity instead in the original asm.
// - subw, absw is used, which is 16 bit incompatible, internal sum limits, all stuff can be used safely for 10-12 bits max
// will not differentiate by bit-depth, too much work a.t.m.
SAD16_x264(16, 16, sse2);
SAD16_x264(8, 8, sse2);
#undef SAD16_x264
#endif
#endif

/* included from x264/x265 */
// now the prefix is still x264 (see project properties preprocessor directives for sad-a.asm)
// to be changed
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
SAD_x264(8, 8, mmx2);
SAD_x264(8, 4, mmx2);

// 6*x
SAD_x264(6, 24, sse2);
SAD_x264(6, 12, sse2);
SAD_x264(6, 6, sse2);

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

//dummy for testing and deactivate SAD
//MK_CFUNC(SadDummy);
#undef MK_CFUNC


#endif
