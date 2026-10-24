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

// partial vector loads: no read beyond the block width, the other lanes are zero (zero absolute difference)
static MV_FORCEINLINE uint8x8_t load_4x8(const uint8_t *p) {
  uint32_t v;
  memcpy(&v, p, 4);
  return vreinterpret_u8_u32(vset_lane_u32(v, vdup_n_u32(0), 0));
}

// 4 pixels of two rows
static MV_FORCEINLINE uint8x8_t load_4x8_2rows(const uint8_t *p0, const uint8_t *p1) {
  uint32_t a, b;
  memcpy(&a, p0, 4);
  memcpy(&b, p1, 4);
  return vreinterpret_u8_u32(vset_lane_u32(b, vdup_n_u32(a), 1));
}

static MV_FORCEINLINE uint8x8_t load_2x8(const uint8_t *p) {
  uint16_t v;
  memcpy(&v, p, 2);
  return vreinterpret_u8_u16(vset_lane_u16(v, vdup_n_u16(0), 0));
}

// The sum is exact (integer), so the summation order does not matter: identical to Sad_C.
// One accumulator would be a serial chain (latency bound): the vector steps of a row (16 bytes, then the
// 8/4/2 pixel rest) go round robin into 4 independent accumulators; narrow blocks put G rows into one round.
// 8 bit: 16 bit lanes, a lane gets at most 64 steps of 2 * 255 (64x64: 4 steps per row, one per accumulator).
template<int nBlkWidth, int nBlkHeight, typename pixel_t>
unsigned int Sad_neon(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch)
{
  unsigned int tail = 0;

  if constexpr (sizeof(pixel_t) == 1) {
    uint16x8_t acc[4] = { vdupq_n_u16(0), vdupq_n_u16(0), vdupq_n_u16(0), vdupq_n_u16(0) };
    if constexpr (nBlkWidth == 4) {
      // two rows in one vector
      constexpr int nPairs = nBlkHeight / 2;
      auto pair = [&](int k) {
        acc[k] = vabal_u8(acc[k], load_4x8_2rows(pSrc, pSrc + nSrcPitch), load_4x8_2rows(pRef, pRef + nRefPitch));
        pSrc += 2 * nSrcPitch;
        pRef += 2 * nRefPitch;
      };
      for (int p = 0; p < nPairs / 4; p++)
        for (int k = 0; k < 4; k++)
          pair(k);
      for (int k = 0; k < nPairs % 4; k++)
        pair(k);
      if constexpr (nBlkHeight & 1)
        acc[3] = vabal_u8(acc[3], load_4x8(pSrc), load_4x8(pRef));
    }
    else {
      static constexpr int w16 = nBlkWidth / 16 * 16;
      static constexpr bool has8 = (nBlkWidth % 16) >= 8;
      static constexpr int x4 = w16 + (has8 ? 8 : 0);
      static constexpr bool has4 = (nBlkWidth - x4) >= 4;
      static constexpr int x2 = x4 + (has4 ? 4 : 0);
      static constexpr bool has2 = (nBlkWidth - x2) >= 2;
      static constexpr int x1 = x2 + (has2 ? 2 : 0);
      static constexpr int nSteps = w16 / 16 + has8 + has4 + has2; // vector steps per row
      static constexpr int G = (nSteps == 0 || nSteps >= 4) ? 1 : 4 / nSteps; // rows per round
      auto row = [&](int r) { // r: row in the round
        int k = r * nSteps;
        for (int x = 0; x < w16; x += 16, k++)
          acc[k & 3] = vpadalq_u8(acc[k & 3], vabdq_u8(vld1q_u8(pSrc + x), vld1q_u8(pRef + x)));
        if constexpr (has8) {
          acc[k & 3] = vabal_u8(acc[k & 3], vld1_u8(pSrc + w16), vld1_u8(pRef + w16));
          k++;
        }
        if constexpr (has4) {
          acc[k & 3] = vabal_u8(acc[k & 3], load_4x8(pSrc + x4), load_4x8(pRef + x4));
          k++;
        }
        if constexpr (has2)
          acc[k & 3] = vabal_u8(acc[k & 3], load_2x8(pSrc + x2), load_2x8(pRef + x2));
        for (int x = x1; x < nBlkWidth; x++)
          tail += std::abs((int)pSrc[x] - (int)pRef[x]);
        pSrc += nSrcPitch;
        pRef += nRefPitch;
      };
      for (int y = 0; y < nBlkHeight / G; y++)
        for (int r = 0; r < G; r++)
          row(r);
      for (int r = 0; r < nBlkHeight % G; r++)
        row(r);
    }
    if constexpr (nBlkWidth * nBlkHeight <= 256) // the whole sum fits 16 bits
      return vaddvq_u16(vaddq_u16(vaddq_u16(acc[0], acc[1]), vaddq_u16(acc[2], acc[3]))) + tail;
    else {
      uint32x4_t sum = vpaddlq_u16(acc[0]);
      sum = vpadalq_u16(sum, acc[1]);
      sum = vpadalq_u16(sum, acc[2]);
      sum = vpadalq_u16(sum, acc[3]);
      return vaddvq_u32(sum) + tail;
    }
  }
  else {
    uint32x4_t acc[4] = { vdupq_n_u32(0), vdupq_n_u32(0), vdupq_n_u32(0), vdupq_n_u32(0) };
    static constexpr int w8 = nBlkWidth / 8 * 8;
    static constexpr bool has4 = (nBlkWidth % 8) >= 4;
    static constexpr int x1 = w8 + (has4 ? 4 : 0);
    static constexpr int nSteps = w8 / 8 + has4;
    static constexpr int G = (nSteps == 0 || nSteps >= 4) ? 1 : 4 / nSteps;
    auto row = [&](int r) {
      const uint16_t *s = reinterpret_cast<const uint16_t *>(pSrc);
      const uint16_t *rf = reinterpret_cast<const uint16_t *>(pRef);
      int k = r * nSteps;
      for (int x = 0; x < w8; x += 8, k++)
        acc[k & 3] = vpadalq_u16(acc[k & 3], vabdq_u16(vld1q_u16(s + x), vld1q_u16(rf + x)));
      if constexpr (has4)
        acc[k & 3] = vabal_u16(acc[k & 3], vld1_u16(s + w8), vld1_u16(rf + w8));
      for (int x = x1; x < nBlkWidth; x++)
        tail += std::abs((int)s[x] - (int)rf[x]);
      pSrc += nSrcPitch;
      pRef += nRefPitch;
    };
    for (int y = 0; y < nBlkHeight / G; y++)
      for (int r = 0; r < G; r++)
        row(r);
    for (int r = 0; r < nBlkHeight % G; r++)
      row(r);
    return vaddvq_u32(vaddq_u32(vaddq_u32(acc[0], acc[1]), vaddq_u32(acc[2], acc[3]))) + tail;
  }
}

#define MAKE_SAD_FN(x, y) \
template unsigned int Sad_neon<x, y, uint8_t>(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch); \
template unsigned int Sad_neon<x, y, uint16_t>(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch);
MVTOOLS_BLOCKSIZES(MAKE_SAD_FN)
#undef MAKE_SAD_FN

// ---- SATD (sum of absolute 4x4 Hadamard transformed differences), the same results as mvtools_satd_*_c:
// blocks with width mod 8 are summed by 8x4 pieces ((S_left + S_right) >> 1), the others by 4x4 pieces (S >> 1),
// where S is the exact sum of the 16 absolute coefficients. The abs sum does not depend on the coefficient order.

// 4 point Hadamard of 4 vectors (in place); intrinsics, not vector operators, and distinct names:
// MSVC has no operators for the NEON types and all its 128 bit NEON types are the same type
static MV_FORCEINLINE void hadamard4_s16(int16x8_t &v0, int16x8_t &v1, int16x8_t &v2, int16x8_t &v3)
{
  const int16x8_t a = vaddq_s16(v0, v1), b = vsubq_s16(v0, v1), c = vaddq_s16(v2, v3), d = vsubq_s16(v2, v3);
  v0 = vaddq_s16(a, c); v1 = vaddq_s16(b, d); v2 = vsubq_s16(a, c); v3 = vsubq_s16(b, d);
}

static MV_FORCEINLINE void hadamard4_s32(int32x4_t &v0, int32x4_t &v1, int32x4_t &v2, int32x4_t &v3)
{
  const int32x4_t a = vaddq_s32(v0, v1), b = vsubq_s32(v0, v1), c = vaddq_s32(v2, v3), d = vsubq_s32(v2, v3);
  v0 = vaddq_s32(a, c); v1 = vaddq_s32(b, d); v2 = vsubq_s32(a, c); v3 = vsubq_s32(b, d);
}

// 8 bit: two horizontally adjacent 4x4 blocks (x .. x+7, or x .. x+3 and the next 4x4 when only 4 wide is used:
// the caller takes the halves). Returns the abs sums of the left (low 4 lanes) and right block in s16x8 lanes summed.
static MV_FORCEINLINE uint16x8_t satd_2x4x4_u8_abs(const uint8_t *p1, int i1, const uint8_t *p2, int i2)
{
  int16x8_t r0 = vreinterpretq_s16_u16(vsubl_u8(vld1_u8(p1), vld1_u8(p2)));
  int16x8_t r1 = vreinterpretq_s16_u16(vsubl_u8(vld1_u8(p1 + i1), vld1_u8(p2 + i2)));
  int16x8_t r2 = vreinterpretq_s16_u16(vsubl_u8(vld1_u8(p1 + 2 * i1), vld1_u8(p2 + 2 * i2)));
  int16x8_t r3 = vreinterpretq_s16_u16(vsubl_u8(vld1_u8(p1 + 3 * i1), vld1_u8(p2 + 3 * i2)));
  hadamard4_s16(r0, r1, r2, r3); // vertical (between rows)
  // transpose both 4x4 blocks: lanes become rows
  const int16x8x2_t t0 = vtrnq_s16(r0, r1);
  const int16x8x2_t t1 = vtrnq_s16(r2, r3);
  const int32x4x2_t u0 = vtrnq_s32(vreinterpretq_s32_s16(t0.val[0]), vreinterpretq_s32_s16(t1.val[0]));
  const int32x4x2_t u1 = vtrnq_s32(vreinterpretq_s32_s16(t0.val[1]), vreinterpretq_s32_s16(t1.val[1]));
  int16x8_t c0 = vreinterpretq_s16_s32(u0.val[0]);
  int16x8_t c1 = vreinterpretq_s16_s32(u1.val[0]);
  int16x8_t c2 = vreinterpretq_s16_s32(u0.val[1]);
  int16x8_t c3 = vreinterpretq_s16_s32(u1.val[1]);
  hadamard4_s16(c0, c1, c2, c3); // horizontal; |coef| <= 16 * 255, the abs sum of 4 <= 16320
  return vaddq_u16(vaddq_u16(vreinterpretq_u16_s16(vabsq_s16(c0)), vreinterpretq_u16_s16(vabsq_s16(c1))),
    vaddq_u16(vreinterpretq_u16_s16(vabsq_s16(c2)), vreinterpretq_u16_s16(vabsq_s16(c3))));
}

// 16 bit: one 4x4 block in 32 bit lanes, returns the exact abs sum S
static MV_FORCEINLINE uint32_t satd_4x4_u16_abs(const uint8_t *p1, int i1, const uint8_t *p2, int i2)
{
  auto row = [](const uint8_t *a, const uint8_t *b) {
    return vreinterpretq_s32_u32(vsubl_u16(vld1_u16(reinterpret_cast<const uint16_t *>(a)), vld1_u16(reinterpret_cast<const uint16_t *>(b))));
  };
  int32x4_t r0 = row(p1, p2), r1 = row(p1 + i1, p2 + i2), r2 = row(p1 + 2 * i1, p2 + 2 * i2), r3 = row(p1 + 3 * i1, p2 + 3 * i2);
  hadamard4_s32(r0, r1, r2, r3);
  const int32x4x2_t t0 = vtrnq_s32(r0, r1);
  const int32x4x2_t t1 = vtrnq_s32(r2, r3);
  int32x4_t c0 = vcombine_s32(vget_low_s32(t0.val[0]), vget_low_s32(t1.val[0]));
  int32x4_t c1 = vcombine_s32(vget_low_s32(t0.val[1]), vget_low_s32(t1.val[1]));
  int32x4_t c2 = vcombine_s32(vget_high_s32(t0.val[0]), vget_high_s32(t1.val[0]));
  int32x4_t c3 = vcombine_s32(vget_high_s32(t0.val[1]), vget_high_s32(t1.val[1]));
  hadamard4_s32(c0, c1, c2, c3); // |coef| <= 16 * 65535
  const uint32x4_t s = vaddq_u32(vaddq_u32(vreinterpretq_u32_s32(vabsq_s32(c0)), vreinterpretq_u32_s32(vabsq_s32(c1))),
    vaddq_u32(vreinterpretq_u32_s32(vabsq_s32(c2)), vreinterpretq_u32_s32(vabsq_s32(c3))));
  return vaddvq_u32(s);
}

template<int nBlkWidth, int nBlkHeight, typename pixel_t>
unsigned int Satd_neon(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch)
{
  unsigned int sum = 0;
  for (int y = 0; y < nBlkHeight; y += 4) {
    const uint8_t *s = pSrc + y * nSrcPitch;
    const uint8_t *r = pRef + y * nRefPitch;
    if constexpr (nBlkWidth % 8 == 0) { // 8x4 pieces
      for (int x = 0; x < nBlkWidth; x += 8) {
        if constexpr (sizeof(pixel_t) == 1)
          sum += vaddlvq_u16(satd_2x4x4_u8_abs(s + x, nSrcPitch, r + x, nRefPitch)) >> 1;
        else
          sum += (satd_4x4_u16_abs(s + x * 2, nSrcPitch, r + x * 2, nRefPitch)
            + satd_4x4_u16_abs(s + x * 2 + 8, nSrcPitch, r + x * 2 + 8, nRefPitch)) >> 1;
      }
    }
    else { // 4x4 pieces
      for (int x = 0; x < nBlkWidth; x += 4) {
        if constexpr (sizeof(pixel_t) == 1) {
          if (x + 8 <= nBlkWidth) { // two at once
            const uint16x8_t a = satd_2x4x4_u8_abs(s + x, nSrcPitch, r + x, nRefPitch);
            sum += (vaddlv_u16(vget_low_u16(a)) >> 1) + (vaddlv_u16(vget_high_u16(a)) >> 1);
            x += 4;
          }
          else { // the last 4: no read beyond the block width
            uint8_t a[4][8] = {}, b[4][8] = {};
            for (int j = 0; j < 4; j++) {
              memcpy(a[j], s + x + j * nSrcPitch, 4);
              memcpy(b[j], r + x + j * nRefPitch, 4);
            }
            sum += vaddlv_u16(vget_low_u16(satd_2x4x4_u8_abs(a[0], 8, b[0], 8))) >> 1;
          }
        }
        else
          sum += satd_4x4_u16_abs(s + x * 2, nSrcPitch, r + x * 2, nRefPitch) >> 1;
      }
    }
  }
  return sum;
}

#define MAKE_SATD_FN(x, y) \
template unsigned int Satd_neon<x, y, uint8_t>(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch); \
template unsigned int Satd_neon<x, y, uint16_t>(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch);
MAKE_SATD_FN(64, 64) MAKE_SATD_FN(64, 48) MAKE_SATD_FN(64, 32) MAKE_SATD_FN(64, 16)
MAKE_SATD_FN(48, 64) MAKE_SATD_FN(48, 48) MAKE_SATD_FN(48, 24) MAKE_SATD_FN(48, 12)
MAKE_SATD_FN(32, 64) MAKE_SATD_FN(32, 32) MAKE_SATD_FN(32, 24) MAKE_SATD_FN(32, 16) MAKE_SATD_FN(32, 8) MAKE_SATD_FN(32, 4)
MAKE_SATD_FN(24, 48) MAKE_SATD_FN(24, 32) MAKE_SATD_FN(24, 24) MAKE_SATD_FN(24, 12)
MAKE_SATD_FN(16, 64) MAKE_SATD_FN(16, 32) MAKE_SATD_FN(16, 16) MAKE_SATD_FN(16, 12) MAKE_SATD_FN(16, 8) MAKE_SATD_FN(16, 4)
MAKE_SATD_FN(12, 48) MAKE_SATD_FN(12, 24) MAKE_SATD_FN(12, 16) MAKE_SATD_FN(12, 12)
MAKE_SATD_FN(8, 32) MAKE_SATD_FN(8, 16) MAKE_SATD_FN(8, 8) MAKE_SATD_FN(8, 4)
MAKE_SATD_FN(4, 32) MAKE_SATD_FN(4, 16) MAKE_SATD_FN(4, 8) MAKE_SATD_FN(4, 4)
#undef MAKE_SATD_FN
