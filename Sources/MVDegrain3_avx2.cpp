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

#if defined (__GNUC__) && ! defined (__INTEL_COMPILER) && ! defined(__INTEL_LLVM_COMPILER)
#include <x86intrin.h>
// x86intrin.h includes header files for whatever instruction
// sets are specified on the compiler command line, such as: xopintrin.h, fma4intrin.h
#else
#include <immintrin.h> // MS version of immintrin.h covers AVX, AVX2 and FMA3
#endif // __GNUC__

#if !defined(__FMA__)
// Assume that all processors that have AVX2 also have FMA3
#if defined (__GNUC__) && ! defined (__INTEL_COMPILER) && ! defined(__INTEL_LLVM_COMPILER) && ! defined (__clang__)
// Prevent error message in g++ when using FMA intrinsics with avx2:
#pragma message "It is recommended to specify also option -mfma when using -mavx2 or higher"
#else
#define __FMA__  1
#endif
#endif
// FMA3 instruction set
#if defined (__FMA__) && (defined(__GNUC__) || defined(__clang__) || defined(__INTEL_LLVM_COMPILER))  && ! defined (__INTEL_COMPILER) 
#include <fmaintrin.h>
#endif // __FMA__ 

#ifndef _mm256_loadu2_m128i
#define _mm256_loadu2_m128i(/* __m128i const* */ hiaddr, \
                            /* __m128i const* */ loaddr) \
    _mm256_set_m128i(_mm_loadu_si128(hiaddr), _mm_loadu_si128(loaddr))
#endif

#include "MVDegrain3.h"
#include "MVDegrain3_avx2.h"

#include <stdint.h>
#include "def.h"

#pragma warning( push )
#pragma warning( disable : 4101)

// this one is slower than the plain sse2 implementation so it is disabled in function selector
// ymm register saving overhead??

// out16_type: 
//   0: native 8 or 16
//   1: 8bit in, lsb
//   2: 8bit in, native16 out
//   3: 8 bit in: float out (out32)
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

  __m256i wbf_256[level];

#if 0
  __m256i wbf1_256;
  __m256i wbf2_256;
  __m256i wbf3_256;
  __m256i wbf4_256;
  __m256i wbf5_256;
  __m256i wbf6_256;
#endif
  // interleave 0 and center weight
  ws_256 = _mm256_set1_epi16((0 << 16) + WSrc);

  // interleave F and B for madd
  for(int i = 0; i < level; i++)
    wbf_256[i] = _mm256_set1_epi32((WRefB[i] << 16) | WRefF[i]);

#if 0
  // interleave F and B for madd
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
#endif

  //  (lsb_flag || out16 || out32): 8 bit in 16 bits out, out32 will be converted to float
  //  else 8 bit in 8 bit out (but 16 bit intermediate)
  __m128i lsb_mask = _mm_set1_epi16(255);
  for (int h = 0; h < realBlockHeight; h++)
  {
    for (int x = 0; x < blockWidth; x += 8)
    {

        // lambda to protect overread. Read exact number of bytes for all blocksizes 
      auto getpixels_as_uint8_in_m128i = [&](const BYTE* p) {
        __m128i pixels;
        if constexpr (blockWidth == 2)
          pixels = _mm_cvtsi32_si128(*(reinterpret_cast<const uint16_t*>(p + x))); // 2 bytes
        else if constexpr (blockWidth == 3)
          pixels = _mm_cvtsi32_si128(*(reinterpret_cast<const uint16_t*>(p + x)) + (*(p + x + 2) << 16)); // 2+1 bytes
        else if constexpr (blockWidth == 4)
          pixels = _mm_cvtsi32_si128(*(reinterpret_cast<const uint32_t*>(p + x))); // 4 bytes
        else if constexpr (blockWidth == 6) // uint32 + uint16
          pixels = _mm_or_si128(
            _mm_cvtsi32_si128(*(reinterpret_cast<const uint32_t*>(p + x))), // 4 bytes
            _mm_slli_si128(_mm_cvtsi32_si128(*(reinterpret_cast<const uint16_t*>(p + x + 4))), 4) // 2 bytes
          );
        else if constexpr (blockWidth == 12) {
          if (x == 0)
            pixels = _mm_loadl_epi64((__m128i*)(p + x)); // 8 bytes in x==0 loop
          else // x == 1
            pixels = _mm_cvtsi32_si128(*(reinterpret_cast<const uint32_t*>(p + x))); // 4 bytes from 2nd loop (x==1)
        }
        else
          pixels = _mm_loadl_epi64((__m128i*)(p + x)); // 8 bytes

        return pixels;
      };

      __m128i val; // basic, lsb (stacked) and out16 operation
      __m256i val2; // out32 (float output) case

      auto src = getpixels_as_uint8_in_m128i(pSrc);
      // Interleave Src 0 Src 0 ...
      auto src_0 = _mm256_cvtepu8_epi32(src);
      val2 = _mm256_madd_epi16(src_0, ws_256);

      for (int i = 0; i < level; i++) {
        // Interleave SrcF SrcB
        auto f = getpixels_as_uint8_in_m128i(pRefF[i]);
        auto b = getpixels_as_uint8_in_m128i(pRefB[i]);
        auto fb = _mm256_cvtepu8_epi16(_mm_unpacklo_epi8(f, b));
        // madd: SrcF*WF + SrcB*WB
        val2 = _mm256_add_epi32(val2, _mm256_madd_epi16(fb, wbf_256[i]));
      }

      if (!out32) {
        // 256 -> 128
        __m256i result_2x4x_uint16 = _mm256_packus_epi32(val2, zero_256); // 8*32+zeros = lower 4*16 in both 128bit lanes
        __m128i result_2x4x_uint16_128 = _mm256_castsi256_si128(_mm256_permute4x64_epi64(result_2x4x_uint16, (0 << 0) | (2 << 2) | (0 << 4) | (0 << 6))); // low64 of 2nd 128bit lane to hi64 of 1st 128bit lane
        val = result_2x4x_uint16_128;
      }

      // storage lsb/out16/out32/8 bit
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
          }
          else { //if (x == 1) { // last 4 pixels 16 bytes
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
      else {
        // simpliest case: usual 8 bit to 8 bit
        // 8 bit in, 8 bit out
        __m128i rounder = _mm_set1_epi16(128);
        // pDst[x] = (pSrc[x]*WSrc + pRefF[x]*WRefF + pRefB[x]*WRefB + 128)>>8;// weighted (by SAD) average
        // pDst[x] = (pSrc[x]*WSrc + pRefF[x]*WRefF + pRefB[x]*WRefB + pRefF2[x]*WRefF2 + pRefB2[x]*WRefB2 + [...] 128)>>8;

        auto res = _mm_srli_epi16(_mm_add_epi16(val, rounder), 8);
        res = _mm_packus_epi16(res, zero);

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
      // end of all storage
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


