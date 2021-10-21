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

#include "PlaneOfBlocks.h"
#include <map>
#include <tuple>

#include <stdint.h>
#include "def.h"

void PlaneOfBlocks::ExhaustiveSearch8x8_uint8_sp4_avx2(WorkingArea& workarea, int mvx, int mvy)
{
  // debug check !
    // idea - may be not 4 checks are required - only upper left corner (starting addresses of buffer) and lower right (to not over-run atfer end of buffer - need check/test)
  if (!workarea.IsVectorOK(mvx - 4, mvy - 4))
  {
    return;
  }
  if (!workarea.IsVectorOK(mvx + 3, mvy + 3))
  {
    return;
  }
  /*
  if (!workarea.IsVectorOK(mvx - 4, mvy + 3))
  {
    return;
  }
  if (!workarea.IsVectorOK(mvx + 3, mvy - 4))
  {
    return;
  }
  */
  // array of sads 8x8
  // due to 256bit registers limit is actually -4..+3 H search around zero, but full -4 +4 V search
  unsigned short ArrSADs[8][9];
  const uint8_t* pucRef = GetRefBlock(workarea, mvx - 4, mvy - 4); // upper left corner
  const uint8_t* pucCurr = workarea.pSrc[0];

  __m128i xmm10_Src_01, xmm11_Src_23, xmm12_Src_45, xmm13_Src_67;

  __m256i ymm0_Ref_01, ymm1_Ref_23, ymm2_Ref_45, ymm3_Ref_67; // 2x16bytes store, require buf padding to allow 16bytes reads to xmm
  __m256i ymm10_Src_01, ymm11_Src_23, ymm12_Src_45, ymm13_Src_67;

  __m256i ymm4_tmp, ymm5_tmp, ymm6_tmp, ymm7_tmp;


  //        __m256i 	my_ymm_test = _mm256_set_epi8(31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0);// -debug values for check how permutes work
  //        __m256i 	my_ymm_test = _mm256_set_epi8(15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0);// -debug values for check how permutes work

  xmm10_Src_01 = _mm_loadu_si64((__m128i*)pucCurr);
  xmm10_Src_01 = _mm_unpacklo_epi64(xmm10_Src_01, _mm_loadu_si64((__m128i*)(pucCurr + nSrcPitch[0])));
  ymm10_Src_01 = _mm256_permute4x64_epi64(_mm256_castsi128_si256(xmm10_Src_01), 220);

  xmm11_Src_23 = _mm_loadu_si64((__m128i*)(pucCurr + nSrcPitch[0] * 2));
  xmm11_Src_23 = _mm_unpacklo_epi64(xmm11_Src_23, _mm_loadu_si64((__m128i*)(pucCurr + nSrcPitch[0] * 3)));
  ymm11_Src_23 = _mm256_permute4x64_epi64(_mm256_castsi128_si256(xmm11_Src_23), 220);

  xmm12_Src_45 = _mm_loadu_si64((__m128i*)(pucCurr + nSrcPitch[0] * 4));
  xmm12_Src_45 = _mm_unpacklo_epi64(xmm12_Src_45, _mm_loadu_si64((__m128i*)(pucCurr + nSrcPitch[0] * 5)));
  ymm12_Src_45 = _mm256_permute4x64_epi64(_mm256_castsi128_si256(xmm12_Src_45), 220);

  xmm13_Src_67 = _mm_loadu_si64((__m128i*)(pucCurr + nSrcPitch[0] * 6));
  xmm13_Src_67 = _mm_unpacklo_epi64(xmm13_Src_67, _mm_loadu_si64((__m128i*)(pucCurr + nSrcPitch[0] * 7)));
  ymm13_Src_67 = _mm256_permute4x64_epi64(_mm256_castsi128_si256(xmm13_Src_67), 220);
  // loaded 8 rows of 8x8 Src block, now in low and high 128bits of ymms

  for (int i = 0; i < 9; i++)
  {
    ymm0_Ref_01 = _mm256_loadu2_m128i((__m128i*)(pucRef + nRefPitch[0] * (i + 1)), (__m128i*)(pucRef + nRefPitch[0] * (i + 0)));
    ymm1_Ref_23 = _mm256_loadu2_m128i((__m128i*)(pucRef + nRefPitch[0] * (i + 3)), (__m128i*)(pucRef + nRefPitch[0] * (i + 2)));
    ymm2_Ref_45 = _mm256_loadu2_m128i((__m128i*)(pucRef + nRefPitch[0] * (i + 5)), (__m128i*)(pucRef + nRefPitch[0] * (i + 4)));
    ymm3_Ref_67 = _mm256_loadu2_m128i((__m128i*)(pucRef + nRefPitch[0] * (i + 7)), (__m128i*)(pucRef + nRefPitch[0] * (i + 6)));
    // loaded 8 rows of Ref plane 16samples wide into ymm0..ymm3
    _mm_prefetch(const_cast<const CHAR*>(reinterpret_cast<const CHAR*>(pucRef + nRefPitch[0] * (i + 8))), _MM_HINT_NTA); // prefetch next Ref row

    // process sad[-4,i-4]
    ymm4_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm0_Ref_01, 51);
    ymm5_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm1_Ref_23, 51);
    ymm6_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm2_Ref_45, 51);
    ymm7_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm3_Ref_67, 51);

    ymm4_tmp = _mm256_sad_epu8(ymm4_tmp, ymm10_Src_01);
    ymm5_tmp = _mm256_sad_epu8(ymm5_tmp, ymm11_Src_23);
    ymm6_tmp = _mm256_sad_epu8(ymm6_tmp, ymm12_Src_45);
    ymm7_tmp = _mm256_sad_epu8(ymm7_tmp, ymm13_Src_67);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm5_tmp);
    ymm6_tmp = _mm256_add_epi32(ymm6_tmp, ymm7_tmp);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm6_tmp);

    ymm5_tmp = _mm256_permute2f128_si256(ymm4_tmp, ymm4_tmp, 65);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm5_tmp);
    // sad -4,i-4 ready in low of mm4
    _mm_storeu_si16(&ArrSADs[0][i], _mm256_castsi256_si128(ymm4_tmp));

    // rotate Ref to 1 samples
    ymm0_Ref_01 = _mm256_alignr_epi8(ymm0_Ref_01, ymm0_Ref_01, 1);
    ymm1_Ref_23 = _mm256_alignr_epi8(ymm1_Ref_23, ymm1_Ref_23, 1);
    ymm2_Ref_45 = _mm256_alignr_epi8(ymm2_Ref_45, ymm2_Ref_45, 1);
    ymm3_Ref_67 = _mm256_alignr_epi8(ymm3_Ref_67, ymm3_Ref_67, 1);

    // process sad[-3,i-4]
    ymm4_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm0_Ref_01, 51);
    ymm5_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm1_Ref_23, 51);
    ymm6_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm2_Ref_45, 51);
    ymm7_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm3_Ref_67, 51);

    ymm4_tmp = _mm256_sad_epu8(ymm4_tmp, ymm10_Src_01);
    ymm5_tmp = _mm256_sad_epu8(ymm5_tmp, ymm11_Src_23);
    ymm6_tmp = _mm256_sad_epu8(ymm6_tmp, ymm12_Src_45);
    ymm7_tmp = _mm256_sad_epu8(ymm7_tmp, ymm13_Src_67);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm5_tmp);
    ymm6_tmp = _mm256_add_epi32(ymm6_tmp, ymm7_tmp);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm6_tmp);

    ymm5_tmp = _mm256_permute2f128_si256(ymm4_tmp, ymm4_tmp, 65);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm5_tmp);
    // sad -3,i-4 ready in low of mm4
    _mm_storeu_si16(&ArrSADs[1][i], _mm256_castsi256_si128(ymm4_tmp));

    // rotate Ref to 1 samples
    ymm0_Ref_01 = _mm256_alignr_epi8(ymm0_Ref_01, ymm0_Ref_01, 1);
    ymm1_Ref_23 = _mm256_alignr_epi8(ymm1_Ref_23, ymm1_Ref_23, 1);
    ymm2_Ref_45 = _mm256_alignr_epi8(ymm2_Ref_45, ymm2_Ref_45, 1);
    ymm3_Ref_67 = _mm256_alignr_epi8(ymm3_Ref_67, ymm3_Ref_67, 1);

    // process sad[-2,i-4]
    ymm4_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm0_Ref_01, 51);
    ymm5_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm1_Ref_23, 51);
    ymm6_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm2_Ref_45, 51);
    ymm7_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm3_Ref_67, 51);

    ymm4_tmp = _mm256_sad_epu8(ymm4_tmp, ymm10_Src_01);
    ymm5_tmp = _mm256_sad_epu8(ymm5_tmp, ymm11_Src_23);
    ymm6_tmp = _mm256_sad_epu8(ymm6_tmp, ymm12_Src_45);
    ymm7_tmp = _mm256_sad_epu8(ymm7_tmp, ymm13_Src_67);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm5_tmp);
    ymm6_tmp = _mm256_add_epi32(ymm6_tmp, ymm7_tmp);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm6_tmp);

    ymm5_tmp = _mm256_permute2f128_si256(ymm4_tmp, ymm4_tmp, 65);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm5_tmp);
    // sad -2,i-2 ready in low of mm4
    _mm_storeu_si16(&ArrSADs[2][i], _mm256_castsi256_si128(ymm4_tmp));

    // rotate Ref to 1 samples
    ymm0_Ref_01 = _mm256_alignr_epi8(ymm0_Ref_01, ymm0_Ref_01, 1);
    ymm1_Ref_23 = _mm256_alignr_epi8(ymm1_Ref_23, ymm1_Ref_23, 1);
    ymm2_Ref_45 = _mm256_alignr_epi8(ymm2_Ref_45, ymm2_Ref_45, 1);
    ymm3_Ref_67 = _mm256_alignr_epi8(ymm3_Ref_67, ymm3_Ref_67, 1);

    // process sad[-1,i-4]
    ymm4_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm0_Ref_01, 51);
    ymm5_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm1_Ref_23, 51);
    ymm6_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm2_Ref_45, 51);
    ymm7_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm3_Ref_67, 51);

    ymm4_tmp = _mm256_sad_epu8(ymm4_tmp, ymm10_Src_01);
    ymm5_tmp = _mm256_sad_epu8(ymm5_tmp, ymm11_Src_23);
    ymm6_tmp = _mm256_sad_epu8(ymm6_tmp, ymm12_Src_45);
    ymm7_tmp = _mm256_sad_epu8(ymm7_tmp, ymm13_Src_67);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm5_tmp);
    ymm6_tmp = _mm256_add_epi32(ymm6_tmp, ymm7_tmp);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm6_tmp);

    ymm5_tmp = _mm256_permute2f128_si256(ymm4_tmp, ymm4_tmp, 65);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm5_tmp);
    // sad -1,i-4 ready in low of mm4
    _mm_storeu_si16(&ArrSADs[3][i], _mm256_castsi256_si128(ymm4_tmp));

    // rotate Ref to 1 samples
    ymm0_Ref_01 = _mm256_alignr_epi8(ymm0_Ref_01, ymm0_Ref_01, 1);
    ymm1_Ref_23 = _mm256_alignr_epi8(ymm1_Ref_23, ymm1_Ref_23, 1);
    ymm2_Ref_45 = _mm256_alignr_epi8(ymm2_Ref_45, ymm2_Ref_45, 1);
    ymm3_Ref_67 = _mm256_alignr_epi8(ymm3_Ref_67, ymm3_Ref_67, 1);

    // process sad[0,i-4]
    ymm4_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm0_Ref_01, 51);
    ymm5_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm1_Ref_23, 51);
    ymm6_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm2_Ref_45, 51);
    ymm7_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm3_Ref_67, 51);

    ymm4_tmp = _mm256_sad_epu8(ymm4_tmp, ymm10_Src_01);
    ymm5_tmp = _mm256_sad_epu8(ymm5_tmp, ymm11_Src_23);
    ymm6_tmp = _mm256_sad_epu8(ymm6_tmp, ymm12_Src_45);
    ymm7_tmp = _mm256_sad_epu8(ymm7_tmp, ymm13_Src_67);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm5_tmp);
    ymm6_tmp = _mm256_add_epi32(ymm6_tmp, ymm7_tmp);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm6_tmp);

    ymm5_tmp = _mm256_permute2f128_si256(ymm4_tmp, ymm4_tmp, 65);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm5_tmp);
    // sad 0,i-4 ready in low of mm4
    _mm_storeu_si16(&ArrSADs[4][i], _mm256_castsi256_si128(ymm4_tmp));

    // rotate Ref to 1 samples
    ymm0_Ref_01 = _mm256_alignr_epi8(ymm0_Ref_01, ymm0_Ref_01, 1);
    ymm1_Ref_23 = _mm256_alignr_epi8(ymm1_Ref_23, ymm1_Ref_23, 1);
    ymm2_Ref_45 = _mm256_alignr_epi8(ymm2_Ref_45, ymm2_Ref_45, 1);
    ymm3_Ref_67 = _mm256_alignr_epi8(ymm3_Ref_67, ymm3_Ref_67, 1);

    // process sad[1,i-4]
    ymm4_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm0_Ref_01, 51);
    ymm5_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm1_Ref_23, 51);
    ymm6_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm2_Ref_45, 51);
    ymm7_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm3_Ref_67, 51);

    ymm4_tmp = _mm256_sad_epu8(ymm4_tmp, ymm10_Src_01);
    ymm5_tmp = _mm256_sad_epu8(ymm5_tmp, ymm11_Src_23);
    ymm6_tmp = _mm256_sad_epu8(ymm6_tmp, ymm12_Src_45);
    ymm7_tmp = _mm256_sad_epu8(ymm7_tmp, ymm13_Src_67);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm5_tmp);
    ymm6_tmp = _mm256_add_epi32(ymm6_tmp, ymm7_tmp);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm6_tmp);

    ymm5_tmp = _mm256_permute2f128_si256(ymm4_tmp, ymm4_tmp, 65);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm5_tmp);
    // sad 1,i-4 ready in low of mm4
    _mm_storeu_si16(&ArrSADs[5][i], _mm256_castsi256_si128(ymm4_tmp));

    // rotate Ref to 1 samples
    ymm0_Ref_01 = _mm256_alignr_epi8(ymm0_Ref_01, ymm0_Ref_01, 1);
    ymm1_Ref_23 = _mm256_alignr_epi8(ymm1_Ref_23, ymm1_Ref_23, 1);
    ymm2_Ref_45 = _mm256_alignr_epi8(ymm2_Ref_45, ymm2_Ref_45, 1);
    ymm3_Ref_67 = _mm256_alignr_epi8(ymm3_Ref_67, ymm3_Ref_67, 1);

    // process sad[2,i-4]
    ymm4_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm0_Ref_01, 51);
    ymm5_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm1_Ref_23, 51);
    ymm6_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm2_Ref_45, 51);
    ymm7_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm3_Ref_67, 51);

    ymm4_tmp = _mm256_sad_epu8(ymm4_tmp, ymm10_Src_01);
    ymm5_tmp = _mm256_sad_epu8(ymm5_tmp, ymm11_Src_23);
    ymm6_tmp = _mm256_sad_epu8(ymm6_tmp, ymm12_Src_45);
    ymm7_tmp = _mm256_sad_epu8(ymm7_tmp, ymm13_Src_67);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm5_tmp);
    ymm6_tmp = _mm256_add_epi32(ymm6_tmp, ymm7_tmp);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm6_tmp);

    ymm5_tmp = _mm256_permute2f128_si256(ymm4_tmp, ymm4_tmp, 65);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm5_tmp);
    // sad 2,i-4 ready in low of mm4
    _mm_storeu_si16(&ArrSADs[6][i], _mm256_castsi256_si128(ymm4_tmp));

    // rotate Ref to 1 samples
    ymm0_Ref_01 = _mm256_alignr_epi8(ymm0_Ref_01, ymm0_Ref_01, 1);
    ymm1_Ref_23 = _mm256_alignr_epi8(ymm1_Ref_23, ymm1_Ref_23, 1);
    ymm2_Ref_45 = _mm256_alignr_epi8(ymm2_Ref_45, ymm2_Ref_45, 1);
    ymm3_Ref_67 = _mm256_alignr_epi8(ymm3_Ref_67, ymm3_Ref_67, 1);

    // process sad[3,i-4]
    ymm4_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm0_Ref_01, 51);
    ymm5_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm1_Ref_23, 51);
    ymm6_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm2_Ref_45, 51);
    ymm7_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm3_Ref_67, 51);

    ymm4_tmp = _mm256_sad_epu8(ymm4_tmp, ymm10_Src_01);
    ymm5_tmp = _mm256_sad_epu8(ymm5_tmp, ymm11_Src_23);
    ymm6_tmp = _mm256_sad_epu8(ymm6_tmp, ymm12_Src_45);
    ymm7_tmp = _mm256_sad_epu8(ymm7_tmp, ymm13_Src_67);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm5_tmp);
    ymm6_tmp = _mm256_add_epi32(ymm6_tmp, ymm7_tmp);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm6_tmp);

    ymm5_tmp = _mm256_permute2f128_si256(ymm4_tmp, ymm4_tmp, 65);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm5_tmp);
    // sad 3,i-4 ready in low of mm4
    _mm_storeu_si16(&ArrSADs[7][i], _mm256_castsi256_si128(ymm4_tmp));
  }

  unsigned short minsad = 65535;
  int x_minsad = 0;
  int y_minsad = 0;
  for (int x = -4; x < 4; x++)
  {
    for (int y = -4; y < 5; y++)
    {
      if (ArrSADs[x + 4][y + 4] < minsad)
      {
        minsad = ArrSADs[x + 4][y + 4];
        x_minsad = x;
        y_minsad = y;
      }
    }
  }

  sad_t cost = minsad + ((penaltyNew * minsad) >> 8);
  if (cost >= workarea.nMinCost) return;

  workarea.bestMV.x = mvx + x_minsad;
  workarea.bestMV.y = mvy + y_minsad;
  workarea.nMinCost = cost;
  workarea.bestMV.sad = minsad;

  _mm256_zeroupper();
}

#if 0
// DTL: wrong way (speedwise)
void PlaneOfBlocks::ExhaustiveSearch8x8_uint8_sp4_avx2_2(WorkingArea& workarea, int mvx, int mvy)
{
  // debug check !
  // idea - may be not 4 checks are required - only upper left corner (starting addresses of buffer) and lower right (to not over-run atfer end of buffer - need check/test)
  if (!workarea.IsVectorOK(mvx - 4, mvy - 4))
  {
    return;
  }
  if (!workarea.IsVectorOK(mvx + 3, mvy + 3))
  {
    return;
  }
  /*	if (!workarea.IsVectorOK(mvx - 4, mvy + 3))
    {
      return;
    }
    if (!workarea.IsVectorOK(mvx + 3, mvy - 4))
    {
      return;
    }
    */ // need to check if it is non needed cheks

    // due to 256bit registers limit is actually -4..+3 H search around zero, but full -4 +4 V search
  alignas(256) unsigned short SIMD256Res[16];

  const uint8_t* pucRef = GetRefBlock(workarea, mvx - 4, mvy - 4); // upper left corner
  const uint8_t* pucCurr = workarea.pSrc[0];

  __m128i xmm10_Src_01, xmm11_Src_23, xmm12_Src_45, xmm13_Src_67;

  __m256i ymm0_Ref_01, ymm1_Ref_23, ymm2_Ref_45, ymm3_Ref_67; // 2x12bytes store, require buf padding to allow 16bytes reads to xmm
  __m256i ymm4_tmp, ymm5_tmp, ymm6_tmp, ymm7_tmp;
  __m256i ymm8_x1 = _mm256_set_epi16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0);
  __m256i ymm9_y1 = _mm256_set_epi16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0);
  __m256i ymm10_Src_01, ymm11_Src_23, ymm12_Src_45, ymm13_Src_67;
  __m256i ymm14_yx_minsad = _mm256_set_epi32(0, 0, 0, 0, 0, 0, 0, 1073741824); // 2^30 large signed 32bit
  __m256i ymm15_yx_cnt = _mm256_setzero_si256(); // y,x coords of search count from 0

  //        __m256i 	my_ymm_test = _mm256_set_epi8(31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0);// -debug values for check how permutes work
  //        __m256i 	my_ymm_test = _mm256_set_epi8(15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0);// -debug values for check how permutes work
  //        my_ymm_test = _mm256_alignr_epi8(my_ymm_test, my_ymm_test, 2); // rotate each half of ymm by number of bytes

  xmm10_Src_01 = _mm_loadu_si64((__m128i*)pucCurr);
  xmm10_Src_01 = _mm_unpacklo_epi64(xmm10_Src_01, _mm_loadu_si64((__m128i*)(pucCurr + nSrcPitch[0])));
  ymm10_Src_01 = _mm256_permute4x64_epi64(_mm256_castsi128_si256(xmm10_Src_01), 220);

  xmm11_Src_23 = _mm_loadu_si64((__m128i*)(pucCurr + nSrcPitch[0] * 2));
  xmm11_Src_23 = _mm_unpacklo_epi64(xmm11_Src_23, _mm_loadu_si64((__m128i*)(pucCurr + nSrcPitch[0] * 3)));
  ymm11_Src_23 = _mm256_permute4x64_epi64(_mm256_castsi128_si256(xmm11_Src_23), 220);

  xmm12_Src_45 = _mm_loadu_si64((__m128i*)(pucCurr + nSrcPitch[0] * 4));
  xmm12_Src_45 = _mm_unpacklo_epi64(xmm12_Src_45, _mm_loadu_si64((__m128i*)(pucCurr + nSrcPitch[0] * 5)));
  ymm12_Src_45 = _mm256_permute4x64_epi64(_mm256_castsi128_si256(xmm12_Src_45), 220);

  xmm13_Src_67 = _mm_loadu_si64((__m128i*)(pucCurr + nSrcPitch[0] * 6));
  xmm13_Src_67 = _mm_unpacklo_epi64(xmm13_Src_67, _mm_loadu_si64((__m128i*)(pucCurr + nSrcPitch[0] * 7)));
  ymm13_Src_67 = _mm256_permute4x64_epi64(_mm256_castsi128_si256(xmm13_Src_67), 220);
  // loaded 8 rows of 8x8 Src block, now in low and high 128bits of ymms

  for (int i = 0; i < 9; i++)
  {
    ymm0_Ref_01 = _mm256_loadu2_m128i((__m128i*)(pucRef + nRefPitch[0] * (i + 1)), (__m128i*)(pucRef + nRefPitch[0] * (i + 0)));
    ymm1_Ref_23 = _mm256_loadu2_m128i((__m128i*)(pucRef + nRefPitch[0] * (i + 3)), (__m128i*)(pucRef + nRefPitch[0] * (i + 2)));
    ymm2_Ref_45 = _mm256_loadu2_m128i((__m128i*)(pucRef + nRefPitch[0] * (i + 5)), (__m128i*)(pucRef + nRefPitch[0] * (i + 4)));
    ymm3_Ref_67 = _mm256_loadu2_m128i((__m128i*)(pucRef + nRefPitch[0] * (i + 7)), (__m128i*)(pucRef + nRefPitch[0] * (i + 6)));
    // loaded 8 rows of Ref plane 16samples wide into ymm0..ymm3

    // process sad[-4,i-4]
    ymm4_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm0_Ref_01, 51);
    ymm5_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm1_Ref_23, 51);
    ymm6_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm2_Ref_45, 51);
    ymm7_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm3_Ref_67, 51);

    ymm4_tmp = _mm256_sad_epu8(ymm4_tmp, ymm10_Src_01);
    ymm5_tmp = _mm256_sad_epu8(ymm5_tmp, ymm11_Src_23);
    ymm6_tmp = _mm256_sad_epu8(ymm6_tmp, ymm12_Src_45);
    ymm7_tmp = _mm256_sad_epu8(ymm7_tmp, ymm13_Src_67);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm5_tmp);
    ymm6_tmp = _mm256_add_epi32(ymm6_tmp, ymm7_tmp);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm6_tmp);

    ymm5_tmp = _mm256_permute2f128_si256(ymm4_tmp, ymm4_tmp, 65);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm5_tmp);
    // sad -4,i-4 ready in low of ymm4
    //check if min and replace in ymm14_yx_minsad
    ymm5_tmp = _mm256_cmpgt_epi32(ymm14_yx_minsad, ymm4_tmp); // mask in 0 16bit word for update minsad if needed
    ymm5_tmp = _mm256_slli_epi64(ymm5_tmp, 32);// clear higher bits of mask - may be better method exist
    ymm5_tmp = _mm256_srli_epi64(ymm5_tmp, 32);// clear higher bits of mask
    ymm14_yx_minsad = _mm256_blendv_epi8(ymm14_yx_minsad, ymm4_tmp, ymm5_tmp); // blend minsad if not greater than
    ymm5_tmp = _mm256_shufflelo_epi16(ymm5_tmp, 10); // move/broadcast mask from 0 to 2 and 3 16bit word for update y,x if needed
    ymm14_yx_minsad = _mm256_blendv_epi8(ymm14_yx_minsad, ymm15_yx_cnt, ymm5_tmp); // blend y,x of minsad if not greater than

    ymm15_yx_cnt = _mm256_add_epi16(ymm15_yx_cnt, ymm8_x1);//increment x coord

    // rotate Ref to 1 samples
    ymm0_Ref_01 = _mm256_alignr_epi8(ymm0_Ref_01, ymm0_Ref_01, 1);
    ymm1_Ref_23 = _mm256_alignr_epi8(ymm1_Ref_23, ymm1_Ref_23, 1);
    ymm2_Ref_45 = _mm256_alignr_epi8(ymm2_Ref_45, ymm2_Ref_45, 1);
    ymm3_Ref_67 = _mm256_alignr_epi8(ymm3_Ref_67, ymm3_Ref_67, 1);

    // process sad[-3,i-4]
    ymm4_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm0_Ref_01, 51);
    ymm5_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm1_Ref_23, 51);
    ymm6_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm2_Ref_45, 51);
    ymm7_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm3_Ref_67, 51);

    ymm4_tmp = _mm256_sad_epu8(ymm4_tmp, ymm10_Src_01);
    ymm5_tmp = _mm256_sad_epu8(ymm5_tmp, ymm11_Src_23);
    ymm6_tmp = _mm256_sad_epu8(ymm6_tmp, ymm12_Src_45);
    ymm7_tmp = _mm256_sad_epu8(ymm7_tmp, ymm13_Src_67);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm5_tmp);
    ymm6_tmp = _mm256_add_epi32(ymm6_tmp, ymm7_tmp);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm6_tmp);

    ymm5_tmp = _mm256_permute2f128_si256(ymm4_tmp, ymm4_tmp, 65);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm5_tmp);
    // sad -3,i-4 ready in low of ymm4
    //check if min and replace in ymm14_yx_minsad
    ymm5_tmp = _mm256_cmpgt_epi32(ymm14_yx_minsad, ymm4_tmp); // mask in 0 16bit word for update minsad if needed
    ymm5_tmp = _mm256_slli_epi64(ymm5_tmp, 32);// clear higher bits of mask - may be better method exist
    ymm5_tmp = _mm256_srli_epi64(ymm5_tmp, 32);// clear higher bits of mask
    ymm14_yx_minsad = _mm256_blendv_epi8(ymm14_yx_minsad, ymm4_tmp, ymm5_tmp); // blend minsad if not greater than
    ymm5_tmp = _mm256_shufflelo_epi16(ymm5_tmp, 10); // move/broadcast mask from 0 to 2 and 3 16bit word for update y,x if needed
    ymm14_yx_minsad = _mm256_blendv_epi8(ymm14_yx_minsad, ymm15_yx_cnt, ymm5_tmp); // blend y,x of minsad if not greater than

    ymm15_yx_cnt = _mm256_add_epi16(ymm15_yx_cnt, ymm8_x1);//increment x coord

    // rotate Ref to 1 samples
    ymm0_Ref_01 = _mm256_alignr_epi8(ymm0_Ref_01, ymm0_Ref_01, 1);
    ymm1_Ref_23 = _mm256_alignr_epi8(ymm1_Ref_23, ymm1_Ref_23, 1);
    ymm2_Ref_45 = _mm256_alignr_epi8(ymm2_Ref_45, ymm2_Ref_45, 1);
    ymm3_Ref_67 = _mm256_alignr_epi8(ymm3_Ref_67, ymm3_Ref_67, 1);

    // process sad[-2,i-4]
    ymm4_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm0_Ref_01, 51);
    ymm5_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm1_Ref_23, 51);
    ymm6_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm2_Ref_45, 51);
    ymm7_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm3_Ref_67, 51);

    ymm4_tmp = _mm256_sad_epu8(ymm4_tmp, ymm10_Src_01);
    ymm5_tmp = _mm256_sad_epu8(ymm5_tmp, ymm11_Src_23);
    ymm6_tmp = _mm256_sad_epu8(ymm6_tmp, ymm12_Src_45);
    ymm7_tmp = _mm256_sad_epu8(ymm7_tmp, ymm13_Src_67);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm5_tmp);
    ymm6_tmp = _mm256_add_epi32(ymm6_tmp, ymm7_tmp);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm6_tmp);

    ymm5_tmp = _mm256_permute2f128_si256(ymm4_tmp, ymm4_tmp, 65);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm5_tmp);
    // sad -2,i-4 ready in low of ymm4
    //check if min and replace in ymm14_yx_minsad
    ymm5_tmp = _mm256_cmpgt_epi32(ymm14_yx_minsad, ymm4_tmp); // mask in 0 16bit word for update minsad if needed
    ymm5_tmp = _mm256_slli_epi64(ymm5_tmp, 32);// clear higher bits of mask - may be better method exist
    ymm5_tmp = _mm256_srli_epi64(ymm5_tmp, 32);// clear higher bits of mask
    ymm14_yx_minsad = _mm256_blendv_epi8(ymm14_yx_minsad, ymm4_tmp, ymm5_tmp); // blend minsad if not greater than
    ymm5_tmp = _mm256_shufflelo_epi16(ymm5_tmp, 10); // move/broadcast mask from 0 to 2 and 3 16bit word for update y,x if needed
    ymm14_yx_minsad = _mm256_blendv_epi8(ymm14_yx_minsad, ymm15_yx_cnt, ymm5_tmp); // blend y,x of minsad if not greater than

    ymm15_yx_cnt = _mm256_add_epi16(ymm15_yx_cnt, ymm8_x1);//increment x coord

    // rotate Ref to 1 samples
    ymm0_Ref_01 = _mm256_alignr_epi8(ymm0_Ref_01, ymm0_Ref_01, 1);
    ymm1_Ref_23 = _mm256_alignr_epi8(ymm1_Ref_23, ymm1_Ref_23, 1);
    ymm2_Ref_45 = _mm256_alignr_epi8(ymm2_Ref_45, ymm2_Ref_45, 1);
    ymm3_Ref_67 = _mm256_alignr_epi8(ymm3_Ref_67, ymm3_Ref_67, 1);

    // process sad[-1,i-4]
    ymm4_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm0_Ref_01, 51);
    ymm5_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm1_Ref_23, 51);
    ymm6_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm2_Ref_45, 51);
    ymm7_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm3_Ref_67, 51);

    ymm4_tmp = _mm256_sad_epu8(ymm4_tmp, ymm10_Src_01);
    ymm5_tmp = _mm256_sad_epu8(ymm5_tmp, ymm11_Src_23);
    ymm6_tmp = _mm256_sad_epu8(ymm6_tmp, ymm12_Src_45);
    ymm7_tmp = _mm256_sad_epu8(ymm7_tmp, ymm13_Src_67);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm5_tmp);
    ymm6_tmp = _mm256_add_epi32(ymm6_tmp, ymm7_tmp);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm6_tmp);

    ymm5_tmp = _mm256_permute2f128_si256(ymm4_tmp, ymm4_tmp, 65);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm5_tmp);
    // sad -1,i-4 ready in low of mm4
    //check if min and replace in ymm14_yx_minsad
    ymm5_tmp = _mm256_cmpgt_epi32(ymm14_yx_minsad, ymm4_tmp); // mask in 0 16bit word for update minsad if needed
    ymm5_tmp = _mm256_slli_epi64(ymm5_tmp, 32);// clear higher bits of mask - may be better method exist
    ymm5_tmp = _mm256_srli_epi64(ymm5_tmp, 32);// clear higher bits of mask
    ymm14_yx_minsad = _mm256_blendv_epi8(ymm14_yx_minsad, ymm4_tmp, ymm5_tmp); // blend minsad if not greater than
    ymm5_tmp = _mm256_shufflelo_epi16(ymm5_tmp, 10); // move/broadcast mask from 0 to 2 and 3 16bit word for update y,x if needed
    ymm14_yx_minsad = _mm256_blendv_epi8(ymm14_yx_minsad, ymm15_yx_cnt, ymm5_tmp); // blend y,x of minsad if not greater than

    ymm15_yx_cnt = _mm256_add_epi16(ymm15_yx_cnt, ymm8_x1);//increment x coord

    // rotate Ref to 1 samples
    ymm0_Ref_01 = _mm256_alignr_epi8(ymm0_Ref_01, ymm0_Ref_01, 1);
    ymm1_Ref_23 = _mm256_alignr_epi8(ymm1_Ref_23, ymm1_Ref_23, 1);
    ymm2_Ref_45 = _mm256_alignr_epi8(ymm2_Ref_45, ymm2_Ref_45, 1);
    ymm3_Ref_67 = _mm256_alignr_epi8(ymm3_Ref_67, ymm3_Ref_67, 1);

    // process sad[0,i-4]
    ymm4_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm0_Ref_01, 51);
    ymm5_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm1_Ref_23, 51);
    ymm6_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm2_Ref_45, 51);
    ymm7_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm3_Ref_67, 51);

    ymm4_tmp = _mm256_sad_epu8(ymm4_tmp, ymm10_Src_01);
    ymm5_tmp = _mm256_sad_epu8(ymm5_tmp, ymm11_Src_23);
    ymm6_tmp = _mm256_sad_epu8(ymm6_tmp, ymm12_Src_45);
    ymm7_tmp = _mm256_sad_epu8(ymm7_tmp, ymm13_Src_67);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm5_tmp);
    ymm6_tmp = _mm256_add_epi32(ymm6_tmp, ymm7_tmp);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm6_tmp);

    ymm5_tmp = _mm256_permute2f128_si256(ymm4_tmp, ymm4_tmp, 65);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm5_tmp);
    // sad 0,i-4 ready in low of mm4
    //check if min and replace in ymm14_yx_minsad
    ymm5_tmp = _mm256_cmpgt_epi32(ymm14_yx_minsad, ymm4_tmp); // mask in 0 16bit word for update minsad if needed
    ymm5_tmp = _mm256_slli_epi64(ymm5_tmp, 32);// clear higher bits of mask - may be better method exist
    ymm5_tmp = _mm256_srli_epi64(ymm5_tmp, 32);// clear higher bits of mask
    ymm14_yx_minsad = _mm256_blendv_epi8(ymm14_yx_minsad, ymm4_tmp, ymm5_tmp); // blend minsad if not greater than
    ymm5_tmp = _mm256_shufflelo_epi16(ymm5_tmp, 10); // move/broadcast mask from 0 to 2 and 3 16bit word for update y,x if needed
    ymm14_yx_minsad = _mm256_blendv_epi8(ymm14_yx_minsad, ymm15_yx_cnt, ymm5_tmp); // blend y,x of minsad if not greater than

    ymm15_yx_cnt = _mm256_add_epi16(ymm15_yx_cnt, ymm8_x1);//increment x coord

    // rotate Ref to 1 samples
    ymm0_Ref_01 = _mm256_alignr_epi8(ymm0_Ref_01, ymm0_Ref_01, 1);
    ymm1_Ref_23 = _mm256_alignr_epi8(ymm1_Ref_23, ymm1_Ref_23, 1);
    ymm2_Ref_45 = _mm256_alignr_epi8(ymm2_Ref_45, ymm2_Ref_45, 1);
    ymm3_Ref_67 = _mm256_alignr_epi8(ymm3_Ref_67, ymm3_Ref_67, 1);

    // process sad[1,i-4]
    ymm4_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm0_Ref_01, 51);
    ymm5_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm1_Ref_23, 51);
    ymm6_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm2_Ref_45, 51);
    ymm7_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm3_Ref_67, 51);

    ymm4_tmp = _mm256_sad_epu8(ymm4_tmp, ymm10_Src_01);
    ymm5_tmp = _mm256_sad_epu8(ymm5_tmp, ymm11_Src_23);
    ymm6_tmp = _mm256_sad_epu8(ymm6_tmp, ymm12_Src_45);
    ymm7_tmp = _mm256_sad_epu8(ymm7_tmp, ymm13_Src_67);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm5_tmp);
    ymm6_tmp = _mm256_add_epi32(ymm6_tmp, ymm7_tmp);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm6_tmp);

    ymm5_tmp = _mm256_permute2f128_si256(ymm4_tmp, ymm4_tmp, 65);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm5_tmp);
    // sad 1,i-4 ready in low of mm4
    //check if min and replace in ymm14_yx_minsad
    ymm5_tmp = _mm256_cmpgt_epi32(ymm14_yx_minsad, ymm4_tmp); // mask in 0 16bit word for update minsad if needed
    ymm5_tmp = _mm256_slli_epi64(ymm5_tmp, 32);// clear higher bits of mask - may be better method exist
    ymm5_tmp = _mm256_srli_epi64(ymm5_tmp, 32);// clear higher bits of mask
    ymm14_yx_minsad = _mm256_blendv_epi8(ymm14_yx_minsad, ymm4_tmp, ymm5_tmp); // blend minsad if not greater than
    ymm5_tmp = _mm256_shufflelo_epi16(ymm5_tmp, 10); // move/broadcast mask from 0 to 2 and 3 16bit word for update y,x if needed
    ymm14_yx_minsad = _mm256_blendv_epi8(ymm14_yx_minsad, ymm15_yx_cnt, ymm5_tmp); // blend y,x of minsad if not greater than


    ymm15_yx_cnt = _mm256_add_epi16(ymm15_yx_cnt, ymm8_x1);//increment x coord

    // rotate Ref to 1 samples
    ymm0_Ref_01 = _mm256_alignr_epi8(ymm0_Ref_01, ymm0_Ref_01, 1);
    ymm1_Ref_23 = _mm256_alignr_epi8(ymm1_Ref_23, ymm1_Ref_23, 1);
    ymm2_Ref_45 = _mm256_alignr_epi8(ymm2_Ref_45, ymm2_Ref_45, 1);
    ymm3_Ref_67 = _mm256_alignr_epi8(ymm3_Ref_67, ymm3_Ref_67, 1);

    // process sad[2,i-4]
    ymm4_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm0_Ref_01, 51);
    ymm5_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm1_Ref_23, 51);
    ymm6_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm2_Ref_45, 51);
    ymm7_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm3_Ref_67, 51);

    ymm4_tmp = _mm256_sad_epu8(ymm4_tmp, ymm10_Src_01);
    ymm5_tmp = _mm256_sad_epu8(ymm5_tmp, ymm11_Src_23);
    ymm6_tmp = _mm256_sad_epu8(ymm6_tmp, ymm12_Src_45);
    ymm7_tmp = _mm256_sad_epu8(ymm7_tmp, ymm13_Src_67);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm5_tmp);
    ymm6_tmp = _mm256_add_epi32(ymm6_tmp, ymm7_tmp);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm6_tmp);

    ymm5_tmp = _mm256_permute2f128_si256(ymm4_tmp, ymm4_tmp, 65);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm5_tmp);
    // sad 2,i-4 ready in low of mm4
    //check if min and replace in ymm14_yx_minsad
    ymm5_tmp = _mm256_cmpgt_epi32(ymm14_yx_minsad, ymm4_tmp); // mask in 0 16bit word for update minsad if needed
    ymm5_tmp = _mm256_slli_epi64(ymm5_tmp, 32);// clear higher bits of mask - may be better method exist
    ymm5_tmp = _mm256_srli_epi64(ymm5_tmp, 32);// clear higher bits of mask
    ymm14_yx_minsad = _mm256_blendv_epi8(ymm14_yx_minsad, ymm4_tmp, ymm5_tmp); // blend minsad if not greater than
    ymm5_tmp = _mm256_shufflelo_epi16(ymm5_tmp, 10); // move/broadcast mask from 0 to 2 and 3 16bit word for update y,x if needed
    ymm14_yx_minsad = _mm256_blendv_epi8(ymm14_yx_minsad, ymm15_yx_cnt, ymm5_tmp); // blend y,x of minsad if not greater than

    ymm15_yx_cnt = _mm256_add_epi16(ymm15_yx_cnt, ymm8_x1);//increment x coord

    // rotate Ref to 1 samples
    ymm0_Ref_01 = _mm256_alignr_epi8(ymm0_Ref_01, ymm0_Ref_01, 1);
    ymm1_Ref_23 = _mm256_alignr_epi8(ymm1_Ref_23, ymm1_Ref_23, 1);
    ymm2_Ref_45 = _mm256_alignr_epi8(ymm2_Ref_45, ymm2_Ref_45, 1);
    ymm3_Ref_67 = _mm256_alignr_epi8(ymm3_Ref_67, ymm3_Ref_67, 1);

    // process sad[3,i-4]
    ymm4_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm0_Ref_01, 51);
    ymm5_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm1_Ref_23, 51);
    ymm6_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm2_Ref_45, 51);
    ymm7_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm3_Ref_67, 51);

    ymm4_tmp = _mm256_sad_epu8(ymm4_tmp, ymm10_Src_01);
    ymm5_tmp = _mm256_sad_epu8(ymm5_tmp, ymm11_Src_23);
    ymm6_tmp = _mm256_sad_epu8(ymm6_tmp, ymm12_Src_45);
    ymm7_tmp = _mm256_sad_epu8(ymm7_tmp, ymm13_Src_67);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm5_tmp);
    ymm6_tmp = _mm256_add_epi32(ymm6_tmp, ymm7_tmp);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm6_tmp);

    ymm5_tmp = _mm256_permute2f128_si256(ymm4_tmp, ymm4_tmp, 65);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm5_tmp);
    // sad 3,i-4 ready in low of mm4
    //check if min and replace in ymm14_yx_minsad
    ymm5_tmp = _mm256_cmpgt_epi32(ymm14_yx_minsad, ymm4_tmp); // mask in 0 16bit word for update minsad if needed
    ymm5_tmp = _mm256_slli_epi64(ymm5_tmp, 32);// clear higher bits of mask - may be better method exist
    ymm5_tmp = _mm256_srli_epi64(ymm5_tmp, 32);// clear higher bits of mask
    ymm14_yx_minsad = _mm256_blendv_epi8(ymm14_yx_minsad, ymm4_tmp, ymm5_tmp); // blend minsad if not greater than
    ymm5_tmp = _mm256_shufflelo_epi16(ymm5_tmp, 10); // move/broadcast mask from 0 to 2 and 3 16bit word for update y,x if needed
    ymm14_yx_minsad = _mm256_blendv_epi8(ymm14_yx_minsad, ymm15_yx_cnt, ymm5_tmp); // blend y,x of minsad if not greater than

    ymm15_yx_cnt = _mm256_add_epi16(ymm15_yx_cnt, ymm9_y1);//increment y coord

    // clear x cord to 0
    ymm15_yx_cnt = _mm256_srli_epi64(ymm15_yx_cnt, 48);
    ymm15_yx_cnt = _mm256_slli_epi64(ymm15_yx_cnt, 48);

  } // rows counter

  // store result
  _mm256_store_si256((__m256i*) & SIMD256Res[0], ymm14_yx_minsad);

/*
  SIMD256Res[0]; // minsad;
  SIMD256Res[2] - 2; // x of minsad vector
  SIMD256Res[3] - 2; // y of minsad vector
  )*/

  sad_t cost = SIMD256Res[0] + ((penaltyNew * SIMD256Res[0]) >> 8);
  if (cost >= workarea.nMinCost) return;

  workarea.bestMV.x = mvx + SIMD256Res[2] - 4;
  workarea.bestMV.y = mvy + SIMD256Res[3] - 4;
  workarea.nMinCost = cost;
  workarea.bestMV.sad = SIMD256Res[0];

  _mm256_zeroupper();
}
#endif

void PlaneOfBlocks::ExhaustiveSearch8x8_uint8_sp2_avx2(WorkingArea& workarea, int mvx, int mvy)
{
  // debug check !! need to fix caller to now allow illegal vectors 
    // idea - may be not 4 checks are required - only upper left corner (starting addresses of buffer) and lower right (to not over-run atfer end of buffer - need check/test)
  if (!workarea.IsVectorOK(mvx - 2, mvy - 2))
  {
    return;
  }
  if (!workarea.IsVectorOK(mvx + 2, mvy + 2))
  {
    return;
  }
  /*
  if (!workarea.IsVectorOK(mvx - 2, mvy + 2))
  {
    return;
  }
  if (!workarea.IsVectorOK(mvx + 2, mvy - 2))
  {
    return;
  }
  */
  // array of sads 5x5 
  unsigned short ArrSADs[5][5];
  const uint8_t* pucRef = GetRefBlock(workarea, mvx - 2, mvy - 2); // upper left corner
  const uint8_t* pucCurr = workarea.pSrc[0];

  __m128i xmm10_Src_01, xmm11_Src_23, xmm12_Src_45, xmm13_Src_67;

  __m256i ymm0_Ref_01, ymm1_Ref_23, ymm2_Ref_45, ymm3_Ref_67; // 2x12bytes store, require buf padding to allow 16bytes reads to xmm
  __m256i ymm10_Src_01, ymm11_Src_23, ymm12_Src_45, ymm13_Src_67;

  __m256i ymm4_tmp, ymm5_tmp, ymm6_tmp, ymm7_tmp;


  //        __m256i 	my_ymm_test = _mm256_set_epi8(31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0);// -debug values for check how permutes work
  //        __m256i 	my_ymm_test = _mm256_set_epi8(15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0);// -debug values for check how permutes work
  //        my_ymm_test = _mm256_alignr_epi8(my_ymm_test, my_ymm_test, 2); // rotate each half of ymm by number of bytes

  xmm10_Src_01 = _mm_loadu_si64((__m128i*)pucCurr);
  xmm10_Src_01 = _mm_unpacklo_epi64(xmm10_Src_01, _mm_loadu_si64((__m128i*)(pucCurr + nSrcPitch[0])));
  ymm10_Src_01 = _mm256_permute4x64_epi64(_mm256_castsi128_si256(xmm10_Src_01), 220);

  xmm11_Src_23 = _mm_loadu_si64((__m128i*)(pucCurr + nSrcPitch[0] * 2));
  xmm11_Src_23 = _mm_unpacklo_epi64(xmm11_Src_23, _mm_loadu_si64((__m128i*)(pucCurr + nSrcPitch[0] * 3)));
  ymm11_Src_23 = _mm256_permute4x64_epi64(_mm256_castsi128_si256(xmm11_Src_23), 220);

  xmm12_Src_45 = _mm_loadu_si64((__m128i*)(pucCurr + nSrcPitch[0] * 4));
  xmm12_Src_45 = _mm_unpacklo_epi64(xmm12_Src_45, _mm_loadu_si64((__m128i*)(pucCurr + nSrcPitch[0] * 5)));
  ymm12_Src_45 = _mm256_permute4x64_epi64(_mm256_castsi128_si256(xmm12_Src_45), 220);

  xmm13_Src_67 = _mm_loadu_si64((__m128i*)(pucCurr + nSrcPitch[0] * 6));
  xmm13_Src_67 = _mm_unpacklo_epi64(xmm13_Src_67, _mm_loadu_si64((__m128i*)(pucCurr + nSrcPitch[0] * 7)));
  ymm13_Src_67 = _mm256_permute4x64_epi64(_mm256_castsi128_si256(xmm13_Src_67), 220);
  // loaded 8 rows of 8x8 Src block, now in low and high 128bits of ymms

  for (int i = 0; i < 5; i++)
  {
    ymm0_Ref_01 = _mm256_loadu2_m128i((__m128i*)(pucRef + nRefPitch[0] * (i + 1)), (__m128i*)(pucRef + nRefPitch[0] * (i + 0)));
    ymm1_Ref_23 = _mm256_loadu2_m128i((__m128i*)(pucRef + nRefPitch[0] * (i + 3)), (__m128i*)(pucRef + nRefPitch[0] * (i + 2)));
    ymm2_Ref_45 = _mm256_loadu2_m128i((__m128i*)(pucRef + nRefPitch[0] * (i + 5)), (__m128i*)(pucRef + nRefPitch[0] * (i + 4)));
    ymm3_Ref_67 = _mm256_loadu2_m128i((__m128i*)(pucRef + nRefPitch[0] * (i + 7)), (__m128i*)(pucRef + nRefPitch[0] * (i + 6)));
    // loaded 8 rows of Ref plane 16samples wide into ymm0..ymm3

    // process sad[-2,i-2]
    ymm4_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm0_Ref_01, 51);
    ymm5_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm1_Ref_23, 51);
    ymm6_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm2_Ref_45, 51);
    ymm7_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm3_Ref_67, 51);

    ymm4_tmp = _mm256_sad_epu8(ymm4_tmp, ymm10_Src_01);
    ymm5_tmp = _mm256_sad_epu8(ymm5_tmp, ymm11_Src_23);
    ymm6_tmp = _mm256_sad_epu8(ymm6_tmp, ymm12_Src_45);
    ymm7_tmp = _mm256_sad_epu8(ymm7_tmp, ymm13_Src_67);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm5_tmp);
    ymm6_tmp = _mm256_add_epi32(ymm6_tmp, ymm7_tmp);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm6_tmp);

    ymm5_tmp = _mm256_permute2f128_si256(ymm4_tmp, ymm4_tmp, 65);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm5_tmp);
    // sad -2,i-2 ready in low of mm4
    _mm_storeu_si16(&ArrSADs[0][i], _mm256_castsi256_si128(ymm4_tmp));

    // rotate Ref to 1 samples
    ymm0_Ref_01 = _mm256_alignr_epi8(ymm0_Ref_01, ymm0_Ref_01, 1);
    ymm1_Ref_23 = _mm256_alignr_epi8(ymm1_Ref_23, ymm1_Ref_23, 1);
    ymm2_Ref_45 = _mm256_alignr_epi8(ymm2_Ref_45, ymm2_Ref_45, 1);
    ymm3_Ref_67 = _mm256_alignr_epi8(ymm3_Ref_67, ymm3_Ref_67, 1);

    // process sad[-1,-2]
    ymm4_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm0_Ref_01, 51);
    ymm5_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm1_Ref_23, 51);
    ymm6_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm2_Ref_45, 51);
    ymm7_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm3_Ref_67, 51);

    ymm4_tmp = _mm256_sad_epu8(ymm4_tmp, ymm10_Src_01);
    ymm5_tmp = _mm256_sad_epu8(ymm5_tmp, ymm11_Src_23);
    ymm6_tmp = _mm256_sad_epu8(ymm6_tmp, ymm12_Src_45);
    ymm7_tmp = _mm256_sad_epu8(ymm7_tmp, ymm13_Src_67);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm5_tmp);
    ymm6_tmp = _mm256_add_epi32(ymm6_tmp, ymm7_tmp);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm6_tmp);

    ymm5_tmp = _mm256_permute2f128_si256(ymm4_tmp, ymm4_tmp, 65);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm5_tmp);
    // sad -1,i-2 ready in low of mm4
    _mm_storeu_si16(&ArrSADs[1][i], _mm256_castsi256_si128(ymm4_tmp));

    // rotate Ref to 1 samples
    ymm0_Ref_01 = _mm256_alignr_epi8(ymm0_Ref_01, ymm0_Ref_01, 1);
    ymm1_Ref_23 = _mm256_alignr_epi8(ymm1_Ref_23, ymm1_Ref_23, 1);
    ymm2_Ref_45 = _mm256_alignr_epi8(ymm2_Ref_45, ymm2_Ref_45, 1);
    ymm3_Ref_67 = _mm256_alignr_epi8(ymm3_Ref_67, ymm3_Ref_67, 1);

    // process sad[-0,-2]
    ymm4_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm0_Ref_01, 51);
    ymm5_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm1_Ref_23, 51);
    ymm6_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm2_Ref_45, 51);
    ymm7_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm3_Ref_67, 51);

    ymm4_tmp = _mm256_sad_epu8(ymm4_tmp, ymm10_Src_01);
    ymm5_tmp = _mm256_sad_epu8(ymm5_tmp, ymm11_Src_23);
    ymm6_tmp = _mm256_sad_epu8(ymm6_tmp, ymm12_Src_45);
    ymm7_tmp = _mm256_sad_epu8(ymm7_tmp, ymm13_Src_67);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm5_tmp);
    ymm6_tmp = _mm256_add_epi32(ymm6_tmp, ymm7_tmp);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm6_tmp);

    ymm5_tmp = _mm256_permute2f128_si256(ymm4_tmp, ymm4_tmp, 65);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm5_tmp);
    // sad 0,i-2 ready in low of mm4
    _mm_storeu_si16(&ArrSADs[2][i], _mm256_castsi256_si128(ymm4_tmp));

    // rotate Ref to 1 samples
    ymm0_Ref_01 = _mm256_alignr_epi8(ymm0_Ref_01, ymm0_Ref_01, 1);
    ymm1_Ref_23 = _mm256_alignr_epi8(ymm1_Ref_23, ymm1_Ref_23, 1);
    ymm2_Ref_45 = _mm256_alignr_epi8(ymm2_Ref_45, ymm2_Ref_45, 1);
    ymm3_Ref_67 = _mm256_alignr_epi8(ymm3_Ref_67, ymm3_Ref_67, 1);

    // process sad[1,-2]
    ymm4_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm0_Ref_01, 51);
    ymm5_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm1_Ref_23, 51);
    ymm6_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm2_Ref_45, 51);
    ymm7_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm3_Ref_67, 51);

    ymm4_tmp = _mm256_sad_epu8(ymm4_tmp, ymm10_Src_01);
    ymm5_tmp = _mm256_sad_epu8(ymm5_tmp, ymm11_Src_23);
    ymm6_tmp = _mm256_sad_epu8(ymm6_tmp, ymm12_Src_45);
    ymm7_tmp = _mm256_sad_epu8(ymm7_tmp, ymm13_Src_67);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm5_tmp);
    ymm6_tmp = _mm256_add_epi32(ymm6_tmp, ymm7_tmp);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm6_tmp);

    ymm5_tmp = _mm256_permute2f128_si256(ymm4_tmp, ymm4_tmp, 65);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm5_tmp);
    // sad 1,i-2 ready in low of mm4
    _mm_storeu_si16(&ArrSADs[3][i], _mm256_castsi256_si128(ymm4_tmp));

    // rotate Ref to 1 samples
    ymm0_Ref_01 = _mm256_alignr_epi8(ymm0_Ref_01, ymm0_Ref_01, 1);
    ymm1_Ref_23 = _mm256_alignr_epi8(ymm1_Ref_23, ymm1_Ref_23, 1);
    ymm2_Ref_45 = _mm256_alignr_epi8(ymm2_Ref_45, ymm2_Ref_45, 1);
    ymm3_Ref_67 = _mm256_alignr_epi8(ymm3_Ref_67, ymm3_Ref_67, 1);

    // process sad[2,i-2]
    ymm4_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm0_Ref_01, 51);
    ymm5_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm1_Ref_23, 51);
    ymm6_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm2_Ref_45, 51);
    ymm7_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm3_Ref_67, 51);

    ymm4_tmp = _mm256_sad_epu8(ymm4_tmp, ymm10_Src_01);
    ymm5_tmp = _mm256_sad_epu8(ymm5_tmp, ymm11_Src_23);
    ymm6_tmp = _mm256_sad_epu8(ymm6_tmp, ymm12_Src_45);
    ymm7_tmp = _mm256_sad_epu8(ymm7_tmp, ymm13_Src_67);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm5_tmp);
    ymm6_tmp = _mm256_add_epi32(ymm6_tmp, ymm7_tmp);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm6_tmp);

    ymm5_tmp = _mm256_permute2f128_si256(ymm4_tmp, ymm4_tmp, 65);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm5_tmp);
    // sad 2,i-2 ready in low of mm4
    _mm_storeu_si16(&ArrSADs[4][i], _mm256_castsi256_si128(ymm4_tmp));
  }

  unsigned short minsad = 65535;
  int x_minsad = 0;
  int y_minsad = 0;
  for (int x = -2; x < 3; x++)
  {
    for (int y = -2; y < 3; y++)
    {
      if (ArrSADs[x + 2][y + 2] < minsad)
      {
        minsad = ArrSADs[x + 2][y + 2];
        x_minsad = x;
        y_minsad = y;
      }
    }
  }

  sad_t cost = minsad + ((penaltyNew * minsad) >> 8);
  if (cost >= workarea.nMinCost) return;

  workarea.bestMV.x = mvx + x_minsad;
  workarea.bestMV.y = mvy + y_minsad;
  workarea.nMinCost = cost;
  workarea.bestMV.sad = minsad;

  _mm256_zeroupper();
}


#if 0
// DTL: wrong way (speedwise)
void PlaneOfBlocks::ExhaustiveSearch8x8_uint8_sp2_avx2_2(WorkingArea& workarea, int mvx, int mvy)
{
  // debug check !! need to fix caller to now allow illegal vectors 
  // idea - may be not 4 checks are required - only upper left corner (starting addresses of buffer) and lower right (to not over-run atfer end of buffer - need check/test)
  if (!workarea.IsVectorOK(mvx - 2, mvy - 2))
  {
    return;
  }
  if (!workarea.IsVectorOK(mvx + 2, mvy + 2))
  {
    return;
  }
  /*	if (!workarea.IsVectorOK(mvx - 2, mvy + 2))
    {
      return;
    }
    if (!workarea.IsVectorOK(mvx + 2, mvy - 2))
    {
      return;
    }
    */ // - check if it is OK to skip these cheks

  alignas(256) unsigned short SIMD256Res[16];
  const uint8_t* pucRef = GetRefBlock(workarea, mvx - 2, mvy - 2); // upper left corner
  const uint8_t* pucCurr = workarea.pSrc[0];

  __m128i xmm10_Src_01, xmm11_Src_23, xmm12_Src_45, xmm13_Src_67;

  __m256i ymm0_Ref_01, ymm1_Ref_23, ymm2_Ref_45, ymm3_Ref_67; // 2x12bytes store, require buf padding to allow 16bytes reads to xmm
  __m256i ymm4_tmp, ymm5_tmp, ymm6_tmp, ymm7_tmp;
  __m256i ymm8_x1 = _mm256_set_epi16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0);
  __m256i ymm9_y1 = _mm256_set_epi16(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0);
  __m256i ymm10_Src_01, ymm11_Src_23, ymm12_Src_45, ymm13_Src_67;
  __m256i ymm14_yx_minsad = _mm256_set_epi32(0, 0, 0, 0, 0, 0, 0, 1073741824); // 2^30 large signed 32bit
  __m256i ymm15_yx_cnt = _mm256_setzero_si256(); // y,x coords of search count from 0

  //        __m256i 	my_ymm_test = _mm256_set_epi8(31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0);// -debug values for check how permutes work
  //        __m256i 	my_ymm_test = _mm256_set_epi8(15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0);// -debug values for check how permutes work
  //        my_ymm_test = _mm256_alignr_epi8(my_ymm_test, my_ymm_test, 2); // rotate each half of ymm by number of bytes

  xmm10_Src_01 = _mm_loadu_si64((__m128i*)pucCurr);
  xmm10_Src_01 = _mm_unpacklo_epi64(xmm10_Src_01, _mm_loadu_si64((__m128i*)(pucCurr + nSrcPitch[0])));
  ymm10_Src_01 = _mm256_permute4x64_epi64(_mm256_castsi128_si256(xmm10_Src_01), 220);

  xmm11_Src_23 = _mm_loadu_si64((__m128i*)(pucCurr + nSrcPitch[0] * 2));
  xmm11_Src_23 = _mm_unpacklo_epi64(xmm11_Src_23, _mm_loadu_si64((__m128i*)(pucCurr + nSrcPitch[0] * 3)));
  ymm11_Src_23 = _mm256_permute4x64_epi64(_mm256_castsi128_si256(xmm11_Src_23), 220);

  xmm12_Src_45 = _mm_loadu_si64((__m128i*)(pucCurr + nSrcPitch[0] * 4));
  xmm12_Src_45 = _mm_unpacklo_epi64(xmm12_Src_45, _mm_loadu_si64((__m128i*)(pucCurr + nSrcPitch[0] * 5)));
  ymm12_Src_45 = _mm256_permute4x64_epi64(_mm256_castsi128_si256(xmm12_Src_45), 220);

  xmm13_Src_67 = _mm_loadu_si64((__m128i*)(pucCurr + nSrcPitch[0] * 6));
  xmm13_Src_67 = _mm_unpacklo_epi64(xmm13_Src_67, _mm_loadu_si64((__m128i*)(pucCurr + nSrcPitch[0] * 7)));
  ymm13_Src_67 = _mm256_permute4x64_epi64(_mm256_castsi128_si256(xmm13_Src_67), 220);
  // loaded 8 rows of 8x8 Src block, now in low and high 128bits of ymms

  for (int i = 0; i < 5; i++)
  {
    ymm0_Ref_01 = _mm256_loadu2_m128i((__m128i*)(pucRef + nRefPitch[0] * (i + 1)), (__m128i*)(pucRef + nRefPitch[0] * (i + 0)));
    ymm1_Ref_23 = _mm256_loadu2_m128i((__m128i*)(pucRef + nRefPitch[0] * (i + 3)), (__m128i*)(pucRef + nRefPitch[0] * (i + 2)));
    ymm2_Ref_45 = _mm256_loadu2_m128i((__m128i*)(pucRef + nRefPitch[0] * (i + 5)), (__m128i*)(pucRef + nRefPitch[0] * (i + 4)));
    ymm3_Ref_67 = _mm256_loadu2_m128i((__m128i*)(pucRef + nRefPitch[0] * (i + 7)), (__m128i*)(pucRef + nRefPitch[0] * (i + 6)));
    // loaded 8 rows of Ref plane 16samples wide into ymm0..ymm3
    _mm_prefetch(const_cast<const CHAR*>(reinterpret_cast<const CHAR*>(pucRef + nRefPitch[0] * (i + 8))), _MM_HINT_NTA); // prefetch next Ref row

    // process sad[-2,i-2]
    ymm4_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm0_Ref_01, 51);
    ymm5_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm1_Ref_23, 51);
    ymm6_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm2_Ref_45, 51);
    ymm7_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm3_Ref_67, 51);

    ymm4_tmp = _mm256_sad_epu8(ymm4_tmp, ymm10_Src_01);
    ymm5_tmp = _mm256_sad_epu8(ymm5_tmp, ymm11_Src_23);
    ymm6_tmp = _mm256_sad_epu8(ymm6_tmp, ymm12_Src_45);
    ymm7_tmp = _mm256_sad_epu8(ymm7_tmp, ymm13_Src_67);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm5_tmp);
    ymm6_tmp = _mm256_add_epi32(ymm6_tmp, ymm7_tmp);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm6_tmp);

    ymm5_tmp = _mm256_permute2f128_si256(ymm4_tmp, ymm4_tmp, 65);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm5_tmp);
    // sad -2,i-2 ready in low of ymm4
    //check if min and replace in ymm14_yx_minsad
    ymm5_tmp = _mm256_cmpgt_epi32(ymm14_yx_minsad, ymm4_tmp); // mask in 0 16bit word for update minsad if needed
    ymm5_tmp = _mm256_slli_epi64(ymm5_tmp, 32);// clear higher bits of mask - may be better method exist
    ymm5_tmp = _mm256_srli_epi64(ymm5_tmp, 32);// clear higher bits of mask
    ymm14_yx_minsad = _mm256_blendv_epi8(ymm14_yx_minsad, ymm4_tmp, ymm5_tmp); // blend minsad if not greater than
    ymm5_tmp = _mm256_shufflelo_epi16(ymm5_tmp, 10); // move/broadcast mask from 0 to 2 and 3 16bit word for update y,x if needed
    ymm14_yx_minsad = _mm256_blendv_epi8(ymm14_yx_minsad, ymm15_yx_cnt, ymm5_tmp); // blend y,x of minsad if not greater than

    ymm15_yx_cnt = _mm256_add_epi16(ymm15_yx_cnt, ymm8_x1);//increment x coord

    // rotate Ref to 1 samples
    ymm0_Ref_01 = _mm256_alignr_epi8(ymm0_Ref_01, ymm0_Ref_01, 1);
    ymm1_Ref_23 = _mm256_alignr_epi8(ymm1_Ref_23, ymm1_Ref_23, 1);
    ymm2_Ref_45 = _mm256_alignr_epi8(ymm2_Ref_45, ymm2_Ref_45, 1);
    ymm3_Ref_67 = _mm256_alignr_epi8(ymm3_Ref_67, ymm3_Ref_67, 1);

    // process sad[-1,-2]
    ymm4_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm0_Ref_01, 51);
    ymm5_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm1_Ref_23, 51);
    ymm6_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm2_Ref_45, 51);
    ymm7_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm3_Ref_67, 51);

    ymm4_tmp = _mm256_sad_epu8(ymm4_tmp, ymm10_Src_01);
    ymm5_tmp = _mm256_sad_epu8(ymm5_tmp, ymm11_Src_23);
    ymm6_tmp = _mm256_sad_epu8(ymm6_tmp, ymm12_Src_45);
    ymm7_tmp = _mm256_sad_epu8(ymm7_tmp, ymm13_Src_67);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm5_tmp);
    ymm6_tmp = _mm256_add_epi32(ymm6_tmp, ymm7_tmp);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm6_tmp);

    ymm5_tmp = _mm256_permute2f128_si256(ymm4_tmp, ymm4_tmp, 65);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm5_tmp);
    // sad -1,i-2 ready in low of ymm4
    //check if min and replace in ymm14_yx_minsad
    ymm5_tmp = _mm256_cmpgt_epi32(ymm14_yx_minsad, ymm4_tmp); // mask in 0 16bit word for update minsad if needed
    ymm5_tmp = _mm256_slli_epi64(ymm5_tmp, 32);// clear higher bits of mask - may be better method exist
    ymm5_tmp = _mm256_srli_epi64(ymm5_tmp, 32);// clear higher bits of mask
    ymm14_yx_minsad = _mm256_blendv_epi8(ymm14_yx_minsad, ymm4_tmp, ymm5_tmp); // blend minsad if not greater than
    ymm5_tmp = _mm256_shufflelo_epi16(ymm5_tmp, 10); // move/broadcast mask from 0 to 2 and 3 16bit word for update y,x if needed
    ymm14_yx_minsad = _mm256_blendv_epi8(ymm14_yx_minsad, ymm15_yx_cnt, ymm5_tmp); // blend y,x of minsad if not greater than

    ymm15_yx_cnt = _mm256_add_epi16(ymm15_yx_cnt, ymm8_x1);//increment x coord

    // rotate Ref to 1 samples
    ymm0_Ref_01 = _mm256_alignr_epi8(ymm0_Ref_01, ymm0_Ref_01, 1);
    ymm1_Ref_23 = _mm256_alignr_epi8(ymm1_Ref_23, ymm1_Ref_23, 1);
    ymm2_Ref_45 = _mm256_alignr_epi8(ymm2_Ref_45, ymm2_Ref_45, 1);
    ymm3_Ref_67 = _mm256_alignr_epi8(ymm3_Ref_67, ymm3_Ref_67, 1);

    // process sad[-0,-2]
    ymm4_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm0_Ref_01, 51);
    ymm5_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm1_Ref_23, 51);
    ymm6_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm2_Ref_45, 51);
    ymm7_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm3_Ref_67, 51);

    ymm4_tmp = _mm256_sad_epu8(ymm4_tmp, ymm10_Src_01);
    ymm5_tmp = _mm256_sad_epu8(ymm5_tmp, ymm11_Src_23);
    ymm6_tmp = _mm256_sad_epu8(ymm6_tmp, ymm12_Src_45);
    ymm7_tmp = _mm256_sad_epu8(ymm7_tmp, ymm13_Src_67);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm5_tmp);
    ymm6_tmp = _mm256_add_epi32(ymm6_tmp, ymm7_tmp);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm6_tmp);

    ymm5_tmp = _mm256_permute2f128_si256(ymm4_tmp, ymm4_tmp, 65);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm5_tmp);
    // sad 0,i-2 ready in low of ymm4
    //check if min and replace in ymm14_yx_minsad
    ymm5_tmp = _mm256_cmpgt_epi32(ymm14_yx_minsad, ymm4_tmp); // mask in 0 16bit word for update minsad if needed
    ymm5_tmp = _mm256_slli_epi64(ymm5_tmp, 32);// clear higher bits of mask - may be better method exist
    ymm5_tmp = _mm256_srli_epi64(ymm5_tmp, 32);// clear higher bits of mask
    ymm14_yx_minsad = _mm256_blendv_epi8(ymm14_yx_minsad, ymm4_tmp, ymm5_tmp); // blend minsad if not greater than
    ymm5_tmp = _mm256_shufflelo_epi16(ymm5_tmp, 10); // move/broadcast mask from 0 to 2 and 3 16bit word for update y,x if needed
    ymm14_yx_minsad = _mm256_blendv_epi8(ymm14_yx_minsad, ymm15_yx_cnt, ymm5_tmp); // blend y,x of minsad if not greater than

    ymm15_yx_cnt = _mm256_add_epi16(ymm15_yx_cnt, ymm8_x1);//increment x coord

    // rotate Ref to 1 samples
    ymm0_Ref_01 = _mm256_alignr_epi8(ymm0_Ref_01, ymm0_Ref_01, 1);
    ymm1_Ref_23 = _mm256_alignr_epi8(ymm1_Ref_23, ymm1_Ref_23, 1);
    ymm2_Ref_45 = _mm256_alignr_epi8(ymm2_Ref_45, ymm2_Ref_45, 1);
    ymm3_Ref_67 = _mm256_alignr_epi8(ymm3_Ref_67, ymm3_Ref_67, 1);

    // process sad[1,-2]
    ymm4_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm0_Ref_01, 51);
    ymm5_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm1_Ref_23, 51);
    ymm6_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm2_Ref_45, 51);
    ymm7_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm3_Ref_67, 51);

    ymm4_tmp = _mm256_sad_epu8(ymm4_tmp, ymm10_Src_01);
    ymm5_tmp = _mm256_sad_epu8(ymm5_tmp, ymm11_Src_23);
    ymm6_tmp = _mm256_sad_epu8(ymm6_tmp, ymm12_Src_45);
    ymm7_tmp = _mm256_sad_epu8(ymm7_tmp, ymm13_Src_67);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm5_tmp);
    ymm6_tmp = _mm256_add_epi32(ymm6_tmp, ymm7_tmp);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm6_tmp);

    ymm5_tmp = _mm256_permute2f128_si256(ymm4_tmp, ymm4_tmp, 65);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm5_tmp);
    // sad 1,i-2 ready in low of mm4
    //check if min and replace in ymm14_yx_minsad
    ymm5_tmp = _mm256_cmpgt_epi32(ymm14_yx_minsad, ymm4_tmp); // mask in 0 16bit word for update minsad if needed
    ymm5_tmp = _mm256_slli_epi64(ymm5_tmp, 32);// clear higher bits of mask - may be better method exist
    ymm5_tmp = _mm256_srli_epi64(ymm5_tmp, 32);// clear higher bits of mask
    ymm14_yx_minsad = _mm256_blendv_epi8(ymm14_yx_minsad, ymm4_tmp, ymm5_tmp); // blend minsad if not greater than
    ymm5_tmp = _mm256_shufflelo_epi16(ymm5_tmp, 10); // move/broadcast mask from 0 to 2 and 3 16bit word for update y,x if needed
    ymm14_yx_minsad = _mm256_blendv_epi8(ymm14_yx_minsad, ymm15_yx_cnt, ymm5_tmp); // blend y,x of minsad if not greater than

    ymm15_yx_cnt = _mm256_add_epi16(ymm15_yx_cnt, ymm8_x1);//increment x coord

    // rotate Ref to 1 samples
    ymm0_Ref_01 = _mm256_alignr_epi8(ymm0_Ref_01, ymm0_Ref_01, 1);
    ymm1_Ref_23 = _mm256_alignr_epi8(ymm1_Ref_23, ymm1_Ref_23, 1);
    ymm2_Ref_45 = _mm256_alignr_epi8(ymm2_Ref_45, ymm2_Ref_45, 1);
    ymm3_Ref_67 = _mm256_alignr_epi8(ymm3_Ref_67, ymm3_Ref_67, 1);

    // process sad[2,i-2]
    ymm4_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm0_Ref_01, 51);
    ymm5_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm1_Ref_23, 51);
    ymm6_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm2_Ref_45, 51);
    ymm7_tmp = _mm256_blend_epi32(_mm256_setzero_si256(), ymm3_Ref_67, 51);

    ymm4_tmp = _mm256_sad_epu8(ymm4_tmp, ymm10_Src_01);
    ymm5_tmp = _mm256_sad_epu8(ymm5_tmp, ymm11_Src_23);
    ymm6_tmp = _mm256_sad_epu8(ymm6_tmp, ymm12_Src_45);
    ymm7_tmp = _mm256_sad_epu8(ymm7_tmp, ymm13_Src_67);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm5_tmp);
    ymm6_tmp = _mm256_add_epi32(ymm6_tmp, ymm7_tmp);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm6_tmp);

    ymm5_tmp = _mm256_permute2f128_si256(ymm4_tmp, ymm4_tmp, 65);

    ymm4_tmp = _mm256_add_epi32(ymm4_tmp, ymm5_tmp);
    // sad 2,i-2 ready in low of mm4
    //check if min and replace in ymm14_yx_minsad
    ymm5_tmp = _mm256_cmpgt_epi32(ymm14_yx_minsad, ymm4_tmp); // mask in 0 16bit word for update minsad if needed
    ymm5_tmp = _mm256_slli_epi64(ymm5_tmp, 32);// clear higher bits of mask - may be better method exist
    ymm5_tmp = _mm256_srli_epi64(ymm5_tmp, 32);// clear higher bits of mask
    ymm14_yx_minsad = _mm256_blendv_epi8(ymm14_yx_minsad, ymm4_tmp, ymm5_tmp); // blend minsad if not greater than
    ymm5_tmp = _mm256_shufflelo_epi16(ymm5_tmp, 10); // move/broadcast mask from 0 to 2 and 3 16bit word for update y,x if needed
    ymm14_yx_minsad = _mm256_blendv_epi8(ymm14_yx_minsad, ymm15_yx_cnt, ymm5_tmp); // blend y,x of minsad if not greater than

    ymm15_yx_cnt = _mm256_add_epi16(ymm15_yx_cnt, ymm9_y1);//increment y coord

    // clear x cord to 0
    ymm15_yx_cnt = _mm256_srli_epi64(ymm15_yx_cnt, 48);
    ymm15_yx_cnt = _mm256_slli_epi64(ymm15_yx_cnt, 48);

  } // rows counter

  // store result
  _mm256_store_si256((__m256i*) & SIMD256Res[0], ymm14_yx_minsad);

/*
  SIMD256Res[0]; // minsad;
  SIMD256Res[2] - 2; // x of minsad vector
  SIMD256Res[3] - 2; // y of minsad vector
  )*/

  sad_t cost = SIMD256Res[0] + ((penaltyNew * SIMD256Res[0]) >> 8);
  if (cost >= workarea.nMinCost) return;

  workarea.bestMV.x = mvx + SIMD256Res[2] - 2;
  workarea.bestMV.y = mvy + SIMD256Res[3] - 2;
  workarea.nMinCost = cost;
  workarea.bestMV.sad = SIMD256Res[0];

  _mm256_zeroupper();
}
#endif
