// Make a motion compensate temporal denoiser
// Copyright(c)2006 A.G.Balakhnin aka Fizick
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

#if defined (__GNUC__) && ! defined (__INTEL_COMPILER)
#include <x86intrin.h>
// x86intrin.h includes header files for whatever instruction
// sets are specified on the compiler command line, such as: xopintrin.h, fma4intrin.h
#else
#include <immintrin.h> // MS version of immintrin.h covers AVX, AVX2 and FMA3
#endif // __GNUC__

#if !defined(__FMA__)
// Assume that all processors that have AVX2 also have FMA3
#if defined (__GNUC__) && ! defined (__INTEL_COMPILER) && ! defined (__clang__)
// Prevent error message in g++ when using FMA intrinsics with avx2:
#pragma message "It is recommended to specify also option -mfma when using -mavx2 or higher"
#else
#define __FMA__  1
#endif
#endif
// FMA3 instruction set
#if defined (__FMA__) && (defined(__GNUC__) || defined(__clang__))  && ! defined (__INTEL_COMPILER)
#include <fmaintrin.h>
#endif // __FMA__ 

#ifndef _mm256_loadu2_m128i
#define _mm256_loadu2_m128i(/* __m128i const* */ hiaddr, \
                            /* __m128i const* */ loaddr) \
    _mm256_set_m128i(_mm_loadu_si128(hiaddr), _mm_loadu_si128(loaddr))
#endif

#include "MVDegrain3.h"
#include "MVDegrain3_avx2.h"
//#include <map>
//#include <tuple>

#include <stdint.h>
#include "def.h"

#pragma warning( push )
#pragma warning( disable : 4101)
// out16_type: 
//   0: native 8 or 16
//   1: 8bit in, lsb
//   2: 8bit in, native16 out
//   3: 8 bit in: float out
template<int blockWidth, int blockHeight, int out16_type, int level>
void Degrain1to6_avx2(BYTE* pDst, BYTE* pDstLsb, int WidthHeightForC, int nDstPitch, const BYTE* pSrc, int nSrcPitch,
  const BYTE* pRefB[MAX_DEGRAIN], int BPitch[MAX_DEGRAIN], const BYTE* pRefF[MAX_DEGRAIN], int FPitch[MAX_DEGRAIN],
  int WSrc,
  int WRefB[MAX_DEGRAIN], int WRefF[MAX_DEGRAIN])
{
  // avoid unnecessary templates for larger heights
  const int blockHeightParam = (WidthHeightForC & 0xFFFF);
  const int realBlockHeight = blockHeight == 0 ? blockHeightParam : blockHeight;

  constexpr bool lsb_flag = (out16_type == 1);
  constexpr bool out16 = (out16_type == 2);
  constexpr bool out32 = (out16_type == 3);

  // B: Back F: Forward
  __m128i zero = _mm_setzero_si128();
  __m256i zero_256 = _mm256_setzero_si256();
  __m128i ws;
  __m256i ws_256;
  __m128i wb1, wf1;
  __m128i wb2, wf2;
  __m128i wb3, wf3;
  __m128i wb4, wf4;
  __m128i wb5, wf5;
  __m128i wb6, wf6;
  __m256i wbf1_256;
  __m256i wbf2_256;
  __m256i wbf3_256;
  __m256i wbf4_256;
  __m256i wbf5_256;
  __m256i wbf6_256;
  if constexpr (out32) {
    ws_256 = _mm256_set1_epi16(WSrc);
    wbf1_256 = _mm256_set1_epi32((WRefB[0] << 16) | WRefF[0]);
    if constexpr (level >= 2) {
      wbf2_256 = _mm256_set1_epi32((WRefB[1] << 16) | WRefF[1]);
      if constexpr (level >= 3) {
        wbf3_256 = _mm256_set1_epi32((WRefB[2] << 16) | WRefF[2]);
        if constexpr (level >= 4) {
          wbf4_256 = _mm256_set1_epi32((WRefB[3] << 16) | WRefF[3]);
          if constexpr (level >= 5) {
            wbf5_256 = _mm256_set1_epi32((WRefB[4] << 16) | WRefF[4]);
            if constexpr (level >= 6) {
              wbf6_256 = _mm256_set1_epi32((WRefB[5] << 16) | WRefF[5]);
            }
          }
        }
      }
    }
  }
  else {
    ws = _mm_set1_epi16(WSrc);
    wb1 = _mm_set1_epi16(WRefB[0]);
    wf1 = _mm_set1_epi16(WRefF[0]);
    if constexpr (level >= 2) {
      wb2 = _mm_set1_epi16(WRefB[1]);
      wf2 = _mm_set1_epi16(WRefF[1]);
      if constexpr (level >= 3) {
        wb3 = _mm_set1_epi16(WRefB[2]);
        wf3 = _mm_set1_epi16(WRefF[2]);
        if constexpr (level >= 4) {
          wb4 = _mm_set1_epi16(WRefB[3]);
          wf4 = _mm_set1_epi16(WRefF[3]);
          if constexpr (level >= 5) {
            wb5 = _mm_set1_epi16(WRefB[4]);
            wf5 = _mm_set1_epi16(WRefF[4]);
            if constexpr (level >= 6) {
              wb6 = _mm_set1_epi16(WRefB[5]);
              wf6 = _mm_set1_epi16(WRefF[5]);
            }
          }
        }
      }
    }
  }

  if constexpr (lsb_flag || out16 || out32)
  {
    // 8 bit in 16/32 bits out
    __m128i lsb_mask = _mm_set1_epi16(255);
    for (int h = 0; h < realBlockHeight; h++)
    {
      for (int x = 0; x < blockWidth; x += 8)
      {
        __m128i val; // basic, lsb (stacked) and out16 operation
        __m256i val2; // out32 (float output) case
        if constexpr (level == 6) {
          if constexpr (!out32) {
            val =
              _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pSrc + x))), ws),
                _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefB[0] + x))), wb1),
                  _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefF[0] + x))), wf1),
                    _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefB[1] + x))), wb2),
                      _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefF[1] + x))), wf2),
                        _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefB[2] + x))), wb3),
                          _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefF[2] + x))), wf3),
                            _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefB[3] + x))), wb4),
                              _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefF[3] + x))), wf4),
                                _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefB[4] + x))), wb5),
                                  _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefF[4] + x))), wf5),
                                    _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefB[5] + x))), wb6),
                                      _mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefF[5] + x))), wf6)))))))))))));
          }
          else {
            val2 = _mm256_madd_epi16(_mm256_cvtepu8_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pSrc + x)), zero)), ws_256);
            val2 = _mm256_add_epi32(val2, _mm256_madd_epi16(_mm256_cvtepu8_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[0] + x)), _mm_loadl_epi64((__m128i*)(pRefB[0] + x)))), wbf1_256));
            val2 = _mm256_add_epi32(val2, _mm256_madd_epi16(_mm256_cvtepu8_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[1] + x)), _mm_loadl_epi64((__m128i*)(pRefB[1] + x)))), wbf2_256));
            val2 = _mm256_add_epi32(val2, _mm256_madd_epi16(_mm256_cvtepu8_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[2] + x)), _mm_loadl_epi64((__m128i*)(pRefB[2] + x)))), wbf3_256));
            val2 = _mm256_add_epi32(val2, _mm256_madd_epi16(_mm256_cvtepu8_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[3] + x)), _mm_loadl_epi64((__m128i*)(pRefB[3] + x)))), wbf4_256));
            val2 = _mm256_add_epi32(val2, _mm256_madd_epi16(_mm256_cvtepu8_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[4] + x)), _mm_loadl_epi64((__m128i*)(pRefB[4] + x)))), wbf5_256));
            val2 = _mm256_add_epi32(val2, _mm256_madd_epi16(_mm256_cvtepu8_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[5] + x)), _mm_loadl_epi64((__m128i*)(pRefB[5] + x)))), wbf6_256));
          }
        }
        if constexpr (level == 5) {
          if constexpr (!out32) {
            val =
              _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pSrc + x))), ws),
                _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefB[0] + x))), wb1),
                  _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefF[0] + x))), wf1),
                    _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefB[1] + x))), wb2),
                      _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefF[1] + x))), wf2),
                        _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefB[2] + x))), wb3),
                          _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefF[2] + x))), wf3),
                            _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefB[3] + x))), wb4),
                              _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefF[3] + x))), wf4),
                                _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefB[4] + x))), wb5),
                                  _mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefF[4] + x))), wf5)))))))))));
          }
          else {
            val2 = _mm256_madd_epi16(_mm256_cvtepu8_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pSrc + x)), zero)), ws_256);
            val2 = _mm256_add_epi32(val2, _mm256_madd_epi16(_mm256_cvtepu8_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[0] + x)), _mm_loadl_epi64((__m128i*)(pRefB[0] + x)))), wbf1_256));
            val2 = _mm256_add_epi32(val2, _mm256_madd_epi16(_mm256_cvtepu8_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[1] + x)), _mm_loadl_epi64((__m128i*)(pRefB[1] + x)))), wbf2_256));
            val2 = _mm256_add_epi32(val2, _mm256_madd_epi16(_mm256_cvtepu8_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[2] + x)), _mm_loadl_epi64((__m128i*)(pRefB[2] + x)))), wbf3_256));
            val2 = _mm256_add_epi32(val2, _mm256_madd_epi16(_mm256_cvtepu8_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[3] + x)), _mm_loadl_epi64((__m128i*)(pRefB[3] + x)))), wbf4_256));
            val2 = _mm256_add_epi32(val2, _mm256_madd_epi16(_mm256_cvtepu8_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[4] + x)), _mm_loadl_epi64((__m128i*)(pRefB[4] + x)))), wbf5_256));
          }
        }
        if constexpr (level == 4) {
          if constexpr (!out32) {
            val =
              _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pSrc + x))), ws),
                _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefB[0] + x))), wb1),
                  _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefF[0] + x))), wf1),
                    _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefB[1] + x))), wb2),
                      _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefF[1] + x))), wf2),
                        _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefB[2] + x))), wb3),
                          _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefF[2] + x))), wf3),
                            _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefB[3] + x))), wb4),
                              _mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefF[3] + x))), wf4)))))))));
          }
          else {
            val2 = _mm256_madd_epi16(_mm256_cvtepu8_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pSrc + x)), zero)), ws_256);
            val2 = _mm256_add_epi32(val2, _mm256_madd_epi16(_mm256_cvtepu8_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[0] + x)), _mm_loadl_epi64((__m128i*)(pRefB[0] + x)))), wbf1_256));
            val2 = _mm256_add_epi32(val2, _mm256_madd_epi16(_mm256_cvtepu8_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[1] + x)), _mm_loadl_epi64((__m128i*)(pRefB[1] + x)))), wbf2_256));
            val2 = _mm256_add_epi32(val2, _mm256_madd_epi16(_mm256_cvtepu8_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[2] + x)), _mm_loadl_epi64((__m128i*)(pRefB[2] + x)))), wbf3_256));
            val2 = _mm256_add_epi32(val2, _mm256_madd_epi16(_mm256_cvtepu8_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[3] + x)), _mm_loadl_epi64((__m128i*)(pRefB[3] + x)))), wbf4_256));
          }
        }
        if constexpr (level == 3) {
          if constexpr (!out32) {
            val =
              _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pSrc + x))), ws),
                _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefB[0] + x))), wb1),
                  _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefF[0] + x))), wf1),
                    _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefB[1] + x))), wb2),
                      _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefF[1] + x))), wf2),
                        _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefB[2] + x))), wb3),
                          _mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefF[2] + x))), wf3)))))));
          }
          else {
            val2 = _mm256_madd_epi16(_mm256_cvtepu8_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pSrc + x)), zero)), ws_256);
            val2 = _mm256_add_epi32(val2, _mm256_madd_epi16(_mm256_cvtepu8_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[0] + x)), _mm_loadl_epi64((__m128i*)(pRefB[0] + x)))), wbf1_256));
            val2 = _mm256_add_epi32(val2, _mm256_madd_epi16(_mm256_cvtepu8_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[1] + x)), _mm_loadl_epi64((__m128i*)(pRefB[1] + x)))), wbf2_256));
            val2 = _mm256_add_epi32(val2, _mm256_madd_epi16(_mm256_cvtepu8_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[2] + x)), _mm_loadl_epi64((__m128i*)(pRefB[2] + x)))), wbf3_256));
          }
        }
        else if constexpr (level == 2) {
          if constexpr (!out32) {
            val =
              _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pSrc + x))), ws),
                _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefB[0] + x))), wb1),
                  _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefF[0] + x))), wf1),
                    _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefB[1] + x))), wb2),
                      _mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefF[1] + x))), wf2)))));
          }
          else {
            val2 = _mm256_madd_epi16(_mm256_cvtepu8_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pSrc + x)), zero)), ws_256);
            val2 = _mm256_add_epi32(val2, _mm256_madd_epi16(_mm256_cvtepu8_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[0] + x)), _mm_loadl_epi64((__m128i*)(pRefB[0] + x)))), wbf1_256));
            val2 = _mm256_add_epi32(val2, _mm256_madd_epi16(_mm256_cvtepu8_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[1] + x)), _mm_loadl_epi64((__m128i*)(pRefB[1] + x)))), wbf2_256));
          }
        }
        else if constexpr (level == 1) {
          if constexpr (!out32) {
            val =
              _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pSrc + x))), ws),
                _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefB[0] + x))), wb1),
                  _mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefF[0] + x))), wf1)));
          }
          else {
            val2 = _mm256_madd_epi16(_mm256_cvtepu8_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pSrc + x)), zero)), ws_256);
            val2 = _mm256_add_epi32(val2, _mm256_madd_epi16(_mm256_cvtepu8_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[0] + x)), _mm_loadl_epi64((__m128i*)(pRefB[0] + x)))), wbf1_256));
          }
        }

        if constexpr (lsb_flag) {
          if constexpr (blockWidth == 12) {
            // 8 from the first, 4 from the second cycle
            if (x == 0) { // 1st 8 bytes
              _mm_storel_epi64((__m128i*)(pDst + x), _mm_packus_epi16(_mm_srli_epi16(val, 8), zero));
              _mm_storel_epi64((__m128i*)(pDstLsb + x), _mm_packus_epi16(_mm_and_si128(val, lsb_mask), zero));
            }
            else { // 2nd 4 bytes
              *(uint32_t*)(pDst + x) = _mm_cvtsi128_si32(_mm_packus_epi16(_mm_srli_epi16(val, 8), zero));
              *(uint32_t*)(pDstLsb + x) = _mm_cvtsi128_si32(_mm_packus_epi16(_mm_and_si128(val, lsb_mask), zero));
            }
          }
          else if constexpr (blockWidth >= 8) {
            // 8, 16, 24, ....
            _mm_storel_epi64((__m128i*)(pDst + x), _mm_packus_epi16(_mm_srli_epi16(val, 8), zero));
            _mm_storel_epi64((__m128i*)(pDstLsb + x), _mm_packus_epi16(_mm_and_si128(val, lsb_mask), zero));
          }
          else if constexpr (blockWidth == 6) {
            // x is always 0
            // 4+2 bytes
            __m128i upper = _mm_packus_epi16(_mm_srli_epi16(val, 8), zero);
            __m128i lower = _mm_packus_epi16(_mm_and_si128(val, lsb_mask), zero);
            *(uint32_t*)(pDst + x) = _mm_cvtsi128_si32(upper);
            *(uint32_t*)(pDstLsb + x) = _mm_cvtsi128_si32(lower);
            *(uint16_t*)(pDst + x + 4) = (uint16_t)_mm_cvtsi128_si32(_mm_srli_si128(upper, 4));
            *(uint16_t*)(pDstLsb + x + 4) = (uint16_t)_mm_cvtsi128_si32(_mm_srli_si128(lower, 4));
          }
          else if constexpr (blockWidth == 4) {
            // x is always 0
            *(uint32_t*)(pDst + x) = _mm_cvtsi128_si32(_mm_packus_epi16(_mm_srli_epi16(val, 8), zero));
            *(uint32_t*)(pDstLsb + x) = _mm_cvtsi128_si32(_mm_packus_epi16(_mm_and_si128(val, lsb_mask), zero));
          }
          else if constexpr (blockWidth == 3) {
            // x is always 0
            // 2 + 1 bytes
            uint32_t reslo = _mm_cvtsi128_si32(_mm_packus_epi16(_mm_srli_epi16(val, 8), zero));
            uint32_t reshi = _mm_cvtsi128_si32(_mm_packus_epi16(_mm_and_si128(val, lsb_mask), zero));
            *(uint16_t*)(pDst + x) = (uint16_t)reslo;
            *(uint16_t*)(pDstLsb + x) = (uint16_t)reshi;
            *(uint8_t*)(pDst + x + 2) = (uint8_t)(reslo >> 16);
            *(uint8_t*)(pDstLsb + x + 2) = (uint8_t)(reshi >> 16);
          }
          else if constexpr (blockWidth == 2) {
            // x is always 0
            // 2 bytes
            uint32_t reslo = _mm_cvtsi128_si32(_mm_packus_epi16(_mm_srli_epi16(val, 8), zero));
            uint32_t reshi = _mm_cvtsi128_si32(_mm_packus_epi16(_mm_and_si128(val, lsb_mask), zero));
            *(uint16_t*)(pDst + x) = (uint16_t)reslo;
            *(uint16_t*)(pDstLsb + x) = (uint16_t)reshi;
          }
        }
        else if constexpr (out16) {
          // keep 16 bit result: no split into msb/lsb

          if constexpr (blockWidth == 12) {
            // 8 from the first, 4 from the second cycle
            if (x == 0) { // 1st 8 pixels 16 bytes
              _mm_store_si128((__m128i*)(pDst + x * 2), val);
            }
            else { // 2nd: 4 pixels 8 bytes bytes
              _mm_storel_epi64((__m128i*)(pDst + x * 2), val);
            }
          }
          else if constexpr (blockWidth >= 8) {
            // 8, 16, 24, ....
            _mm_store_si128((__m128i*)(pDst + x * 2), val);
          }
          else if constexpr (blockWidth == 6) {
            // x is always 0
            // 4+2 pixels: 8 + 4 bytes
            _mm_storel_epi64((__m128i*)(pDst + x * 2), val);
            *(uint32_t*)(pDst + x * 2 + 8) = (uint32_t)_mm_cvtsi128_si32(_mm_srli_si128(val, 8));
          }
          else if constexpr (blockWidth == 4) {
            // x is always 0, 4 pixels, 8 bytes
            _mm_storel_epi64((__m128i*)(pDst + x * 2), val);
          }
          else if constexpr (blockWidth == 3) {
            // x is always 0
            // 2+1 pixels: 4 + 2 bytes
            uint32_t reslo = _mm_cvtsi128_si32(val);
            uint16_t reshi = (uint16_t)_mm_cvtsi128_si32(_mm_srli_si128(val, 4));
            *(uint32_t*)(pDst + x * 2) = reslo;
            *(uint16_t*)(pDst + x * 2 + 4) = reshi;
          }
          else if constexpr (blockWidth == 2) {
            // x is always 0
            // 2 pixels: 4 bytes
            uint32_t reslo = _mm_cvtsi128_si32(val);
            *(uint32_t*)(pDst + x * 2) = reslo;
          }
        } // out16 end
        else if constexpr (out32) {
          // val2: 32 bit integer result by 'madd'
          // storage: converted to float
          if constexpr (blockWidth == 12) {
            // 8 pixels from the first, 4 from the second cycle
            __m256 valf = _mm256_cvtepi32_ps(val2);
            if (x == 0) {
              _mm256_storeu_ps((float*)(pDst + 0 * sizeof(float)), valf);
            } else { //if (x == 1) { // last 4 pixels 16 bytes
              // 4 pixels from the 2nd cycle
              __m128 valf_lo = _mm256_castps256_ps128(valf);
              _mm_storeu_ps((float*)(pDst + 8 * sizeof(float)), valf_lo);
            }
          }
          else if constexpr (blockWidth >= 8) {
            __m256 valf = _mm256_cvtepi32_ps(val2);
            _mm256_storeu_ps((float*)(pDst + x * sizeof(float)), valf);
          }
          else if constexpr (blockWidth == 6) {
            // x is always 0
            // 4+2 pixels: 16 + 8 bytes
            // 4 pixels
            __m256 valf = _mm256_cvtepi32_ps(val2);
            __m128 valf_lo = _mm256_castps256_ps128(valf);
            _mm_storeu_ps((float*)(pDst + 0 * sizeof(float)), valf_lo);
            // 2 pixels
            __m128 valf_hi = _mm256_extractf128_ps(valf, 1); // upper 128
            _mm_storel_epi64((__m128i*)(pDst + 4 * sizeof(float)), _mm_castps_si128(valf_hi));
          }
          else if constexpr (blockWidth == 4) {
            // x is always 0. 4 pixels, 16 bytes
            __m256 valf = _mm256_cvtepi32_ps(val2);
            __m128 valf_lo = _mm256_castps256_ps128(valf);
            _mm_store_ps((float*)(pDst + 0 * sizeof(float)), valf_lo);
          }
          else if constexpr (blockWidth == 3) {
            // x is always 0
            // 2+1 pixels: 8 + 4 bytes
            __m256 valf = _mm256_cvtepi32_ps(val2);
            __m128 valf_lo = _mm256_castps256_ps128(valf);
            _mm_storel_epi64((__m128i*)(pDst + 0 * sizeof(float)), _mm_castps_si128(valf_lo));
            // 1 pixels: single float
            float resf = _mm_cvtss_f32(_mm_castsi128_ps(_mm_srli_si128(_mm_castps_si128(valf_lo), 4)));
            *(float*)(pDst + 2 * sizeof(float)) = resf;
          }
          else if constexpr (blockWidth == 2) {
            // x is always 0
            // 2 pixels: 8 bytes
            __m256 valf = _mm256_cvtepi32_ps(val2);
            __m128 valf_lo = _mm256_castps256_ps128(valf);
            _mm_storel_epi64((__m128i*)(pDst + 0 * sizeof(float)), _mm_castps_si128(valf_lo));
          }
        } // out32 end

      }
      pDst += nDstPitch;
      if constexpr (lsb_flag)
        pDstLsb += nDstPitch;
      pSrc += nSrcPitch;

      pRefB[0] += BPitch[0];
      pRefF[0] += FPitch[0];
      if constexpr (level >= 2) {
        pRefB[1] += BPitch[1];
        pRefF[1] += FPitch[1];
        if constexpr (level >= 3) {
          pRefB[2] += BPitch[2];
          pRefF[2] += FPitch[2];
        }
        if constexpr (level >= 4) {
          pRefB[3] += BPitch[3];
          pRefF[3] += FPitch[3];
        }
        if constexpr (level >= 5) {
          pRefB[4] += BPitch[4];
          pRefF[4] += FPitch[4];
        }
        if constexpr (level >= 6) {
          pRefB[5] += BPitch[5];
          pRefF[5] += FPitch[5];
        }
      }
    }
  }
  else
  {
    // 8 bit in, 8 bit out
    __m128i rounder = _mm_set1_epi16(128);
    for (int h = 0; h < realBlockHeight; h++)
    {
      for (int x = 0; x < blockWidth; x += 8)
      {
        __m128i res;
        if constexpr (level == 6) {
          res = _mm_packus_epi16(_mm_srli_epi16(
            _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pSrc + x))), ws),
              _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefB[0] + x))), wb1),
                _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefF[0] + x))), wf1),
                  _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefB[1] + x))), wb2),
                    _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefF[1] + x))), wf2),
                      _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefB[2] + x))), wb3),
                        _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefF[2] + x))), wf3),
                          _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefB[3] + x))), wb4),
                            _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefF[3] + x))), wf4),
                              _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefB[4] + x))), wb5),
                                _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefF[4] + x))), wf5),
                                  _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefB[5] + x))), wb6),
                                    _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefF[5] + x))), wf6),
                                      rounder))))))))))))), 8), zero);
        }
        if constexpr (level == 5) {
          res = _mm_packus_epi16(_mm_srli_epi16(
            _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pSrc + x))), ws),
              _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefB[0] + x))), wb1),
                _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefF[0] + x))), wf1),
                  _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefB[1] + x))), wb2),
                    _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefF[1] + x))), wf2),
                      _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefB[2] + x))), wb3),
                        _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefF[2] + x))), wf3),
                          _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefB[3] + x))), wb4),
                            _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefF[3] + x))), wf4),
                              _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefB[4] + x))), wb5),
                                _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefF[4] + x))), wf5),
                                  rounder))))))))))), 8), zero);
        }
        else if constexpr (level == 4) {
          res = _mm_packus_epi16(_mm_srli_epi16(
            _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pSrc + x))), ws),
              _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefB[0] + x))), wb1),
                _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefF[0] + x))), wf1),
                  _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefB[1] + x))), wb2),
                    _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefF[1] + x))), wf2),
                      _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefB[2] + x))), wb3),
                        _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefF[2] + x))), wf3),
                          _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefB[3] + x))), wb4),
                            _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefF[3] + x))), wf4),
                              rounder))))))))), 8), zero);
        }
        else if constexpr (level == 3) {
          res = _mm_packus_epi16(_mm_srli_epi16(
            _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pSrc + x))), ws),
              _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefB[0] + x))), wb1),
                _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefF[0] + x))), wf1),
                  _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefB[1] + x))), wb2),
                    _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefF[1] + x))), wf2),
                      _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefB[2] + x))), wb3),
                        _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefF[2] + x))), wf3),
                          rounder))))))), 8), zero);
          // pDst[x] = (pRefF[x]*WRefF + pSrc[x]*WSrc + pRefB[x]*WRefB + pRefF2[x]*WRefF2 + pRefB2[x]*WRefB2 + pRefF3[x]*WRefF3 + pRefB3[x]*WRefB3 + 128)>>8;
        }
        else if constexpr (level == 2) {
          res = _mm_packus_epi16(_mm_srli_epi16(
            _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pSrc + x))), ws),
              _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefB[0] + x))), wb1),
                _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefF[0] + x))), wf1),
                  _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefB[1] + x))), wb2),
                    _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefF[1] + x))), wf2),
                      rounder))))), 8), zero);
          // pDst[x] = (pRefF[x]*WRefF + pSrc[x]*WSrc + pRefB[x]*WRefB + pRefF2[x]*WRefF2 + pRefB2[x]*WRefB2 + 128)>>8;
        }
        else if constexpr (level == 1) {
          res = _mm_packus_epi16(_mm_srli_epi16(
            _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pSrc + x))), ws),
              _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefB[0] + x))), wb1),
                _mm_add_epi16(_mm_mullo_epi16(_mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i*)(pRefF[0] + x))), wf1),
                  rounder))), 8), zero);
          // pDst[x] = (pRefF[x]*WRefF + pSrc[x]*WSrc + pRefB[x]*WRefB + 128)>>8;// weighted (by SAD) average
        }
        if constexpr (blockWidth == 12) {
          // 8 from the first, 4 from the second cycle
          if (x == 0) { // 1st 8 bytes
            _mm_storel_epi64((__m128i*)(pDst + x), res);
          }
          else { // 2nd 4 bytes
            *(uint32_t*)(pDst + x) = _mm_cvtsi128_si32(res);
          }
        }
        else if constexpr (blockWidth >= 8) {
          // 8, 16, 24, ....
          _mm_storel_epi64((__m128i*)(pDst + x), res);
        }
        else if constexpr (blockWidth == 6) {
          // x is always 0
          // 4+2 bytes
          *(uint32_t*)(pDst + x) = _mm_cvtsi128_si32(res);
          *(uint16_t*)(pDst + x + 4) = (uint16_t)_mm_cvtsi128_si32(_mm_srli_si128(res, 4));
        }
        else if constexpr (blockWidth == 4) {
          // x is always 0
          *(uint32_t*)(pDst + x) = _mm_cvtsi128_si32(res);
        }
        else if constexpr (blockWidth == 3) {
          // x is always 0
          // 2+1 bytes
          uint32_t res32 = _mm_cvtsi128_si32(res);
          *(uint16_t*)(pDst + x) = (uint16_t)res32;
          *(uint8_t*)(pDst + x + 2) = (uint8_t)(res32 >> 16);
        }
        else if constexpr (blockWidth == 2) {
          // x is always 0
          // 2 bytes
          uint32_t res32 = _mm_cvtsi128_si32(res);
          *(uint16_t*)(pDst + x) = (uint16_t)res32;
        }
      }
      pDst += nDstPitch;
      pSrc += nSrcPitch;

      pRefB[0] += BPitch[0];
      pRefF[0] += FPitch[0];
      if constexpr (level >= 2) {
        pRefB[1] += BPitch[1];
        pRefF[1] += FPitch[1];
        if constexpr (level >= 3) {
          pRefB[2] += BPitch[2];
          pRefF[2] += FPitch[2];
          if constexpr (level >= 4) {
            pRefB[3] += BPitch[3];
            pRefF[3] += FPitch[3];
            if constexpr (level >= 5) {
              pRefB[4] += BPitch[4];
              pRefF[4] += FPitch[4];
              if constexpr (level >= 6) {
                pRefB[5] += BPitch[5];
                pRefF[5] += FPitch[5];
              }
            }
          }
        }
      }
    }
  }
  _mm256_zeroupper();
}

// instantiate
#define MAKE_FN_LEVEL(x, y, level) \
template void Degrain1to6_avx2<x, y, 0, level>(BYTE*, BYTE*, int, int, const BYTE*, int, const BYTE* pRefB[MAX_DEGRAIN], int BPitch[MAX_DEGRAIN], const BYTE* pRefF[MAX_DEGRAIN], int FPitch[MAX_DEGRAIN], int, int WRefB[MAX_DEGRAIN], int WRefF[MAX_DEGRAIN]); \
template void Degrain1to6_avx2<x, y, 1, level>(BYTE*, BYTE*, int, int, const BYTE*, int, const BYTE* pRefB[MAX_DEGRAIN], int BPitch[MAX_DEGRAIN], const BYTE* pRefF[MAX_DEGRAIN], int FPitch[MAX_DEGRAIN], int, int WRefB[MAX_DEGRAIN], int WRefF[MAX_DEGRAIN]); \
template void Degrain1to6_avx2<x, y, 2, level>(BYTE*, BYTE*, int, int, const BYTE*, int, const BYTE* pRefB[MAX_DEGRAIN], int BPitch[MAX_DEGRAIN], const BYTE* pRefF[MAX_DEGRAIN], int FPitch[MAX_DEGRAIN], int, int WRefB[MAX_DEGRAIN], int WRefF[MAX_DEGRAIN]); \
template void Degrain1to6_avx2<x, y, 3, level>(BYTE*, BYTE*, int, int, const BYTE*, int, const BYTE* pRefB[MAX_DEGRAIN], int BPitch[MAX_DEGRAIN], const BYTE* pRefF[MAX_DEGRAIN], int FPitch[MAX_DEGRAIN], int, int WRefB[MAX_DEGRAIN], int WRefF[MAX_DEGRAIN]); \
/*func_degrain[make_tuple(x, y, DEGRAIN_TYPE_10to14BIT, level, USE_SSE41)] = Degrain1to6_16_sse41<x, yy, level, true>;*/ \
/*func_degrain[make_tuple(x, y, DEGRAIN_TYPE_16BIT, level, USE_SSE41)] = Degrain1to6_16_sse41<x, yy, level, false>;*/

// the very same list of get_denoise123_function

#define MAKE_FN(x, y) \
MAKE_FN_LEVEL(x,y,1) \
MAKE_FN_LEVEL(x,y,2) \
MAKE_FN_LEVEL(x,y,3) \
MAKE_FN_LEVEL(x,y,4) \
MAKE_FN_LEVEL(x,y,5) \
MAKE_FN_LEVEL(x,y,6)

MAKE_FN(64, 0)
//MAKE_FN(64, 64)
//MAKE_FN(64, 48)
//MAKE_FN(64, 32)
//MAKE_FN(64, 16)
MAKE_FN(48, 0)
//MAKE_FN(48, 64)
//MAKE_FN(48, 48)
//MAKE_FN(48, 24)
//MAKE_FN(48, 12)
MAKE_FN(32, 0)
//MAKE_FN(32, 64)
//MAKE_FN(32, 32)
//MAKE_FN(32, 24)
//MAKE_FN(32, 16)
//MAKE_FN(32, 8)
MAKE_FN(24, 0)
//MAKE_FN(24, 48)
//MAKE_FN(24, 32)
//MAKE_FN(24, 24)
//MAKE_FN(24, 12)
//MAKE_FN(24, 6)
MAKE_FN(16, 0)
//MAKE_FN(16, 64)
//MAKE_FN(16, 32)
//MAKE_FN(16, 16)
//MAKE_FN(16, 12)
//MAKE_FN(16, 8)
MAKE_FN(16, 4)
MAKE_FN(16, 2)
MAKE_FN(16, 1)
MAKE_FN(12, 0)
//MAKE_FN(12, 48)
//MAKE_FN(12, 24)
//MAKE_FN(12, 16)
//MAKE_FN(12, 12)
MAKE_FN(12, 6)
MAKE_FN(12, 3)
MAKE_FN(8, 0)
//MAKE_FN(8, 32)
//MAKE_FN(8, 16)
MAKE_FN(8, 8)
MAKE_FN(8, 4)
MAKE_FN(8, 2)
MAKE_FN(8, 1)
MAKE_FN(6, 0)
//MAKE_FN(6, 24)
//MAKE_FN(6, 12)
MAKE_FN(6, 6)
MAKE_FN(6, 3)
MAKE_FN(4, 8)
MAKE_FN(4, 4)
MAKE_FN(4, 2)
MAKE_FN(4, 1)
MAKE_FN(3, 6)
MAKE_FN(3, 3)
MAKE_FN(2, 4)
MAKE_FN(2, 2)
MAKE_FN(2, 1)
#undef MAKE_FN
#undef MAKE_FN_LEVEL



#pragma warning( pop ) 


