// Overlap copy (really addition)
// Copyright(c)2006 A.G.Balakhnin aka Fizick

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

#include "overlap.h"
#include "def.h"

#include <cmath>
#include <tuple>
#include <map>

#ifndef min
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif

#ifndef max
#define max(a,b)            (((a) < (b)) ? (b) : (a))
#endif


//--------------------------------------------------------------------
// Overlap Windows class
OverlapWindows::OverlapWindows(int _nx, int _ny, int _ox, int _oy)
{
	nx = _nx;
	ny = _ny;
	ox = _ox;
	oy = _oy;
	size = nx*ny;

	   //  windows
	   fWin1UVx = new float[nx];
	   fWin1UVxfirst = new float[nx];
	   fWin1UVxlast = new float[nx];
	   for (int i=0; i<ox; i++)
	   {
			fWin1UVx[i] = float (cos(PI*(i-ox+0.5f)/(ox*2)));
			fWin1UVx[i] = fWin1UVx[i]*fWin1UVx[i];// left window (rised cosine)
			fWin1UVxfirst[i] = 1; // very first window
			fWin1UVxlast[i] = fWin1UVx[i]; // very last
	   }
	   for (int i=ox; i<nx-ox; i++)
	   {
			fWin1UVx[i] = 1;
			fWin1UVxfirst[i] = 1; // very first window
			fWin1UVxlast[i] = 1; // very last
	   }
	   for (int i=nx-ox; i<nx; i++)
	   {
			fWin1UVx[i] = float (cos(PI*(i-nx+ox+0.5f)/(ox*2)));
			fWin1UVx[i] = fWin1UVx[i]*fWin1UVx[i];// right window (falled cosine)
			fWin1UVxfirst[i] = fWin1UVx[i]; // very first window
			fWin1UVxlast[i] = 1; // very last
	   }

	   fWin1UVy = new float[ny];
	   fWin1UVyfirst = new float[ny];
	   fWin1UVylast = new float[ny];
	   for (int i=0; i<oy; i++)
	   {
			fWin1UVy[i] = float (cos(PI*(i-oy+0.5f)/(oy*2)));
			fWin1UVy[i] = fWin1UVy[i]*fWin1UVy[i];// left window (rised cosine)
			fWin1UVyfirst[i] = 1; // very first window
			fWin1UVylast[i] = fWin1UVy[i]; // very last
	   }
	   for (int i=oy; i<ny-oy; i++)
	   {
			fWin1UVy[i] = 1;
			fWin1UVyfirst[i] = 1; // very first window
			fWin1UVylast[i] = 1; // very last
	   }
	   for (int i=ny-oy; i<ny; i++)
	   {
			fWin1UVy[i] = float (cos(PI*(i-ny+oy+0.5f)/(oy*2)));
			fWin1UVy[i] = fWin1UVy[i]*fWin1UVy[i];// right window (falled cosine)
			fWin1UVyfirst[i] = fWin1UVy[i]; // very first window
			fWin1UVylast[i] = 1; // very last
	   }


		Overlap9Windows = new short[size*9];

	   short *winOverUVTL = Overlap9Windows;
	   short *winOverUVTM = Overlap9Windows + size;
	   short *winOverUVTR = Overlap9Windows + size*2;
	   short *winOverUVML = Overlap9Windows + size*3;
	   short *winOverUVMM = Overlap9Windows + size*4;
	   short *winOverUVMR = Overlap9Windows + size*5;
	   short *winOverUVBL = Overlap9Windows + size*6;
	   short *winOverUVBM = Overlap9Windows + size*7;
	   short *winOverUVBR = Overlap9Windows + size*8;

	   for (int j=0; j<ny; j++)
	   {
		   for (int i=0; i<nx; i++)
		   {
			   winOverUVTL[i] = (int)(fWin1UVyfirst[j]*fWin1UVxfirst[i]*2048 + 0.5f);
			   winOverUVTM[i] = (int)(fWin1UVyfirst[j]*fWin1UVx[i]*2048 + 0.5f);
			   winOverUVTR[i] = (int)(fWin1UVyfirst[j]*fWin1UVxlast[i]*2048 + 0.5f);
			   winOverUVML[i] = (int)(fWin1UVy[j]*fWin1UVxfirst[i]*2048 + 0.5f);
			   winOverUVMM[i] = (int)(fWin1UVy[j]*fWin1UVx[i]*2048 + 0.5f);
			   winOverUVMR[i] = (int)(fWin1UVy[j]*fWin1UVxlast[i]*2048 + 0.5f);
			   winOverUVBL[i] = (int)(fWin1UVylast[j]*fWin1UVxfirst[i]*2048 + 0.5f);
			   winOverUVBM[i] = (int)(fWin1UVylast[j]*fWin1UVx[i]*2048 + 0.5f);
			   winOverUVBR[i] = (int)(fWin1UVylast[j]*fWin1UVxlast[i]*2048 + 0.5f);
		   }
		   winOverUVTL += nx;
		   winOverUVTM += nx;
		   winOverUVTR += nx;
		   winOverUVML += nx;
		   winOverUVMM += nx;
		   winOverUVMR += nx;
		   winOverUVBL += nx;
		   winOverUVBM += nx;
		   winOverUVBR += nx;
	   }
}

OverlapWindows::~OverlapWindows()
{
	delete [] Overlap9Windows;
	delete [] fWin1UVx;
	delete [] fWin1UVxfirst;
	delete [] fWin1UVxlast;
	delete [] fWin1UVy;
	delete [] fWin1UVyfirst;
	delete [] fWin1UVylast;
}

void Short2Bytes(unsigned char *pDst, int nDstPitch, unsigned short *pDstShort, int dstShortPitch, int nWidth, int nHeight)
{
	for (int h=0; h<nHeight; h++)
	{
		for (int i=0; i<nWidth; i++)
		{
			int a = (pDstShort[i])>>5;
			pDst[i] = a | (255-a) >> (sizeof(int)*8-1);
//			pDst[i] = min(255, (pDstShort[i])>>5);
		}
		pDst += nDstPitch;
		pDstShort += dstShortPitch;
	}
}

void Short2BytesLsb(unsigned char *pDst, unsigned char *pDstLsb, int nDstPitch, int *pDstInt, int dstIntPitch, int nWidth, int nHeight)
{
	for (int h=0; h<nHeight; h++)
	{
		for (int i=0; i<nWidth; i++)
		{
			const int		a = pDstInt [i] >> (5+6);
			pDst [i] = a >> 8;
			pDstLsb [i] = (unsigned char) (a);
		}
		pDst += nDstPitch;
		pDstLsb += nDstPitch;
		pDstInt += dstIntPitch;
	}
}

template<typename pixel_t>
void LimitChanges_c(unsigned char *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, int nWidth, int nHeight, int nLimit)
{
	for (int h=0; h<nHeight; h++)
	{
		for (int i=0; i<nWidth; i++)
			reinterpret_cast<pixel_t *>(pDst)[i] = min( max (reinterpret_cast<pixel_t *>(pDst)[i], (reinterpret_cast<const pixel_t *>(pSrc)[i]-nLimit)), (reinterpret_cast<const pixel_t *>(pSrc)[i]+nLimit));
		pDst += nDstPitch;
		pSrc += nSrcPitch;
	}
}

// if a non-static template function is in cpp, we have to instantiate it
template void LimitChanges_c<uint8_t>(unsigned char *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, int nWidth, int nHeight, int nLimit);
template void LimitChanges_c<uint16_t>(unsigned char *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, int nWidth, int nHeight, int nLimit);


OverlapsFunction *get_overlaps_function(int BlockX, int BlockY, int pixelsize, arch_t arch)
{
    //---------- OVERLAPS
    // 8 bit only (pixelsize==1)
    // BlkSizeX, BlkSizeY, pixelsize, arch_t
    std::map<std::tuple<int, int, int, arch_t>, OverlapsFunction*> func_overlaps;
    using std::make_tuple;
    func_overlaps[make_tuple(32, 32, 1, NO_SIMD)] = Overlaps_C<32, 32>;
    func_overlaps[make_tuple(32, 16, 1, NO_SIMD)] = Overlaps_C<32, 16>;
    func_overlaps[make_tuple(32, 8 , 1, NO_SIMD)] = Overlaps_C<32, 8>;
    func_overlaps[make_tuple(16, 32, 1, NO_SIMD)] = Overlaps_C<16, 32>;
    func_overlaps[make_tuple(16, 16, 1, NO_SIMD)] = Overlaps_C<16, 16>;
    func_overlaps[make_tuple(16, 8 , 1, NO_SIMD)] = Overlaps_C<16, 8>;
    func_overlaps[make_tuple(16, 4 , 1, NO_SIMD)] = Overlaps_C<16, 4>;
    func_overlaps[make_tuple(16, 2 , 1, NO_SIMD)] = Overlaps_C<16, 2>;
    func_overlaps[make_tuple(8 , 16, 1, NO_SIMD)] = Overlaps_C<8 , 16>;
    func_overlaps[make_tuple(8 , 8 , 1, NO_SIMD)] = Overlaps_C<8 , 8>;
    func_overlaps[make_tuple(8 , 4 , 1, NO_SIMD)] = Overlaps_C<8 , 4>;
    func_overlaps[make_tuple(8 , 2 , 1, NO_SIMD)] = Overlaps_C<8 , 2>;
    func_overlaps[make_tuple(8 , 1 , 1, NO_SIMD)] = Overlaps_C<8 , 1>;
    func_overlaps[make_tuple(4 , 8 , 1, NO_SIMD)] = Overlaps_C<4 , 8>;
    func_overlaps[make_tuple(4 , 4 , 1, NO_SIMD)] = Overlaps_C<4 , 4>;
    func_overlaps[make_tuple(4 , 2 , 1, NO_SIMD)] = Overlaps_C<4 , 2>;
    func_overlaps[make_tuple(2 , 4 , 1, NO_SIMD)] = Overlaps_C<2 , 4>;
    func_overlaps[make_tuple(2 , 2 , 1, NO_SIMD)] = Overlaps_C<2 , 2>;

    func_overlaps[make_tuple(32, 32, 1, USE_SSE2)] = Overlaps32x32_sse2;
    func_overlaps[make_tuple(32, 16, 1, USE_SSE2)] = Overlaps32x16_sse2;
    func_overlaps[make_tuple(32, 8 , 1, USE_SSE2)] = Overlaps32x8_sse2;
    func_overlaps[make_tuple(16, 32, 1, USE_SSE2)] = Overlaps16x32_sse2;
    func_overlaps[make_tuple(16, 16, 1, USE_SSE2)] = Overlaps16x16_sse2;
    func_overlaps[make_tuple(16, 8 , 1, USE_SSE2)] = Overlaps16x8_sse2;
    func_overlaps[make_tuple(16, 4 , 1, USE_SSE2)] = Overlaps16x4_sse2;
    func_overlaps[make_tuple(16, 2 , 1, USE_SSE2)] = Overlaps16x2_sse2;
    func_overlaps[make_tuple(8 , 16, 1, USE_SSE2)] = Overlaps8x16_sse2;
    func_overlaps[make_tuple(8 , 8 , 1, USE_SSE2)] = Overlaps8x8_sse2;
    func_overlaps[make_tuple(8 , 4 , 1, USE_SSE2)] = Overlaps8x4_sse2;
    func_overlaps[make_tuple(8 , 2 , 1, USE_SSE2)] = Overlaps8x2_sse2;
    func_overlaps[make_tuple(8 , 1 , 1, USE_SSE2)] = Overlaps8x1_sse2;
    func_overlaps[make_tuple(4 , 8 , 1, USE_SSE2)] = Overlaps4x8_sse2;
    func_overlaps[make_tuple(4 , 4 , 1, USE_SSE2)] = Overlaps4x4_sse2;
    func_overlaps[make_tuple(4 , 2 , 1, USE_SSE2)] = Overlaps4x2_sse2;
    func_overlaps[make_tuple(2 , 4 , 1, USE_SSE2)] = Overlaps2x4_sse2;
    func_overlaps[make_tuple(2 , 2 , 1, USE_SSE2)] = Overlaps2x2_sse2;
    // Why did the original code used sse2 named functions for overlaps 
    // when no CPUF_SSE2 was detected bit isse param is true?
    func_overlaps[make_tuple(32, 32, 1, USE_MMX)] = Overlaps32x32_sse2;
    func_overlaps[make_tuple(32, 16, 1, USE_MMX)] = Overlaps32x16_sse2;
    func_overlaps[make_tuple(32, 8 , 1, USE_MMX)] = Overlaps32x8_sse2;
    func_overlaps[make_tuple(16, 32, 1, USE_MMX)] = Overlaps16x32_sse2;
    func_overlaps[make_tuple(16, 16, 1, USE_MMX)] = Overlaps16x16_sse2;
    func_overlaps[make_tuple(16, 8 , 1, USE_MMX)] = Overlaps16x8_sse2;
    func_overlaps[make_tuple(16, 4 , 1, USE_MMX)] = Overlaps16x4_sse2;
    func_overlaps[make_tuple(16, 2 , 1, USE_MMX)] = Overlaps16x2_sse2;
    func_overlaps[make_tuple(8 , 16, 1, USE_MMX)] = Overlaps8x16_sse2;
    func_overlaps[make_tuple(8 , 8 , 1, USE_MMX)] = Overlaps8x8_sse2;
    func_overlaps[make_tuple(8 , 4 , 1, USE_MMX)] = Overlaps8x4_sse2;
    func_overlaps[make_tuple(8 , 2 , 1, USE_MMX)] = Overlaps8x2_sse2;
    func_overlaps[make_tuple(8 , 1 , 1, USE_MMX)] = Overlaps8x1_sse2;
    func_overlaps[make_tuple(4 , 8 , 1, USE_MMX)] = Overlaps4x8_sse2;
    func_overlaps[make_tuple(4 , 4 , 1, USE_MMX)] = Overlaps4x4_sse2;
    func_overlaps[make_tuple(4 , 2 , 1, USE_MMX)] = Overlaps4x2_sse2;
    func_overlaps[make_tuple(2 , 4 , 1, USE_MMX)] = Overlaps2x4_sse2;
    func_overlaps[make_tuple(2 , 2 , 1, USE_MMX)] = Overlaps2x2_sse2;

    OverlapsFunction *result = func_overlaps[std::make_tuple(BlockX, BlockY, pixelsize, arch)];
    if (result == nullptr)
        result = func_overlaps[std::make_tuple(BlockX, BlockY, pixelsize, NO_SIMD)]; // fallback to C
    return result;
}

OverlapsLsbFunction *get_overlaps_lsb_function(int BlockX, int BlockY, int pixelsize, arch_t arch)
{
    // 8 bit only (pixelsize==1)
    // BlkSizeX, BlkSizeY, pixelsize, arch_t
    std::map<std::tuple<int, int, int, arch_t>, OverlapsLsbFunction*> func_overlaps_lsb;
    using std::make_tuple;
    func_overlaps_lsb[make_tuple(32, 32, 1, NO_SIMD)] = OverlapsLsb_C<32, 32>;
    func_overlaps_lsb[make_tuple(32, 16, 1, NO_SIMD)] = OverlapsLsb_C<32, 16>;
    func_overlaps_lsb[make_tuple(32, 8 , 1, NO_SIMD)] = OverlapsLsb_C<32, 8>;
    func_overlaps_lsb[make_tuple(16, 32, 1, NO_SIMD)] = OverlapsLsb_C<16, 32>;
    func_overlaps_lsb[make_tuple(16, 16, 1, NO_SIMD)] = OverlapsLsb_C<16, 16>;
    func_overlaps_lsb[make_tuple(16, 8 , 1, NO_SIMD)] = OverlapsLsb_C<16, 8>;
    func_overlaps_lsb[make_tuple(16, 4 , 1, NO_SIMD)] = OverlapsLsb_C<16, 4>;
    func_overlaps_lsb[make_tuple(16, 2 , 1, NO_SIMD)] = OverlapsLsb_C<16, 2>;
    func_overlaps_lsb[make_tuple(8 , 16, 1, NO_SIMD)] = OverlapsLsb_C<8 , 16>;
    func_overlaps_lsb[make_tuple(8 , 8 , 1, NO_SIMD)] = OverlapsLsb_C<8 , 8>;
    func_overlaps_lsb[make_tuple(8 , 4 , 1, NO_SIMD)] = OverlapsLsb_C<8 , 4>;
    func_overlaps_lsb[make_tuple(8 , 2 , 1, NO_SIMD)] = OverlapsLsb_C<8 , 2>;
    func_overlaps_lsb[make_tuple(8 , 1 , 1, NO_SIMD)] = OverlapsLsb_C<8 , 1>;
    func_overlaps_lsb[make_tuple(4 , 8 , 1, NO_SIMD)] = OverlapsLsb_C<4 , 8>;
    func_overlaps_lsb[make_tuple(4 , 4 , 1, NO_SIMD)] = OverlapsLsb_C<4 , 4>;
    func_overlaps_lsb[make_tuple(4 , 2 , 1, NO_SIMD)] = OverlapsLsb_C<4 , 2>;
    func_overlaps_lsb[make_tuple(2 , 4 , 1, NO_SIMD)] = OverlapsLsb_C<2 , 4>;
    func_overlaps_lsb[make_tuple(2 , 2 , 1, NO_SIMD)] = OverlapsLsb_C<2 , 2>;

    OverlapsLsbFunction *result = func_overlaps_lsb[std::make_tuple(BlockX, BlockY, pixelsize, arch)];
    if (result == nullptr)
        result = func_overlaps_lsb[std::make_tuple(BlockX, BlockY, pixelsize, NO_SIMD)]; // fallback to C
    return result;
}



