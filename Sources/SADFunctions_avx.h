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

//inline unsigned int SADABS(int x) {	return ( x < -16 ) ? 16 : ( x < 0 ) ? -x : ( x > 16) ? 16 : x; }

// simple AVX still does not support 256 bit integer SIMD versions of SSE2 functions
// Still the compiler can generate more optimized code
template<int nBlkWidth, int nBlkHeight, typename pixel_t>
unsigned int Sad_AVX_C(const uint8_t *pSrc, int nSrcPitch,const uint8_t *pRef,
					     int nRefPitch)
{
	unsigned int sum = 0; // int is probably enough for 32x32
	for ( int y = 0; y < nBlkHeight; y++ )
	{
		for ( int x = 0; x < nBlkWidth; x++ )
			sum += SADABS(reinterpret_cast<const pixel_t *>(pSrc)[x] - reinterpret_cast<const pixel_t *>(pRef)[x]);
      pSrc += nSrcPitch;
      pRef += nRefPitch;
	}
	return sum;
}


#endif
