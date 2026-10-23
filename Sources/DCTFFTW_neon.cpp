// DCTFFTW pixel <-> float conversion: aarch64 NEON

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

#include "DCTFFTW.h"
#include <arm_neon.h>
#include <algorithm>
#include <cmath>

// same results as Bytes2Float_C: exact integer -> float
template<typename pixel_t>
void DCTFFTW::Bytes2Float_neon(const unsigned char * srcp, int src_pitch, float * realdata)
{
  const int floatpitch = sizex;
  const int w8 = sizex / 8 * 8;
  for (int j = 0; j < sizey; j++)
  {
    const pixel_t *s = reinterpret_cast<const pixel_t *>(srcp);
    for (int i = 0; i < w8; i += 8) {
      const uint16x8_t v = (sizeof(pixel_t) == 1) ? vmovl_u8(vld1_u8(reinterpret_cast<const uint8_t *>(s) + i))
        : vld1q_u16(reinterpret_cast<const uint16_t *>(s) + i);
      vst1q_f32(realdata + i, vcvtq_f32_u32(vmovl_u16(vget_low_u16(v))));
      vst1q_f32(realdata + i + 4, vcvtq_f32_u32(vmovl_high_u16(v)));
    }
    for (int i = w8; i < sizex; i++)
      realdata[i] = s[i];
    srcp += src_pitch;
    realdata += floatpitch;
  }
}

// same results as Float2Bytes_C: one multiply (no contraction question), DC truncated, AC rounded to nearest even
// (vcvtnq == lrint in the default rounding mode), clamped to 0..maxPixelValue
template<typename pixel_t>
void DCTFFTW::Float2Bytes_neon(unsigned char * dstp0, int dst_pitch, float * realdata)
{
  pixel_t *dstp = reinterpret_cast<pixel_t *>(dstp0);
  dst_pitch /= sizeof(pixel_t);
  const int floatpitch = sizex;
  const int maxPixelValue = (1 << bits_per_pixel) - 1;
  const int middlePixelValue = 1 << (bits_per_pixel - 1);
  const float32x4_t norm = vdupq_n_f32(dctNormalize_AC);
  const int32x4_t vmid = vdupq_n_s32(middlePixelValue);
  const int32x4_t vmax = vdupq_n_s32(maxPixelValue);
  const int32x4_t vzero = vdupq_n_s32(0);
  const int w4 = sizex / 4 * 4;

  const int first = std::min(maxPixelValue, std::max(0, int(realdata[0] * dctNormalize_DC) + middlePixelValue)); // DC
  pixel_t *dstp_save = dstp;

  for (int j = 0; j < sizey; j++)
  {
    for (int i = 0; i < w4; i += 4) {
      int32x4_t v = vaddq_s32(vcvtnq_s32_f32(vmulq_f32(vld1q_f32(realdata + i), norm)), vmid);
      v = vminq_s32(vmaxq_s32(v, vzero), vmax);
      const uint16x4_t h = vmovn_u32(vreinterpretq_u32_s32(v));
      if constexpr (sizeof(pixel_t) == 1) {
        const uint8x8_t b = vmovn_u16(vcombine_u16(h, h));
        vst1_lane_u32(reinterpret_cast<uint32_t *>(dstp + i), vreinterpret_u32_u8(b), 0);
      }
      else
        vst1_u16(reinterpret_cast<uint16_t *>(dstp) + i, h);
    }
    for (int i = w4; i < sizex; i++) {
      const float f = realdata[i] * dctNormalize_AC;
      dstp[i] = (pixel_t)std::min(maxPixelValue, std::max(0, int(std::lrint(f)) + middlePixelValue));
    }
    dstp += dst_pitch;
    realdata += floatpitch;
  }
  dstp_save[0] = (pixel_t)first; // overwrite the very first
}

template void DCTFFTW::Bytes2Float_neon<uint8_t>(const unsigned char *, int, float *);
template void DCTFFTW::Bytes2Float_neon<uint16_t>(const unsigned char *, int, float *);
template void DCTFFTW::Float2Bytes_neon<uint8_t>(unsigned char *, int, float *);
template void DCTFFTW::Float2Bytes_neon<uint16_t>(unsigned char *, int, float *);
