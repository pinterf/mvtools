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

#ifndef __OVERLAP__
#define __OVERLAP__

// top, middle, botom and left, middle, right windows
#define OW_TL 0
#define OW_TM 1
#define OW_TR 2
#define OW_ML 3
#define OW_MM 4
#define OW_MR 5
#define OW_BL 6
#define OW_BM 7
#define OW_BR 8

class OverlapWindows
{
	int nx; // window sizes
	int ny;
	int ox; // overap sizes
	int oy;
	int size; // full window size= nx*ny

	short * Overlap9Windows;

	float *fWin1UVx;
	float *fWin1UVxfirst;
	float *fWin1UVxlast;
	float *fWin1UVy;
	float *fWin1UVyfirst;
	float *fWin1UVylast;
public :

	OverlapWindows(int _nx, int _ny, int _ox, int _oy);
   ~OverlapWindows();

   inline int Getnx() const { return nx; }
   inline int Getny() const { return ny; }
   inline int GetSize() const { return size; }
   inline short *GetWindow(int i) const { return Overlap9Windows + size*i; }
};

typedef void (OverlapsFunction)(unsigned short *pDst, int nDstPitch,
                            const unsigned char *pSrc, int nSrcPitch,
							short *pWin, int nWinPitch);
typedef void (OverlapsLsbFunction)(int *pDst, int nDstPitch, const unsigned char *pSrc, const unsigned char *pSrcLsb, int nSrcPitch, short *pWin, int nWinPitch);

//=============================================================
// short
template <int blockWidth, int blockHeight>
void Overlaps_C(unsigned short *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch)
{
	// pWin from 0 to 2048
	for (int j=0; j<blockHeight; j++)
	{
	    for (int i=0; i<blockWidth; i++)
	    {
            pDst[i] = ( pDst[i] + ((pSrc[i]*pWin[i]+256)>>6));
	    }
		pDst += nDstPitch;
		pSrc += nSrcPitch;
		pWin += nWinPitch;
	}
}

template <int blockWidth, int blockHeight>
void OverlapsLsb_C(int *pDst, int nDstPitch, const unsigned char *pSrc, const unsigned char *pSrcLsb, int nSrcPitch, short *pWin, int nWinPitch)
{
	// pWin from 0 to 2048
	for (int j=0; j<blockHeight; j++)
	{
		for (int i=0; i<blockWidth; i++)
		{
			const int		val = (pSrc [i] << 8) + pSrcLsb [i];
			pDst [i] += val * pWin [i];
		}
		pDst += nDstPitch;
		pSrc += nSrcPitch;
		pSrcLsb += nSrcPitch;
		pWin += nWinPitch;
	}
}



extern "C" void __cdecl  Overlaps32x32_sse2(unsigned short *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch);
extern "C" void __cdecl  Overlaps16x32_sse2(unsigned short *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch);
extern "C" void __cdecl  Overlaps32x16_sse2(unsigned short *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch);
extern "C" void __cdecl  Overlaps16x16_sse2(unsigned short *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch);
extern "C" void __cdecl  Overlaps8x16_sse2(unsigned short *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch);
extern "C" void __cdecl  Overlaps8x8_sse2(unsigned short *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch);
extern "C" void __cdecl  Overlaps4x8_sse2(unsigned short *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch);
extern "C" void __cdecl  Overlaps4x4_sse2(unsigned short *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch);
extern "C" void __cdecl  Overlaps2x4_sse2(unsigned short *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch);
extern "C" void __cdecl  Overlaps2x2_sse2(unsigned short *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch);
extern "C" void __cdecl  Overlaps8x4_sse2(unsigned short *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch);
extern "C" void __cdecl  Overlaps4x2_sse2(unsigned short *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch);
extern "C" void __cdecl  Overlaps16x8_sse2(unsigned short *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch);
extern "C" void __cdecl  Overlaps16x2_sse2(unsigned short *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch);
extern "C" void __cdecl  Overlaps8x2_sse2(unsigned short *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch);
extern "C" void __cdecl  Overlaps8x1_sse2(unsigned short *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch);

void Short2Bytes(unsigned char *pDst, int nDstPitch, unsigned short *pDstShort, int dstShortPitch, int nWidth, int nHeight);
void Short2BytesLsb(unsigned char *pDst, unsigned char *pDstLsb, int nDstPitch, int *pDstInt, int dstIntPitch, int nWidth, int nHeight);

template<typename pixel_t>
void LimitChanges_c(unsigned char *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, int nWidth, int nHeight, int nLimit);
extern "C" void  __cdecl  LimitChanges_sse2(unsigned char *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, int nWidth, int nHeight, int nLimit);

// Not really related to overlap, but common to MDegrainX functions
inline int DegrainWeight(int thSAD, int blockSAD)
{
	// Returning directly prevents a divide by 0 if thSAD == blockSAD == 0.
	if (thSAD <= blockSAD)
	{
		return 0;
	}

	const int      thSAD2    = thSAD    * thSAD;
	const int      blockSAD2 = blockSAD * blockSAD;
	const int      num = thSAD2 - blockSAD2;
	const int		den = thSAD2 + blockSAD2;
	const int      res =   (num < (1<<23))
	                     ? (num<<8) /  den      // small numerator
	                     :  num     / (den>>8); // very large numerator, prevent overflow
	return (res);
}

#endif
