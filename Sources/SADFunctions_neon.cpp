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

// ---- SATD (sum of absolute 4x4 Hadamard transformed differences), the same results as mvtools_satd_*_c:
// blocks with width mod 8 are summed by 8x4 pieces ((S_left + S_right) >> 1), the others by 4x4 pieces (S >> 1),
// where S is the exact sum of the 16 absolute coefficients. The abs sum does not depend on the coefficient order.

// 4 point Hadamard of 4 vectors (in place)
template<typename V>
static MV_FORCEINLINE void hadamard4(V &v0, V &v1, V &v2, V &v3)
{
  const V a = v0 + v1, b = v0 - v1, c = v2 + v3, d = v2 - v3;
  v0 = a + c; v1 = b + d; v2 = a - c; v3 = b - d;
}

// 8 bit: two horizontally adjacent 4x4 blocks (x .. x+7, or x .. x+3 and the next 4x4 when only 4 wide is used:
// the caller takes the halves). Returns the abs sums of the left (low 4 lanes) and right block in s16x8 lanes summed.
static MV_FORCEINLINE uint16x8_t satd_2x4x4_u8_abs(const uint8_t *p1, int i1, const uint8_t *p2, int i2)
{
  int16x8_t r0 = vreinterpretq_s16_u16(vsubl_u8(vld1_u8(p1), vld1_u8(p2)));
  int16x8_t r1 = vreinterpretq_s16_u16(vsubl_u8(vld1_u8(p1 + i1), vld1_u8(p2 + i2)));
  int16x8_t r2 = vreinterpretq_s16_u16(vsubl_u8(vld1_u8(p1 + 2 * i1), vld1_u8(p2 + 2 * i2)));
  int16x8_t r3 = vreinterpretq_s16_u16(vsubl_u8(vld1_u8(p1 + 3 * i1), vld1_u8(p2 + 3 * i2)));
  hadamard4(r0, r1, r2, r3); // vertical (between rows)
  // transpose both 4x4 blocks: lanes become rows
  const int16x8x2_t t0 = vtrnq_s16(r0, r1);
  const int16x8x2_t t1 = vtrnq_s16(r2, r3);
  const int32x4x2_t u0 = vtrnq_s32(vreinterpretq_s32_s16(t0.val[0]), vreinterpretq_s32_s16(t1.val[0]));
  const int32x4x2_t u1 = vtrnq_s32(vreinterpretq_s32_s16(t0.val[1]), vreinterpretq_s32_s16(t1.val[1]));
  int16x8_t c0 = vreinterpretq_s16_s32(u0.val[0]);
  int16x8_t c1 = vreinterpretq_s16_s32(u1.val[0]);
  int16x8_t c2 = vreinterpretq_s16_s32(u0.val[1]);
  int16x8_t c3 = vreinterpretq_s16_s32(u1.val[1]);
  hadamard4(c0, c1, c2, c3); // horizontal; |coef| <= 16 * 255, the abs sum of 4 <= 16320
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
  hadamard4(r0, r1, r2, r3);
  const int32x4x2_t t0 = vtrnq_s32(r0, r1);
  const int32x4x2_t t1 = vtrnq_s32(r2, r3);
  int32x4_t c0 = vcombine_s32(vget_low_s32(t0.val[0]), vget_low_s32(t1.val[0]));
  int32x4_t c1 = vcombine_s32(vget_low_s32(t0.val[1]), vget_low_s32(t1.val[1]));
  int32x4_t c2 = vcombine_s32(vget_high_s32(t0.val[0]), vget_high_s32(t1.val[0]));
  int32x4_t c3 = vcombine_s32(vget_high_s32(t0.val[1]), vget_high_s32(t1.val[1]));
  hadamard4(c0, c1, c2, c3); // |coef| <= 16 * 65535
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
