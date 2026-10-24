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
#include "DegrainLanes_neon.h"
#include <cstddef>

// Wall[] is normalized to a sum of exactly 1 << DEGRAIN_WEIGHT_BITS (256), never negative (MDegrainN::norm_weights):
// an 8 bit weighted sum is at most 255 * 256, it fits the 16 bit lanes, the results are exact.
// vrshrn_n(x, 8) is exactly (x + 128) >> 8, the rounding of DegrainN_C.
// trad is a runtime value: per block row the references are the outer loop, all 8 lane steps of the row
// (DegrainLanes_neon.h; blocks up to 4 pixels wide: two rows per step) are accumulated in registers,
// so a reference pointer and its weight are read once per row.
// one block row, or two rows (pair: blocks up to 4 pixels wide); a function, not a lambda: GCC does not inline the lambda
template <int blockWidth, typename pixel_t, int out16_type, bool pair>
static MV_FORCEINLINE void DegrainN_neon_rows(
  uint8_t* pDst, int nDstPitch,
  const uint8_t* pSrc, int nSrcPitch,
  const uint8_t* pRef[], int Pitch[],
  int Wall[], int nRefs, int h)
{
  constexpr int w8 = blockWidth / 8 * 8;
  constexpr int rest = blockWidth - w8;
  constexpr int nFull = w8 / 8;
  constexpr int nSteps = nFull + (rest > 0 ? 1 : 0);
  constexpr int restMode = pair ? LANES_PAIR : LANES_PART;
  const uint8_t* s = pSrc + (ptrdiff_t)h * nSrcPitch;
  uint8_t* d = pDst + (ptrdiff_t)h * nDstPitch;
  if constexpr (sizeof(pixel_t) == 1) {
    uint16x8_t acc[nSteps];
    const uint16_t w0 = (uint16_t)Wall[0];
    for (int c = 0; c < nFull; c++)
      acc[c] = vmulq_n_u16(vmovl_u8((load_lanes_u8<LANES_FULL, 8>(s + 8 * c, nSrcPitch))), w0);
    if constexpr (rest > 0)
      acc[nFull] = vmulq_n_u16(vmovl_u8((load_lanes_u8<restMode, rest>(s + w8, nSrcPitch))), w0);
    for (int j = 0; j < nRefs; j++) {
      const int pitch = Pitch[j];
      const uint8_t* r = pRef[j] + (ptrdiff_t)h * pitch;
      const uint16_t w = (uint16_t)Wall[j + 1];
      for (int c = 0; c < nFull; c++)
        acc[c] = vmlaq_n_u16(acc[c], vmovl_u8((load_lanes_u8<LANES_FULL, 8>(r + 8 * c, pitch))), w);
      if constexpr (rest > 0)
        acc[nFull] = vmlaq_n_u16(acc[nFull], vmovl_u8((load_lanes_u8<restMode, rest>(r + w8, pitch))), w);
    }
    for (int c = 0; c < nFull; c++)
      store_degrain_u8<out16_type, LANES_FULL, 8>(d, 8 * c, nDstPitch, acc[c]);
    if constexpr (rest > 0)
      store_degrain_u8<out16_type, restMode, rest>(d, w8, nDstPitch, acc[nFull]);
  }
  else {
    uint32x4_t lo[nSteps], hi[nSteps];
    const uint16_t w0 = (uint16_t)Wall[0];
    auto first = [&](int c, uint16x8_t v) {
      lo[c] = vmull_n_u16(vget_low_u16(v), w0);
      hi[c] = vmull_high_n_u16(v, w0);
    };
    for (int c = 0; c < nFull; c++)
      first(c, load_lanes_u16<LANES_FULL, 8>(s + 16 * c, nSrcPitch));
    if constexpr (rest > 0)
      first(nFull, load_lanes_u16<restMode, rest>(s + 2 * w8, nSrcPitch));
    for (int j = 0; j < nRefs; j++) {
      const int pitch = Pitch[j];
      const uint8_t* r = pRef[j] + (ptrdiff_t)h * pitch;
      const uint16_t w = (uint16_t)Wall[j + 1];
      auto add = [&](int c, uint16x8_t v) {
        lo[c] = vmlal_n_u16(lo[c], vget_low_u16(v), w);
        hi[c] = vmlal_high_n_u16(hi[c], v, w);
      };
      for (int c = 0; c < nFull; c++)
        add(c, load_lanes_u16<LANES_FULL, 8>(r + 16 * c, pitch));
      if constexpr (rest > 0)
        add(nFull, load_lanes_u16<restMode, rest>(r + 2 * w8, pitch));
    }
    for (int c = 0; c < nFull; c++)
      store_lanes_u16<LANES_FULL, 8>(d + 16 * c, nDstPitch, vcombine_u16(vrshrn_n_u32(lo[c], 8), vrshrn_n_u32(hi[c], 8)));
    if constexpr (rest > 0)
      store_lanes_u16<restMode, rest>(d + 2 * w8, nDstPitch, vcombine_u16(vrshrn_n_u32(lo[nFull], 8), vrshrn_n_u32(hi[nFull], 8)));
  }
}

template <int blockWidth, typename pixel_t, int out16_type>
static MV_FORCEINLINE void DegrainN_neon_impl(
  uint8_t* pDst, int nDstPitch,
  const uint8_t* pSrc, int nSrcPitch,
  const uint8_t* pRef[], int Pitch[],
  int Wall[], int trad, const int blockHeight)
{
  static_assert(DEGRAIN_WEIGHT_BITS == 8, "the 8 bit path needs the 16 bit range of the weighted sum");
  const int nRefs = trad * 2; // pRef[j] has the weight Wall[j + 1]
  int h = 0;
  if constexpr (blockWidth <= 4) {
    for (; h + 2 <= blockHeight; h += 2)
      DegrainN_neon_rows<blockWidth, pixel_t, out16_type, true>(pDst, nDstPitch, pSrc, nSrcPitch, pRef, Pitch, Wall, nRefs, h);
  }
  for (; h < blockHeight; h++)
    DegrainN_neon_rows<blockWidth, pixel_t, out16_type, false>(pDst, nDstPitch, pSrc, nSrcPitch, pRef, Pitch, Wall, nRefs, h);
}

// float: as GCC contracts DegrainN_C<float> on aarch64 (checked in the disassembly):
// val = fma(src, W0, 0.0f) (the + rounder; +0 also turns a -0 product into +0), then per reference pair
// val = val + fma(A, W1, B * W2), finally * (1/256)
template <int blockWidth, int blockHeight>
void DegrainN_float_neon(
  uint8_t* pDst, int nDstPitch,
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
  uint8_t* pDst, int nDstPitch,
  const uint8_t* pSrc, int nSrcPitch,
  const uint8_t* pRef[], int Pitch[],
  int Wall[], int trad
)
{
  DegrainN_neon_impl<blockWidth, pixel_t, out16_type>(pDst, nDstPitch, pSrc, nSrcPitch, pRef, Pitch, Wall, trad, blockHeight);
}

#define MAKE_FN(x, y) \
template void DegrainN_neon<uint8_t, x, y, 0>(uint8_t*, int, const uint8_t*, int, const uint8_t*[], int[], int[], int); \
template void DegrainN_neon<uint8_t, x, y, 2>(uint8_t*, int, const uint8_t*, int, const uint8_t*[], int[], int[], int); \
template void DegrainN_neon<uint16_t, x, y, 0>(uint8_t*, int, const uint8_t*, int, const uint8_t*[], int[], int[], int); \
template void DegrainN_float_neon<x, y>(uint8_t*, int, const uint8_t*, int, const uint8_t*[], int[], int[], int);
MVTOOLS_BLOCKSIZES(MAKE_FN)
#undef MAKE_FN
