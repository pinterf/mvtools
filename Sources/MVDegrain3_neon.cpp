// MDegrain1-6 weighting: aarch64 NEON

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

#include "MVDegrain3_neon.h"
#include "MVDegrain3.h"
#include <arm_neon.h>

// The weights are normalized to a sum of exactly 1 << DEGRAIN_WEIGHT_BITS (256) and are never negative (norm_weights),
// so an 8 bit weighted sum is at most 255 * 256: it fits the 16 bit lanes, the results are exact.
// vrshrn_n(x, 8) is exactly (x + 128) >> 8, the rounding of Degrain1to6_C.
template<typename pixel_t, int out16_type, int level>
void Degrain1to6_neon(uint8_t *pDst, uint8_t *pDstLsb, int WidthHeightForC, int nDstPitch, const uint8_t *pSrc, int nSrcPitch,
  const uint8_t *pRefB[], int BPitch[], const uint8_t *pRefF[], int FPitch[],
  int WSrc,
  int WRefB[], int WRefF[])
{
  static_assert(DEGRAIN_WEIGHT_BITS == 8, "the 8 bit path needs the 16 bit range of the weighted sum");
  const int blockWidth = (WidthHeightForC >> 16);
  const int blockHeight = (WidthHeightForC & 0xFFFF);
  const int w8 = blockWidth / 8 * 8;

  constexpr bool lsb_flag = (out16_type == 1);
  constexpr bool out16 = (out16_type == 2);
  constexpr int SHIFTBACK = (lsb_flag || out16) ? (DEGRAIN_WEIGHT_BITS - 8) : DEGRAIN_WEIGHT_BITS;
  constexpr int rounder = (1 << SHIFTBACK) / 2;

  for (int h = 0; h < blockHeight; h++)
  {
    if constexpr (sizeof(pixel_t) == 1) {
      for (int x = 0; x < w8; x += 8) {
        uint16x8_t acc = vmulq_n_u16(vmovl_u8(vld1_u8(pSrc + x)), (uint16_t)WSrc);
        for (int k = 0; k < level; k++) {
          acc = vmlaq_n_u16(acc, vmovl_u8(vld1_u8(pRefF[k] + x)), (uint16_t)WRefF[k]);
          acc = vmlaq_n_u16(acc, vmovl_u8(vld1_u8(pRefB[k] + x)), (uint16_t)WRefB[k]);
        }
        if constexpr (lsb_flag) {
          vst1_u8(pDst + x, vshrn_n_u16(acc, 8));
          vst1_u8(pDstLsb + x, vmovn_u16(acc));
        }
        else if constexpr (out16)
          vst1q_u16(reinterpret_cast<uint16_t *>(pDst) + x, acc);
        else
          vst1_u8(pDst + x, vrshrn_n_u16(acc, 8));
      }
    }
    else {
      for (int x = 0; x < w8; x += 8) {
        const uint16x8_t s = vld1q_u16(reinterpret_cast<const uint16_t *>(pSrc) + x);
        uint32x4_t lo = vmull_n_u16(vget_low_u16(s), (uint16_t)WSrc);
        uint32x4_t hi = vmull_high_n_u16(s, (uint16_t)WSrc);
        for (int k = 0; k < level; k++) {
          const uint16x8_t f = vld1q_u16(reinterpret_cast<const uint16_t *>(pRefF[k]) + x);
          const uint16x8_t b = vld1q_u16(reinterpret_cast<const uint16_t *>(pRefB[k]) + x);
          lo = vmlal_n_u16(lo, vget_low_u16(f), (uint16_t)WRefF[k]);
          hi = vmlal_high_n_u16(hi, f, (uint16_t)WRefF[k]);
          lo = vmlal_n_u16(lo, vget_low_u16(b), (uint16_t)WRefB[k]);
          hi = vmlal_high_n_u16(hi, b, (uint16_t)WRefB[k]);
        }
        vst1q_u16(reinterpret_cast<uint16_t *>(pDst) + x, vcombine_u16(vrshrn_n_u32(lo, 8), vrshrn_n_u32(hi, 8)));
      }
    }
    // the rest, as Degrain1to6_C
    for (int x = w8; x < blockWidth; x++) {
      int val = reinterpret_cast<const pixel_t *>(pSrc)[x] * WSrc;
      for (int k = 0; k < level; k++)
        val += reinterpret_cast<const pixel_t *>(pRefF[k])[x] * WRefF[k] + reinterpret_cast<const pixel_t *>(pRefB[k])[x] * WRefB[k];
      val = (val + rounder) >> SHIFTBACK;
      if constexpr (lsb_flag) {
        pDst[x] = val >> 8;
        pDstLsb[x] = val & 255;
      }
      else if constexpr (out16)
        reinterpret_cast<uint16_t *>(pDst)[x] = val;
      else
        reinterpret_cast<pixel_t *>(pDst)[x] = val;
    }
    pDst += nDstPitch;
    if constexpr (lsb_flag)
      pDstLsb += nDstPitch;
    pSrc += nSrcPitch;
    for (int k = 0; k < level; k++) {
      pRefB[k] += BPitch[k];
      pRefF[k] += FPitch[k];
    }
  }
}

#define MAKE_FN_LEVEL(level) \
template void Degrain1to6_neon<uint8_t, 0, level>(uint8_t *, uint8_t *, int, int, const uint8_t *, int, const uint8_t *[], int[], const uint8_t *[], int[], int, int[], int[]); \
template void Degrain1to6_neon<uint8_t, 1, level>(uint8_t *, uint8_t *, int, int, const uint8_t *, int, const uint8_t *[], int[], const uint8_t *[], int[], int, int[], int[]); \
template void Degrain1to6_neon<uint8_t, 2, level>(uint8_t *, uint8_t *, int, int, const uint8_t *, int, const uint8_t *[], int[], const uint8_t *[], int[], int, int[], int[]); \
template void Degrain1to6_neon<uint16_t, 0, level>(uint8_t *, uint8_t *, int, int, const uint8_t *, int, const uint8_t *[], int[], const uint8_t *[], int[], int, int[], int[]);
MAKE_FN_LEVEL(1)
MAKE_FN_LEVEL(2)
MAKE_FN_LEVEL(3)
MAKE_FN_LEVEL(4)
MAKE_FN_LEVEL(5)
MAKE_FN_LEVEL(6)
#undef MAKE_FN_LEVEL
