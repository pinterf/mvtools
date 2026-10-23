// MSuper reduce and refine filters: aarch64 NEON

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

#include "Interpolation_neon.h"
#include <arm_neon.h>
#include <algorithm>

// All integer: (sum + 4) >> 3 == vrshr(sum, 3); the Wiener (sum + 16) >> 5 with the clamp to 0..255 == vqrshrun(sum, 5)
// (signed rounding shift = floor shift of the C on negative values, then unsigned saturation).

// 8 bit: a + 3b + 3c + d <= 8 * 255 fits 16 bit lanes; 16 bit: 32 bit lanes.
template<typename pixel_t>
int RB2BilinearFilteredVerticalLine_neon(pixel_t *pDst, const pixel_t *pSrc, int nSrcPitch, int nWidth)
{
  const int w8 = nWidth / 8 * 8;
  for (int x = 0; x < w8; x += 8) {
    if constexpr (sizeof(pixel_t) == 1) {
      const uint16x8_t a = vmovl_u8(vld1_u8(pSrc + x - nSrcPitch));
      const uint16x8_t b = vmovl_u8(vld1_u8(pSrc + x));
      const uint16x8_t c = vmovl_u8(vld1_u8(pSrc + x + nSrcPitch));
      const uint16x8_t d = vmovl_u8(vld1_u8(pSrc + x + nSrcPitch * 2));
      const uint16x8_t sum = vmlaq_n_u16(vaddq_u16(a, d), vaddq_u16(b, c), 3);
      vst1_u8(pDst + x, vmovn_u16(vrshrq_n_u16(sum, 3)));
    }
    else {
      const uint16x8_t a = vld1q_u16(pSrc + x - nSrcPitch);
      const uint16x8_t b = vld1q_u16(pSrc + x);
      const uint16x8_t c = vld1q_u16(pSrc + x + nSrcPitch);
      const uint16x8_t d = vld1q_u16(pSrc + x + nSrcPitch * 2);
      uint32x4_t lo = vaddl_u16(vget_low_u16(a), vget_low_u16(d));
      uint32x4_t hi = vaddl_high_u16(a, d);
      lo = vmlal_n_u16(vmlal_n_u16(lo, vget_low_u16(b), 3), vget_low_u16(c), 3);
      hi = vmlal_high_n_u16(vmlal_high_n_u16(hi, b, 3), c, 3);
      vst1q_u16(pDst + x, vcombine_u16(vrshrn_n_u32(lo, 3), vrshrn_n_u32(hi, 3)));
    }
  }
  return w8;
}

// in place: pSrc[x] = (s[2x-1] + 3 s[2x] + 3 s[2x+1] + s[2x+2] + 4) >> 3 for x in [1, n);
// a chunk of 8 is loaded completely before it is stored, and it only overwrites indexes below the next chunk's reads.
template<typename pixel_t>
int RB2BilinearFilteredHorizontalInplaceLine_neon(pixel_t *pSrc, int nWidth)
{
  int x = 1;
  for (; x + 8 <= nWidth - 1; x += 8) {
    if constexpr (sizeof(pixel_t) == 1) {
      const uint8x8x2_t m = vld2_u8(pSrc + 2 * x - 1); // val[0]: s[2x-1], val[1]: s[2x]
      const uint8x8x2_t p = vld2_u8(pSrc + 2 * x + 1); // val[0]: s[2x+1], val[1]: s[2x+2]
      const uint16x8_t sum = vmlaq_n_u16(vaddl_u8(m.val[0], p.val[1]), vaddl_u8(m.val[1], p.val[0]), 3);
      vst1_u8(pSrc + x, vmovn_u16(vrshrq_n_u16(sum, 3)));
    }
    else {
      const uint16x8x2_t m = vld2q_u16(pSrc + 2 * x - 1);
      const uint16x8x2_t p = vld2q_u16(pSrc + 2 * x + 1);
      uint32x4_t lo = vaddl_u16(vget_low_u16(m.val[0]), vget_low_u16(p.val[1]));
      uint32x4_t hi = vaddl_high_u16(m.val[0], p.val[1]);
      lo = vmlal_n_u16(vmlal_n_u16(lo, vget_low_u16(m.val[1]), 3), vget_low_u16(p.val[0]), 3);
      hi = vmlal_high_n_u16(vmlal_high_n_u16(hi, m.val[1], 3), p.val[0], 3);
      vst1q_u16(pSrc + x, vcombine_u16(vrshrn_n_u32(lo, 3), vrshrn_n_u32(hi, 3)));
    }
  }
  return x;
}

// Wiener 6 tap: (a + (-b + 4c + 4d - e) * 5 + f + 16) >> 5, clamped to 0..max_pixel_value.
// 8 bit: the sum is within -2550..10710, 16 bit lanes; 16 bit: 32 bit lanes.
template<typename pixel_t>
static inline void wiener8(pixel_t *dst, const pixel_t *a, const pixel_t *b, const pixel_t *c, const pixel_t *d, const pixel_t *e, const pixel_t *f,
  uint16x8_t vmax)
{
  if constexpr (sizeof(pixel_t) == 1) {
    const int16x8_t va = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(a)));
    const int16x8_t vb = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(b)));
    const int16x8_t vc = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(c)));
    const int16x8_t vd = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(d)));
    const int16x8_t ve = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(e)));
    const int16x8_t vf = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(f)));
    const int16x8_t mid = vsubq_s16(vshlq_n_s16(vaddq_s16(vc, vd), 2), vaddq_s16(vb, ve));
    const int16x8_t sum = vmlaq_n_s16(vaddq_s16(va, vf), mid, 5);
    vst1_u8(dst, vqrshrun_n_s16(sum, 5));
  }
  else {
    const uint16x8_t ua = vld1q_u16(a), ub = vld1q_u16(b), uc = vld1q_u16(c), ud = vld1q_u16(d), ue = vld1q_u16(e), uf = vld1q_u16(f);
    // mid = 4(c + d) - (b + e) in 32 bit
    const int32x4_t cd_lo = vreinterpretq_s32_u32(vaddl_u16(vget_low_u16(uc), vget_low_u16(ud)));
    const int32x4_t cd_hi = vreinterpretq_s32_u32(vaddl_high_u16(uc, ud));
    const int32x4_t be_lo = vreinterpretq_s32_u32(vaddl_u16(vget_low_u16(ub), vget_low_u16(ue)));
    const int32x4_t be_hi = vreinterpretq_s32_u32(vaddl_high_u16(ub, ue));
    const int32x4_t mid_lo = vsubq_s32(vshlq_n_s32(cd_lo, 2), be_lo);
    const int32x4_t mid_hi = vsubq_s32(vshlq_n_s32(cd_hi, 2), be_hi);
    const int32x4_t af_lo = vreinterpretq_s32_u32(vaddl_u16(vget_low_u16(ua), vget_low_u16(uf)));
    const int32x4_t af_hi = vreinterpretq_s32_u32(vaddl_high_u16(ua, uf));
    const int32x4_t sum_lo = vmlaq_n_s32(af_lo, mid_lo, 5);
    const int32x4_t sum_hi = vmlaq_n_s32(af_hi, mid_hi, 5);
    const uint16x8_t r = vcombine_u16(vqrshrun_n_s32(sum_lo, 5), vqrshrun_n_s32(sum_hi, 5));
    vst1q_u16(dst, vminq_u16(r, vmax));
  }
}

template<typename pixel_t>
void HorizontalWiener_neon(unsigned char *pDst8, const unsigned char *pSrc8, int nDstPitch,
  int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel)
{
  pixel_t *pDst = reinterpret_cast<pixel_t *>(pDst8);
  const pixel_t *pSrc = reinterpret_cast<const pixel_t *>(pSrc8);
  nSrcPitch /= sizeof(pixel_t);
  nDstPitch /= sizeof(pixel_t);
  const int max_pixel_value = sizeof(pixel_t) == 1 ? 255 : (1 << bits_per_pixel) - 1;
  const uint16x8_t vmax = vdupq_n_u16((uint16_t)max_pixel_value);

  for (int j = 0; j < nHeight; j++)
  {
    pDst[0] = (pSrc[0] + pSrc[1] + 1) >> 1;
    pDst[1] = (pSrc[1] + pSrc[2] + 1) >> 1;
    int i = 2;
    for (; i + 8 <= nWidth - 4; i += 8)
      wiener8<pixel_t>(pDst + i, pSrc + i - 2, pSrc + i - 1, pSrc + i, pSrc + i + 1, pSrc + i + 2, pSrc + i + 3, vmax);
    for (; i < nWidth - 4; i++)
      pDst[i] = std::min(max_pixel_value, std::max(0, ((pSrc[i - 2]) + (-(pSrc[i - 1]) + (pSrc[i] << 2)
        + (pSrc[i + 1] << 2) - (pSrc[i + 2])) * 5 + (pSrc[i + 3]) + 16) >> 5));
    for (int i = nWidth - 4; i < nWidth - 1; i++)
      pDst[i] = (pSrc[i] + pSrc[i + 1] + 1) >> 1;
    pDst[nWidth - 1] = pSrc[nWidth - 1];
    pDst += nDstPitch;
    pSrc += nSrcPitch;
  }
}

template<typename pixel_t>
void VerticalWiener_neon(unsigned char *pDst8, const unsigned char *pSrc8, int nDstPitch,
  int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel)
{
  pixel_t *pDst = reinterpret_cast<pixel_t *>(pDst8);
  const pixel_t *pSrc = reinterpret_cast<const pixel_t *>(pSrc8);
  nSrcPitch /= sizeof(pixel_t);
  nDstPitch /= sizeof(pixel_t);
  const int max_pixel_value = sizeof(pixel_t) == 1 ? 255 : (1 << bits_per_pixel) - 1;
  const uint16x8_t vmax = vdupq_n_u16((uint16_t)max_pixel_value);
  const int w8 = nWidth / 8 * 8;

  for (int j = 0; j < 2; j++)
  {
    for (int i = 0; i < nWidth; i++)
      pDst[i] = (pSrc[i] + pSrc[i + nSrcPitch] + 1) >> 1;
    pDst += nDstPitch;
    pSrc += nSrcPitch;
  }
  for (int j = 2; j < nHeight - 4; j++)
  {
    for (int i = 0; i < w8; i += 8)
      wiener8<pixel_t>(pDst + i, pSrc + i - nSrcPitch * 2, pSrc + i - nSrcPitch, pSrc + i, pSrc + i + nSrcPitch,
        pSrc + i + nSrcPitch * 2, pSrc + i + nSrcPitch * 3, vmax);
    for (int i = w8; i < nWidth; i++)
      pDst[i] = std::min(max_pixel_value, std::max(0,
        ((pSrc[i - nSrcPitch * 2])
          + (-(pSrc[i - nSrcPitch]) + (pSrc[i] << 2) + (pSrc[i + nSrcPitch] << 2) - (pSrc[i + nSrcPitch * 2])) * 5
          + (pSrc[i + nSrcPitch * 3]) + 16) >> 5));
    pDst += nDstPitch;
    pSrc += nSrcPitch;
  }
  for (int j = nHeight - 4; j < nHeight - 1; j++)
  {
    for (int i = 0; i < nWidth; i++)
      pDst[i] = (pSrc[i] + pSrc[i + nSrcPitch] + 1) >> 1;
    pDst += nDstPitch;
    pSrc += nSrcPitch;
  }
  for (int i = 0; i < nWidth; i++)
    pDst[i] = pSrc[i];
}

// RB2F: (2x2 box sum + 2) / 4 for all rows (pitches in bytes, as RB2F_C)
template<typename pixel_t>
void RB2F_neon(pixel_t *pDst, const pixel_t *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight)
{
  nDstPitch /= sizeof(pixel_t);
  nSrcPitch /= sizeof(pixel_t);
  const int w8 = nWidth / 8 * 8;
  for (int y = 0; y < nHeight; ++y)
  {
    for (int x = 0; x < w8; x += 8) {
      if constexpr (sizeof(pixel_t) == 1) {
        const uint8x8x2_t r0 = vld2_u8(pSrc + x * 2);
        const uint8x8x2_t r1 = vld2_u8(pSrc + x * 2 + nSrcPitch);
        const uint16x8_t s = vaddq_u16(vaddl_u8(r0.val[0], r0.val[1]), vaddl_u8(r1.val[0], r1.val[1]));
        vst1_u8(pDst + x, vrshrn_n_u16(s, 2));
      }
      else {
        const uint16x8x2_t r0 = vld2q_u16(pSrc + x * 2);
        const uint16x8x2_t r1 = vld2q_u16(pSrc + x * 2 + nSrcPitch);
        const uint32x4_t lo = vaddq_u32(vaddl_u16(vget_low_u16(r0.val[0]), vget_low_u16(r0.val[1])), vaddl_u16(vget_low_u16(r1.val[0]), vget_low_u16(r1.val[1])));
        const uint32x4_t hi = vaddq_u32(vaddl_high_u16(r0.val[0], r0.val[1]), vaddl_high_u16(r1.val[0], r1.val[1]));
        vst1q_u16(pDst + x, vcombine_u16(vrshrn_n_u32(lo, 2), vrshrn_n_u32(hi, 2)));
      }
    }
    for (int x = w8; x < nWidth; x++)
      pDst[x] = (pSrc[x * 2] + pSrc[x * 2 + 1] + pSrc[x * 2 + nSrcPitch] + pSrc[x * 2 + nSrcPitch + 1] + 2) / 4;
    pDst += nDstPitch;
    pSrc += nSrcPitch * 2;
  }
}

// 1 2 1 filter of three vectors: (a + 2b + c + 2) >> 2
template<typename pixel_t>
static inline void tap3_121(pixel_t *dst, uint16x8_t a, uint16x8_t b, uint16x8_t c)
{
  if constexpr (sizeof(pixel_t) == 1) {
    const uint16x8_t s = vaddq_u16(vaddq_u16(a, c), vshlq_n_u16(b, 1)); // <= 4 * 255
    vst1_u8(dst, vmovn_u16(vrshrq_n_u16(s, 2)));
  }
  else {
    uint32x4_t lo = vaddl_u16(vget_low_u16(a), vget_low_u16(c));
    uint32x4_t hi = vaddl_high_u16(a, c);
    lo = vmlal_n_u16(lo, vget_low_u16(b), 2);
    hi = vmlal_high_n_u16(hi, b, 2);
    vst1q_u16(dst, vcombine_u16(vrshrn_n_u32(lo, 2), vrshrn_n_u32(hi, 2)));
  }
}

template<typename pixel_t>
static inline uint16x8_t load8(const pixel_t *p)
{
  if constexpr (sizeof(pixel_t) == 1)
    return vmovl_u8(vld1_u8(p));
  else
    return vld1q_u16(p);
}

// RB2FilteredVertical row (element pitch): (s[-p] + 2 s[0] + s[p] + 2) >> 2 for x in [0, n), n returned
template<typename pixel_t>
int RB2FilteredVerticalLine_neon(pixel_t *pDst, const pixel_t *pSrc, int nSrcPitch, int nWidth)
{
  const int w8 = nWidth / 8 * 8;
  for (int x = 0; x < w8; x += 8)
    tap3_121<pixel_t>(pDst + x, load8(pSrc + x - nSrcPitch), load8(pSrc + x), load8(pSrc + x + nSrcPitch));
  return w8;
}

// RB2FilteredHorizontalInplace row: (s[2x-1] + 2 s[2x] + s[2x+1] + 2) >> 2 for x in [1, n), n returned
template<typename pixel_t>
int RB2FilteredHorizontalInplaceLine_neon(pixel_t *pSrc, int nWidth)
{
  int x = 1;
  for (; x + 8 <= nWidth - 1; x += 8) { // the last load reaches s[2x+2] (not used): stay within the C's reads
    if constexpr (sizeof(pixel_t) == 1) {
      const uint8x8x2_t m = vld2_u8(pSrc + 2 * x - 1); // s[2x-1], s[2x]
      const uint8x8x2_t p = vld2_u8(pSrc + 2 * x + 1); // s[2x+1], (s[2x+2])
      tap3_121<pixel_t>(pSrc + x, vmovl_u8(m.val[0]), vmovl_u8(m.val[1]), vmovl_u8(p.val[0]));
    }
    else {
      const uint16x8x2_t m = vld2q_u16(pSrc + 2 * x - 1);
      const uint16x8x2_t p = vld2q_u16(pSrc + 2 * x + 1);
      tap3_121<pixel_t>(pSrc + x, m.val[0], m.val[1], p.val[0]);
    }
  }
  return x;
}

// symmetric 6 tap (1, W1, W2, W2, W1, 1) with rounding (sum + (1 << (SH - 1))) >> SH; all weights positive:
// 8 bit sum <= 64 * 255 fits 16 bit lanes, 16 bit uses 32 bit lanes
template<typename pixel_t, int W1, int W2, int SH>
static inline void tap6(pixel_t *dst, uint16x8_t a, uint16x8_t b, uint16x8_t c, uint16x8_t d, uint16x8_t e, uint16x8_t f)
{
  if constexpr (sizeof(pixel_t) == 1) {
    uint16x8_t s = vaddq_u16(a, f);
    s = vmlaq_n_u16(s, vaddq_u16(b, e), W1);
    s = vmlaq_n_u16(s, vaddq_u16(c, d), W2);
    vst1_u8(dst, vmovn_u16(vrshrq_n_u16(s, SH)));
  }
  else {
    uint32x4_t lo = vaddl_u16(vget_low_u16(a), vget_low_u16(f));
    uint32x4_t hi = vaddl_high_u16(a, f);
    lo = vmlal_n_u16(vmlal_n_u16(lo, vget_low_u16(b), W1), vget_low_u16(e), W1);
    hi = vmlal_high_n_u16(vmlal_high_n_u16(hi, b, W1), e, W1);
    lo = vmlal_n_u16(vmlal_n_u16(lo, vget_low_u16(c), W2), vget_low_u16(d), W2);
    hi = vmlal_high_n_u16(vmlal_high_n_u16(hi, c, W2), d, W2);
    vst1q_u16(dst, vcombine_u16(vrshrn_n_u32(lo, SH), vrshrn_n_u32(hi, SH)));
  }
}

// RB2Quadratic/RB2Cubic vertical row (element pitch): rows -2..3, x in [0, n), n returned
template<typename pixel_t, int W1, int W2, int SH>
int RB2Tap6VerticalLine_neon(pixel_t *pDst, const pixel_t *pSrc, int nSrcPitch, int nWidth)
{
  const int w8 = nWidth / 8 * 8;
  for (int x = 0; x < w8; x += 8)
    tap6<pixel_t, W1, W2, SH>(pDst + x, load8(pSrc + x - nSrcPitch * 2), load8(pSrc + x - nSrcPitch), load8(pSrc + x),
      load8(pSrc + x + nSrcPitch), load8(pSrc + x + nSrcPitch * 2), load8(pSrc + x + nSrcPitch * 3));
  return w8;
}

// RB2Quadratic/RB2Cubic horizontal in-place row: s[2x-2..2x+3], x in [1, n), n returned
template<typename pixel_t, int W1, int W2, int SH>
int RB2Tap6HorizontalInplaceLine_neon(pixel_t *pSrc, int nWidth)
{
  int x = 1;
  for (; x + 8 <= nWidth - 1; x += 8) {
    if constexpr (sizeof(pixel_t) == 1) {
      const uint8x8x2_t m = vld2_u8(pSrc + 2 * x - 2); // s[2x-2], s[2x-1]
      const uint8x8x2_t c = vld2_u8(pSrc + 2 * x);     // s[2x],   s[2x+1]
      const uint8x8x2_t p = vld2_u8(pSrc + 2 * x + 2); // s[2x+2], s[2x+3]
      tap6<pixel_t, W1, W2, SH>(pSrc + x, vmovl_u8(m.val[0]), vmovl_u8(m.val[1]), vmovl_u8(c.val[0]),
        vmovl_u8(c.val[1]), vmovl_u8(p.val[0]), vmovl_u8(p.val[1]));
    }
    else {
      const uint16x8x2_t m = vld2q_u16(pSrc + 2 * x - 2);
      const uint16x8x2_t c = vld2q_u16(pSrc + 2 * x);
      const uint16x8x2_t p = vld2q_u16(pSrc + 2 * x + 2);
      tap6<pixel_t, W1, W2, SH>(pSrc + x, m.val[0], m.val[1], c.val[0], c.val[1], p.val[0], p.val[1]);
    }
  }
  return x;
}

template void RB2F_neon<uint8_t>(uint8_t *, const uint8_t *, int, int, int, int);
template void RB2F_neon<uint16_t>(uint16_t *, const uint16_t *, int, int, int, int);
template int RB2FilteredVerticalLine_neon<uint8_t>(uint8_t *, const uint8_t *, int, int);
template int RB2FilteredVerticalLine_neon<uint16_t>(uint16_t *, const uint16_t *, int, int);
template int RB2FilteredHorizontalInplaceLine_neon<uint8_t>(uint8_t *, int);
template int RB2FilteredHorizontalInplaceLine_neon<uint16_t>(uint16_t *, int);
template int RB2Tap6VerticalLine_neon<uint8_t, 9, 22, 6>(uint8_t *, const uint8_t *, int, int);
template int RB2Tap6VerticalLine_neon<uint16_t, 9, 22, 6>(uint16_t *, const uint16_t *, int, int);
template int RB2Tap6VerticalLine_neon<uint8_t, 5, 10, 5>(uint8_t *, const uint8_t *, int, int);
template int RB2Tap6VerticalLine_neon<uint16_t, 5, 10, 5>(uint16_t *, const uint16_t *, int, int);
template int RB2Tap6HorizontalInplaceLine_neon<uint8_t, 9, 22, 6>(uint8_t *, int);
template int RB2Tap6HorizontalInplaceLine_neon<uint16_t, 9, 22, 6>(uint16_t *, int);
template int RB2Tap6HorizontalInplaceLine_neon<uint8_t, 5, 10, 5>(uint8_t *, int);
template int RB2Tap6HorizontalInplaceLine_neon<uint16_t, 5, 10, 5>(uint16_t *, int);

// (a + b + 1) >> 1 == vrhadd, exact for 8 and 16 bit
template<typename pixel_t>
static inline void avg8(pixel_t *dst, const pixel_t *a, const pixel_t *b)
{
  if constexpr (sizeof(pixel_t) == 1)
    vst1_u8(dst, vrhadd_u8(vld1_u8(a), vld1_u8(b)));
  else
    vst1q_u16(dst, vrhaddq_u16(vld1q_u16(a), vld1q_u16(b)));
}

// (a + b + c + d + 2) >> 2
template<typename pixel_t>
static inline void avg4_8(pixel_t *dst, const pixel_t *a, const pixel_t *b, const pixel_t *c, const pixel_t *d)
{
  if constexpr (sizeof(pixel_t) == 1) {
    const uint16x8_t s = vaddq_u16(vaddl_u8(vld1_u8(a), vld1_u8(b)), vaddl_u8(vld1_u8(c), vld1_u8(d)));
    vst1_u8(dst, vrshrn_n_u16(s, 2));
  }
  else {
    const uint16x8_t va = vld1q_u16(a), vb = vld1q_u16(b), vc = vld1q_u16(c), vd = vld1q_u16(d);
    const uint32x4_t lo = vaddq_u32(vaddl_u16(vget_low_u16(va), vget_low_u16(vb)), vaddl_u16(vget_low_u16(vc), vget_low_u16(vd)));
    const uint32x4_t hi = vaddq_u32(vaddl_high_u16(va, vb), vaddl_high_u16(vc, vd));
    vst1q_u16(dst, vcombine_u16(vrshrn_n_u32(lo, 2), vrshrn_n_u32(hi, 2)));
  }
}

// bicubic 4 tap: (-(a + d) + (b + c) * 9 + 8) >> 4, clamped to 0..max_pixel_value
// 8 bit: within -510..4590, 16 bit lanes; 16 bit: 32 bit lanes
template<typename pixel_t>
static inline void bicubic8(pixel_t *dst, const pixel_t *a, const pixel_t *b, const pixel_t *c, const pixel_t *d, uint16x8_t vmax)
{
  if constexpr (sizeof(pixel_t) == 1) {
    const int16x8_t ad = vreinterpretq_s16_u16(vaddl_u8(vld1_u8(a), vld1_u8(d)));
    const int16x8_t bc = vreinterpretq_s16_u16(vaddl_u8(vld1_u8(b), vld1_u8(c)));
    vst1_u8(dst, vqrshrun_n_s16(vmlaq_n_s16(vnegq_s16(ad), bc, 9), 4));
  }
  else {
    const uint16x8_t va = vld1q_u16(a), vb = vld1q_u16(b), vc = vld1q_u16(c), vd = vld1q_u16(d);
    const int32x4_t ad_lo = vreinterpretq_s32_u32(vaddl_u16(vget_low_u16(va), vget_low_u16(vd)));
    const int32x4_t ad_hi = vreinterpretq_s32_u32(vaddl_high_u16(va, vd));
    const int32x4_t bc_lo = vreinterpretq_s32_u32(vaddl_u16(vget_low_u16(vb), vget_low_u16(vc)));
    const int32x4_t bc_hi = vreinterpretq_s32_u32(vaddl_high_u16(vb, vc));
    const uint16x8_t r = vcombine_u16(vqrshrun_n_s32(vmlaq_n_s32(vnegq_s32(ad_lo), bc_lo, 9), 4),
      vqrshrun_n_s32(vmlaq_n_s32(vnegq_s32(ad_hi), bc_hi, 9), 4));
    vst1q_u16(dst, vminq_u16(r, vmax));
  }
}

template<typename pixel_t>
void VerticalBilin_neon(unsigned char *pDst8, const unsigned char *pSrc8, int nDstPitch,
  int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel)
{
  pixel_t *pDst = reinterpret_cast<pixel_t *>(pDst8);
  const pixel_t *pSrc = reinterpret_cast<const pixel_t *>(pSrc8);
  nSrcPitch /= sizeof(pixel_t);
  nDstPitch /= sizeof(pixel_t);
  const int w8 = nWidth / 8 * 8;
  for (int j = 0; j < nHeight - 1; j++)
  {
    for (int i = 0; i < w8; i += 8)
      avg8<pixel_t>(pDst + i, pSrc + i, pSrc + i + nSrcPitch);
    for (int i = w8; i < nWidth; i++)
      pDst[i] = (pSrc[i] + pSrc[i + nSrcPitch] + 1) >> 1;
    pDst += nDstPitch;
    pSrc += nSrcPitch;
  }
  for (int i = 0; i < nWidth; i++)
    pDst[i] = pSrc[i];
}

template<typename pixel_t>
void HorizontalBilin_neon(unsigned char *pDst8, const unsigned char *pSrc8, int nDstPitch,
  int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel)
{
  pixel_t *pDst = reinterpret_cast<pixel_t *>(pDst8);
  const pixel_t *pSrc = reinterpret_cast<const pixel_t *>(pSrc8);
  nSrcPitch /= sizeof(pixel_t);
  nDstPitch /= sizeof(pixel_t);
  for (int j = 0; j < nHeight; j++)
  {
    int i = 0;
    for (; i + 8 <= nWidth - 1; i += 8)
      avg8<pixel_t>(pDst + i, pSrc + i, pSrc + i + 1);
    for (; i < nWidth - 1; i++)
      pDst[i] = (pSrc[i] + pSrc[i + 1] + 1) >> 1;
    pDst[nWidth - 1] = pSrc[nWidth - 1];
    pDst += nDstPitch;
    pSrc += nSrcPitch;
  }
}

template<typename pixel_t>
void DiagonalBilin_neon(unsigned char *pDst8, const unsigned char *pSrc8, int nDstPitch,
  int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel)
{
  pixel_t *pDst = reinterpret_cast<pixel_t *>(pDst8);
  const pixel_t *pSrc = reinterpret_cast<const pixel_t *>(pSrc8);
  nSrcPitch /= sizeof(pixel_t);
  nDstPitch /= sizeof(pixel_t);
  for (int j = 0; j < nHeight - 1; j++)
  {
    int i = 0;
    for (; i + 8 <= nWidth - 1; i += 8)
      avg4_8<pixel_t>(pDst + i, pSrc + i, pSrc + i + 1, pSrc + i + nSrcPitch, pSrc + i + nSrcPitch + 1);
    for (; i < nWidth - 1; i++)
      pDst[i] = (pSrc[i] + pSrc[i + 1] + pSrc[i + nSrcPitch] + pSrc[i + nSrcPitch + 1] + 2) >> 2;
    pDst[nWidth - 1] = (pSrc[nWidth - 1] + pSrc[nWidth + nSrcPitch - 1] + 1) >> 1;
    pDst += nDstPitch;
    pSrc += nSrcPitch;
  }
  int i = 0;
  for (; i + 8 <= nWidth - 1; i += 8)
    avg8<pixel_t>(pDst + i, pSrc + i, pSrc + i + 1);
  for (; i < nWidth - 1; i++) // except rightmost
    pDst[i] = (pSrc[i] + pSrc[i + 1] + 1) >> 1;
  pDst[nWidth - 1] = pSrc[nWidth - 1];
}

template<typename pixel_t>
void HorizontalBicubic_neon(unsigned char *pDst8, const unsigned char *pSrc8, int nDstPitch,
  int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel)
{
  pixel_t *pDst = reinterpret_cast<pixel_t *>(pDst8);
  const pixel_t *pSrc = reinterpret_cast<const pixel_t *>(pSrc8);
  nSrcPitch /= sizeof(pixel_t);
  nDstPitch /= sizeof(pixel_t);
  const int max_pixel_value = sizeof(pixel_t) == 1 ? 255 : (1 << bits_per_pixel) - 1;
  const uint16x8_t vmax = vdupq_n_u16((uint16_t)max_pixel_value);
  for (int j = 0; j < nHeight; j++)
  {
    pDst[0] = (pSrc[0] + pSrc[1] + 1) >> 1;
    int i = 1;
    for (; i + 8 <= nWidth - 3; i += 8)
      bicubic8<pixel_t>(pDst + i, pSrc + i - 1, pSrc + i, pSrc + i + 1, pSrc + i + 2, vmax);
    for (; i < nWidth - 3; i++)
      pDst[i] = std::min(max_pixel_value, std::max(0,
        (-(pSrc[i - 1] + pSrc[i + 2]) + (pSrc[i] + pSrc[i + 1]) * 9 + 8) >> 4));
    for (int i = nWidth - 3; i < nWidth - 1; i++)
      pDst[i] = (pSrc[i] + pSrc[i + 1] + 1) >> 1;
    pDst[nWidth - 1] = pSrc[nWidth - 1];
    pDst += nDstPitch;
    pSrc += nSrcPitch;
  }
}

template<typename pixel_t>
void VerticalBicubic_neon(unsigned char *pDst8, const unsigned char *pSrc8, int nDstPitch,
  int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel)
{
  pixel_t *pDst = reinterpret_cast<pixel_t *>(pDst8);
  const pixel_t *pSrc = reinterpret_cast<const pixel_t *>(pSrc8);
  nSrcPitch /= sizeof(pixel_t);
  nDstPitch /= sizeof(pixel_t);
  const int max_pixel_value = sizeof(pixel_t) == 1 ? 255 : (1 << bits_per_pixel) - 1;
  const uint16x8_t vmax = vdupq_n_u16((uint16_t)max_pixel_value);
  const int w8 = nWidth / 8 * 8;
  for (int i = 0; i < nWidth; i++)
    pDst[i] = (pSrc[i] + pSrc[i + nSrcPitch] + 1) >> 1;
  pDst += nDstPitch;
  pSrc += nSrcPitch;
  for (int j = 1; j < nHeight - 3; j++)
  {
    for (int i = 0; i < w8; i += 8)
      bicubic8<pixel_t>(pDst + i, pSrc + i - nSrcPitch, pSrc + i, pSrc + i + nSrcPitch, pSrc + i + nSrcPitch * 2, vmax);
    for (int i = w8; i < nWidth; i++)
      pDst[i] = std::min(max_pixel_value, std::max(0,
        (-pSrc[i - nSrcPitch] - pSrc[i + nSrcPitch * 2] + (pSrc[i] + pSrc[i + nSrcPitch]) * 9 + 8) >> 4));
    pDst += nDstPitch;
    pSrc += nSrcPitch;
  }
  for (int j = nHeight - 3; j < nHeight - 1; j++)
  {
    for (int i = 0; i < w8; i += 8)
      avg8<pixel_t>(pDst + i, pSrc + i, pSrc + i + nSrcPitch);
    for (int i = w8; i < nWidth; i++)
      pDst[i] = (pSrc[i] + pSrc[i + nSrcPitch] + 1) >> 1;
    pDst += nDstPitch;
    pSrc += nSrcPitch;
  }
  for (int i = 0; i < nWidth; i++)
    pDst[i] = pSrc[i];
}

template<typename pixel_t>
void Average2_neon(unsigned char *pDst8, const unsigned char *pSrc1_8, const unsigned char *pSrc2_8,
  int nPitch, int nWidth, int nHeight)
{
  pixel_t *pDst = reinterpret_cast<pixel_t *>(pDst8);
  const pixel_t *pSrc1 = reinterpret_cast<const pixel_t *>(pSrc1_8);
  const pixel_t *pSrc2 = reinterpret_cast<const pixel_t *>(pSrc2_8);
  nPitch /= sizeof(pixel_t);
  const int w8 = nWidth / 8 * 8;
  for (int j = 0; j < nHeight; j++)
  {
    for (int i = 0; i < w8; i += 8)
      avg8<pixel_t>(pDst + i, pSrc1 + i, pSrc2 + i);
    for (int i = w8; i < nWidth; i++)
      pDst[i] = (pSrc1[i] + pSrc2[i] + 1) >> 1;
    pDst += nPitch;
    pSrc1 += nPitch;
    pSrc2 += nPitch;
  }
}

#define MAKE_REFINE(name) \
template void name<uint8_t>(unsigned char *, const unsigned char *, int, int, int, int, int); \
template void name<uint16_t>(unsigned char *, const unsigned char *, int, int, int, int, int);
MAKE_REFINE(VerticalBilin_neon)
MAKE_REFINE(HorizontalBilin_neon)
MAKE_REFINE(DiagonalBilin_neon)
MAKE_REFINE(HorizontalBicubic_neon)
MAKE_REFINE(VerticalBicubic_neon)
#undef MAKE_REFINE
template void Average2_neon<uint8_t>(unsigned char *, const unsigned char *, const unsigned char *, int, int, int);
template void Average2_neon<uint16_t>(unsigned char *, const unsigned char *, const unsigned char *, int, int, int);

template int RB2BilinearFilteredVerticalLine_neon<uint8_t>(uint8_t *pDst, const uint8_t *pSrc, int nSrcPitch, int nWidth);
template int RB2BilinearFilteredVerticalLine_neon<uint16_t>(uint16_t *pDst, const uint16_t *pSrc, int nSrcPitch, int nWidth);
template int RB2BilinearFilteredHorizontalInplaceLine_neon<uint8_t>(uint8_t *pSrc, int nWidth);
template int RB2BilinearFilteredHorizontalInplaceLine_neon<uint16_t>(uint16_t *pSrc, int nWidth);
template void HorizontalWiener_neon<uint8_t>(unsigned char *, const unsigned char *, int, int, int, int, int);
template void HorizontalWiener_neon<uint16_t>(unsigned char *, const unsigned char *, int, int, int, int, int);
template void VerticalWiener_neon<uint8_t>(unsigned char *, const unsigned char *, int, int, int, int, int);
template void VerticalWiener_neon<uint16_t>(unsigned char *, const unsigned char *, int, int, int, int, int);
