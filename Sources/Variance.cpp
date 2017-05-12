// Author: Manao

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

#include "Variance.h"
#include <stdint.h>
#include "CopyCode.h" // arch_t
#include <tuple>
#include <map>
#include "def.h"
#include <cassert>

MV_FORCEINLINE unsigned int ABS(const int x)
{
	return ( x < 0 ) ? -x : x;
}

template<int nBlkWidth, int nBlkHeight>
unsigned int Luma8_sse2(const unsigned char *pSrc, int nSrcPitch)
{
  /*
  const unsigned char *s = pSrc;
  int sumLuma = 0;
  for ( int j = 0; j < nBlkHeight; j++ )
  {
  for ( int i = 0; i < nBlkWidth; i++ )
  sumLuma += reinterpret_cast<const pixel_t *>(s)[i];
  s += nSrcPitch;
  }
  return sumLuma;
  */
  // down to  8x2 uint8_t
  //      or  4x2 uint16_t
  __m128i zero = _mm_setzero_si128();
  __m128i sum = _mm_setzero_si128(); // 2x or 4x int is probably enough for 32x32
  const int simd_step = (nBlkWidth % 16) == 0 ? 16 : 8;
  const bool two_rows = simd_step == 8;

  for (int y = 0; y < nBlkHeight; y += (two_rows ? 2 : 1))
  {
    if (nBlkWidth % 16 == 0) {
      for (int x = 0; x < nBlkWidth; x += 16)
      {
        __m128i src1 = _mm_loadu_si128((__m128i *) (pSrc + x));
        sum = _mm_add_epi32(sum, _mm_sad_epu8(src1, zero));
        // sum1_32, 0, sum2_32, 0
          // result in two 32 bit areas at the upper and lower 64 bytes
      }
      pSrc += nSrcPitch;
    }
    else if (nBlkWidth % 8 == 0) {
      for (int x = 0; x < nBlkWidth; x += 8)
      {
        __m128i src1 = _mm_or_si128(_mm_loadl_epi64((__m128i *) (pSrc + x)), _mm_slli_si128(_mm_loadl_epi64((__m128i *) (pSrc + x + nSrcPitch)), 8));
        sum = _mm_add_epi32(sum, _mm_sad_epu8(src1, zero));
        // sum1_32, 0, sum2_32, 0
        // result in two 32 bit areas at the upper and lower 64 bytes
      }
      pSrc += nSrcPitch * 2;
    }
    else {
      assert(0);
    }
  }
  // sum here: two 32 bit partial result: sum1 0 sum2 0
  __m128i sum_hi = _mm_unpackhi_epi64(sum, zero); // a1 + a3. 2 dwords right 
  sum = _mm_add_epi32(sum, sum_hi);  // a0 + a2 + a1 + a3
  unsigned int result = _mm_cvtsi128_si32(sum);

  return result;
}

template<int nBlkWidth, int nBlkHeight>
unsigned int Luma16_sse2(const unsigned char *pSrc, int nSrcPitch)
{
  /*
  const unsigned char *s = pSrc;
  int sumLuma = 0;
  for ( int j = 0; j < nBlkHeight; j++ )
  {
    for ( int i = 0; i < nBlkWidth; i++ )
      sumLuma += reinterpret_cast<const pixel_t *>(s)[i];
    s += nSrcPitch;
  }
  return sumLuma;
  */
  __m128i zero = _mm_setzero_si128();
  __m128i sum = _mm_setzero_si128(); // 2x or 4x int is probably enough for 32x32
  const int simd_step = (nBlkWidth % 8) == 0 ? 16 : 8;
  const bool two_rows = simd_step == 8;
  // blksize 4, 8, 12, 16,...
  for ( int y = 0; y < nBlkHeight; y+= (two_rows ? 2 : 1))
  {
    for ( int x = 0; x < nBlkWidth * sizeof(uint16_t); x+=simd_step )
    {
      __m128i src1;
      if (two_rows) {
        // (8 bytes or 4 words) * 2 rows
        src1 = _mm_or_si128(_mm_loadl_epi64((__m128i *) (pSrc + x)),_mm_slli_si128(_mm_loadl_epi64((__m128i *) (pSrc + x + nSrcPitch)),8));
      } else {
        src1 = _mm_loadu_si128((__m128i *) (pSrc + x));
      }
      sum = _mm_add_epi32(sum, _mm_unpacklo_epi16(src1, zero));
      sum = _mm_add_epi32(sum, _mm_unpackhi_epi16(src1, zero));
      // result in four 32 bit sum1_32, sum2_32, sum3_32, sum4_32
    }
    if (two_rows) {
      pSrc += nSrcPitch*2;
    } else {
      pSrc += nSrcPitch;
    }
  }
    // at 16 bits: we have 4 integers for sum: a0 a1 a2 a3
  __m128i a0_a1 = _mm_unpacklo_epi32(sum, zero); // a0 0 a1 0
  __m128i a2_a3 = _mm_unpackhi_epi32(sum, zero); // a2 0 a3 0
  sum = _mm_add_epi32( a0_a1, a2_a3 ); // a0+a2, 0, a1+a3, 0
  // sum here: two 32 bit partial result: sum1 0 sum2 0
  __m128i sum_hi = _mm_unpackhi_epi64(sum, zero); // a1 + a3. 2 dwords right 
  sum = _mm_add_epi32(sum, sum_hi);  // a0 + a2 + a1 + a3
  unsigned int result = _mm_cvtsi128_si32(sum);

  return result;
}

template<int nBlkWidth, int nBlkHeight, typename pixel_t>
unsigned int Luma_C(const unsigned char *pSrc, int nSrcPitch)
{
    const unsigned char *s = pSrc;
    int sumLuma = 0;
    for ( int j = 0; j < nBlkHeight; j++ )
    {
        for ( int i = 0; i < nBlkWidth; i++ )
            sumLuma += reinterpret_cast<const pixel_t *>(s)[i];
        s += nSrcPitch;
    }
    return sumLuma;
}

template<int nBlkSize, typename pixel_t>
unsigned int Luma_C(const unsigned char *pSrc, int nSrcPitch)
{
    return Luma_C<nBlkSize, nBlkSize, pixel_t>(pSrc, nSrcPitch);
}


LUMAFunction* get_luma_function(int BlockX, int BlockY, int pixelsize, arch_t arch)
{
    // 8 bit only (pixelsize==1)
    //---------- LUMA
    // BlkSizeX, BlkSizeY, pixelsize, arch_t
    std::map<std::tuple<int, int, int, arch_t>, LUMAFunction*> func_luma;
    using std::make_tuple;

#define MAKE_LUMA_FN(x, y) func_luma[make_tuple(x, y, 1, NO_SIMD)] = Luma_C<x, y, uint8_t>; \
func_luma[make_tuple(x, y, 2, NO_SIMD)] = Luma_C<x, y, uint16_t>;
    MAKE_LUMA_FN(64, 64)
      MAKE_LUMA_FN(64, 48)
      MAKE_LUMA_FN(64, 32)
      MAKE_LUMA_FN(64, 16)
      MAKE_LUMA_FN(48, 64)
      MAKE_LUMA_FN(48, 48)
      MAKE_LUMA_FN(48, 24)
      MAKE_LUMA_FN(48, 12)
      MAKE_LUMA_FN(32, 64)
      MAKE_LUMA_FN(32, 32)
      MAKE_LUMA_FN(32, 24)
      MAKE_LUMA_FN(32, 16)
      MAKE_LUMA_FN(32, 8)
      MAKE_LUMA_FN(24, 48)
      MAKE_LUMA_FN(24, 32)
      MAKE_LUMA_FN(24, 24)
      MAKE_LUMA_FN(24, 12)
      MAKE_LUMA_FN(24, 6)
      MAKE_LUMA_FN(16, 64)
      MAKE_LUMA_FN(16, 32)
      MAKE_LUMA_FN(16, 16)
      MAKE_LUMA_FN(16, 12)
      MAKE_LUMA_FN(16, 8)
      MAKE_LUMA_FN(16, 4)
      MAKE_LUMA_FN(16, 2)
      MAKE_LUMA_FN(16, 1)
      MAKE_LUMA_FN(12, 48)
      MAKE_LUMA_FN(12, 24)
      MAKE_LUMA_FN(12, 16)
      MAKE_LUMA_FN(12, 12)
      MAKE_LUMA_FN(12, 6)
      MAKE_LUMA_FN(8, 32)
      MAKE_LUMA_FN(8, 16)
      MAKE_LUMA_FN(8, 8)
      MAKE_LUMA_FN(8, 4)
      MAKE_LUMA_FN(8, 2)
      MAKE_LUMA_FN(8, 1)
      MAKE_LUMA_FN(6, 12)
      MAKE_LUMA_FN(6, 6)
      MAKE_LUMA_FN(6, 3)
      MAKE_LUMA_FN(4, 8)
      MAKE_LUMA_FN(4, 4)
      MAKE_LUMA_FN(4, 2)
      MAKE_LUMA_FN(4, 1)
      MAKE_LUMA_FN(3, 6)
      MAKE_LUMA_FN(3, 3)
      MAKE_LUMA_FN(2, 4)
      MAKE_LUMA_FN(2, 2)
      MAKE_LUMA_FN(2, 1)
#undef MAKE_LUMA_FN

    func_luma[make_tuple(64, 64, 1, USE_SSE2)] = Luma8_sse2<64, 64>;
    func_luma[make_tuple(64, 48, 1, USE_SSE2)] = Luma8_sse2<64, 48>;
    func_luma[make_tuple(64, 32, 1, USE_SSE2)] = Luma8_sse2<64, 32>;
    func_luma[make_tuple(64, 16, 1, USE_SSE2)] = Luma8_sse2<64, 16>;
    func_luma[make_tuple(48, 64, 1, USE_SSE2)] = Luma8_sse2<48, 64>;
    func_luma[make_tuple(48, 48, 1, USE_SSE2)] = Luma8_sse2<48, 48>;
    func_luma[make_tuple(48, 48, 1, USE_SSE2)] = Luma8_sse2<48, 24>;
    func_luma[make_tuple(48, 48, 1, USE_SSE2)] = Luma8_sse2<48, 12>;
    func_luma[make_tuple(32, 64, 1, USE_SSE2)] = Luma8_sse2<32, 64>;
    func_luma[make_tuple(32, 32, 1, USE_SSE2)] = Luma32x32_sse2;
    func_luma[make_tuple(32, 24, 1, USE_SSE2)] = Luma8_sse2<32, 24>;
    func_luma[make_tuple(32, 16, 1, USE_SSE2)] = Luma32x16_sse2;
    func_luma[make_tuple(32, 8, 1, USE_SSE2)] = Luma8_sse2<32, 8>;
    func_luma[make_tuple(24, 48, 1, USE_SSE2)] = Luma8_sse2<24, 48>;
    func_luma[make_tuple(24, 32, 1, USE_SSE2)] = Luma8_sse2<24, 32>;
    func_luma[make_tuple(24, 24, 1, USE_SSE2)] = Luma8_sse2<24, 24>;
    func_luma[make_tuple(24, 12, 1, USE_SSE2)] = Luma8_sse2<24, 12>;
    func_luma[make_tuple(24, 6, 1, USE_SSE2)] = Luma8_sse2<24, 6>;
    func_luma[make_tuple(16, 64, 1, USE_SSE2)] = Luma8_sse2<16, 64>;
    func_luma[make_tuple(16, 32, 1, USE_SSE2)] = Luma16x32_sse2;
    func_luma[make_tuple(16, 16, 1, USE_SSE2)] = Luma16x16_sse2;
    func_luma[make_tuple(16, 12, 1, USE_SSE2)] = Luma8_sse2<16, 12>;
    func_luma[make_tuple(16, 8 , 1, USE_SSE2)] = Luma16x8_sse2;
    func_luma[make_tuple(16, 4, 1, USE_SSE2)] = Luma8_sse2<16, 4>;
    func_luma[make_tuple(16, 2 , 1, USE_SSE2)] = Luma16x2_sse2;
    func_luma[make_tuple(16, 1, 1, USE_SSE2)] = Luma8_sse2<16, 1>;
    func_luma[make_tuple(8, 32, 1, USE_SSE2)] = Luma8_sse2<8, 32>;
    func_luma[make_tuple(8, 16, 1, USE_SSE2)] = Luma8_sse2<8, 16>;
    func_luma[make_tuple(8 , 8 , 1, USE_SSE2)] = Luma8x8_sse2;
    func_luma[make_tuple(8 , 4 , 1, USE_SSE2)] = Luma8x4_sse2;
    func_luma[make_tuple(8, 2, 1, USE_SSE2)] = Luma8_sse2<8, 2>;
    func_luma[make_tuple(4 , 4 , 1, USE_SSE2)] = Luma4x4_sse2;

    // no 4,1 or 2,x,x for uint16_t
    // nor 12*
#define MAKE_LUMA_FN(x, y) func_luma[make_tuple(x, y, 2, USE_SSE2)] = Luma16_sse2<x, y>;
    MAKE_LUMA_FN(64, 64)
      MAKE_LUMA_FN(64, 48)
      MAKE_LUMA_FN(64, 32)
      MAKE_LUMA_FN(64, 16)
      MAKE_LUMA_FN(48, 64)
      MAKE_LUMA_FN(32, 64)
      MAKE_LUMA_FN(32, 32)
      MAKE_LUMA_FN(32, 24)
      MAKE_LUMA_FN(32, 16)
      MAKE_LUMA_FN(32, 8)
      MAKE_LUMA_FN(24, 48)
      MAKE_LUMA_FN(24, 32)
      MAKE_LUMA_FN(24, 24)
      MAKE_LUMA_FN(24, 12)
      MAKE_LUMA_FN(24, 6)
      MAKE_LUMA_FN(16, 64)
      MAKE_LUMA_FN(16, 32)
      MAKE_LUMA_FN(16, 16)
      MAKE_LUMA_FN(16, 12)
      MAKE_LUMA_FN(16, 8)
      MAKE_LUMA_FN(16, 4)
      MAKE_LUMA_FN(16, 2)
      MAKE_LUMA_FN(16, 1)
      MAKE_LUMA_FN(12, 48)
      MAKE_LUMA_FN(12, 24)
      MAKE_LUMA_FN(12, 16)
      MAKE_LUMA_FN(12, 12)
      MAKE_LUMA_FN(8, 32)
      MAKE_LUMA_FN(8, 16)
      MAKE_LUMA_FN(8, 8)
      MAKE_LUMA_FN(8, 4)
      MAKE_LUMA_FN(8, 2)
      MAKE_LUMA_FN(8, 1)
      MAKE_LUMA_FN(4, 8)
      MAKE_LUMA_FN(4, 4)
      MAKE_LUMA_FN(4, 2)
      //MAKE_LUMA_FN(4, 1)
      //MAKE_LUMA_FN(2, 4)
      //MAKE_LUMA_FN(2, 2)
      //MAKE_LUMA_FN(2, 1)
#undef MAKE_LUMA_FN

    LUMAFunction *result = nullptr;
    arch_t archlist[] = { USE_AVX2, USE_AVX, USE_SSE41, USE_SSE2, NO_SIMD };
    int index = 0;
    while (result == nullptr) {
      arch_t current_arch_try = archlist[index++];
      if (current_arch_try > arch) continue;
      result = func_luma[make_tuple(BlockX, BlockY, pixelsize, current_arch_try)];
      if (result == nullptr && current_arch_try == NO_SIMD)
        break;
    }

    return result;
}


//
//unsigned int Var16x16_C(const unsigned char *pSrc, int nSrcPitch, int *pLuma)
//{
//   const unsigned char *s = pSrc;
//   int meanLuma = 0;
//   int meanVariance = 0;
//   for ( int j = 0; j < 16; j++ )
//   {
//      for ( int i = 0; i < 16; i++ )
//         meanLuma += s[i];
//      s += nSrcPitch;
//   }
//   *pLuma = meanLuma;
//   meanLuma = (meanLuma + 32) / 64;
//   s = pSrc;
//   for ( int j = 0; j < 8; j++ )
//   {
//      for ( int i = 0; i < 8; i++ )
//         meanVariance += ABS(s[i] - meanLuma);
//      s += nSrcPitch;
//   }
//   return meanVariance;
//}
//
//
//unsigned int Var8x8_C(const unsigned char *pSrc, int nSrcPitch, int *pLuma)
//{
//   const unsigned char *s = pSrc;
//   int meanLuma = 0;
//   int meanVariance = 0;
//   for ( int j = 0; j < 8; j++ )
//   {
//      for ( int i = 0; i < 8; i++ )
//         meanLuma += s[i];
//      s += nSrcPitch;
//   }
//   *pLuma = meanLuma;
//   meanLuma = (meanLuma + 32) / 64;
//   s = pSrc;
//   for ( int j = 0; j < 8; j++ )
//   {
//      for ( int i = 0; i < 8; i++ )
//         meanVariance += ABS(s[i] - meanLuma);
//      s += nSrcPitch;
//   }
//   return meanVariance;
//}
//
//unsigned int Var4x4_C(const unsigned char *pSrc, int nSrcPitch, int *pLuma)
//{
//   const unsigned char *s = pSrc;
//   int meanLuma = 0;
//   int meanVariance = 0;
//
//   for ( int j = 0; j < 4; j++ )
//   {
//      for ( int i = 0; i < 4; i++ )
//         meanLuma += s[i];
//      s += nSrcPitch;
//   }
//
//   *pLuma = meanLuma;
//
//   meanLuma = (meanLuma + 8) / 16;
//
//   s = pSrc;
//
//   for ( int j = 0; j < 4; j++ )
//   {
//      for ( int i = 0; i < 4; i++ )
//         meanVariance += ABS(s[i] - meanLuma);
//      s += nSrcPitch;
//   }
//
//   return meanVariance;
//}


//unsigned int Luma8x8_C(const unsigned char *pSrc, int nSrcPitch)
//{
//   const unsigned char *s = pSrc;
//   int meanLuma = 0;
//   for ( int j = 0; j < 8; j++ )
//   {
//      for ( int i = 0; i < 8; i++ )
//         meanLuma += s[i];
//      s += nSrcPitch;
//   }
//   return meanLuma;
//}
//
//unsigned int Luma4x4_C(const unsigned char *pSrc, int nSrcPitch)
//{
//   const unsigned char *s = pSrc;
//   int meanLuma = 0;
//   for ( int j = 0; j < 4; j++ )
//   {
//      for ( int i = 0; i < 4; i++ )
//         meanLuma += s[i];
//      s += nSrcPitch;
//   }
//   return meanLuma;
//}