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
#include "BlockSizes.h"
#include "DegrainLanes_neon.h"

// The weights are normalized to a sum of exactly 1 << DEGRAIN_WEIGHT_BITS (256) and are never negative (norm_weights),
// so an 8 bit weighted sum is at most 255 * 256: it fits the 16 bit lanes, the results are exact.
// vrshrn_n(x, 8) is exactly (x + 128) >> 8, the rounding of Degrain1to6_C.
// The block width is a template parameter: rows are 8 pixel steps plus a rest (DegrainLanes_neon.h), blocks
// up to 4 pixels wide take two rows per step. Pointers, pitches and weights are kept in locals: the caller's
// arrays could alias the stores, the compiler would reload them for every step.

// one 8 lane step at pixel x (a function, not a generic lambda: MSVC cannot parse that one)
template<typename pixel_t, int out16_type, int level, int mode, int n>
static MV_FORCEINLINE void Degrain1to6_neon_step(uint8_t *pDst, int nDstPitch, const uint8_t *pSrc, int nSrcPitch,
  const uint8_t *const rB[], const int pB[], const uint16_t wB[],
  const uint8_t *const rF[], const int pF[], const uint16_t wF[], uint16_t wS, int x)
{
  if constexpr (sizeof(pixel_t) == 1) {
    uint16x8_t acc = vmulq_n_u16(vmovl_u8((load_lanes_u8<mode, n>(pSrc + x, nSrcPitch))), wS);
    for (int k = 0; k < level; k++) {
      acc = vmlaq_n_u16(acc, vmovl_u8((load_lanes_u8<mode, n>(rF[k] + x, pF[k]))), wF[k]);
      acc = vmlaq_n_u16(acc, vmovl_u8((load_lanes_u8<mode, n>(rB[k] + x, pB[k]))), wB[k]);
    }
    store_degrain_u8<out16_type, mode, n>(pDst, x, nDstPitch, acc);
  }
  else {
    const int xb = 2 * x;
    const uint16x8_t s = load_lanes_u16<mode, n>(pSrc + xb, nSrcPitch);
    uint32x4_t lo = vmull_n_u16(vget_low_u16(s), wS);
    uint32x4_t hi = vmull_high_n_u16(s, wS);
    for (int k = 0; k < level; k++) {
      const uint16x8_t f = load_lanes_u16<mode, n>(rF[k] + xb, pF[k]);
      const uint16x8_t b = load_lanes_u16<mode, n>(rB[k] + xb, pB[k]);
      lo = vmlal_n_u16(lo, vget_low_u16(f), wF[k]);
      hi = vmlal_high_n_u16(hi, f, wF[k]);
      lo = vmlal_n_u16(lo, vget_low_u16(b), wB[k]);
      hi = vmlal_high_n_u16(hi, b, wB[k]);
    }
    store_lanes_u16<mode, n>(pDst + xb, nDstPitch, vcombine_u16(vrshrn_n_u32(lo, 8), vrshrn_n_u32(hi, 8)));
  }
}

template<int blockWidth, typename pixel_t, int out16_type, int level>
void Degrain1to6_neon(uint8_t *pDst, int WidthHeightForC, int nDstPitch, const uint8_t *pSrc, int nSrcPitch,
  const uint8_t *pRefB[], int BPitch[], const uint8_t *pRefF[], int FPitch[],
  int WSrc,
  int WRefB[], int WRefF[])
{
  static_assert(DEGRAIN_WEIGHT_BITS == 8, "the 8 bit path needs the 16 bit range of the weighted sum");
  const int blockHeight = (WidthHeightForC & 0xFFFF);
  constexpr int w8 = blockWidth / 8 * 8;
  constexpr int rest = blockWidth - w8;

  const uint8_t *rB[level], *rF[level];
  int pB[level], pF[level];
  uint16_t wB[level], wF[level];
  for (int k = 0; k < level; k++) {
    rB[k] = pRefB[k]; rF[k] = pRefF[k];
    pB[k] = BPitch[k]; pF[k] = FPitch[k];
    wB[k] = (uint16_t)WRefB[k]; wF[k] = (uint16_t)WRefF[k];
  }
  const uint16_t wS = (uint16_t)WSrc;

  auto next_rows = [&](int rows) {
    pDst += rows * nDstPitch;
    pSrc += rows * nSrcPitch;
    for (int k = 0; k < level; k++) {
      rB[k] += rows * pB[k];
      rF[k] += rows * pF[k];
    }
  };

  int h = 0;
  if constexpr (blockWidth <= 4) {
    for (; h + 2 <= blockHeight; h += 2) {
      Degrain1to6_neon_step<pixel_t, out16_type, level, LANES_PAIR, rest>(pDst, nDstPitch, pSrc, nSrcPitch, rB, pB, wB, rF, pF, wF, wS, 0);
      next_rows(2);
    }
  }
  for (; h < blockHeight; h++) {
    for (int x = 0; x < w8; x += 8)
      Degrain1to6_neon_step<pixel_t, out16_type, level, LANES_FULL, 8>(pDst, nDstPitch, pSrc, nSrcPitch, rB, pB, wB, rF, pF, wF, wS, x);
    if constexpr (rest > 0)
      Degrain1to6_neon_step<pixel_t, out16_type, level, LANES_PART, rest>(pDst, nDstPitch, pSrc, nSrcPitch, rB, pB, wB, rF, pF, wF, wS, w8);
    next_rows(1);
  }
}

#define MAKE_FN_LEVEL(w, level) \
template void Degrain1to6_neon<w, uint8_t, 0, level>(uint8_t *, int, int, const uint8_t *, int, const uint8_t *[], int[], const uint8_t *[], int[], int, int[], int[]); \
template void Degrain1to6_neon<w, uint8_t, 2, level>(uint8_t *, int, int, const uint8_t *, int, const uint8_t *[], int[], const uint8_t *[], int[], int, int[], int[]); \
template void Degrain1to6_neon<w, uint16_t, 0, level>(uint8_t *, int, int, const uint8_t *, int, const uint8_t *[], int[], const uint8_t *[], int[], int, int[], int[]);
#define MAKE_FN(w) \
MAKE_FN_LEVEL(w, 1) MAKE_FN_LEVEL(w, 2) MAKE_FN_LEVEL(w, 3) MAKE_FN_LEVEL(w, 4) MAKE_FN_LEVEL(w, 5) MAKE_FN_LEVEL(w, 6)
MVTOOLS_BLOCKWIDTHS(MAKE_FN)
#undef MAKE_FN
#undef MAKE_FN_LEVEL
