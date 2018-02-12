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
#include "avs\minmax.h"
#include <cassert>
#include <emmintrin.h>
#include <smmintrin.h>

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

void Short2Bytes_sse2(unsigned char *pDst, int nDstPitch, unsigned short *pDstShort, int dstShortPitch, int nWidth, int nHeight)
{
  /*
  for (int h=0; h<nHeight; h++)
  {
    for (int i=0; i<nWidth; i++)
    {
      int a = (pDstShort[i])>>5;
      pDst[i] = min(255, a); // PF everyone can understand it
    }
    pDst += nDstPitch;
    pDstShort += dstShortPitch;
  }
  */

  const int nSrcPitch = dstShortPitch * sizeof(short); // back to byte size
  BYTE *pSrc8 = reinterpret_cast<BYTE *>(pDstShort);
  BYTE *pDst8 = reinterpret_cast<BYTE *>(pDst);
  int wMod16 = (nWidth / 16) * 16;
  int wMod8 = (nWidth / 8) * 8;
  for (int y = 0; y < nHeight; y++)
  {
    for (int x = 0; x < wMod16; x += 16) { // 32 source bytes = 8 short sized pixels, 16 bytes of 8*uint16_t destination
                                            // 2*4 int -> 8 uint16_t
      __m128i src07 = _mm_loadu_si128((__m128i *)(pSrc8 + x*2)); // 8 short pixels
      __m128i src8f = _mm_loadu_si128((__m128i *)(pSrc8 + x*2 + 16)); // 8 short pixels
      __m128i res07 = _mm_srai_epi16(src07, 5); // shift and limit
      __m128i res8f = _mm_srai_epi16(src8f, 5); // shift and limit
      __m128i res = _mm_packus_epi16(res07, res8f);
      _mm_store_si128((__m128i *)(pDst8 + x), res);
    }
    if (wMod8 != wMod16) {
      __m128i src07 = _mm_loadu_si128((__m128i *)(pSrc8 + wMod16 * 2)); // 8 short pixels
      __m128i res07 = _mm_srai_epi16(src07, 5); // shift and limit
      __m128i res = _mm_packus_epi16(res07, res07);
      _mm_storel_epi64((__m128i *)(pDst8 + wMod16), res);
    }
    for (int x = wMod8; x < nWidth; x++) {
      int a = (reinterpret_cast<unsigned short *>(pSrc8)[x]) >> 5;
      pDst8[x] = min(255, a);
    }
    pDst8 += nDstPitch;
    pSrc8 += nSrcPitch;
  }
}

void Short2Bytes(unsigned char *pDst, int nDstPitch, unsigned short *pDstShort, int dstShortPitch, int nWidth, int nHeight)
{
	for (int h=0; h<nHeight; h++)
	{
		for (int i=0; i<nWidth; i++)
		{
			int a = (pDstShort[i])>>5;
      pDst[i] = min(255, a); // PF everyone can understand it
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

// better name: Int to uint16
void Short2Bytes_Int32toWord16(uint16_t *pDst, int nDstPitch, int *pDstInt, int dstIntPitch, int nWidth, int nHeight, int bits_per_pixel)
{
  const int max_pixel_value = (1 << bits_per_pixel) - 1;
  for (int h=0; h<nHeight; h++)
  {
    for (int i=0; i<nWidth; i++)
    {
      //const int		a = (pDstInt [i] + (1 << 10)) >> (5+6); //scale back do we need 1<<10 round?)
      const int		a = pDstInt [i] >> (5+6); //scale back
      pDst [i] = min(a, max_pixel_value); // no need 8*shift
    }
    pDst += nDstPitch/sizeof(uint16_t);
    pDstInt += dstIntPitch; // this pitch is int granularity
  }
}

void Short2Bytes_Int32toWord16_sse4(uint16_t *pDst, int nDstPitch, int *pDstInt, int dstIntPitch, int nWidth, int nHeight, int bits_per_pixel)
{
  typedef uint16_t pixel_t;
  const int max_pixel_value = (1 << bits_per_pixel) - 1;
  /*
  for (int h=0; h<nHeight; h++)
  {
    for (int i=0; i<nWidth; i++)
    {
      const int		a = pDstInt [i] >> (5+6); //scale back
      pDst [i] = min(a, max_pixel_value); // no need 8*shift
    }
    pDst += nDstPitch/sizeof(uint16_t);
    pDstInt += dstIntPitch; // this pitch is int granularity
  }
  */
  __m128i limits, limits16;
  limits = _mm_set1_epi32(max_pixel_value);
  limits16 = _mm_set1_epi16(max_pixel_value);

  // pDstInt is 16 byte granularity -> pitch is 64 bytes aligned
  const int nSrcPitch = dstIntPitch * sizeof(int); // back to byte size
  //nDstPitch /= sizeof(uint16_t);
  const int width_b = nWidth * sizeof(uint16_t); // destination byte size
  BYTE *pSrc8 = reinterpret_cast<BYTE *>(pDstInt);
  BYTE *pDst8 = reinterpret_cast<BYTE *>(pDst);

  for (int y = 0; y < nHeight; y++)
  {
    for (int x = 0; x < width_b; x += 16) { // 32 source bytes = 8 integer sized pixels, 16 bytes of 8*uint16_t destination
      // 2*4 int -> 8 uint16_t
      __m128i src03 = _mm_loadu_si128((__m128i *)(pSrc8 + x*2)); // 4 int pixels
      __m128i src47 = _mm_loadu_si128((__m128i *)(pSrc8 + x*2 + 16)); // 4 int pixels
      __m128i res03 = _mm_srai_epi32(src03, (5+6)); // shift and limit
      __m128i res47 = _mm_srai_epi32(src47, (5+6)); // shift and limit
      //__m128i res03 = _mm_min_epi32(_mm_srai_epi32(src03, (5+6)), limits); // shift and limit
      //__m128i res47 = _mm_min_epi32(_mm_srai_epi32(src47, (5+6)), limits); // shift and limit
      __m128i res = _mm_packus_epi32(res03, res47); // sse4 int->uint16_t, already limiting to 65535
      res = _mm_min_epu16(res, limits16); // 10,12,14 bits can be lesser
      _mm_store_si128((__m128i *)(pDst8 + x), res);
    }
    pDst8 += nDstPitch;
    pSrc8 += nSrcPitch;
  }
}

void Short2Bytes_FloatInInt32ArrayToFloat(float *pDst, int nDstPitch, int *pDstInt, int dstIntPitch, int nWidth, int nHeight)
{
  float *pDstIntF = reinterpret_cast<float *>(pDstInt);
  for (int h=0; h<nHeight; h++)
  {
    for (int i=0; i<nWidth; i++)
    {
      // const int		a = pDstInt [i] >> (5+6); float: no scale
      pDst [i] = min(pDstIntF[i], 1.0f);
    }
    pDst += nDstPitch/sizeof(float);
    pDstInt += dstIntPitch; // this pitch is int/float (4 byte) granularity
  }
}

__forceinline __m128i _MM_CMPLE_EPU16(__m128i x, __m128i y)
{
  // Returns 0xFFFF where x <= y:
  return _mm_cmpeq_epi16(_mm_subs_epu16(x, y), _mm_setzero_si128());
}

__forceinline __m128i _MM_BLENDV_SI128(__m128i x, __m128i y, __m128i mask)
{
  // Replace bit in x with bit in y when matching bit in mask is set:
  return _mm_or_si128(_mm_andnot_si128(mask, x), _mm_and_si128(mask, y));
}

// sse2 simulation of SSE4's _mm_min_epu16
__forceinline __m128i _MM_MIN_EPU16(__m128i x, __m128i y)
{
  // Returns x where x <= y, else y:
  return _MM_BLENDV_SI128(y, x, _MM_CMPLE_EPU16(x, y));
}

// sse2 simulation of SSE4's _mm_max_epu16
__forceinline __m128i _MM_MAX_EPU16(__m128i x, __m128i y)
{
  // Returns x where x >= y, else y:
  return _MM_BLENDV_SI128(x, y, _MM_CMPLE_EPU16(x, y));
}

// MVDegrains-a.asm ported to intrinsics + 16 bit PF 161002
template<typename pixel_t, bool hasSSE41>
void LimitChanges_sse2_new(unsigned char *pDst8, int nDstPitch, const unsigned char *pSrc8, int nSrcPitch, const int nWidth, int nHeight, int nLimit)
{
  __m128i limits;
  if (sizeof(pixel_t) == 1)
    limits = _mm_set1_epi8(nLimit);
  else
    limits = _mm_set1_epi16(nLimit);

  const int stride = nWidth * sizeof(pixel_t); // back to byte size

  __m128i zero = _mm_setzero_si128();
  for (int y = 0; y < nHeight; y++)
  {
    for (int x = 0; x < stride; x += 16) {
      __m128i src = _mm_load_si128((__m128i *)(pSrc8 + x));
      __m128i dst = _mm_load_si128((__m128i *)(pDst8 + x));
      __m128i res;
      if (sizeof(pixel_t) == 1) {
        __m128i src_plus_limit = _mm_adds_epu8(src, limits);   //  max possible
        __m128i src_minus_limit = _mm_subs_epu8(src, limits);   //  min possible
        res = _mm_min_epu8(_mm_max_epu8(src_minus_limit, dst), src_plus_limit);
      }
      else {

        __m128i src_plus_limit = _mm_adds_epu16(src, limits);   //  max possible
        __m128i src_minus_limit = _mm_subs_epu16(src, limits);   //  min possible
        if(hasSSE41)
          res = _mm_min_epu16(_mm_max_epu16(src_minus_limit, dst), src_plus_limit); // SSE4
        else
          res = _MM_MIN_EPU16(_MM_MAX_EPU16(src_minus_limit, dst), src_plus_limit); // SSE4
      }
      _mm_stream_si128((__m128i *)(pDst8 + x), res);
    }
    //  reinterpret_cast<pixel_t *>(pDst)[i] = 
    //  (pixel_t)clamp((int)reinterpret_cast<pixel_t *>(pDst)[i], (reinterpret_cast<const pixel_t *>(pSrc)[i]-nLimit), (reinterpret_cast<const pixel_t *>(pSrc)[i]+nLimit));
    pDst8 += nDstPitch;
    pSrc8 += nSrcPitch;
  }
}


template<typename pixel_t>
void LimitChanges_c(unsigned char *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, int nWidth, int nHeight, int nLimit)
{
	for (int h=0; h<nHeight; h++)
	{
		for (int i=0; i<nWidth; i++)
			reinterpret_cast<pixel_t *>(pDst)[i] = 
        (pixel_t)clamp((int)reinterpret_cast<pixel_t *>(pDst)[i], (reinterpret_cast<const pixel_t *>(pSrc)[i]-nLimit), (reinterpret_cast<const pixel_t *>(pSrc)[i]+nLimit));
		pDst += nDstPitch;
		pSrc += nSrcPitch;
	}
}

void LimitChanges_float_c(unsigned char *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, int nWidth, int nHeight, float nLimit)
{
  typedef float pixel_t;
  for (int h=0; h<nHeight; h++)
  {
    for (int i=0; i<nWidth; i++)
      reinterpret_cast<pixel_t *>(pDst)[i] = 
      (pixel_t)clamp(reinterpret_cast<pixel_t *>(pDst)[i], (reinterpret_cast<const pixel_t *>(pSrc)[i]-nLimit), (reinterpret_cast<const pixel_t *>(pSrc)[i]+nLimit));
    pDst += nDstPitch;
    pSrc += nSrcPitch;
  }
}

// if a non-static template function is in cpp, we have to instantiate it
template void LimitChanges_c<uint8_t>(unsigned char *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, int nWidth, int nHeight, int nLimit);
template void LimitChanges_c<uint16_t>(unsigned char *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, int nWidth, int nHeight, int nLimit);
template void LimitChanges_sse2_new<uint8_t,0>(unsigned char *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, int nWidth, int nHeight, int nLimit);
template void LimitChanges_sse2_new<uint16_t,0>(unsigned char *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, int nWidth, int nHeight, int nLimit);
template void LimitChanges_sse2_new<uint16_t,1>(unsigned char *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, int nWidth, int nHeight, int nLimit);

template <typename pixel_t, int blockWidth, int blockHeight>
// pDst is short* for 8 bit, int * for 16 bit
// works for blockWidth >= 4 && uint16_t
// only src_pitch is byte-level
// dstPitch knows int* or short* 
// winPitch knows short *
void Overlaps_sse4(unsigned short *pDst0, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch)
{
  // pWin from 0 to 2048
  // when pixel_t == uint16_t, dst should be int*
  typedef typename std::conditional < sizeof(pixel_t) == 1, short, int>::type target_t;
  target_t *pDst = reinterpret_cast<target_t *>(pDst0);
  __m128i zero = _mm_setzero_si128();
  //const int stride = BlockWidth * sizeof(pixel_t); // back to byte size
  assert(sizeof(pixel_t) != 1);

  for (int j = 0; j < blockHeight; j++)
  {
    __m128i dst;
    __m128i win, src;

    if (sizeof(pixel_t) == 1) {
      assert(0); // not implemented
    }
    else if (sizeof(pixel_t) == 2)
    {
      if (blockWidth == 4) // half of 1x16 byte
      {
        win = _mm_loadl_epi64(reinterpret_cast<__m128i *>(pWin)); // 4x16 short: Window
        src = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(pSrc)); // 4x16 uint16_t: source pixels

        __m128i reslo = _mm_mullo_epi32(_mm_unpacklo_epi16(src, zero), _mm_unpacklo_epi16(win, zero));
        dst = _mm_loadu_si128(reinterpret_cast<__m128i *>(pDst)); // 4x32 int: destination pixels
        dst = _mm_add_epi32(dst, reslo);
        _mm_storeu_si128(reinterpret_cast<__m128i *>(pDst), dst);

      }
      else if (blockWidth == 8) // exact 1x16 byte
      {
        win = _mm_loadu_si128(reinterpret_cast<__m128i *>(pWin)); // 8x16 short: Window
        src = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pSrc)); // 8x16 uint16_t: source pixels

        __m128i reslo = _mm_mullo_epi32(_mm_unpacklo_epi16(src, zero), _mm_unpacklo_epi16(win, zero));
        dst = _mm_loadu_si128(reinterpret_cast<__m128i *>(pDst)); // 4x32 int: destination pixels
        dst = _mm_add_epi32(dst, reslo);
        _mm_storeu_si128(reinterpret_cast<__m128i *>(pDst), dst);

        __m128i reshi = _mm_mullo_epi32(_mm_unpackhi_epi16(src, zero), _mm_unpackhi_epi16(win, zero));
        dst = _mm_loadu_si128(reinterpret_cast<__m128i *>(pDst + 4)); // next 4x32 int: destination pixels
        dst = _mm_add_epi32(dst, reshi);
        _mm_storeu_si128(reinterpret_cast<__m128i *>(pDst + 4), dst);
      }
      else if (blockWidth == 16) // 2x16 byte: 2x8 pixels
      {
        win = _mm_loadu_si128(reinterpret_cast<__m128i *>(pWin)); // 8x16 short: Window
        src = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pSrc)); // 8x16 uint16_t: source pixels

        __m128i reslo = _mm_mullo_epi32(_mm_unpacklo_epi16(src, zero), _mm_unpacklo_epi16(win, zero));
        dst = _mm_loadu_si128(reinterpret_cast<__m128i *>(pDst)); // 4x32 int: destination pixels
        dst = _mm_add_epi32(dst, reslo);
        _mm_storeu_si128(reinterpret_cast<__m128i *>(pDst), dst);

        __m128i reshi = _mm_mullo_epi32(_mm_unpackhi_epi16(src, zero), _mm_unpackhi_epi16(win, zero));
        dst = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pDst + 4)); // next 4x32 int: destination pixels
        dst = _mm_add_epi32(dst, reshi);
        _mm_storeu_si128(reinterpret_cast<__m128i *>(pDst + 4), dst);

        win = _mm_loadu_si128(reinterpret_cast<__m128i *>(pWin + 8)); // next 8x16 short: Window
        src = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pSrc + 16)); // next 8x16 uint16_t: source pixels

        // once again
        reslo = _mm_mullo_epi32(_mm_unpacklo_epi16(src, zero), _mm_unpacklo_epi16(win, zero));
        dst = _mm_loadu_si128(reinterpret_cast<__m128i *>(pDst + 8)); // 4x32 int: destination pixels
        dst = _mm_add_epi32(dst, reslo);
        _mm_storeu_si128(reinterpret_cast<__m128i *>(pDst + 8), dst);

        reshi = _mm_mullo_epi32(_mm_unpackhi_epi16(src, zero), _mm_unpackhi_epi16(win, zero));
        dst = _mm_loadu_si128(reinterpret_cast<__m128i *>(pDst + 8 + 4)); // next 4x32 int: destination pixels
        dst = _mm_add_epi32(dst, reshi);
        _mm_storeu_si128(reinterpret_cast<__m128i *>(pDst + 8 + 4), dst);
      }
      else if ((blockWidth % (16 / sizeof(pixel_t))) == 0) {
        for (int x = 0; x < blockWidth; x += 16 / sizeof(pixel_t)) {
          win = _mm_loadu_si128(reinterpret_cast<__m128i *>(pWin + x)); // 8x16 short: Window
          src = _mm_loadu_si128(reinterpret_cast<const __m128i *>(pSrc + x * 2)); // 8x16 uint16_t: source pixels

          __m128i reslo = _mm_mullo_epi32(_mm_unpacklo_epi16(src, zero), _mm_unpacklo_epi16(win, zero));
          dst = _mm_loadu_si128(reinterpret_cast<__m128i *>(pDst + x)); // 4x32 int: destination pixels
          dst = _mm_add_epi32(dst, reslo);
          _mm_storeu_si128(reinterpret_cast<__m128i *>(pDst + x), dst);

          __m128i reshi = _mm_mullo_epi32(_mm_unpackhi_epi16(src, zero), _mm_unpackhi_epi16(win, zero));
          dst = _mm_loadu_si128(reinterpret_cast<__m128i *>(pDst + x + 4)); // next 4x32 int: destination pixels
          dst = _mm_add_epi32(dst, reshi);
          _mm_storeu_si128(reinterpret_cast<__m128i *>(pDst + x + 4), dst);
        }
      }
      else if ((blockWidth % (8 / sizeof(pixel_t))) == 0) {
        for (int x = 0; x < blockWidth; x += 8 / sizeof(pixel_t)) {
          win = _mm_loadl_epi64(reinterpret_cast<__m128i *>(pWin + x)); // 4x16 short: Window
          src = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(pSrc + x * 2)); // 4x16 uint16_t: source pixels

          __m128i reslo = _mm_mullo_epi32(_mm_unpacklo_epi16(src, zero), _mm_unpacklo_epi16(win, zero));
          dst = _mm_loadu_si128(reinterpret_cast<__m128i *>(pDst+x)); // 4x32 int: destination pixels
          dst = _mm_add_epi32(dst, reslo);
          _mm_storeu_si128(reinterpret_cast<__m128i *>(pDst+x), dst);
        }
      }
      else {
        assert(0);
      }
    } // pixel_t == 2

    /*
      for (int i=0; i<blockWidth; i++)
      {
        if(sizeof(pixel_t) == 1)
          pDst[i] = ( pDst[i] + ((reinterpret_cast<const pixel_t *>(pSrc)[i]*pWin[i] + 256)>> 6)); // shift 5 in Short2Bytes<uint8_t> in overlap.cpp
        else
          pDst[i] = ( pDst[i] + ((reinterpret_cast<const pixel_t *>(pSrc)[i]*pWin[i]))); // shift (5+6); in Short2Bytes16
                                                                                         // no shift 6
      }
      */
    pDst += nDstPitch;
    pSrc += nSrcPitch;
    pWin += nWinPitch;
  }
}


OverlapsFunction *get_overlaps_function(int BlockX, int BlockY, int pixelsize, arch_t arch)
{
    //---------- OVERLAPS
    // 8 bit only (pixelsize==1)
    // BlkSizeX, BlkSizeY, pixelsize, arch_t
    std::map<std::tuple<int, int, int, arch_t>, OverlapsFunction*> func_overlaps;
    using std::make_tuple;
    // define C for 8/16 bits
#define MAKE_OVR_FN(x, y) func_overlaps[make_tuple(x, y, 1, NO_SIMD)] = Overlaps_C<uint8_t, x, y>; \
func_overlaps[make_tuple(x, y, 2, NO_SIMD)] = Overlaps_C<uint16_t, x, y>;
    // sad, copy, overlap, luma, should be the same list
    MAKE_OVR_FN(64, 64)
      MAKE_OVR_FN(64, 48)
      MAKE_OVR_FN(64, 32)
      MAKE_OVR_FN(64, 16)
      MAKE_OVR_FN(48, 64)
      MAKE_OVR_FN(48, 48)
      MAKE_OVR_FN(48, 24)
      MAKE_OVR_FN(48, 12)
      MAKE_OVR_FN(32, 64)
      MAKE_OVR_FN(32, 32)
      MAKE_OVR_FN(32, 24)
      MAKE_OVR_FN(32, 16)
      MAKE_OVR_FN(32, 8)
      MAKE_OVR_FN(24, 48)
      MAKE_OVR_FN(24, 32)
      MAKE_OVR_FN(24, 24)
      MAKE_OVR_FN(24, 12)
      MAKE_OVR_FN(24, 6)
      MAKE_OVR_FN(16, 64)
      MAKE_OVR_FN(16, 32)
      MAKE_OVR_FN(16, 16)
      MAKE_OVR_FN(16, 12)
      MAKE_OVR_FN(16, 8)
      MAKE_OVR_FN(16, 4)
      MAKE_OVR_FN(16, 2)
      MAKE_OVR_FN(16, 1)
      MAKE_OVR_FN(12, 48)
      MAKE_OVR_FN(12, 24)
      MAKE_OVR_FN(12, 16)
      MAKE_OVR_FN(12, 12)
      MAKE_OVR_FN(12, 6)
      MAKE_OVR_FN(12, 3)
      MAKE_OVR_FN(8, 32)
      MAKE_OVR_FN(8, 16)
      MAKE_OVR_FN(8, 8)
      MAKE_OVR_FN(8, 4)
      MAKE_OVR_FN(8, 2)
      MAKE_OVR_FN(8, 1)
      MAKE_OVR_FN(6, 24)
      MAKE_OVR_FN(6, 12)
      MAKE_OVR_FN(6, 6)
      MAKE_OVR_FN(6, 3)
      MAKE_OVR_FN(4, 8)
      MAKE_OVR_FN(4, 4)
      MAKE_OVR_FN(4, 2)
      MAKE_OVR_FN(4, 1)
      MAKE_OVR_FN(3, 6)
      MAKE_OVR_FN(3, 3)
      MAKE_OVR_FN(2, 4)
      MAKE_OVR_FN(2, 2)
      MAKE_OVR_FN(2, 1)
#undef MAKE_OVR_FN

    func_overlaps[make_tuple(64, 64, 1, USE_SSE2)] = Overlaps64x64_sse2; // in overlap-a.asm
    func_overlaps[make_tuple(64, 48, 1, USE_SSE2)] = Overlaps64x48_sse2;
    func_overlaps[make_tuple(64, 32, 1, USE_SSE2)] = Overlaps64x32_sse2;
    func_overlaps[make_tuple(64, 16, 1, USE_SSE2)] = Overlaps64x16_sse2;
    func_overlaps[make_tuple(48, 64, 1, USE_SSE2)] = Overlaps48x64_sse2;
    func_overlaps[make_tuple(48, 48, 1, USE_SSE2)] = Overlaps48x48_sse2;
    func_overlaps[make_tuple(48, 24, 1, USE_SSE2)] = Overlaps48x24_sse2;
    func_overlaps[make_tuple(48, 12, 1, USE_SSE2)] = Overlaps48x12_sse2;
    func_overlaps[make_tuple(32, 64, 1, USE_SSE2)] = Overlaps32x64_sse2;
    func_overlaps[make_tuple(32, 32, 1, USE_SSE2)] = Overlaps32x32_sse2;
    func_overlaps[make_tuple(32, 24, 1, USE_SSE2)] = Overlaps32x24_sse2;
    func_overlaps[make_tuple(32, 16, 1, USE_SSE2)] = Overlaps32x16_sse2;
    func_overlaps[make_tuple(32, 8 , 1, USE_SSE2)] = Overlaps32x8_sse2;
    func_overlaps[make_tuple(24, 48, 1, USE_SSE2)] = Overlaps24x48_sse2;
    func_overlaps[make_tuple(24, 32, 1, USE_SSE2)] = Overlaps24x32_sse2;
    func_overlaps[make_tuple(24, 24, 1, USE_SSE2)] = Overlaps24x24_sse2;
    func_overlaps[make_tuple(24, 12, 1, USE_SSE2)] = Overlaps24x12_sse2;
    func_overlaps[make_tuple(24, 6, 1, USE_SSE2)] = Overlaps24x6_sse2;
    func_overlaps[make_tuple(16, 64, 1, USE_SSE2)] = Overlaps16x64_sse2;
    func_overlaps[make_tuple(16, 32, 1, USE_SSE2)] = Overlaps16x32_sse2;
    func_overlaps[make_tuple(16, 16, 1, USE_SSE2)] = Overlaps16x16_sse2;
    func_overlaps[make_tuple(16, 12, 1, USE_SSE2)] = Overlaps16x12_sse2;
    func_overlaps[make_tuple(16, 8 , 1, USE_SSE2)] = Overlaps16x8_sse2;
    func_overlaps[make_tuple(16, 4 , 1, USE_SSE2)] = Overlaps16x4_sse2;
    func_overlaps[make_tuple(16, 2 , 1, USE_SSE2)] = Overlaps16x2_sse2;
    func_overlaps[make_tuple(12, 48, 1, USE_SSE2)] = Overlaps12x48_sse2;
    func_overlaps[make_tuple(12, 24, 1, USE_SSE2)] = Overlaps12x24_sse2;
    func_overlaps[make_tuple(12, 16, 1, USE_SSE2)] = Overlaps12x16_sse2;
    func_overlaps[make_tuple(12, 12, 1, USE_SSE2)] = Overlaps12x12_sse2;
    func_overlaps[make_tuple(12,  6, 1, USE_SSE2)] = Overlaps12x6_sse2;
    func_overlaps[make_tuple(12, 3, 1, USE_SSE2)] = Overlaps12x3_sse2;
    func_overlaps[make_tuple(8, 32, 1, USE_SSE2)] = Overlaps8x32_sse2;
    func_overlaps[make_tuple(8 , 16, 1, USE_SSE2)] = Overlaps8x16_sse2;
    func_overlaps[make_tuple(8 , 8 , 1, USE_SSE2)] = Overlaps8x8_sse2;
    func_overlaps[make_tuple(8 , 4 , 1, USE_SSE2)] = Overlaps8x4_sse2;
    func_overlaps[make_tuple(8 , 2 , 1, USE_SSE2)] = Overlaps8x2_sse2;
    func_overlaps[make_tuple(8 , 1 , 1, USE_SSE2)] = Overlaps8x1_sse2;
    // todo 6x
    func_overlaps[make_tuple(4 , 8 , 1, USE_SSE2)] = Overlaps4x8_sse2;
    func_overlaps[make_tuple(4 , 4 , 1, USE_SSE2)] = Overlaps4x4_sse2;
    func_overlaps[make_tuple(4 , 2 , 1, USE_SSE2)] = Overlaps4x2_sse2;
    func_overlaps[make_tuple(2 , 4 , 1, USE_SSE2)] = Overlaps2x4_sse2;
    func_overlaps[make_tuple(2 , 2 , 1, USE_SSE2)] = Overlaps2x2_sse2;
    
    // define sse4 for 16 bits
#define MAKE_OVR_FN(x, y) func_overlaps[make_tuple(x, y, 2, USE_SSE41)] = Overlaps_sse4<uint16_t, x, y>;
    MAKE_OVR_FN(64, 64)
      MAKE_OVR_FN(64, 48)
      MAKE_OVR_FN(64, 32)
      MAKE_OVR_FN(64, 16)
      MAKE_OVR_FN(48, 64)
      MAKE_OVR_FN(48, 48)
      MAKE_OVR_FN(48, 24)
      MAKE_OVR_FN(48, 12)
      MAKE_OVR_FN(32, 64)
      MAKE_OVR_FN(32, 32)
      MAKE_OVR_FN(32, 24)
      MAKE_OVR_FN(32, 16)
      MAKE_OVR_FN(32, 8)
      MAKE_OVR_FN(24, 48)
      MAKE_OVR_FN(24, 32)
      MAKE_OVR_FN(24, 24)
      MAKE_OVR_FN(24, 12)
      MAKE_OVR_FN(24, 6)
      MAKE_OVR_FN(16, 64)
      MAKE_OVR_FN(16, 32)
      MAKE_OVR_FN(16, 16)
      MAKE_OVR_FN(16, 12)
      MAKE_OVR_FN(16, 8)
      MAKE_OVR_FN(16, 4)
      MAKE_OVR_FN(16, 2)
      MAKE_OVR_FN(16, 1)
      MAKE_OVR_FN(12, 48)
      MAKE_OVR_FN(12, 24)
      MAKE_OVR_FN(12, 16)
      MAKE_OVR_FN(12, 12)
      MAKE_OVR_FN(12, 6)
      MAKE_OVR_FN(12, 3)
      MAKE_OVR_FN(8, 32)
      MAKE_OVR_FN(8, 16)
      MAKE_OVR_FN(8, 8)
      MAKE_OVR_FN(8, 4)
      MAKE_OVR_FN(8, 2)
      MAKE_OVR_FN(8, 1)
      //MAKE_OVR_FN(6, 24)
      //MAKE_OVR_FN(6, 12)
      //MAKE_OVR_FN(6, 6)
      //MAKE_OVR_FN(6, 3)  // todo
      MAKE_OVR_FN(4, 8)
      MAKE_OVR_FN(4, 4)
      MAKE_OVR_FN(4, 2)
      // not supported:
      //MAKE_OVR_FN(4, 1)
      //MAKE_OVR_FN(2, 4)
      //MAKE_OVR_FN(2, 2)
      //MAKE_OVR_FN(2, 1)
#undef MAKE_OVR_FN

    OverlapsFunction *result = nullptr;

    arch_t archlist[] = { USE_AVX2, USE_AVX, USE_SSE41, USE_SSE2, NO_SIMD };
    int index = 0;
    while (result == nullptr) {
      arch_t current_arch_try = archlist[index++];
      if (current_arch_try > arch) continue;
      result = func_overlaps[make_tuple(BlockX, BlockY, pixelsize, current_arch_try)];
      if (result == nullptr && current_arch_try == NO_SIMD)
        break;
    }
    return result;
}

OverlapsLsbFunction *get_overlaps_lsb_function(int BlockX, int BlockY, int pixelsize, arch_t arch)
{
    // 8 bit only (pixelsize==1)
    // BlkSizeX, BlkSizeY, pixelsize, arch_t
    std::map<std::tuple<int, int, int, arch_t>, OverlapsLsbFunction*> func_overlaps_lsb;
    using std::make_tuple;
    // define C for 8/16 bits
#define MAKE_OVR_FN(x, y) func_overlaps_lsb[make_tuple(x, y, 1, NO_SIMD)] = OverlapsLsb_C<x, y>;
    // sad, copy, overlap, luma, should be the same list
    MAKE_OVR_FN(64, 64)
      MAKE_OVR_FN(64, 48)
      MAKE_OVR_FN(64, 32)
      MAKE_OVR_FN(64, 16)
      MAKE_OVR_FN(48, 64)
      MAKE_OVR_FN(48, 48)
      MAKE_OVR_FN(48, 24)
      MAKE_OVR_FN(48, 12)
      MAKE_OVR_FN(32, 64)
      MAKE_OVR_FN(32, 32)
      MAKE_OVR_FN(32, 24)
      MAKE_OVR_FN(32, 16)
      MAKE_OVR_FN(32, 8)
      MAKE_OVR_FN(24, 48)
      MAKE_OVR_FN(24, 32)
      MAKE_OVR_FN(24, 24)
      MAKE_OVR_FN(24, 12)
      MAKE_OVR_FN(24, 6)
      MAKE_OVR_FN(16, 64)
      MAKE_OVR_FN(16, 32)
      MAKE_OVR_FN(16, 16)
      MAKE_OVR_FN(16, 12)
      MAKE_OVR_FN(16, 8)
      MAKE_OVR_FN(16, 4)
      MAKE_OVR_FN(16, 2)
      MAKE_OVR_FN(16, 1)
      MAKE_OVR_FN(12, 48)
      MAKE_OVR_FN(12, 24)
      MAKE_OVR_FN(12, 16)
      MAKE_OVR_FN(12, 12)
      MAKE_OVR_FN(12, 6)
      MAKE_OVR_FN(12, 3)
      MAKE_OVR_FN(8, 32)
      MAKE_OVR_FN(8, 16)
      MAKE_OVR_FN(8, 8)
      MAKE_OVR_FN(8, 4)
      MAKE_OVR_FN(8, 2)
      MAKE_OVR_FN(8, 1)
      MAKE_OVR_FN(6, 24)
      MAKE_OVR_FN(6, 12)
      MAKE_OVR_FN(6, 6)
      MAKE_OVR_FN(6, 3)
      MAKE_OVR_FN(4, 8)
      MAKE_OVR_FN(4, 4)
      MAKE_OVR_FN(4, 2)
      MAKE_OVR_FN(4, 1)
      MAKE_OVR_FN(3, 6)
      MAKE_OVR_FN(3, 3)
      MAKE_OVR_FN(2, 4)
      MAKE_OVR_FN(2, 2)
      MAKE_OVR_FN(2, 1)
#undef MAKE_OVR_FN

    OverlapsLsbFunction *result = nullptr;
    arch_t archlist[] = { USE_AVX2, USE_AVX, USE_SSE41, USE_SSE2, NO_SIMD };
    int index = 0;
    while (result == nullptr) {
      arch_t current_arch_try = archlist[index++];
      if (current_arch_try > arch) continue;
      result = func_overlaps_lsb[make_tuple(BlockX, BlockY, pixelsize, current_arch_try)];
      if (result == nullptr && current_arch_try == NO_SIMD)
        break;
    }

    return result;
}



