// Functions that computes distances between blocks: aarch64 NEON

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

#ifndef __SAD_FUNC_NEON__
#define __SAD_FUNC_NEON__

#include <stdint.h>

// pixel_t: uint8_t (8 bit) or uint16_t (10-16 bit); exact sum, same result as Sad_C
template<int nBlkWidth, int nBlkHeight, typename pixel_t>
unsigned int Sad_neon(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch);

// SATD, same results as mvtools_satd_WxH_c (the same 8x4 / 4x4 piece decomposition)
template<int nBlkWidth, int nBlkHeight, typename pixel_t>
unsigned int Satd_neon(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch);

#endif
