// Functions that interpolates a frame

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

/*! \file Interpolation.h
 *  \brief Interpolating functions.
 *
 * This set of functions is used to create a picture 4 times bigger, by
 * interpolating the value of the half pixels. There are three different
 * interpolations : horizontal computes the pixels ( x + 0.5, y ), vertical
 * computes the pixels ( x, y + 0.5 ), and finally, diagonal computes the
 * pixels ( x + 0.5, y + 0.5 ).
 * For each type of interpolations, there are two functions, one in classical C,
 * and one optimized for iSSE processors.
 * 
 * sse2 simd intrinsics, 8 bit versions: dubhater - mvtools-vs
 * 32bit float C: pinterf
 * sse2/sse41 16 bit versions: pinterf
 */

#ifndef __INTERPOL__
#define __INTERPOL__

template<typename pixel_t>
void RB2_jump(int y_new, int &y, pixel_t * &pDst, const pixel_t * &pSrc, int nDstPitch, int nSrcPitch);

template<typename pixel_t>
void VerticalBilin(  unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template<typename pixel_t>
void VerticalBilin_sse2(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template<typename pixel_t>
void HorizontalBilin(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template<typename pixel_t>
void HorizontalBilin_sse2(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template<typename pixel_t>
void DiagonalBilin(  unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template<typename pixel_t, bool hasSSE41>
void DiagonalBilin_sse2(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);

template<typename pixel_t>
void RB2F(               unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int y_beg, int y_end, int cpuFlags);
template<typename pixel_t>
void RB2Filtered(        unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int y_beg, int y_end, int cpuFlags);
template<typename pixel_t>
void RB2BilinearFiltered(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int y_beg, int y_end, int cpuFlags);
template<typename pixel_t>
void RB2Quadratic(       unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int y_beg, int y_end, int cpuFlags);
template<typename pixel_t>
void RB2Cubic(           unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int y_beg, int y_end, int cpuFlags);

//commented out: sse2 8 bit conversion backported from mvtools-vs
//extern "C" void __cdecl VerticalBilin_iSSE(  unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
//extern "C" void __cdecl HorizontalBilin_iSSE(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
//extern "C" void __cdecl DiagonalBilin_iSSE(  unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);

/* dubhater's comment from mvtools-vs: TODO: port these
done [pinterf] extern "C" void  VerticalBicubic_iSSE(uint8_t *pDst, const uint8_t *pSrc, intptr_t nDstPitch, intptr_t nWidth, intptr_t nHeight);
done [pinterf] extern "C" void  HorizontalBicubic_iSSE(uint8_t *pDst, const uint8_t *pSrc, intptr_t nDstPitch, intptr_t nWidth, intptr_t nHeight);
extern "C" void  RB2F_iSSE(uint8_t *pDst, const uint8_t *pSrc, intptr_t nDstPitch, intptr_t nSrcPitch, intptr_t nWidth, intptr_t nHeight);
extern "C" void  RB2FilteredVerticalLine_SSE(uint8_t *pDst, const uint8_t *pSrc, intptr_t nSrcPitch, intptr_t nWidthMMX);
extern "C" void  RB2FilteredHorizontalInplaceLine_SSE(uint8_t *pSrc, intptr_t nWidthMMX);
*/

extern "C" void __cdecl RB2F_iSSE(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight);

//commented out: sse2 8 bit conversion backported from mvtools-vs
//extern "C" void __cdecl RB2CubicHorizontalInplaceLine_SSE(unsigned char *pSrc, int nWidthMMX);
//extern "C" void __cdecl RB2CubicVerticalLine_SSE(unsigned char *pDst, const unsigned char *pSrc, int nSrcPitch, int nWidthMMX);
//extern "C" void __cdecl RB2QuadraticHorizontalInplaceLine_SSE(unsigned char *pSrc, int nWidthMMX);
//extern "C" void __cdecl RB2QuadraticVerticalLine_SSE(unsigned char *pDst, const unsigned char *pSrc, int nSrcPitch, int nWidthMMX);
extern "C" void __cdecl RB2FilteredVerticalLine_SSE(unsigned char *pDst, const unsigned char *pSrc, int nSrcPitch, int nWidthMMX);
extern "C" void __cdecl RB2FilteredHorizontalInplaceLine_SSE(unsigned char *pSrc, int nWidthMMX);
//extern "C" void __cdecl RB2BilinearFilteredVerticalLine_SSE(unsigned char *pDst, const unsigned char *pSrc, int nSrcPitch, int nWidthMMX);
//extern "C" void __cdecl RB2BilinearFilteredHorizontalInplaceLine_SSE(unsigned char *pSrc, int nWidthMMX);

template<typename pixel_t>
void VerticalWiener(  unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template<typename pixel_t, bool hasSSE41>
void VerticalWiener_sse2(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template<typename pixel_t>
void HorizontalWiener(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template<typename pixel_t, bool hasSSE41>
void HorizontalWiener_sse2(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
#if 0 // not used
template<typename pixel_t>
void DiagonalWiener(  unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
#endif

//commented out: sse2 8 bit conversion backported from mvtools-vs
//extern "C" void __cdecl VerticalWiener_iSSE(  unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeigh, int bits_per_pixelt);
//extern "C" void __cdecl HorizontalWiener_iSSE(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);

template<typename pixel_t>
void VerticalBicubic(  unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template<typename pixel_t, bool hasSSE41>
void VerticalBicubic_sse2(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);

template<typename pixel_t>
void HorizontalBicubic(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template<typename pixel_t, bool hasSSE41>
void HorizontalBicubic_sse2(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);

#if 0 // not used
template<typename pixel_t>
void DiagonalBicubic(  unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
#endif

//commented out: SIMD intrinsics sse2 by pinterf
//extern "C" void __cdecl VerticalBicubic_iSSE(  unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
//extern "C" void __cdecl HorizontalBicubic_iSSE(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);

template<typename pixel_t>
void Average2(     unsigned char *pDst, const unsigned char *pSrc1, const unsigned char *pSrc2, int nPitch, int nWidth, int nHeight);
template<typename pixel_t>
void Average2_sse2(unsigned char *pDst, const unsigned char *pSrc1, const unsigned char *pSrc2, int nPitch, int nWidth, int nHeight);
//commented out: sse2 8 bit conversion backported from mvtools-vs
//extern "C" void Average2_iSSE(unsigned char *pDst, const unsigned char *pSrc1, const unsigned char *pSrc2, int nPitch, int nWidth, int nHeight);

#endif
