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

#ifndef __VARIANCE_H__
#define __VARIANCE_H__

#include <stdint.h>
#include <CopyCode.h> // arch_t

inline unsigned int VARABS(int x) { return x < 0 ? -x : x; }

typedef unsigned int (VARFunction)(const unsigned char *pSrc, int nSrcPitch, int *pLuma);
// PF: VAR is not used
template<int nBlkWidth, int nBlkHeight, typename pixel_t>
unsigned int Var_C(const unsigned char *pSrc, int nSrcPitch, int *pLuma)
{
   const unsigned char *s = pSrc;
   int meanLuma = 0;
   int meanVariance = 0;
   for ( int j = 0; j < nBlkHeight; j++ )
   {
      for ( int i = 0; i < nBlkWidth; i++ )
         meanLuma += reinterpret_cast<const pixel_t *>(s)[i];
      s += nSrcPitch;
   }
   *pLuma = meanLuma;
   meanLuma = (meanLuma + ((nBlkWidth * nBlkHeight) >> 1)) / (nBlkWidth * nBlkHeight);
   s = pSrc;
   for ( int j = 0; j < nBlkHeight; j++ )
   {
      for ( int i = 0; i < nBlkWidth; i++ )
         meanVariance += VARABS(reinterpret_cast<const pixel_t *>(s)[i] - meanLuma);
      s += nSrcPitch;
   }
   return meanVariance;
}

template<int nBlkSize, typename pixel_t>
unsigned int Var_C(const unsigned char *pSrc, int nSrcPitch, int *pLuma)
{
   return Var_C<nBlkSize, nBlkSize, pixel_t>(pSrc, nSrcPitch, pLuma);
}

extern "C" unsigned int __cdecl Var32x32_sse2(const unsigned char *pSrc, int nSrcPitch, int *pLuma);
extern "C" unsigned int __cdecl Var16x32_sse2(const unsigned char *pSrc, int nSrcPitch, int *pLuma);
extern "C" unsigned int __cdecl Var32x16_sse2(const unsigned char *pSrc, int nSrcPitch, int *pLuma);
extern "C" unsigned int __cdecl Var16x16_sse2(const unsigned char *pSrc, int nSrcPitch, int *pLuma);
extern "C" unsigned int __cdecl Var8x8_sse2(const unsigned char *pSrc, int nSrcPitch, int *pLuma);
extern "C" unsigned int __cdecl Var4x4_sse2(const unsigned char *pSrc, int nSrcPitch, int *pLuma);
extern "C" unsigned int __cdecl Var8x4_sse2(const unsigned char *pSrc, int nSrcPitch, int *pLuma);
extern "C" unsigned int __cdecl Var16x8_sse2(const unsigned char *pSrc, int nSrcPitch, int *pLuma);
extern "C" unsigned int __cdecl Var16x2_sse2(const unsigned char *pSrc, int nSrcPitch, int *pLuma);

// todo sad_t (float) and here sad_t is unsigned int, and at all other places in code: int
//using LUMAFunction = sad_t (*)(const unsigned char *pSrc, int nSrcPitch);
typedef unsigned int (LUMAFunction)(const unsigned char *pSrc, int nSrcPitch);
LUMAFunction* get_luma_function(int BlockX, int BlockY, int pixelsize, arch_t arch);

// todo sad_t float
template<int nBlkWidth, int nBlkHeight, typename pixel_t>
unsigned int Luma_C(const unsigned char *pSrc, int nSrcPitch);

// todo sad_t float
template<int nBlkSize, typename pixel_t>
unsigned int Luma_C(const unsigned char *pSrc, int nSrcPitch);

extern "C" unsigned int __cdecl Luma32x32_sse2(const unsigned char *pSrc, int nSrcPitch);
extern "C" unsigned int __cdecl Luma16x32_sse2(const unsigned char *pSrc, int nSrcPitch);
extern "C" unsigned int __cdecl Luma32x16_sse2(const unsigned char *pSrc, int nSrcPitch);
extern "C" unsigned int __cdecl Luma16x16_sse2(const unsigned char *pSrc, int nSrcPitch);
extern "C" unsigned int __cdecl Luma8x8_sse2(const unsigned char *pSrc, int nSrcPitch);
extern "C" unsigned int __cdecl Luma4x4_sse2(const unsigned char *pSrc, int nSrcPitch);
extern "C" unsigned int __cdecl Luma8x4_sse2(const unsigned char *pSrc, int nSrcPitch);
extern "C" unsigned int __cdecl Luma16x8_sse2(const unsigned char *pSrc, int nSrcPitch);
extern "C" unsigned int __cdecl Luma16x2_sse2(const unsigned char *pSrc, int nSrcPitch);

#endif
