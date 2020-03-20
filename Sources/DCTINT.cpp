// DCT calculation with integer dct asm
// Copyright(c)2006 A.G.Balakhnin aka Fizick

// Code mostly borrowed from DCTFilter plugin by Tom Barry
// Copyright (c) 2002 Tom Barry.  All rights reserved.
//      trbarry@trbarry.com

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

#include "DCTINT.h"
#include "types.h"

#include "malloc.h"
#include <emmintrin.h>
#include <stdint.h>

#define DCTSHIFT 2 // 2 is 4x decreasing,
// only DC component need in shift 4 (i.e. 16x) for real images

DCTINT::DCTINT(int _sizex, int _sizey, int _dctmode)

{
  // only 8x8 implemented !
  sizex = _sizex;
  sizey = _sizey;
  dctmode = _dctmode;
//		int size2d = sizey*sizex;
//		int cursize = 1;
//		dctshift = 0;
//		while (cursize < size2d) 
//		{
//			dctshift++;
//			cursize = (cursize<<1);
//		}


// 64 words working buffer followed by a 64 word internal temp buffer (multithreading)
  pWorkArea = (short * const)_aligned_malloc(2 * (8 * 8 * sizeof(short)), 128);


}



DCTINT::~DCTINT()
{
  _aligned_free(pWorkArea);
  pWorkArea = 0;
}

void DCTINT::DCTBytes2D(const unsigned char *srcp, int src_pit, unsigned char *dstp, int dst_pit)
{
  if ((sizex != 8) || (sizey != 8)) // may be sometime we will implement 4x4 and 16x16 dct
    return;

  int rowsize = sizex;
  int FldHeight = sizey;
  int dctshift0ext = 4 - DCTSHIFT;// for DC component


  const uint8_t * pSrc = srcp;
  const uint8_t * pSrcW;
  uint8_t * pDest = dstp;
  uint8_t * pDestW;

  int y;
  int ct = (rowsize / 8);
  int ctW;
  //short* pFactors = &factors[0][0];

/* PF: Test with 0<dct<5 and BlockSize==8
a=a.QTGMC(Preset="Slower",dct=3, BlockSize=8, ChromaMotion=true)
sup = a.MSuper(pel=1)
fw = sup.MAnalyse(isb=false, delta=1, overlap=4)
bw = sup.MAnalyse(isb=true, delta=1, overlap=4)
a=a.MFlowInter(sup, bw, fw, time=50, thSCD1=400)
a
*/
  for (y = 0; y <= FldHeight - 8; y += 8)
  {
    pSrcW = pSrc;
    pDestW = pDest;
    ctW = ct;
    // rewritten by PF for SSE2 intrinsics instead of MMX inline asm
    for (int i = 0; i < ctW; i++)
    {
      __m128i zero = _mm_setzero_si128();
      __m128i src0 = zero, src1 = zero, src0_8words, src1_8words;
      const uint8_t *tmp_pSrcW = pSrcW;
      // 8-8 bytes #0-#1 (8: DCTINT 8 implementation)
      src0 = _mm_castpd_si128(_mm_loadl_pd(_mm_castsi128_pd(src0), reinterpret_cast<const double *>(tmp_pSrcW)));  // load the lower 64 bits from (unaligned) ptr
      src1 = _mm_castpd_si128(_mm_loadl_pd(_mm_castsi128_pd(src1), reinterpret_cast<const double *>(tmp_pSrcW + src_pit)));
      src0_8words = _mm_unpacklo_epi8(src0, zero);  // 8 bytes -> 8 words
      _mm_store_si128(reinterpret_cast<__m128i *>(pWorkArea + 0 * 8), src0_8words);
      src1_8words = _mm_unpacklo_epi8(src1, zero);  // 8 bytes -> 8 words
      _mm_store_si128(reinterpret_cast<__m128i *>(pWorkArea + 1 * 8), src1_8words);
      tmp_pSrcW += 2 * src_pit;

      // 8-8 bytes #2-#3 (8: DCTINT 8 implementation)
      src0 = _mm_castpd_si128(_mm_loadl_pd(_mm_castsi128_pd(src0), reinterpret_cast<const double *>(tmp_pSrcW)));  // load the lower 64 bits from (unaligned) ptr
      src1 = _mm_castpd_si128(_mm_loadl_pd(_mm_castsi128_pd(src1), reinterpret_cast<const double *>(tmp_pSrcW + src_pit)));
      src0_8words = _mm_unpacklo_epi8(src0, zero);  // 8 bytes -> 8 words
      _mm_store_si128(reinterpret_cast<__m128i *>(pWorkArea + 2 * 8), src0_8words);
      src1_8words = _mm_unpacklo_epi8(src1, zero);  // 8 bytes -> 8 words
      _mm_store_si128(reinterpret_cast<__m128i *>(pWorkArea + 3 * 8), src1_8words);
      tmp_pSrcW += 2 * src_pit;

      // 8-8 bytes #4-#5 (8: DCTINT 8 implementation)
      src0 = _mm_castpd_si128(_mm_loadl_pd(_mm_castsi128_pd(src0), reinterpret_cast<const double *>(tmp_pSrcW)));  // load the lower 64 bits from (unaligned) ptr
      src1 = _mm_castpd_si128(_mm_loadl_pd(_mm_castsi128_pd(src1), reinterpret_cast<const double *>(tmp_pSrcW + src_pit)));
      src0_8words = _mm_unpacklo_epi8(src0, zero);  // 8 bytes -> 8 words
      _mm_store_si128(reinterpret_cast<__m128i *>(pWorkArea + 4 * 8), src0_8words);
      src1_8words = _mm_unpacklo_epi8(src1, zero);  // 8 bytes -> 8 words
      _mm_store_si128(reinterpret_cast<__m128i *>(pWorkArea + 5 * 8), src1_8words);
      tmp_pSrcW += 2 * src_pit;

      // 8-8 bytes #6-#7 (8: DCTINT 8 implementation)
      src0 = _mm_castpd_si128(_mm_loadl_pd(_mm_castsi128_pd(src0), reinterpret_cast<const double *>(tmp_pSrcW)));  // load the lower 64 bits from (unaligned) ptr
      src1 = _mm_castpd_si128(_mm_loadl_pd(_mm_castsi128_pd(src1), reinterpret_cast<const double *>(tmp_pSrcW + src_pit)));
      src0_8words = _mm_unpacklo_epi8(src0, zero);  // 8 bytes -> 8 words
      _mm_store_si128(reinterpret_cast<__m128i *>(pWorkArea + 6 * 8), src0_8words);
      src1_8words = _mm_unpacklo_epi8(src1, zero);  // 8 bytes -> 8 words
      _mm_store_si128(reinterpret_cast<__m128i *>(pWorkArea + 7 * 8), src1_8words);

      pSrcW += 8;

      fdct_sse2(pWorkArea);					// go do forward DCT

      // decrease dc component
      *(short *)(pWorkArea) >>= dctshift0ext; // PF instead of asm

      // decrease all components

      // lets adjust some of the DCT components
      __m128i signbits_byte = _mm_set1_epi8(-128); // prepare constant: bytes sign bits 0x80
      uint8_t *tmp_pDestW_rdi = pDestW;
      __m128i mm20, mm31, result;

      // #0-#1 of 8
      mm20 = _mm_load_si128(reinterpret_cast<const __m128i *>(pWorkArea + 0 * 8));
      mm31 = _mm_load_si128(reinterpret_cast<const __m128i *>(pWorkArea + 1 * 8));
      mm20 = _mm_srai_epi16(mm20, DCTSHIFT); // decrease by bits shift from (-2047, +2047) to 
      mm31 = _mm_srai_epi16(mm31, DCTSHIFT);
      result = _mm_packs_epi16(mm20, mm31); // to bytes with signed saturation
      result = _mm_xor_si128(result, signbits_byte); // convert to unsigned (0, 255) by adding 128
      _mm_storel_pd(reinterpret_cast<double *>(tmp_pDestW_rdi + 0 * dst_pit), _mm_castsi128_pd(result)); // store lower 8 byte
      _mm_storeh_pd(reinterpret_cast<double *>(tmp_pDestW_rdi + 1 * dst_pit), _mm_castsi128_pd(result)); // store upper 8 byte
      // #2-#3 of 8
      mm20 = _mm_load_si128(reinterpret_cast<const __m128i *>(pWorkArea + 2 * 8));
      mm31 = _mm_load_si128(reinterpret_cast<const __m128i *>(pWorkArea + 3 * 8));
      mm20 = _mm_srai_epi16(mm20, DCTSHIFT); // decrease by bits shift from (-2047, +2047) to 
      mm31 = _mm_srai_epi16(mm31, DCTSHIFT);
      result = _mm_packs_epi16(mm20, mm31); // to bytes with signed saturation
      result = _mm_xor_si128(result, signbits_byte); // convert to unsigned (0, 255) by adding 128
      _mm_storel_pd(reinterpret_cast<double *>(tmp_pDestW_rdi + 2 * dst_pit), _mm_castsi128_pd(result)); // store lower 8 byte
      _mm_storeh_pd(reinterpret_cast<double *>(tmp_pDestW_rdi + 3 * dst_pit), _mm_castsi128_pd(result)); // store upper 8 byte
      // #4-#5 of 8
      mm20 = _mm_load_si128(reinterpret_cast<const __m128i *>(pWorkArea + 4 * 8));
      mm31 = _mm_load_si128(reinterpret_cast<const __m128i *>(pWorkArea + 5 * 8));
      mm20 = _mm_srai_epi16(mm20, DCTSHIFT); // decrease by bits shift from (-2047, +2047) to 
      mm31 = _mm_srai_epi16(mm31, DCTSHIFT);
      result = _mm_packs_epi16(mm20, mm31); // to bytes with signed saturation
      result = _mm_xor_si128(result, signbits_byte); // convert to unsigned (0, 255) by adding 128
      _mm_storel_pd(reinterpret_cast<double *>(tmp_pDestW_rdi + 4 * dst_pit), _mm_castsi128_pd(result)); // store lower 8 byte
      _mm_storeh_pd(reinterpret_cast<double *>(tmp_pDestW_rdi + 5 * dst_pit), _mm_castsi128_pd(result)); // store upper 8 byte
      // #6-#7 of 8
      mm20 = _mm_load_si128(reinterpret_cast<const __m128i *>(pWorkArea + 6 * 8));
      mm31 = _mm_load_si128(reinterpret_cast<const __m128i *>(pWorkArea + 7 * 8));
      mm20 = _mm_srai_epi16(mm20, DCTSHIFT); // decrease by bits shift from (-2047, +2047) to 
      mm31 = _mm_srai_epi16(mm31, DCTSHIFT);
      result = _mm_packs_epi16(mm20, mm31); // to bytes with signed saturation
      result = _mm_xor_si128(result, signbits_byte); // convert to unsigned (0, 255) by adding 128
      _mm_storel_pd(reinterpret_cast<double *>(tmp_pDestW_rdi + 6 * dst_pit), _mm_castsi128_pd(result)); // store lower 8 byte
      _mm_storeh_pd(reinterpret_cast<double *>(tmp_pDestW_rdi + 7 * dst_pit), _mm_castsi128_pd(result)); // store upper 8 byte
      pDestW += 8;
    } // for ctW

    // adjust for next line
    pSrc += 8 * src_pit;
    pDest += 8 * dst_pit;
  }
#ifndef _M_X64 
  _mm_empty();
#endif
}

