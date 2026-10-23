// Block luma sum: aarch64 NEON

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

#include "Variance_neon.h"
#include "BlockSizes.h"
#include "def.h"
#include <arm_neon.h>
#include <cstring>

// Exact integer sum, the same as Luma_C. 8 bit: a row of 64 pixels adds at most 4*2*255 + 255 to a 16 bit lane,
// it is widened to 32 bit per row; no read beyond the block width.
template<int nBlkWidth, int nBlkHeight, typename pixel_t>
unsigned int Luma_neon(const unsigned char *pSrc, int nSrcPitch)
{
  uint32x4_t acc = vdupq_n_u32(0);
  unsigned int tail = 0;
  if constexpr (sizeof(pixel_t) == 1) {
    constexpr int w16 = nBlkWidth / 16 * 16;
    constexpr bool has8 = (nBlkWidth % 16) >= 8;
    constexpr int x4 = w16 + (has8 ? 8 : 0);
    constexpr bool has4 = (nBlkWidth - x4) >= 4;
    constexpr int x1 = x4 + (has4 ? 4 : 0);
    for (int y = 0; y < nBlkHeight; y++) {
      uint16x8_t row = vdupq_n_u16(0);
      for (int x = 0; x < w16; x += 16)
        row = vpadalq_u8(row, vld1q_u8(pSrc + x));
      if constexpr (has8)
        row = vaddw_u8(row, vld1_u8(pSrc + w16));
      if constexpr (has4) {
        uint32_t v;
        memcpy(&v, pSrc + x4, 4);
        row = vaddq_u16(row, vcombine_u16(vget_low_u16(vmovl_u8(vreinterpret_u8_u32(vdup_n_u32(v)))), vdup_n_u16(0)));
      }
      for (int x = x1; x < nBlkWidth; x++)
        tail += pSrc[x];
      acc = vpadalq_u16(acc, row);
      pSrc += nSrcPitch;
    }
  }
  else {
    constexpr int w8 = nBlkWidth / 8 * 8;
    constexpr bool has4 = (nBlkWidth % 8) >= 4;
    constexpr int x1 = w8 + (has4 ? 4 : 0);
    for (int y = 0; y < nBlkHeight; y++) {
      const uint16_t *s = reinterpret_cast<const uint16_t *>(pSrc);
      for (int x = 0; x < w8; x += 8)
        acc = vpadalq_u16(acc, vld1q_u16(s + x));
      if constexpr (has4)
        acc = vaddw_u16(acc, vld1_u16(s + w8));
      for (int x = x1; x < nBlkWidth; x++)
        tail += s[x];
      pSrc += nSrcPitch;
    }
  }
  return vaddvq_u32(acc) + tail;
}

#define MAKE_LUMA_FN(x, y) \
template unsigned int Luma_neon<x, y, uint8_t>(const unsigned char *pSrc, int nSrcPitch); \
template unsigned int Luma_neon<x, y, uint16_t>(const unsigned char *pSrc, int nSrcPitch);
MVTOOLS_BLOCKSIZES(MAKE_LUMA_FN)
#undef MAKE_LUMA_FN
