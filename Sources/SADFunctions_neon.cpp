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

#include "SADFunctions_neon.h"
#include "BlockSizes.h"
#include "def.h"
#include <arm_neon.h>
#include <cstring>
#include <cstdlib>

// partial vector loads: no read beyond the block width
static MV_FORCEINLINE uint8x8_t load_4x8(const uint8_t *p) {
  uint32_t v;
  memcpy(&v, p, 4);
  return vreinterpret_u8_u32(vdup_n_u32(v));
}

static MV_FORCEINLINE uint8x8_t load_2x8(const uint8_t *p) {
  uint16_t v;
  memcpy(&v, p, 2);
  return vreinterpret_u8_u16(vdup_n_u16(v));
}

// The sum is exact (integer), so the summation order does not matter: identical to Sad_C.
// 8 bit: one row of 64 pixels adds at most 4*2*255 + 255 to a 16 bit lane, it is widened to 32 bit per row.
template<int nBlkWidth, int nBlkHeight, typename pixel_t>
unsigned int Sad_neon(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch)
{
  uint32x4_t acc = vdupq_n_u32(0);
  unsigned int tail = 0;

  if constexpr (sizeof(pixel_t) == 1) {
    constexpr int w16 = nBlkWidth / 16 * 16;
    constexpr bool has8 = (nBlkWidth % 16) >= 8;
    constexpr int x4 = w16 + (has8 ? 8 : 0);
    constexpr bool has4 = (nBlkWidth - x4) >= 4;
    constexpr int x2 = x4 + (has4 ? 4 : 0);
    constexpr bool has2 = (nBlkWidth - x2) >= 2;
    constexpr int x1 = x2 + (has2 ? 2 : 0);
    for (int y = 0; y < nBlkHeight; y++) {
      uint16x8_t row = vdupq_n_u16(0);
      for (int x = 0; x < w16; x += 16)
        row = vpadalq_u8(row, vabdq_u8(vld1q_u8(pSrc + x), vld1q_u8(pRef + x)));
      if constexpr (has8)
        row = vaddw_u8(row, vabd_u8(vld1_u8(pSrc + w16), vld1_u8(pRef + w16)));
      // the unused (duplicated) lanes are cut by the widening of the low half only
      if constexpr (has4)
        row = vaddq_u16(row, vcombine_u16(vget_low_u16(vmovl_u8(vabd_u8(load_4x8(pSrc + x4), load_4x8(pRef + x4)))), vdup_n_u16(0)));
      if constexpr (has2)
        row = vaddq_u16(row, vcombine_u16(vget_low_u16(vmovl_u8(vext_u8(vabd_u8(load_2x8(pSrc + x2), load_2x8(pRef + x2)), vdup_n_u8(0), 6))), vdup_n_u16(0)));
      for (int x = x1; x < nBlkWidth; x++)
        tail += std::abs((int)pSrc[x] - (int)pRef[x]);
      acc = vpadalq_u16(acc, row);
      pSrc += nSrcPitch;
      pRef += nRefPitch;
    }
  }
  else {
    constexpr int w8 = nBlkWidth / 8 * 8;
    constexpr bool has4 = (nBlkWidth % 8) >= 4;
    constexpr int x1 = w8 + (has4 ? 4 : 0);
    for (int y = 0; y < nBlkHeight; y++) {
      const uint16_t *s = reinterpret_cast<const uint16_t *>(pSrc);
      const uint16_t *r = reinterpret_cast<const uint16_t *>(pRef);
      for (int x = 0; x < w8; x += 8)
        acc = vpadalq_u16(acc, vabdq_u16(vld1q_u16(s + x), vld1q_u16(r + x)));
      if constexpr (has4)
        acc = vaddw_u16(acc, vabd_u16(vld1_u16(s + w8), vld1_u16(r + w8)));
      for (int x = x1; x < nBlkWidth; x++)
        tail += std::abs((int)s[x] - (int)r[x]);
      pSrc += nSrcPitch;
      pRef += nRefPitch;
    }
  }
  return vaddvq_u32(acc) + tail;
}

#define MAKE_SAD_FN(x, y) \
template unsigned int Sad_neon<x, y, uint8_t>(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch); \
template unsigned int Sad_neon<x, y, uint16_t>(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch);
MVTOOLS_BLOCKSIZES(MAKE_SAD_FN)
#undef MAKE_SAD_FN
