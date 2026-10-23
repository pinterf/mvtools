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

template int RB2BilinearFilteredVerticalLine_neon<uint8_t>(uint8_t *pDst, const uint8_t *pSrc, int nSrcPitch, int nWidth);
template int RB2BilinearFilteredVerticalLine_neon<uint16_t>(uint16_t *pDst, const uint16_t *pSrc, int nSrcPitch, int nWidth);
template int RB2BilinearFilteredHorizontalInplaceLine_neon<uint8_t>(uint8_t *pSrc, int nWidth);
template int RB2BilinearFilteredHorizontalInplaceLine_neon<uint16_t>(uint16_t *pSrc, int nWidth);
template void HorizontalWiener_neon<uint8_t>(unsigned char *, const unsigned char *, int, int, int, int, int);
template void HorizontalWiener_neon<uint16_t>(unsigned char *, const unsigned char *, int, int, int, int, int);
template void VerticalWiener_neon<uint8_t>(unsigned char *, const unsigned char *, int, int, int, int, int);
template void VerticalWiener_neon<uint16_t>(unsigned char *, const unsigned char *, int, int, int, int, int);
