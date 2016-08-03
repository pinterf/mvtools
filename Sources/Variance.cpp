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
unsigned int Luma_C(const unsigned char *pSrc, int nSrcPitch)
{
    const unsigned char *s = pSrc;
    int meanLuma = 0;
    for ( int j = 0; j < nBlkHeight; j++ )
    {
        for ( int i = 0; i < nBlkWidth; i++ )
            meanLuma += reinterpret_cast<const pixel_t *>(s)[i];
        s += nSrcPitch;
    }
    return meanLuma;
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

    LUMAFunction *result = func_luma[make_tuple(BlockX, BlockY, pixelsize, arch)];
    if (result == nullptr)
        result = func_luma[make_tuple(BlockX, BlockY, pixelsize, NO_SIMD)]; // fallback to C
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