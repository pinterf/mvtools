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

#define DCTSHIFT 2 // 2 is 4x decreasing,
// only DC component need in shift 4 (i.e. 16x) for real images

DCTINT::DCTINT(int _sizex, int _sizey, int _dctmode) 

{
	// only 8x8 implemented !
		sizex = _sizex;
		sizey = _sizey;
		dctmode = _dctmode;
		int size2d = sizey*sizex;

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

	for (y=0; y <= FldHeight-8; y+=8)	
	{
		pSrcW = pSrc;
		pDestW = pDest;
		ctW = ct;
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
		pxor	mm7, mm7
		
		movq	mm0, qword ptr[rsi]			// # 0
		movq	mm2, qword ptr[rsi+rbx]		// # 1

		movq	mm1, mm0
		punpcklbw mm0, mm7					// low bytes to words
		movq	qword ptr[rax], mm0			// low words to work area
		punpckhbw mm1, mm7					// high bytes to words
		movq	qword ptr[rax+8], mm1		// high words to work area
		
		movq	mm3, mm2
		punpcklbw mm2, mm7					// low bytes to words
		movq	qword ptr[rax+1*16], mm2	// low words to work area
		punpckhbw mm3, mm7					// high bytes to words
		movq	qword ptr[rax+1*16+8], mm3	// high words to work area
		
		lea		rsi, [rsi+2*rbx]

		movq	mm0, qword ptr[rsi]			// #2
		movq	mm2, qword ptr[rsi+rbx]		// #3

		movq	mm1, mm0
		punpcklbw mm0, mm7					// low bytes to words
		movq	qword ptr[rax+2*16], mm0	// low words to work area
		punpckhbw mm1, mm7					// high bytes to words
		movq	qword ptr[rax+2*16+8], mm1	// high words to work area
		
		movq	mm3, mm2
		punpcklbw mm2, mm7					// low bytes to words
		movq	qword ptr[rax+3*16], mm2	// low words to work area
		punpckhbw mm3, mm7					// high bytes to words
		movq	qword ptr[rax+3*16+8], mm3	// high words to work area
		
		lea		rsi, [rsi+2*rbx]

		movq	mm0, qword ptr[rsi]			// #4
		movq	mm2, qword ptr[rsi+rbx]		// #5

		movq	mm1, mm0
		punpcklbw mm0, mm7					// low bytes to words
		movq	qword ptr[rax+4*16], mm0	// low words to work area
		punpckhbw mm1, mm7					// high bytes to words
		movq	qword ptr[rax+4*16+8], mm1	// high words to work area
		
		movq	mm3, mm2
		punpcklbw mm2, mm7					// low bytes to words
		movq	qword ptr[rax+5*16], mm2	// low words to work area
		punpckhbw mm3, mm7					// high bytes to words
		movq	qword ptr[rax+5*16+8], mm3	// high words to work area
		
		lea		rsi, [rsi+2*rbx]

		movq	mm0, qword ptr[rsi]			// #6
		movq	mm2, qword ptr[rsi+rbx]		// #7

		movq	mm1, mm0
		punpcklbw mm0, mm7					// low bytes to words
		movq	qword ptr[rax+6*16], mm0	// low words to work area
		punpckhbw mm1, mm7					// high bytes to words
		movq	qword ptr[rax+6*16+8], mm1	// high words to work area
		
		movq	mm3, mm2
		punpcklbw mm2, mm7					// low bytes to words
		movq	qword ptr[rax+7*16], mm2	// low words to work area
		punpckhbw mm3, mm7					// high bytes to words
		movq	qword ptr[rax+7*16+8], mm3	// high words to work area	
		}


		
		fdct_sse2(pWorkAreaW);					// go do forward DCT

		_asm 
		{
		// decrease dc component
			mov	rax, pWorkAreaW;
			mov ebx, [rax];
			mov ecx, dctshift0ext;
			sar bx, cl;
			mov [rax], ebx;
		}
		// decrease all components

		// lets adjust some of the DCT components
//		DO_ADJUST(pWorkAreaW);

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

	// adjust for next line
	pSrc  += 8*src_pit;
	pDest += 8*dst_pit;
	}
	_mm_empty();
}
