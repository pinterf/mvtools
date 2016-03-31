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


SimpleResize::SimpleResize(int _newwidth, int _newheight, int _oldwidth, int _oldheight, long CPUFlags)
{
		oldwidth = _oldwidth;
		oldheight = _oldheight;
		newwidth = _newwidth;
		newheight = _newheight;
		SSE2enabled = (CPUFlags & CPUF_SSE2) !=0;
		SSEMMXenabled = (CPUFlags& CPUF_INTEGER_SSE) !=0;
	    
			vOffsetsUV = (unsigned int*) _aligned_malloc(newheight*4, 128);  
			vWeightsUV = (unsigned int*) _aligned_malloc(newheight*4, 128);  

//			if (!vOffsetsUV || !vWeightsUV)
			{
//				env->ThrowError("SimpleResize: memory allocation error");
			}

//			env->ThrowError("SimpleResize: Horizontal pixels must divide by 2");

		// 2 qwords, 2 offsets, and prefetch slack
	    hControl = (unsigned int*) _aligned_malloc(newwidth*12+128, 128);   // aligned for P4 cache line
	    hControlUV = (unsigned int*) _aligned_malloc(newwidth*12+128, 128);   // added by Fizick
	    vWorkY = (unsigned int*) _aligned_malloc(2*oldwidth+128, 128);   
	    vWorkUV = (unsigned int*) _aligned_malloc(oldwidth+128, 128);   
	    vOffsets = (unsigned int*) _aligned_malloc(newheight*4, 128);  
	    vWeights = (unsigned int*) _aligned_malloc(newheight*4, 128);  

//		if (!hControl || !vWeights)
		{
//			env->ThrowError("SimpleResize: memory allocation error");
		}
		
		InitTables_YV12();
};

SimpleResize::~SimpleResize()
{
	_aligned_free (vOffsetsUV);
	_aligned_free (vWeightsUV);
	_aligned_free (hControl);
	_aligned_free (hControlUV);
	_aligned_free (vWorkY);
	_aligned_free (vWorkUV);
	_aligned_free (vOffsets);
	_aligned_free (vWeights);

}

// YV12 Luma
void SimpleResize::SimpleResizeDo(uint8_t *dstp,  int row_size, int height, int dst_pitch, 
						  const uint8_t* srcp, int src_row_size, int src_pitch, int Planar_Type) 
{
// Note: PlanarType is dummy, I (Fizick) do not use croma planes code for resize in MVTools

	int vWeight1[4];
	int vWeight2[4];
	const __int64 YMask[2]    = {0x00ff00ff00ff00ff,0x00ff00ff00ff00ff}; // keeps only luma
	const __int64 FPround1[2] = {0x0080008000800080,0x0080008000800080}; // round words
	const __int64 FPround2[2] = {0x0000008000000080,0x0000008000000080};// round dwords
	const __int64 FPround4	  = 0x0080008000800080;// round words


//	const uint8_t* srcp = src->GetReadPtr(Planar_Type);
	const uint8_t* srcp2W = srcp;
	uint8_t* dstp2 = dstp;

	//	uint8_t* dstp = dst->GetWritePtr(Planar_Type);
//	const int src_pitch = src->GetPitch(Planar_Type);
//	const int dst_pitch = dst->GetPitch(Planar_Type);
//	const int src_row_size = src->GetRowSize(Planar_Type);   
 //   const int row_size = dst->GetRowSize(Planar_Type);
//	const int height = dst->GetHeight(Planar_Type);

	const unsigned int* pControl = (Planar_Type == PLANAR_Y) 
		? &hControl[0]
		: &hControlUV[0]; // added by Fizick

    const unsigned char* srcp1;
    const unsigned char* srcp2;
	unsigned int* vWorkYW = vWorkY;
	
	unsigned int* vOffsetsW = (Planar_Type == PLANAR_Y)
		? vOffsets
		: vOffsetsUV; 

	unsigned int* vWeightsW = (Planar_Type == PLANAR_Y)
		? vWeights
		: vWeightsUV;
		
	bool	SSE2enabledW = SSE2enabled;		// in local storage for asm
	bool	SSEMMXenabledW = SSEMMXenabled;		// in local storage for asm
//>>>>>>>> remove this 
//	bool	SSE2enabledW = 0;     SSE2enabled;		// in local storage for asm
//	bool	SSEMMXenabledW = SSEMMXenabled;		// in local storage for asm

	
	// Just in case things are not aligned right, maybe turn off sse2

	for (int y = 0; y < height; y++)
	{

		vWeight1[0] = vWeight1[1] = vWeight1[2] = vWeight1[3] = 
			(256-vWeightsW[y]) << 16 | (256-vWeightsW[y]);
		vWeight2[0] = vWeight2[1] = vWeight2[2] = vWeight2[3] = 
			vWeightsW[y] << 16 | vWeightsW[y];

		srcp1 = srcp + vOffsetsW[y] * src_pitch;
		
		{
			srcp2 = (y < height-1)
				? srcp1 + src_pitch
				: srcp1;
		}

		_asm		
		{
			emms // added by paranoid Fizick
		//	push	ecx						// have to save this?
			mov		ecx, src_row_size
			shr		ecx, 3					// 8 bytes a time
			mov		rsi, srcp1				// top of 2 src lines to get
			mov		rdx, srcp2				// next "
			mov		rdi, vWorkYW			// luma work destination line
			xor		eax, eax

// Let's check here to see if we are on a P4 or higher and can use SSE2 instructions.
// This first loop is not the performance bottleneck anyway but it is trivial to tune
// using SSE2 if we have proper alignment.

			test    SSE2enabledW,1			// is SSE2 supported?
			jz		vMaybeSSEMMX				// n, can't do anyway

			cmp     ecx, 2					// we have at least 16 byts, 2 qwords?
			jl		vMaybeSSEMMX				// n, don't bother
			
			mov		rbx, rsi
			or		rbx, rdx
			test    rbx, 0xf				// both src rows 16 byte aligned?
			jnz		vMaybeSSEMMX			// n, don't use sse2
			
			shr		ecx, 1					// do 16 bytes at a time instead
			dec		ecx						// jigger loop ct
		    align	16
			movdqu  xmm0, FPround1
			movdqu	xmm5, vWeight1
			movdqu	xmm6, vWeight2
			pxor	xmm7, xmm7

			align   16
	vLoopSSE2_Fetch:	
			prefetcht0 [rsi+rax*2+16]
			prefetcht0 [rdx+rax*2+16]

	vLoopSSE2:	
			movdqu	xmm1, xmmword ptr[rsi+rax] // top of 2 lines to interpolate
			movdqu	xmm3, xmmword ptr[rdx+rax] // 2nd of 2 lines
			movdqa  xmm2, xmm1
			movdqa  xmm4, xmm3

			punpcklbw xmm1, xmm7			// make words
			punpckhbw xmm2, xmm7			// "
			punpcklbw xmm3, xmm7			// "
			punpckhbw xmm4, xmm7			// "

			pmullw	xmm1, xmm5				// mult by top weighting factor
			pmullw	xmm2, xmm5              // "
			pmullw	xmm3, xmm6				// mult by bot weighting factor
			pmullw	xmm4, xmm6              // "

			paddw	xmm1, xmm3				// combine lumas low
			paddw	xmm2, xmm4				// combine lumas high

			paddusw	xmm1, xmm0				// round
			paddusw	xmm2, xmm0				// round
			
			psrlw	xmm1, 8					// right adjust luma
			psrlw	xmm2, 8					// right adjust luma
						
			packuswb xmm1, xmm2				// pack words to our 16 byte answer
			movntdq	xmmword ptr[rdi+rax], xmm1	// save lumas in our work area

			lea     eax, [eax+16]
			dec		ecx						// don
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
			test    SSEMMXenabledW,1		// is SSE supported?
			jz		vLoopMMX				// n, can't do anyway
			dec     ecx						// jigger loop ctr

		    align	16
	vLoopSSEMMX_Fetch:
			prefetcht0 [rsi+rax+8]
			prefetcht0 [rdx+rax+8]
	
	vLoopSSEMMX:
			movq	mm1, qword ptr[rsi+rax] // top of 2 lines to interpolate
			movq	mm3, qword ptr[rdx+rax] // 2nd of 2 lines
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
			
			packuswb mm1,mm2				// pack UV's into low dword

			movntq	qword ptr[rdi+rax], mm1	// save in our work area

			lea     eax, [eax+8]
			dec		ecx
			jg		vLoopSSEMMX_Fetch			// if not on last one loop, prefetch
			jz		vLoopSSEMMX				// or just loop, or not
			sfence
			jmp		MoreSpareChange			// all done with vertical

		    align	16
	vLoopMMX:	
			movq	mm1, qword ptr[rsi+rax] // top of 2 lines to interpolate
			movq	mm3, qword ptr[rdx+rax] // 2nd of 2 lines
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
			
			packuswb mm1,mm2				// pack UV's into low dword

			movq	qword ptr[rdi+rax], mm1	// save lumas in our work area
	
			lea     eax, [eax+8]
			loop	vLoopMMX

// Add a little code here to check if we have more pixels to do and, if so, make one
// more pass thru vLoopMMX. We were processing in multiples of 8 pixels and alway have
// an even number so there will never be more than 7 left. 
	MoreSpareChange:
			cmp		eax, src_row_size		// did we get them all
			jnl		DoHorizontal			// yes, else have 2 left
			mov		ecx, 1					// jigger loop ct
			mov		eax, src_row_size
			sub		eax, 8					// back up to last 8 pixels
			jmp		vLoopMMX
	

// We've taken care of the vertical scaling, now do horizontal
	DoHorizontal:
			pxor    mm7, mm7
			movq	mm6, FPround2		// useful rounding constant, dwords
			mov		rsi, pControl		// @ horiz control bytes			
			mov		ecx, row_size
			shr		ecx, 2				// 4 bytes a time, 4 pixels
			mov     rdx, vWorkYW		// our luma data
			mov		rdi, dstp			// the destination line
			test    SSEMMXenabledW,1		// is SSE2 supported?
			jz		hLoopMMX				// n

// With SSE support we will make 8 pixels (from 8 pairs) at a time
			shr		ecx, 1				// 8 bytes a time instead of 4
			jz		LessThan8
			align 16
	hLoopMMXSSE:   
			// handle first 2 pixels			
			mov		eax, [rsi+16]		// get data offset in pixels, 1st pixel pair
			mov		ebx, [rsi+20]		// get data offset in pixels, 2nd pixel pair
			movd	mm0, [rdx+rax]		// copy luma pair 0000xxYY
			punpcklwd mm0, [rdx+rbx]    // 2nd luma pair, now xxxxYYYY
			punpcklbw mm0, mm7		    // make words out of bytes, 0Y0Y0Y0Y
			mov		eax, [rsi+16+24]	// get data offset in pixels, 3rd pixel pair
			mov		ebx, [rsi+20+24]	// get data offset in pixels, 4th pixel pair
			pmaddwd mm0, [rsi]			// mult and sum lumas by ctl weights
			paddusw	mm0, mm6			// round
			psrlw	mm0, 8				// right just 4 luma pixel value 0Y0Y0Y0Y

			// handle 3rd and 4th pixel pairs			
			movd	mm1, [rdx+rax]		// copy luma pair 0000xxYY
			punpcklwd mm1, [rdx+rbx]    // 2nd luma pair, now xxxxYYYY
			punpcklbw mm1, mm7		    // make words out of bytes, 0Y0Y0Y0Y
			mov		eax, [rsi+16+48]	// get data offset in pixels, 5th pixel pair
			mov		ebx, [rsi+20+48]	// get data offset in pixels, 6th pixel pair
			pmaddwd mm1, [rsi+24]		// mult and sum lumas by ctl weights
			paddusw	mm1, mm6			// round
			psrlw	mm1, 8				// right just 4 luma pixel value 0Y0Y0Y0Y

			// handle 5th and 6th pixel pairs			
			movd	mm2, [rdx+rax]		// copy luma pair 0000xxYY
			punpcklwd mm2, [rdx+rbx]    // 2nd luma pair, now xxxxYYYY
			punpcklbw mm2, mm7		    // make words out of bytes, 0Y0Y0Y0Y
			mov		eax, [rsi+16+72]	// get data offset in pixels, 7th pixel pair
			mov		ebx, [rsi+20+72]	// get data offset in pixels, 8th pixel pair
			pmaddwd mm2, [rsi+48]			// mult and sum lumas by ctl weights
			paddusw	mm2, mm6			// round
			psrlw	mm2, 8				// right just 4 luma pixel value 0Y0Y0Y0Y

			// handle 7th and 8th pixel pairs			
			movd	mm3, [rdx+rax]		// copy luma pair
			punpcklwd mm3, [rdx+rbx]    // 2nd luma pair
			punpcklbw mm3, mm7		    // make words out of bytes
			pmaddwd mm3, [rsi+72]		// mult and sum lumas by ctl weights
			paddusw	mm3, mm6			// round
			psrlw	mm3, 8				// right just 4 luma pixel value 0Y0Y0Y0Y

			// combine, store, and loop
			packuswb mm0,mm1			// pack into qword, 0Y0Y0Y0Y
			packuswb mm2,mm3			// pack into qword, 0Y0Y0Y0Y
			packuswb mm0,mm2			// and again into  YYYYYYYY				
			movntq	qword ptr[rdi], mm0	// done with 4 pixels

			lea    rsi, [rsi+96]		// bump to next control bytest
			lea    rdi, [rdi+8]			// bump to next output pixel addr
			dec	   ecx
            jg	   hLoopMMXSSE				// loop for more
			sfence

	LessThan8:
			mov		ecx, row_size
			and		ecx, 7				// we have done all but maybe this
			shr		ecx, 2				// now do only 4 bytes at a time
			jz		LessThan4

			align 16
	hLoopMMX:   
			// handle first 2 pixels			
			mov		eax, [rsi+16]		// get data offset in pixels, 1st pixel pair
			mov		ebx, [rsi+20]		// get data offset in pixels, 2nd pixel pair
			movd	mm0, [rdx+rax]		// copy luma pair 0000xxYY
			punpcklwd mm0, [rdx+rbx]    // 2nd luma pair, now xxxxYYYY
			punpcklbw mm0, mm7		    // make words out of bytes, 0Y0Y0Y0Y
			mov		eax, [rsi+16+24]	// get data offset in pixels, 3rd pixel pair
			mov		ebx, [rsi+20+24]	// get data offset in pixels, 4th pixel pair
			pmaddwd mm0, [rsi]			// mult and sum lumas by ctl weights
			paddusw	mm0, mm6			// round
			psrlw	mm0, 8				// right just 4 luma pixel value 0Y0Y0Y0Y

			// handle 3rd and 4th pixel pairs			
			movd	mm1, [rdx+rax]		// copy luma pair
			punpcklwd mm1, [rdx+rbx]    // 2nd luma pair
			punpcklbw mm1, mm7		    // make words out of bytes
			pmaddwd mm1, [rsi+24]			// mult and sum lumas by ctl weights
			paddusw	mm1, mm6			// round
			psrlw	mm1, 8				// right just 4 luma pixel value 0Y0Y0Y0Y

			// combine, store, and loop
			packuswb mm0,mm1			// pack all into qword, 0Y0Y0Y0Y
			packuswb mm0,mm7			// and again into  0000YYYY				
			movd	dword ptr[rdi], mm0	// done with 4 pixels
			lea    rsi, [rsi+48]		// bump to next control bytest
			lea    rdi, [rdi+4]			// bump to next output pixel addr
            loop   hLoopMMX				// loop for more

			// test to see if we have a mod 4 size row, if not then more spare change
	LessThan4:
			mov		ecx, row_size
			and		ecx, 3				// remainder size mod 4
			cmp		ecx, 2
			jl		LastOne				// none, done

			// handle 2 more pixels			
			mov		eax, [rsi+16]		// get data offset in pixels, 1st pixel pair
			mov		ebx, [rsi+20]		// get data offset in pixels, 2nd pixel pair
			movd	mm0, [rdx+rax]		// copy luma pair 0000xxYY
			punpcklwd mm0, [rdx+rbx]    // 2nd luma pair, now xxxxYYYY
			punpcklbw mm0, mm7		    // make words out of bytes, 0Y0Y0Y0Y

			pmaddwd mm0, [rsi]			// mult and sum lumas by ctl weights
			paddusw	mm0, mm6			// round
			psrlw	mm0, 8				// right just 2 luma pixel value 000Y,000Y
			packuswb mm0,mm7			// pack all into qword, 00000Y0Y
			packuswb mm0,mm7			// and again into  000000YY		
			movd	dword ptr[rdi], mm0	// store, we are guarrenteed room in buffer (8 byte mult)
			sub		ecx, 2
			lea		rsi, [rsi+24]		// bump to next control bytest
			lea		rdi, [rdi+2]			// bump to next output pixel addr

			// maybe one last pixel
	LastOne:
			cmp		ecx, 0				// still more?
			jz		AllDone				// n, done
		
			mov		eax, [rsi+16]		// get data offset in pixels, 1st pixel pair
			movd	mm0, [rdx+rax]		// copy luma pair 0000xxYY
			punpcklbw mm0, mm7		    // make words out of bytes, xxxx0Y0Y

			pmaddwd mm0, [rsi]			// mult and sum lumas by ctl weights
			paddusw	mm0, mm6			// round
			psrlw	mm0, 8				// right just 2 luma pixel value xxxx000Y
			movd	eax, mm0			
			mov		byte ptr[rdi], al	// store last one

	AllDone:
		//	pop		ecx
            emms
		}                               // done with one line
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

void SimpleResize::InitTables_YV12(void)
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

	for(i=0; i < newwidth; i+=2)
	{
		// first make even pixel control
		j = i * 256 * (oldwidth) / (newwidth)  + (128*oldwidth) / newwidth - 128;//  wrong scale fixed by Fizick
		if (j<0) j=0; // added by Fizick

		k = j>>8;
		wY2 = j - (k << 8);				// luma weight of right pixel
		wY1 = 256 - wY2;				// luma weight of left pixel

		if (k > oldwidth - 2)
		{
			hControl[i*3+4] = oldwidth - 1;	 //	point to last byte
			hControl[i*3] =   0x00000100;    // use 100% of rightmost Y
		}
		else
		{
			hControl[i*3+4] = k;			// pixel offset
			hControl[i*3] = wY2 << 16 | wY1; // luma weights
		}

		// now make odd pixel control
		j = (i+1) * 256 * (oldwidth) / (newwidth)  + (128*oldwidth)/newwidth - 128 ;// wrong scale fixed by Fizick - 
		if (j<0) j=0; // added by Fizick

		k = j>>8;
		wY2 = j - (k << 8);				// luma weight of right pixel
		wY1 = 256 - wY2;				// luma weight of left pixel

		if (k > oldwidth - 2)
		{
			hControl[i*3+5] = oldwidth - 1;	 //	point to last byte
			hControl[i*3+1] = 0x00000100;    // use 100% of rightmost Y
		}
		else
		{
			hControl[i*3+5] = k;			// pixel offset
			hControl[i*3+1] = wY2 << 16 | wY1; // luma weights
		}
	}

	hControl[newwidth*3+4] =  2 * (oldwidth-1);		// give it something to prefetch at end
	hControl[newwidth*3+5] =  2 * (oldwidth-1);		// "
	hControl[newwidth*3+4] =  2 * (oldwidth-1);		// give it something to prefetch at end
	hControl[newwidth*3+5] =  2 * (oldwidth-1);		// "


	// Croma UV plane - code added by Fizick
	for(i=0; i < newwidth/2; i+=2)
	{
		// first make even pixel control
		j = i * 256 * (oldwidth) / (newwidth) + (128*oldwidth)/newwidth - 128; // Fixed by Fizick
		if (j<0) j=0; // added by Fizick

		k = j>>8;
		wY2 = j - (k << 8);				// luma weight of right pixel
		wY1 = 256 - wY2;				// luma weight of left pixel

		if (k > oldwidth/2 - 2 )
		{
			hControlUV[i*3+4] = oldwidth/2 - 1;	 //	point to last byte
			hControlUV[i*3] =   0x00000100;    // use 100% of rightmost Y
		}
		else
		{
			hControlUV[i*3+4] = k;			// pixel offset
			hControlUV[i*3] = wY2 << 16 | wY1; // luma weights
		}

		// now make odd pixel control
		j = (i+1) * 256 * (oldwidth) / (newwidth) + (128*oldwidth)/newwidth - 128;
		if (j<0) j=0; // added by Fizick

		k = j>>8;
		wY2 = j - (k << 8);				// luma weight of right pixel
		wY1 = 256 - wY2;				// luma weight of left pixel

		if (k > oldwidth/2 - 2)
		{
			hControlUV[i*3+5] = oldwidth/2 - 1;	 //	point to last byte
			hControlUV[i*3+1] = 0x00000100;    // use 100% of rightmost Y
		}
		else
		{
			hControlUV[i*3+5] = k;			// pixel offset
			hControlUV[i*3+1] = wY2 << 16 | wY1; // luma weights
		}
	}

	hControlUV[newwidth/2*3+4] =  2 * (oldwidth/2-1);		// give it something to prefetch at end
	hControlUV[newwidth/2*3+5] =  2 * (oldwidth/2-1);		// "
	hControlUV[newwidth/2*3+4] =  2 * (oldwidth/2-1);		// give it something to prefetch at end
	hControlUV[newwidth/2*3+5] =  2 * (oldwidth/2-1);		// "



	// Next set up vertical tables. The offsets are measured in lines and will be mult
	// by the source pitch later .

	// For YV12 we need separate Luma and chroma tables
	// First Luma Table
	
	for(i=0; i< newheight; ++i)
	{
		j = i * 256 * (oldheight) / (newheight) + (128*oldheight)/newheight - 128; // Fizick
		if (j<0) j=0; // added by Fizick

			k = j >> 8;
			if (k > oldheight-2) { k = oldheight-1; j = k<<8; } // added by Fizick
			vOffsets[i] = k;
			wY2 = j - (k << 8); 
			vWeights[i] = wY2;				// weight to give to 2nd line
	}

	// Vertical table for chroma
		for(i=0; i< newheight/2; ++i)
	{
		j = (int) ( i * 256 * (oldheight) / (newheight) + (128*oldheight)/newheight - 128 ); // Fizick
		if (j<0) j=0; // added by Fizick
			k = j >> 8;
			if (k > oldheight/2-2) { k = oldheight/2-1; j = k<<8; } // added by Fizick
			vOffsetsUV[i] = k;
			wY2 = j - (k << 8); 
			vWeightsUV[i] = wY2;				// weight to give to 2nd line
	}
}
