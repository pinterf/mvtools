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
#include <emmintrin.h>
#include <cassert>

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
  // fix: 2.7.19.22: use occMaskPitch instead of nBlkX (result of 30 hours debug)
  // or else we have random garbage on bottom right part
	MemZoneSet(occMask, 0, nBlkX, nBlkY, 0, 0, occMaskPitch); 
	int time4096X = time256*16/blkSizeX;
	int time4096Y = time256*16/blkSizeY;
#ifndef _M_X64 
  _mm_empty ();
#endif
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
#ifndef _M_X64 
  _mm_empty ();
#endif
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
#ifndef _M_X64 
  _mm_empty ();
#endif
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
#ifndef _M_X64 
  _mm_empty ();
#endif
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

// creates 8 bit sad mask value from (8-16bit) block sad, scaling through dSADNormFactor
unsigned char ByteNorm(sad_t sad, double dSADNormFactor, double fGamma)
{
  //	    double dSADNormFactor = 4 / (dMaskNormFactor*blkSizeX*blkSizeY);
  double l = 255 * pow(sad*dSADNormFactor, fGamma); // Fizick - now linear for gm=1
  return (unsigned char)((l > 255) ? 255 : l);
}


void MakeSADMaskTime(MVClip &mvClip, int nBlkX, int nBlkY, double dSADNormFactor, double fGamma, int nPel, BYTE * Mask, int MaskPitch, int time256, int nBlkStepX, int nBlkStepY)
{
  // Make approximate SAD mask at intermediate time
  //    double dSADNormFactor = 4 / (dMaskNormDivider*nBlkSizeX*nBlkSizeY);
  MemZoneSet(Mask, 0, nBlkX, nBlkY, 0, 0, MaskPitch);
  int time4096X = (256 - time256) * 16 / (nBlkStepX*nPel); // blkstep here is really blksize-overlap
  int time4096Y = (256 - time256) * 16 / (nBlkStepY*nPel);

  for (int by = 0; by<nBlkY; by++)
  {
    for (int bx = 0; bx<nBlkX; bx++)
    {
      int i = bx + by*nBlkX; // current block
      const FakeBlockData &block = mvClip.GetBlock(0, i);
      int vx = block.GetMV().x;
      int vy = block.GetMV().y;
      int bxi = bx - vx*time4096X / 4096; // limits?
      int byi = by - vy*time4096Y / 4096;
      if (bxi <0 || bxi >= nBlkX || byi <0 || byi >= nBlkY)
      {
        bxi = bx;
        byi = by;
      }
      int i1 = bxi + byi*nBlkX;
      sad_t sad = mvClip.GetBlock(0, i1).GetSAD();
      Mask[bx + by*nBlkX] = ByteNorm(sad, dSADNormFactor, fGamma); // bits_per_pixel scale through dSADNormFactor
    }
  }
}


void MakeVectorSmallMasks(MVClip &mvClip, int nBlkX, int nBlkY, short *VXSmallY, int pitchVXSmallY, short *VYSmallY, int pitchVYSmallY)
{
  // make  vector vx and vy small masks
  for (int by = 0; by<nBlkY; by++)
  {
    for (int bx = 0; bx<nBlkX; bx++)
    {
      int i = bx + by*nBlkX;
      const FakeBlockData &block = mvClip.GetBlock(0, i);
      int vx = block.GetMV().x;
      int vy = block.GetMV().y;
      VXSmallY[bx + by*pitchVXSmallY] = vx; // luma
      VYSmallY[bx + by*pitchVYSmallY] = vy; // luma
    }
  }
}


// simply copies (ratioUV==1) /or halves (ratioUV==2) VSmallY[x,y] to VSmallUV[x,y]
// x:0..nBlkX,   y:0..nBlkY
// it can be called with X and Y vectors, ratioUV can be xRatioUV or yRatioUV
void VectorSmallMaskYToHalfUV(short * VSmallY, int nBlkX, int nBlkY, short *VSmallUV, int ratioUV)
{
  if (ratioUV == 2)
  {
    // e.g. YV12 colorformat 
    for (int by = 0; by<nBlkY; by++)
    {
      for (int bx = 0; bx<nBlkX; bx++)
      {
        VSmallUV[bx] = ((VSmallY[bx]) >> 1); // chroma
      }
      VSmallY += nBlkX;
      VSmallUV += nBlkX;
    }
  }
  else // ratioUV==1
  {
    // e.g. Height YUY2 colorformat
    for (int by = 0; by<nBlkY; by++)
    {
      for (int bx = 0; bx<nBlkX; bx++)
      {
        VSmallUV[bx] = VSmallY[bx]; // chroma
      }
      VSmallY += nBlkX;
      VSmallUV += nBlkX;
    }
  }

}

template<typename pixel_t>
static void Merge4PlanesToBig_c(
  uint8_t *pel2Plane, int pel2Pitch, const uint8_t *pPlane0, const uint8_t *pPlane1,
  const uint8_t *pPlane2, const uint8_t * pPlane3, int width, int height, int pitch, int bits_per_pixel)
{
  
  for (int h=0; h<height; h++)
  {
    for (int w=0; w<width; w++)
    {
      reinterpret_cast<pixel_t *>(pel2Plane)[w<<1] = reinterpret_cast<const pixel_t *>(pPlane0)[w];
      reinterpret_cast<pixel_t *>(pel2Plane)[(w<<1) +1] = reinterpret_cast<const pixel_t *>(pPlane1)[w];
    }
    pel2Plane += pel2Pitch;
    for (int w=0; w<width; w++)
    {
      reinterpret_cast<pixel_t *>(pel2Plane)[w<<1] = reinterpret_cast<const pixel_t *>(pPlane2)[w];
      reinterpret_cast<pixel_t *>(pel2Plane)[(w<<1) +1] = reinterpret_cast<const pixel_t *>(pPlane3)[w];
    }
    pel2Plane += pel2Pitch;
    pPlane0 += pitch;
    pPlane1 += pitch;
    pPlane2 += pitch;
    pPlane3 += pitch;
  }
}

template<typename pixel_t>
static void Merge4PlanesToBig_sse2(
  uint8_t *pel2Plane, int pel2Pitch, const uint8_t *pPlane0, const uint8_t *pPlane1,
  const uint8_t *pPlane2, const uint8_t * pPlane3, int width, int height, int pitch, int bits_per_pixel)
{
  int mod8width = width;
  const int simd_width = width * sizeof(pixel_t);
  const int mod16width = (simd_width / 16) * 16;
  // copy refined planes to big one plane
  // P =  p0 p1 p0 p1 p0 p1 p0 p1...
  //      p2 p3 p2 p3 p2 p3 p2 p3
  for (int h = 0; h < height; h++)
  {
    for (int x = 0; x < mod16width; x += 16)
    {
      // 0-1
      __m128i src0 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pPlane0 + x));
      __m128i src1 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pPlane1 + x));
      __m128i mix_lo, mix_hi;
      if (sizeof(pixel_t) == 1) {
        mix_lo = _mm_unpacklo_epi8(src0, src1);
        mix_hi = _mm_unpackhi_epi8(src0, src1);
      }
      else {
        mix_lo = _mm_unpacklo_epi16(src0, src1);
        mix_hi = _mm_unpackhi_epi16(src0, src1);
      }
      _mm_storeu_si128(reinterpret_cast<__m128i *>(pel2Plane + x * 2), mix_lo);
      _mm_storeu_si128(reinterpret_cast<__m128i *>(pel2Plane + x * 2 + 16), mix_hi);
      // 2-3
      __m128i src2 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pPlane2 + x));
      __m128i src3 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pPlane3 + x));
      if (sizeof(pixel_t) == 1) {
        mix_lo = _mm_unpacklo_epi8(src2, src3);
        mix_hi = _mm_unpackhi_epi8(src2, src3);
      }
      else {
        mix_lo = _mm_unpacklo_epi16(src2, src3);
        mix_hi = _mm_unpackhi_epi16(src2, src3);
      }
      _mm_storeu_si128(reinterpret_cast<__m128i *>(pel2Plane + pel2Pitch + x * 2), mix_lo);
      _mm_storeu_si128(reinterpret_cast<__m128i *>(pel2Plane + pel2Pitch + x * 2 + 16), mix_hi);
    }
    for (int w = mod16width / sizeof(pixel_t); w < width; w++)
    {
      reinterpret_cast<pixel_t *>(pel2Plane)[(w << 1)] = reinterpret_cast<const pixel_t *>(pPlane0)[w];
      reinterpret_cast<pixel_t *>(pel2Plane)[(w << 1) + 1] = reinterpret_cast<const pixel_t *>(pPlane1)[w];
      reinterpret_cast<pixel_t *>(pel2Plane)[(w << 1) + pel2Pitch / sizeof(pixel_t)] = reinterpret_cast<const pixel_t *>(pPlane2)[w];
      reinterpret_cast<pixel_t *>(pel2Plane)[(w << 1) + pel2Pitch / sizeof(pixel_t) + 1] = reinterpret_cast<const pixel_t *>(pPlane3)[w];
    }
    pel2Plane += 2 * pel2Pitch;
    pPlane0 += pitch;
    pPlane1 += pitch;
    pPlane2 += pitch;
    pPlane3 += pitch;
  }
}

void Merge4PlanesToBig(
	uint8_t *pel2Plane, int pel2Pitch, const uint8_t *pPlane0, const uint8_t *pPlane1,
	const uint8_t *pPlane2, const uint8_t * pPlane3, int width, int height, int pitch, int pixelsize, int cpuFlags)
{
	// copy refined planes to big one plane
  // P =  p0 p1 p0 p1 p0 p1 p0 p1...
  //      p2 p3 p2 p3 p2 p3 p2 p3
  // 
  bool isse = !!(cpuFlags & CPUF_SSE2);
	if (!isse || pixelsize == 4)
	{
    if(pixelsize==1)
      Merge4PlanesToBig_c<uint8_t>(pel2Plane, pel2Pitch, pPlane0, pPlane1, pPlane2, pPlane3, width, height, pitch, pixelsize);
    else if(pixelsize==2)
      Merge4PlanesToBig_c<uint16_t>(pel2Plane, pel2Pitch, pPlane0, pPlane1, pPlane2, pPlane3, width, height, pitch, pixelsize);
    else if(pixelsize==4)
      Merge4PlanesToBig_c<float>(pel2Plane, pel2Pitch, pPlane0, pPlane1, pPlane2, pPlane3, width, height, pitch, pixelsize);
    return;
  }
  else {
    if (pixelsize == 1)
      Merge4PlanesToBig_sse2<uint8_t>(pel2Plane, pel2Pitch, pPlane0, pPlane1, pPlane2, pPlane3, width, height, pitch, pixelsize);
    else if (pixelsize == 2)
      Merge4PlanesToBig_sse2<uint16_t>(pel2Plane, pel2Pitch, pPlane0, pPlane1, pPlane2, pPlane3, width, height, pitch, pixelsize);
    else
      assert(0);
  }
  return;
#if 0
#if !(defined(_M_X64) && defined(_MSC_VER))
  else // isse - not very optimized
	{
    // todo SIMD intrinsic
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
#endif // inline assembly MSVC X64 ignore
#endif // if 0
}



template<typename pixel_t>
static void Merge16PlanesToBig_c(
  uint8_t *pel4Plane, int pel4Pitch,
  const uint8_t *pPlane0, const uint8_t *pPlane1, const uint8_t *pPlane2, const uint8_t *pPlane3,
  const uint8_t *pPlane4, const uint8_t *pPlane5, const uint8_t *pPlane6, const uint8_t *pPlane7,
  const uint8_t *pPlane8, const uint8_t *pPlane9, const uint8_t *pPlane10, const uint8_t *pPlane11,
  const uint8_t *pPlane12, const uint8_t *pPlane13, const uint8_t *pPlane14, const uint8_t *pPlane15,
  int width, int height, int pitch)
{
  // copy refined planes to big one plane
//	if (!isse)
  {
    for (int h = 0; h < height; h++)
    {
      for (int w = 0; w < width; w++)
      {
        reinterpret_cast<pixel_t *>(pel4Plane)[w << 2] = reinterpret_cast<const pixel_t *>(pPlane0)[w];
        reinterpret_cast<pixel_t *>(pel4Plane)[(w << 2) + 1] = reinterpret_cast<const pixel_t *>(pPlane1)[w];
        reinterpret_cast<pixel_t *>(pel4Plane)[(w << 2) + 2] = reinterpret_cast<const pixel_t *>(pPlane2)[w];
        reinterpret_cast<pixel_t *>(pel4Plane)[(w << 2) + 3] = reinterpret_cast<const pixel_t *>(pPlane3)[w];
      }
      pel4Plane += pel4Pitch;
      for (int w = 0; w < width; w++)
      {
        reinterpret_cast<pixel_t *>(pel4Plane)[w << 2] = reinterpret_cast<const pixel_t *>(pPlane4)[w];
        reinterpret_cast<pixel_t *>(pel4Plane)[(w << 2) + 1] = reinterpret_cast<const pixel_t *>(pPlane5)[w];
        reinterpret_cast<pixel_t *>(pel4Plane)[(w << 2) + 2] = reinterpret_cast<const pixel_t *>(pPlane6)[w];
        reinterpret_cast<pixel_t *>(pel4Plane)[(w << 2) + 3] = reinterpret_cast<const pixel_t *>(pPlane7)[w];
      }
      pel4Plane += pel4Pitch;
      for (int w = 0; w < width; w++)
      {
        reinterpret_cast<pixel_t *>(pel4Plane)[w << 2] = reinterpret_cast<const pixel_t *>(pPlane8)[w];
        reinterpret_cast<pixel_t *>(pel4Plane)[(w << 2) + 1] = reinterpret_cast<const pixel_t *>(pPlane9)[w];
        reinterpret_cast<pixel_t *>(pel4Plane)[(w << 2) + 2] = reinterpret_cast<const pixel_t *>(pPlane10)[w];
        reinterpret_cast<pixel_t *>(pel4Plane)[(w << 2) + 3] = reinterpret_cast<const pixel_t *>(pPlane11)[w];
      }
      pel4Plane += pel4Pitch;
      for (int w = 0; w < width; w++)
      {
        reinterpret_cast<pixel_t *>(pel4Plane)[w << 2] = reinterpret_cast<const pixel_t *>(pPlane12)[w];
        reinterpret_cast<pixel_t *>(pel4Plane)[(w << 2) + 1] = reinterpret_cast<const pixel_t *>(pPlane13)[w];
        reinterpret_cast<pixel_t *>(pel4Plane)[(w << 2) + 2] = reinterpret_cast<const pixel_t *>(pPlane14)[w];
        reinterpret_cast<pixel_t *>(pel4Plane)[(w << 2) + 3] = reinterpret_cast<const pixel_t *>(pPlane15)[w];
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
      pPlane13 += pitch;
      pPlane14 += pitch;
      pPlane15 += pitch;
    }
  }
}

void Merge16PlanesToBig(
  uint8_t *pel4Plane, int pel4Pitch,
  const uint8_t *pPlane0,  const uint8_t *pPlane1,  const uint8_t *pPlane2,  const uint8_t *pPlane3,
  const uint8_t *pPlane4,  const uint8_t *pPlane5,  const uint8_t *pPlane6,  const uint8_t *pPlane7,
  const uint8_t *pPlane8,  const uint8_t *pPlane9,  const uint8_t *pPlane10, const uint8_t *pPlane11,
  const uint8_t *pPlane12, const uint8_t *pPlane13, const uint8_t *pPlane14, const uint8_t *pPlane15,
  int width, int height, int pitch, int pixelsize, int cpuFlags)
{
  // no SSE2 here
  if (pixelsize == 1)
    Merge16PlanesToBig_c<uint8_t>(pel4Plane, pel4Pitch,
      pPlane0, pPlane1, pPlane2, pPlane3,
      pPlane4, pPlane5, pPlane6, pPlane7,
      pPlane8, pPlane9, pPlane10, pPlane11,
      pPlane12, pPlane13, pPlane14, pPlane15,
      width, height, pitch);
  else if(pixelsize==2)
    Merge16PlanesToBig_c<uint16_t>(pel4Plane, pel4Pitch,
      pPlane0, pPlane1, pPlane2, pPlane3,
      pPlane4, pPlane5, pPlane6, pPlane7,
      pPlane8, pPlane9, pPlane10, pPlane11,
      pPlane12, pPlane13, pPlane14, pPlane15,
      width, height, pitch);
  else if(pixelsize==4)
    Merge16PlanesToBig_c<float>(pel4Plane, pel4Pitch,
      pPlane0, pPlane1, pPlane2, pPlane3,
      pPlane4, pPlane5, pPlane6, pPlane7,
      pPlane8, pPlane9, pPlane10, pPlane11,
      pPlane12, pPlane13, pPlane14, pPlane15,
      width, height, pitch);
}

//-----------------------------------------------------------
unsigned char SADToMask(sad_t sad, sad_t sadnorm1024)
{
	// sadnorm1024 = 255 * (4*1024)/(mlSAD*nBlkSize*nBlkSize*chromablockfactor)
  // Check todo: bits_per_pixel? pixelsize?
	sad_t l = sadnorm1024*sad/1024;
	return (unsigned char)((l > 255) ? 255 : l); // internally mask clip is 8 bit always, even for 10+ bit clips
}


void Create_LUTV(int time256, short *LUTVB, short *LUTVF)
{
	for (int v=0; v<256; v++)
	{
		LUTVB[v] = (short)(((v-128)*(256-time256))/256);
		LUTVF[v] = (short)(((v-128)*time256)/256);
	}
}

/* was: in maskfun.hpp (template)*/

// todo: SSE2
// pitches: byte offsets
template<typename pixel_t>
void Blend(uint8_t * pdst8, const uint8_t * psrc8, const uint8_t * pref8, int height, int width, int dst_pitch, int src_pitch, int ref_pitch, int time256, int cpuFlags)
{
  pixel_t *pdst = reinterpret_cast<pixel_t *>(pdst8);
  const pixel_t *psrc = reinterpret_cast<const pixel_t *>(psrc8);
  const pixel_t *pref = reinterpret_cast<const pixel_t *>(pref8);
  dst_pitch /= sizeof(pixel_t);
  src_pitch /= sizeof(pixel_t);
  ref_pitch /= sizeof(pixel_t);
  // add isse
  int h, w;
  for (h = 0; h<height; h++)
  {
    for (w = 0; w<width; w++)
    {
      pdst[w] =(pixel_t)( (psrc[w] * (256 - time256) + pref[w] * time256) >> 8);
    }
    pdst += dst_pitch;
    psrc += src_pitch;
    pref += ref_pitch;
  }
}

// instantiate Blend
template void Blend<uint8_t>(uint8_t * pdst8, const uint8_t * psrc8, const uint8_t * pref8, int height, int width, int dst_pitch, int src_pitch, int ref_pitch, int time256, int cpuFlags);
template void Blend<uint16_t>(uint8_t * pdst8, const uint8_t * psrc8, const uint8_t * pref8, int height, int width, int dst_pitch, int src_pitch, int ref_pitch, int time256, int cpuFlags);

template<typename pixel_t>
void FlowInter(
  uint8_t * pdst, int dst_pitch, const uint8_t *prefB, const uint8_t *prefF, int ref_pitch,
  short *VXFullB, short *VXFullF, short *VYFullB, short *VYFullF, uint8_t *MaskB, uint8_t *MaskF,
  int VPitch, int width, int height, int time256, int nPel)
{
  if (nPel == 1)
  {
    FlowInter_NPel <pixel_t, 0>(
      pdst, dst_pitch, prefB, prefF, ref_pitch,
      VXFullB, VXFullF, VYFullB, VYFullF, MaskB, MaskF,
      VPitch, width, height, time256
      );
  }
  else if (nPel == 2)
  {
    FlowInter_NPel <pixel_t, 1>(
      pdst, dst_pitch, prefB, prefF, ref_pitch,
      VXFullB, VXFullF, VYFullB, VYFullF, MaskB, MaskF,
      VPitch, width, height, time256
      );
  }
  else if (nPel == 4)
  {
    FlowInter_NPel <pixel_t, 2>(
      pdst, dst_pitch, prefB, prefF, ref_pitch,
      VXFullB, VXFullF, VYFullB, VYFullF, MaskB, MaskF,
      VPitch, width, height, time256
      );
  }
}

template<typename pixel_t>
void FlowInterExtra(
  uint8_t * pdst, int dst_pitch, const uint8_t *prefB, const uint8_t *prefF, int ref_pitch,
  short *VXFullB, short *VXFullF, short *VYFullB, short *VYFullF, uint8_t *MaskB, uint8_t *MaskF,
  int VPitch, int width, int height, int time256, int nPel,
  short *VXFullBB, short *VXFullFF, short *VYFullBB, short *VYFullFF)
{
  if (nPel == 1)
  {
    FlowInterExtra_NPel <pixel_t, 0>(
      pdst, dst_pitch, prefB, prefF, ref_pitch,
      VXFullB, VXFullF, VYFullB, VYFullF, MaskB, MaskF,
      VPitch, width, height, time256,
      VXFullBB, VXFullFF, VYFullBB, VYFullFF
      );
  }
  else if (nPel == 2)
  {
    FlowInterExtra_NPel <pixel_t, 1>(
      pdst, dst_pitch, prefB, prefF, ref_pitch,
      VXFullB, VXFullF, VYFullB, VYFullF, MaskB, MaskF,
      VPitch, width, height, time256,
      VXFullBB, VXFullFF, VYFullBB, VYFullFF
      );
  }
  else if (nPel == 4)
  {
    FlowInterExtra_NPel <pixel_t, 2>(
      pdst, dst_pitch, prefB, prefF, ref_pitch,
      VXFullB, VXFullF, VYFullB, VYFullF, MaskB, MaskF,
      VPitch, width, height, time256,
      VXFullBB, VXFullFF, VYFullBB, VYFullFF
      );
  }
}

template<typename pixel_t>
void FlowInterSimple(
  uint8_t * pdst, int dst_pitch, const uint8_t *prefB, const uint8_t *prefF, int ref_pitch,
  short *VXFullB, short *VXFullF, short *VYFullB, short *VYFullF, uint8_t *MaskB, uint8_t *MaskF,
  int VPitch, int width, int height, int time256, int nPel)
{
  if (nPel == 1)
  {
    FlowInterSimple_Pel1<pixel_t>(
      pdst, dst_pitch, prefB, prefF, ref_pitch,
      VXFullB, VXFullF, VYFullB, VYFullF, MaskB, MaskF,
      VPitch, width, height, time256
      );
  }
  else if (nPel == 2)
  {
    FlowInterSimple_NPel <pixel_t, 1>(
      pdst, dst_pitch, prefB, prefF, ref_pitch,
      VXFullB, VXFullF, VYFullB, VYFullF, MaskB, MaskF,
      VPitch, width, height, time256
      );
  }
  else if (nPel == 4)
  {
    FlowInterSimple_NPel <pixel_t, 2>(
      pdst, dst_pitch, prefB, prefF, ref_pitch,
      VXFullB, VXFullF, VYFullB, VYFullF, MaskB, MaskF,
      VPitch, width, height, time256
      );
  }
}

// instantiate
template void FlowInterSimple<uint8_t>(uint8_t * pdst, int dst_pitch, const uint8_t *prefB, const uint8_t *prefF, int ref_pitch,
  short *VXFullB, short *VXFullF, short *VYFullB, short *VYFullF, uint8_t *MaskB, uint8_t *MaskF,
  int VPitch, int width, int height, int time256, int nPel );
template void FlowInterSimple<uint16_t>(uint8_t * pdst, int dst_pitch, const uint8_t *prefB, const uint8_t *prefF, int ref_pitch,
  short *VXFullB, short *VXFullF, short *VYFullB, short *VYFullF, uint8_t *MaskB, uint8_t *MaskF,
  int VPitch, int width, int height, int time256, int nPel );

template void FlowInter<uint8_t>(uint8_t * pdst, int dst_pitch, const uint8_t *prefB, const uint8_t *prefF, int ref_pitch,
  short *VXFullB, short *VXFullF, short *VYFullB, short *VYFullF, uint8_t *MaskB, uint8_t *MaskF,
  int VPitch, int width, int height, int time256, int nPel );
template void FlowInter<uint16_t>(uint8_t * pdst, int dst_pitch, const uint8_t *prefB, const uint8_t *prefF, int ref_pitch,
  short *VXFullB, short *VXFullF, short *VYFullB, short *VYFullF, uint8_t *MaskB, uint8_t *MaskF,
  int VPitch, int width, int height, int time256, int nPel );

template void FlowInterExtra<uint8_t>(uint8_t * pdst, int dst_pitch, const uint8_t *prefB, const uint8_t *prefF, int ref_pitch,
  short *VXFullB, short *VXFullF, short *VYFullB, short *VYFullF, uint8_t *MaskB, uint8_t *MaskF,
  int VPitch, int width, int height, int time256, int nPel,
  short *VXFullBB, short *VXFullFF, short *VYFullBB, short *VYFullFF);
template void FlowInterExtra<uint16_t>(uint8_t * pdst, int dst_pitch, const uint8_t *prefB, const uint8_t *prefF, int ref_pitch,
  short *VXFullB, short *VXFullF, short *VYFullB, short *VYFullF, uint8_t *MaskB, uint8_t *MaskF,
  int VPitch, int width, int height, int time256, int nPel,
  short *VXFullBB, short *VXFullFF, short *VYFullBB, short *VYFullFF);


