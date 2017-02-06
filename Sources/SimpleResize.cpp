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

// Intrinsics by PF
// unsigned char
// or short
template<typename src_type>
void SimpleResize::SimpleResizeDo_New(uint8_t *dstp8, int row_size, int height, int dst_pitch,
  const uint8_t* srcp8, int src_row_size, int src_pitch)
{
  src_type *dstp = reinterpret_cast<src_type *>(dstp8);
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

  unsigned int last_vOffsetsW = vOffsetsW[height - 1];
  for (int y = 0; y < height; y++)
  {
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
    int row_size_mod4 = row_size & ~3;
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
      if (sizeof(src_type) == 1) {
        // 8 bit version
        result = _mm_packus_epi16(result, result); // 4xword ->4xbyte
        uint32_t final4x8bitpixels = _mm_cvtsi128_si32(result);
        *(uint32_t *)(&dstp[x]) = final4x8bitpixels;
      }
      else {
        // for short version: 
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
      dstp[x] = (vWorkYW[offs] * wY1 + vWorkYW[offs + 1] * wY2 + 128) >> 8;
    }
    // rework end

    dstp += dst_pitch;

  } // for y
}

// YV12 Luma, PF: not YV12 specific!
// srcp2 is the next line to srcp1
void SimpleResize::SimpleResizeDo_uint8(uint8_t *dstp, int row_size, int height, int dst_pitch,
  const uint8_t* srcp, int src_row_size, int src_pitch, int Planar_Type)
{
  // Note: PlanarType is dummy, I (Fizick) do not use croma planes code for resize in MVTools
/* test:
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

  bool use_c = false;
#ifndef OLD_ASM
  if (SSE2enabled) {
    SimpleResizeDo_New<uint8_t>(dstp, row_size, height, dst_pitch, srcp, src_row_size, src_pitch);
    return;
  }
#endif
  use_c = true; 
  typedef unsigned char src_type;
  // later for the other SimpleResizeDo for vectors this is unsigned short
  // typedef short src_type; 

  typedef src_type workY_type; // be the same

#ifdef OLD_ASM
  int vWeight1[4];
  int vWeight2[4];
  // not needed for short
  const __int64 FPround1[2] = { 0x0080008000800080,0x0080008000800080 }; // round words
                                                                         // needed for byte and short
  const __int64 FPround2[2] = { 0x0000008000000080,0x0000008000000080 };// round dwords
                                                                        // not needed for short
  const __int64 FPround4 = 0x0080008000800080;// round words
#endif

  const unsigned int* pControl = &hControl[0];

  const src_type * srcp1;
  const src_type * srcp2;
  workY_type * vWorkYW = sizeof(workY_type) == 1 ? (workY_type *)vWorkY : (workY_type *)vWorkY2;

  unsigned int* vOffsetsW = vOffsets;
  unsigned int* vWeightsW = vWeights;

#ifdef OLD_ASM
  bool	SSE2enabledW = SSE2enabled;		// in local storage for asm
  bool	SSEMMXenabledW = SSEMMXenabled;		// in local storage for asm
#endif
  // Just in case things are not aligned right, maybe turn off sse2

  unsigned int last_vOffsetsW = vOffsetsW[height - 1];

  for (int y = 0; y < height; y++)
  {
    int CurrentWeight = vWeightsW[y];
    int invCurrentWeight = 256 - CurrentWeight;

#ifdef OLD_ASM
    // fill 4x(16x2) bit for inline asm 
    // for intrinsic it is not needed anymore
    vWeight1[0] = vWeight1[1] = vWeight1[2] = vWeight1[3] =
      (invCurrentWeight << 16) | invCurrentWeight;

    vWeight2[0] = vWeight2[1] = vWeight2[2] = vWeight2[3] =
      (CurrentWeight << 16) | CurrentWeight;
#endif
    srcp1 = srcp + vOffsetsW[y] * src_pitch;

    // fix in 2.7.14.22: srcp2 may reach beyond block
    // see comment in SimpleResizeDo_New
    bool UseNextLine = vOffsetsW[y] < last_vOffsetsW;
    srcp2 = UseNextLine ? srcp1 + src_pitch : srcp; // pitch is uchar/short-aware

    if (use_c) // always C here
    {
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


    }
#ifdef OLD_ASM
    // inline asm ignored 
    else {
      // Do Vertical. First SSE2 (mod16) then MMX (mod8) then MMX (for last_pos-8)
      // This one already ported to intrinsics (SSE2 mod16) + rest C
      _asm
      {
        //emms // added by paranoid Fizick
        //push	ecx						// have to save this? Intel2017:should be commented out
        mov		ecx, src_row_size
        shr		ecx, 3					// 8 bytes a time
        mov		rsi, srcp1				// top of 2 src lines to get
        mov		rdx, srcp2				// next "
        mov		rdi, vWorkYW			// luma work destination line
        xor		rax, rax                // P.F. 16.04.28 should be rax here not eax (hack! define rax as eax in 32 bit) eax/rax is an indexer 
        xor   rbx, rbx                // P.F. 16.04.28 later we use only EBX but index with RBX

        // Let's check here to see if we are on a P4 or higher and can use SSE2 instructions.
        // This first loop is not the performance bottleneck anyway but it is trivial to tune
        // using SSE2 if we have proper alignment.

        test    SSE2enabledW, 1			// is SSE2 supported?
        jz		vMaybeSSEMMX				// n, can't do anyway

// if src_row_size < 16 jump to vMaybeSSEMMX
cmp     ecx, 2					// we have at least 16 byts, 2 qwords?
jl		vMaybeSSEMMX				// n, don't bother
// srcp1 and srcp2 should be 16 byte aligned else jump to vMaybeSSEMMX
mov		rbx, rsi
or rbx, rdx
test    rbx, 0xf				// both src rows 16 byte aligned?
jnz		vMaybeSSEMMX			// n, don't use sse2
// SSE2 part from here
shr		ecx, 1					// do 16 bytes at a time instead. ecx now src_row_size/16
dec		ecx						// jigger loop ct
align	16
movdqu  xmm0, FPround1 // { 0x0080008000800080,0x0080008000800080 }; // round words _mm_set1_epi16(0x0080)
movdqu	xmm5, vWeight1  // __m128i weight1_xmm5 = _mm_loadu_si128(vWeight1)
movdqu	xmm6, vWeight2  // __m128i weight2_xmm6 = _mm_loadu_si128(vWeight2)
pxor	xmm7, xmm7        // __m128i zero = _mm_setzero_si128()

align   16
vLoopSSE2_Fetch:
        prefetcht0[rsi + rax * 2 + 16]
          prefetcht0[rdx + rax * 2 + 16]

          // move unaligned
          vLoopSSE2 :
          // __mm128i src_lo_xmm1 = _mm_loadu_si128(src1 + x_eax)
          movdqu	xmm1, xmmword ptr[rsi + rax] // top of 2 lines to interpolate
            // __mm128i src_hi_xmm3 = _mm_loadu_si128(src2 + x_eax)
          movdqu	xmm3, xmmword ptr[rdx + rax] // 2nd of 2 lines
            // xmm2 = xmm1
            // xmm4 = xmm3
          movdqa  xmm2, xmm1
          movdqa  xmm4, xmm3
            // xmm1 = _mm_unpacklo_epi8(xmm1, zero)
            // xmm2 = _mm_unpackhi_epi8(xmm2, zero)
          punpcklbw xmm1, xmm7			// make words
          punpckhbw xmm2, xmm7			// "
            // xmm3 = _mm_unpacklo_epi8(xmm3, zero)
            // xmm4 = _mm_unpackhi_epi8(xmm4, zero)
          punpcklbw xmm3, xmm7			// "
          punpckhbw xmm4, xmm7			// "

            // xmm1 = _mm_mullo_epi16(xmm1, weight1_xmm5) // mult by weighting factor 1
            // xmm2 = _mm_mullo_epi16(xmm2, weight1_xmm5)
          pmullw	xmm1, xmm5				// mult by top weighting factor
          pmullw	xmm2, xmm5              // "
          // xmm3 = _mm_mullo_epi16(xmm3, weight2_xmm6) // mult by weighting factor 2
          // xmm4 = _mm_mullo_epi16(xmm4, weight2_xmm6)
          pmullw	xmm3, xmm6				// mult by bot weighting factor
          pmullw	xmm4, xmm6              // "
            // xmm1 = _mm_add_epi16(xmm1,xmm3) // combine lumas low
          paddw	xmm1, xmm3				// combine lumas low
            // xmm2 = _mm_add_epi16(xmm2,xmm4) // combine lumas high
          paddw	xmm2, xmm4				// combine lumas high
            // xmm1 = _mm_adds_epu16(xmm1, round_xmm0)
            // xmm2 = _mm_adds_epu16(xmm2, round_xmm0)
          paddusw	xmm1, xmm0				// round
          paddusw	xmm2, xmm0				// round
            // xmm1 = _mm_srli_epi16(xmm1, 8) // right adjust luma
            // xmm2 = _mm_srli_epi16(xmm2, 8)
          psrlw	xmm1, 8					// right adjust luma
          psrlw	xmm2, 8					// right adjust luma
            // xmm1 = _mm_packus_epi16(xmm1, xmm2)
          packuswb xmm1, xmm2				// pack words to our 16 byte answer
            // _mm_stream_si128(vWorkYW+eax, xmm1) // movntdq don't pollute cache
          movntdq	xmmword ptr[rdi + rax], xmm1	// save lumas in our work area
            //x_eax+=16
          lea     rax, [rax + 16]  // P.F. 16.04.28 ?should be rax here not eax (hack! define rax eax in 32 bit)
          dec		ecx						// was: src_row_size/16 loop until zero
          jg		vLoopSSE2_Fetch			// if not on last one loop, prefetch
          jz		vLoopSSE2				// or just loop, or not

    // done with our SSE2 fortified loop but we may need to pick up the spare change
          sfence
          mov		ecx, src_row_size		// get count again
          and		ecx, 0x0000000f			// just need mod 16
          movq	mm5, vWeight1
          movq	mm6, vWeight2
          movq	mm0, FPround1			// useful rounding constant
          shr		ecx, 3					// 8 bytes at a time, any?
          jz		MoreSpareChange			// n, did them all		


    // Let's check here to see if we are on a P2 or Athlon and can use SSEMMX instructions.
    // This first loop is not the performance bottleneck anyway but it is trivial to tune
    // using SSE if we have proper alignment.
        vMaybeSSEMMX:
        movq	mm5, vWeight1
          movq	mm6, vWeight2
          movq	mm0, FPround1			// useful rounding constant
          pxor	mm7, mm7
          test    SSEMMXenabledW, 1		// is SSE supported?
          jz		vLoopMMX				// n, can't do anyway
          dec     ecx						// jigger loop ctr

          align	16
          vLoopSSEMMX_Fetch:
        prefetcht0[rsi + rax + 8]
          prefetcht0[rdx + rax + 8]

          vLoopSSEMMX :
          movq	mm1, qword ptr[rsi + rax] // top of 2 lines to interpolate
          movq	mm3, qword ptr[rdx + rax] // 2nd of 2 lines
          movq	mm2, mm1				// copy top bytes
          movq	mm4, mm3				// copy 2nd bytes

          punpcklbw mm1, mm7				// make words
          punpckhbw mm2, mm7				// "
          punpcklbw mm3, mm7				// "
          punpckhbw mm4, mm7				// "

          pmullw	mm1, mm5				// mult by weighting factor
          pmullw	mm2, mm5				// mult by weighting factor
          pmullw	mm3, mm6				// mult by weighting factor
          pmullw	mm4, mm6				// mult by weighting factor

          paddw	mm1, mm3				// combine lumas
          paddw	mm2, mm4				// combine lumas

          paddusw	mm1, mm0				// round
          paddusw	mm2, mm0				// round

          psrlw	mm1, 8					// right adjust luma
          psrlw	mm2, 8					// right adjust luma

          packuswb mm1, mm2				// pack UV's into low dword

          movntq	qword ptr[rdi + rax], mm1	// save in our work area

          lea     rax, [rax + 8]      // P.F. 16.04.28 should be rax here not eax (hack! define rax eax in 32 bit)
          dec		ecx
          jg		vLoopSSEMMX_Fetch			// if not on last one loop, prefetch
          jz		vLoopSSEMMX				// or just loop, or not
          sfence
          jmp		MoreSpareChange			// all done with vertical

          align	16
          vLoopMMX:
        movq	mm1, qword ptr[rsi + rax] // top of 2 lines to interpolate
          movq	mm3, qword ptr[rdx + rax] // 2nd of 2 lines
          movq	mm2, mm1				// copy top bytes
          movq	mm4, mm3				// copy 2nd bytes

          punpcklbw mm1, mm7				// make words
          punpckhbw mm2, mm7				// "
          punpcklbw mm3, mm7				// "
          punpckhbw mm4, mm7				// "

          pmullw	mm1, mm5				// mult by weighting factor
          pmullw	mm2, mm5				// mult by weighting factor
          pmullw	mm3, mm6				// mult by weighting factor
          pmullw	mm4, mm6				// mult by weighting factor

          paddw	mm1, mm3				// combine lumas
          paddw	mm2, mm4				// combine lumas

          paddusw	mm1, mm0				// round
          paddusw	mm2, mm0				// round

          psrlw	mm1, 8					// right just 
          psrlw	mm2, 8					// right just 

          packuswb mm1, mm2				// pack UV's into low dword

          movq	qword ptr[rdi + rax], mm1	// save lumas in our work area

          lea     rax, [rax + 8]            // P.F. 16.04.28 should be rax here not eax (hack! define rax eax in 32 bit)
          loop	vLoopMMX

          // Trick!
          // Add a little code here to check if we have more pixels to do and, if so, make one
          // more pass thru vLoopMMX. We were processing in multiples of 8 pixels and alway have
          // an even number so there will never be more than 7 left. 
        MoreSpareChange:
        cmp		eax, src_row_size		// EAX! did we get them all // P.F. 16.04.28 should be rax here not eax (hack! define rax eax in 32 bit)
          jnl		DoHorizontal			// yes, else have 2 left
          mov		ecx, 1					// jigger loop ct
          mov		eax, src_row_size       // EAX! P.F. 16.04.28 should be rax here not eax (hack! define rax eax in 32 bit)
          sub		rax, 8					// back up to last 8 pixels // P.F. 16.04.28 should be rax here not eax (hack! define rax eax in 32 bit)
          jmp		vLoopMMX

          // Do vertical END This one already ported to intrinsics (SSE2 mod16) + rest C

//-----------------------------------------------------
         // We've taken care of the vertical scaling, now do horizontal
        DoHorizontal:
        pxor    mm7, mm7
          movq	mm6, FPround2		// useful rounding constant, dwords
          mov		rsi, pControl		// @ horiz control bytes			
          mov		ecx, row_size
          shr		ecx, 2				// 4 bytes a time, 4 pixels
          mov     rdx, vWorkYW		// our luma data
          mov		rdi, dstp			// the destination line
          test    SSEMMXenabledW, 1		// is SSE2 supported?
          jz		hLoopMMX				// n

    // With SSE support we will make 8 pixels (from 8 pairs) at a time
          shr		ecx, 1				// 8 bytes a time instead of 4
          jz		LessThan8
          align 16
          hLoopMMXSSE:
        // handle first 2 pixels			
        mov		eax, [rsi + 16]		// EAX! get data offset in pixels, 1st pixel pair 
          mov		ebx, [rsi + 20]		// EBX! get data offset in pixels, 2nd pixel pair 
          movd	mm0, [rdx + rax]		// copy luma pair 0000xxYY
          punpcklwd mm0, [rdx + rbx]    // 2nd luma pair, now xxxxYYYY
          punpcklbw mm0, mm7		    // make words out of bytes, 0Y0Y0Y0Y
          mov		eax, [rsi + 16 + 24]	// EAX! get data offset in pixels, 3rd pixel pair 
          mov		ebx, [rsi + 20 + 24]	// EBX! get data offset in pixels, 4th pixel pair 
          pmaddwd mm0, [rsi]			// mult and sum lumas by ctl weights
          paddusw	mm0, mm6			// round
          psrlw	mm0, 8				// right just 4 luma pixel value 0Y0Y0Y0Y

          // handle 3rd and 4th pixel pairs			
          movd	mm1, [rdx + rax]		// copy luma pair 0000xxYY
          punpcklwd mm1, [rdx + rbx]    // 2nd luma pair, now xxxxYYYY
          punpcklbw mm1, mm7		    // make words out of bytes, 0Y0Y0Y0Y
          mov		eax, [rsi + 16 + 48]	// EAX! get data offset in pixels, 5th pixel pair 
          mov		ebx, [rsi + 20 + 48]	// EBX! get data offset in pixels, 6th pixel pair 
          pmaddwd mm1, [rsi + 24]		// mult and sum lumas by ctl weights
          paddusw	mm1, mm6			// round
          psrlw	mm1, 8				// right just 4 luma pixel value 0Y0Y0Y0Y

          // handle 5th and 6th pixel pairs			
          movd	mm2, [rdx + rax]		// copy luma pair 0000xxYY
          punpcklwd mm2, [rdx + rbx]    // 2nd luma pair, now xxxxYYYY
          punpcklbw mm2, mm7		    // make words out of bytes, 0Y0Y0Y0Y
          mov		eax, [rsi + 16 + 72]	// EAX! get data offset in pixels, 7th pixel pair 
          mov		ebx, [rsi + 20 + 72]	// EBX! get data offset in pixels, 8th pixel pair 
          pmaddwd mm2, [rsi + 48]			// mult and sum lumas by ctl weights
          paddusw	mm2, mm6			// round
          psrlw	mm2, 8				// right just 4 luma pixel value 0Y0Y0Y0Y

          // handle 7th and 8th pixel pairs			
          movd	mm3, [rdx + rax]		// copy luma pair
          punpcklwd mm3, [rdx + rbx]    // 2nd luma pair
          punpcklbw mm3, mm7		    // make words out of bytes
          pmaddwd mm3, [rsi + 72]		// mult and sum lumas by ctl weights
 // _mm_adds_epu16
          paddusw	mm3, mm6			// round
          psrlw	mm3, 8				// right just 4 luma pixel value 0Y0Y0Y0Y

          // combine, store, and loop
          packuswb mm0, mm1			// pack into qword, 0Y0Y0Y0Y
          packuswb mm2, mm3			// pack into qword, 0Y0Y0Y0Y
          packuswb mm0, mm2			// and again into  YYYYYYYY				
          movntq	qword ptr[rdi], mm0	// done with 4 pixels

          lea    rsi, [rsi + 96]		// bump to next control bytest
          lea    rdi, [rdi + 8]			// bump to next output pixel addr
          dec	   ecx
          jg	   hLoopMMXSSE				// loop for more
          sfence

          LessThan8 :
        mov		ecx, row_size
          and		ecx, 7				// we have done all but maybe this
          shr		ecx, 2				// now do only 4 bytes at a time
          jz		LessThan4

          align 16
          hLoopMMX:
        // handle first 2 pixels			
        mov		eax, [rsi + 16]		// EAX! get data offset in pixels, 1st pixel pair 
          mov		ebx, [rsi + 20]		// EBX! get data offset in pixels, 2nd pixel pair 
          movd	mm0, [rdx + rax]		// copy luma pair 0000xxYY
          punpcklwd mm0, [rdx + rbx]    // 2nd luma pair, now xxxxYYYY
          punpcklbw mm0, mm7		    // make words out of bytes, 0Y0Y0Y0Y
          mov		eax, [rsi + 16 + 24]	// get data offset in pixels, 3rd pixel pair
          mov		ebx, [rsi + 20 + 24]	// get data offset in pixels, 4th pixel pair
          pmaddwd mm0, [rsi]			// mult and sum lumas by ctl weights
          paddusw	mm0, mm6			// round
          psrlw	mm0, 8				// right just 4 luma pixel value 0Y0Y0Y0Y

          // handle 3rd and 4th pixel pairs			
          movd	mm1, [rdx + rax]		// copy luma pair
          punpcklwd mm1, [rdx + rbx]    // 2nd luma pair
          punpcklbw mm1, mm7		    // make words out of bytes
          pmaddwd mm1, [rsi + 24]			// mult and sum lumas by ctl weights
          paddusw	mm1, mm6			// round
          psrlw	mm1, 8				// right just 4 luma pixel value 0Y0Y0Y0Y

          // combine, store, and loop
          packuswb mm0, mm1			// pack all into qword, 0Y0Y0Y0Y
          packuswb mm0, mm7			// and again into  0000YYYY				
          movd	dword ptr[rdi], mm0	// done with 4 pixels
          lea    rsi, [rsi + 48]		// bump to next control bytest
          lea    rdi, [rdi + 4]			// bump to next output pixel addr
          loop   hLoopMMX				// loop for more

    // test to see if we have a mod 4 size row, if not then more spare change
          LessThan4 :
        mov		ecx, row_size
          and		ecx, 3				// remainder size mod 4
          cmp		ecx, 2
          jl		LastOne				// none, done

          // handle 2 more pixels			
          mov		eax, [rsi + 16]		// EAX! get data offset in pixels, 1st pixel pair 
          mov		ebx, [rsi + 20]		// EBX! get data offset in pixels, 2nd pixel pair 
          movd	mm0, [rdx + rax]		// copy luma pair 0000xxYY
          punpcklwd mm0, [rdx + rbx]    // 2nd luma pair, now xxxxYYYY
          punpcklbw mm0, mm7		    // make words out of bytes, 0Y0Y0Y0Y

          pmaddwd mm0, [rsi]			// mult and sum lumas by ctl weights
          paddusw	mm0, mm6			// round
          psrlw	mm0, 8				// right just 2 luma pixel value 000Y,000Y
          packuswb mm0, mm7			// pack all into qword, 00000Y0Y
          packuswb mm0, mm7			// and again into  000000YY		
          movd	dword ptr[rdi], mm0	// store, we are guarrenteed room in buffer (8 byte mult)
          sub		ecx, 2
          lea		rsi, [rsi + 24]		// bump to next control bytest
          lea		rdi, [rdi + 2]			// bump to next output pixel addr

          // maybe one last pixel
          LastOne:
        cmp		ecx, 0				// still more?
          jz		AllDone				// n, done

          mov		eax, [rsi + 16]		// EAX! get data offset in pixels, 1st pixel pair 
          movd	mm0, [rdx + rax]		// copy luma pair 0000xxYY
          punpcklbw mm0, mm7		    // make words out of bytes, xxxx0Y0Y

          pmaddwd mm0, [rsi]			// mult and sum lumas by ctl weights
          paddusw	mm0, mm6			// round
          psrlw	mm0, 8				// right just 2 luma pixel value xxxx000Y
          movd	eax, mm0
          mov		byte ptr[rdi], al	// store last one

          AllDone :
        //pop		ecx Intel2017:should be commented out
        emms
      }
    }                               // done with one line
#endif // MSVC x64 asm ignore

    dstp += dst_pitch;
  }

}

// brand new in 2.5.11.2 -> 2.5.11.22
// bilinear resizer for vectors as short integer
void SimpleResize::SimpleResizeDo_uint16(short *dstp, int row_size, int height, int dst_pitch,
  const short* srcp, int src_row_size, int src_pitch)
{
  if (SSE2enabled) {
    SimpleResizeDo_New<short>((uint8_t *)dstp, row_size, height, dst_pitch, (uint8_t *)srcp, src_row_size, src_pitch);
    return;
  }
  
  int vWeight1[4];
  int vWeight2[4];
  const __int64 FPround2[2] = { 0x0000008000000080,0x0000008000000080 };// round dwords

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

    // fill 4x(16x2) bit for inline asm 
    // for intrinsic it is not needed anymore
    vWeight1[0] = vWeight1[1] = vWeight1[2] = vWeight1[3] =
      (invCurrentWeight << 16) | invCurrentWeight;

    vWeight2[0] = vWeight2[1] = vWeight2[2] = vWeight2[3] =
      (CurrentWeight << 16) | CurrentWeight;

    srcp1 = srcp + vOffsetsW[y] * src_pitch;

    // fix in 2.7.14.22: srcp2 may reach beyond block
    // see comment in SimpleResizeDo_New
    bool UseNextLine = vOffsetsW[y] < last_vOffsetsW;
    srcp2 = UseNextLine ? srcp1 + src_pitch : srcp; // pitch is uchar/short-aware

    if (true) // make it true for C version
    {
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
    }
#if 0
    // inline asm ignored for MSVC X64 build: !(defined(_M_X64) && defined(_MSC_VER))
    else {

      _asm
      { // MMX resizer code adopted to sized short int
        //push	ecx Intel2017:should be commented out
        mov		ecx, src_row_size // size in words
        shr		ecx, 2					// 8 bytes = 4 words a time
        mov		rsi, srcp1				// top of 2 src lines to get
        mov		rdx, srcp2				// next "
        mov		rdi, vWorkYW			// luma work destination line
        xor		rax, rax                // P.F. 16.04.28 should be rax here not eax (hack! define rax eax in 32 bit)
        xor       rbx, rbx                // P.F. 16.04.28 leater only ebx used

        movq	mm5, vWeight1
        movq	mm6, vWeight2
        movq	mm0, FPround2			// useful rounding constant
        pxor	mm7, mm7
        align	16
        vLoopMMX:
        movq	mm1, qword ptr[rsi + rax * 2] // top of 2 lines to interpolate (4 vectors)
          movq	mm3, qword ptr[rdx + rax * 2] // 2nd of 2 lines (4 vectors)
          movq	mm2, mm1				// copy top bytes
          movq	mm4, mm3				// copy 2nd bytes

          //__m128i _mm_mullo_epi16 (__m128i a, __m128i b);
          pmullw	mm1, mm5				// mult by weighting factor
          //__m128i _mm_mulhi_epi16 (__m128i a, __m128i b);
          pmulhw	mm2, mm5				// mult by weighting factor, high
          pmullw	mm3, mm6				// mult by weighting factor
          pmulhw	mm4, mm6				// mult by weighting factor, high

          movq    mm7, mm1 // copy
          // _mm_unpacklo_epi16
          punpcklwd mm1, mm2 // double from low and high
          // _mm_unpackhi_epi16
          punpckhwd mm7, mm2 // double from low and high

          movq    mm2, mm3 // copy
          punpcklwd mm3, mm4 // double from low and high
          punpckhwd mm2, mm4 // double from low and high
           // __m128i _mm_add_epi32 (__m128i a, __m128i b);
          paddd	mm1, mm3				// combine lumas
          paddd	mm2, mm7				// combine lumas

          paddd	mm1, mm0				// round
          paddd	mm2, mm0				// round

          // _mm_sra_epi32
          psrad	mm1, 8					// right just
          psrad	mm2, 8					// right just

          //_mm_packs_epi32
          packssdw mm1, mm2				// pack into 4 words

          movq	qword ptr[rdi + rax * 2], mm1	// save 4 words in our work area 

          lea     rax, [rax + 4]
          loop	vLoopMMX // decrement ecx

                   // Add a little code here to check if we have more pixels to do and, if so, make one
                   // more pass thru vLoopMMX. We were processing in multiples of 8 pixels and alway have
                   // an even number so there will never be more than 7 left.
                   //	MoreSpareChange:
          cmp		eax, src_row_size		// EAX! did we get them all
          jnl		DoHorizontal			// yes, else have 2 left
          mov		ecx, 1					// jigger loop ct
          mov		eax, src_row_size       // EAX!
          sub		eax, 4					// back up to last 8 pixels
          jmp		vLoopMMX

          // We've taken care of the vertical scaling, now do horizontal
          // x pixels at a time
          DoHorizontal :
        pxor    mm7, mm7
          movq	mm6, FPround2		// useful rounding constant, dwords
          mov		rsi, pControl		// @ horiz control bytes
          mov		ecx, row_size // size in words
          shr		ecx, 2				// 8 bytes a time, 4 words
          mov     rdx, vWorkYW		// our luma data
          mov		rdi, dstp			// the destination line
          align 16
          hLoopMMX:
        // handle first 2 pixels
        mov		eax, [rsi + 16]		// EAX! get data offset in pixels, 1st pixel pair. Control[4]=offs
          mov		ebx, [rsi + 20]		// EBX! get data offset in pixels, 2nd pixel pair. Control[5]=offsodd
          // (uint32_t)pair1     = (uint32_t *)((short *)(vWorkYW)[offs])      // vWorkYW[offs] and vWorkYW[offs+1]
          // (uint32_t)pair2odds = (uint32_t *)((short *)(vWorkYW)[offsodds])  // vWorkYW[offsodds] and vWorkYW[offsodds+1]

          // using only 64 bit MMX registers
            // __m128i _mm_cvtsi32_si128 (uint32_t*(vWorkYW)[offs]);
            // __m128i _mm_cvtsi64_si128 (uint32_t*(vWorkYW)[offs]);
          movd	mm0, [rdx + rax * 2]		// copy luma pair 0000W1W0  work[offs]
            // _mm_unpacklo_epi32 (mm0, uint32_t*(&ushort*(vWorkYW)[offsodd])
          punpckldq mm0, [rdx + rbx * 2]    // 2nd luma pair, now V1V0W1W0
            // _mm_madd_epi16(.. PControl[0-1])
          pmaddwd mm0, [rsi]			// mult and sum lumas by ctl weights. v1v0 Control[1] and w1w0 Control[0]
            // _mm_add_epi32
          paddd	mm0, mm6			// round
            // _mm_srai_epi32
          psrad	mm0, 8				// right just 2 luma pixel

                    // handle 3rd and 4th pixel pairs
          mov		eax, [rsi + 16 + 24]	// EAX! get data offset in pixels, 3rd pixel pair. Control[4+6}
          mov		ebx, [rsi + 20 + 24]	// EBX! get data offset in pixels, 4th pixel pair. Control[5+6}
          movd	mm1, [rdx + rax * 2]		// copy luma pair
          punpckldq mm1, [rdx + rbx * 2]    // 2nd luma pair
          pmaddwd mm1, [rsi + 24]		// mult and sum lumas by ctl weights. Control[6] and Control[7]
          paddd	mm1, mm6			// round
          psrad	mm1, 8				// right just 2 luma pixel

                    // combine, store, and loop
            // _mm_packs_epi32
          packssdw mm0, mm1			// pack all 4 into qword, 0Y0Y0Y0Y
            // store 4 word, 8 bytes
            // _mm_move_epi64
          movq	qword ptr[rdi], mm0	// done with 4 pixels

          lea    rsi, [rsi + 48]		// bump to next control bytest. Control[12]
          lea    rdi, [rdi + 8]			// bump to next output pixel addr
          loop   hLoopMMX				// loop for more

                      // test to see if we have a mod 4 size row, if not then more spare change
                      //	LessThan4:
          mov		ecx, row_size
          and		ecx, 3				// remainder size mod 4
          cmp		ecx, 2
          jl		LastOne				// none, done

                    // handle 2 more pixels
          mov		eax, [rsi + 16]		// EAX! get data offset in pixels, 1st pixel pair
          mov		ebx, [rsi + 20]		// EBX! get data offset in pixels, 2nd pixel pair
          movd	mm0, [rdx + rax * 2]		// copy luma pair 0000xxYY
          punpckldq mm0, [rdx + rbx * 2]    // 2nd luma pair, now xxxxYYYY

          pmaddwd mm0, [rsi]			// mult and sum lumas by ctl weights
          paddd	mm0, mm6			// round
          psrad	mm0, 8				// right just 2 luma pixel value 000Y,000Y
          packssdw mm0, mm7			// and again into  00000Y0Y
          movd	dword ptr[rdi], mm0	// store, we are guarrenteed room in buffer (8 byte mult)
          sub		ecx, 2
          lea		rsi, [rsi + 24]		// bump to next control bytest
          lea		rdi, [rdi + 4]			// bump to next output pixel addr

                        // maybe one last pixel
          LastOne:
        cmp		ecx, 0				// still more?
          jz		AllDone				// n, done

          mov		eax, [rsi + 16]		// EAX! get data offset in pixels, 1st pixel pair
          movd	mm0, [rdx + rax * 2]		// copy luma pair 0000xxYY
          punpckldq mm0, mm7		    // make dwords out of words, xxxx0Y0Y

          pmaddwd mm0, [rsi]			// mult and sum lumas by ctl weights
          paddd	mm0, mm6			// round
          psrad	mm0, 8				// right just 1 luma pixel value xxxx000Y
          movd	eax, mm0
          mov		word ptr[rdi], ax	// store last one

          AllDone :
        //pop		ecx Intel2017:should be commented out
        emms
      }
    }
#endif // asm ignore
    dstp += dst_pitch;
  }

}


// YV12

// For each horizontal output pair of pixels there is are 2 qword masks followed by 2 int
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
  // We will geneerate these values in pairs, mostly because that's the way
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
