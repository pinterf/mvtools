// I (Fizick) borrow this fast code from great SimpleResize Avisynth plugin by Tom Barry.
// But it seems, the code tables were wrong: edge pixels was resized to new egde pixels,
// So scale was (newsize-1)/(oldsize-1), but must be (newsize/oldsize). 
// May be it was not a bug, but feature. I fixed it for using in MVTools.
// Chroma handling of rightmost pixels was not quite correct too.
// Now results is exactly same as Avisynth BilinearResize function.
// Copyright(c)2005 A.G.Balakhnin aka Fizick
//------------------------------------------------------------------------
//
//  Simple (faster) resize for avisynth
//	Copyright (C) 2002 Tom Barry
//
//	This program is free software; you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation; either version 2 of the License, or
//	(at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with this program; if not, write to the Free Software
//	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//  
//  Also, this program is "Philanthropy-Ware".  That is, if you like it and 
//  feel the need to reward or inspire the author then please feel free (but
//  not obligated) to consider joining or donating to the Electronic Frontier
//  Foundation. This will help keep cyber space free of barbed wire and bullsh*t.  
//
//  See their web page at www.eff.org


#include "SimpleResize.h"

#define	NOGDI
#define	NOMINMAX
#define	WIN32_LEAN_AND_MEAN
#include "Windows.h"
#include	"avisynth.h"
#include	"malloc.h"
#include <emmintrin.h>
#include <smmintrin.h>
#include "def.h"
#include <algorithm>    // std::max

#if !(defined(_M_X64))
//#define OLD_ASM // for testing similarity with old code P.F. 161204
#endif

#ifdef OLD_ASM
#if !defined(_M_X64)
#define rax	eax
#define rbx	ebx
#define rcx	ecx
#define rdx	edx
#define rsi	esi
#define rdi	edi
#define rbp	ebp
#define rsp	esp
#define movzx_int mov
#define movsx_int mov
#else
#define rax	rax
#define rbx	rbx
#define rcx	rcx
#define rdx	rdx
#define rsi	rsi
#define rdi	rdi
#define rbp	rbp
#define rsp	r15
#define movzx_int movzxd
#define movsx_int movsxd
#endif
#endif


SimpleResize::SimpleResize(int _newwidth, int _newheight, int _oldwidth, int _oldheight, long CPUFlags)
{
  oldwidth = _oldwidth;
  oldheight = _oldheight;
  newwidth = _newwidth;
  newheight = _newheight;
  SSE2enabled = (CPUFlags & CPUF_SSE2) != 0;
//  SSEMMXenabled = (CPUFlags& CPUF_INTEGER_SSE) != 0;

  // 2 qwords, 2 offsets, and prefetch slack
  hControl = (unsigned int*)_aligned_malloc(newwidth * 12 + 128, 128);   // aligned for P4 cache line
  vWorkY = (unsigned char*)_aligned_malloc(2 * oldwidth + 128, 128); // for unsigned char type resize
  vOffsets = (unsigned int*)_aligned_malloc(newheight * 4, 128);
  vWeights = (unsigned int*)_aligned_malloc(newheight * 4, 128);

  vWorkY2 = (short*)_aligned_malloc(2 * oldwidth + 128, 128); // for short type resize

  //		if (!hControl || !vWeights)
  {
    //			env->ThrowError("SimpleResize: memory allocation error");
  }

  InitTables();
};

SimpleResize::~SimpleResize()
{
  _aligned_free(hControl);
  _aligned_free(vWorkY);
  _aligned_free(vWorkY2);
  _aligned_free(vOffsets);
  _aligned_free(vWeights);
}

template<int cpuflags>
static MV_FORCEINLINE __m128i simd_blend_epi8(__m128i const &selector, __m128i const &a, __m128i const &b) {
  if (cpuflags >= CPUF_SSE4_1) {
    return _mm_blendv_epi8(b, a, selector);
  }
  else {
    return _mm_or_si128(_mm_and_si128(selector, a), _mm_andnot_si128(selector, b));
  }
}

// Intrinsics by PF
// unsigned char
// or short
// or 8->16 bits: for MMask, scaling 8 bit masks to 10-16 bits

// for non 16->16 bit: limitIt = false, nPelLog1 and isXpart are n/a (e.g. 0, true)
template<typename src_type, typename dst_type, bool limitIt, int nPel, bool isXpart>
void SimpleResize::SimpleResizeDo_New(uint8_t *dstp8, int row_size, int height, int dst_pitch,
  const uint8_t* srcp8, int src_row_size, int src_pitch, int bits_per_pixel, int real_width, int real_height)
{
  dst_type *dstp = reinterpret_cast<dst_type *>(dstp8);
  const src_type *srcp = reinterpret_cast<const src_type *>(srcp8);
  // Note: PlanarType is dummy, I (Fizick) do not use croma planes code for resize in MVTools
  /*
  SetMemoryMax(6000)
  a=Avisource("c:\tape13\Videos\Tape02_digitall_Hi8.avi").assumefps(25,1).trim(0, 499)
  a=a.AssumeBFF()
  a=a.QTGMC(Preset="Slower",dct=5, ChromaMotion=true)
  sup = a.MSuper(pel=1)
  fw = sup.MAnalyse(isb=false, delta=1, overlap=4)
  bw = sup.MAnalyse(isb=true, delta=1, overlap=4)
  a=a.MFlowInter(sup, bw, fw, time=50, thSCD1=400) # MFlowInter uses SimpleResize
  a
  */
  typedef src_type workY_type; // be the same
  const unsigned int* pControl = &hControl[0];

  const src_type * srcp1;
  const src_type * srcp2;
  workY_type * vWorkYW = sizeof(workY_type) == 1 ? (workY_type *)vWorkY : (workY_type *)vWorkY2;

  unsigned int* vOffsetsW = vOffsets;
  unsigned int* vWeightsW = vWeights;

  // variables for int16->int16 vector (X or Y) resize
  constexpr int nPelLog2 = nPel == 1 ? 0 : nPel == 2 ? 1 : nPel == 4 ? 2 : 0; // for !limitIt -> n/a
  // limitIt == true helpers
  __m128i minRelY, maxRelY; // used when we have Y parts of the vector
  int minRelY_c, maxRelY_c;
  const __m128i relInc =  isXpart ? _mm_set1_epi16(nPel * 4) : _mm_set1_epi16(nPel);
  // for X : simd code does 4 pixels at a time, decrement by 4 X-unit
  const int relInc_c = nPel; // no difference for X
  const __m128i maxRelXStart = _mm_set_epi16(
    ((real_width - 7) * nPel) - 1, // not used, only lower 4x16bit pixels are processed
    ((real_width - 6) * nPel) - 1, // not used
    ((real_width - 5) * nPel) - 1, // not used
    ((real_width - 4) * nPel) - 1, // not used
    ((real_width - 3) * nPel) - 1,
    ((real_width - 2) * nPel) - 1,
    ((real_width - 1) * nPel) - 1,
    ((real_width - 0) * nPel) - 1);
  const __m128i minRelXStart = _mm_set_epi16(
    -7 * nPel, // not used, only lower 4x16bit pixels are processed
    -6 * nPel, // not used
    -5 * nPel, // not used
    -4 * nPel, // not used
    -3 * nPel,
    -2 * nPel,
    -1 * nPel,
    0 * nPel);

  const int row_size_mod4 = row_size & ~3; // covered by SIMD code
  const int maxRelXStart_c = ((real_width - row_size_mod4) << nPelLog2) - 1;
  const int minRelXStart_c = (-row_size_mod4) << nPelLog2;
  
  // A note for int16->int16 version:
  // Vectors originated from outside [(real_width << nPelLog2) - 1; (real_height << nPelLog2)-1] sized frame
  // are not even used in FlowInterExtra and FlowInter, probably the should not even be calculated here?

  unsigned int last_vOffsetsW = vOffsetsW[height - 1];

  if constexpr (limitIt && !isXpart)
  {
    maxRelY_c = (real_height << nPelLog2) - 1;
    minRelY_c = 0;
    maxRelY = _mm_set1_epi16(maxRelY_c);
    minRelY = _mm_set1_epi16(0);
  }

  for (int y = 0; y < height; y++)
  {

    __m128i minRelX, maxRelX;
    int minRelX_c, maxRelX_c;

    if constexpr (limitIt && isXpart)
    {
      maxRelX = maxRelXStart;
      minRelX = minRelXStart;
      maxRelX_c = maxRelXStart_c;
      minRelX_c = minRelXStart_c;
    }

    int CurrentWeight = vWeightsW[y];
    int invCurrentWeight = 256 - CurrentWeight;

    srcp1 = srcp + vOffsetsW[y] * src_pitch;

    // When height /oldheight ratio is too big, (e.g. x10e0/0x438) then the (y-1)th offset is already the last row.
    // bug: we set the srcp2 pointer to the last row+1, thus resulting in access violator
    // Reason: before 2.7.14.22 checks for y-1, but condition may fail sooner
    //   vOffsetsW[0x10dd]	0x00000436
    //   vOffsetsW[0x10de]	0x00000437   <<- y-1, already the last, we cannot use text pitch 0x438!
    //   vOffsetsW[0x10df]	0x00000437
    // Bug was probably introduced in the original 2.5.11.22
    // bad: srcp2 = (y < height - 1) ? srcp1 + src_pitch : srcp1; // pitch is uchar/short-aware

    // scrp2 is the next line if applicable
    bool UseNextLine = vOffsetsW[y] < last_vOffsetsW;
    srcp2 = UseNextLine ? srcp1 + src_pitch : srcp; // pitch is uchar/short-aware

    int mod8or16w = src_row_size / (16 / sizeof(workY_type)) * (16 / sizeof(workY_type));
    __m128i FPround1 = _mm_set1_epi16(0x0080);
    __m128i FPround2 = _mm_set1_epi32(0x0080);
    // weights are 0..255 and (8 bit scaled) , rounding is 128 (0.5)
    __m128i weight1 = _mm_set1_epi16(invCurrentWeight); // _mm_loadu_si128(reinterpret_cast<const __m128i *>(vWeight1));
    __m128i weight2 = _mm_set1_epi16(CurrentWeight);    // _mm_loadu_si128(reinterpret_cast<const __m128i *>(vWeight2));
    __m128i zero = _mm_setzero_si128();
    for (int x = 0; x < src_row_size; x += (16 / sizeof(workY_type))) {
      // top of 2 lines to interpolate
      // load or loadu?
      __m128i src1, src2, result;
      if (sizeof(src_type) == 1 && sizeof(workY_type) == 2) {
        src1 = _mm_loadl_epi64(reinterpret_cast<const __m128i *>((src_type *)srcp1 + x));
        src2 = _mm_loadl_epi64(reinterpret_cast<const __m128i *>((src_type *)srcp2 + x)); // 2nd of 2 lines
        src1 = _mm_unpacklo_epi8(src1, zero); // make words
        src2 = _mm_unpackhi_epi8(src2, zero);
        // we have 8 words 
      }
      else {
        // unaligned! src_pitch is not 16 byte friendly
        src1 = _mm_loadu_si128(reinterpret_cast<const __m128i *>((src_type *)srcp1 + x));
        src2 = _mm_loadu_si128(reinterpret_cast<const __m128i *>((src_type *)srcp2 + x)); // 2nd of 2 lines
      }
      if (sizeof(src_type) == 1 && sizeof(workY_type) == 1) {
        __m128i src1_lo = _mm_unpacklo_epi8(src1, zero); // make words
        __m128i src1_hi = _mm_unpackhi_epi8(src1, zero);
        __m128i src2_lo = _mm_unpacklo_epi8(src2, zero);
        __m128i src2_hi = _mm_unpackhi_epi8(src2, zero);

        __m128i res1_lo = _mm_mullo_epi16(src1_lo, weight1); // mult by weighting factor 1
        __m128i res1_hi = _mm_mullo_epi16(src1_hi, weight1); // mult by weighting factor 1
        __m128i res2_lo = _mm_mullo_epi16(src2_lo, weight2); // mult by weighting factor 2
        __m128i res2_hi = _mm_mullo_epi16(src2_hi, weight2); // mult by weighting factor 2
        __m128i res_lo = _mm_srli_epi16(_mm_adds_epu16(_mm_add_epi16(res1_lo, res2_lo), FPround1), 8); // combine lumas low + round and right adjust luma
        __m128i res_hi = _mm_srli_epi16(_mm_adds_epu16(_mm_add_epi16(res1_hi, res2_hi), FPround1), 8); // combine lumas high + round and right
        result = _mm_packus_epi16(res_lo, res_hi);// pack words to our 16 byte answer
      }
      else {
     // workY_type == short
        __m128i res1_lo = _mm_mullo_epi16(src1, weight1); // mult by weighting factor 1
        __m128i res1_hi = _mm_mulhi_epi16(src1, weight1); // upper 32 bits of src1 * weight
        __m128i res2_lo = _mm_mullo_epi16(src2, weight2); // mult by weighting factor 2
        __m128i res2_hi = _mm_mulhi_epi16(src2, weight2); // upper 32 bits of src2 * weight
                                                          // combine 16lo 16hi results to 32 bit result
        __m128i mulres1_lo = _mm_unpacklo_epi16(res1_lo, res1_hi); //  src1 0..3   4x32 bit result of multiplication as 32 bit
        __m128i mulres1_hi = _mm_unpackhi_epi16(res1_lo, res1_hi); //       4..7   4x32 bit result of multiplication as 32 bit
        __m128i mulres2_lo = _mm_unpacklo_epi16(res2_lo, res2_hi); //  src2 0..3   4x32 bit result of multiplication as 32 bit
        __m128i mulres2_hi = _mm_unpackhi_epi16(res2_lo, res2_hi); //       4..7   4x32 bit result of multiplication as 32 bit

        __m128i res_lo = _mm_srai_epi32(_mm_add_epi32(_mm_add_epi32(mulres1_lo, mulres2_lo), FPround2), 8); // combine lumas + dword rounder 0x00000080
        __m128i res_hi = _mm_srai_epi32(_mm_add_epi32(_mm_add_epi32(mulres1_hi, mulres2_hi), FPround2), 8); // combine lumas + dword rounder 0x00000080;

        result = _mm_packs_epi32(res_lo, res_hi); // pack into 8 words
      }
      // workY is well aligned (even 128 bytes)
      _mm_stream_si128(reinterpret_cast<__m128i *>((workY_type *)vWorkYW + x), result); // movntdq don't pollute cache
    }
    // the rest one-by-one
    for (int x = mod8or16w; x < src_row_size; x++) {
      vWorkYW[x] = (srcp1[x] * invCurrentWeight + srcp2[x] * CurrentWeight + 128) >> 8;
    }

    // We've taken care of the vertical scaling, now do horizontal
    // rework
    // We've taken care of the vertical scaling, now do horizontal
    // vvv for src_type char it can be loop by 8
    for (int x = 0; x < row_size_mod4; x += 4) {
#ifdef USE_SSE2_HORIZONTAL_HELPERS        
      unsigned int pc0, pc1, pc2, pc3;
      short b0, b1, b2, b3, b4, b5, b6, b7;
#endif
      unsigned int offs0, offs1, offs2, offs3;

      // mov		eax, [rsi + 16]
      offs0 = pControl[3 * x + 4];   // get data offset in pixels, 1st pixel pair. Control[4]=offs
                                     // mov		ebx, [rsi + 20]
      offs1 = pControl[3 * x + 5];   // get data offset in pixels, 2nd pixel pair. Control[5]=offsodd

      offs2 = pControl[3 * x + 4 + (2 * 3)];   // get data offset in pixels, 3nd pixel pair. Control[5]=offsodd
      offs3 = pControl[3 * x + 5 + (2 * 3)];   // get data offset in pixels, 4nd pixel pair. Control[5]=offsodd

                                             /*source_t*/
                                             // (uint32_t)pair1     = (uint32_t *)((short *)(vWorkYW)[offs])      // vWorkYW[offs] and vWorkYW[offs+1]
                                             // (uint32_t)pair2odds = (uint32_t *)((short *)(vWorkYW)[offsodds])  // vWorkYW[offsodds] and vWorkYW[offsodds+1]
      __m128i a_pair3210;
      uint32_t pair0, pair1, pair2, pair3;
      if (sizeof(workY_type) == 1) { // vWorkYW uchars
        uint16_t pair0_2x8 = *(uint16_t *)(&vWorkYW[offs0]);      // vWorkYW[offs] and vWorkYW[offs+1]
        uint16_t pair1_2x8 = *(uint16_t *)(&vWorkYW[offs1]);      // vWorkYW[offs] and vWorkYW[offs+1]
        uint16_t pair2_2x8 = *(uint16_t *)(&vWorkYW[offs2]);      // vWorkYW[offs] and vWorkYW[offs+1]
        uint16_t pair3_2x8 = *(uint16_t *)(&vWorkYW[offs3]);      // vWorkYW[offs] and vWorkYW[offs+1]
        pair0 = (((uint32_t)pair0_2x8 & 0xFF00) << 8) + (pair0_2x8 & 0xFF);
        pair1 = (((uint32_t)pair1_2x8 & 0xFF00) << 8) + (pair1_2x8 & 0xFF);
        pair2 = (((uint32_t)pair2_2x8 & 0xFF00) << 8) + (pair2_2x8 & 0xFF);
        pair3 = (((uint32_t)pair3_2x8 & 0xFF00) << 8) + (pair3_2x8 & 0xFF);
        __m128i a_pair10 = _mm_or_si128(_mm_shuffle_epi32(_mm_cvtsi32_si128(pair1), _MM_SHUFFLE(0, 0, 0, 1)), _mm_cvtsi32_si128(pair0));
        __m128i a_pair32 = _mm_or_si128(_mm_shuffle_epi32(_mm_cvtsi32_si128(pair3), _MM_SHUFFLE(0, 0, 0, 1)), _mm_cvtsi32_si128(pair2));
        a_pair3210 = _mm_unpacklo_epi64(a_pair10, a_pair32);

      }
      else { // vWorkYW shorts
        pair0 = *(uint32_t *)(&vWorkYW[offs0]);      // vWorkYW[offs] and vWorkYW[offs+1]
        pair1 = *(uint32_t *)(&vWorkYW[offs1]);      // vWorkYW[offs] and vWorkYW[offs+1]
        pair2 = *(uint32_t *)(&vWorkYW[offs2]);      // vWorkYW[offs] and vWorkYW[offs+1]
        pair3 = *(uint32_t *)(&vWorkYW[offs3]);      // vWorkYW[offs] and vWorkYW[offs+1]
        __m128i a_pair10 = _mm_or_si128(_mm_shuffle_epi32(_mm_cvtsi32_si128(pair1), _MM_SHUFFLE(0, 0, 0, 1)), _mm_cvtsi32_si128(pair0));
        __m128i a_pair32 = _mm_or_si128(_mm_shuffle_epi32(_mm_cvtsi32_si128(pair3), _MM_SHUFFLE(0, 0, 0, 1)), _mm_cvtsi32_si128(pair2));
        a_pair3210 = _mm_unpacklo_epi64(a_pair10, a_pair32);
      }

#ifdef USE_SSE2_HORIZONTAL_HELPERS        
      pc0 = pControl[3 * x + 0];         // uint32: Y2_0:Y1_0
      pc1 = pControl[3 * x + 1];         // uint32: Y2_1:Y1_1 
      pc2 = pControl[3 * x + 0 + (2 * 3)]; // uint32: Y2_2:Y1_2 
      pc3 = pControl[3 * x + 1 + (2 * 3)]; // uint32: Y2_3:Y1_3 
#endif

      // pc1, pc0
      __m128i b_pair10 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(&pControl[3 * x + 0])); // +0, +1
      // load the lower 64 bits from (unaligned)
      // pc3, pc2
      __m128i b_pair3210 = _mm_castpd_si128(_mm_loadh_pd(_mm_castsi128_pd(b_pair10), reinterpret_cast<const double*>(&pControl[3 * x + 0 + (2 * 3)])));
      // load the higher 64 bits from 2nd param, lower 64 bit copied from 1st param

#ifdef USE_SSE2_HORIZONTAL_HELPERS        
      unsigned int wY1_0 = b0 = pc0 & 0x0000ffff; //low
      unsigned int wY2_0 = b1 = pc0 >> 16; //high
      unsigned int wY1_1 = b2 = pc1 & 0x0000ffff; //low
      unsigned int wY2_1 = b3 = pc1 >> 16; //high
      unsigned int wY1_2 = b4 = pc2 & 0x0000ffff; //low
      unsigned int wY2_2 = b5 = pc2 >> 16; //high
      unsigned int wY1_3 = b6 = pc3 & 0x0000ffff; //low
      unsigned int wY2_3 = b7 = pc3 >> 16; //high
      short a0 = vWorkYW[offs0]; // 1 or 2 byte
      short a1 = vWorkYW[offs0 + 1];
      short a2 = vWorkYW[offs1];
      short a3 = vWorkYW[offs1 + 1];
      short a4 = vWorkYW[offs2];
      short a5 = vWorkYW[offs2 + 1];
      short a6 = vWorkYW[offs3];
      short a7 = vWorkYW[offs3 + 1];

      BYTE res0 = (a0 * b0 + a1 * b1 + 128) >> 8;
      BYTE res1 = (a2 * b2 + a3 * b3 + 128) >> 8;
      BYTE res2 = (a4 * b4 + a5 * b5 + 128) >> 8;
      BYTE res3 = (a6 * b6 + a7 * b7 + 128) >> 8;
      BYTE cmp0 = dstp[x + 0];
      BYTE cmp1 = dstp[x + 1];
      BYTE cmp2 = dstp[x + 2];
      BYTE cmp3 = dstp[x + 3];
      /*
      dstp[x+0] = res0;
      dstp[x+1] = res1;
      dstp[x+2] = res2;
      dstp[x+3] = res3;
      */
#endif
      // pmaddwd mm0, [rsi]			// mult and sum lumas by ctl weights. v1v0 Control[1] and w1w0 Control[0]
      // __m128i _mm_madd_epi16 (__m128i a, __m128i b);
      // PMADDWD
      // r0 := (a0 * b0) + (a1 * b1)
      // r1 := (a2 * b2) + (a3 * b3)
      // r2 := (a4 * b4) + (a5 * b5)
      // r3 := (a6 * b6) + (a7 * b7)
      __m128i result = _mm_madd_epi16(a_pair3210, b_pair3210); // 4x integer
      __m128i rounder = _mm_set1_epi32(0x0080);
      result = _mm_add_epi32(result, rounder);

      if (sizeof(src_type) == 1)
        result = _mm_srli_epi32(result, 8);
      else
        result = _mm_srai_epi32(result, 8);  // 16 bit: arithmetic shift as in orig asm
      result = _mm_packs_epi32(result, result); // 4xqword ->4xword
      if (sizeof(src_type) == 1 && sizeof(dst_type) == 1) {
        // 8 bit version
        result = _mm_packus_epi16(result, result); // 4xword ->4xbyte
        uint32_t final4x8bitpixels = _mm_cvtsi128_si32(result);
        *(uint32_t *)(&dstp[x]) = final4x8bitpixels;
      }
      else if (sizeof(src_type) == 1 && sizeof(dst_type) == 2) {
        // 8 bit mask to 16 bit clip (MMask)
        // We have 4 words here, but range is 0..255
        // difference from 8bits->8bits: data should be converted to bits_per_pixel uint16_size
        auto mask_max = _mm_cmpgt_epi16(_mm_set1_epi16(0x00FF), result); // (255 > b0) ? 0xffff : 0x0
        result = _mm_slli_epi16(result, bits_per_pixel - 8); // scale 8 bits to bits_per_pixel
        // ensure that over max mask value returns 16 bit max_pixel_value
        // original values==255 have to be max_pixel_value, shift is not exact enough
        // this could be CPUF_SSE4_1, but we have only SSE2 here
        result = simd_blend_epi8<CPUF_SSE2>(mask_max, result, _mm_set1_epi16((1 << bits_per_pixel) - 1));
        _mm_storel_epi64(reinterpret_cast<__m128i *>(&dstp[x]), result);

      }
      else {
        // for short to short version: 
        if constexpr (limitIt) {
          if constexpr (isXpart)
          {
            result = _mm_max_epi16(_mm_min_epi16(result, maxRelX), minRelX);
            maxRelX = _mm_sub_epi16(maxRelX, relInc); // 4 pixels at a time, decrement by 4 X-unit
            minRelX = _mm_sub_epi16(minRelX, relInc); // 4 pixels at a time, decrement by 4 X-unit;
          }
          else
          {
            result = _mm_max_epi16(_mm_min_epi16(result, maxRelY), minRelY);
          }
        }
        _mm_storel_epi64(reinterpret_cast<__m128i *>(&dstp[x]), result);
      }
    }
    // remaining
    for (int x = row_size_mod4; x < row_size; x++) {
      unsigned int pc;
      unsigned int offs;

      if ((x & 1) == 0) { // even
        pc = pControl[3 * x];
        offs = pControl[3 * x + 4];
      }
      else { // odd
        pc = pControl[3 * x - 2]; // [3*xeven+1]
        offs = pControl[3 * x + 2]; // [3*xeven+5]
      }
      unsigned int wY1 = pc & 0x0000ffff; //low
      unsigned int wY2 = pc >> 16; //high
      if (sizeof(src_type) == 1 && sizeof(dst_type) == 2) {
        // 8 to 16 bits
        int val = ((vWorkYW[offs] * wY1 + vWorkYW[offs + 1] * wY2 + 128) >> 8);
        if (val >= 255)
          dstp[x] = (1 << bits_per_pixel) - 1;
        else
          dstp[x] = val << (bits_per_pixel - 8);

      }
      else {
        int result = (vWorkYW[offs] * wY1 + vWorkYW[offs + 1] * wY2 + 128) >> 8;
        // resize of signed 16 bit vectors needs limiting
        if constexpr (limitIt) {
          if constexpr (isXpart)
          {
            result = std::max(std::min(result, maxRelX_c), minRelX_c);
            maxRelX_c -= relInc_c;
            minRelX_c -= relInc_c;
          }
          else
          {
            result = std::max(std::min(result, maxRelY_c), minRelY_c);
          }
        }
        dstp[x] = result;
      }
    }

    if constexpr (limitIt)
    {
      if constexpr (!isXpart) {
        maxRelY = _mm_sub_epi16(maxRelY, relInc);
        minRelY = _mm_sub_epi16(minRelY, relInc);
        maxRelY_c -= relInc_c;
        minRelY_c -= relInc_c;
      }
    }
    dstp += dst_pitch;

  } // for y
}

void SimpleResize::SimpleResizeDo_uint8_to_uint16(uint8_t *dstp, int row_size, int height, int dst_pitch,
  const uint8_t* srcp, int src_row_size, int src_pitch, int bits_per_pixel) {
  dst_pitch /= sizeof(uint16_t);  // pitch from byte granularity to uint16 for SimpleResizeDo
  SimpleResizeDo_New<uint8_t, uint16_t, false, 0, true>(dstp, row_size, height, dst_pitch, srcp, src_row_size, src_pitch, bits_per_pixel,
    row_size, height); // n/a: no limiting in 8->16;
  return;
}

void SimpleResize::SimpleResizeDo_uint8(uint8_t *dstp, int row_size, int height, int dst_pitch,
  const uint8_t* srcp, int src_row_size, int src_pitch)
{

  if (SSE2enabled) {
    SimpleResizeDo_New<uint8_t, uint8_t, false, 0, true>(dstp, row_size, height, dst_pitch, srcp, src_row_size, src_pitch, 8, 
      row_size, height); // n/a: no limiting in 8->8;
    return;
  }
  // C-only
  typedef unsigned char src_type;
  
  typedef src_type workY_type; // be the same

  const unsigned int* pControl = &hControl[0];

  const src_type * srcp1;
  const src_type * srcp2;
  workY_type * vWorkYW = sizeof(workY_type) == 1 ? (workY_type *)vWorkY : (workY_type *)vWorkY2;

  unsigned int* vOffsetsW = vOffsets;
  unsigned int* vWeightsW = vWeights;

  unsigned int last_vOffsetsW = vOffsetsW[height - 1];

  for (int y = 0; y < height; y++)
  {
    int CurrentWeight = vWeightsW[y];
    int invCurrentWeight = 256 - CurrentWeight;

    srcp1 = srcp + vOffsetsW[y] * src_pitch;

    // srcp2 is the next line to srcp1
    // fix in 2.7.14.22: srcp2 may reach beyond block
    // see comment in SimpleResizeDo_New
    bool UseNextLine = vOffsetsW[y] < last_vOffsetsW;
    srcp2 = UseNextLine ? srcp1 + src_pitch : srcp; // pitch is uchar/short-aware

    // recovered (and commented) C version as sort of doc
    for (int x = 0; x < src_row_size; x++) {
      vWorkYW[x] = (srcp1[x] * invCurrentWeight + srcp2[x] * CurrentWeight + 128) >> 8;
    }
    // We've taken care of the vertical scaling, now do horizontal
    for (int x = 0; x < row_size; x++) {
      unsigned int pc;
      unsigned int offs;

      // eax: get data offset in pixels, 1st pixel pair. Control[4]=offs
      if ((x & 1) == 0) { // even
        pc = pControl[3 * x];
        offs = pControl[3 * x + 4];
      }
      else { // odd
        pc = pControl[3 * x - 2]; // [3*xeven+1]
        offs = pControl[3 * x + 2]; // [3*xeven+5]
      }
      unsigned int wY1 = pc & 0x0000ffff; //low
      unsigned int wY2 = pc >> 16; //high
      dstp[x] = (vWorkYW[offs] * wY1 + vWorkYW[offs + 1] * wY2 + 128) >> 8;
    }
    dstp += dst_pitch;
  }

}

static void MakeVectorsSafe_c(short *dstp, int row_size, int height, int dst_pitch, int nPel, bool isXpart)
{
  const int nPelLog2 = nPel == 1 ? 0 : nPel == 2 ? 1 : nPel == 4 ? 2 : 0; // convert to needed shift count
  if (isXpart) {
    // dealing with the horizontal part of motion vectors
    short *VXFull = dstp;
    const int width = row_size;
    for (int h = 0; h < height; h++)
    {
      // todo: what about the vectors in a future 8K era?! with nPel=4 (<<2) they may not be safe anymore
      short maxRelX = ((width - 0) << nPelLog2) - 1;
      short minRelX = 0;
      short diff = 1 << nPelLog2;
      for (int w = 0; w < row_size; w++)
      {
        /*
        int rel_x = VXFull[w];
        short maxRelX = ((width - w) << nPelLog2) - 1;
        short minRelX = -(w << nPelLog2);
        if (rel_x > maxRelX)
          VXFull[w] = maxRelX; // dont reach out on the right
        else if (rel_x < minRelX)
          VXFull[w] = minRelX; // dont reach out on the left
          */
        VXFull[w] = std::max(std::min(VXFull[w], maxRelX), minRelX);
        minRelX -= diff;
        maxRelX -= diff;
      }
      VXFull += dst_pitch;
    }
  }
  else {
    // dealing with the vertical part of motion vectors
    short *VYFull = dstp;
    const int width = row_size;

    short maxRelY = ((height - 0) << nPelLog2) - 1;
    short minRelY = 0;
    short diff = 1 << nPelLog2;

    for (int h = 0; h < height; h++)
    {
      //short maxRelY = ((height - h) << nPelLog2) - 1;
      //short minRelY = -(h << nPelLog2);
      for (int w = 0; w < width; w++)
      {
        /*
        int rel_y = VYFull[w];
        if (rel_y > maxRelY)
          VYFull[w] = maxRelY; // dont reach out downward
        else if (rel_y < minRelY)
          VYFull[w] = minRelY; // dont reach out upward
          */
        VYFull[w] = std::max(std::min(VYFull[w], maxRelY), minRelY);
      }
      minRelY -= diff;
      maxRelY -= diff;
      VYFull += dst_pitch;
    }
  }
}

// brand new in 2.5.11.2 -> 2.5.11.22
// bilinear resizer for vectors as short integer
// 20181111: In a previous attempt we used MakeVFullSafe<NPELL2> (v2.7.33-) just before the usage of this resized vector set (MFlowXXX).
// It prevented access violation by limiting vectors pointing out of frame area
// limit x and y to prevent overflow in pel ref frame indexing
// valid pref[x;y] is [0..(height<<nLogPel)-1 ; 0..(width<<nLogPel)-1]
// invalid indexes appeared when enlarging small vector mask to full mask

// Vector validity was checked and were made safe by limiting them, since during the resize operation some vectors can point out of the valid frame dimensions.
// When enlarging the X part of the vectors, they may point to negative X positions on the left or beyond the rightmost pixel on the right.
// When enlarging the Y part of the vectors, they may point to negative Y positions at the top or below the most bottom line
// These vectors when converted to source pixel memory addresses will couse out-of-buffer access violation exceptions.
// Though the vectors are enlarged to frame size, they may point to an enlarged (frame size << nPel) reference frame, the limits should
// take nPel into account.
void SimpleResize::SimpleResizeDo_int16(short *dstp, int row_size, int height, int dst_pitch,
  const short* srcp, int src_row_size, int src_pitch, int nPel, bool isXpart, int real_width, int real_height)
{
  const bool limitVectors = true;

  if (SSE2enabled) {
    if (limitVectors)
    {
      if (isXpart) {
        if (nPel == 1)
          SimpleResizeDo_New<short, short, true, 1, true>((uint8_t *)dstp, row_size, height, dst_pitch, (uint8_t *)srcp, src_row_size, src_pitch, 16, real_width, real_height);
        else if (nPel == 2)
          SimpleResizeDo_New<short, short, true, 2, true>((uint8_t *)dstp, row_size, height, dst_pitch, (uint8_t *)srcp, src_row_size, src_pitch, 16, real_width, real_height);
        else if (nPel == 4)
          SimpleResizeDo_New<short, short, true, 4, true>((uint8_t *)dstp, row_size, height, dst_pitch, (uint8_t *)srcp, src_row_size, src_pitch, 16, real_width, real_height);
      }
      else {
        if (nPel == 1)
          SimpleResizeDo_New<short, short, true, 1, false>((uint8_t *)dstp, row_size, height, dst_pitch, (uint8_t *)srcp, src_row_size, src_pitch, 16, real_width, real_height);
        else if (nPel == 2)
          SimpleResizeDo_New<short, short, true, 2, false>((uint8_t *)dstp, row_size, height, dst_pitch, (uint8_t *)srcp, src_row_size, src_pitch, 16, real_width, real_height);
        else if (nPel == 4)
          SimpleResizeDo_New<short, short, true, 4, false>((uint8_t *)dstp, row_size, height, dst_pitch, (uint8_t *)srcp, src_row_size, src_pitch, 16, real_width, real_height);
      }
    }
    else
    {
      // we really do not have yet int16->int16 resize which needs no limiting, but for the sake of completeness
      SimpleResizeDo_New<short, short, false, 0, true>((uint8_t *)dstp, row_size, height, dst_pitch, (uint8_t *)srcp, src_row_size, src_pitch, 16, real_width, real_height);
    }
    
    // it's done in SimpleResizeDo_New sse2 version
    //if(limitVectors) MakeVectorsSafe_c(dstp, row_size, height, dst_pitch, nPel, isXpart, real_width, real_height);

    return;
  }
  
  // C version
  const unsigned int* pControl = &hControl[0];

  const  short* srcp1;
  const  short* srcp2;
  short* vWorkYW = vWorkY2;

  unsigned int* vOffsetsW = vOffsets;

  unsigned int* vWeightsW = vWeights;

  unsigned int last_vOffsetsW = vOffsetsW[height - 1];

  for (int y = 0; y < height; y++)
  {
    int CurrentWeight = vWeightsW[y];
    int invCurrentWeight = 256 - CurrentWeight;

    srcp1 = srcp + vOffsetsW[y] * src_pitch;

    // fix in 2.7.14.22: srcp2 may reach beyond block
    // see comment in SimpleResizeDo_New
    bool UseNextLine = vOffsetsW[y] < last_vOffsetsW;
    srcp2 = UseNextLine ? srcp1 + src_pitch : srcp; // pitch is uchar/short-aware

    // recovered (and commented) C version as sort of doc
    for (int x = 0; x < src_row_size; x++) {
      vWorkYW[x] = (srcp1[x] * invCurrentWeight + srcp2[x] * CurrentWeight + 128) >> 8;
    }
    // We've taken care of the vertical scaling, now do horizontal
    for (int x = 0; x < row_size; x++) {
      unsigned int pc;
      unsigned int offs;
      if ((x & 1) == 0) { // even
        pc = pControl[3 * x];
        offs = pControl[3 * x + 4];
      }
      else { // odd
        pc = pControl[3 * x - 2]; // [3*xeven+1]
        offs = pControl[3 * x + 2]; // [3*xeven+5]
      }
      unsigned int wY1 = pc & 0x0000ffff; //low
      unsigned int wY2 = pc >> 16; //high
      dstp[x] = (vWorkYW[offs] * wY1 + vWorkYW[offs + 1] * wY2 + 128) >> 8;
    }
    dstp += dst_pitch;
  }

  // for reference. We don't try to integrate it to the C code above
  if (limitVectors) MakeVectorsSafe_c(dstp, real_width, real_height, dst_pitch, nPel, isXpart); // use real_width, real_height instead of row_size, height

}


// YV12

// For each horizontal output pair of pixels there are 2 qword masks followed by 2 int
// offsets. The 2 masks are the weights to be used for the luma and chroma, respectively.
// Each mask contains LeftWeight1, RightWeight1, LeftWeight2, RightWeight2. So a pair of pixels
// will later be processed each pass through the horizontal resize loop.  I think with my
// current math the Horizontal Luma and Chroma contains the same values but since I may have screwed it
// up I'll leave it this way for now. Vertical chroma is different.

// Note - try just using the luma calcs for both, seem to be the same. (It was not correct for right edge! - Fizick)

// The weights are scaled 0-256 and the left and right weights will sum to 256 for each pixel.

void SimpleResize::InitTables(void)
{
  int i;
  int j;
  int k;
  int wY1;
  int wY2;

  // First set up horizontal table, use for both luma & chroma since 
  // it seems to have the same equation.
  // We will generate these values in pairs, mostly because that's the way
  // I wrote it for YUY2 above.

  for (i = 0; i < newwidth; i += 2)
  {
    // first make even pixel control
    j = i * 256 * (oldwidth) / (newwidth)+(128 * oldwidth) / newwidth - 128;//  wrong scale fixed by Fizick
    if (j < 0) j = 0; // added by Fizick

    k = j >> 8;
    wY2 = j - (k << 8);				// luma weight of right pixel
    wY1 = 256 - wY2;				// luma weight of left pixel

    if (k > oldwidth - 2)
    {
      hControl[i * 3 + 4] = oldwidth - 1;	 //	point to last byte
      hControl[i * 3] = 0x00000100;    // use 100% of rightmost Y
    }
    else
    {
      hControl[i * 3 + 4] = k;			// pixel offset
      hControl[i * 3] = wY2 << 16 | wY1; // luma weights
    }

    // now make odd pixel control
    j = (i + 1) * 256 * (oldwidth) / (newwidth)+(128 * oldwidth) / newwidth - 128;// wrong scale fixed by Fizick - 
    if (j < 0) j = 0; // added by Fizick

    k = j >> 8;
    wY2 = j - (k << 8);				// luma weight of right pixel
    wY1 = 256 - wY2;				// luma weight of left pixel

    if (k > oldwidth - 2)
    {
      hControl[i * 3 + 5] = oldwidth - 1;	 //	point to last byte
      hControl[i * 3 + 1] = 0x00000100;    // use 100% of rightmost Y
    }
    else
    {
      hControl[i * 3 + 5] = k;			// pixel offset
      hControl[i * 3 + 1] = wY2 << 16 | wY1; // luma weights
    }
  }

  hControl[newwidth * 3 + 4] = 2 * (oldwidth - 1);		// give it something to prefetch at end
  hControl[newwidth * 3 + 5] = 2 * (oldwidth - 1);		// "

  // Next set up vertical tables. The offsets are measured in lines and will be mult
  // by the source pitch later .

  // For YV12 we need separate Luma and chroma tables
  // First Luma Table
  //
  /* PF todo check it is OK? (does not seem to be symmetric at the beginning and the end)
    vOffsetsW[0x0000]	0x00000000
    vOffsetsW[0x0001]	0x00000000
    vOffsetsW[0x0002]	0x00000000
    vOffsetsW[0x0003]	0x00000000
    vOffsetsW[0x0004]	0x00000000
    vOffsetsW[0x0005]	0x00000000
    vOffsetsW[0x0006]	0x00000001
    vOffsetsW[0x0007]	0x00000001
    vOffsetsW[0x0008]	0x00000001
    vOffsetsW[0x0009]	0x00000001
    vOffsetsW[0x000a]	0x00000002
    vOffsetsW[0x000b]	0x00000002
    vOffsetsW[0x000c]	0x00000002
    vOffsetsW[0x000d]	0x00000002
    vOffsetsW[0x000e]	0x00000003
    vOffsetsW[0x000f]	0x00000003
      ...
    vOffsetsW[0x10d9]	0x00000435
    vOffsetsW[0x10da]	0x00000436
    vOffsetsW[0x10db]	0x00000436
    vOffsetsW[0x10dc]	0x00000436
    vOffsetsW[0x10dd]	0x00000436
    vOffsetsW[0x10de]	0x00000437
    vOffsetsW[0x10df]	0x00000437
      */





  for (i = 0; i < newheight; ++i)
  {
    j = i * 256 * (oldheight) / (newheight)+(128 * oldheight) / newheight - 128; // Fizick
    if (j < 0) j = 0; // added by Fizick

    k = j >> 8;
    if (k > oldheight - 2) { k = oldheight - 1; j = k << 8; } // added by Fizick
    vOffsets[i] = k;
    wY2 = j - (k << 8);
    vWeights[i] = wY2;				// weight to give to 2nd line
  }
}
