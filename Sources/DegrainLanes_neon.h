// MDegrain1-6 and MDegrainN, aarch64 NEON: 8 lane loads and stores of a block row
// (included only by the *_neon.cpp files)

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

#ifndef __DEGRAINLANES_NEON__
#define __DEGRAINLANES_NEON__

#include "def.h"
#include <arm_neon.h>
#include <cstring>
#include <stdint.h>

// The weighting works on 8 lanes. A block row is covered by
//   LANES_FULL: 8 pixels,
//   LANES_PART: the n < 8 pixel rest of a row, the other lanes are zero and are not stored,
//   LANES_PAIR: n <= 4 pixels of two rows (pitch apart): lanes 0..n-1 and 4..4+n-1; for blocks up to 4 wide.
// No read or write beyond the n pixels.
enum { LANES_FULL, LANES_PART, LANES_PAIR };

template<int nbytes>
static MV_FORCEINLINE uint64_t load_le(const uint8_t *p) {
  uint64_t v = 0;
  memcpy(&v, p, nbytes);
  return v;
}

template<int nbytes>
static MV_FORCEINLINE void store_le(uint8_t *p, uint64_t v) {
  memcpy(p, &v, nbytes);
}

// 8 bit pixels
template<int mode, int n>
static MV_FORCEINLINE uint8x8_t load_lanes_u8(const uint8_t *p, int pitch) {
  if constexpr (mode == LANES_FULL)
    return vld1_u8(p);
  else if constexpr (mode == LANES_PART)
    return vcreate_u8(load_le<n>(p));
  else
    return vcreate_u8(load_le<n>(p) | (load_le<n>(p + pitch) << 32));
}

template<int mode, int n>
static MV_FORCEINLINE void store_lanes_u8(uint8_t *p, int pitch, uint8x8_t v) {
  if constexpr (mode == LANES_FULL)
    vst1_u8(p, v);
  else {
    const uint64_t x = vget_lane_u64(vreinterpret_u64_u8(v), 0);
    store_le<n>(p, x);
    if constexpr (mode == LANES_PAIR)
      store_le<n>(p + pitch, x >> 32);
  }
}

// 16 bit pixels (p is a byte pointer)
template<int mode, int n>
static MV_FORCEINLINE uint16x8_t load_lanes_u16(const uint8_t *p, int pitch) {
  if constexpr (mode == LANES_FULL)
    return vld1q_u16(reinterpret_cast<const uint16_t *>(p));
  else if constexpr (mode == LANES_PART) {
    const uint16x4_t lo = vcreate_u16(load_le<2 * (n < 4 ? n : 4)>(p));
    if constexpr (n > 4)
      return vcombine_u16(lo, vcreate_u16(load_le<2 * (n - 4)>(p + 8)));
    else
      return vcombine_u16(lo, vdup_n_u16(0));
  }
  else
    return vcombine_u16(vcreate_u16(load_le<2 * n>(p)), vcreate_u16(load_le<2 * n>(p + pitch)));
}

template<int mode, int n>
static MV_FORCEINLINE void store_lanes_u16(uint8_t *p, int pitch, uint16x8_t v) {
  if constexpr (mode == LANES_FULL)
    vst1q_u16(reinterpret_cast<uint16_t *>(p), v);
  else {
    const uint64_t lo = vgetq_lane_u64(vreinterpretq_u64_u16(v), 0);
    const uint64_t hi = vgetq_lane_u64(vreinterpretq_u64_u16(v), 1);
    if constexpr (mode == LANES_PART) {
      store_le<2 * (n < 4 ? n : 4)>(p, lo);
      if constexpr (n > 4)
        store_le<2 * (n - 4)>(p + 8, hi);
    }
    else {
      store_le<2 * n>(p, lo);
      store_le<2 * n>(p + pitch, hi);
    }
  }
}

// the weighted 8 bit sum (at most 255 * 256, 16 bit lanes) to the output formats
//   out16_type 0: (x + 128) >> 8, 2: 16 bit output
template<int out16_type, int mode, int n>
static MV_FORCEINLINE void store_degrain_u8(uint8_t *pDst, int x, int nDstPitch, uint16x8_t acc) {
  if constexpr (out16_type == 2)
    store_lanes_u16<mode, n>(pDst + 2 * x, nDstPitch, acc);
  else
    store_lanes_u8<mode, n>(pDst + x, nDstPitch, vrshrn_n_u16(acc, 8));
}

#endif
