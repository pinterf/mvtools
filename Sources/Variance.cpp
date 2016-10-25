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

inline unsigned int ABS(const int x)
{
	return ( x < 0 ) ? -x : x;
}

template<int nBlkWidth, int nBlkHeight, typename pixel_t>
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
  // down to  8x2 uint8_t
  //      or  4x2 uint16_t
  __m128i zero = _mm_setzero_si128();
  __m128i sum = _mm_setzero_si128(); // 2x or 4x int is probably enough for 32x32
  const bool two_rows = (sizeof(pixel_t) == 2 && nBlkWidth <= 4) || (sizeof(pixel_t) == 1 && nBlkWidth <= 8);

  for ( int y = 0; y < nBlkHeight; y+= (two_rows ? 2 : 1))
  {
    for ( int x = 0; x < nBlkWidth; x+=16 )
    {
      __m128i src1;
      if (two_rows) {
        // (8 bytes or 4 words) * 2 rows
        src1 = _mm_or_si128(_mm_loadl_epi64((__m128i *) (pSrc + x)),_mm_slli_si128(_mm_loadl_epi64((__m128i *) (pSrc + x + nSrcPitch)),8));
      } else {
        src1 = _mm_loadu_si128((__m128i *) (pSrc + x));
      }
      if(sizeof(pixel_t) == 1) {
        sum = _mm_add_epi32(sum, _mm_sad_epu8(src1, zero)); 
        // sum1_32, 0, sum2_32, 0
        // result in two 32 bit areas at the upper and lower 64 bytes
      }
      else {
        sum = _mm_add_epi32(sum, _mm_unpacklo_epi16(src1, zero));
        sum = _mm_add_epi32(sum, _mm_unpackhi_epi16(src1, zero));
        // result in four 32 bit sum1_32, sum2_32, sum3_32, sum4_32
      }
    }
    if (two_rows) {
      pSrc += nSrcPitch*2;
    } else {
      pSrc += nSrcPitch;
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

    /* SSSE3:
    sum = _mm_hadd_epi32(sum, zero);  // A1+A2, B1+B2, 0+0, 0+0
    sum = _mm_hadd_epi32(sum, zero);  // A1+A2+B1+B2, 0+0+0+0, 0+0+0+0, 0+0+0+0
    */
  }
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

    func_luma[make_tuple(32, 32, 1, NO_SIMD)] = Luma_C<32, 32,uint8_t>;
    func_luma[make_tuple(32, 16, 1, NO_SIMD)] = Luma_C<32, 16,uint8_t>;
    func_luma[make_tuple(32, 8 , 1, NO_SIMD)] = Luma_C<32, 8,uint8_t>;
    func_luma[make_tuple(16, 32, 1, NO_SIMD)] = Luma_C<16, 32,uint8_t>;
    func_luma[make_tuple(16, 16, 1, NO_SIMD)] = Luma_C<16, 16,uint8_t>;
    func_luma[make_tuple(16, 8 , 1, NO_SIMD)] = Luma_C<16, 8,uint8_t>;
    func_luma[make_tuple(16, 4 , 1, NO_SIMD)] = Luma_C<16, 4,uint8_t>;
    func_luma[make_tuple(16, 2 , 1, NO_SIMD)] = Luma_C<16, 2,uint8_t>;
    func_luma[make_tuple(16, 1 , 1, NO_SIMD)] = Luma_C<16, 1,uint8_t>;
    func_luma[make_tuple(8 , 16, 1, NO_SIMD)] = Luma_C<8 , 16,uint8_t>;
    func_luma[make_tuple(8 , 8 , 1, NO_SIMD)] = Luma_C<8 , 8,uint8_t>;
    func_luma[make_tuple(8 , 4 , 1, NO_SIMD)] = Luma_C<8 , 4,uint8_t>;
    func_luma[make_tuple(8 , 2 , 1, NO_SIMD)] = Luma_C<8 , 2,uint8_t>;
    func_luma[make_tuple(8 , 1 , 1, NO_SIMD)] = Luma_C<8 , 1,uint8_t>;
    func_luma[make_tuple(4 , 8 , 1, NO_SIMD)] = Luma_C<4 , 8,uint8_t>;
    func_luma[make_tuple(4 , 4 , 1, NO_SIMD)] = Luma_C<4 , 4,uint8_t>;
    func_luma[make_tuple(4 , 2 , 1, NO_SIMD)] = Luma_C<4 , 2,uint8_t>;
    func_luma[make_tuple(4 , 1 , 1, NO_SIMD)] = Luma_C<4 , 1,uint8_t>;
    func_luma[make_tuple(2 , 4 , 1, NO_SIMD)] = Luma_C<2 , 4,uint8_t>;
    func_luma[make_tuple(2 , 2 , 1, NO_SIMD)] = Luma_C<2 , 2,uint8_t>;
    func_luma[make_tuple(2 , 1 , 1, NO_SIMD)] = Luma_C<2 , 1,uint8_t>;

    func_luma[make_tuple(32, 32, 2, NO_SIMD)] = Luma_C<32, 32,uint16_t>;
    func_luma[make_tuple(32, 16, 2, NO_SIMD)] = Luma_C<32, 16,uint16_t>;
    func_luma[make_tuple(32, 8 , 2, NO_SIMD)] = Luma_C<32, 8,uint16_t>;
    func_luma[make_tuple(16, 32, 2, NO_SIMD)] = Luma_C<16, 32,uint16_t>;
    func_luma[make_tuple(16, 16, 2, NO_SIMD)] = Luma_C<16, 16,uint16_t>;
    func_luma[make_tuple(16, 8 , 2, NO_SIMD)] = Luma_C<16, 8,uint16_t>;
    func_luma[make_tuple(16, 4 , 2, NO_SIMD)] = Luma_C<16, 4,uint16_t>;
    func_luma[make_tuple(16, 2 , 2, NO_SIMD)] = Luma_C<16, 2,uint16_t>;
    func_luma[make_tuple(16, 1 , 2, NO_SIMD)] = Luma_C<16, 1,uint16_t>;
    func_luma[make_tuple(8 , 16, 2, NO_SIMD)] = Luma_C<8 , 16,uint16_t>;
    func_luma[make_tuple(8 , 8 , 2, NO_SIMD)] = Luma_C<8 , 8,uint16_t>;
    func_luma[make_tuple(8 , 4 , 2, NO_SIMD)] = Luma_C<8 , 4,uint16_t>;
    func_luma[make_tuple(8 , 2 , 2, NO_SIMD)] = Luma_C<8 , 2,uint16_t>;
    func_luma[make_tuple(8 , 1 , 2, NO_SIMD)] = Luma_C<8 , 1,uint16_t>;
    func_luma[make_tuple(4 , 8 , 2, NO_SIMD)] = Luma_C<4 , 8,uint16_t>;
    func_luma[make_tuple(4 , 4 , 2, NO_SIMD)] = Luma_C<4 , 4,uint16_t>;
    func_luma[make_tuple(4 , 2 , 2, NO_SIMD)] = Luma_C<4 , 2,uint16_t>;
    func_luma[make_tuple(4 , 1 , 2, NO_SIMD)] = Luma_C<4 , 1,uint16_t>;
    func_luma[make_tuple(2 , 4 , 2, NO_SIMD)] = Luma_C<2 , 4,uint16_t>;
    func_luma[make_tuple(2 , 2 , 2, NO_SIMD)] = Luma_C<2 , 2,uint16_t>;
    func_luma[make_tuple(2 , 1 , 2, NO_SIMD)] = Luma_C<2 , 1,uint16_t>;

    func_luma[make_tuple(32, 32, 1, USE_SSE2)] = Luma32x32_sse2;
    func_luma[make_tuple(32, 16, 1, USE_SSE2)] = Luma32x16_sse2;
    //func_luma[make_tuple(32, 8 , 1, USE_SSE2)] = Luma32x8_sse2;
    func_luma[make_tuple(16, 32, 1, USE_SSE2)] = Luma16x32_sse2;
    func_luma[make_tuple(16, 16, 1, USE_SSE2)] = Luma16x16_sse2;
    func_luma[make_tuple(16, 8 , 1, USE_SSE2)] = Luma16x8_sse2;
    //func_luma[make_tuple(16, 4 , 1, USE_SSE2)] = Luma16x4_sse2;
    func_luma[make_tuple(16, 2 , 1, USE_SSE2)] = Luma16x2_sse2;
    //func_luma[make_tuple(8 , 16, 1, USE_SSE2)] = Luma8x16_sse2;
    func_luma[make_tuple(8 , 8 , 1, USE_SSE2)] = Luma8x8_sse2;
    func_luma[make_tuple(8 , 4 , 1, USE_SSE2)] = Luma8x4_sse2;
    //func_luma[make_tuple(8 , 2 , 1, USE_SSE2)] = Luma8x2_sse2;
    //func_luma[make_tuple(8 , 1 , 1, USE_SSE2)] = Luma8x1_sse2;
    //func_luma[make_tuple(4 , 8 , 1, USE_SSE2)] = Luma4x8_sse2;
    func_luma[make_tuple(4 , 4 , 1, USE_SSE2)] = Luma4x4_sse2;
    //func_luma[make_tuple(4 , 2 , 1, USE_SSE2)] = Luma4x2_sse2;
    //func_luma[make_tuple(2 , 4 , 1, USE_SSE2)] = Luma2x4_sse2;
    //func_luma[make_tuple(2 , 2 , 1, USE_SSE2)] = Luma2x2_sse2;
    
    func_luma[make_tuple(32, 32, 2, USE_SSE2)] = Luma16_sse2<32, 32,uint16_t>;
    func_luma[make_tuple(32, 16, 2, USE_SSE2)] = Luma16_sse2<32, 16,uint16_t>;
    func_luma[make_tuple(32, 8 , 2, USE_SSE2)] = Luma16_sse2<32, 8,uint16_t>;
    func_luma[make_tuple(16, 32, 2, USE_SSE2)] = Luma16_sse2<16, 32,uint16_t>;
    func_luma[make_tuple(16, 16, 2, USE_SSE2)] = Luma16_sse2<16, 16,uint16_t>;
    func_luma[make_tuple(16, 8 , 2, USE_SSE2)] = Luma16_sse2<16, 8,uint16_t>;
    func_luma[make_tuple(16, 4 , 2, USE_SSE2)] = Luma16_sse2<16, 4,uint16_t>;
    func_luma[make_tuple(16, 2 , 2, USE_SSE2)] = Luma16_sse2<16, 2,uint16_t>;
    func_luma[make_tuple(16, 1 , 2, USE_SSE2)] = Luma16_sse2<16, 1,uint16_t>;
    func_luma[make_tuple(8 , 16, 2, USE_SSE2)] = Luma16_sse2<8 , 16,uint16_t>;
    func_luma[make_tuple(8 , 8 , 2, USE_SSE2)] = Luma16_sse2<8 , 8,uint16_t>;
    func_luma[make_tuple(8 , 4 , 2, USE_SSE2)] = Luma16_sse2<8 , 4,uint16_t>;
    func_luma[make_tuple(8 , 2 , 2, USE_SSE2)] = Luma16_sse2<8 , 2,uint16_t>;
    func_luma[make_tuple(8 , 1 , 2, USE_SSE2)] = Luma16_sse2<8 , 1,uint16_t>;
    func_luma[make_tuple(4 , 8 , 2, USE_SSE2)] = Luma16_sse2<4 , 8,uint16_t>;
    func_luma[make_tuple(4 , 4 , 2, USE_SSE2)] = Luma16_sse2<4 , 4,uint16_t>;
    func_luma[make_tuple(4 , 2 , 2, USE_SSE2)] = Luma16_sse2<4 , 2,uint16_t>;
    // no 4,1 or 2,x,x for uint16_t
    
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

#if 0
    LUMAFunction *result = func_luma[make_tuple(BlockX, BlockY, pixelsize, arch)];

    arch_t arch_orig = arch;

    // no AVX2 -> try AVX
    if (result == nullptr && (arch==USE_AVX2 || arch_orig==USE_AVX)) {
      arch = USE_AVX;
      result = func_luma[make_tuple(BlockX, BlockY, pixelsize, USE_AVX)];
    }
    // no AVX -> try SSE2
    if (result == nullptr && (arch==USE_AVX || arch_orig==USE_SSE2)) {
      arch = USE_SSE2;
      result = func_luma[make_tuple(BlockX, BlockY, pixelsize, USE_SSE2)];
    }
    // no SSE2 -> try C
    if (result == nullptr && (arch==USE_SSE2 || arch_orig==NO_SIMD)) {
      arch = NO_SIMD;
      /* C version variations are only working in SAD
      // priority: C version compiled to avx2, avx
      if(arch_orig==USE_AVX2)
      result = get_luma_avx2_C_function(BlockX, BlockY, pixelsize, NO_SIMD);
      else if(arch_orig==USE_AVX)
      result = get_luma_avx_C_function(BlockX, BlockY, pixelsize, NO_SIMD);
      */
      if(result == nullptr)
        result = func_luma[make_tuple(BlockX, BlockY, pixelsize, NO_SIMD)]; // fallback to C
    }
#endif
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