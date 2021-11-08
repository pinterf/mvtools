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

#ifndef _mm_storeu_si16
#define _mm_storeu_si16(p, a) (void)(*(short*)(p) = (short)_mm_cvtsi128_si32((a)))
#endif

#include "PlaneOfBlocks.h"
#include <map>
#include <tuple>

#include <stdint.h>
#include "def.h"

void PlaneOfBlocks::ExhaustiveSearch8x8_uint8_np1_sp4_avx2(WorkingArea& workarea, int mvx, int mvy) // still old design - need to redo to new approach
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

void PlaneOfBlocks::ExhaustiveSearch8x8_uint8_np1_sp2_avx2(WorkingArea& workarea, int mvx, int mvy)
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

  __m256i ymm0_Ref_01, ymm1_Ref_23, ymm2_Ref_45, ymm3_Ref_67; // 2x12bytes store, require buf padding to allow 16bytes reads to xmm
  __m256i ymm4_tmp, ymm5_tmp;

  __m256i ymm10_Src_2031, ymm11_Src_6475;
  __m128i xmm10_Src_20, xmm11_Src_31, xmm12_Src_64, xmm13_Src_75;

  const uint8_t* pucRef = GetRefBlock(workarea, mvx - 2, mvy - 2); // upper left corner
  const uint8_t* pucCurr = workarea.pSrc[0];

  _mm_prefetch(const_cast<const CHAR*>(reinterpret_cast<const CHAR*>(pucRef + nRefPitch[0] * (9))), _MM_HINT_NTA); // prefetch next Ref row
  _mm_prefetch(const_cast<const CHAR*>(reinterpret_cast<const CHAR*>(pucRef + nRefPitch[0] * (10))), _MM_HINT_NTA); // prefetch next Ref row
  _mm_prefetch(const_cast<const CHAR*>(reinterpret_cast<const CHAR*>(pucRef + nRefPitch[0] * (11))), _MM_HINT_NTA); // prefetch next Ref row
  _mm_prefetch(const_cast<const CHAR*>(reinterpret_cast<const CHAR*>(pucRef + nRefPitch[0] * (12))), _MM_HINT_NTA); // prefetch next Ref row


  __m256i ymm7_minsad8_1, ymm8_minsad8_2, ymm9_minsad8_3; // vectors of minsads for SSE4.1 _mm_minpos_epu16() minsad and pos search

#ifdef _DEBUG
  ymm7_minsad8_1 = _mm256_setzero_si256();
  ymm8_minsad8_2 = _mm256_setzero_si256();
  ymm9_minsad8_3 = _mm256_setzero_si256();
#endif

  __m256i ymm6_part_sads = _mm256_setzero_si256();

  xmm10_Src_20 = _mm_unpacklo_epi64(_mm_loadu_si64((__m128i*)(pucCurr + nSrcPitch[0] * 2)), _mm_loadu_si64((__m128i*)(pucCurr + nSrcPitch[0] * 0)));
  xmm11_Src_31 = _mm_unpacklo_epi64(_mm_loadu_si64((__m128i*)(pucCurr + nSrcPitch[0] * 3)), _mm_loadu_si64((__m128i*)(pucCurr + nSrcPitch[0] * 1)));
  ymm10_Src_2031 = _mm256_permute2x128_si256(_mm256_castsi128_si256(xmm10_Src_20), _mm256_castsi128_si256(xmm11_Src_31), 32);

  xmm12_Src_64 = _mm_unpacklo_epi64(_mm_loadu_si64((__m128i*)(pucCurr + nSrcPitch[0] * 6)), _mm_loadu_si64((__m128i*)(pucCurr + nSrcPitch[0] * 4)));
  xmm13_Src_75 = _mm_unpacklo_epi64(_mm_loadu_si64((__m128i*)(pucCurr + nSrcPitch[0] * 7)), _mm_loadu_si64((__m128i*)(pucCurr + nSrcPitch[0] * 5)));
  ymm11_Src_6475 = _mm256_permute2x128_si256(_mm256_castsi128_si256(xmm12_Src_64), _mm256_castsi128_si256(xmm13_Src_75), 32);
  // current block for search loaded into ymm10 and ymm11

  // 1st row 
  ULONG64 i = 0;
  ymm0_Ref_01 = _mm256_loadu2_m128i((__m128i*)(pucRef + nRefPitch[0] * (i + 1)), (__m128i*)(pucRef + nRefPitch[0] * (i + 0)));
  ymm1_Ref_23 = _mm256_loadu2_m128i((__m128i*)(pucRef + nRefPitch[0] * (i + 3)), (__m128i*)(pucRef + nRefPitch[0] * (i + 2)));
  ymm2_Ref_45 = _mm256_loadu2_m128i((__m128i*)(pucRef + nRefPitch[0] * (i + 5)), (__m128i*)(pucRef + nRefPitch[0] * (i + 4)));
  ymm3_Ref_67 = _mm256_loadu2_m128i((__m128i*)(pucRef + nRefPitch[0] * (i + 7)), (__m128i*)(pucRef + nRefPitch[0] * (i + 6)));
  // loaded 8 rows of Ref plane 16samples wide into ymm0..ymm3

  // process sad[-2,i-2]
  ymm4_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm0_Ref_01, 8), ymm1_Ref_23, 51), ymm10_Src_2031);
  ymm5_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm2_Ref_45, 8), ymm3_Ref_67, 51), ymm11_Src_6475);

  ymm4_tmp = _mm256_adds_epu16(ymm4_tmp, ymm5_tmp);
  // sad -2,i-2 4 parts ready in low of mm4
  ymm6_part_sads = _mm256_blend_epi16(ymm6_part_sads, ymm4_tmp, 17); //  partial sums 1 of 8 in hi and low 128bits

  // shift is possibly faster at IceLake and newer
  ymm0_Ref_01 = _mm256_srli_si256(ymm0_Ref_01, 1);
  ymm1_Ref_23 = _mm256_srli_si256(ymm1_Ref_23, 1);
  ymm2_Ref_45 = _mm256_srli_si256(ymm2_Ref_45, 1);
  ymm3_Ref_67 = _mm256_srli_si256(ymm3_Ref_67, 1);

  // process sad[-1,-2]
  ymm4_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm0_Ref_01, 8), ymm1_Ref_23, 51), ymm10_Src_2031);
  ymm5_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm2_Ref_45, 8), ymm3_Ref_67, 51), ymm11_Src_6475);

  ymm4_tmp = _mm256_adds_epu16(ymm4_tmp, ymm5_tmp);
  // sad -1,i-2 4 parts ready in low of mm4
  ymm6_part_sads = _mm256_slli_si256(ymm6_part_sads, 2);
  ymm6_part_sads = _mm256_blend_epi16(ymm6_part_sads, ymm4_tmp, 17); //  partial sums 2 of 8 in hi and low 128bits

  ymm0_Ref_01 = _mm256_srli_si256(ymm0_Ref_01, 1);
  ymm1_Ref_23 = _mm256_srli_si256(ymm1_Ref_23, 1);
  ymm2_Ref_45 = _mm256_srli_si256(ymm2_Ref_45, 1);
  ymm3_Ref_67 = _mm256_srli_si256(ymm3_Ref_67, 1);

  // process sad[-0,-2]
  ymm4_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm0_Ref_01, 8), ymm1_Ref_23, 51), ymm10_Src_2031);
  ymm5_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm2_Ref_45, 8), ymm3_Ref_67, 51), ymm11_Src_6475);

  ymm4_tmp = _mm256_adds_epu16(ymm4_tmp, ymm5_tmp);
  // sad 0,i-2 4 parts ready in low of mm4
  ymm6_part_sads = _mm256_slli_si256(ymm6_part_sads, 2);
  ymm6_part_sads = _mm256_blend_epi16(ymm6_part_sads, ymm4_tmp, 17); // partial sums 3 of 8 in hi and low 128bits

  ymm0_Ref_01 = _mm256_srli_si256(ymm0_Ref_01, 1);
  ymm1_Ref_23 = _mm256_srli_si256(ymm1_Ref_23, 1);
  ymm2_Ref_45 = _mm256_srli_si256(ymm2_Ref_45, 1);
  ymm3_Ref_67 = _mm256_srli_si256(ymm3_Ref_67, 1);

  // process sad[1,-2]
  ymm4_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm0_Ref_01, 8), ymm1_Ref_23, 51), ymm10_Src_2031);
  ymm5_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm2_Ref_45, 8), ymm3_Ref_67, 51), ymm11_Src_6475);

  ymm4_tmp = _mm256_adds_epu16(ymm4_tmp, ymm5_tmp);
  // sad 1,i-2 4 parts ready in low of mm4
  ymm6_part_sads = _mm256_slli_si256(ymm6_part_sads, 2);
  ymm6_part_sads = _mm256_blend_epi16(ymm6_part_sads, ymm4_tmp, 17); // partial sums 4 of 8 in hi and low 128bits

  // process 4 partial sads in 2 partial sads
  ymm6_part_sads = _mm256_adds_epu16(_mm256_srli_si256(ymm6_part_sads, 8), ymm6_part_sads);
  ymm7_minsad8_1 = _mm256_blend_epi16(ymm7_minsad8_1, ymm6_part_sads, 15);

  ymm0_Ref_01 = _mm256_srli_si256(ymm0_Ref_01, 1);
  ymm1_Ref_23 = _mm256_srli_si256(ymm1_Ref_23, 1);
  ymm2_Ref_45 = _mm256_srli_si256(ymm2_Ref_45, 1);
  ymm3_Ref_67 = _mm256_srli_si256(ymm3_Ref_67, 1);

  // process sad[2,i-2]
  ymm4_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm0_Ref_01, 8), ymm1_Ref_23, 51), ymm10_Src_2031);
  ymm5_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm2_Ref_45, 8), ymm3_Ref_67, 51), ymm11_Src_6475);

  ymm4_tmp = _mm256_adds_epu16(ymm4_tmp, ymm5_tmp);
  // sad 2,i-2 4 parts ready in low of mm4
  ymm6_part_sads = _mm256_blend_epi16(ymm6_part_sads, ymm4_tmp, 17); // partial sums 5 of 8 in hi and low 128bits

  //
  //2 row 
  //
  i = 1;
  ymm0_Ref_01 = _mm256_loadu2_m128i((__m128i*)(pucRef + nRefPitch[0] * (i + 1)), (__m128i*)(pucRef + nRefPitch[0] * (i + 0)));
  ymm1_Ref_23 = _mm256_loadu2_m128i((__m128i*)(pucRef + nRefPitch[0] * (i + 3)), (__m128i*)(pucRef + nRefPitch[0] * (i + 2)));
  ymm2_Ref_45 = _mm256_loadu2_m128i((__m128i*)(pucRef + nRefPitch[0] * (i + 5)), (__m128i*)(pucRef + nRefPitch[0] * (i + 4)));
  ymm3_Ref_67 = _mm256_loadu2_m128i((__m128i*)(pucRef + nRefPitch[0] * (i + 7)), (__m128i*)(pucRef + nRefPitch[0] * (i + 6)));
  // loaded 8 rows of Ref plane 16samples wide into ymm0..ymm3

  // process sad[-2,i-2]
  ymm4_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm0_Ref_01, 8), ymm1_Ref_23, 51), ymm10_Src_2031);
  ymm5_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm2_Ref_45, 8), ymm3_Ref_67, 51), ymm11_Src_6475);

  ymm4_tmp = _mm256_adds_epu16(ymm4_tmp, ymm5_tmp);
  // sad -2,i-2 2 parts ready in low of mm4
  ymm6_part_sads = _mm256_slli_si256(ymm6_part_sads, 2);
  ymm6_part_sads = _mm256_blend_epi16(ymm6_part_sads, ymm4_tmp, 17); // partial sums 6 of 8 in hi and low 128bits

  ymm0_Ref_01 = _mm256_srli_si256(ymm0_Ref_01, 1);
  ymm1_Ref_23 = _mm256_srli_si256(ymm1_Ref_23, 1);
  ymm2_Ref_45 = _mm256_srli_si256(ymm2_Ref_45, 1);
  ymm3_Ref_67 = _mm256_srli_si256(ymm3_Ref_67, 1);

  // process sad[-1,-2]
  ymm4_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm0_Ref_01, 8), ymm1_Ref_23, 51), ymm10_Src_2031);
  ymm5_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm2_Ref_45, 8), ymm3_Ref_67, 51), ymm11_Src_6475);

  ymm4_tmp = _mm256_adds_epu16(ymm4_tmp, ymm5_tmp);
  // sad -1,i-2 2 parts ready in low of mm4
  ymm6_part_sads = _mm256_slli_si256(ymm6_part_sads, 2);
  ymm6_part_sads = _mm256_blend_epi16(ymm6_part_sads, ymm4_tmp, 17); // partial sums 7 of 8 in hi and low 128bits

  ymm0_Ref_01 = _mm256_srli_si256(ymm0_Ref_01, 1);
  ymm1_Ref_23 = _mm256_srli_si256(ymm1_Ref_23, 1);
  ymm2_Ref_45 = _mm256_srli_si256(ymm2_Ref_45, 1);
  ymm3_Ref_67 = _mm256_srli_si256(ymm3_Ref_67, 1);

  // process sad[-0,-2]
  ymm4_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm0_Ref_01, 8), ymm1_Ref_23, 51), ymm10_Src_2031);
  ymm5_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm2_Ref_45, 8), ymm3_Ref_67, 51), ymm11_Src_6475);

  ymm4_tmp = _mm256_adds_epu16(ymm4_tmp, ymm5_tmp);
  // sad 0,i-2 2 parts ready in low of mm4
  ymm6_part_sads = _mm256_slli_si256(ymm6_part_sads, 2);
  ymm6_part_sads = _mm256_blend_epi16(ymm6_part_sads, ymm4_tmp, 17); // partial sums 8 of 8 in hi and low 128bits

  ymm6_part_sads = _mm256_add_epi16(_mm256_srli_si256(ymm6_part_sads, 8), ymm6_part_sads);
  ymm7_minsad8_1 = _mm256_slli_si256(ymm7_minsad8_1, 8);
  ymm7_minsad8_1 = _mm256_blend_epi16(ymm7_minsad8_1, ymm6_part_sads, 15);

  ymm7_minsad8_1 = _mm256_adds_epu16(_mm256_castsi128_si256(_mm256_extracti128_si256(ymm7_minsad8_1, 1)), ymm7_minsad8_1);   // minsad8_1 ready

  ymm0_Ref_01 = _mm256_srli_si256(ymm0_Ref_01, 1);
  ymm1_Ref_23 = _mm256_srli_si256(ymm1_Ref_23, 1);
  ymm2_Ref_45 = _mm256_srli_si256(ymm2_Ref_45, 1);
  ymm3_Ref_67 = _mm256_srli_si256(ymm3_Ref_67, 1);

  // process sad[1,-2]
  ymm4_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm0_Ref_01, 8), ymm1_Ref_23, 51), ymm10_Src_2031);
  ymm5_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm2_Ref_45, 8), ymm3_Ref_67, 51), ymm11_Src_6475);

  ymm4_tmp = _mm256_adds_epu16(ymm4_tmp, ymm5_tmp);
  // sad 1,i-2 ready in low of mm4
  ymm6_part_sads = _mm256_blend_epi16(ymm6_part_sads, ymm4_tmp, 17); // partial sums 1 of 8 in hi and low 128bits

  ymm0_Ref_01 = _mm256_srli_si256(ymm0_Ref_01, 1);
  ymm1_Ref_23 = _mm256_srli_si256(ymm1_Ref_23, 1);
  ymm2_Ref_45 = _mm256_srli_si256(ymm2_Ref_45, 1);
  ymm3_Ref_67 = _mm256_srli_si256(ymm3_Ref_67, 1);

  // process sad[2,i-2]
  ymm4_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm0_Ref_01, 8), ymm1_Ref_23, 51), ymm10_Src_2031);
  ymm5_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm2_Ref_45, 8), ymm3_Ref_67, 51), ymm11_Src_6475);

  ymm4_tmp = _mm256_adds_epu16(ymm4_tmp, ymm5_tmp);
  // sad 2,i-2 ready in low of mm4
  ymm6_part_sads = _mm256_slli_si256(ymm6_part_sads, 2);
  ymm6_part_sads = _mm256_blend_epi16(ymm6_part_sads, ymm4_tmp, 17); // partial sums 2 of 8 in hi and low 128bits


  //
  //3 row 
  //
  i = 2;
  ymm0_Ref_01 = _mm256_loadu2_m128i((__m128i*)(pucRef + nRefPitch[0] * (i + 1)), (__m128i*)(pucRef + nRefPitch[0] * (i + 0)));
  ymm1_Ref_23 = _mm256_loadu2_m128i((__m128i*)(pucRef + nRefPitch[0] * (i + 3)), (__m128i*)(pucRef + nRefPitch[0] * (i + 2)));
  ymm2_Ref_45 = _mm256_loadu2_m128i((__m128i*)(pucRef + nRefPitch[0] * (i + 5)), (__m128i*)(pucRef + nRefPitch[0] * (i + 4)));
  ymm3_Ref_67 = _mm256_loadu2_m128i((__m128i*)(pucRef + nRefPitch[0] * (i + 7)), (__m128i*)(pucRef + nRefPitch[0] * (i + 6)));
  // loaded 8 rows of Ref plane 16samples wide into ymm0..ymm3

  // process sad[-2,i-2]
  ymm4_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm0_Ref_01, 8), ymm1_Ref_23, 51), ymm10_Src_2031);
  ymm5_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm2_Ref_45, 8), ymm3_Ref_67, 51), ymm11_Src_6475);

  ymm4_tmp = _mm256_adds_epu16(ymm4_tmp, ymm5_tmp);
  // sad -2,i-2 ready in low of mm4
  ymm6_part_sads = _mm256_slli_si256(ymm6_part_sads, 2);
  ymm6_part_sads = _mm256_blend_epi16(ymm6_part_sads, ymm4_tmp, 17); // partial sums 3 of 8 in hi and low 128bits

  ymm0_Ref_01 = _mm256_srli_si256(ymm0_Ref_01, 1);
  ymm1_Ref_23 = _mm256_srli_si256(ymm1_Ref_23, 1);
  ymm2_Ref_45 = _mm256_srli_si256(ymm2_Ref_45, 1);
  ymm3_Ref_67 = _mm256_srli_si256(ymm3_Ref_67, 1);

  // process sad[-1,-2]
  ymm4_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm0_Ref_01, 8), ymm1_Ref_23, 51), ymm10_Src_2031);
  ymm5_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm2_Ref_45, 8), ymm3_Ref_67, 51), ymm11_Src_6475);

  ymm4_tmp = _mm256_adds_epu16(ymm4_tmp, ymm5_tmp);
  // sad -1,i-2 ready in low of mm4
  ymm6_part_sads = _mm256_slli_si256(ymm6_part_sads, 2);
  ymm6_part_sads = _mm256_blend_epi16(ymm6_part_sads, ymm4_tmp, 17); // partial sums 4 of 8 in hi and low 128bits

  // process 4 partial sads in 2 partial sads
  ymm6_part_sads = _mm256_adds_epu16(_mm256_srli_si256(ymm6_part_sads, 8), ymm6_part_sads);
  ymm8_minsad8_2 = _mm256_blend_epi16(ymm8_minsad8_2, ymm6_part_sads, 15);

  ymm0_Ref_01 = _mm256_srli_si256(ymm0_Ref_01, 1);
  ymm1_Ref_23 = _mm256_srli_si256(ymm1_Ref_23, 1);
  ymm2_Ref_45 = _mm256_srli_si256(ymm2_Ref_45, 1);
  ymm3_Ref_67 = _mm256_srli_si256(ymm3_Ref_67, 1);

  // process sad[-0,-2]
  ymm4_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm0_Ref_01, 8), ymm1_Ref_23, 51), ymm10_Src_2031);
  ymm5_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm2_Ref_45, 8), ymm3_Ref_67, 51), ymm11_Src_6475);

  ymm4_tmp = _mm256_adds_epu16(ymm4_tmp, ymm5_tmp);
  // sad 0,i-2 ready in low of mm4
  ymm6_part_sads = _mm256_slli_si256(ymm6_part_sads, 2);
  ymm6_part_sads = _mm256_blend_epi16(ymm6_part_sads, ymm4_tmp, 17); // partial sums 5 of 8 in hi and low 128bits

  ymm0_Ref_01 = _mm256_srli_si256(ymm0_Ref_01, 1);
  ymm1_Ref_23 = _mm256_srli_si256(ymm1_Ref_23, 1);
  ymm2_Ref_45 = _mm256_srli_si256(ymm2_Ref_45, 1);
  ymm3_Ref_67 = _mm256_srli_si256(ymm3_Ref_67, 1);

  // process sad[1,-2]
  ymm4_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm0_Ref_01, 8), ymm1_Ref_23, 51), ymm10_Src_2031);
  ymm5_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm2_Ref_45, 8), ymm3_Ref_67, 51), ymm11_Src_6475);

  ymm4_tmp = _mm256_adds_epu16(ymm4_tmp, ymm5_tmp);
  // sad 1,i-2 ready in low of mm4
  ymm6_part_sads = _mm256_slli_si256(ymm6_part_sads, 2);
  ymm6_part_sads = _mm256_blend_epi16(ymm6_part_sads, ymm4_tmp, 17); // partial sums 6 of 8 in hi and low 128bits

  ymm0_Ref_01 = _mm256_srli_si256(ymm0_Ref_01, 1);
  ymm1_Ref_23 = _mm256_srli_si256(ymm1_Ref_23, 1);
  ymm2_Ref_45 = _mm256_srli_si256(ymm2_Ref_45, 1);
  ymm3_Ref_67 = _mm256_srli_si256(ymm3_Ref_67, 1);

  // process sad[2,i-2]
  ymm4_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm0_Ref_01, 8), ymm1_Ref_23, 51), ymm10_Src_2031);
  ymm5_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm2_Ref_45, 8), ymm3_Ref_67, 51), ymm11_Src_6475);

  ymm4_tmp = _mm256_adds_epu16(ymm4_tmp, ymm5_tmp);
  // sad 2,i-2 ready in low of mm4
  ymm6_part_sads = _mm256_slli_si256(ymm6_part_sads, 2);
  ymm6_part_sads = _mm256_blend_epi16(ymm6_part_sads, ymm4_tmp, 17); // partial sums 7 of 8 in hi and low 128bits

  //
  //4 row 
  //
  i = 3;
  ymm0_Ref_01 = _mm256_loadu2_m128i((__m128i*)(pucRef + nRefPitch[0] * (i + 1)), (__m128i*)(pucRef + nRefPitch[0] * (i + 0)));
  ymm1_Ref_23 = _mm256_loadu2_m128i((__m128i*)(pucRef + nRefPitch[0] * (i + 3)), (__m128i*)(pucRef + nRefPitch[0] * (i + 2)));
  ymm2_Ref_45 = _mm256_loadu2_m128i((__m128i*)(pucRef + nRefPitch[0] * (i + 5)), (__m128i*)(pucRef + nRefPitch[0] * (i + 4)));
  ymm3_Ref_67 = _mm256_loadu2_m128i((__m128i*)(pucRef + nRefPitch[0] * (i + 7)), (__m128i*)(pucRef + nRefPitch[0] * (i + 6)));
  // loaded 8 rows of Ref plane 16samples wide into ymm0..ymm3

  // process sad[-2,i-2]
  ymm4_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm0_Ref_01, 8), ymm1_Ref_23, 51), ymm10_Src_2031);
  ymm5_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm2_Ref_45, 8), ymm3_Ref_67, 51), ymm11_Src_6475);

  ymm4_tmp = _mm256_adds_epu16(ymm4_tmp, ymm5_tmp);
  // sad -2,i-2 ready in low of mm4
  ymm6_part_sads = _mm256_slli_si256(ymm6_part_sads, 2);
  ymm6_part_sads = _mm256_blend_epi16(ymm6_part_sads, ymm4_tmp, 17); // partial sums 8 of 8 in hi and low 128bits

  ymm6_part_sads = _mm256_add_epi16(_mm256_srli_si256(ymm6_part_sads, 8), ymm6_part_sads);
  ymm8_minsad8_2 = _mm256_slli_si256(ymm8_minsad8_2, 8);
  ymm8_minsad8_2 = _mm256_blend_epi16(ymm8_minsad8_2, ymm6_part_sads, 15);

  ymm8_minsad8_2 = _mm256_adds_epu16(_mm256_castsi128_si256(_mm256_extracti128_si256(ymm8_minsad8_2, 1)), ymm8_minsad8_2);   // minsad8_2 ready

  ymm0_Ref_01 = _mm256_srli_si256(ymm0_Ref_01, 1);
  ymm1_Ref_23 = _mm256_srli_si256(ymm1_Ref_23, 1);
  ymm2_Ref_45 = _mm256_srli_si256(ymm2_Ref_45, 1);
  ymm3_Ref_67 = _mm256_srli_si256(ymm3_Ref_67, 1);

  // process sad[-1,-2]
  ymm4_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm0_Ref_01, 8), ymm1_Ref_23, 51), ymm10_Src_2031);
  ymm5_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm2_Ref_45, 8), ymm3_Ref_67, 51), ymm11_Src_6475);

  ymm4_tmp = _mm256_adds_epu16(ymm4_tmp, ymm5_tmp);
  // sad -1,i-2 ready in low of mm4
  ymm6_part_sads = _mm256_blend_epi16(ymm6_part_sads, ymm4_tmp, 17); // partial sums 1 of 8 in hi and low 128bits

  ymm0_Ref_01 = _mm256_srli_si256(ymm0_Ref_01, 1);
  ymm1_Ref_23 = _mm256_srli_si256(ymm1_Ref_23, 1);
  ymm2_Ref_45 = _mm256_srli_si256(ymm2_Ref_45, 1);
  ymm3_Ref_67 = _mm256_srli_si256(ymm3_Ref_67, 1);

  // process sad[-0,-2]
  ymm4_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm0_Ref_01, 8), ymm1_Ref_23, 51), ymm10_Src_2031);
  ymm5_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm2_Ref_45, 8), ymm3_Ref_67, 51), ymm11_Src_6475);

  ymm4_tmp = _mm256_adds_epu16(ymm4_tmp, ymm5_tmp);
  // sad 0,i-2 ready in low of mm4
  ymm6_part_sads = _mm256_slli_si256(ymm6_part_sads, 2);
  ymm6_part_sads = _mm256_blend_epi16(ymm6_part_sads, ymm4_tmp, 17); // partial sums 2 of 8 in hi and low 128bits

  ymm0_Ref_01 = _mm256_srli_si256(ymm0_Ref_01, 1);
  ymm1_Ref_23 = _mm256_srli_si256(ymm1_Ref_23, 1);
  ymm2_Ref_45 = _mm256_srli_si256(ymm2_Ref_45, 1);
  ymm3_Ref_67 = _mm256_srli_si256(ymm3_Ref_67, 1);

  // process sad[1,-2]
  ymm4_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm0_Ref_01, 8), ymm1_Ref_23, 51), ymm10_Src_2031);
  ymm5_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm2_Ref_45, 8), ymm3_Ref_67, 51), ymm11_Src_6475);

  ymm4_tmp = _mm256_adds_epu16(ymm4_tmp, ymm5_tmp);
  // sad 1,i-2 ready in low of mm4
  ymm6_part_sads = _mm256_slli_si256(ymm6_part_sads, 2);
  ymm6_part_sads = _mm256_blend_epi16(ymm6_part_sads, ymm4_tmp, 17); // partial sums 3 of 8 in hi and low 128bits

  ymm0_Ref_01 = _mm256_srli_si256(ymm0_Ref_01, 1);
  ymm1_Ref_23 = _mm256_srli_si256(ymm1_Ref_23, 1);
  ymm2_Ref_45 = _mm256_srli_si256(ymm2_Ref_45, 1);
  ymm3_Ref_67 = _mm256_srli_si256(ymm3_Ref_67, 1);

  // process sad[2,i-2]
  ymm4_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm0_Ref_01, 8), ymm1_Ref_23, 51), ymm10_Src_2031);
  ymm5_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm2_Ref_45, 8), ymm3_Ref_67, 51), ymm11_Src_6475);

  ymm4_tmp = _mm256_adds_epu16(ymm4_tmp, ymm5_tmp);
  // sad 2,i-2 ready in low of mm4
  ymm6_part_sads = _mm256_slli_si256(ymm6_part_sads, 2);
  ymm6_part_sads = _mm256_blend_epi16(ymm6_part_sads, ymm4_tmp, 17); // partial sums 4 of 8 in hi and low 128bits

  // process 4 partial sads in 2 partial sads
  ymm6_part_sads = _mm256_adds_epu16(_mm256_srli_si256(ymm6_part_sads, 8), ymm6_part_sads);
  ymm9_minsad8_3 = _mm256_blend_epi16(ymm9_minsad8_3, ymm6_part_sads, 15);
  //
  //5 row 
  //
  i = 4;
  ymm0_Ref_01 = _mm256_loadu2_m128i((__m128i*)(pucRef + nRefPitch[0] * (i + 1)), (__m128i*)(pucRef + nRefPitch[0] * (i + 0)));
  ymm1_Ref_23 = _mm256_loadu2_m128i((__m128i*)(pucRef + nRefPitch[0] * (i + 3)), (__m128i*)(pucRef + nRefPitch[0] * (i + 2)));
  ymm2_Ref_45 = _mm256_loadu2_m128i((__m128i*)(pucRef + nRefPitch[0] * (i + 5)), (__m128i*)(pucRef + nRefPitch[0] * (i + 4)));
  ymm3_Ref_67 = _mm256_loadu2_m128i((__m128i*)(pucRef + nRefPitch[0] * (i + 7)), (__m128i*)(pucRef + nRefPitch[0] * (i + 6)));
  // loaded 8 rows of Ref plane 16samples wide into ymm0..ymm3

  // process sad[-2,i-2]
  ymm4_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm0_Ref_01, 8), ymm1_Ref_23, 51), ymm10_Src_2031);
  ymm5_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm2_Ref_45, 8), ymm3_Ref_67, 51), ymm11_Src_6475);

  ymm4_tmp = _mm256_adds_epu16(ymm4_tmp, ymm5_tmp);
  // sad -2,i-2 ready in low of mm4
  ymm6_part_sads = _mm256_slli_si256(ymm6_part_sads, 2);
  ymm6_part_sads = _mm256_blend_epi16(ymm6_part_sads, ymm4_tmp, 17); // partial sums 5 of 8 in hi and low 128bits

  ymm0_Ref_01 = _mm256_srli_si256(ymm0_Ref_01, 1);
  ymm1_Ref_23 = _mm256_srli_si256(ymm1_Ref_23, 1);
  ymm2_Ref_45 = _mm256_srli_si256(ymm2_Ref_45, 1);
  ymm3_Ref_67 = _mm256_srli_si256(ymm3_Ref_67, 1);

  // process sad[-1,-2]
  ymm4_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm0_Ref_01, 8), ymm1_Ref_23, 51), ymm10_Src_2031);
  ymm5_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm2_Ref_45, 8), ymm3_Ref_67, 51), ymm11_Src_6475);

  ymm4_tmp = _mm256_adds_epu16(ymm4_tmp, ymm5_tmp);
  // sad -1,i-2 ready in low of mm4
  ymm6_part_sads = _mm256_slli_si256(ymm6_part_sads, 2);
  ymm6_part_sads = _mm256_blend_epi16(ymm6_part_sads, ymm4_tmp, 17); // partial sums 6 of 8 in hi and low 128bits

  ymm0_Ref_01 = _mm256_srli_si256(ymm0_Ref_01, 1);
  ymm1_Ref_23 = _mm256_srli_si256(ymm1_Ref_23, 1);
  ymm2_Ref_45 = _mm256_srli_si256(ymm2_Ref_45, 1);
  ymm3_Ref_67 = _mm256_srli_si256(ymm3_Ref_67, 1);


  // process sad[-0,-2]
  ymm4_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm0_Ref_01, 8), ymm1_Ref_23, 51), ymm10_Src_2031);
  ymm5_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm2_Ref_45, 8), ymm3_Ref_67, 51), ymm11_Src_6475);

  ymm4_tmp = _mm256_adds_epu16(ymm4_tmp, ymm5_tmp);
  // sad 0,i-2 ready in low of mm4
  ymm6_part_sads = _mm256_slli_si256(ymm6_part_sads, 2);
  ymm6_part_sads = _mm256_blend_epi16(ymm6_part_sads, ymm4_tmp, 17); // partial sums 7 of 8 in hi and low 128bits

  ymm0_Ref_01 = _mm256_srli_si256(ymm0_Ref_01, 1);
  ymm1_Ref_23 = _mm256_srli_si256(ymm1_Ref_23, 1);
  ymm2_Ref_45 = _mm256_srli_si256(ymm2_Ref_45, 1);
  ymm3_Ref_67 = _mm256_srli_si256(ymm3_Ref_67, 1);


  // process sad[1,-2]
  ymm4_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm0_Ref_01, 8), ymm1_Ref_23, 51), ymm10_Src_2031);
  ymm5_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm2_Ref_45, 8), ymm3_Ref_67, 51), ymm11_Src_6475);

  ymm4_tmp = _mm256_adds_epu16(ymm4_tmp, ymm5_tmp);
  // sad 1,i-2 ready in low of mm4
  ymm6_part_sads = _mm256_slli_si256(ymm6_part_sads, 2);
  ymm6_part_sads = _mm256_blend_epi16(ymm6_part_sads, ymm4_tmp, 17); // partial sums 8 of 8 in hi and low 128bits

  ymm6_part_sads = _mm256_add_epi16(_mm256_srli_si256(ymm6_part_sads, 8), ymm6_part_sads);
  ymm9_minsad8_3 = _mm256_slli_si256(ymm9_minsad8_3, 8);
  ymm9_minsad8_3 = _mm256_blend_epi16(ymm9_minsad8_3, ymm6_part_sads, 15);

  ymm9_minsad8_3 = _mm256_adds_epu16(_mm256_castsi128_si256(_mm256_extracti128_si256(ymm9_minsad8_3, 1)), ymm9_minsad8_3);   // minsad8_3 ready

  // last column in 5 row of scan is not processed because of 3x8=24 only values for minsad SIMD, may be proc and stored separately for compare
  // if needed, but r2 search with 'round' do not require corners in fast processing. here only 1 of 4 corners is not checked.

  unsigned short minsad = 65535;
  int idx_min_sad = 0;

  unsigned int uiSADRes1 = _mm_cvtsi128_si32(_mm_minpos_epu16(_mm256_castsi256_si128(ymm7_minsad8_1)));
  unsigned int uiSADRes2 = _mm_cvtsi128_si32(_mm_minpos_epu16(_mm256_castsi256_si128(ymm8_minsad8_2)));
  unsigned int uiSADRes3 = _mm_cvtsi128_si32(_mm_minpos_epu16(_mm256_castsi256_si128(ymm9_minsad8_3)));

  if ((unsigned short)uiSADRes1 < minsad)
  {
    minsad = (unsigned short)uiSADRes1;
    idx_min_sad = 7 - (uiSADRes1 >> 16);
  }

  if ((unsigned short)uiSADRes2 < minsad)
  {
    minsad = (unsigned short)uiSADRes2;
    idx_min_sad = 7 - (uiSADRes2 >> 16) + 8;
  }

  if ((unsigned short)uiSADRes3 < minsad)
  {
    minsad = (unsigned short)uiSADRes3;
    idx_min_sad = 7 - (uiSADRes3 >> 16) + 16;
  }

  //  x_minsad = (idx_min_sad % 5) - 2; - just comment where from x,y minsad come from
  //  y_minsad = (idx_min_sad / 5) - 2;

  sad_t cost = minsad + ((penaltyNew * minsad) >> 8);
  if (cost >= workarea.nMinCost) return;

  workarea.bestMV.x = mvx + (idx_min_sad % 5) - 2;
  workarea.bestMV.y = mvy + (idx_min_sad / 5) - 2;
  workarea.nMinCost = cost;
  workarea.bestMV.sad = minsad;

  _mm256_zeroupper();

}

void PlaneOfBlocks::ExhaustiveSearch8x8_uint8_np1_sp1_avx2(WorkingArea& workarea, int mvx, int mvy)
{
  // debug check !! need to fix caller to now allow illegal vectors 
  // idea - may be not 4 checks are required - only upper left corner (starting addresses of buffer) and lower right (to not over-run atfer end of buffer - need check/test)
  if (!workarea.IsVectorOK(mvx - 1, mvy - 1))
  {
    return;
  }
  if (!workarea.IsVectorOK(mvx + 1, mvy + 1))
  {
    return;
  }
  /*
  if (!workarea.IsVectorOK(mvx - 1, mvy + 1))
  {
    return;
  }
  if (!workarea.IsVectorOK(mvx + 1, mvy - 1))
  {
    return;
  }
  */

  __m256i ymm0_Ref_01, ymm1_Ref_23, ymm2_Ref_45, ymm3_Ref_67; // 2x12bytes store, require buf padding to allow 16bytes reads to xmm
  __m256i ymm4_tmp, ymm5_tmp;

  __m256i ymm10_Src_2031, ymm11_Src_6475;
  __m128i xmm10_Src_20, xmm11_Src_31, xmm12_Src_64, xmm13_Src_75;

  const uint8_t* pucRef = GetRefBlock(workarea, mvx - 1, mvy - 1); // upper left corner
  const uint8_t* pucCurr = workarea.pSrc[0];

  _mm_prefetch(const_cast<const CHAR*>(reinterpret_cast<const CHAR*>(pucRef + nRefPitch[0] * (9))), _MM_HINT_NTA); // prefetch next Ref row


  __m256i ymm7_minsad8_1; // vectors of minsads for SSE4.1 _mm_minpos_epu16() minsad and pos search

#ifdef _DEBUG
  ymm7_minsad8_1 = _mm256_setzero_si256(); // to prevent debug break on access non-init value, it will be replaced total at using
#endif

  __m256i ymm6_part_sads = _mm256_setzero_si256(); // also last +1,+1 sad

  xmm10_Src_20 = _mm_unpacklo_epi64(_mm_loadu_si64((__m128i*)(pucCurr + nSrcPitch[0] * 2)), _mm_loadu_si64((__m128i*)(pucCurr + nSrcPitch[0] * 0)));
  xmm11_Src_31 = _mm_unpacklo_epi64(_mm_loadu_si64((__m128i*)(pucCurr + nSrcPitch[0] * 3)), _mm_loadu_si64((__m128i*)(pucCurr + nSrcPitch[0] * 1)));
  ymm10_Src_2031 = _mm256_permute2x128_si256(_mm256_castsi128_si256(xmm10_Src_20), _mm256_castsi128_si256(xmm11_Src_31), 32);

  xmm12_Src_64 = _mm_unpacklo_epi64(_mm_loadu_si64((__m128i*)(pucCurr + nSrcPitch[0] * 6)), _mm_loadu_si64((__m128i*)(pucCurr + nSrcPitch[0] * 4)));
  xmm13_Src_75 = _mm_unpacklo_epi64(_mm_loadu_si64((__m128i*)(pucCurr + nSrcPitch[0] * 7)), _mm_loadu_si64((__m128i*)(pucCurr + nSrcPitch[0] * 5)));
  ymm11_Src_6475 = _mm256_permute2x128_si256(_mm256_castsi128_si256(xmm12_Src_64), _mm256_castsi128_si256(xmm13_Src_75), 32);
  // current block for search loaded into ymm10 and ymm11

  // 1st row 
  ULONG64 i = 0;
  ymm0_Ref_01 = _mm256_loadu2_m128i((__m128i*)(pucRef + nRefPitch[0] * (i + 1)), (__m128i*)(pucRef + nRefPitch[0] * (i + 0)));
  ymm1_Ref_23 = _mm256_loadu2_m128i((__m128i*)(pucRef + nRefPitch[0] * (i + 3)), (__m128i*)(pucRef + nRefPitch[0] * (i + 2)));
  ymm2_Ref_45 = _mm256_loadu2_m128i((__m128i*)(pucRef + nRefPitch[0] * (i + 5)), (__m128i*)(pucRef + nRefPitch[0] * (i + 4)));
  ymm3_Ref_67 = _mm256_loadu2_m128i((__m128i*)(pucRef + nRefPitch[0] * (i + 7)), (__m128i*)(pucRef + nRefPitch[0] * (i + 6)));
  // loaded 8 rows of Ref plane 16samples wide into ymm0..ymm3

  // process sad[-1,-1]
  ymm4_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm0_Ref_01, 8), ymm1_Ref_23, 51), ymm10_Src_2031);
  ymm5_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm2_Ref_45, 8), ymm3_Ref_67, 51), ymm11_Src_6475);

  ymm4_tmp = _mm256_adds_epu16(ymm4_tmp, ymm5_tmp);
  // sad -1,i-1 4 parts ready in low of mm4
  ymm6_part_sads = _mm256_blend_epi16(ymm6_part_sads, ymm4_tmp, 17); //  partial sums 1 of 8 in hi and low 128bits

  // shift is possibly faster at IceLake and newer
  ymm0_Ref_01 = _mm256_srli_si256(ymm0_Ref_01, 1);
  ymm1_Ref_23 = _mm256_srli_si256(ymm1_Ref_23, 1);
  ymm2_Ref_45 = _mm256_srli_si256(ymm2_Ref_45, 1);
  ymm3_Ref_67 = _mm256_srli_si256(ymm3_Ref_67, 1);

  // process sad[-0,-1]
  ymm4_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm0_Ref_01, 8), ymm1_Ref_23, 51), ymm10_Src_2031);
  ymm5_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm2_Ref_45, 8), ymm3_Ref_67, 51), ymm11_Src_6475);

  ymm4_tmp = _mm256_adds_epu16(ymm4_tmp, ymm5_tmp);
  // sad 0,-1 4 parts ready in low of mm4
  ymm6_part_sads = _mm256_slli_si256(ymm6_part_sads, 2);
  ymm6_part_sads = _mm256_blend_epi16(ymm6_part_sads, ymm4_tmp, 17); //  partial sums 2 of 8 in hi and low 128bits

  ymm0_Ref_01 = _mm256_srli_si256(ymm0_Ref_01, 1);
  ymm1_Ref_23 = _mm256_srli_si256(ymm1_Ref_23, 1);
  ymm2_Ref_45 = _mm256_srli_si256(ymm2_Ref_45, 1);
  ymm3_Ref_67 = _mm256_srli_si256(ymm3_Ref_67, 1);

  // process sad[1,-1]
  ymm4_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm0_Ref_01, 8), ymm1_Ref_23, 51), ymm10_Src_2031);
  ymm5_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm2_Ref_45, 8), ymm3_Ref_67, 51), ymm11_Src_6475);

  ymm4_tmp = _mm256_adds_epu16(ymm4_tmp, ymm5_tmp);
  // sad 1,-1 4 parts ready in low of mm4
  ymm6_part_sads = _mm256_slli_si256(ymm6_part_sads, 2);
  ymm6_part_sads = _mm256_blend_epi16(ymm6_part_sads, ymm4_tmp, 17); // partial sums 3 of 8 in hi and low 128bits

  //
  //2 row 
  //
  i = 1;
  ymm0_Ref_01 = _mm256_loadu2_m128i((__m128i*)(pucRef + nRefPitch[0] * (i + 1)), (__m128i*)(pucRef + nRefPitch[0] * (i + 0)));
  ymm1_Ref_23 = _mm256_loadu2_m128i((__m128i*)(pucRef + nRefPitch[0] * (i + 3)), (__m128i*)(pucRef + nRefPitch[0] * (i + 2)));
  ymm2_Ref_45 = _mm256_loadu2_m128i((__m128i*)(pucRef + nRefPitch[0] * (i + 5)), (__m128i*)(pucRef + nRefPitch[0] * (i + 4)));
  ymm3_Ref_67 = _mm256_loadu2_m128i((__m128i*)(pucRef + nRefPitch[0] * (i + 7)), (__m128i*)(pucRef + nRefPitch[0] * (i + 6)));
  // loaded 8 rows of Ref plane 16samples wide into ymm0..ymm3

  // process sad[-1,0]
  ymm4_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm0_Ref_01, 8), ymm1_Ref_23, 51), ymm10_Src_2031);
  ymm5_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm2_Ref_45, 8), ymm3_Ref_67, 51), ymm11_Src_6475);

  ymm4_tmp = _mm256_adds_epu16(ymm4_tmp, ymm5_tmp);
  // sad -1,0 4 parts ready in low of mm4
  ymm6_part_sads = _mm256_slli_si256(ymm6_part_sads, 2);
  ymm6_part_sads = _mm256_blend_epi16(ymm6_part_sads, ymm4_tmp, 17); // partial sums 4 of 8 in hi and low 128bits

  // process 4 partial sads in 2 partial sads
  ymm6_part_sads = _mm256_adds_epu16(_mm256_srli_si256(ymm6_part_sads, 8), ymm6_part_sads);
  ymm7_minsad8_1 = _mm256_blend_epi16(ymm7_minsad8_1, ymm6_part_sads, 15);

  ymm0_Ref_01 = _mm256_srli_si256(ymm0_Ref_01, 1);
  ymm1_Ref_23 = _mm256_srli_si256(ymm1_Ref_23, 1);
  ymm2_Ref_45 = _mm256_srli_si256(ymm2_Ref_45, 1);
  ymm3_Ref_67 = _mm256_srli_si256(ymm3_Ref_67, 1);

  // process sad[0,0]
  ymm4_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm0_Ref_01, 8), ymm1_Ref_23, 51), ymm10_Src_2031);
  ymm5_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm2_Ref_45, 8), ymm3_Ref_67, 51), ymm11_Src_6475);

  ymm4_tmp = _mm256_adds_epu16(ymm4_tmp, ymm5_tmp);
  // sad 0,0 4 parts ready in low of mm4
  ymm6_part_sads = _mm256_blend_epi16(ymm6_part_sads, ymm4_tmp, 17); // partial sums 5 of 8 in hi and low 128bits

  ymm0_Ref_01 = _mm256_srli_si256(ymm0_Ref_01, 1);
  ymm1_Ref_23 = _mm256_srli_si256(ymm1_Ref_23, 1);
  ymm2_Ref_45 = _mm256_srli_si256(ymm2_Ref_45, 1);
  ymm3_Ref_67 = _mm256_srli_si256(ymm3_Ref_67, 1);

  // process sad[1,0]
  ymm4_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm0_Ref_01, 8), ymm1_Ref_23, 51), ymm10_Src_2031);
  ymm5_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm2_Ref_45, 8), ymm3_Ref_67, 51), ymm11_Src_6475);

  ymm4_tmp = _mm256_adds_epu16(ymm4_tmp, ymm5_tmp);
  // sad 1,0 4 parts ready in low of mm4
  ymm6_part_sads = _mm256_slli_si256(ymm6_part_sads, 2);
  ymm6_part_sads = _mm256_blend_epi16(ymm6_part_sads, ymm4_tmp, 17); // partial sums 6 of 8 in hi and low 128bits

  //
  //3 row 
  //
  i = 2;
  ymm0_Ref_01 = _mm256_loadu2_m128i((__m128i*)(pucRef + nRefPitch[0] * (i + 1)), (__m128i*)(pucRef + nRefPitch[0] * (i + 0)));
  ymm1_Ref_23 = _mm256_loadu2_m128i((__m128i*)(pucRef + nRefPitch[0] * (i + 3)), (__m128i*)(pucRef + nRefPitch[0] * (i + 2)));
  ymm2_Ref_45 = _mm256_loadu2_m128i((__m128i*)(pucRef + nRefPitch[0] * (i + 5)), (__m128i*)(pucRef + nRefPitch[0] * (i + 4)));
  ymm3_Ref_67 = _mm256_loadu2_m128i((__m128i*)(pucRef + nRefPitch[0] * (i + 7)), (__m128i*)(pucRef + nRefPitch[0] * (i + 6)));
  // loaded 8 rows of Ref plane 16samples wide into ymm0..ymm3

  // process sad[-1,1]
  ymm4_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm0_Ref_01, 8), ymm1_Ref_23, 51), ymm10_Src_2031);
  ymm5_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm2_Ref_45, 8), ymm3_Ref_67, 51), ymm11_Src_6475);

  ymm4_tmp = _mm256_adds_epu16(ymm4_tmp, ymm5_tmp);
  // sad -1,1 ready in low of mm4
  ymm6_part_sads = _mm256_slli_si256(ymm6_part_sads, 2);
  ymm6_part_sads = _mm256_blend_epi16(ymm6_part_sads, ymm4_tmp, 17); // partial sums 7 of 8 in hi and low 128bits

  ymm0_Ref_01 = _mm256_srli_si256(ymm0_Ref_01, 1);
  ymm1_Ref_23 = _mm256_srli_si256(ymm1_Ref_23, 1);
  ymm2_Ref_45 = _mm256_srli_si256(ymm2_Ref_45, 1);
  ymm3_Ref_67 = _mm256_srli_si256(ymm3_Ref_67, 1);

  // process sad[0,1]
  ymm4_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm0_Ref_01, 8), ymm1_Ref_23, 51), ymm10_Src_2031);
  ymm5_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm2_Ref_45, 8), ymm3_Ref_67, 51), ymm11_Src_6475);

  ymm4_tmp = _mm256_adds_epu16(ymm4_tmp, ymm5_tmp);
  // sad 0,1 ready in low of mm4
  ymm6_part_sads = _mm256_slli_si256(ymm6_part_sads, 2);
  ymm6_part_sads = _mm256_blend_epi16(ymm6_part_sads, ymm4_tmp, 17); // partial sums 8 of 8 in hi and low 128bits

  ymm6_part_sads = _mm256_adds_epu16(_mm256_srli_si256(ymm6_part_sads, 8), ymm6_part_sads);
  ymm7_minsad8_1 = _mm256_slli_si256(ymm7_minsad8_1, 8);
  ymm7_minsad8_1 = _mm256_blend_epi16(ymm7_minsad8_1, ymm6_part_sads, 15);

  ymm7_minsad8_1 = _mm256_adds_epu16(_mm256_castsi128_si256(_mm256_extracti128_si256(ymm7_minsad8_1, 1)), ymm7_minsad8_1);   // minsad8_1 ready
  
  ymm0_Ref_01 = _mm256_srli_si256(ymm0_Ref_01, 1);
  ymm1_Ref_23 = _mm256_srli_si256(ymm1_Ref_23, 1);
  ymm2_Ref_45 = _mm256_srli_si256(ymm2_Ref_45, 1);
  ymm3_Ref_67 = _mm256_srli_si256(ymm3_Ref_67, 1);

  // process sad[1,1]
  ymm4_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm0_Ref_01, 8), ymm1_Ref_23, 51), ymm10_Src_2031);
  ymm5_tmp = _mm256_sad_epu8(_mm256_blend_epi32(_mm256_slli_si256(ymm2_Ref_45, 8), ymm3_Ref_67, 51), ymm11_Src_6475);

  ymm4_tmp = _mm256_adds_epu16(ymm4_tmp, ymm5_tmp);
  // sad 1,1 4 parts ready in low of mm4
  ymm6_part_sads = _mm256_adds_epu16(_mm256_srli_si256(ymm6_part_sads, 8), ymm6_part_sads);
  ymm6_part_sads = _mm256_adds_epu16(_mm256_castsi128_si256(_mm256_extracti128_si256(ymm6_part_sads, 1)), ymm6_part_sads);


  unsigned short minsad = 65535;
  int idx_min_sad = 0;

  unsigned int uiSADRes1 = _mm_cvtsi128_si32(_mm_minpos_epu16(_mm256_castsi256_si128(ymm7_minsad8_1)));

  if ((unsigned short)uiSADRes1 < minsad)
  {
    minsad = (unsigned short)uiSADRes1;
    idx_min_sad = 7 - (uiSADRes1 >> 16);
  }

  if ((unsigned short)_mm_cvtsi128_si32(_mm256_castsi256_si128(ymm6_part_sads)) < minsad)
  {
    minsad = (unsigned short)_mm_cvtsi128_si32(_mm256_castsi256_si128(ymm6_part_sads));
    idx_min_sad = 8;
  }

  //  x_minsad = (idx_min_sad % 3) - 1; - just comment where from x,y minsad come from
  //  y_minsad = (idx_min_sad / 3) - 1;

  sad_t cost = minsad + ((penaltyNew * minsad) >> 8);
  if (cost >= workarea.nMinCost) return;

  workarea.bestMV.x = mvx + (idx_min_sad % 3) - 1;
  workarea.bestMV.y = mvy + (idx_min_sad / 3) - 1;
  workarea.nMinCost = cost;
  workarea.bestMV.sad = minsad;

  _mm256_zeroupper();

}

