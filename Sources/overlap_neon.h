// Overlap block blending: aarch64 NEON

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

#ifndef __OVERLAP_NEON__
#define __OVERLAP_NEON__

#include <stdint.h>

// same results as Overlaps_C / Overlaps_float_C / OverlapsLsb_C
template <typename pixel_t, int blockWidth, int blockHeight>
void Overlaps_neon(uint16_t *pDst0, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch);

template <int blockWidth, int blockHeight>
void Overlaps_float_neon(uint16_t *pDst0, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch);

template <int blockWidth, int blockHeight>
void OverlapsLsb_neon(int *pDst, int nDstPitch, const unsigned char *pSrc, const unsigned char *pSrcLsb, int nSrcPitch, short *pWin, int nWinPitch);

#endif
