// AVX2 versions of the subpixel interpolators (Wiener, bicubic), 8 and 10-16 bit

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

#ifndef __INTERPOLATION_AVX2__
#define __INTERPOLATION_AVX2__

// Same results as the C versions in Interpolation.cpp
template<typename pixel_t>
void VerticalWiener_avx2(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template<typename pixel_t>
void HorizontalWiener_avx2(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template<typename pixel_t>
void VerticalBicubic_avx2(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template<typename pixel_t>
void HorizontalBicubic_avx2(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);

#endif
