// Create an overlay mask with the motion vectors
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



#include "MaskFun.h"
#include <intrin.h>

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



void MakeVectorOcclusionMaskTime(MVClip &mvClip, int nBlkX, int nBlkY, double dMaskNormFactor, double fGamma, int nPel, uint8_t * occMask, int occMaskPitch, int time256, int blkSizeX, int blkSizeY)
{	// analyse vectors field to detect occlusion
	MemZoneSet(occMask, 0, nBlkX, nBlkY, 0, 0, nBlkX);
	int time4096X = time256*16/blkSizeX;
	int time4096Y = time256*16/blkSizeY;
	_mm_empty();
	double occnorm = 10 / dMaskNormFactor/nPel;
	int occlusion;

	for (int by=0; by<nBlkY; by++)
	{
		for (int bx=0; bx<nBlkX; bx++)
		{
			int i = bx + by*nBlkX; // current block
			const FakeBlockData &block = mvClip.GetBlock(0, i);
			int vx = block.GetMV().x;
			int vy = block.GetMV().y;
			if (bx < nBlkX-1) // right neighbor
			{
				int i1 = i+1;
				const FakeBlockData &block1 = mvClip.GetBlock(0, i1);
				int vx1 = block1.GetMV().x;
				//int vy1 = block1.GetMV().y;
				if (vx1<vx) {
					occlusion = vx-vx1;
					for (int bxi=bx+vx1*time4096X/4096; bxi<=bx+vx*time4096X/4096+1 && bxi>=0 && bxi<nBlkX; bxi++)
						ByteOccMask(&occMask[bxi+by*occMaskPitch], occlusion, occnorm, fGamma);
				}
			}
			if (by < nBlkY-1) // bottom neighbor
			{
				int i1 = i + nBlkX;
				const FakeBlockData &block1 = mvClip.GetBlock(0, i1);
				//int vx1 = block1.GetMV().x;
				int vy1 = block1.GetMV().y;
				if (vy1<vy) {
					occlusion = vy-vy1;
					for (int byi=by+vy1*time4096Y/4096; byi<=by+vy*time4096Y/4096+1 && byi>=0 && byi<nBlkY; byi++)
						ByteOccMask(&occMask[bx+byi*occMaskPitch], occlusion, occnorm, fGamma);
				}
			}
		}
	}
}



void VectorMasksToOcclusionMaskTime(uint8_t *VXMask, uint8_t *VYMask, int nBlkX, int nBlkY, double dMaskNormFactor, double fGamma, int nPel, uint8_t * occMask, int occMaskPitch, int time256, int blkSizeX, int blkSizeY)
{	// analyse vectors field to detect occlusion
	MemZoneSet(occMask, 0, nBlkX, nBlkY, 0, 0, nBlkX);
	int time4096X = time256*16/blkSizeX;
	int time4096Y = time256*16/blkSizeY;
	_mm_empty();
	double occnorm = 10 / dMaskNormFactor/nPel;
	int occlusion;
	for (int by=0; by<nBlkY; by++)
	{
		for (int bx=0; bx<nBlkX; bx++)
		{
			int i = bx + by*nBlkX; // current block
			int vx = VXMask[i]-128;
			int vy = VYMask[i]-128;
			int bxv = bx+vx*time4096X/4096;
			int byv = by+vy*time4096Y/4096;
			if (bx < nBlkX-1) // right neighbor
			{
				int i1 = i+1;
				int vx1 = VXMask[i1]-128;
				if (vx1<vx) {
					occlusion = vx-vx1;
					for (int bxi=bx+vx1*time4096X/4096; bxi<=bx+vx*time4096X/4096+1 && bxi>=0 && bxi<nBlkX; bxi++)
					ByteOccMask(&occMask[bxi+byv*occMaskPitch], occlusion, occnorm, fGamma);
				}
			}
			if (by < nBlkY-1) // bottom neighbor
			{
				int i1 = i + nBlkX;
				int vy1 = VYMask[i1]-128;
				if (vy1<vy) {
					occlusion = vy-vy1;
					for (int byi=by+vy1*time4096Y/4096; byi<=by+vy*time4096Y/4096+1 && byi>=0 && byi<nBlkY; byi++)
						ByteOccMask(&occMask[bxv+byi*occMaskPitch], occlusion, occnorm, fGamma);
				}
			}
		}
	}
}



// it is old pre 1.8 version
void MakeVectorOcclusionMask(MVClip &mvClip, int nBlkX, int nBlkY, double dMaskNormFactor, double fGamma, int nPel, uint8_t * occMask, int occMaskPitch)
{	// analyse vectors field to detect occlusion
	_mm_empty();
	double occnorm = 10 / dMaskNormFactor/nPel; // empirical
	for (int by=0; by<nBlkY; by++)
	{
		for (int bx=0; bx<nBlkX; bx++)
		{
			int occlusion = 0;
			int i = bx + by*nBlkX; // current block
			const FakeBlockData &block = mvClip.GetBlock(0, i);
			int vx = block.GetMV().x;
			int vy = block.GetMV().y;
			if (bx > 0) // left neighbor
			{
				int i1 = i-1;
				const FakeBlockData &block1 = mvClip.GetBlock(0, i1);
				int vx1 = block1.GetMV().x;
				//int vy1 = block1.GetMV().y;
				if (vx1>vx) occlusion += (vx1-vx); // only positive (abs)
			}
			if (bx < nBlkX-1) // right neighbor
			{
				int i1 = i+1;
				const FakeBlockData &block1 = mvClip.GetBlock(0, i1);
				int vx1 = block1.GetMV().x;
				//int vy1 = block1.GetMV().y;
				if (vx1<vx) occlusion += vx-vx1;
			}
			if (by > 0) // top neighbor
			{
				int i1 = i - nBlkX;
				const FakeBlockData &block1 = mvClip.GetBlock(0, i1);
				//int vx1 = block1.GetMV().x;
				int vy1 = block1.GetMV().y;
				if (vy1>vy) occlusion += vy1-vy;
			}
			if (by < nBlkY-1) // bottom neighbor
			{
				int i1 = i + nBlkX;
				const FakeBlockData &block1 = mvClip.GetBlock(0, i1);
				//int vx1 = block1.GetMV().x;
				int vy1 = block1.GetMV().y;
				if (vy1<vy) occlusion += vy-vy1;
			}
			if (fGamma == 1.0)
			{
				occMask[bx + by*occMaskPitch] =
					std::min (int (255 *      occlusion*occnorm         ), 255);
			}
			else
			{
				occMask[bx + by*occMaskPitch] =
					std::min (int (255 * pow (occlusion*occnorm, fGamma)), 255);
			}
		}
	}
}


// it is olr pre 1.8 mask
void VectorMasksToOcclusionMask(uint8_t *VXMask, uint8_t *VYMask, int nBlkX, int nBlkY, double dMaskNormFactor, double fGamma, int nPel, uint8_t * occMask)
{	// analyse vectors field to detect occlusion
	// note: dMaskNormFactor was = 1/ml, now is = ml
	_mm_empty();
	double occnorm = 10 / dMaskNormFactor/nPel; // empirical
	for (int by=0; by<nBlkY; by++)
	{
		for (int bx=0; bx<nBlkX; bx++)
		{
			int occlusion = 0;
			int i = bx + by*nBlkX; // current block
			int vx = VXMask[i];
			int vy = VYMask[i];
			if (bx > 0) // left neighbor
			{
				int i1 = i-1;
				int vx1 = VXMask[i1];
//				int vy1 = VYMask[i1];
				if (vx1>vx) occlusion += (vx1-vx); // only positive (abs)
			}
			if (bx < nBlkX-1) // right neighbor
			{
				int i1 = i+1;
				int vx1 = VXMask[i1];
//				int vy1 = VYMask[i1];
				if (vx1<vx) occlusion += vx-vx1;
			}
			if (by > 0) // top neighbor
			{
				int i1 = i - nBlkX;
//				int vx1 = VXMask[i1];
				int vy1 = VYMask[i1];
				if (vy1>vy) occlusion += vy1-vy;
			}
			if (by < nBlkY-1) // bottom neighbor
			{
				int i1 = i + nBlkX;
//				int vx1 = VXMask[i1];
				int vy1 = VYMask[i1];
				if (vy1<vy) occlusion += vy-vy1;
			}
			if (fGamma == 1.0)
			{
				occMask[i] = std::min (int (255 *      occlusion*occnorm),          255);
			}
			else
			{
				occMask[i] = std::min (int (255 * pow (occlusion*occnorm, fGamma)), 255);
			}
		}
	}
}

void MakeVectorSmallMasks(MVClip &mvClip, int nBlkX, int nBlkY, uint8_t *VXSmallY, int pitchVXSmallY, uint8_t*VYSmallY, int pitchVYSmallY)
{
	// make  vector vx and vy small masks
	// 1. ATTENTION: vectors are assumed SHORT (|vx|, |vy| < 127) !
	// 2. they will be zeroed if not
	// 3. added 128 to all values
	for (int by=0; by<nBlkY; by++)
	{
		for (int bx=0; bx<nBlkX; bx++)
		{
			int i = bx + by*nBlkX;
			const FakeBlockData &block = mvClip.GetBlock(0, i);
			int vx = block.GetMV().x;
			int vy = block.GetMV().y;
			if (vx>127) vx= 127;
			else if (vx<-127) vx= -127;
			if (vy>127) vy = 127;
			else if (vy<-127) vy = -127;
			VXSmallY[bx+by*pitchVXSmallY] = vx + 128; // luma
			VYSmallY[bx+by*pitchVYSmallY] = vy + 128; // luma
		}
	}
}

void VectorSmallMaskYToHalfUV(uint8_t * VSmallY, int nBlkX, int nBlkY, uint8_t *VSmallUV, int ratioUV)
{
	if (ratioUV==2)
	{
		// YV12 colorformat
		for (int by=0; by<nBlkY; by++)
		{
			for (int bx=0; bx<nBlkX; bx++)
			{
				VSmallUV[bx] = ((VSmallY[bx]-128)>>1) + 128; // chroma
			}
			VSmallY += nBlkX;
			VSmallUV += nBlkX;
		}
	}
	else // ratioUV==1
	{
		// Height YUY2 colorformat
		for (int by=0; by<nBlkY; by++)
		{
			for (int bx=0; bx<nBlkX; bx++)
			{
				VSmallUV[bx] = VSmallY[bx]; // chroma
			}
			VSmallY += nBlkX;
			VSmallUV += nBlkX;
		}
	}

}


void Merge4PlanesToBig(
	uint8_t *pel2Plane, int pel2Pitch, const uint8_t *pPlane0, const uint8_t *pPlane1,
	const uint8_t *pPlane2, const uint8_t * pPlane3, int width, int height, int pitch, bool isse)
{
	// copy refined planes to big one plane
	if (!isse)
	{
		for (int h=0; h<height; h++)
		{
			for (int w=0; w<width; w++)
			{
				pel2Plane[w<<1] = pPlane0[w];
				pel2Plane[(w<<1) +1] = pPlane1[w];
			}
			pel2Plane += pel2Pitch;
			for (int w=0; w<width; w++)
			{
				pel2Plane[w<<1] = pPlane2[w];
				pel2Plane[(w<<1) +1] = pPlane3[w];
			}
			pel2Plane += pel2Pitch;
			pPlane0 += pitch;
			pPlane1 += pitch;
			pPlane2 += pitch;
			pPlane3 += pitch;
		}
	}
	else // isse - not very optimized
	{
		_asm
		{
#if !defined(_M_X64)
#define pitch_ pitch
#define pel2Pitch_ pel2Pitch
#else
#define pitch_ r8
#define pel2Pitch_ r9
		movsx_int pitch_,pitch
		movsx_int pel2Pitch_,pel2Pitch
#endif
		//	push ebx;
		//	push esi;
		//	push edi;
			mov rdi, pel2Plane;
			mov rsi, pPlane0;
			mov rdx, pPlane1;

			mov ebx, height;
looph1:
			mov ecx, width;
			mov eax, 0;
align 16
loopw1:
			movd mm0, [rsi+rax];
			movd mm1, [rdx+rax];
			punpcklbw mm0, mm1;
			shl eax, 1;
			movq [rdi+rax], mm0;
			shr eax, 1;
			add eax, 4;
			cmp eax, ecx;
			jl loopw1;

			mov rax, pel2Pitch_;
			add rdi, rax;
			add rdi, rax;
			mov rax, pitch_;
			add rsi, rax;
			add rdx, rax;
			dec ebx;
			cmp ebx, 0;
			jg looph1;

			mov rdi, pel2Plane;
			mov rsi, pPlane2;
			mov rdx, pPlane3;

			mov rax, pel2Pitch_;
			add rdi, rax;

			mov ebx, height;
looph2:
			mov ecx, width;
			mov eax, 0;
align 16
loopw2:
			movd mm0, [rsi+rax];
			movd mm1, [rdx+rax];
			punpcklbw mm0, mm1;
			shl eax, 1;
			movq [rdi+rax], mm0;
			shr eax, 1;
			add eax, 4;
			cmp eax, ecx;
			jl loopw2;

			mov rax, pel2Pitch_;
			add rdi, rax;
			add rdi, rax;
			mov rax, pitch_;
			add rsi, rax;
			add rdx, rax;
			dec ebx;
			cmp ebx, 0;
			jg looph2;

		//	pop edi;
		//	pop esi;
		//	pop ebx;
			emms;
#undef src_pitch_
#undef pel2Pitch_
		}
	}
}


void Merge16PlanesToBig(
	uint8_t *pel4Plane, int pel4Pitch,
	const uint8_t *pPlane0,  const uint8_t *pPlane1,  const uint8_t *pPlane2,  const uint8_t *pPlane3,
	const uint8_t *pPlane4,  const uint8_t *pPlane5,  const uint8_t *pPlane6,  const uint8_t *pPlane7,
	const uint8_t *pPlane8,  const uint8_t *pPlane9,  const uint8_t *pPlane10, const uint8_t *pPlane11,
	const uint8_t *pPlane12, const uint8_t *pPlane13, const uint8_t *pPlane14, const uint8_t *pPlane15,
	int width, int height, int pitch, bool isse)
{
	// copy refined planes to big one plane
//	if (!isse)
	{
		for (int h=0; h<height; h++)
		{
			for (int w=0; w<width; w++)
			{
				pel4Plane[w<<2] = pPlane0[w];
				pel4Plane[(w<<2) +1] = pPlane1[w];
				pel4Plane[(w<<2) +2] = pPlane2[w];
				pel4Plane[(w<<2) +3] = pPlane3[w];
			}
			pel4Plane += pel4Pitch;
			for (int w=0; w<width; w++)
			{
				pel4Plane[w<<2] = pPlane4[w];
				pel4Plane[(w<<2) +1] = pPlane5[w];
				pel4Plane[(w<<2) +2] = pPlane6[w];
				pel4Plane[(w<<2) +3] = pPlane7[w];
			}
			pel4Plane += pel4Pitch;
			for (int w=0; w<width; w++)
			{
				pel4Plane[w<<2] = pPlane8[w];
				pel4Plane[(w<<2) +1] = pPlane9[w];
				pel4Plane[(w<<2) +2] = pPlane10[w];
				pel4Plane[(w<<2) +3] = pPlane11[w];
			}
			pel4Plane += pel4Pitch;
			for (int w=0; w<width; w++)
			{
				pel4Plane[w<<2] = pPlane12[w];
				pel4Plane[(w<<2) +1] = pPlane13[w];
				pel4Plane[(w<<2) +2] = pPlane14[w];
				pel4Plane[(w<<2) +3] = pPlane15[w];
			}
			pel4Plane += pel4Pitch;
			pPlane0 += pitch;
			pPlane1 += pitch;
			pPlane2 += pitch;
			pPlane3 += pitch;
			pPlane4 += pitch;
			pPlane5 += pitch;
			pPlane6 += pitch;
			pPlane7 += pitch;
			pPlane8 += pitch;
			pPlane9 += pitch;
			pPlane10 += pitch;
			pPlane11 += pitch;
			pPlane12 += pitch;
			pPlane13+= pitch;
			pPlane14 += pitch;
			pPlane15 += pitch;
		}
	}
}

//-----------------------------------------------------------
unsigned char SADToMask(unsigned int sad, unsigned int sadnorm1024)
{
	// sadnorm1024 = 255 * (4*1024)/(mlSAD*nBlkSize*nBlkSize*chromablockfactor)
	unsigned int l = sadnorm1024*sad/1024;
	return (unsigned char)((l > 255) ? 255 : l);
}


void Create_LUTV(int time256, short *LUTVB, short *LUTVF)
{
	for (int v=0; v<256; v++)
	{
		LUTVB[v] = ((v-128)*(256-time256))/256;
		LUTVF[v] = ((v-128)*time256)/256;
	}
}
