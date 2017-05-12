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

#ifndef __SAD_FUNC_AVX2__
#define __SAD_FUNC_AVX2__

#include "types.h"
#include <stdint.h>
#include <immintrin.h>
#include <cassert>

SADFunction* get_sad_avx2_C_function(int BlockX, int BlockY, int pixelsize, arch_t arch);

template<int nBlkWidth, int nBlkHeight, typename pixel_t>
unsigned int Sad16_avx2(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch);

// match with SADFunctions.cpp
#define MAKE_SAD_FN(x, y) template unsigned int Sad16_avx2<x, y, uint16_t>(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch);
  MAKE_SAD_FN(64, 64)
  MAKE_SAD_FN(64, 48)
  MAKE_SAD_FN(64, 32)
  MAKE_SAD_FN(64, 16)
  MAKE_SAD_FN(48, 64)
  MAKE_SAD_FN(48, 48)
  MAKE_SAD_FN(48, 24)
  MAKE_SAD_FN(48, 12)
  MAKE_SAD_FN(32, 64)
  MAKE_SAD_FN(32, 32)
  MAKE_SAD_FN(32, 24)
  MAKE_SAD_FN(32, 16)
  MAKE_SAD_FN(32, 8)
  // MAKE_SAD_FN(24, 32) // 24*2 is not mod 32 bytes
  MAKE_SAD_FN(16, 64)
  MAKE_SAD_FN(16, 32)
  MAKE_SAD_FN(16, 16)
  MAKE_SAD_FN(16, 12)
  MAKE_SAD_FN(16, 8)
  MAKE_SAD_FN(16, 4)
  MAKE_SAD_FN(16, 2)
  MAKE_SAD_FN(16, 1) // 32 bytes with height=1 is OK for AVX2
  //MAKE_SAD_FN(12, 16) 12*2 not mod 32 bytes
  MAKE_SAD_FN(8, 32)
  MAKE_SAD_FN(8, 16)
  MAKE_SAD_FN(8, 8)
  MAKE_SAD_FN(8, 4)
  MAKE_SAD_FN(8, 2)
  // MAKE_SAD_FN(8, 1) // 16 bytes with height=1 not supported for AVX2
  //MAKE_SAD_FN(4, 8)
  //MAKE_SAD_FN(4, 4)
  //MAKE_SAD_FN(4, 2)
  //MAKE_SAD_FN(4, 1)  // 8 bytes with height=1 not supported for SSE2
  //MAKE_SAD_FN(2, 4)  // 2 pixels 4 bytes not supported with SSE2
  //MAKE_SAD_FN(2, 2)
  //MAKE_SAD_FN(2, 1)
#undef MAKE_SAD_FN

#endif
