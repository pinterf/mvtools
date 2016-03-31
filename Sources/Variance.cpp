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

inline unsigned int ABS(const int x)
{
	return ( x < 0 ) ? -x : x;
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