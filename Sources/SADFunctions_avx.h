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

#ifndef __SAD_FUNC_AVX__
#define __SAD_FUNC_AVX__

#include "types.h"
#include <stdint.h>
#include "SADFunctions.h"
#include <immintrin.h>
#include <cassert>

SADFunction* get_sad_avx_C_function(int BlockX, int BlockY, int pixelsize, arch_t arch);

// static
/*
template<int nBlkWidth, int nBlkHeight, typename pixel_t>
unsigned int Sad_AVX_C(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef,
  int nRefPitch);
*/
template<int nBlkWidth, int nBlkHeight, typename pixel_t>
unsigned int Sad16_sse2_avx(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch);

template unsigned int Sad16_sse2_avx<32, 32, uint16_t>(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch);
template unsigned int Sad16_sse2_avx<32, 16, uint16_t>(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch);
template unsigned int Sad16_sse2_avx<32, 8, uint16_t>(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch);
template unsigned int Sad16_sse2_avx<16, 32,uint16_t>(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch);
template unsigned int Sad16_sse2_avx<16, 16,uint16_t>(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch);
template unsigned int Sad16_sse2_avx<16, 8,uint16_t>(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch);
template unsigned int Sad16_sse2_avx<16, 4,uint16_t>(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch);
template unsigned int Sad16_sse2_avx<16, 2,uint16_t>(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch);
template unsigned int Sad16_sse2_avx<16, 1,uint16_t>(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch);
template unsigned int Sad16_sse2_avx<8 , 16,uint16_t>(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch);
template unsigned int Sad16_sse2_avx<8 , 8,uint16_t>(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch);
template unsigned int Sad16_sse2_avx<8 , 4,uint16_t>(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch);
template unsigned int Sad16_sse2_avx<8 , 2,uint16_t>(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch);
template unsigned int Sad16_sse2_avx<8 , 1,uint16_t>(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch);
template unsigned int Sad16_sse2_avx<4 , 8,uint16_t>(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch);
template unsigned int Sad16_sse2_avx<4 , 4,uint16_t>(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch);
template unsigned int Sad16_sse2_avx<4 , 2,uint16_t>(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch);



#endif
