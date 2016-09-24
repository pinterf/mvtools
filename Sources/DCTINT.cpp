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
//#include <intrin.h>
#include <emmintrin.h>
#include <stdint.h>

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

#if 1
// PF 160915 DCTINT for 8x8 is omitted (inline asm vs. x64, 
// and breaks generality

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
		

		pWorkArea = (short * const) _aligned_malloc(8*8*2, 128);


}



DCTINT::~DCTINT()
{
	_aligned_free (pWorkArea);
	pWorkArea = 0;
}

void DCTINT::DCTBytes2D(const unsigned char *srcp, int src_pit, unsigned char *dstp, int dst_pit)
{
	if ( (sizex != 8) || (sizey != 8) ) // may be sometime we will implement 4x4 and 16x16 dct
		return;

	int rowsize = sizex;
	int FldHeight = sizey;
	int dctshift0ext = 4 - DCTSHIFT;// for DC component


	const uint8_t * pSrc = srcp;
	const uint8_t * pSrcW;
	uint8_t * pDest = dstp;
	uint8_t * pDestW;

	const unsigned __int64 i128 = 0x8080808080808080;

	int y;
	int ct = (rowsize/8);
	int ctW;
	//short* pFactors = &factors[0][0];
	short* pWorkAreaW = pWorkArea;			// so inline asm can access

/* PF: Test with 0<dct<5 and BlockSize==8
a=a.QTGMC(Preset="Slower",dct=3, BlockSize=8, ChromaMotion=true)
sup = a.MSuper(pel=1)
fw = sup.MAnalyse(isb=false, delta=1, overlap=4)
bw = sup.MAnalyse(isb=true, delta=1, overlap=4)
a=a.MFlowInter(sup, bw, fw, time=50, thSCD1=400)
a
*/
#define USE_SSE2_DCTINT_INTRINSIC

	for (y=0; y <= FldHeight-8; y+=8)
	{
		pSrcW = pSrc;
		pDestW = pDest;
		ctW = ct;
#ifdef USE_SSE2_DCTINT_INTRINSIC
    // rewritten by PF for SSE2 intrinsics instead of MMX inline asm
    for(int i=0; i<ctW; i++)
    {
      __m128i zero = _mm_setzero_si128();
    __m128i src0 = zero, src1 = zero, src0_8words, src1_8words;
    const uint8_t *tmp_pSrcW = pSrcW;
    // 8-8 bytes #0-#1 (8: DCTINT 8 implementation)
    src0 = _mm_castpd_si128(_mm_loadl_pd(_mm_castsi128_pd(src0), reinterpret_cast<const double *>(tmp_pSrcW)));  // load the lower 64 bits from (unaligned) ptr
    src1 = _mm_castpd_si128(_mm_loadl_pd(_mm_castsi128_pd(src1), reinterpret_cast<const double *>(tmp_pSrcW+src_pit)));
    src0_8words = _mm_unpacklo_epi8(src0, zero);  // 8 bytes -> 8 words
    _mm_storeu_si128(reinterpret_cast<__m128i *>(pWorkAreaW+0*8), src0_8words);
    src1_8words = _mm_unpacklo_epi8(src1, zero);  // 8 bytes -> 8 words
    _mm_storeu_si128(reinterpret_cast<__m128i *>(pWorkAreaW+1*8), src1_8words);
    tmp_pSrcW += 2 * src_pit;

    // 8-8 bytes #2-#3 (8: DCTINT 8 implementation)
    src0 = _mm_castpd_si128(_mm_loadl_pd(_mm_castsi128_pd(src0), reinterpret_cast<const double *>(tmp_pSrcW)));  // load the lower 64 bits from (unaligned) ptr
    src1 = _mm_castpd_si128(_mm_loadl_pd(_mm_castsi128_pd(src1), reinterpret_cast<const double *>(tmp_pSrcW+src_pit)));
    src0_8words = _mm_unpacklo_epi8(src0, zero);  // 8 bytes -> 8 words
    _mm_storeu_si128(reinterpret_cast<__m128i *>(pWorkAreaW+2*8), src0_8words);
    src1_8words = _mm_unpacklo_epi8(src1, zero);  // 8 bytes -> 8 words
    _mm_storeu_si128(reinterpret_cast<__m128i *>(pWorkAreaW+3*8), src1_8words);
    tmp_pSrcW += 2 * src_pit;

    // 8-8 bytes #4-#5 (8: DCTINT 8 implementation)
    src0 = _mm_castpd_si128(_mm_loadl_pd(_mm_castsi128_pd(src0), reinterpret_cast<const double *>(tmp_pSrcW)));  // load the lower 64 bits from (unaligned) ptr
    src1 = _mm_castpd_si128(_mm_loadl_pd(_mm_castsi128_pd(src1), reinterpret_cast<const double *>(tmp_pSrcW+src_pit)));
    src0_8words = _mm_unpacklo_epi8(src0, zero);  // 8 bytes -> 8 words
    _mm_storeu_si128(reinterpret_cast<__m128i *>(pWorkAreaW+4*8), src0_8words);
    src1_8words = _mm_unpacklo_epi8(src1, zero);  // 8 bytes -> 8 words
    _mm_storeu_si128(reinterpret_cast<__m128i *>(pWorkAreaW+5*8), src1_8words);
    tmp_pSrcW += 2 * src_pit;

    // 8-8 bytes #6-#7 (8: DCTINT 8 implementation)
    src0 = _mm_castpd_si128(_mm_loadl_pd(_mm_castsi128_pd(src0), reinterpret_cast<const double *>(tmp_pSrcW)));  // load the lower 64 bits from (unaligned) ptr
    src1 = _mm_castpd_si128(_mm_loadl_pd(_mm_castsi128_pd(src1), reinterpret_cast<const double *>(tmp_pSrcW+src_pit)));
    src0_8words = _mm_unpacklo_epi8(src0, zero);  // 8 bytes -> 8 words
    _mm_storeu_si128(reinterpret_cast<__m128i *>(pWorkAreaW+6*8), src0_8words);
    src1_8words = _mm_unpacklo_epi8(src1, zero);  // 8 bytes -> 8 words
    _mm_storeu_si128(reinterpret_cast<__m128i *>(pWorkAreaW+7*8), src1_8words);

    pSrcW += 8;

    fdct_sse2(pWorkAreaW);					// go do forward DCT

    // decrease dc component
    *(short *)(pWorkAreaW) >>= dctshift0ext; // PF instead of asm

    // decrease all components

    // lets adjust some of the DCT components
    //		DO_ADJUST(pWorkAreaW);
    __m128i signbits_byte = _mm_set1_epi8(-128); // prepare constant: bytes sign bits 0x80
    uint8_t *tmp_pDestW_rdi = pDestW;
    __m128i mm20, mm31, result;

    // #0-#1 of 8
    mm20 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pWorkAreaW+0*8));
    mm31 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pWorkAreaW+1*8));
    mm20 = _mm_srai_epi16(mm20, DCTSHIFT); // decrease by bits shift from (-2047, +2047) to 
    mm31 = _mm_srai_epi16(mm31, DCTSHIFT);
    result = _mm_packs_epi16(mm20, mm31); // to bytes with signed saturation
    result = _mm_xor_si128(result, signbits_byte); // convert to unsigned (0, 255) by adding 128
    _mm_storel_pd(reinterpret_cast<double *>(tmp_pDestW_rdi + 0 * dst_pit), _mm_castsi128_pd(result)); // store lower 8 byte
    _mm_storeh_pd(reinterpret_cast<double *>(tmp_pDestW_rdi + 1 * dst_pit), _mm_castsi128_pd(result)); // store upper 8 byte
    // #2-#3 of 8
    mm20 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pWorkAreaW+2*8));
    mm31 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pWorkAreaW+3*8));
    mm20 = _mm_srai_epi16(mm20, DCTSHIFT); // decrease by bits shift from (-2047, +2047) to 
    mm31 = _mm_srai_epi16(mm31, DCTSHIFT);
    result = _mm_packs_epi16(mm20, mm31); // to bytes with signed saturation
    result = _mm_xor_si128(result, signbits_byte); // convert to unsigned (0, 255) by adding 128
    _mm_storel_pd(reinterpret_cast<double *>(tmp_pDestW_rdi + 2 * dst_pit), _mm_castsi128_pd(result)); // store lower 8 byte
    _mm_storeh_pd(reinterpret_cast<double *>(tmp_pDestW_rdi + 3 * dst_pit), _mm_castsi128_pd(result)); // store upper 8 byte
    // #4-#5 of 8
    mm20 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pWorkAreaW+4*8));
    mm31 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pWorkAreaW+5*8));
    mm20 = _mm_srai_epi16(mm20, DCTSHIFT); // decrease by bits shift from (-2047, +2047) to 
    mm31 = _mm_srai_epi16(mm31, DCTSHIFT);
    result = _mm_packs_epi16(mm20, mm31); // to bytes with signed saturation
    result = _mm_xor_si128(result, signbits_byte); // convert to unsigned (0, 255) by adding 128
    _mm_storel_pd(reinterpret_cast<double *>(tmp_pDestW_rdi + 4 * dst_pit), _mm_castsi128_pd(result)); // store lower 8 byte
    _mm_storeh_pd(reinterpret_cast<double *>(tmp_pDestW_rdi + 5 * dst_pit), _mm_castsi128_pd(result)); // store upper 8 byte
    // #6-#7 of 8
    mm20 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pWorkAreaW+6*8));
    mm31 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pWorkAreaW+7*8));
    mm20 = _mm_srai_epi16(mm20, DCTSHIFT); // decrease by bits shift from (-2047, +2047) to 
    mm31 = _mm_srai_epi16(mm31, DCTSHIFT);
    result = _mm_packs_epi16(mm20, mm31); // to bytes with signed saturation
    result = _mm_xor_si128(result, signbits_byte); // convert to unsigned (0, 255) by adding 128
    _mm_storel_pd(reinterpret_cast<double *>(tmp_pDestW_rdi + 6 * dst_pit), _mm_castsi128_pd(result)); // store lower 8 byte
    _mm_storeh_pd(reinterpret_cast<double *>(tmp_pDestW_rdi + 7 * dst_pit), _mm_castsi128_pd(result)); // store upper 8 byte
    pDestW += 8;
   } // for ctW

#else
		__asm
		{
		// Loop general reg usage
		//
		// ecx - inner loop counter
		// ebx - src pitch
		// edx - dest pitch
		// edi - dest
		// esi - src pixels

// SPLIT Qword

		align 16
LoopQ:	
		mov		rsi, pSrcW   
		lea		rax, [rsi+8]
		mov		pSrcW, rax					// save for next time
		mov		ebx, src_pit        // PF todo 160620 rbx??!! in theory, 32 bit operands are zero extended to 64 bit
		mov		rax, pWorkAreaW

		// expand bytes to words in work area
      // __m128i zero = _mm_setzero_si128()
		pxor	mm7, mm7
//-----------------------
      // __m128i src0 = _mm_move_epi64 (pSrcW);          // 8 bytes #0 DCTINT 8!
		movq	mm0, qword ptr[rsi]			// # 0
      // __m128i src1 = _mm_move_epi64 (pSrcW+src_pit);  // 8 bytes #1
    movq	mm2, qword ptr[rsi+rbx]		// # 1

		movq	mm1, mm0
		punpcklbw mm0, mm7					// low bytes to words
		movq	qword ptr[rax], mm0			// low words to work area
		punpckhbw mm1, mm7					// high bytes to words
		movq	qword ptr[rax+8], mm1		// high words to work area
  // assembly above uses 64 bit mmx registers, we can do it in one step in sse2
  // __m128i src0_8words = _mm_unpacklo_epi8(src0, zero);  // 8 bytes -> 8 words
  // _mm_storeu_epi128(reinterpret_cast<__m128i *>(pWorkAreaW), src0_8words);

		movq	mm3, mm2
		punpcklbw mm2, mm7					// low bytes to words
		movq	qword ptr[rax+1*16], mm2	// low words to work area
		punpckhbw mm3, mm7					// high bytes to words
		movq	qword ptr[rax+1*16+8], mm3	// high words to work area
    // assembly above uses 64 bit mmx registers, we can do it in one step in sse2
    // __m128i src1_8words = _mm_unpacklo_epi8(src1, zero);  // 8 bytes -> 8 words
    // void _mm_storeu_epi128(reinterpret_cast<__m128i *>(pWorkAreaW+1*16), src1_8words);

		lea		rsi, [rsi+2*rbx]
      // pSrcW += 2*src_pit;
//-----------------------
      // __m128i src2 = _mm_move_epi64 (pSrcW);          // 8 bytes #2 DCTINT 8!
    movq	mm0, qword ptr[rsi]			// #2
      // __m128i src3 = _mm_move_epi64 (pSrcW+src_pit);  // 8 bytes #3
    movq	mm2, qword ptr[rsi+rbx]		// #3

		movq	mm1, mm0
		punpcklbw mm0, mm7					// low bytes to words
		movq	qword ptr[rax+2*16], mm0	// low words to work area
		punpckhbw mm1, mm7					// high bytes to words
		movq	qword ptr[rax+2*16+8], mm1	// high words to work area
    // assembly above uses 64 bit mmx registers, we can do it in one step in sse2
    // __m128i src2_8words = _mm_unpacklo_epi8(src2, zero);  // 8 bytes -> 8 words
    // _mm_storeu_epi128(reinterpret_cast<__m128i *>(pWorkAreaW+2*16), src2_8words);

		movq	mm3, mm2
		punpcklbw mm2, mm7					// low bytes to words
		movq	qword ptr[rax+3*16], mm2	// low words to work area
		punpckhbw mm3, mm7					// high bytes to words
		movq	qword ptr[rax+3*16+8], mm3	// high words to work area
    // assembly above uses 64 bit mmx registers, we can do it in one step in sse2
    // __m128i src3_8words = _mm_unpacklo_epi8(src3, zero);  // 8 bytes -> 8 words
    // _mm_storeu_epi128(reinterpret_cast<__m128i *>(pWorkAreaW+3*16), src3_8words);

		lea		rsi, [rsi+2*rbx]
      // pSrcW += 2*src_pit;
//-----------------------
    
    // __m128i src4 = _mm_move_epi64 (pSrcW);          // 8 bytes #4 DCTINT 8!
    movq	mm0, qword ptr[rsi]			// #4
    // __m128i src5 = _mm_move_epi64 (pSrcW+src_pit);  // 8 bytes #5
    movq	mm2, qword ptr[rsi+rbx]		// #5

		movq	mm1, mm0
		punpcklbw mm0, mm7					// low bytes to words
		movq	qword ptr[rax+4*16], mm0	// low words to work area
		punpckhbw mm1, mm7					// high bytes to words
		movq	qword ptr[rax+4*16+8], mm1	// high words to work area
    // assembly above uses 64 bit mmx registers, we can do it in one step in sse2
    // __m128i src4_8words = _mm_unpacklo_epi8(src4, zero);  // 8 bytes -> 8 words
    // _mm_storeu_epi128(reinterpret_cast<__m128i *>(pWorkAreaW+4*16), src4_8words);
    movq	mm3, mm2
		punpcklbw mm2, mm7					// low bytes to words
		movq	qword ptr[rax+5*16], mm2	// low words to work area
		punpckhbw mm3, mm7					// high bytes to words
		movq	qword ptr[rax+5*16+8], mm3	// high words to work area
    // assembly above uses 64 bit mmx registers, we can do it in one step in sse2
    // __m128i src5_8words = _mm_unpacklo_epi8(src5, zero);  // 8 bytes -> 8 words
    // _mm_storeu_epi128(reinterpret_cast<__m128i *>(short *pWorkAreaW+5*16), src5_8words);

		lea		rsi, [rsi+2*rbx]
    // pSrcW += 2*src_pit;

//-----------------------

    // __m128i src6 = _mm_move_epi64 (pSrcW);          // 8 bytes #6 DCTINT 8!
		movq	mm0, qword ptr[rsi]			// #6
    // __m128i src7 = _mm_move_epi64 (pSrcW+src_pit);  // 8 bytes #7
    movq	mm2, qword ptr[rsi+rbx]		// #7

		movq	mm1, mm0
		punpcklbw mm0, mm7					// low bytes to words
		movq	qword ptr[rax+6*16], mm0	// low words to work area
		punpckhbw mm1, mm7					// high bytes to words
		movq	qword ptr[rax+6*16+8], mm1	// high words to work area
    // assembly above uses 64 bit mmx registers, we can do it in one step in sse2
    // __m128i src6_8words = _mm_unpacklo_epi8(src6, zero);  // 8 bytes -> 8 words
    // _mm_storeu_epi128(reinterpret_cast<__m128i *>(pWorkAreaW+6*16), src6_8words);

		movq	mm3, mm2
		punpcklbw mm2, mm7					// low bytes to words
		movq	qword ptr[rax+7*16], mm2	// low words to work area
		punpckhbw mm3, mm7					// high bytes to words
		movq	qword ptr[rax+7*16+8], mm3	// high words to work area	
    // assembly above uses 64 bit mmx registers, we can do it in one step in sse2
    // __m128i src7_8words = _mm_unpacklo_epi8(src7, zero);  // 8 bytes -> 8 words
    // _mm_storeu_epi128(reinterpret_cast<__m128i *>(pWorkAreaW+7*16), src7_8words);
    } // end asm

		
		fdct_sse2(pWorkAreaW);					// go do forward DCT

    // decrease dc component
    *(short *)(pWorkAreaW) >>= dctshift0ext; // PF instead of asm
#if 0
		_asm 
		{
			mov	rax, pWorkAreaW;
			mov ebx, [rax];
			mov ecx, dctshift0ext;
			sar bx, cl;
			mov [rax], ebx;
		}
#endif
		_asm
		{

		movq mm7, i128 // prepare constant: bytes sign bits

		mov		rdi, pDestW					// got em, now store em
		lea		rax, [rdi+8]
		mov		pDestW, rax					// save for next time
		mov		edx, dst_pit 

		mov		rax, pWorkAreaW
		// collapse words to bytes in dest

		movq	mm0, qword ptr[rax]			// low qwords
		movq	mm1, qword ptr[rax+1*16]
		movq	mm2, qword ptr[rax+8]			// high qwords
		movq	mm3, qword ptr[rax+1*16+8]
		// decrease by bits shift from (-2047, +2047) to 
		psraw	mm0, DCTSHIFT  
		psraw	mm1, DCTSHIFT
		psraw	mm2, DCTSHIFT
		psraw	mm3, DCTSHIFT
		// to bytes with signed saturation
		packsswb mm0, mm2		
		packsswb mm1, mm3
		// convert to unsigned (0, 255) by adding 128
		pxor mm0, mm7     
		pxor mm1, mm7
		// store to dest
		movq	qword ptr[rdi], mm0	
		movq	qword ptr[rdi+rdx], mm1

		//NEXT
		movq	mm0, qword ptr[rax+2*16]			// low qwords
		movq	mm1, qword ptr[rax+3*16]
		movq	mm2, qword ptr[rax+2*16+8]			// high qwords
		movq	mm3, qword ptr[rax+3*16+8]
		// decrease by bits shift from (-2047, +2047) to 
		psraw	mm0, DCTSHIFT  
		psraw	mm1, DCTSHIFT
		psraw	mm2, DCTSHIFT
		psraw	mm3, DCTSHIFT
		// to bytes with signed saturation
		packsswb mm0, mm2		
		packsswb mm1, mm3
		// convert to unsigned (0, 255) by adding 128
		pxor mm0, mm7     
		pxor mm1, mm7
		// store to dest
		lea		rdi, [rdi+2*rdx]
		movq	qword ptr[rdi], mm0	
		movq	qword ptr[rdi+rdx], mm1

		//NEXT
		movq	mm0, qword ptr[rax+4*16]			// low qwords
		movq	mm1, qword ptr[rax+5*16]
		movq	mm2, qword ptr[rax+4*16+8]			// high qwords
		movq	mm3, qword ptr[rax+5*16+8]
		// decrease by bits shift from (-2047, +2047) to 
		psraw	mm0, DCTSHIFT  
		psraw	mm1, DCTSHIFT
		psraw	mm2, DCTSHIFT
		psraw	mm3, DCTSHIFT
		// to bytes with signed saturation
		packsswb mm0, mm2		
		packsswb mm1, mm3
		// convert to unsigned (0, 255) by adding 128
		pxor mm0, mm7     
		pxor mm1, mm7
		// store to dest
		lea		rdi, [rdi+2*rdx]
		movq	qword ptr[rdi], mm0	
		movq	qword ptr[rdi+rdx], mm1

		//NEXT
		movq	mm0, qword ptr[rax+6*16]			// low qwords
		movq	mm1, qword ptr[rax+7*16]
		movq	mm2, qword ptr[rax+6*16+8]			// high qwords
		movq	mm3, qword ptr[rax+7*16+8]
		// decrease by bits shift from (-2047, +2047) to 
		psraw	mm0, DCTSHIFT  
		psraw	mm1, DCTSHIFT
		psraw	mm2, DCTSHIFT
		psraw	mm3, DCTSHIFT
		// to bytes with signed saturation
		packsswb mm0, mm2		
		packsswb mm1, mm3
		// convert to unsigned (0, 255) by adding 128
		pxor mm0, mm7     
		pxor mm1, mm7
		// store to dest
		lea		edi, [rdi+2*rdx]
		movq	qword ptr[rdi], mm0	
		movq	qword ptr[rdi+rdx], mm1

	
		dec		ctW
		jnz		LoopQ

		}
#endif

	// adjust for next line
	pSrc  += 8*src_pit;
	pDest += 8*dst_pit;
	}
	_mm_empty();
}
#endif

