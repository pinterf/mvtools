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

#include "overlap_neon.h"
#include "BlockSizes.h"
#include "def.h"
#include <arm_neon.h>
#include <cstring>

// pWin: 0..2048 (11 bit), never negative: the unsigned widening multiplies are exact.
// 8 bit:  dst(short) += (val * win + 32) >> 6; vrshrn_n_u32(x, 6) is exactly (x + 32) >> 6, the result fits 13 bits,
//         the 16 bit add wraps the same way as the C short store.
// 16 bit: dst(int) += val * win (27 bits).
template <typename pixel_t, int blockWidth, int blockHeight>
void Overlaps_neon(uint16_t *pDst0, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch)
{
  constexpr int w8 = blockWidth / 8 * 8;
  constexpr bool has4 = (blockWidth % 8) >= 4;
  constexpr int x1 = w8 + (has4 ? 4 : 0);

  if constexpr (sizeof(pixel_t) == 1) {
    short *pDst = reinterpret_cast<short *>(pDst0);
    for (int j = 0; j < blockHeight; j++) {
      for (int i = 0; i < w8; i += 8) {
        const uint16x8_t val = vmovl_u8(vld1_u8(pSrc + i));
        const uint16x8_t win = vreinterpretq_u16_s16(vld1q_s16(pWin + i));
        const uint16x4_t lo = vrshrn_n_u32(vmull_u16(vget_low_u16(val), vget_low_u16(win)), 6);
        const uint16x4_t hi = vrshrn_n_u32(vmull_high_u16(val, win), 6);
        vst1q_s16(pDst + i, vaddq_s16(vld1q_s16(pDst + i), vreinterpretq_s16_u16(vcombine_u16(lo, hi))));
      }
      if constexpr (has4) {
        uint32_t v4;
        memcpy(&v4, pSrc + w8, 4);
        const uint16x4_t val = vget_low_u16(vmovl_u8(vreinterpret_u8_u32(vdup_n_u32(v4))));
        const uint16x4_t win = vreinterpret_u16_s16(vld1_s16(pWin + w8));
        const uint16x4_t res = vrshrn_n_u32(vmull_u16(val, win), 6);
        vst1_s16(pDst + w8, vadd_s16(vld1_s16(pDst + w8), vreinterpret_s16_u16(res)));
      }
      for (int i = x1; i < blockWidth; i++)
        pDst[i] = pDst[i] + ((pSrc[i] * pWin[i] + (1 << 5)) >> 6);
      pDst += nDstPitch;
      pSrc += nSrcPitch;
      pWin += nWinPitch;
    }
  }
  else {
    int *pDst = reinterpret_cast<int *>(pDst0);
    for (int j = 0; j < blockHeight; j++) {
      const uint16_t *src = reinterpret_cast<const uint16_t *>(pSrc);
      for (int i = 0; i < w8; i += 8) {
        const uint16x8_t val = vld1q_u16(src + i);
        const uint16x8_t win = vreinterpretq_u16_s16(vld1q_s16(pWin + i));
        uint32x4_t d0 = vreinterpretq_u32_s32(vld1q_s32(pDst + i));
        uint32x4_t d1 = vreinterpretq_u32_s32(vld1q_s32(pDst + i + 4));
        d0 = vmlal_u16(d0, vget_low_u16(val), vget_low_u16(win));
        d1 = vmlal_high_u16(d1, val, win);
        vst1q_s32(pDst + i, vreinterpretq_s32_u32(d0));
        vst1q_s32(pDst + i + 4, vreinterpretq_s32_u32(d1));
      }
      if constexpr (has4) {
        const uint16x4_t val = vld1_u16(src + w8);
        const uint16x4_t win = vreinterpret_u16_s16(vld1_s16(pWin + w8));
        const uint32x4_t d = vmlal_u16(vreinterpretq_u32_s32(vld1q_s32(pDst + w8)), val, win);
        vst1q_s32(pDst + w8, vreinterpretq_s32_u32(d));
      }
      for (int i = x1; i < blockWidth; i++)
        pDst[i] = pDst[i] + src[i] * pWin[i];
      pDst += nDstPitch;
      pSrc += nSrcPitch;
      pWin += nWinPitch;
    }
  }
}

// dst += val * (win * (1/2048)): the window scaling is exact (power of 2); the C is contracted to fmadd(val, win, dst)
// by GCC on aarch64, vfmaq_f32 gives the same result.
template <int blockWidth, int blockHeight>
void Overlaps_float_neon(uint16_t *pDst0, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch)
{
  constexpr int w4 = blockWidth / 4 * 4;
  float *pDst = reinterpret_cast<float *>(pDst0);
  const float32x4_t scale = vdupq_n_f32(1.0f / 2048.0f);
  for (int j = 0; j < blockHeight; j++) {
    const float *src = reinterpret_cast<const float *>(pSrc);
    for (int i = 0; i < w4; i += 4) {
      const float32x4_t win = vmulq_f32(vcvtq_f32_s32(vmovl_s16(vld1_s16(pWin + i))), scale);
      vst1q_f32(pDst + i, vfmaq_f32(vld1q_f32(pDst + i), vld1q_f32(src + i), win));
    }
    for (int i = w4; i < blockWidth; i++) {
      const float win = pWin[i] * (1.0f / 2048.0f);
      pDst[i] = pDst[i] + src[i] * win;
    }
    pDst += nDstPitch;
    pSrc += nSrcPitch;
    pWin += nWinPitch;
  }
}

#define MAKE_OVR_FN(x, y) \
template void Overlaps_neon<uint8_t, x, y>(uint16_t *pDst0, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch); \
template void Overlaps_neon<uint16_t, x, y>(uint16_t *pDst0, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch); \
template void Overlaps_float_neon<x, y>(uint16_t *pDst0, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch);
MVTOOLS_BLOCKSIZES(MAKE_OVR_FN)
#undef MAKE_OVR_FN
