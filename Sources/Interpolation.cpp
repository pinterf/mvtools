// Functions that interpolates a frame
// Author: Manao
// Copyright(c)2006 A.G.Balakhnin aka Fizick - bicubic, Wiener, separable

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

#ifdef _WIN32
#include "avs/win.h"
#endif

#include "Interpolation.h"

#include <emmintrin.h>
#include <smmintrin.h> // sse 4.1
#include <immintrin.h>
#include	<algorithm>
#include	<cassert>
#include <stdint.h>
#include "def.h"
#include "avs/cpuid.h"

MV_FORCEINLINE __m128i _MM_CMPLE_EPU16(__m128i x, __m128i y)
{
  // Returns 0xFFFF where x <= y:
  return _mm_cmpeq_epi16(_mm_subs_epu16(x, y), _mm_setzero_si128());
}

MV_FORCEINLINE __m128i _MM_BLENDV_SI128(__m128i x, __m128i y, __m128i mask)
{
  // Replace bit in x with bit in y when matching bit in mask is set:
  return _mm_or_si128(_mm_andnot_si128(mask, x), _mm_and_si128(mask, y));
}

// sse2 simulation of SSE4's _mm_min_epu16
MV_FORCEINLINE __m128i _MM_MIN_EPU16(__m128i x, __m128i y)
{
  // Returns x where x <= y, else y:
  return _MM_BLENDV_SI128(y, x, _MM_CMPLE_EPU16(x, y));
}

// sse2 simulation of SSE4's _mm_max_epu16
MV_FORCEINLINE __m128i _MM_MAX_EPU16(__m128i x, __m128i y)
{
  // Returns x where x >= y, else y:
  return _MM_BLENDV_SI128(x, y, _MM_CMPLE_EPU16(x, y));
}

// sse2 replacement of _mm_mullo_epi32 in SSE4.1
// use it after speed test, may have too much overhead and C is faster
MV_FORCEINLINE __m128i _MM_MULLO_EPI32(const __m128i &a, const __m128i &b)
{
  // for SSE 4.1: return _mm_mullo_epi32(a, b);
  __m128i tmp1 = _mm_mul_epu32(a, b); // mul 2,0
  __m128i tmp2 = _mm_mul_epu32(_mm_srli_si128(a, 4), _mm_srli_si128(b, 4)); // mul 3,1
                                                                            // shuffle results to [63..0] and pack. a2->a1, a0->a0
  return _mm_unpacklo_epi32(_mm_shuffle_epi32(tmp1, _MM_SHUFFLE(0, 0, 2, 0)), _mm_shuffle_epi32(tmp2, _MM_SHUFFLE(0, 0, 2, 0)));
}

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4309)
#endif
// fake _mm_packus_epi32 (orig is SSE4.1 only)
static MV_FORCEINLINE __m128i _MM_PACKUS_EPI32(__m128i a, __m128i b)
{
  const __m128i val_32 = _mm_set1_epi32(0x8000);
  const __m128i val_16 = _mm_set1_epi16(0x8000);

  a = _mm_sub_epi32(a, val_32);
  b = _mm_sub_epi32(b, val_32);
  a = _mm_packs_epi32(a, b);
  a = _mm_add_epi16(a, val_16);
  return a;
}
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#define RB2_jump(y_new, y, pDst, pSrc, nDstPitch, nSrcPitch) \
{ const int dif = y_new - y; \
  pDst += nDstPitch / sizeof(*pDst) * dif; \
  pSrc += nSrcPitch / sizeof(*pSrc) * dif * 2; \
  y = y_new; \
}

#define RB2_jump_1(y_new, y, pDst, nDstPitch) \
{ const int dif = y_new - y; \
  pDst += nDstPitch / sizeof(*pDst) * dif; \
  y = y_new; \
}

// 8-32bits
template<typename pixel_t>
void RB2F_C(
  pixel_t *pDst, const pixel_t *pSrc, int nDstPitch, int nSrcPitch,
  int nWidth, int nHeight)
{
  // pitches still in bytes
  nDstPitch /= sizeof(pixel_t);
  nSrcPitch /= sizeof(pixel_t);

  for (int y = 0; y < nHeight; ++y)
  {
    for (int x = 0; x < nWidth; x++)
    {
      if constexpr(sizeof(pixel_t) <= 2)
        pDst[x] = (pSrc[x * 2] + pSrc[x * 2 + 1]
          + pSrc[x * 2 + nSrcPitch] + pSrc[x * 2 + nSrcPitch + 1] + 2) / 4; // int
      else
        pDst[x] = (pSrc[x * 2] + pSrc[x * 2 + 1]
          + pSrc[x * 2 + nSrcPitch] + pSrc[x * 2 + nSrcPitch + 1]) * (1.0f / 4.0f); // float
    }
    pDst += nDstPitch;
    pSrc += nSrcPitch * 2;
  }
}

template<typename pixel_t, bool hasSSE41>
void RB2F_sse2(
  pixel_t* pDst, const pixel_t* pSrc, int nDstPitch, int nSrcPitch,
  int nWidth, int nHeight)
{
  // pitch is byte-level here
  nDstPitch /= sizeof(pixel_t);
  nSrcPitch /= sizeof(pixel_t);

  constexpr int pixels_at_a_time = 8 / sizeof(pixel_t);
  int nWidthMMX = (nWidth / pixels_at_a_time) * pixels_at_a_time;

  __m128i everySecondMask;
  if constexpr (sizeof(pixel_t) == 1)
    everySecondMask = _mm_set1_epi16(0x00FF);
  else
    everySecondMask = _mm_set1_epi32(0x0000FFFF);

  for (int y = 0; y < nHeight; ++y)
  {
    for (int x = 0; x < nWidthMMX; x += pixels_at_a_time)
    {
      __m128i m2 = _mm_loadu_si128((const __m128i*) & pSrc[x * 2]);
      __m128i m3 = _mm_loadu_si128((const __m128i*) & pSrc[x * 2 + 1]);
      __m128i m4 = _mm_loadu_si128((const __m128i*) & pSrc[x * 2 + nSrcPitch]);
      __m128i m5 = _mm_loadu_si128((const __m128i*) & pSrc[x * 2 + nSrcPitch + 1]);
      m2 = _mm_and_si128(m2, everySecondMask);
      m3 = _mm_and_si128(m3, everySecondMask);
      m4 = _mm_and_si128(m4, everySecondMask);
      m5 = _mm_and_si128(m5, everySecondMask);
      __m128i res;
      if constexpr (sizeof(pixel_t) == 1) {
        auto sum = _mm_add_epi16(_mm_add_epi16(m2, m3), _mm_add_epi16(m4, m5));
        sum = _mm_add_epi16(sum, _mm_set1_epi16(2));
        res = _mm_srli_epi16(sum, 2);
        res = _mm_packus_epi16(res, res);
      }
      else {
        auto sum = _mm_add_epi32(_mm_add_epi32(m2, m3), _mm_add_epi32(m4, m5));
        sum = _mm_add_epi32(sum, _mm_set1_epi32(2));
        res = _mm_srli_epi32(sum, 2);
        if constexpr (hasSSE41)
          res = _mm_packus_epi32(res, res);
        else
          m2 = _MM_PACKUS_EPI32(res, res);
      }
      _mm_storel_epi64((__m128i*) & pDst[x], res);
    }
    for (int x = nWidthMMX; x < nWidth; x++)
    {
      pDst[x] = (pSrc[x * 2] + pSrc[x * 2 + 1] + pSrc[x * 2 + nSrcPitch] + pSrc[x * 2 + nSrcPitch + 1] + 2) >> 2;
    }
    pDst += nDstPitch;
    pSrc += nSrcPitch * 2;
  }
}

// 8-32bits
template<typename pixel_t>
void RB2F(
  unsigned char *pDst8, const unsigned char *pSrc8, int nDstPitch, int nSrcPitch,
  int nWidth, int nHeight, int y_beg, int y_end, int cpuFlags)
{
  assert(y_beg >= 0);
  assert(y_end <= nHeight);

  pixel_t *    pDst = reinterpret_cast<pixel_t *>(pDst8);
  const pixel_t *pSrc = reinterpret_cast<const pixel_t *>(pSrc8);

  bool isse2 = !!(cpuFlags & CPUF_SSE2);
  bool isse4 = !!(cpuFlags & CPUF_SSE4_1);

  int y = 0;
  RB2_jump(y_beg, y, pDst, pSrc, nDstPitch, nSrcPitch);
  if constexpr(sizeof(pixel_t) == 4)
    RB2F_C<pixel_t>(pDst, pSrc, nDstPitch, nSrcPitch, nWidth, y_end - y_beg);
  else {
    if (isse4 && sizeof(pixel_t) == 2)
      RB2F_sse2<pixel_t, true>(pDst, pSrc, nDstPitch, nSrcPitch, nWidth, y_end - y_beg);
    else if (isse2)
      RB2F_sse2<pixel_t, false>(pDst, pSrc, nDstPitch, nSrcPitch, nWidth, y_end - y_beg);
    else
      RB2F_C<pixel_t>(pDst, pSrc, nDstPitch, nSrcPitch, nWidth, y_end - y_beg);
  }
}

/*
void RB2Filtered_C(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch,
            int nSrcPitch, int nWidth, int nHeight)
{ // sort of Reduceby2 with 1/4, 1/2, 1/4 filter for smoothing - Fizick v.1.10.3

    for ( int x = 0; x < nWidth; x++ )
            pDst[x] = (pSrc[x*2] + pSrc[x*2+1] + pSrc[x*2+nSrcPitch+1] + pSrc[x*2+nSrcPitch] + 2) / 4;
    pDst += nDstPitch;
    pSrc += nSrcPitch * 2;

  for ( int y = 1; y < nHeight; y++ )
  {
        int x = 0;
            pDst[x] = (2*pSrc[x*2-nSrcPitch] + 2*pSrc[x*2-nSrcPitch+1] +
                        4*pSrc[x*2] + 4*pSrc[x*2+1] +
                        2*pSrc[x*2+nSrcPitch+1] + 2*pSrc[x*2+nSrcPitch] + 8) / 16;

    for ( x = 1; x < nWidth; x++ )
            pDst[x] = (pSrc[x*2-nSrcPitch-1] + pSrc[x*2-nSrcPitch]*2 + pSrc[x*2-nSrcPitch+1] +
                       pSrc[x*2-1]*2 + pSrc[x*2]*4 + pSrc[x*2+1]*2 +
                       pSrc[x*2+nSrcPitch-1] + pSrc[x*2+nSrcPitch]*2 + pSrc[x*2+nSrcPitch+1] + 8) / 16;
    pDst += nDstPitch;
    pSrc += nSrcPitch * 2;
  }
}

void RB2BilinearFiltered_C(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch,
            int nSrcPitch, int nWidth, int nHeight)
{ // filtered bilinear with 1/8, 3/8, 3/8, 1/8 filter for smoothing and anti-aliasing - Fizick v.2.3.1

  for ( int y = 0; y < 1; y++ )
  {
    for ( int x = 0; x < nWidth; x++ )
            pDst[x] = (pSrc[x*2] + pSrc[x*2+1] + pSrc[x*2+nSrcPitch+1] + pSrc[x*2+nSrcPitch] + 2) / 4;
    pDst += nDstPitch;
    pSrc += nSrcPitch * 2;
  }

  for ( int y = 1; y < nHeight-1; y++ )
  {
    for ( int x = 0; x < 1; x++ )
           pDst[x] = (pSrc[x*2] + pSrc[x*2+1] + pSrc[x*2+nSrcPitch+1] + pSrc[x*2+nSrcPitch] + 2) / 4;

    for ( x = 1; x < nWidth-1; x++ )
            pDst[x] = (unsigned int)(pSrc[x*2-nSrcPitch-1] + pSrc[x*2-nSrcPitch]*3 + pSrc[x*2-nSrcPitch+1]*3 + pSrc[x*2-nSrcPitch+2] +
                       pSrc[x*2-1]*3 + pSrc[x*2]*9 + pSrc[x*2+1]*9 + pSrc[x*2+2]*3 +
                       pSrc[x*2+nSrcPitch-1]*3 + pSrc[x*2+nSrcPitch]*9 + pSrc[x*2+nSrcPitch+1]*9 + pSrc[x*2+nSrcPitch+2]*3 +
                       pSrc[x*2+nSrcPitch*2-1] + pSrc[x*2+nSrcPitch*2]*3 + pSrc[x*2+nSrcPitch*2+1]*3 + pSrc[x*2+nSrcPitch*2+2] + 32) / 64;

    for ( int x = max(nWidth-1,1); x < nWidth; x++ )
           pDst[x] = (pSrc[x*2] + pSrc[x*2+1] + pSrc[x*2+nSrcPitch+1] + pSrc[x*2+nSrcPitch] + 2) / 4;

    pDst += nDstPitch;
    pSrc += nSrcPitch * 2;
  }
  for ( int y = max(nHeight-1,1); y < nHeight; y++ )
  {
    for ( int x = 0; x < nWidth; x++ )
            pDst[x] = (pSrc[x*2] + pSrc[x*2+1] + pSrc[x*2+nSrcPitch+1] + pSrc[x*2+nSrcPitch] + 2) / 4;
    pDst += nDstPitch;
    pSrc += nSrcPitch * 2;
  }

}

void RB2Quadratic_C(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch,
            int nSrcPitch, int nWidth, int nHeight)
{ // filtered quadratic with 1/64, 9/64, 22/64, 22/64, 9/64, 1/64 filter for smoothing and anti-aliasing - Fizick v.2.3.1

  for ( int y = 0; y < 1; y++ )
  {
    for ( int x = 0; x < nWidth; x++ )
            pDst[x] = (pSrc[x*2] + pSrc[x*2+1] + pSrc[x*2+nSrcPitch+1] + pSrc[x*2+nSrcPitch] + 2) / 4;
    pDst += nDstPitch;
    pSrc += nSrcPitch * 2;
  }

  for ( int y = 1; y < nHeight-1; y++ )
  {
    for ( int x = 0; x < 1; x++ )
           pDst[x] = (pSrc[x*2] + pSrc[x*2+1] + pSrc[x*2+nSrcPitch+1] + pSrc[x*2+nSrcPitch] + 2) / 4;

    for ( int x = 1; x < nWidth-1; x++ )
            pDst[x] =
            (pSrc[x*2-nSrcPitch*2-2] + pSrc[x*2-nSrcPitch*2-1]*9 + pSrc[x*2-nSrcPitch*2]*22 + pSrc[x*2-nSrcPitch*2+1]*22 + pSrc[x*2-nSrcPitch*2+2]*9 + pSrc[x*2-nSrcPitch*2+3] +
            pSrc[x*2-nSrcPitch-2]*9 + pSrc[x*2-nSrcPitch-1]*81 + pSrc[x*2-nSrcPitch]*198 + pSrc[x*2-nSrcPitch+1]*198 + pSrc[x*2-nSrcPitch+2]*81 + pSrc[x*2-nSrcPitch+3]*9 +
            pSrc[x*2-2]*22 + pSrc[x*2-1]*198 + pSrc[x*2]*484 + pSrc[x*2+1]*484 + pSrc[x*2+2]*198 + pSrc[x*2+3]*22 +
            pSrc[x*2+nSrcPitch-2]*22 + pSrc[x*2+nSrcPitch-1]*198 + pSrc[x*2+nSrcPitch]*484 + pSrc[x*2+nSrcPitch+1]*484 + pSrc[x*2+nSrcPitch+2]*198 + pSrc[x*2+nSrcPitch+3]*22 +
            pSrc[x*2+nSrcPitch*2-2]*9 + pSrc[x*2+nSrcPitch*2-1]*81 + pSrc[x*2+nSrcPitch*2]*198 + pSrc[x*2+nSrcPitch*2+1]*198 + pSrc[x*2+nSrcPitch*2+2]*81 + pSrc[x*2+nSrcPitch*2+3]*9 +
            pSrc[x*2+nSrcPitch*3-2] + pSrc[x*2+nSrcPitch*3-1]*9 + pSrc[x*2+nSrcPitch*3]*22 + pSrc[x*2+nSrcPitch*3+1]*22 + pSrc[x*2+nSrcPitch*3+2]*9 + pSrc[x*2+nSrcPitch*3+3] + 2048) /4096;

    for ( int x = max(nWidth-1,1); x < nWidth; x++ )
           pDst[x] = (pSrc[x*2] + pSrc[x*2+1] + pSrc[x*2+nSrcPitch+1] + pSrc[x*2+nSrcPitch] + 2) / 4;

    pDst += nDstPitch;
    pSrc += nSrcPitch * 2;
  }
  for ( int y = max(nHeight-1,1); y < nHeight; y++ )
  {
    for ( int x = 0; x < nWidth; x++ )
            pDst[x] = (pSrc[x*2] + pSrc[x*2+1] + pSrc[x*2+nSrcPitch+1] + pSrc[x*2+nSrcPitch] + 2) / 4;
    pDst += nDstPitch;
    pSrc += nSrcPitch * 2;
  }

}

void RB2Cubic_C(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch,
            int nSrcPitch, int nWidth, int nHeight)
{ // filtered qubic with 1/32, 5/32, 10/32, 10/32, 5/32, 1/32 filter for smoothing and anti-aliasing - Fizick v.2.3.1

  for ( int y = 0; y < 1; y++ )
  {
    for ( int x = 0; x < nWidth; x++ )
            pDst[x] = (pSrc[x*2] + pSrc[x*2+1] + pSrc[x*2+nSrcPitch+1] + pSrc[x*2+nSrcPitch] + 2) / 4;
    pDst += nDstPitch;
    pSrc += nSrcPitch * 2;
  }

  for ( int y = 1; y < nHeight-1; y++ )
  {
    for ( int x = 0; x < 1; x++ )
           pDst[x] = (pSrc[x*2] + pSrc[x*2+1] + pSrc[x*2+nSrcPitch+1] + pSrc[x*2+nSrcPitch] + 2) / 4;

    for ( int x = 1; x < nWidth-1; x++ )
            pDst[x] =
            (pSrc[x*2-nSrcPitch*2-2] + pSrc[x*2-nSrcPitch*2-1]*5 + pSrc[x*2-nSrcPitch*2]*10 + pSrc[x*2-nSrcPitch*2+1]*10 + pSrc[x*2-nSrcPitch*2+2]*5 + pSrc[x*2-nSrcPitch*2+3] +
            pSrc[x*2-nSrcPitch-2]*5 + pSrc[x*2-nSrcPitch-1]*25 + pSrc[x*2-nSrcPitch]*50 + pSrc[x*2-nSrcPitch+1]*50 + pSrc[x*2-nSrcPitch+2]*25 + pSrc[x*2-nSrcPitch+3]*5 +
            pSrc[x*2-2]*10 + pSrc[x*2-1]*50 + pSrc[x*2]*100 + pSrc[x*2+1]*100 + pSrc[x*2+2]*50 + pSrc[x*2+3]*10 +
            pSrc[x*2+nSrcPitch-2]*10 + pSrc[x*2+nSrcPitch-1]*50 + pSrc[x*2+nSrcPitch]*100 + pSrc[x*2+nSrcPitch+1]*100 + pSrc[x*2+nSrcPitch+2]*50 + pSrc[x*2+nSrcPitch+3]*10 +
            pSrc[x*2+nSrcPitch*2-2]*5 + pSrc[x*2+nSrcPitch*2-1]*25 + pSrc[x*2+nSrcPitch*2]*50 + pSrc[x*2+nSrcPitch*2+1]*50 + pSrc[x*2+nSrcPitch*2+2]*25 + pSrc[x*2+nSrcPitch*2+3]*5 +
            pSrc[x*2+nSrcPitch*3-2] + pSrc[x*2+nSrcPitch*3-1]*5 + pSrc[x*2+nSrcPitch*3]*10 + pSrc[x*2+nSrcPitch*3+1]*10 + pSrc[x*2+nSrcPitch*3+2]*5 + pSrc[x*2+nSrcPitch*3+3] + 512) /1024;

    for ( int x = max(nWidth-1,1); x < nWidth; x++ )
           pDst[x] = (pSrc[x*2] + pSrc[x*2+1] + pSrc[x*2+nSrcPitch+1] + pSrc[x*2+nSrcPitch] + 2) / 4;

    pDst += nDstPitch;
    pSrc += nSrcPitch * 2;
  }
  for ( int y = max(nHeight-1,1); y < nHeight; y++ )
  {
    for ( int x = 0; x < nWidth; x++ )
            pDst[x] = (pSrc[x*2] + pSrc[x*2+1] + pSrc[x*2+nSrcPitch+1] + pSrc[x*2+nSrcPitch] + 2) / 4;
    pDst += nDstPitch;
    pSrc += nSrcPitch * 2;
  }

}
*/

// filtered qubic with 1/32, 5/32, 10/32, 10/32, 5/32, 1/32 filter for smoothing and anti-aliasing
// Width is reduced by 2
// 8-16bits
template<typename pixel_t, bool hasSSE41>
static void RB2CubicHorizontalInplaceLine_sse2(pixel_t *pSrc, int nWidthMMX) {
  __m128i everySecondMask;
  if constexpr(sizeof(pixel_t) == 1)
    everySecondMask = _mm_set1_epi16(0x00FF);
  else
    everySecondMask = _mm_set1_epi32(0x0000FFFF);

  for (int x = 1; x < nWidthMMX; x += 8 / sizeof(pixel_t)) {
    __m128i m0 = _mm_loadu_si128((const __m128i *)&pSrc[x * 2 - 2]);
    __m128i m1 = _mm_loadu_si128((const __m128i *)&pSrc[x * 2 - 1]);
    __m128i m2 = _mm_loadu_si128((const __m128i *)&pSrc[x * 2]);
    __m128i m3 = _mm_loadu_si128((const __m128i *)&pSrc[x * 2 + 1]);
    __m128i m4 = _mm_loadu_si128((const __m128i *)&pSrc[x * 2 + 2]);
    __m128i m5 = _mm_loadu_si128((const __m128i *)&pSrc[x * 2 + 3]);
    
    m0 = _mm_and_si128(m0, everySecondMask);
    m1 = _mm_and_si128(m1, everySecondMask);
    m2 = _mm_and_si128(m2, everySecondMask);
    m3 = _mm_and_si128(m3, everySecondMask);
    m4 = _mm_and_si128(m4, everySecondMask);
    m5 = _mm_and_si128(m5, everySecondMask);

    if constexpr(sizeof(pixel_t) == 1) {
      m2 = _mm_add_epi16(m2, m3);
      m3 = _mm_slli_epi16(m2, 3);
      m2 = _mm_slli_epi16(m2, 1);
      m2 = _mm_add_epi16(m2, m3);

      m1 = _mm_add_epi16(m1, m4);
      m4 = _mm_slli_epi16(m1, 2);
      m1 = _mm_add_epi16(m1, m4);

      m2 = _mm_add_epi16(m2, m1);
      m2 = _mm_add_epi16(m2, m0);
      m2 = _mm_add_epi16(m2, m5);

      m2 = _mm_add_epi16(m2, _mm_set1_epi16(16));
      m2 = _mm_srli_epi16(m2, 5);
      m2 = _mm_packus_epi16(m2, m2);
    }
    else {
      m2 = _mm_add_epi32(m2, m3);
      m3 = _mm_slli_epi32(m2, 3);
      m2 = _mm_slli_epi32(m2, 1);
      m2 = _mm_add_epi32(m2, m3);

      m1 = _mm_add_epi32(m1, m4);
      m4 = _mm_slli_epi32(m1, 2);
      m1 = _mm_add_epi32(m1, m4);

      m2 = _mm_add_epi32(m2, m1);
      m2 = _mm_add_epi32(m2, m0);
      m2 = _mm_add_epi32(m2, m5);

      m2 = _mm_add_epi32(m2, _mm_set1_epi32(16));
      m2 = _mm_srli_epi32(m2, 5);
      if constexpr(hasSSE41)
        m2 = _mm_packus_epi32(m2, m2);
      else
        m2 = _MM_PACKUS_EPI32(m2, m2);
    }
    _mm_storel_epi64((__m128i *)&pSrc[x], m2);
  }
}

template<typename pixel_t, bool hasSSE41>
static void RB2CubicVerticalLine_sse2(uint8_t *pDst, const uint8_t *pSrc, int nSrcPitch, int nWidthMMX) {
  const __m128i zeroes = _mm_setzero_si128();
  // pitch is byte-level here
  for (int x = 0; x < nWidthMMX * (int)sizeof(pixel_t); x += 8) {
    __m128i m0 = _mm_loadl_epi64((const __m128i *)&pSrc[x - nSrcPitch * 2]);
    __m128i m1 = _mm_loadl_epi64((const __m128i *)&pSrc[x - nSrcPitch]);
    __m128i m2 = _mm_loadl_epi64((const __m128i *)&pSrc[x]);
    __m128i m3 = _mm_loadl_epi64((const __m128i *)&pSrc[x + nSrcPitch]);
    __m128i m4 = _mm_loadl_epi64((const __m128i *)&pSrc[x + nSrcPitch * 2]);
    __m128i m5 = _mm_loadl_epi64((const __m128i *)&pSrc[x + nSrcPitch * 3]);

    if constexpr(sizeof(pixel_t) == 1) {
      m0 = _mm_unpacklo_epi8(m0, zeroes);
      m1 = _mm_unpacklo_epi8(m1, zeroes);
      m2 = _mm_unpacklo_epi8(m2, zeroes);
      m3 = _mm_unpacklo_epi8(m3, zeroes);
      m4 = _mm_unpacklo_epi8(m4, zeroes);
      m5 = _mm_unpacklo_epi8(m5, zeroes);

      m2 = _mm_add_epi16(m2, m3);
      m3 = _mm_slli_epi16(m2, 3);
      m2 = _mm_slli_epi16(m2, 1);
      m2 = _mm_add_epi16(m2, m3);

      m1 = _mm_add_epi16(m1, m4);
      m4 = _mm_slli_epi16(m1, 2);
      m1 = _mm_add_epi16(m1, m4);

      m2 = _mm_add_epi16(m2, m1);
      m2 = _mm_add_epi16(m2, m0);
      m2 = _mm_add_epi16(m2, m5);

      m2 = _mm_add_epi16(m2, _mm_set1_epi16(16));
      m2 = _mm_srli_epi16(m2, 5);
      m2 = _mm_packus_epi16(m2, m2);
    }
    else {
      m0 = _mm_unpacklo_epi16(m0, zeroes);
      m1 = _mm_unpacklo_epi16(m1, zeroes);
      m2 = _mm_unpacklo_epi16(m2, zeroes);
      m3 = _mm_unpacklo_epi16(m3, zeroes);
      m4 = _mm_unpacklo_epi16(m4, zeroes);
      m5 = _mm_unpacklo_epi16(m5, zeroes);

      m2 = _mm_add_epi32(m2, m3);
      m3 = _mm_slli_epi32(m2, 3);
      m2 = _mm_slli_epi32(m2, 1);
      m2 = _mm_add_epi32(m2, m3);

      m1 = _mm_add_epi32(m1, m4);
      m4 = _mm_slli_epi32(m1, 2);
      m1 = _mm_add_epi32(m1, m4);

      m2 = _mm_add_epi32(m2, m1);
      m2 = _mm_add_epi32(m2, m0);
      m2 = _mm_add_epi32(m2, m5);

      m2 = _mm_add_epi32(m2, _mm_set1_epi32(16));
      m2 = _mm_srli_epi32(m2, 5);
      if constexpr(hasSSE41)
        m2 = _mm_packus_epi32(m2, m2);
      else
        m2 = _MM_PACKUS_EPI32(m2, m2);
    }
    _mm_storel_epi64((__m128i *)&pDst[x], m2);
  }
}

template<typename pixel_t, bool hasSSE41>
static void RB2QuadraticHorizontalInplaceLine_sse2(pixel_t *pSrc, int nWidthMMX) {
  __m128i everySecondMask;
  if constexpr(sizeof(pixel_t) == 1)
    everySecondMask = _mm_set1_epi16(0x00FF);
  else
    everySecondMask = _mm_set1_epi32(0x0000FFFF);

  for (int x = 1; x < nWidthMMX; x += 8 / sizeof(pixel_t)) {

    __m128i m0 = _mm_loadu_si128((const __m128i *)&pSrc[x * 2 - 2]);
    __m128i m1 = _mm_loadu_si128((const __m128i *)&pSrc[x * 2 - 1]);
    __m128i m2 = _mm_loadu_si128((const __m128i *)&pSrc[x * 2]);
    __m128i m3 = _mm_loadu_si128((const __m128i *)&pSrc[x * 2 + 1]);
    __m128i m4 = _mm_loadu_si128((const __m128i *)&pSrc[x * 2 + 2]);
    __m128i m5 = _mm_loadu_si128((const __m128i *)&pSrc[x * 2 + 3]);

    m0 = _mm_and_si128(m0, everySecondMask);
    m1 = _mm_and_si128(m1, everySecondMask);
    m2 = _mm_and_si128(m2, everySecondMask);
    m3 = _mm_and_si128(m3, everySecondMask);
    m4 = _mm_and_si128(m4, everySecondMask);
    m5 = _mm_and_si128(m5, everySecondMask);

    if constexpr(sizeof(pixel_t) == 1) {
      m2 = _mm_add_epi16(m2, m3);
      m2 = _mm_mullo_epi16(m2, _mm_set1_epi16(22));

      m1 = _mm_add_epi16(m1, m4);
      m4 = _mm_slli_epi16(m1, 3);
      m1 = _mm_add_epi16(m1, m4);

      m2 = _mm_add_epi16(m2, m1);
      m2 = _mm_add_epi16(m2, m0);
      m2 = _mm_add_epi16(m2, m5);

      m2 = _mm_add_epi16(m2, _mm_set1_epi16(32));
      m2 = _mm_srli_epi16(m2, 6);
      m2 = _mm_packus_epi16(m2, m2);
    }
    else {
      m2 = _mm_add_epi32(m2, m3);
      if constexpr(hasSSE41)
        m2 = _mm_mullo_epi32(m2, _mm_set1_epi32(22));
      else
        m2 = _MM_MULLO_EPI32(m2, _mm_set1_epi32(22));

      m1 = _mm_add_epi32(m1, m4);
      m4 = _mm_slli_epi32(m1, 3);
      m1 = _mm_add_epi32(m1, m4);

      m2 = _mm_add_epi32(m2, m1);
      m2 = _mm_add_epi32(m2, m0);
      m2 = _mm_add_epi32(m2, m5);

      m2 = _mm_add_epi32(m2, _mm_set1_epi32(32));
      m2 = _mm_srli_epi32(m2, 6);
      if constexpr(hasSSE41)
        m2 = _mm_packus_epi32(m2, m2);
      else
        m2 = _MM_PACKUS_EPI32(m2, m2);
    }
    _mm_storel_epi64((__m128i *)&pSrc[x], m2);
  }
}

// filtered Quadratic with 1/64, 9/64, 22/64, 22/64, 9/64, 1/64 filter for smoothing and anti-aliasing
// nHeight is dst height which is reduced by 2 source height
template<typename pixel_t, bool hasSSE41>
static void RB2QuadraticVerticalLine_sse2(uint8_t *pDst, const uint8_t *pSrc, int nSrcPitch, int nWidthMMX) {

  const __m128i zeroes = _mm_setzero_si128();

  for (int x = 0; x < nWidthMMX * (int)sizeof(pixel_t); x += 8) {
    __m128i m0 = _mm_loadl_epi64((const __m128i *)&pSrc[x - nSrcPitch * 2]);
    __m128i m1 = _mm_loadl_epi64((const __m128i *)&pSrc[x - nSrcPitch]);
    __m128i m2 = _mm_loadl_epi64((const __m128i *)&pSrc[x]);
    __m128i m3 = _mm_loadl_epi64((const __m128i *)&pSrc[x + nSrcPitch]);
    __m128i m4 = _mm_loadl_epi64((const __m128i *)&pSrc[x + nSrcPitch * 2]);
    __m128i m5 = _mm_loadl_epi64((const __m128i *)&pSrc[x + nSrcPitch * 3]);
    
    if constexpr(sizeof(pixel_t) == 1) {
      m0 = _mm_unpacklo_epi8(m0, zeroes);
      m1 = _mm_unpacklo_epi8(m1, zeroes);
      m2 = _mm_unpacklo_epi8(m2, zeroes);
      m3 = _mm_unpacklo_epi8(m3, zeroes);
      m4 = _mm_unpacklo_epi8(m4, zeroes);
      m5 = _mm_unpacklo_epi8(m5, zeroes);

      m2 = _mm_add_epi16(m2, m3);
      m2 = _mm_mullo_epi16(m2, _mm_set1_epi16(22));

      m1 = _mm_add_epi16(m1, m4);
      m4 = _mm_slli_epi16(m1, 3);
      m1 = _mm_add_epi16(m1, m4);

      m2 = _mm_add_epi16(m2, m1);
      m2 = _mm_add_epi16(m2, m0);
      m2 = _mm_add_epi16(m2, m5);

      m2 = _mm_add_epi16(m2, _mm_set1_epi16(32));
      m2 = _mm_srli_epi16(m2, 6);
      m2 = _mm_packus_epi16(m2, m2);
    }
    else {
      m0 = _mm_unpacklo_epi16(m0, zeroes);
      m1 = _mm_unpacklo_epi16(m1, zeroes);
      m2 = _mm_unpacklo_epi16(m2, zeroes);
      m3 = _mm_unpacklo_epi16(m3, zeroes);
      m4 = _mm_unpacklo_epi16(m4, zeroes);
      m5 = _mm_unpacklo_epi16(m5, zeroes);

      m2 = _mm_add_epi32(m2, m3);
      if constexpr(hasSSE41)
        m2 = _mm_mullo_epi32(m2, _mm_set1_epi32(22));
      else
        m2 = _MM_MULLO_EPI32(m2, _mm_set1_epi32(22));

      m1 = _mm_add_epi32(m1, m4);
      m4 = _mm_slli_epi32(m1, 3);
      m1 = _mm_add_epi32(m1, m4);

      m2 = _mm_add_epi32(m2, m1);
      m2 = _mm_add_epi32(m2, m0);
      m2 = _mm_add_epi32(m2, m5);

      m2 = _mm_add_epi32(m2, _mm_set1_epi32(32));
      m2 = _mm_srli_epi32(m2, 6);
      if constexpr(hasSSE41)
        m2 = _mm_packus_epi32(m2, m2);
      else
        m2 = _MM_PACKUS_EPI32(m2, m2);
    }
    _mm_storel_epi64((__m128i *)&pDst[x], m2);
  }
}


template<typename pixel_t, bool hasSSE41>
static void RB2BilinearFilteredVerticalLine_sse2(uint8_t *pDst, const uint8_t *pSrc, intptr_t nSrcPitch, intptr_t nWidthMMX) {
  const __m128i zeroes = _mm_setzero_si128();

  for (int x = 0; x < nWidthMMX * (int)sizeof(pixel_t); x += 8) {
    __m128i m0 = _mm_loadl_epi64((const __m128i *)&pSrc[x - nSrcPitch]);
    __m128i m1 = _mm_loadl_epi64((const __m128i *)&pSrc[x]);
    __m128i m2 = _mm_loadl_epi64((const __m128i *)&pSrc[x + nSrcPitch]);
    __m128i m3 = _mm_loadl_epi64((const __m128i *)&pSrc[x + nSrcPitch * 2]);

    if constexpr(sizeof(pixel_t) == 1) {
      m0 = _mm_unpacklo_epi8(m0, zeroes);
      m1 = _mm_unpacklo_epi8(m1, zeroes);
      m2 = _mm_unpacklo_epi8(m2, zeroes);
      m3 = _mm_unpacklo_epi8(m3, zeroes);

      m1 = _mm_add_epi16(m1, m2);
      m2 = _mm_slli_epi16(m1, 1);
      m1 = _mm_add_epi16(m1, m2);

      m0 = _mm_add_epi16(m0, m1);
      m0 = _mm_add_epi16(m0, m3);
      m0 = _mm_add_epi16(m0, _mm_set1_epi16(4));
      m0 = _mm_srli_epi16(m0, 3);

      m0 = _mm_packus_epi16(m0, m0);
    }
    else {
      m0 = _mm_unpacklo_epi16(m0, zeroes);
      m1 = _mm_unpacklo_epi16(m1, zeroes);
      m2 = _mm_unpacklo_epi16(m2, zeroes);
      m3 = _mm_unpacklo_epi16(m3, zeroes);

      m1 = _mm_add_epi32(m1, m2);
      m2 = _mm_slli_epi32(m1, 1);
      m1 = _mm_add_epi32(m1, m2);

      m0 = _mm_add_epi32(m0, m1);
      m0 = _mm_add_epi32(m0, m3);
      m0 = _mm_add_epi32(m0, _mm_set1_epi32(4));
      m0 = _mm_srli_epi32(m0, 3);

      if constexpr(hasSSE41)
        m0 = _mm_packus_epi32(m0, m0);
      else
        m0 = _MM_PACKUS_EPI32(m0, m0);
    }
    _mm_storel_epi64((__m128i *)&pDst[x], m0);
  }
}

template<typename pixel_t, bool hasSSE41>
static void RB2BilinearFilteredHorizontalInplaceLine_sse2(pixel_t *pSrc, int nWidthMMX, int nWidth) {
  __m128i everySecondMask;
  if constexpr(sizeof(pixel_t) == 1)
    everySecondMask = _mm_set1_epi16(0x00FF);
  else
    everySecondMask = _mm_set1_epi32(0x0000FFFF);

  // reduces 2 * Width to Width.
  // [0], [Width-2] [Width-1] is calculated in C
  // non mod4/8 pixels before [Width-2] which are not covered by 16 byte SIMD load are also in C
  // 
  // nWidthMMX ensures that when reading the source -1 0 +1 +2 offsets are safely read

  for (int x = 1; x < nWidthMMX; x += 8 / sizeof(pixel_t)) {
    __m128i m0 = _mm_loadu_si128((const __m128i *)&pSrc[x * 2 - 1]);
    __m128i m1 = _mm_loadu_si128((const __m128i *)&pSrc[x * 2]);
    __m128i m2 = _mm_loadu_si128((const __m128i *)&pSrc[x * 2 + 1]);
    __m128i m3 = _mm_loadu_si128((const __m128i *)&pSrc[x * 2 + 2]);

    m0 = _mm_and_si128(m0, everySecondMask);
    m1 = _mm_and_si128(m1, everySecondMask);
    m2 = _mm_and_si128(m2, everySecondMask);
    m3 = _mm_and_si128(m3, everySecondMask);

    if constexpr(sizeof(pixel_t) == 1) {
      m1 = _mm_add_epi16(m1, m2);
      m2 = _mm_slli_epi16(m1, 1);
      m1 = _mm_add_epi16(m1, m2);

      m0 = _mm_add_epi16(m0, m1);
      m0 = _mm_add_epi16(m0, m3);
      m0 = _mm_add_epi16(m0, _mm_set1_epi16(4));
      m0 = _mm_srli_epi16(m0, 3);

      m0 = _mm_packus_epi16(m0, m0);
    }
    else {
      m1 = _mm_add_epi32(m1, m2);
      m2 = _mm_slli_epi32(m1, 1);
      m1 = _mm_add_epi32(m1, m2);

      m0 = _mm_add_epi32(m0, m1);
      m0 = _mm_add_epi32(m0, m3);
      m0 = _mm_add_epi32(m0, _mm_set1_epi32(4));
      m0 = _mm_srli_epi32(m0, 3);

      if constexpr(hasSSE41)
        m0 = _mm_packus_epi32(m0, m0);
      else
        m0 = _MM_PACKUS_EPI32(m0, m0);
    }
    _mm_storel_epi64((__m128i *)&pSrc[x], m0);
  }
}

template<typename pixel_t, bool hasSSE41>
static void RB2FilteredVerticalLine_sse2(pixel_t* pDst, const pixel_t* pSrc, int nSrcPitch, int nWidthMMX) {
  // srcPitch is pixel_t level
  for (int x = 0; x < nWidthMMX; x += 8 / sizeof(pixel_t)) {
    // pDst[x] = (pSrc[x - nSrcPitch / sizeof(pixel_t)] + pSrc[x] * 2 + pSrc[x + nSrcPitch / sizeof(pixel_t)] + 2) / 4;
    auto zero = _mm_setzero_si128();
    __m128i m0;

    if constexpr (sizeof(pixel_t) == 1) {
      m0 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i*) & pSrc[x - nSrcPitch]), zero);
      auto m1 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i*) & pSrc[x]), zero);
      auto m2 = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i*) & pSrc[x + nSrcPitch]), zero);
      m0 = _mm_add_epi16(m0, m2);
      m0 = _mm_add_epi16(m0, _mm_slli_epi16(m1, 1));
      m0 = _mm_add_epi16(m0, _mm_set1_epi16(2));
      m0 = _mm_srli_epi16(m0, 2);
      m0 = _mm_packus_epi16(m0, m0);
    }
    else {
      m0 = _mm_unpacklo_epi16(_mm_loadl_epi64((const __m128i*) & pSrc[x - nSrcPitch]), zero);
      auto m1 = _mm_unpacklo_epi16(_mm_loadl_epi64((const __m128i*) & pSrc[x]), zero);
      auto m2 = _mm_unpacklo_epi16(_mm_loadl_epi64((const __m128i*) & pSrc[x + nSrcPitch]), zero);
      m0 = _mm_add_epi32(m0, m2);
      m0 = _mm_add_epi32(m0, _mm_slli_epi32(m1, 1));
      m0 = _mm_add_epi32(m0, _mm_set1_epi32(2));
      m0 = _mm_srli_epi32(m0, 2);

      if constexpr (hasSSE41)
        m0 = _mm_packus_epi32(m0, m0);
      else
        m0 = _MM_PACKUS_EPI32(m0, m0);
    }
    _mm_storel_epi64((__m128i*) & pDst[x], m0);
  }
}


//8-32 bits
// Filtered with 1/4, 1/2, 1/4 filter for smoothing and anti-aliasing - Fizick
// nHeight is dst height which is reduced by 2 source height
template<typename pixel_t>
void RB2FilteredVertical(
  unsigned char *pDst8, const unsigned char *pSrc8, int nDstPitch, int nSrcPitch,
  int nWidth, int nHeight, int y_beg, int y_end, int cpuFlags)
{

  bool isse2 = !!(cpuFlags & CPUF_SSE2);
  bool isse4 = !!(cpuFlags & CPUF_SSE4_1);

  pixel_t *pDst = reinterpret_cast<pixel_t *>(pDst8);
  const pixel_t *pSrc = reinterpret_cast<const pixel_t *>(pSrc8);

  const int pixels_at_a_time = 8 / sizeof(pixel_t);
  int nWidthMMX = (nWidth / pixels_at_a_time) * pixels_at_a_time;
  const int y_loop_b = std::max(y_beg, 1);
  int y = 0;

  if (y_beg < y_loop_b)
  {
    for (int x = 0; x < nWidth; x++)
    {
      if constexpr(sizeof(pixel_t) <= 2)
        pDst[x] = (pSrc[x] + pSrc[x + nSrcPitch / sizeof(pixel_t)] + 1) >> 1;
      else
        pDst[x] = (pSrc[x] + pSrc[x + nSrcPitch / sizeof(pixel_t)]) * 0.5f;
    }
  }

  RB2_jump(y_loop_b, y, pDst, pSrc, nDstPitch, nSrcPitch);

  nSrcPitch /= sizeof(pixel_t);
  nDstPitch /= sizeof(pixel_t);

  if constexpr (sizeof(pixel_t) == 4) {
    for (; y < y_end; ++y)
    {
      for (int x = 0; x < nWidth; x++)
      {
        if constexpr (sizeof(pixel_t) <= 2)
          pDst[x] = (pSrc[x - nSrcPitch] + pSrc[x] * 2 + pSrc[x + nSrcPitch] + 2) >> 2;
        else
          pDst[x] = (pSrc[x - nSrcPitch] + pSrc[x] * 2 + pSrc[x + nSrcPitch]) * (1.0f / 4.0f);
      }

      pDst += nDstPitch;
      pSrc += nSrcPitch * 2;
    }
  }
  else if ((sizeof(pixel_t) == 2) && isse4 && nWidthMMX >= pixels_at_a_time) {
    for (; y < y_end; ++y)
    {
      RB2FilteredVerticalLine_sse2<pixel_t, true>(pDst, pSrc, nSrcPitch, nWidthMMX);
      for (int x = nWidthMMX; x < nWidth; x++)
      {
        pDst[x] = (pSrc[x - nSrcPitch] + pSrc[x] * 2 + pSrc[x + nSrcPitch] + 2) >> 2;
      }

      pDst += nDstPitch;
      pSrc += nSrcPitch * 2;
    }
  }
  else if (isse2 && nWidthMMX >= pixels_at_a_time)
  {
    for (; y < y_end; ++y)
    {
      RB2FilteredVerticalLine_sse2<pixel_t, false>(pDst, pSrc, nSrcPitch, nWidthMMX);
      for (int x = nWidthMMX; x < nWidth; x++)
      {
        pDst[x] = (pSrc[x - nSrcPitch] + pSrc[x] * 2 + pSrc[x + nSrcPitch] + 2) >> 2;
      }

      pDst += nDstPitch;
      pSrc += nSrcPitch * 2;
    }
  }
  else
  {
    // pure C 
    for (; y < y_end; ++y)
    {
      for (int x = 0; x < nWidth; x++)
      {
        if constexpr(sizeof(pixel_t) <= 2)
          pDst[x] = (pSrc[x - nSrcPitch] + pSrc[x] * 2 + pSrc[x + nSrcPitch] + 2) >> 2;
        else
          pDst[x] = (pSrc[x - nSrcPitch] + pSrc[x] * 2 + pSrc[x + nSrcPitch]) * (1.0f / 4.0f);
      }

      pDst += nDstPitch;
      pSrc += nSrcPitch * 2;
    }
  }
}

template<typename pixel_t, bool hasSSE41>
static void RB2FilteredHorizontalInplaceLine_sse2(pixel_t* pSrc, int nWidthMMX) {
  __m128i everySecondMask;
  if constexpr (sizeof(pixel_t) == 1)
    everySecondMask = _mm_set1_epi16(0x00FF);
  else
    everySecondMask = _mm_set1_epi32(0x0000FFFF);

  for (int x = 1; x < nWidthMMX; x += 8 / sizeof(pixel_t)) {
    // pSrc[x] = (pSrc[x * 2 - 1] + pSrc[x * 2] * 2 + pSrc[x * 2 + 1] + 2) >> 2;
    __m128i m0 = _mm_loadu_si128((const __m128i*) & pSrc[x * 2 - 1]);
    __m128i m1 = _mm_loadu_si128((const __m128i*) & pSrc[x * 2]);
    __m128i m2 = _mm_loadu_si128((const __m128i*) & pSrc[x * 2 + 1]);

    m0 = _mm_and_si128(m0, everySecondMask);
    m1 = _mm_and_si128(m1, everySecondMask);
    m2 = _mm_and_si128(m2, everySecondMask);

    if constexpr (sizeof(pixel_t) == 1) {
      m0 = _mm_add_epi16(m0, m2);
      m1 = _mm_slli_epi16(m1, 1);
      m0 = _mm_add_epi16(m0, m1);

      m0 = _mm_add_epi16(m0, _mm_set1_epi16(2));
      m0 = _mm_srli_epi16(m0, 2);

      m0 = _mm_packus_epi16(m0, m0);
    }
    else {
      m0 = _mm_add_epi32(m0, m2);
      m1 = _mm_slli_epi32(m1, 1);
      m0 = _mm_add_epi32(m0, m1);

      m0 = _mm_add_epi32(m0, _mm_set1_epi32(2));
      m0 = _mm_srli_epi32(m0, 2);
      if constexpr (hasSSE41)
        m0 = _mm_packus_epi32(m0, m0);
      else
        m0 = _MM_PACKUS_EPI32(m0, m0);
    }
    _mm_storel_epi64((__m128i*) & pSrc[x], m0);
  }
}


//8-32bits
// Filtered with 1/4, 1/2, 1/4 filter for smoothing and anti-aliasing - Fizick
// nWidth is dst height which is reduced by 2 source width
template<typename pixel_t>
void RB2FilteredHorizontalInplace(
  unsigned char* pSrc8, int nSrcPitch,
  int nWidth, int nHeight, int y_beg, int y_end, int cpuFlags)
{

  // 8 pixels at 8 bit, 4 pixels at 16 bit
  const int pixels_per_cycle = 8 / sizeof(pixel_t);
  int nWidthMMX = 1 + ((nWidth - 2) / pixels_per_cycle) * pixels_per_cycle;
  int y = 0;

  pixel_t* pSrc = reinterpret_cast<pixel_t*>(pSrc8);

  RB2_jump_1(y_beg, y, pSrc, nSrcPitch);

  bool isse2 = !!(cpuFlags & CPUF_SSE2) && nWidthMMX > 1 + pixels_per_cycle;
  bool isse4 = !!(cpuFlags & CPUF_SSE4_1) && nWidthMMX > 1 + pixels_per_cycle;
  for (; y < y_end; ++y)
  {
    const int x = 0;
    pixel_t pSrc0;
    if constexpr (sizeof(pixel_t) <= 2)
      pSrc0 = (pSrc[x * 2] + pSrc[x * 2 + 1] + 1) >> 1;
    else
      pSrc0 = (pSrc[x * 2] + pSrc[x * 2 + 1]) * 0.5f;

    if constexpr (sizeof(pixel_t) == 4) {
      // float, pure C
      for (int x = 1; x < nWidth; x++)
      {
        if constexpr (sizeof(pixel_t) <= 2)
          pSrc[x] = (pSrc[x * 2 - 1] + pSrc[x * 2] * 2 + pSrc[x * 2 + 1] + 2) >> 2;
        else
          pSrc[x] = (pSrc[x * 2 - 1] + pSrc[x * 2] * 2 + pSrc[x * 2 + 1]) * (1.0f / 4.0f);
      }
    }
    else {
      if (sizeof(pixel_t) == 2 && isse4)
      {
        RB2FilteredHorizontalInplaceLine_sse2<uint16_t, true>((uint16_t*)pSrc, nWidthMMX); // very first is skipped
        for (int x = nWidthMMX; x < nWidth; x++)
        {
          pSrc[x] = (pSrc[x * 2 - 1] + pSrc[x * 2] * 2 + pSrc[x * 2 + 1] + 2) >> 2;
        }
      }
      else if (sizeof(pixel_t) == 1 && isse2)
      {
        RB2FilteredHorizontalInplaceLine_sse2<uint8_t, false>((uint8_t *)pSrc, nWidthMMX); // very first is skipped
        for (int x = nWidthMMX; x < nWidth; x++)
        {
          pSrc[x] = (pSrc[x * 2 - 1] + pSrc[x * 2] * 2 + pSrc[x * 2 + 1] + 2) >> 2;
        }
      }
      else
      {
        // pure C
        for (int x = 1; x < nWidth; x++)
        {
          if constexpr (sizeof(pixel_t) <= 2)
            pSrc[x] = (pSrc[x * 2 - 1] + pSrc[x * 2] * 2 + pSrc[x * 2 + 1] + 2) >> 2;
          else
            pSrc[x] = (pSrc[x * 2 - 1] + pSrc[x * 2] * 2 + pSrc[x * 2 + 1]) * (1.0f / 4.0f);
        }
      }
    }
    pSrc[0] = pSrc0;

    pSrc += nSrcPitch / sizeof(pixel_t);
  }
}

//8-32bits
// separable Filtered with 1/4, 1/2, 1/4 filter for smoothing and anti-aliasing - Fizick v.2.5.2
// assume he have enough horizontal dimension for intermediate results (double as final)
template<typename pixel_t>
void RB2Filtered(
  unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch,
  int nWidth, int nHeight, int y_beg, int y_end, int cpuFlags)
{
  RB2FilteredVertical<pixel_t>(pDst, pSrc, nDstPitch, nSrcPitch, nWidth * 2, nHeight, y_beg, y_end, cpuFlags); // intermediate half height
  RB2FilteredHorizontalInplace<pixel_t>(pDst, nDstPitch, nWidth, nHeight, y_beg, y_end, cpuFlags); // inpace width reduction
}



// 8-32bits
// BilinearFiltered with 1/8, 3/8, 3/8, 1/8 filter for smoothing and anti-aliasing - Fizick
// nHeight is dst height which is reduced by 2 source height
template<typename pixel_t>
void RB2BilinearFilteredVertical(
  unsigned char *pDst8, const unsigned char *pSrc8, int nDstPitch, int nSrcPitch,
  int nWidth, int nHeight, int y_beg, int y_end, int cpuFlags)
{

  bool isse2 = (cpuFlags & CPUF_SSE2) != 0;
  bool isse41 = (cpuFlags & CPUF_SSE4_1) != 0;

  // 8 pixels at 8 bit, 4 pixels at 16 bit
  const int pixels_per_cycle = 8 / sizeof(pixel_t);

  int nWidthMMX = (nWidth / pixels_per_cycle) * pixels_per_cycle;

  const int y_loop_b = std::max(y_beg, 1);
  const int y_loop_e = std::min(y_end, nHeight - 1);
  int y = 0;

  pixel_t *pDst = reinterpret_cast<pixel_t *>(pDst8);
  const pixel_t *pSrc = reinterpret_cast<const pixel_t *>(pSrc8);

  if (y_beg < y_loop_b)
  {
    for (int x = 0; x < nWidth; x++)
    {
      if constexpr(sizeof(pixel_t) <= 2)
        pDst[x] = (pSrc[x] + pSrc[x + nSrcPitch / sizeof(pixel_t)] + 1) >> 1;
      else
        pDst[x] = (pSrc[x] + pSrc[x + nSrcPitch / sizeof(pixel_t)]) * 0.5f;
    }
  }

  RB2_jump(y_loop_b, y, pDst, pSrc, nDstPitch, nSrcPitch);

  for (; y < y_loop_e; ++y)
  {
    int startx = 0;
    if (sizeof(pixel_t) <= 2 && isse2 && nWidthMMX >= pixels_per_cycle) {
      if constexpr(sizeof(pixel_t) == 1)
        RB2BilinearFilteredVerticalLine_sse2<uint8_t, false>((uint8_t *)pDst, (uint8_t *)pSrc, nSrcPitch, nWidthMMX);
      else  {
        if(isse41)
          RB2BilinearFilteredVerticalLine_sse2<uint16_t, true>((uint8_t *)pDst, (uint8_t *)pSrc, nSrcPitch, nWidthMMX);
        else
          RB2BilinearFilteredVerticalLine_sse2<uint16_t, false>((uint8_t *)pDst, (uint8_t *)pSrc, nSrcPitch, nWidthMMX);
      }
      startx = nWidthMMX;
    }

    for (int x = startx; x < nWidth; x++)
    {
      if constexpr(sizeof(pixel_t) <= 2)
        pDst[x] = (pSrc[x - nSrcPitch / sizeof(pixel_t)]
          + pSrc[x] * 3
          + pSrc[x + nSrcPitch / sizeof(pixel_t)] * 3
          + pSrc[x + nSrcPitch / sizeof(pixel_t) * 2] + 4) >> 3;
      else
        pDst[x] = (pSrc[x - nSrcPitch / sizeof(pixel_t)]
          + pSrc[x] * 3.0f
          + pSrc[x + nSrcPitch / sizeof(pixel_t)] * 3.0f
          + pSrc[x + nSrcPitch / sizeof(pixel_t) * 2]) * (1.0f / 8.0f);
    }

    pDst += nDstPitch / sizeof(pixel_t);
    pSrc += nSrcPitch / sizeof(pixel_t) * 2;
  }

  RB2_jump(std::max(y_loop_e, 1), y, pDst, pSrc, nDstPitch, nSrcPitch);

  for (; y < y_end; ++y)
  {
    for (int x = 0; x < nWidth; x++)
    {
      if constexpr(sizeof(pixel_t) <= 2)
        pDst[x] = (pSrc[x] + pSrc[x + nSrcPitch / sizeof(pixel_t)] + 1) / 2;
      else
        pDst[x] = (pSrc[x] + pSrc[x + nSrcPitch / sizeof(pixel_t)]) * 0.5f;
    }
    pDst += nDstPitch / sizeof(pixel_t);
    pSrc += nSrcPitch / sizeof(pixel_t) * 2;
  }
}

// 8-32bits
// BilinearFiltered with 1/8, 3/8, 3/8, 1/8 filter for smoothing and anti-aliasing - Fizick
// nWidth is dst height which is reduced by 2 source width
template<typename pixel_t>
void RB2BilinearFilteredHorizontalInplace(
  unsigned char *pSrc8, int nSrcPitch,
  int nWidth, int nHeight, int y_beg, int y_end, int cpuFlags)
{
  bool isse2 = (cpuFlags & CPUF_SSE2) != 0;
  bool isse41 = (cpuFlags & CPUF_SSE4_1) != 0;

  // 8 pixels at 8 bit, 4 pixels at 16 bit
  const int pixels_per_cycle = 8 / sizeof(pixel_t);
  int nWidthMMX = 1 + ((nWidth - 2) / pixels_per_cycle) * pixels_per_cycle;
  //                                                                                        nWidth
  // inplace 90->45                                                                            v
  //           11111111112222222222333333333344444444445555555555666666666677777777778888888888999999999900
  // 012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901
  // 1
  // RRRRRRRRrrrrrrrrRRRRRRRRrrrrrrrrRRRRRRRRrrrrrrrrRRRRRRRRrrrrrrrrRRRRRRRRrrrrrrrrRRRRRRRR    [-1]
  //  RRRRRRRRrrrrrrrrRRRRRRRRrrrrrrrrRRRRRRRRrrrrrrrrRRRRRRRRrrrrrrrrRRRRRRRRrrrrrrrrRRRRRRRR   [0]
  //   RRRRRRRRrrrrrrrrRRRRRRRRrrrrrrrrRRRRRRRRrrrrrrrrRRRRRRRRrrrrrrrrRRRRRRRRrrrrrrrrRRRRRRRR  [+1]
  //    RRRRRRRRrrrrrrrrRRRRRRRRrrrrrrrrRRRRRRRRrrrrrrrrRRRRRRRRrrrrrrrrRRRRRRRRrrrrrrrrRRRRRRRR [+2]
  //  AAAAaaaaAAAAaaaaAAAAaaaaAAAAaaaaAAAAaaaaAAAA

  int y = 0;

  pixel_t *pSrc = reinterpret_cast<pixel_t *>(pSrc8);

  RB2_jump_1(y_beg, y, pSrc, nSrcPitch);

  for (; y < y_end; ++y)
  {
    int x = 0;
    pixel_t pSrc0;
    if constexpr(sizeof(pixel_t) <= 2)
      pSrc0 = (pSrc[x * 2] + pSrc[x * 2 + 1] + 1) >> 1;
    else
      pSrc0 = (pSrc[x * 2] + pSrc[x * 2 + 1]) * 0.5f;

    int xstart = 1;

    if (isse2) {
      if constexpr(sizeof(pixel_t) <= 2)
      {
        if constexpr (sizeof(pixel_t) == 1) {
          RB2BilinearFilteredHorizontalInplaceLine_sse2<uint8_t, false>(pSrc, nWidthMMX, nWidth); // very first is skipped
        }
        else {
          if (isse41)
            RB2BilinearFilteredHorizontalInplaceLine_sse2<uint16_t, true>(pSrc, nWidthMMX, nWidth);
          else
            RB2BilinearFilteredHorizontalInplaceLine_sse2<uint16_t, false>(pSrc, nWidthMMX, nWidth);
        }
        xstart = nWidthMMX;
      }
    }
    
    for (int x = xstart; x < nWidth - 1; x++)
    {
      if constexpr(sizeof(pixel_t) <= 2)
        pSrc[x] = (pSrc[x * 2 - 1] + pSrc[x * 2] * 3 + pSrc[x * 2 + 1] * 3 + pSrc[x * 2 + 2] + 4) >> 3;
      else
        pSrc[x] = (pSrc[x * 2 - 1] + pSrc[x * 2] * 3.0f + pSrc[x * 2 + 1] * 3.0f + pSrc[x * 2 + 2]) * (1.0f / 8.0f);
    }

    pSrc[0] = pSrc0;

    for (int x = std::max(nWidth - 1, 1); x < nWidth; x++)
    {
      if constexpr(sizeof(pixel_t) <= 2)
        pSrc[x] = (pSrc[x * 2] + pSrc[x * 2 + 1] + 1) >> 1;
      else
        pSrc[x] = (pSrc[x * 2] + pSrc[x * 2 + 1]) * 0.5f;
    }

    pSrc += nSrcPitch / sizeof(pixel_t);
  }
}

// 8-32bits
// separable BilinearFiltered with 1/8, 3/8, 3/8, 1/8 filter for smoothing and anti-aliasing - Fizick v.2.5.2
// assume he have enough horizontal dimension for intermediate results (double as final)
template<typename pixel_t>
void RB2BilinearFiltered(
  unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch,
  int nWidth, int nHeight, int y_beg, int y_end, int cpuFlags)
{
  RB2BilinearFilteredVertical<pixel_t>(pDst, pSrc, nDstPitch, nSrcPitch, nWidth * 2, nHeight, y_beg, y_end, cpuFlags); // intermediate half height
  RB2BilinearFilteredHorizontalInplace<pixel_t>(pDst, nDstPitch, nWidth, nHeight, y_beg, y_end, cpuFlags); // inplace width reduction
}


//8-32bits
// filtered Quadratic with 1/64, 9/64, 22/64, 22/64, 9/64, 1/64 filter for smoothing and anti-aliasing - Fizick
// nHeight is dst height which is reduced by 2 source height
template<typename pixel_t>
void RB2QuadraticVertical(
  unsigned char *pDst8, const unsigned char *pSrc8, int nDstPitch, int nSrcPitch,
  int nWidth, int nHeight, int y_beg, int y_end, int cpuFlags)
{
  bool isse2 = (cpuFlags & CPUF_SSE2) != 0;
  bool isse41 = (cpuFlags & CPUF_SSE4_1) != 0;

  // 8 pixels at 8 bit, 4 pixels at 16 bit
  const int pixels_per_cycle = 8 / sizeof(pixel_t);

  int nWidthMMX = (nWidth / pixels_per_cycle) * pixels_per_cycle;

  const int y_loop_b = std::max(y_beg, 1);
  const int y_loop_e = std::min(y_end, nHeight - 1);
  int y = 0;

  pixel_t *pDst = reinterpret_cast<pixel_t *>(pDst8);
  const pixel_t *pSrc = reinterpret_cast<const pixel_t *>(pSrc8);

  if (y_beg < y_loop_b)
  {
    for (int x = 0; x < nWidth; x++)
    {
      if constexpr(sizeof(pixel_t) <= 2)
        pDst[x] = (pSrc[x] + pSrc[x + nSrcPitch / sizeof(pixel_t)] + 1) >> 1;
      else
        pDst[x] = (pSrc[x] + pSrc[x + nSrcPitch / sizeof(pixel_t)]) * 0.5f;
    }
  }

  RB2_jump(y_loop_b, y, pDst, pSrc, nDstPitch, nSrcPitch);

  for (; y < y_loop_e; ++y)
  {
    int xstart = 0;
    if (sizeof(pixel_t) <= 2 && isse2 && nWidthMMX >= pixels_per_cycle) {
      if constexpr(sizeof(pixel_t) == 1)
        RB2QuadraticVerticalLine_sse2<uint8_t, false>((uint8_t *)pDst, (uint8_t *)pSrc, nSrcPitch, nWidthMMX);
      else  {
        if(isse41)
          RB2QuadraticVerticalLine_sse2<uint16_t, true>((uint8_t *)pDst, (uint8_t *)pSrc, nSrcPitch, nWidthMMX);
        else
          RB2QuadraticVerticalLine_sse2<uint16_t, false>((uint8_t *)pDst, (uint8_t *)pSrc, nSrcPitch, nWidthMMX);
      }
      xstart = nWidthMMX;
    }

    for (int x = xstart; x < nWidth; x++)
    {
      if constexpr(sizeof(pixel_t) <= 2)
        pDst[x] = (pSrc[x - nSrcPitch / sizeof(pixel_t) * 2]
          + pSrc[x - nSrcPitch / sizeof(pixel_t)] * 9
          + pSrc[x] * 22
          + pSrc[x + nSrcPitch / sizeof(pixel_t)] * 22
          + pSrc[x + nSrcPitch / sizeof(pixel_t) * 2] * 9
          + pSrc[x + nSrcPitch / sizeof(pixel_t) * 3] + 32) / 64;
      else
        pDst[x] = (pSrc[x - nSrcPitch / sizeof(pixel_t) * 2]
          + pSrc[x - nSrcPitch / sizeof(pixel_t)] * 9
          + pSrc[x] * 22
          + pSrc[x + nSrcPitch / sizeof(pixel_t)] * 22
          + pSrc[x + nSrcPitch / sizeof(pixel_t) * 2] * 9
          + pSrc[x + nSrcPitch / sizeof(pixel_t) * 3]) * (1.0f / 64.0f);
    }

    pDst += nDstPitch / sizeof(pixel_t);
    pSrc += nSrcPitch / sizeof(pixel_t) * 2;
  }

  RB2_jump(std::max(y_loop_e, 1), y, pDst, pSrc, nDstPitch, nSrcPitch);

  for (; y < y_end; ++y)
  {
    for (int x = 0; x < nWidth; x++)
    {
      if constexpr(sizeof(pixel_t) <= 2)
        pDst[x] = (pSrc[x] + pSrc[x + nSrcPitch / sizeof(pixel_t)] + 1) >> 1;
      else
        pDst[x] = (pSrc[x] + pSrc[x + nSrcPitch / sizeof(pixel_t)]) * 0.5f;
    }
    pDst += nDstPitch / sizeof(pixel_t);
    pSrc += nSrcPitch / sizeof(pixel_t) * 2;
  }
}

// 8-32bits
// filtered Quadratic with 1/64, 9/64, 22/64, 22/64, 9/64, 1/64 filter for smoothing and anti-aliasing - Fizick
// nWidth is dst height which is reduced by 2 source width
template<typename pixel_t>
void RB2QuadraticHorizontalInplace(
  unsigned char *pSrc8, int nSrcPitch,
  int nWidth, int nHeight, int y_beg, int y_end, int cpuFlags)
{
  bool isse2 = (cpuFlags & CPUF_SSE2) != 0;
  bool isse41 = (cpuFlags & CPUF_SSE4_1) != 0;

  // 8 pixels at 8 bit, 4 pixels at 16 bit
  const int pixels_per_cycle = 8 / sizeof(pixel_t);
  int nWidthMMX = 1 + ((nWidth - 2) / pixels_per_cycle) * pixels_per_cycle;

  int y = 0;

  pixel_t *pSrc = reinterpret_cast<pixel_t *>(pSrc8);

  RB2_jump_1(y_beg, y, pSrc, nSrcPitch);

  for (; y < y_end; ++y)
  {
    int x = 0;
    pixel_t pSrc0;
    if constexpr(sizeof(pixel_t) <= 2)
      pSrc0 = (pSrc[x * 2] + pSrc[x * 2 + 1] + 1) >> 1; // store temporary
    else
      pSrc0 = (pSrc[x * 2] + pSrc[x * 2 + 1]) * 0.5f; // store temporary

    int xstart = 1;

    if (isse2) {
      if constexpr(sizeof(pixel_t) <= 2)
      {
        if constexpr (sizeof(pixel_t) == 1)
          RB2QuadraticHorizontalInplaceLine_sse2<uint8_t, false>(pSrc, nWidthMMX);
        else {
          if (isse41)
            RB2QuadraticHorizontalInplaceLine_sse2<uint16_t, true>(pSrc, nWidthMMX);
          else
            RB2QuadraticHorizontalInplaceLine_sse2<uint16_t, false>(pSrc, nWidthMMX);
        }
        xstart = nWidthMMX;
      }
    }
    
    for (int x = xstart; x < nWidth - 1; x++)
    {
      if constexpr(sizeof(pixel_t) <= 2)
        pSrc[x] = (pSrc[x * 2 - 2] + pSrc[x * 2 - 1] * 9 + pSrc[x * 2] * 22
          + pSrc[x * 2 + 1] * 22 + pSrc[x * 2 + 2] * 9 + pSrc[x * 2 + 3] + 32) >> 6;
      else
        pSrc[x] = (pSrc[x * 2 - 2] + pSrc[x * 2 - 1] * 9 + pSrc[x * 2] * 22
          + pSrc[x * 2 + 1] * 22 + pSrc[x * 2 + 2] * 9 + pSrc[x * 2 + 3]) * (1.0f / 64.0f);
    }

    pSrc[0] = pSrc0;

    for (int x = std::max(nWidth - 1, 1); x < nWidth; x++)
    {
      if constexpr(sizeof(pixel_t) <= 2)
        pSrc[x] = (pSrc[x * 2] + pSrc[x * 2 + 1] + 1) / 2;
      else
        pSrc[x] = (pSrc[x * 2] + pSrc[x * 2 + 1]) * 0.5f;
    }

    pSrc += nSrcPitch / sizeof(pixel_t);
  }
}

// 8-32bits
// separable filtered Quadratic with 1/64, 9/64, 22/64, 22/64, 9/64, 1/64 filter for smoothing and anti-aliasing - Fizick v.2.5.2
// assume he have enough horizontal dimension for intermediate results (double as final)
template<typename pixel_t>
void RB2Quadratic(
  unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch,
  int nWidth, int nHeight, int y_beg, int y_end, int cpuFlags)
{
  RB2QuadraticVertical<pixel_t>(pDst, pSrc, nDstPitch, nSrcPitch, nWidth * 2, nHeight, y_beg, y_end, cpuFlags); // intermediate half height
  RB2QuadraticHorizontalInplace<pixel_t>(pDst, nDstPitch, nWidth, nHeight, y_beg, y_end, cpuFlags); // inpace width reduction
}



// 8-32bits
// filtered qubic with 1/32, 5/32, 10/32, 10/32, 5/32, 1/32 filter for smoothing and anti-aliasing - Fizick
// nHeight is dst height which is reduced by 2 source height
template<typename pixel_t>
void RB2CubicVertical(
  unsigned char *pDst8, const unsigned char *pSrc8, int nDstPitch, int nSrcPitch,
  int nWidth, int nHeight, int y_beg, int y_end, int cpuFlags)
{
  bool isse2 = (cpuFlags & CPUF_SSE2) != 0;
  bool isse41 = (cpuFlags & CPUF_SSE4_1) != 0;

  // 8 pixels at 8 bit, 4 pixels at 16 bit
  const int pixels_per_cycle = 8 / sizeof(pixel_t);

  int nWidthMMX = (nWidth / pixels_per_cycle) * pixels_per_cycle;
  const int y_loop_b = std::max(y_beg, 1);
  const int y_loop_e = std::min(y_end, nHeight - 1);
  int y = 0;

  pixel_t *pDst = reinterpret_cast<pixel_t *>(pDst8);
  const pixel_t *pSrc = reinterpret_cast<const pixel_t *>(pSrc8);

  if (y_beg < y_loop_b)
  {
    for (int x = 0; x < nWidth; x++)
    {
      if constexpr(sizeof(pixel_t) <= 2)
        pDst[x] = (pSrc[x] + pSrc[x + nSrcPitch / sizeof(pixel_t)] + 1) >> 1;
      else
        pDst[x] = (pSrc[x] + pSrc[x + nSrcPitch / sizeof(pixel_t)]) * 0.5f;
    }
  }

  RB2_jump(y_loop_b, y, pDst, pSrc, nDstPitch, nSrcPitch);

  for (; y < y_loop_e; ++y)
  {
    int xstart = 0;

    if constexpr(sizeof(pixel_t) <= 2) {
      if (isse2 && nWidthMMX >= pixels_per_cycle) {
        if constexpr(sizeof(pixel_t) == 1)
          RB2CubicVerticalLine_sse2<uint8_t, false>((uint8_t *)pDst, (uint8_t *)pSrc, nSrcPitch, nWidthMMX);
        else  {
          if (!isse41)
            RB2CubicVerticalLine_sse2<uint16_t, false>((uint8_t *)pDst, (uint8_t *)pSrc, nSrcPitch, nWidthMMX); // 16 bit nonsse41
          else
            RB2CubicVerticalLine_sse2<uint16_t, true>((uint8_t *)pDst, (uint8_t *)pSrc, nSrcPitch, nWidthMMX); // 16 bit sse41
        }
        xstart = nWidthMMX;
      }
    }

    for (int x = xstart; x < nWidth; x++)
    {
      if constexpr(sizeof(pixel_t) <= 2)
        pDst[x] = (pSrc[x - nSrcPitch / sizeof(pixel_t) * 2]
          + pSrc[x - nSrcPitch / sizeof(pixel_t)] * 5
          + pSrc[x] * 10
          + pSrc[x + nSrcPitch / sizeof(pixel_t)] * 10
          + pSrc[x + nSrcPitch / sizeof(pixel_t) * 2] * 5
          + pSrc[x + nSrcPitch / sizeof(pixel_t) * 3] + 16) >> 5;
      else
        pDst[x] = (pSrc[x - nSrcPitch / sizeof(pixel_t) * 2]
          + pSrc[x - nSrcPitch / sizeof(pixel_t)] * 5
          + pSrc[x] * 10
          + pSrc[x + nSrcPitch / sizeof(pixel_t)] * 10
          + pSrc[x + nSrcPitch / sizeof(pixel_t) * 2] * 5
          + pSrc[x + nSrcPitch / sizeof(pixel_t) * 3]) * (1.0f / 32.0f);
    }

    pDst += nDstPitch / sizeof(pixel_t);
    pSrc += nSrcPitch / sizeof(pixel_t) * 2;
  }

  RB2_jump(std::max(y_loop_e, 1), y, pDst, pSrc, nDstPitch, nSrcPitch);

  for (; y < y_end; ++y)
  {
    for (int x = 0; x < nWidth; x++)
    {
      if constexpr(sizeof(pixel_t) <= 2)
        pDst[x] = (pSrc[x] + pSrc[x + nSrcPitch / sizeof(pixel_t)] + 1) >> 1;
      else
        pDst[x] = (pSrc[x] + pSrc[x + nSrcPitch / sizeof(pixel_t)]) * 0.5f;
    }
    pDst += nDstPitch / sizeof(pixel_t);
    pSrc += nSrcPitch / sizeof(pixel_t) * 2;
  }
}

// 8-32 bits
// filtered qubic with 1/32, 5/32, 10/32, 10/32, 5/32, 1/32 filter for smoothing and anti-aliasing - Fizick
// nWidth is dst height which is reduced by 2 source width
template<typename pixel_t>
void RB2CubicHorizontalInplace(
  unsigned char *pSrc8, int nSrcPitch,
  int nWidth, int nHeight, int y_beg, int y_end, int cpuFlags)
{
  bool isse2 = (cpuFlags & CPUF_SSE2) != 0;
  bool isse41 = (cpuFlags & CPUF_SSE4_1) != 0;

  // 8 pixels at 8 bit, 4 pixels at 16 bit
  const int pixels_per_cycle = 8 / sizeof(pixel_t); 
  // Read pixels safely from:
  // 16 bit sse2: [(nWidthMMX - 1) * 2 + 3 + (8 - 1)]th position
  // 8 bit sse2: [(nWidthMMX - 1) * 2 + 3 + (15 - 1)]th position
  // 8 bit mmx: [(nWidthMMX - 1) * 2 + 3 + (8 - 1)]th position
  // The last - masked out unused - pixel from the loaded simd block is just beyond the valid original (double) width by one.
  // Note: it does not read from illegal memory area because of the frame alignments.
  int nWidthMMX = 1 + ((nWidth - 2) / pixels_per_cycle) * pixels_per_cycle;
  // ** 8 bit, pixels_per_cycle==8, SSE2 (16byte load)
  // nOrigWidth nWidth  nMmxWidth(8bit) loopvars  pixels_when_last_loopvar
  // 32         16      9
  // 34         17      9               1        1*2+3 .. 1*2+3+15 = 20
  // 36         18      17              1,9      9*2+3 .. 9*2+3+15 = 36 (out of 0-35 range, but this very last pixel is not used)
  // 38         19      17
  // 48         24      17
  // 50         25      17              1,9      9*2+3 .. 9*2+3+15 = 36
  // 52         26      25              1,9,17   17*2+3 .. 17*2+3+15 = 52 (out of 0-51 range, but this very last pixel is not used)
  // ** 16 bit, pixels_per_cycle==4, SSE2 (16byte load)
  // nOrigWidth nWidth  nMmxWidth(8bit) loopvars  pixels_when_last_loopvar
  // 32         16      13
  // 34         17      13              1,5,9     9*2+3 .. 9*2+3+7 = 28
  // 36         18      17              1,5,9,13  13*2+3 .. 13*2+3+7 = 36 (out of 0-51 range, but this very last wordpixel is not used)
  int y = 0;

  pixel_t *pSrc = reinterpret_cast<pixel_t *>(pSrc8);

  RB2_jump_1(y_beg, y, pSrc, nSrcPitch); // pitch is byte level

  for (; y < y_end; ++y)
  {
    int x = 0;
    pixel_t pSrcw0;
    if constexpr(sizeof(pixel_t) <= 2)
      pSrcw0 = (pSrc[x * 2] + pSrc[x * 2 + 1] + 1) >> 1; // store temporary
    else
      pSrcw0 = (pSrc[x * 2] + pSrc[x * 2 + 1]) * 0.5f;

    int xstart = 1;

    if constexpr(sizeof(pixel_t) <= 2) {
      if (isse2)
      {
        if constexpr(sizeof(pixel_t) == 1)
        {
          RB2CubicHorizontalInplaceLine_sse2<uint8_t, false>((uint8_t *)pSrc, nWidthMMX);
        }
        else  {
          if (isse41)
            RB2CubicHorizontalInplaceLine_sse2<uint16_t, true>(pSrc, nWidthMMX);
          else
            RB2CubicHorizontalInplaceLine_sse2<uint16_t, false>(pSrc, nWidthMMX);
        }

        xstart = nWidthMMX;
      }
    }

    for (int x = xstart; x < nWidth - 1; x++)
    {
      if constexpr(sizeof(pixel_t) <= 2)
        pSrc[x] = (pSrc[x * 2 - 2] + pSrc[x * 2 - 1] * 5 + pSrc[x * 2] * 10
          + pSrc[x * 2 + 1] * 10 + pSrc[x * 2 + 2] * 5 + pSrc[x * 2 + 3] + 16) >> 5;
      else
        pSrc[x] = (pSrc[x * 2 - 2] + pSrc[x * 2 - 1] * 5 + pSrc[x * 2] * 10
          + pSrc[x * 2 + 1] * 10 + pSrc[x * 2 + 2] * 5 + pSrc[x * 2 + 3]) * (1.0f / 32.0f);
    }

    pSrc[0] = pSrcw0;

    for (int x = std::max(nWidth - 1, 1); x < nWidth; x++)
    {
      if constexpr(sizeof(pixel_t) <= 2)
        pSrc[x] = (pSrc[x * 2] + pSrc[x * 2 + 1] + 1) / 2;
      else
        pSrc[x] = (pSrc[x * 2] + pSrc[x * 2 + 1]) * 0.5f;
    }

    pSrc += nSrcPitch / sizeof(pixel_t);
  }

}

// 8-32bits
// separable filtered cubic with 1/32, 5/32, 10/32, 10/32, 5/32, 1/32 filter for smoothing and anti-aliasing - Fizick v.2.5.2
// assume he have enough horizontal dimension for intermediate results (double as final)
template<typename pixel_t>
void RB2Cubic(
  unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch,
  int nWidth, int nHeight, int y_beg, int y_end, int cpuFlags)
{
  RB2CubicVertical<pixel_t>(pDst, pSrc, nDstPitch, nSrcPitch, nWidth * 2, nHeight, y_beg, y_end, cpuFlags); // intermediate half height
  RB2CubicHorizontalInplace<pixel_t>(pDst, nDstPitch, nWidth, nHeight, y_beg, y_end, cpuFlags); // inplace width reduction
}



// -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -



// 8-32 bits
template<typename pixel_t>
void VerticalBilin(unsigned char *pDst8, const unsigned char *pSrc8, int nDstPitch,
  int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel)
{
  pixel_t *pDst = reinterpret_cast<pixel_t *>(pDst8);
  const pixel_t *pSrc = reinterpret_cast<const pixel_t *>(pSrc8);

  nSrcPitch /= sizeof(pixel_t);
  nDstPitch /= sizeof(pixel_t);

  for (int j = 0; j < nHeight - 1; j++)
  {
    for (int i = 0; i < nWidth; i++)
      if constexpr(sizeof(pixel_t) <= 2)
        pDst[i] = (pSrc[i] + pSrc[i + nSrcPitch] + 1) >> 1; // int
      else
        pDst[i] = (pSrc[i] + pSrc[i + nSrcPitch]) * 0.5f; // float
    pDst += nDstPitch;
    pSrc += nSrcPitch;
  }
  // last row
  for (int i = 0; i < nWidth; i++)
    pDst[i] = pSrc[i];
}

template<typename pixel_t>
void VerticalBilin_sse2(unsigned char *pDst8, const unsigned char *pSrc8, int nDstPitch,
  int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel) {
  (void)bits_per_pixel; // not used

  for (int y = 0; y < nHeight - 1; y++) {
    for (int x = 0; x < nWidth * (int)sizeof(pixel_t); x += 16) {
      __m128i m0 = _mm_loadu_si128((const __m128i *)&pSrc8[x]);
      __m128i m1 = _mm_loadu_si128((const __m128i *)&pSrc8[x + nSrcPitch]);

      if(sizeof(pixel_t) == 1)
        m0 = _mm_avg_epu8(m0, m1);
      else
        m0 = _mm_avg_epu16(m0, m1);
      _mm_storeu_si128((__m128i *)&pDst8[x], m0);
    }

    pSrc8 += nSrcPitch;
    pDst8 += nDstPitch;
  }
  // last row
  for (int x = 0; x < nWidth; x++)
    reinterpret_cast<pixel_t *>(pDst8)[x] = reinterpret_cast<const pixel_t *>(pSrc8)[x];
}


// 8-32 bits
template<typename pixel_t>
void HorizontalBilin(unsigned char *pDst8, const unsigned char *pSrc8, int nDstPitch,
  int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel)
{
  pixel_t *pDst = reinterpret_cast<pixel_t *>(pDst8);
  const pixel_t *pSrc = reinterpret_cast<const pixel_t *>(pSrc8);

  nSrcPitch /= sizeof(pixel_t);
  nDstPitch /= sizeof(pixel_t);

  for (int j = 0; j < nHeight; j++)
  {
    for (int i = 0; i < nWidth - 1; i++) {
      if constexpr (sizeof(pixel_t) <= 2)
        pDst[i] = (pSrc[i] + pSrc[i + 1] + 1) >> 1; // int
      else
        pDst[i] = (pSrc[i] + pSrc[i + 1]) * 0.5f; // float
    }

    pDst[nWidth - 1] = pSrc[nWidth - 1];
    pDst += nDstPitch;
    pSrc += nSrcPitch;
  }
}


template<typename pixel_t>
void HorizontalBilin_sse2(unsigned char *pDst8, const unsigned char *pSrc8, int nDstPitch,
  int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel) {
  (void)bits_per_pixel; // not used

  pixel_t* pDst = reinterpret_cast<pixel_t*>(pDst8);
  const pixel_t* pSrc = reinterpret_cast<const pixel_t*>(pSrc8);

  nSrcPitch /= sizeof(pixel_t);
  nDstPitch /= sizeof(pixel_t);

  for (int y = 0; y < nHeight; y++) {

    // Byte: safe until x < Width-16;
    // V               V
    // 0123456789012345678
    // 0000000000000000   
    //  1111111111111111  

    // uint16_t: safe until x < Width-8;
    // V        V
    // 0123456789012345678
    // 00000000       
    //  11111111      

    // C: safe until x < Width-1;
    // V V       
    // 0123456789012345678
    // 0          
    //  1         

    // v2.7.46: Width-8 uint8_t and Width-16 (uint16_t) instead of Width
    const int safe_limit = sizeof(pixel_t) == 1 ? nWidth - 16 : nWidth - 8;

    int x; // keep after 'for'

    for (x = 0; x < safe_limit; x += 16 / sizeof(pixel_t)) {
      __m128i m0 = _mm_loadu_si128((const __m128i *)&pSrc[x]);
      __m128i m1 = _mm_loadu_si128((const __m128i *)&pSrc[x + 1]);

      if(sizeof(pixel_t) == 1)
        m0 = _mm_avg_epu8(m0, m1);
      else
        m0 = _mm_avg_epu16(m0, m1);
      _mm_storeu_si128((__m128i *)&pDst[x], m0);
    }

    // right side not covered by 16 byte SIMD load
    // go on with x
    for (; x < nWidth - 1; x++) {
      pDst[x] = (pSrc[x] + pSrc[x + 1] + 1) >> 1; // int
    }

    // rightmost
    pDst[nWidth - 1] = pSrc[nWidth - 1];

    pSrc += nSrcPitch;
    pDst += nDstPitch;
  }
}


// 8-32 bits
template<typename pixel_t>
void DiagonalBilin(unsigned char *pDst8, const unsigned char *pSrc8, int nDstPitch,
  int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel)
{
  pixel_t *pDst = reinterpret_cast<pixel_t *>(pDst8);
  const pixel_t *pSrc = reinterpret_cast<const pixel_t *>(pSrc8);

  nSrcPitch /= sizeof(pixel_t);
  nDstPitch /= sizeof(pixel_t);

  for (int j = 0; j < nHeight - 1; j++)
  {
    for (int i = 0; i < nWidth - 1; i++) {
      if constexpr (sizeof(pixel_t) <= 2) {
        pDst[i] = (pSrc[i] + pSrc[i + 1] + pSrc[i + nSrcPitch] + pSrc[i + nSrcPitch + 1] + 2) >> 2; // int
      }
      else {
        pDst[i] = (pSrc[i] + pSrc[i + 1] + pSrc[i + nSrcPitch] + pSrc[i + nSrcPitch + 1]) * 0.25f; // float
      }
    }
    // rightmost pixel
    if constexpr (sizeof(pixel_t) <= 2) {
      pDst[nWidth - 1] = (pSrc[nWidth - 1] + pSrc[nWidth + nSrcPitch - 1] + 1) >> 1;
    }
    else {
      pDst[nWidth - 1] = (pSrc[nWidth - 1] + pSrc[nWidth + nSrcPitch - 1]) * 0.5f;
    }

    pDst += nDstPitch;
    pSrc += nSrcPitch;
  }
  // bottom line
  for (int i = 0; i < nWidth - 1; i++) // except rightmost
    if constexpr(sizeof(pixel_t) <= 2)
      pDst[i] = (pSrc[i] + pSrc[i + 1] + 1) >> 1; // int
    else
      pDst[i] = (pSrc[i] + pSrc[i + 1]) * 0.5f; // float
  // bottom rightmost
  pDst[nWidth - 1] = pSrc[nWidth - 1];
}



template<typename pixel_t, bool hasSSE41>
void DiagonalBilin_sse2(unsigned char *pDst8, const unsigned char *pSrc8, int nDstPitch,
  int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel)
{
  (void)bits_per_pixel; // not used

  pixel_t* pDst = reinterpret_cast<pixel_t*>(pDst8);
  const pixel_t* pSrc = reinterpret_cast<const pixel_t*>(pSrc8);

  nSrcPitch /= sizeof(pixel_t);
  nDstPitch /= sizeof(pixel_t);

  auto zeroes = _mm_setzero_si128();

  int x; // keep value after 'for'

  for (int y = 0; y < nHeight - 1; y++) {
    // Byte: safe until x < Width-8;
    // V        V
    // 0123456789012345678
    // 00000000   
    //  11111111  

    // uint16_t: safe until x < Width-4;
    // V    V
    // 0123456789012345678
    // 0000       
    //  1111      

    // C: safe until x < Width-1;
    // V V       
    // 0123456789012345678
    // 0          
    //  1         

    // v2.7.46: Width-8 uint8_t and Width-4 (uint16_t) instead of Width - 1
    const int safe_limit = sizeof(pixel_t) == 1 ? nWidth - 8 : nWidth - 4;

    for (x = 0; x < safe_limit; x += 8 / sizeof(pixel_t)) {
      __m128i m0 = _mm_loadl_epi64((const __m128i *)&pSrc[x]);
      __m128i m1 = _mm_loadl_epi64((const __m128i *)&pSrc[x + 1]);
      __m128i m2 = _mm_loadl_epi64((const __m128i *)&pSrc[x + nSrcPitch]);
      __m128i m3 = _mm_loadl_epi64((const __m128i *)&pSrc[x + nSrcPitch + 1]);

      if constexpr(sizeof(pixel_t) == 1) {
        m0 = _mm_unpacklo_epi8(m0, zeroes);
        m1 = _mm_unpacklo_epi8(m1, zeroes);
        m2 = _mm_unpacklo_epi8(m2, zeroes);
        m3 = _mm_unpacklo_epi8(m3, zeroes);

        m0 = _mm_add_epi16(m0, m1);
        m2 = _mm_add_epi16(m2, m3);
        m0 = _mm_add_epi16(m0, _mm_set1_epi16(2)); // rounding
        m0 = _mm_add_epi16(m0, m2);

        m0 = _mm_srli_epi16(m0, 2);

        m0 = _mm_packus_epi16(m0, m0);
      }
      else {
        m0 = _mm_unpacklo_epi16(m0, zeroes);
        m1 = _mm_unpacklo_epi16(m1, zeroes);
        m2 = _mm_unpacklo_epi16(m2, zeroes);
        m3 = _mm_unpacklo_epi16(m3, zeroes);

        m0 = _mm_add_epi32(m0, m1);
        m2 = _mm_add_epi32(m2, m3);
        m0 = _mm_add_epi32(m0, _mm_set1_epi32(2)); // rounding
        m0 = _mm_add_epi32(m0, m2);

        m0 = _mm_srli_epi32(m0, 2);
        if(hasSSE41)
          m0 = _mm_packus_epi32(m0, m0);
        else
          m0 = _MM_PACKUS_EPI32(m0, m0);
      }
      _mm_storel_epi64((__m128i *)&pDst[x], m0);
    }

    // right-side pixels not covered by SIMD 8 byte load
    // go on with x
    for (; x < nWidth - 1; x++) { // except rightmost one
      if constexpr (sizeof(pixel_t) <= 2) {
        pDst[x] = (pSrc[x] + pSrc[x + 1] + pSrc[x + nSrcPitch] + pSrc[x + nSrcPitch + 1] + 2) >> 2; // int
      }
      else {
        pDst[x] = (pSrc[x] + pSrc[x + 1] + pSrc[x + nSrcPitch] + pSrc[x + nSrcPitch + 1]) * 0.25f; // float
      }
    }

    // rightmost
    pDst[nWidth - 1] = (pSrc[nWidth - 1] + pSrc[nWidth - 1 + nSrcPitch] + 1) >> 1; // int

    pSrc += nSrcPitch;
    pDst += nDstPitch;
  }

  // bottom line
  for (x = 0; x < nWidth - 1; x += 8 / sizeof(pixel_t)) {
    __m128i m0 = _mm_loadl_epi64((const __m128i *)&pSrc[x]);
    __m128i m1 = _mm_loadl_epi64((const __m128i *)&pSrc[x + 1]);

    if constexpr(sizeof(pixel_t) == 1)
      m0 = _mm_avg_epu8(m0, m1);
    else
      m0 = _mm_avg_epu16(m0, m1);
    _mm_storel_epi64((__m128i *)&pDst[x], m0);
  }

  // right-side pixels not covered by SIMD 8 byte load
  // go on with present x
  for (; x < nWidth - 1; x++) { // except rightmost one
    pDst[x] = (pSrc[x] + pSrc[x + nSrcPitch] + 1) >> 1;
  }

  // bottom rightmost
  pDst[nWidth - 1] = pSrc[nWidth - 1];
}

// so called Wiener interpolation. (sharp, similar to Lanczos ?)
// invarint simplified, 6 taps. Weights: (1, -5, 20, 20, -5, 1)/32 - added by Fizick
// 8-32 bits
template<typename pixel_t>
void VerticalWiener(unsigned char *pDst8, const unsigned char *pSrc8, int nDstPitch,
  int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel)
{
  pixel_t *pDst = reinterpret_cast<pixel_t *>(pDst8);
  const pixel_t *pSrc = reinterpret_cast<const pixel_t *>(pSrc8);

  nSrcPitch /= sizeof(pixel_t);
  nDstPitch /= sizeof(pixel_t);

  const int max_pixel_value = sizeof(pixel_t) == 1 ? 255 : (1 << bits_per_pixel) - 1;

  for (int j = 0; j < 2; j++)
  {
    for (int i = 0; i < nWidth; i++)
      if constexpr(sizeof(pixel_t) <= 2)
        pDst[i] = (pSrc[i] + pSrc[i + nSrcPitch] + 1) >> 1; // int
      else
        pDst[i] = (pSrc[i] + pSrc[i + nSrcPitch]) * 0.5f; // float
    pDst += nDstPitch;
    pSrc += nSrcPitch;
  }
  for (int j = 2; j < nHeight - 4; j++)
  {
    for (int i = 0; i < nWidth; i++)
    {
      if constexpr(sizeof(pixel_t) <= 2)
        pDst[i] = std::min(max_pixel_value, std::max(0,
        ((pSrc[i - nSrcPitch * 2])
          + (-(pSrc[i - nSrcPitch]) + (pSrc[i] << 2) + (pSrc[i + nSrcPitch] << 2) - (pSrc[i + nSrcPitch * 2])) * 5
          + (pSrc[i + nSrcPitch * 3]) + 16) >> 5));
      else
        pDst[i] =
        (
        (pSrc[i - nSrcPitch * 2])
          + (-(pSrc[i - nSrcPitch]) + (pSrc[i] * 4.0f) + (pSrc[i + nSrcPitch] * 4.0f) - (pSrc[i + nSrcPitch * 2])) * 5.0f // weight 30
          + (pSrc[i + nSrcPitch * 3])
          ) * (1.0f / 32.0f); // no clamp for float!
    }
    pDst += nDstPitch;
    pSrc += nSrcPitch;
  }
  for (int j = nHeight - 4; j < nHeight - 1; j++)
  {
    for (int i = 0; i < nWidth; i++)
    {
      if constexpr(sizeof(pixel_t) <= 2)
        pDst[i] = (pSrc[i] + pSrc[i + nSrcPitch] + 1) >> 1;
      else
        pDst[i] = (pSrc[i] + pSrc[i + nSrcPitch]) * 0.5f;
    }

    pDst += nDstPitch;
    pSrc += nSrcPitch;
  }
  // last row
  for (int i = 0; i < nWidth; i++)
    pDst[i] = pSrc[i];
}

// 8 bit from vs
template<typename pixel_t, bool hasSSE41>
void VerticalWiener_sse2(unsigned char *pDst8, const unsigned char *pSrc8, int nDstPitch,
  int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel)
{
  pixel_t *pDst = reinterpret_cast<pixel_t *>(pDst8);
  const pixel_t *pSrc = reinterpret_cast<const pixel_t *>(pSrc8);

  nSrcPitch /= sizeof(pixel_t);
  nDstPitch /= sizeof(pixel_t);

  const int _max_pixel_value = sizeof(pixel_t) == 1 ? 255 : ((1 << bits_per_pixel) - 1);
  const __m128i max_pixel_value = _mm_set1_epi16(_max_pixel_value);

  auto zeroes = _mm_setzero_si128();

  for (int y = 0; y < 2; y++) {
    for (int x = 0; x < nWidth * (int)sizeof(pixel_t); x += 16 / sizeof(pixel_t)) {
      __m128i m0 = _mm_loadu_si128((const __m128i *)&pSrc[x]);
      __m128i m1 = _mm_loadu_si128((const __m128i *)&pSrc[x + nSrcPitch]);

      if constexpr(sizeof(pixel_t) == 1)
        m0 = _mm_avg_epu8(m0, m1);
      else
        m0 = _mm_avg_epu16(m0, m1);
      _mm_storeu_si128((__m128i *)&pDst[x], m0);
    }

    pSrc += nSrcPitch;
    pDst += nDstPitch;
  }

  for (int y = 2; y < nHeight - 4; y++) {
    for (int x = 0; x < nWidth; x += 8 / sizeof(pixel_t)) {
      __m128i m0 = _mm_loadl_epi64((const __m128i *)&pSrc[x - nSrcPitch * 2]);
      __m128i m1 = _mm_loadl_epi64((const __m128i *)&pSrc[x - nSrcPitch]);
      __m128i m2 = _mm_loadl_epi64((const __m128i *)&pSrc[x]);
      __m128i m3 = _mm_loadl_epi64((const __m128i *)&pSrc[x + nSrcPitch]);
      __m128i m4 = _mm_loadl_epi64((const __m128i *)&pSrc[x + nSrcPitch * 2]);
      __m128i m5 = _mm_loadl_epi64((const __m128i *)&pSrc[x + nSrcPitch * 3]);

      if constexpr(sizeof(pixel_t) == 1)
      {
        m0 = _mm_unpacklo_epi8(m0, zeroes);
        m1 = _mm_unpacklo_epi8(m1, zeroes);
        m2 = _mm_unpacklo_epi8(m2, zeroes);
        m3 = _mm_unpacklo_epi8(m3, zeroes);
        m4 = _mm_unpacklo_epi8(m4, zeroes);
        m5 = _mm_unpacklo_epi8(m5, zeroes);

        m2 = _mm_add_epi16(m2, m3);
        m2 = _mm_slli_epi16(m2, 2);

        m1 = _mm_add_epi16(m1, m4);

        m2 = _mm_sub_epi16(m2, m1);
        m3 = _mm_slli_epi16(m2, 2);
        m2 = _mm_add_epi16(m2, m3);

        m0 = _mm_add_epi16(m0, m5);
        m0 = _mm_add_epi16(m0, m2);
        m0 = _mm_add_epi16(m0, _mm_set1_epi16(16));

        m0 = _mm_srai_epi16(m0, 5);
        m0 = _mm_packus_epi16(m0, m0);
      }
      else {
        // 1, -5, 20, 20, -5, 1 magic
        m0 = _mm_unpacklo_epi16(m0, zeroes);
        m1 = _mm_unpacklo_epi16(m1, zeroes);
        m2 = _mm_unpacklo_epi16(m2, zeroes);
        m3 = _mm_unpacklo_epi16(m3, zeroes);
        m4 = _mm_unpacklo_epi16(m4, zeroes);
        m5 = _mm_unpacklo_epi16(m5, zeroes);

        m2 = _mm_add_epi32(m2, m3);
        m2 = _mm_slli_epi32(m2, 2);

        m1 = _mm_add_epi32(m1, m4);

        m2 = _mm_sub_epi32(m2, m1);
        m3 = _mm_slli_epi32(m2, 2);
        m2 = _mm_add_epi32(m2, m3);

        m0 = _mm_add_epi32(m0, m5);
        m0 = _mm_add_epi32(m0, m2);
        m0 = _mm_add_epi32(m0, _mm_set1_epi32(16));

        m0 = _mm_srai_epi32(m0, 5);
        if (hasSSE41) {
          m0 = _mm_packus_epi32(m0, m0);
          m0 = _mm_min_epu16(m0, max_pixel_value);
        }
        else {
          m0 = _MM_PACKUS_EPI32(m0, m0);
          m0 = _MM_MIN_EPU16(m0, max_pixel_value);
        }
      }
      _mm_storel_epi64((__m128i *)&pDst[x], m0);
    }

    pSrc += nSrcPitch;
    pDst += nDstPitch;
  }

  for (int y = nHeight - 4; y < nHeight - 1; y++) {
    for (int x = 0; x < nWidth * (int)sizeof(pixel_t); x += 16 / sizeof(pixel_t)) {
      __m128i m0 = _mm_loadu_si128((const __m128i *)&pSrc[x]);
      __m128i m1 = _mm_loadu_si128((const __m128i *)&pSrc[x + nSrcPitch]);

      if constexpr(sizeof(pixel_t) == 1)
        m0 = _mm_avg_epu8(m0, m1);
      else
        m0 = _mm_avg_epu16(m0, m1);
      _mm_storeu_si128((__m128i *)&pDst[x], m0);
    }

    pSrc += nSrcPitch;
    pDst += nDstPitch;
  }

  for (int x = 0; x < nWidth; x++)
    pDst[x] = pSrc[x];
}

// 8-32 bits
template<typename pixel_t>
void HorizontalWiener(unsigned char *pDst8, const unsigned char *pSrc8, int nDstPitch,
  int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel)
{
  pixel_t *pDst = reinterpret_cast<pixel_t *>(pDst8);
  const pixel_t *pSrc = reinterpret_cast<const pixel_t *>(pSrc8);

  nSrcPitch /= sizeof(pixel_t);
  nDstPitch /= sizeof(pixel_t);

  const int max_pixel_value = sizeof(pixel_t) == 1 ? 255 : (1 << bits_per_pixel) - 1;

  for (int j = 0; j < nHeight; j++)
  {
    if constexpr(sizeof(pixel_t) <= 2) {
      pDst[0] = (pSrc[0] + pSrc[1] + 1) >> 1;
      pDst[1] = (pSrc[1] + pSrc[2] + 1) >> 1;
    }
    else {
      // float
      pDst[0] = (pSrc[0] + pSrc[1]) * 0.5f;
      pDst[1] = (pSrc[1] + pSrc[2]) * 0.5f;
    }
    for (int i = 2; i < nWidth - 4; i++)
    {
      if constexpr(sizeof(pixel_t) <= 2)
        pDst[i] = std::min(max_pixel_value, std::max(0, ((pSrc[i - 2]) + (-(pSrc[i - 1]) + (pSrc[i] << 2)
          + (pSrc[i + 1] << 2) - (pSrc[i + 2])) * 5 + (pSrc[i + 3]) + 16) >> 5));
      else
        pDst[i] = ((pSrc[i - 2]) + (-(pSrc[i - 1]) + (pSrc[i] * 4.0f)
          + (pSrc[i + 1] * 4.0f) - (pSrc[i + 2])) * 5.0f + (pSrc[i + 3])) * (1.0f / 32.0f);
    }
    for (int i = nWidth - 4; i < nWidth - 1; i++)
      if constexpr(sizeof(pixel_t) <= 2)
        pDst[i] = (pSrc[i] + pSrc[i + 1] + 1) >> 1;
      else
        pDst[i] = (pSrc[i] + pSrc[i + 1]) * 0.5f;

    pDst[nWidth - 1] = pSrc[nWidth - 1];
    pDst += nDstPitch;
    pSrc += nSrcPitch;
  }
}

template<typename pixel_t, bool hasSSE41>
void HorizontalWiener_sse2(unsigned char *pDst8, const unsigned char *pSrc8, int nDstPitch,
  int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel) {

  pixel_t *pDst = reinterpret_cast<pixel_t *>(pDst8);
  const pixel_t *pSrc = reinterpret_cast<const pixel_t *>(pSrc8);

  nSrcPitch /= sizeof(pixel_t);
  nDstPitch /= sizeof(pixel_t);

  const int _max_pixel_value = sizeof(pixel_t) == 1 ? 255 : ((1 << bits_per_pixel) - 1);
  const __m128i max_pixel_value = _mm_set1_epi16(_max_pixel_value);

  auto zeroes = _mm_setzero_si128();

  for (int y = 0; y < nHeight; y++) {
    pDst[0] = (pSrc[0] + pSrc[1] + 1) >> 1;
    pDst[1] = (pSrc[1] + pSrc[2] + 1) >> 1;

    // Byte: safe until x < Width-10;
    //   V          V
    // 0123456789012345678
    // 22222222     
    //  11111111    
    //   00000000   
    //    11111111  
    //     22222222 
    //      33333333

    // uint16_t: safe until x < Width-6;
    //   V      V
    // 0123456789012345678
    // 2222         
    //  1111        
    //   0000       
    //    1111      
    //     2222     
    //      3333    

    // v2.7.46: Width-10 uint8_t and Width-6 (uint16_t) instead of Width - 7
    const int safe_limit = sizeof(pixel_t) == 1 ? nWidth - 10 : nWidth - 6;

    int x; // keep after 'for'

    for (x = 2; x < safe_limit; x += 8 / sizeof(pixel_t)) {
      __m128i m0 = _mm_loadl_epi64((const __m128i *)&pSrc[x - 2]);
      __m128i m1 = _mm_loadl_epi64((const __m128i *)&pSrc[x - 1]);
      __m128i m2 = _mm_loadl_epi64((const __m128i *)&pSrc[x]);
      __m128i m3 = _mm_loadl_epi64((const __m128i *)&pSrc[x + 1]);
      __m128i m4 = _mm_loadl_epi64((const __m128i *)&pSrc[x + 2]);
      __m128i m5 = _mm_loadl_epi64((const __m128i *)&pSrc[x + 3]);

      if constexpr(sizeof(pixel_t) == 1)
      {

        m0 = _mm_unpacklo_epi8(m0, zeroes);
        m1 = _mm_unpacklo_epi8(m1, zeroes);
        m2 = _mm_unpacklo_epi8(m2, zeroes);
        m3 = _mm_unpacklo_epi8(m3, zeroes);
        m4 = _mm_unpacklo_epi8(m4, zeroes);
        m5 = _mm_unpacklo_epi8(m5, zeroes);

        m2 = _mm_add_epi16(m2, m3);
        m2 = _mm_slli_epi16(m2, 2);

        m1 = _mm_add_epi16(m1, m4);

        m2 = _mm_sub_epi16(m2, m1);
        m3 = _mm_slli_epi16(m2, 2);
        m2 = _mm_add_epi16(m2, m3);

        m0 = _mm_add_epi16(m0, m5);
        m0 = _mm_add_epi16(m0, m2);
        m0 = _mm_add_epi16(m0, _mm_set1_epi16(16));

        m0 = _mm_srai_epi16(m0, 5);
        m0 = _mm_packus_epi16(m0, m0);
      }
      else {
        m0 = _mm_unpacklo_epi16(m0, zeroes);
        m1 = _mm_unpacklo_epi16(m1, zeroes);
        m2 = _mm_unpacklo_epi16(m2, zeroes);
        m3 = _mm_unpacklo_epi16(m3, zeroes);
        m4 = _mm_unpacklo_epi16(m4, zeroes);
        m5 = _mm_unpacklo_epi16(m5, zeroes);

        m2 = _mm_add_epi32(m2, m3);
        m2 = _mm_slli_epi32(m2, 2);

        m1 = _mm_add_epi32(m1, m4);

        m2 = _mm_sub_epi32(m2, m1);
        m3 = _mm_slli_epi32(m2, 2);
        m2 = _mm_add_epi32(m2, m3);

        m0 = _mm_add_epi32(m0, m5);
        m0 = _mm_add_epi32(m0, m2);
        m0 = _mm_add_epi32(m0, _mm_set1_epi32(16));

        m0 = _mm_srai_epi32(m0, 5);
        if (hasSSE41) {
          m0 = _mm_packus_epi32(m0, m0);
          m0 = _mm_min_epu16(m0, max_pixel_value);
        }
        else {
          m0 = _MM_PACKUS_EPI32(m0, m0);
          m0 = _MM_MIN_EPU16(m0, max_pixel_value);
        }
      }
      _mm_storel_epi64((__m128i *)&pDst[x], m0);
    }

    // go on with x
    for (; x < nWidth - 4; x++)
        pDst[x] = std::min(_max_pixel_value, std::max(0, ((pSrc[x - 2]) + (-(pSrc[x - 1]) + (pSrc[x] << 2)
          + (pSrc[x + 1] << 2) - (pSrc[x + 2])) * 5 + (pSrc[x + 3]) + 16) >> 5));

    // go on with x
    for (; x < nWidth - 1; x++)
      pDst[x] = (pSrc[x] + pSrc[x + 1] + 1) >> 1;

    pDst[nWidth - 1] = pSrc[nWidth - 1];

    pDst += nDstPitch;
    pSrc += nSrcPitch;
  }
}

#if 0 // not used
template<typename pixel_t>
void DiagonalWiener(unsigned char *pDst8, const unsigned char *pSrc8, int nDstPitch,
  int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel)
{
  pixel_t *pDst = reinterpret_cast<pixel_t *>(pDst8);
  const pixel_t *pSrc = reinterpret_cast<const pixel_t *>(pSrc8);

  nSrcPitch /= sizeof(pixel_t);
  nDstPitch /= sizeof(pixel_t);

  const int max_pixel_value = sizeof(pixel_t) == 1 ? 255 : (1 << bits_per_pixel) - 1;

  for (int j = 0; j < 2; j++)
  {
    for (int i = 0; i < nWidth - 1; i++)
      pDst[i] = (pSrc[i] + pSrc[i + 1] + pSrc[i + nSrcPitch] + pSrc[i + nSrcPitch + 1] + 2) >> 2;

    pDst[nWidth - 1] = (pSrc[nWidth - 1] + pSrc[nWidth + nSrcPitch - 1] + 1) >> 1;
    pDst += nDstPitch;
    pSrc += nSrcPitch;
  }
  for (int j = 2; j < nHeight - 4; j++)
  {
    for (int i = 0; i < 2; i++)
      pDst[i] = (pSrc[i] + pSrc[i + 1] + pSrc[i + nSrcPitch] + pSrc[i + nSrcPitch + 1] + 2) >> 2;
    for (int i = 2; i < nWidth - 4; i++)
    {
      pDst[i] = std::min(max_pixel_value, std::max(0,
        ((pSrc[i - 2 - nSrcPitch * 2]) + (-(pSrc[i - 1 - nSrcPitch]) + (pSrc[i] << 2)
          + (pSrc[i + 1 + nSrcPitch] << 2) - (pSrc[i + 2 + nSrcPitch * 2] << 2)) * 5 + (pSrc[i + 3 + nSrcPitch * 3])
          + (pSrc[i + 3 - nSrcPitch * 2]) + (-(pSrc[i + 2 - nSrcPitch]) + (pSrc[i + 1] << 2)
            + (pSrc[i + nSrcPitch] << 2) - (pSrc[i - 1 + nSrcPitch * 2])) * 5 + (pSrc[i - 2 + nSrcPitch * 3])
          + 32) >> 6));
    }
    for (int i = nWidth - 4; i < nWidth - 1; i++)
      pDst[i] = (pSrc[i] + pSrc[i + 1] + pSrc[i + nSrcPitch] + pSrc[i + nSrcPitch + 1] + 2) >> 2;

    pDst[nWidth - 1] = (pSrc[nWidth - 1] + pSrc[nWidth + nSrcPitch - 1] + 1) >> 1;
    pDst += nDstPitch;
    pSrc += nSrcPitch;
  }
  for (int j = nHeight - 4; j < nHeight - 1; j++)
  {
    for (int i = 0; i < nWidth - 1; i++)
      pDst[i] = (pSrc[i] + pSrc[i + 1] + pSrc[i + nSrcPitch] + pSrc[i + nSrcPitch + 1] + 2) >> 2;

    pDst[nWidth - 1] = (pSrc[nWidth - 1] + pSrc[nWidth + nSrcPitch - 1] + 1) >> 1;
    pDst += nDstPitch;
    pSrc += nSrcPitch;
  }
  for (int i = 0; i < nWidth - 1; i++)
    pDst[i] = (pSrc[i] + pSrc[i + 1] + 1) >> 1;
  pDst[nWidth - 1] = pSrc[nWidth - 1];
}
#endif

// 8-32 bits
// bicubic (Catmull-Rom 4 taps interpolation)
template<typename pixel_t>
void VerticalBicubic(unsigned char *pDst8, const unsigned char *pSrc8, int nDstPitch,
  int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel)
{
  pixel_t *pDst = reinterpret_cast<pixel_t *>(pDst8);
  const pixel_t *pSrc = reinterpret_cast<const pixel_t *>(pSrc8);

  nSrcPitch /= sizeof(pixel_t);
  nDstPitch /= sizeof(pixel_t);

  const int max_pixel_value = sizeof(pixel_t) == 1 ? 255 : (1 << bits_per_pixel) - 1;

  for (int j = 0; j < 1; j++)
  {
    for (int i = 0; i < nWidth; i++)
      if constexpr(sizeof(pixel_t) <= 2)
        pDst[i] = (pSrc[i] + pSrc[i + nSrcPitch] + 1) >> 1; // int
      else
        pDst[i] = (pSrc[i] + pSrc[i + nSrcPitch]) * 0.5f; // float
    pDst += nDstPitch;
    pSrc += nSrcPitch;
  }
  for (int j = 1; j < nHeight - 3; j++)
  {
    for (int i = 0; i < nWidth; i++)
    {
      if constexpr(sizeof(pixel_t) <= 2)
        pDst[i] = std::min(max_pixel_value, std::max(0,
        (-pSrc[i - nSrcPitch] - pSrc[i + nSrcPitch * 2] + (pSrc[i] + pSrc[i + nSrcPitch]) * 9 + 8) >> 4));
      else
        pDst[i] = 
        (-pSrc[i - nSrcPitch] - pSrc[i + nSrcPitch * 2] + (pSrc[i] + pSrc[i + nSrcPitch]) * 9.0f) * (1.0f / 16.0f);
    }
    pDst += nDstPitch;
    pSrc += nSrcPitch;
  }
  for (int j = nHeight - 3; j < nHeight - 1; j++)
  {
    for (int i = 0; i < nWidth; i++)
    {
      if constexpr(sizeof(pixel_t) <= 2)
        pDst[i] = (pSrc[i] + pSrc[i + nSrcPitch] + 1) >> 1; // int
      else
        pDst[i] = (pSrc[i] + pSrc[i + nSrcPitch]) * 0.5f; // float
    }

    pDst += nDstPitch;
    pSrc += nSrcPitch;
  }
  // last row
  for (int i = 0; i < nWidth; i++)
    pDst[i] = pSrc[i];
}

template<typename pixel_t, bool hasSSE41>
void VerticalBicubic_sse2(unsigned char* pDst8, const unsigned char* pSrc8, int nDstPitch,
  int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel)
{
  pixel_t* pDst = reinterpret_cast<pixel_t*>(pDst8);
  const pixel_t* pSrc = reinterpret_cast<const pixel_t*>(pSrc8);

  nSrcPitch /= sizeof(pixel_t);
  nDstPitch /= sizeof(pixel_t);

  const int _max_pixel_value = sizeof(pixel_t) == 1 ? 255 : ((1 << bits_per_pixel) - 1);
  const __m128i max_pixel_value = _mm_set1_epi16(_max_pixel_value);

  auto zeroes = _mm_setzero_si128();

  for (int y = 0; y < 1; y++) {
    for (int x = 0; x < nWidth * (int)sizeof(pixel_t); x += 16 / sizeof(pixel_t)) {
      __m128i m0 = _mm_loadu_si128((const __m128i*) & pSrc[x]);
      __m128i m1 = _mm_loadu_si128((const __m128i*) & pSrc[x + nSrcPitch]);

      if constexpr (sizeof(pixel_t) == 1)
        m0 = _mm_avg_epu8(m0, m1);
      else
        m0 = _mm_avg_epu16(m0, m1);
      _mm_storeu_si128((__m128i*) & pDst[x], m0);
    }

    pSrc += nSrcPitch;
    pDst += nDstPitch;
  }

  for (int y = 1; y < nHeight - 3; y++) {
    for (int x = 0; x < nWidth; x += 8 / sizeof(pixel_t)) {
      // (-pSrc[i - nSrcPitch] - pSrc[i + nSrcPitch * 2] + (pSrc[i] + pSrc[i + nSrcPitch]) * 9 + 8) >> 4));
      // (-m1 -m4 + (m2+m3)*9 + 8) >> 4
      const pixel_t* pSrc0 = pSrc + x;
      __m128i m1 = _mm_loadl_epi64((const __m128i*)(pSrc0 - nSrcPitch));
      __m128i m2 = _mm_loadl_epi64((const __m128i*)(pSrc0));
      __m128i m3 = _mm_loadl_epi64((const __m128i*)(pSrc0 + nSrcPitch));
      __m128i m4 = _mm_loadl_epi64((const __m128i*)(pSrc0 + nSrcPitch * 2 ));
      __m128i res;

      if constexpr (sizeof(pixel_t) == 1)
      {
        const auto rounder_eight = _mm_set1_epi16(8);

        if constexpr (hasSSE41) {
          m1 = _mm_cvtepu8_epi16(m1);
          m2 = _mm_cvtepu8_epi16(m2);
          m3 = _mm_cvtepu8_epi16(m3);
          m4 = _mm_cvtepu8_epi16(m4);
        }
        else {
          m1 = _mm_unpacklo_epi8(m1, zeroes);
          m2 = _mm_unpacklo_epi8(m2, zeroes);
          m3 = _mm_unpacklo_epi8(m3, zeroes);
          m4 = _mm_unpacklo_epi8(m4, zeroes);
        }

        auto tmp1 = _mm_add_epi16(m2, m3);
        auto tmp2 = _mm_add_epi16(_mm_slli_epi16(tmp1, 3), tmp1); // *9
        auto tmp3 = _mm_sub_epi16(_mm_sub_epi16(tmp2, m1), m4);
        res = _mm_srai_epi16(_mm_add_epi16(tmp3, rounder_eight), 4);
        res = _mm_packus_epi16(res, res);
      }
      else {
        const auto rounder_eight = _mm_set1_epi32(8);
        m1 = _mm_unpacklo_epi16(m1, zeroes);
        m2 = _mm_unpacklo_epi16(m2, zeroes);
        m3 = _mm_unpacklo_epi16(m3, zeroes);
        m4 = _mm_unpacklo_epi16(m4, zeroes);

        auto tmp1 = _mm_add_epi32(m2, m3);
        auto tmp2 = _mm_add_epi32(_mm_slli_epi32(tmp1, 3), tmp1); // *9
        auto tmp3 = _mm_sub_epi32(_mm_sub_epi32(tmp2, m1), m4);
        res = _mm_srai_epi32(_mm_add_epi32(tmp3, rounder_eight), 4);

        if constexpr(hasSSE41) {
          res = _mm_packus_epi32(res, res);
          res = _mm_min_epu16(res, max_pixel_value);
        }
        else {
          res = _MM_PACKUS_EPI32(res, res);
          res = _MM_MIN_EPU16(res, max_pixel_value);
        }
      }
      _mm_storel_epi64((__m128i*) & pDst[x], res);
    }

    pSrc += nSrcPitch;
    pDst += nDstPitch;
  }

  for (int y = nHeight - 3; y < nHeight - 1; y++) {
    for (int x = 0; x < nWidth * (int)sizeof(pixel_t); x += 16 / sizeof(pixel_t)) {
      __m128i m0 = _mm_loadu_si128((const __m128i*) & pSrc[x]);
      __m128i m1 = _mm_loadu_si128((const __m128i*) & pSrc[x + nSrcPitch]);

      if constexpr (sizeof(pixel_t) == 1)
        m0 = _mm_avg_epu8(m0, m1);
      else
        m0 = _mm_avg_epu16(m0, m1);
      _mm_storeu_si128((__m128i*) & pDst[x], m0);
    }

    pSrc += nSrcPitch;
    pDst += nDstPitch;
  }

  for (int x = 0; x < nWidth; x++)
    pDst[x] = pSrc[x];
}

// 8-32bits
template<typename pixel_t>
void HorizontalBicubic(unsigned char *pDst8, const unsigned char *pSrc8, int nDstPitch,
  int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel)
{
  pixel_t *pDst = reinterpret_cast<pixel_t *>(pDst8);
  const pixel_t *pSrc = reinterpret_cast<const pixel_t *>(pSrc8);

  nSrcPitch /= sizeof(pixel_t);
  nDstPitch /= sizeof(pixel_t);

  const int max_pixel_value = sizeof(pixel_t) == 1 ? 255 : (1 << bits_per_pixel) - 1;

  for (int j = 0; j < nHeight; j++)
  {
    if constexpr(sizeof(pixel_t) <= 2)
      pDst[0] = (pSrc[0] + pSrc[1] + 1) >> 1; // int
    else
      pDst[0] = (pSrc[0] + pSrc[1] + 1) * 0.5f; // float
    for (int i = 1; i < nWidth - 3; i++)
    {
      if constexpr(sizeof(pixel_t) <= 2)
        pDst[i] = std::min(max_pixel_value, std::max(0,
        (-(pSrc[i - 1] + pSrc[i + 2]) + (pSrc[i] + pSrc[i + 1]) * 9 + 8) >> 4));
      else
        pDst[i] = 
        (-(pSrc[i - 1] + pSrc[i + 2]) + (pSrc[i] + pSrc[i + 1]) * 9.0f) * (1.0f / 16.0f); // no clamp for float
    }
    for (int i = nWidth - 3; i < nWidth - 1; i++)
      if constexpr(sizeof(pixel_t) <= 2)
        pDst[i] = (pSrc[i] + pSrc[i + 1] + 1) >> 1; // int
      else
        pDst[i] = (pSrc[i] + pSrc[i + 1]) * 0.5f; // float

    pDst[nWidth - 1] = pSrc[nWidth - 1];
    pDst += nDstPitch;
    pSrc += nSrcPitch;
  }
}

template<typename pixel_t, bool hasSSE41>
void HorizontalBicubic_sse2(unsigned char* pDst8, const unsigned char* pSrc8, int nDstPitch,
  int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel) {

  pixel_t* pDst = reinterpret_cast<pixel_t*>(pDst8);
  const pixel_t* pSrc = reinterpret_cast<const pixel_t*>(pSrc8);

  nSrcPitch /= sizeof(pixel_t);
  nDstPitch /= sizeof(pixel_t);

  const int _max_pixel_value = sizeof(pixel_t) == 1 ? 255 : ((1 << bits_per_pixel) - 1);
  const __m128i max_pixel_value = _mm_set1_epi16(_max_pixel_value);

  auto zeroes = _mm_setzero_si128();

  for (int y = 0; y < nHeight; y++) {
    pDst[0] = (pSrc[0] + pSrc[1] + 1) >> 1;

    // Byte: safe until x < Width-9;
    //  V         V
    // 0123456789012345678
    // 11111111    
    //  00000000   
    //   11111111  
    //    22222222 

    // uint16_t: safe until x < Width-5;
    //  V     V
    // 0123456789012345678
    // 1111        
    //  0000       
    //   1111      
    //    2222     

    // v2.7.46: Width-9 uint8_t and Width-5 (uint16_t) instead of Width - 7
    const int safe_limit = sizeof(pixel_t) == 1 ? nWidth - 9 : nWidth - 5;

    int x; // keep value after 'for'

    // 1 to nWidth - 3 (wiener: 2 to nWidth - 4)
    for (x = 1; x < safe_limit; x += 8 / sizeof(pixel_t)) {
      // (-m1 -m4 + (m2+m3)*9 + 8) >> 4
      __m128i m1 = _mm_loadl_epi64((const __m128i*) & pSrc[x - 1]);
      __m128i m2 = _mm_loadl_epi64((const __m128i*) & pSrc[x]);
      __m128i m3 = _mm_loadl_epi64((const __m128i*) & pSrc[x + 1]);
      __m128i m4 = _mm_loadl_epi64((const __m128i*) & pSrc[x + 2]);
      __m128i res;

      if constexpr (sizeof(pixel_t) == 1)
      {
        const auto rounder_eight = _mm_set1_epi16(8);

        m1 = _mm_unpacklo_epi8(m1, zeroes);
        m2 = _mm_unpacklo_epi8(m2, zeroes);
        m3 = _mm_unpacklo_epi8(m3, zeroes);
        m4 = _mm_unpacklo_epi8(m4, zeroes);

        auto tmp1 = _mm_add_epi16(m2, m3);
        auto tmp2 = _mm_add_epi16(_mm_slli_epi16(tmp1, 3), tmp1); // *9
        auto tmp3 = _mm_sub_epi16(_mm_sub_epi16(tmp2, m1), m4);
        res = _mm_srai_epi16(_mm_add_epi16(tmp3, rounder_eight), 4);
        res = _mm_packus_epi16(res, res);
      }
      else {
        const auto rounder_eight = _mm_set1_epi32(8);
        m1 = _mm_unpacklo_epi16(m1, zeroes);
        m2 = _mm_unpacklo_epi16(m2, zeroes);
        m3 = _mm_unpacklo_epi16(m3, zeroes);
        m4 = _mm_unpacklo_epi16(m4, zeroes);

        auto tmp1 = _mm_add_epi32(m2, m3);
        auto tmp2 = _mm_add_epi32(_mm_slli_epi32(tmp1, 3), tmp1); // *9
        auto tmp3 = _mm_sub_epi32(_mm_sub_epi32(tmp2, m1), m4);
        res = _mm_srai_epi32(_mm_add_epi32(tmp3, rounder_eight), 4);

        if (hasSSE41) {
          res = _mm_packus_epi32(res, res);
          res = _mm_min_epu16(res, max_pixel_value);
        }
        else {
          res = _MM_PACKUS_EPI32(res, res);
          res = _MM_MIN_EPU16(res, max_pixel_value);
        }
      }
      _mm_storel_epi64((__m128i*) & pDst[x], res);
    }

    // go on with x
    for (; x < nWidth - 3; x++)
      pDst[x] = std::min(_max_pixel_value, std::max(0,
        (-(pSrc[x - 1] + pSrc[x + 2]) + (pSrc[x] + pSrc[x + 1]) * 9 + 8) >> 4));

    // go on with x
    for (; x < nWidth - 1; x++)
      pDst[x] = (pSrc[x] + pSrc[x + 1] + 1) >> 1;

    pDst[nWidth - 1] = pSrc[nWidth - 1];

    pDst += nDstPitch;
    pSrc += nSrcPitch;
  }
}

#if 0
// not used
template<typename pixel_t>
void DiagonalBicubic(unsigned char *pDst8, const unsigned char *pSrc8, int nDstPitch,
  int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel)
{
  pixel_t *pDst = reinterpret_cast<pixel_t *>(pDst8);
  const pixel_t *pSrc = reinterpret_cast<const pixel_t *>(pSrc8);

  nSrcPitch /= sizeof(pixel_t);
  nDstPitch /= sizeof(pixel_t);

  const int max_pixel_value = sizeof(pixel_t) == 1 ? 255 : (1 << bits_per_pixel) - 1;

  for (int j = 0; j < 1; j++)
  {
    for (int i = 0; i < nWidth - 1; i++)
      pDst[i] = (pSrc[i] + pSrc[i + 1] + pSrc[i + nSrcPitch] + pSrc[i + nSrcPitch + 1] + 2) >> 2;

    pDst[nWidth - 1] = (pSrc[nWidth - 1] + pSrc[nWidth + nSrcPitch - 1] + 1) >> 1;
    pDst += nDstPitch;
    pSrc += nSrcPitch;
  }
  for (int j = 1; j < nHeight - 3; j++)
  {
    for (int i = 0; i < 1; i++)
      pDst[i] = (pSrc[i] + pSrc[i + 1] + pSrc[i + nSrcPitch] + pSrc[i + nSrcPitch + 1] + 2) >> 2;
    for (int i = 1; i < nWidth - 3; i++)
    {
      pDst[i] = std::min(max_pixel_value, std::max(0,
        (-pSrc[i - 1 - nSrcPitch] - pSrc[i + 2 + nSrcPitch * 2] + (pSrc[i] + pSrc[i + 1 + nSrcPitch]) * 9
          - pSrc[i - 1 + nSrcPitch * 2] - pSrc[i + 2 - nSrcPitch] + (pSrc[i + 1] + pSrc[i + nSrcPitch]) * 9
          + 16) >> 5));
    }
    for (int i = nWidth - 3; i < nWidth - 1; i++)
      pDst[i] = (pSrc[i] + pSrc[i + 1] + pSrc[i + nSrcPitch] + pSrc[i + nSrcPitch + 1] + 2) >> 2;

    pDst[nWidth - 1] = (pSrc[nWidth - 1] + pSrc[nWidth + nSrcPitch - 1] + 1) >> 1;
    pDst += nDstPitch;
    pSrc += nSrcPitch;
  }
  for (int j = nHeight - 3; j < nHeight - 1; j++)
  {
    for (int i = 0; i < nWidth - 1; i++)
      pDst[i] = (pSrc[i] + pSrc[i + 1] + pSrc[i + nSrcPitch] + pSrc[i + nSrcPitch + 1] + 2) >> 2;

    pDst[nWidth - 1] = (pSrc[nWidth - 1] + pSrc[nWidth + nSrcPitch - 1] + 1) >> 1;
    pDst += nDstPitch;
    pSrc += nSrcPitch;
  }
  for (int i = 0; i < nWidth - 1; i++)
    pDst[i] = (pSrc[i] + pSrc[i + 1] + 1) >> 1;
  pDst[nWidth - 1] = pSrc[nWidth - 1];
}
#endif


template<typename pixel_t>
void Average2_sse2(unsigned char *pDst8, const unsigned char *pSrc1_8, const unsigned char *pSrc2_8,
  int nPitch, int nWidth, int nHeight) {
  for (int y = 0; y < nHeight; y++) {
    for (int x = 0; x < nWidth * (int)sizeof(pixel_t); x += 16) {
      __m128i m0 = _mm_loadu_si128((const __m128i *)&pSrc1_8[x]);
      __m128i m1 = _mm_loadu_si128((const __m128i *)&pSrc2_8[x]);

      if(sizeof(pixel_t) == 1)
        m0 = _mm_avg_epu8(m0, m1);
      else // uint16_t
        m0 = _mm_avg_epu16(m0, m1);
      _mm_storeu_si128((__m128i *)&pDst8[x], m0);
    }

    pSrc1_8 += nPitch;
    pSrc2_8 += nPitch;
    pDst8 += nPitch;
  }
}

// 8-32 bits
template<typename pixel_t>
void Average2(unsigned char *pDst8, const unsigned char *pSrc1_8, const unsigned char *pSrc2_8,
  int nPitch, int nWidth, int nHeight)
{ // assume all pitches equal

  pixel_t *pDst = reinterpret_cast<pixel_t *>(pDst8);
  const pixel_t *pSrc1 = reinterpret_cast<const pixel_t *>(pSrc1_8);
  const pixel_t *pSrc2 = reinterpret_cast<const pixel_t *>(pSrc2_8);

  nPitch /= sizeof(pixel_t);

  for (int j = 0; j < nHeight; j++)
  {
    for (int i = 0; i < nWidth; i++)
      if constexpr(sizeof(pixel_t) <= 2)
        pDst[i] = (pSrc1[i] + pSrc2[i] + 1) >> 1;
      else
        pDst[i] = (pSrc1[i] + pSrc2[i]) * 0.5f;

    pDst += nPitch;
    pSrc1 += nPitch;
    pSrc2 += nPitch;
  }
}


// instantiate templates defined in cpp
template void VerticalBilin<uint8_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeigh, int bits_per_pixelt);
template void VerticalBilin<uint16_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template void VerticalBilin<float>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);

template void VerticalBilin_sse2<uint8_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeigh, int bits_per_pixelt);
template void VerticalBilin_sse2<uint16_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);

template void HorizontalBilin<uint8_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template void HorizontalBilin<uint16_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template void HorizontalBilin<float>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);

template void HorizontalBilin_sse2<uint8_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template void HorizontalBilin_sse2<uint16_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);

template void DiagonalBilin<uint8_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template void DiagonalBilin<uint16_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template void DiagonalBilin<float>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);

template void DiagonalBilin_sse2<uint8_t, 0>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template void DiagonalBilin_sse2<uint16_t, 0>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template void DiagonalBilin_sse2<uint16_t, 1>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);

template void RB2F<uint8_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int y_beg, int y_end, int cpuFlags);
template void RB2F<uint16_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int y_beg, int y_end, int cpuFlags);
template void RB2F<float>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int y_beg, int y_end, int cpuFlags);

template void RB2Filtered<uint8_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int y_beg, int y_end, int cpuFlags);
template void RB2Filtered<uint16_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int y_beg, int y_end, int cpuFlags);
template void RB2Filtered<float>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int y_beg, int y_end, int cpuFlags);

template void RB2BilinearFiltered<uint8_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int y_beg, int y_end, int cpuFlags);
template void RB2BilinearFiltered<uint16_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int y_beg, int y_end, int cpuFlags);
template void RB2BilinearFiltered<float>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int y_beg, int y_end, int cpuFlags);

template void RB2Quadratic<uint8_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int y_beg, int y_end, int cpuFlags);
template void RB2Quadratic<uint16_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int y_beg, int y_end, int cpuFlags);
template void RB2Quadratic<float>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int y_beg, int y_end, int cpuFlags);

template void RB2Cubic<uint8_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int y_beg, int y_end, int cpuFlags);
template void RB2Cubic<uint16_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int y_beg, int y_end, int cpuFlags);
template void RB2Cubic<float>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int y_beg, int y_end, int cpuFlags);

template void VerticalWiener<uint8_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template void VerticalWiener<uint16_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template void VerticalWiener<float>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);

template void VerticalWiener_sse2<uint8_t, false>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template void VerticalWiener_sse2<uint16_t, false>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template void VerticalWiener_sse2<uint16_t, true>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);

template void HorizontalWiener<uint8_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template void HorizontalWiener<uint16_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template void HorizontalWiener<float>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);

template void HorizontalWiener_sse2<uint8_t, false>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template void HorizontalWiener_sse2<uint16_t, false>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template void HorizontalWiener_sse2<uint16_t, true>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);

#if 0 // not used
template void DiagonalWiener<uint8_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template void DiagonalWiener<uint16_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
#endif

template void VerticalBicubic<uint8_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template void VerticalBicubic<uint16_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template void VerticalBicubic<float>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);

template void VerticalBicubic_sse2<uint8_t, false>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template void VerticalBicubic_sse2<uint8_t, true>(unsigned char* pDst, const unsigned char* pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template void VerticalBicubic_sse2<uint16_t, false>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template void VerticalBicubic_sse2<uint16_t, true>(unsigned char* pDst, const unsigned char* pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);

template void HorizontalBicubic<uint8_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template void HorizontalBicubic<uint16_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template void HorizontalBicubic<float>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);

template void HorizontalBicubic_sse2<uint8_t, false>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template void HorizontalBicubic_sse2<uint16_t, false>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template void HorizontalBicubic_sse2<uint16_t, true>(unsigned char* pDst, const unsigned char* pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);

#if 0 // not used
template void DiagonalBicubic<uint8_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template void DiagonalBicubic<uint16_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template void DiagonalBicubic<float>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
#endif

template void Average2<uint8_t>(unsigned char *pDst, const unsigned char *pSrc1, const unsigned char *pSrc2, int nPitch, int nWidth, int nHeight);
template void Average2<uint16_t>(unsigned char *pDst, const unsigned char *pSrc1, const unsigned char *pSrc2, int nPitch, int nWidth, int nHeight);
template void Average2<float>(unsigned char *pDst, const unsigned char *pSrc1, const unsigned char *pSrc2, int nPitch, int nWidth, int nHeight);

template void Average2_sse2<uint8_t>(unsigned char *pDst, const unsigned char *pSrc1, const unsigned char *pSrc2, int nPitch, int nWidth, int nHeight);
template void Average2_sse2<uint16_t>(unsigned char *pDst, const unsigned char *pSrc1, const unsigned char *pSrc2, int nPitch, int nWidth, int nHeight);

