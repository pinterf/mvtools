// MDegrainN weighting: aarch64 NEON

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

#include "MDegrainN_neon.h"
#include "MVDegrain3.h" // DEGRAIN_WEIGHT_BITS
#include "BlockSizes.h"
#include <arm_neon.h>

// Wall[] is normalized to a sum of exactly 1 << DEGRAIN_WEIGHT_BITS (256), never negative (MDegrainN::norm_weights):
// an 8 bit weighted sum is at most 255 * 256, it fits the 16 bit lanes, the results are exact.
// vrshrn_n(x, 8) is exactly (x + 128) >> 8, the rounding of DegrainN_C.
template <typename pixel_t, int out16_type>
static void DegrainN_neon_impl(
  uint8_t* pDst, uint8_t* pDstLsb, int nDstPitch,
  const uint8_t* pSrc, int nSrcPitch,
  const uint8_t* pRef[], int Pitch[],
  int Wall[], int trad, const int blockWidth, const int blockHeight)
{
  static_assert(DEGRAIN_WEIGHT_BITS == 8, "the 8 bit path needs the 16 bit range of the weighted sum");
  constexpr bool lsb_flag = (out16_type == 1);
  constexpr bool out16 = (out16_type == 2);
  const int w8 = blockWidth / 8 * 8;

  for (int h = 0; h < blockHeight; ++h)
  {
    if constexpr (sizeof(pixel_t) == 1) {
      for (int x = 0; x < w8; x += 8) {
        uint16x8_t acc = vmulq_n_u16(vmovl_u8(vld1_u8(pSrc + x)), (uint16_t)Wall[0]);
        for (int k = 0; k < trad; ++k) {
          acc = vmlaq_n_u16(acc, vmovl_u8(vld1_u8(pRef[k * 2] + x)), (uint16_t)Wall[k * 2 + 1]);
          acc = vmlaq_n_u16(acc, vmovl_u8(vld1_u8(pRef[k * 2 + 1] + x)), (uint16_t)Wall[k * 2 + 2]);
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
        uint32x4_t lo = vmull_n_u16(vget_low_u16(s), (uint16_t)Wall[0]);
        uint32x4_t hi = vmull_high_n_u16(s, (uint16_t)Wall[0]);
        for (int k = 0; k < trad; ++k) {
          const uint16x8_t b = vld1q_u16(reinterpret_cast<const uint16_t *>(pRef[k * 2]) + x);
          const uint16x8_t f = vld1q_u16(reinterpret_cast<const uint16_t *>(pRef[k * 2 + 1]) + x);
          lo = vmlal_n_u16(lo, vget_low_u16(b), (uint16_t)Wall[k * 2 + 1]);
          hi = vmlal_high_n_u16(hi, b, (uint16_t)Wall[k * 2 + 1]);
          lo = vmlal_n_u16(lo, vget_low_u16(f), (uint16_t)Wall[k * 2 + 2]);
          hi = vmlal_high_n_u16(hi, f, (uint16_t)Wall[k * 2 + 2]);
        }
        vst1q_u16(reinterpret_cast<uint16_t *>(pDst) + x, vcombine_u16(vrshrn_n_u32(lo, 8), vrshrn_n_u32(hi, 8)));
      }
    }
    // the rest, as DegrainN_C
    for (int x = w8; x < blockWidth; ++x) {
      int val = reinterpret_cast<const pixel_t*>(pSrc)[x] * Wall[0];
      for (int k = 0; k < trad; ++k)
        val += reinterpret_cast<const pixel_t*>(pRef[k * 2])[x] * Wall[k * 2 + 1]
          + reinterpret_cast<const pixel_t*>(pRef[k * 2 + 1])[x] * Wall[k * 2 + 2];
      if constexpr (lsb_flag) {
        pDst[x] = (uint8_t)(val >> 8);
        pDstLsb[x] = (uint8_t)(val & 255);
      }
      else if constexpr (out16)
        reinterpret_cast<uint16_t*>(pDst)[x] = (uint16_t)val;
      else
        reinterpret_cast<pixel_t*>(pDst)[x] = (pixel_t)((val + 128) >> 8);
    }

    pDst += nDstPitch;
    if constexpr (lsb_flag)
      pDstLsb += nDstPitch;
    pSrc += nSrcPitch;
    for (int k = 0; k < trad; ++k)
    {
      pRef[k * 2] += Pitch[k * 2];
      pRef[k * 2 + 1] += Pitch[k * 2 + 1];
    }
  }
}

// float: as GCC contracts DegrainN_C<float> on aarch64 (checked in the disassembly):
// val = fma(src, W0, 0.0f) (the + rounder; +0 also turns a -0 product into +0), then per reference pair
// val = val + fma(A, W1, B * W2), finally * (1/256)
template <int blockWidth, int blockHeight>
void DegrainN_float_neon(
  uint8_t* pDst, uint8_t* pDstLsb, int nDstPitch,
  const uint8_t* pSrc, int nSrcPitch,
  const uint8_t* pRef[], int Pitch[],
  int Wall[], int trad
)
{
  constexpr int w4 = blockWidth / 4 * 4;
  const float scaleback = 1.0f / (1 << DEGRAIN_WEIGHT_BITS);
  const float32x4_t vscale = vdupq_n_f32(scaleback);
  const float32x4_t vzero = vdupq_n_f32(0.0f);
  for (int h = 0; h < blockHeight; ++h)
  {
    for (int x = 0; x < w4; x += 4) {
      float32x4_t val = vfmaq_f32(vzero, vld1q_f32(reinterpret_cast<const float *>(pSrc) + x), vdupq_n_f32((float)Wall[0]));
      for (int k = 0; k < trad; ++k) {
        const float32x4_t a = vld1q_f32(reinterpret_cast<const float *>(pRef[k * 2]) + x);
        const float32x4_t b = vld1q_f32(reinterpret_cast<const float *>(pRef[k * 2 + 1]) + x);
        const float32x4_t t = vfmaq_f32(vmulq_f32(b, vdupq_n_f32((float)Wall[k * 2 + 2])), a, vdupq_n_f32((float)Wall[k * 2 + 1]));
        val = vaddq_f32(val, t);
      }
      vst1q_f32(reinterpret_cast<float *>(pDst) + x, vmulq_f32(val, vscale));
    }
    for (int x = w4; x < blockWidth; ++x) {
      float val = reinterpret_cast<const float*>(pSrc)[x] * (float)Wall[0] + 0.0f;
      for (int k = 0; k < trad; ++k)
        val += reinterpret_cast<const float*>(pRef[k * 2])[x] * (float)Wall[k * 2 + 1]
          + reinterpret_cast<const float*>(pRef[k * 2 + 1])[x] * (float)Wall[k * 2 + 2];
      reinterpret_cast<float*>(pDst)[x] = val * scaleback;
    }
    pDst += nDstPitch;
    pSrc += nSrcPitch;
    for (int k = 0; k < trad; ++k)
    {
      pRef[k * 2] += Pitch[k * 2];
      pRef[k * 2 + 1] += Pitch[k * 2 + 1];
    }
  }
}

template <typename pixel_t, int blockWidth, int blockHeight, int out16_type>
void DegrainN_neon(
  uint8_t* pDst, uint8_t* pDstLsb, int nDstPitch,
  const uint8_t* pSrc, int nSrcPitch,
  const uint8_t* pRef[], int Pitch[],
  int Wall[], int trad
)
{
  DegrainN_neon_impl<pixel_t, out16_type>(pDst, pDstLsb, nDstPitch, pSrc, nSrcPitch, pRef, Pitch, Wall, trad, blockWidth, blockHeight);
}

#define MAKE_FN(x, y) \
template void DegrainN_neon<uint8_t, x, y, 0>(uint8_t*, uint8_t*, int, const uint8_t*, int, const uint8_t*[], int[], int[], int); \
template void DegrainN_neon<uint8_t, x, y, 1>(uint8_t*, uint8_t*, int, const uint8_t*, int, const uint8_t*[], int[], int[], int); \
template void DegrainN_neon<uint8_t, x, y, 2>(uint8_t*, uint8_t*, int, const uint8_t*, int, const uint8_t*[], int[], int[], int); \
template void DegrainN_neon<uint16_t, x, y, 0>(uint8_t*, uint8_t*, int, const uint8_t*, int, const uint8_t*[], int[], int[], int); \
template void DegrainN_float_neon<x, y>(uint8_t*, uint8_t*, int, const uint8_t*, int, const uint8_t*[], int[], int[], int);
MVTOOLS_BLOCKSIZES(MAKE_FN)
#undef MAKE_FN
