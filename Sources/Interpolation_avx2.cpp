// AVX2 versions of the subpixel interpolators (Wiener, bicubic), 8 and 10-16 bit

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

#include <immintrin.h>
#include <stdint.h>
#include "def.h"
#include "Interpolation_avx2.h"

// Same arithmetic as the SSE2 versions: 8 bit pixels are processed in 16 bit
// lanes, 10-16 bit pixels in 32 bit lanes; packus + min gives the clamp of the C code.
// The SIMD loops only work on whole blocks inside the row, the remaining columns
// are done in C, so nothing is read or written beyond nWidth.
// Only file-local helpers here (no std::), this file is compiled with AVX2.

static MV_FORCEINLINE int clamp_pixel(int v, int max_pixel_value)
{
  return v < 0 ? 0 : (v > max_pixel_value ? max_pixel_value : v);
}

// 8 bit: 16 pixels widened to 16 bit lanes
static MV_FORCEINLINE __m256i load16_u8(const uint8_t *p)
{
  return _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i *)p));
}

// 16 bit: 8 pixels widened to 32 bit lanes
static MV_FORCEINLINE __m256i load8_u16(const uint16_t *p)
{
  return _mm256_cvtepu16_epi32(_mm_loadu_si128((const __m128i *)p));
}

// 1, -5, 20, 20, -5, 1 taps: (m0 + (-m1 + (m2 << 2) + (m3 << 2) - m4) * 5 + m5 + 16) >> 5
static MV_FORCEINLINE __m256i wiener_epi16(__m256i m0, __m256i m1, __m256i m2, __m256i m3, __m256i m4, __m256i m5)
{
  m2 = _mm256_slli_epi16(_mm256_add_epi16(m2, m3), 2);
  m2 = _mm256_sub_epi16(m2, _mm256_add_epi16(m1, m4));
  m2 = _mm256_add_epi16(m2, _mm256_slli_epi16(m2, 2));
  m0 = _mm256_add_epi16(_mm256_add_epi16(m0, m5), m2);
  m0 = _mm256_add_epi16(m0, _mm256_set1_epi16(16));
  return _mm256_srai_epi16(m0, 5);
}

static MV_FORCEINLINE __m256i wiener_epi32(__m256i m0, __m256i m1, __m256i m2, __m256i m3, __m256i m4, __m256i m5)
{
  m2 = _mm256_slli_epi32(_mm256_add_epi32(m2, m3), 2);
  m2 = _mm256_sub_epi32(m2, _mm256_add_epi32(m1, m4));
  m2 = _mm256_add_epi32(m2, _mm256_slli_epi32(m2, 2));
  m0 = _mm256_add_epi32(_mm256_add_epi32(m0, m5), m2);
  m0 = _mm256_add_epi32(m0, _mm256_set1_epi32(16));
  return _mm256_srai_epi32(m0, 5);
}

// Catmull-Rom: (-m1 - m4 + (m2 + m3) * 9 + 8) >> 4
static MV_FORCEINLINE __m256i bicubic_epi16(__m256i m1, __m256i m2, __m256i m3, __m256i m4)
{
  __m256i t = _mm256_add_epi16(m2, m3);
  t = _mm256_add_epi16(_mm256_slli_epi16(t, 3), t);
  t = _mm256_sub_epi16(_mm256_sub_epi16(t, m1), m4);
  return _mm256_srai_epi16(_mm256_add_epi16(t, _mm256_set1_epi16(8)), 4);
}

static MV_FORCEINLINE __m256i bicubic_epi32(__m256i m1, __m256i m2, __m256i m3, __m256i m4)
{
  __m256i t = _mm256_add_epi32(m2, m3);
  t = _mm256_add_epi32(_mm256_slli_epi32(t, 3), t);
  t = _mm256_sub_epi32(_mm256_sub_epi32(t, m1), m4);
  return _mm256_srai_epi32(_mm256_add_epi32(t, _mm256_set1_epi32(8)), 4);
}

// two halves of 16 bit results (pixels 0-15, 16-31) to 32 bytes
static MV_FORCEINLINE void store32_u8(uint8_t *p, __m256i lo, __m256i hi)
{
  __m256i r = _mm256_packus_epi16(lo, hi); // lanes: lo0-7 hi0-7 | lo8-15 hi8-15
  _mm256_storeu_si256((__m256i *)p, _mm256_permute4x64_epi64(r, 0xD8));
}

// two halves of 32 bit results (pixels 0-7, 8-15) to 16 uint16_t, clamped to max
static MV_FORCEINLINE void store16_u16(uint16_t *p, __m256i lo, __m256i hi, __m256i max_pixel_value)
{
  __m256i r = _mm256_packus_epi32(lo, hi); // lanes: lo0-3 hi0-3 | lo4-7 hi4-7
  r = _mm256_permute4x64_epi64(r, 0xD8);
  _mm256_storeu_si256((__m256i *)p, _mm256_min_epu16(r, max_pixel_value));
}

// (a + b + 1) >> 1 for a whole row
template<typename pixel_t>
static MV_FORCEINLINE void avg_row(pixel_t *pDst, const pixel_t *pSrc1, const pixel_t *pSrc2, int nWidth)
{
  constexpr int block = 32 / sizeof(pixel_t);
  int x = 0;
  for (; x + block <= nWidth; x += block) {
    __m256i a = _mm256_loadu_si256((const __m256i *)(pSrc1 + x));
    __m256i b = _mm256_loadu_si256((const __m256i *)(pSrc2 + x));
    if constexpr (sizeof(pixel_t) == 1)
      a = _mm256_avg_epu8(a, b);
    else
      a = _mm256_avg_epu16(a, b);
    _mm256_storeu_si256((__m256i *)(pDst + x), a);
  }
  for (; x < nWidth; x++)
    pDst[x] = (pSrc1[x] + pSrc2[x] + 1) >> 1;
}

template<typename pixel_t>
void VerticalWiener_avx2(unsigned char *pDst8, const unsigned char *pSrc8, int nDstPitch,
  int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel)
{
  pixel_t *pDst = reinterpret_cast<pixel_t *>(pDst8);
  const pixel_t *pSrc = reinterpret_cast<const pixel_t *>(pSrc8);

  nSrcPitch /= sizeof(pixel_t);
  nDstPitch /= sizeof(pixel_t);

  const int max_pixel_value = sizeof(pixel_t) == 1 ? 255 : ((1 << bits_per_pixel) - 1);
  const __m256i max_pixel_value_v = _mm256_set1_epi16((short)max_pixel_value);
  constexpr int block = 32 / sizeof(pixel_t); // output pixels per iteration

  for (int y = 0; y < 2; y++) {
    avg_row(pDst, pSrc, pSrc + nSrcPitch, nWidth);
    pSrc += nSrcPitch;
    pDst += nDstPitch;
  }

  for (int y = 2; y < nHeight - 4; y++) {
    int x = 0;
    for (; x + block <= nWidth; x += block) {
      const pixel_t *p = pSrc + x;
      if constexpr (sizeof(pixel_t) == 1) {
        __m256i lo = wiener_epi16(load16_u8(p - nSrcPitch * 2), load16_u8(p - nSrcPitch), load16_u8(p),
          load16_u8(p + nSrcPitch), load16_u8(p + nSrcPitch * 2), load16_u8(p + nSrcPitch * 3));
        p += 16;
        __m256i hi = wiener_epi16(load16_u8(p - nSrcPitch * 2), load16_u8(p - nSrcPitch), load16_u8(p),
          load16_u8(p + nSrcPitch), load16_u8(p + nSrcPitch * 2), load16_u8(p + nSrcPitch * 3));
        store32_u8(pDst + x, lo, hi);
      }
      else {
        __m256i lo = wiener_epi32(load8_u16(p - nSrcPitch * 2), load8_u16(p - nSrcPitch), load8_u16(p),
          load8_u16(p + nSrcPitch), load8_u16(p + nSrcPitch * 2), load8_u16(p + nSrcPitch * 3));
        p += 8;
        __m256i hi = wiener_epi32(load8_u16(p - nSrcPitch * 2), load8_u16(p - nSrcPitch), load8_u16(p),
          load8_u16(p + nSrcPitch), load8_u16(p + nSrcPitch * 2), load8_u16(p + nSrcPitch * 3));
        store16_u16(pDst + x, lo, hi, max_pixel_value_v);
      }
    }
    for (; x < nWidth; x++)
      pDst[x] = clamp_pixel((pSrc[x - nSrcPitch * 2]
        + (-pSrc[x - nSrcPitch] + (pSrc[x] << 2) + (pSrc[x + nSrcPitch] << 2) - pSrc[x + nSrcPitch * 2]) * 5
        + pSrc[x + nSrcPitch * 3] + 16) >> 5, max_pixel_value);

    pSrc += nSrcPitch;
    pDst += nDstPitch;
  }

  for (int y = nHeight - 4; y < nHeight - 1; y++) {
    avg_row(pDst, pSrc, pSrc + nSrcPitch, nWidth);
    pSrc += nSrcPitch;
    pDst += nDstPitch;
  }

  for (int x = 0; x < nWidth; x++)
    pDst[x] = pSrc[x];
}

template<typename pixel_t>
void HorizontalWiener_avx2(unsigned char *pDst8, const unsigned char *pSrc8, int nDstPitch,
  int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel)
{
  pixel_t *pDst = reinterpret_cast<pixel_t *>(pDst8);
  const pixel_t *pSrc = reinterpret_cast<const pixel_t *>(pSrc8);

  nSrcPitch /= sizeof(pixel_t);
  nDstPitch /= sizeof(pixel_t);

  const int max_pixel_value = sizeof(pixel_t) == 1 ? 255 : ((1 << bits_per_pixel) - 1);
  const __m256i max_pixel_value_v = _mm256_set1_epi16((short)max_pixel_value);
  constexpr int block = 32 / sizeof(pixel_t); // output pixels per iteration

  for (int y = 0; y < nHeight; y++) {
    pDst[0] = (pSrc[0] + pSrc[1] + 1) >> 1;
    pDst[1] = (pSrc[1] + pSrc[2] + 1) >> 1;

    // Wiener for x = 2 .. nWidth - 5, reads x - 2 .. x + 3
    int x = 2;
    for (; x + block <= nWidth - 4; x += block) {
      const pixel_t *p = pSrc + x;
      if constexpr (sizeof(pixel_t) == 1) {
        __m256i lo = wiener_epi16(load16_u8(p - 2), load16_u8(p - 1), load16_u8(p),
          load16_u8(p + 1), load16_u8(p + 2), load16_u8(p + 3));
        p += 16;
        __m256i hi = wiener_epi16(load16_u8(p - 2), load16_u8(p - 1), load16_u8(p),
          load16_u8(p + 1), load16_u8(p + 2), load16_u8(p + 3));
        store32_u8(pDst + x, lo, hi);
      }
      else {
        __m256i lo = wiener_epi32(load8_u16(p - 2), load8_u16(p - 1), load8_u16(p),
          load8_u16(p + 1), load8_u16(p + 2), load8_u16(p + 3));
        p += 8;
        __m256i hi = wiener_epi32(load8_u16(p - 2), load8_u16(p - 1), load8_u16(p),
          load8_u16(p + 1), load8_u16(p + 2), load8_u16(p + 3));
        store16_u16(pDst + x, lo, hi, max_pixel_value_v);
      }
    }
    for (; x < nWidth - 4; x++)
      pDst[x] = clamp_pixel((pSrc[x - 2] + (-pSrc[x - 1] + (pSrc[x] << 2)
        + (pSrc[x + 1] << 2) - pSrc[x + 2]) * 5 + pSrc[x + 3] + 16) >> 5, max_pixel_value);
    for (; x < nWidth - 1; x++)
      pDst[x] = (pSrc[x] + pSrc[x + 1] + 1) >> 1;
    pDst[nWidth - 1] = pSrc[nWidth - 1];

    pDst += nDstPitch;
    pSrc += nSrcPitch;
  }
}

template<typename pixel_t>
void VerticalBicubic_avx2(unsigned char *pDst8, const unsigned char *pSrc8, int nDstPitch,
  int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel)
{
  pixel_t *pDst = reinterpret_cast<pixel_t *>(pDst8);
  const pixel_t *pSrc = reinterpret_cast<const pixel_t *>(pSrc8);

  nSrcPitch /= sizeof(pixel_t);
  nDstPitch /= sizeof(pixel_t);

  const int max_pixel_value = sizeof(pixel_t) == 1 ? 255 : ((1 << bits_per_pixel) - 1);
  const __m256i max_pixel_value_v = _mm256_set1_epi16((short)max_pixel_value);
  constexpr int block = 32 / sizeof(pixel_t); // output pixels per iteration

  avg_row(pDst, pSrc, pSrc + nSrcPitch, nWidth);
  pSrc += nSrcPitch;
  pDst += nDstPitch;

  for (int y = 1; y < nHeight - 3; y++) {
    int x = 0;
    for (; x + block <= nWidth; x += block) {
      const pixel_t *p = pSrc + x;
      if constexpr (sizeof(pixel_t) == 1) {
        __m256i lo = bicubic_epi16(load16_u8(p - nSrcPitch), load16_u8(p), load16_u8(p + nSrcPitch), load16_u8(p + nSrcPitch * 2));
        p += 16;
        __m256i hi = bicubic_epi16(load16_u8(p - nSrcPitch), load16_u8(p), load16_u8(p + nSrcPitch), load16_u8(p + nSrcPitch * 2));
        store32_u8(pDst + x, lo, hi);
      }
      else {
        __m256i lo = bicubic_epi32(load8_u16(p - nSrcPitch), load8_u16(p), load8_u16(p + nSrcPitch), load8_u16(p + nSrcPitch * 2));
        p += 8;
        __m256i hi = bicubic_epi32(load8_u16(p - nSrcPitch), load8_u16(p), load8_u16(p + nSrcPitch), load8_u16(p + nSrcPitch * 2));
        store16_u16(pDst + x, lo, hi, max_pixel_value_v);
      }
    }
    for (; x < nWidth; x++)
      pDst[x] = clamp_pixel((-pSrc[x - nSrcPitch] - pSrc[x + nSrcPitch * 2]
        + (pSrc[x] + pSrc[x + nSrcPitch]) * 9 + 8) >> 4, max_pixel_value);

    pSrc += nSrcPitch;
    pDst += nDstPitch;
  }

  for (int y = nHeight - 3; y < nHeight - 1; y++) {
    avg_row(pDst, pSrc, pSrc + nSrcPitch, nWidth);
    pSrc += nSrcPitch;
    pDst += nDstPitch;
  }

  for (int x = 0; x < nWidth; x++)
    pDst[x] = pSrc[x];
}

template<typename pixel_t>
void HorizontalBicubic_avx2(unsigned char *pDst8, const unsigned char *pSrc8, int nDstPitch,
  int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel)
{
  pixel_t *pDst = reinterpret_cast<pixel_t *>(pDst8);
  const pixel_t *pSrc = reinterpret_cast<const pixel_t *>(pSrc8);

  nSrcPitch /= sizeof(pixel_t);
  nDstPitch /= sizeof(pixel_t);

  const int max_pixel_value = sizeof(pixel_t) == 1 ? 255 : ((1 << bits_per_pixel) - 1);
  const __m256i max_pixel_value_v = _mm256_set1_epi16((short)max_pixel_value);
  constexpr int block = 32 / sizeof(pixel_t); // output pixels per iteration

  for (int y = 0; y < nHeight; y++) {
    pDst[0] = (pSrc[0] + pSrc[1] + 1) >> 1;

    // bicubic for x = 1 .. nWidth - 4, reads x - 1 .. x + 2
    int x = 1;
    for (; x + block <= nWidth - 3; x += block) {
      const pixel_t *p = pSrc + x;
      if constexpr (sizeof(pixel_t) == 1) {
        __m256i lo = bicubic_epi16(load16_u8(p - 1), load16_u8(p), load16_u8(p + 1), load16_u8(p + 2));
        p += 16;
        __m256i hi = bicubic_epi16(load16_u8(p - 1), load16_u8(p), load16_u8(p + 1), load16_u8(p + 2));
        store32_u8(pDst + x, lo, hi);
      }
      else {
        __m256i lo = bicubic_epi32(load8_u16(p - 1), load8_u16(p), load8_u16(p + 1), load8_u16(p + 2));
        p += 8;
        __m256i hi = bicubic_epi32(load8_u16(p - 1), load8_u16(p), load8_u16(p + 1), load8_u16(p + 2));
        store16_u16(pDst + x, lo, hi, max_pixel_value_v);
      }
    }
    for (; x < nWidth - 3; x++)
      pDst[x] = clamp_pixel((-(pSrc[x - 1] + pSrc[x + 2]) + (pSrc[x] + pSrc[x + 1]) * 9 + 8) >> 4, max_pixel_value);
    for (; x < nWidth - 1; x++)
      pDst[x] = (pSrc[x] + pSrc[x + 1] + 1) >> 1;
    pDst[nWidth - 1] = pSrc[nWidth - 1];

    pDst += nDstPitch;
    pSrc += nSrcPitch;
  }
}

template void VerticalWiener_avx2<uint8_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template void VerticalWiener_avx2<uint16_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template void HorizontalWiener_avx2<uint8_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template void HorizontalWiener_avx2<uint16_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template void VerticalBicubic_avx2<uint8_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template void VerticalBicubic_avx2<uint16_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template void HorizontalBicubic_avx2<uint8_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template void HorizontalBicubic_avx2<uint16_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
