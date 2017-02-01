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
//			pDst[i] = a | ((255-a) >> (sizeof(int)*8-1)); // tricky but conditional move can be faster nowadays
      /*
      mov	ecx, 255				; 000000ffH
      shr	edx, 5
      sub	ecx, edx
      sar	ecx, 31					; 0000001fH
      or	cl, dl
      mov	BYTE PTR [eax+esi], cl
      */
			pDst[i] = min(255, a); // PF everyone can understand it
/*
cmp	edx, ebp
movzx	ecx, dl
cmovg	ecx, ebp
mov	BYTE PTR [eax+esi], cl
*/
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



// MVDegrains-a.asm ported to intrinsics + 16 bit PF 161002
template<typename pixel_t>
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
        // dst <= (src + limit)  ==>   dst - src - limit <= 0  ==> zero where dst was ok (under limit) (dst <= (src + limit) == true)
        __m128i compare_tmp = _mm_subs_epu8(dst, src); // dst - orig,   saturated to 0
        compare_tmp = _mm_subs_epu8(compare_tmp, src_plus_limit);  // did it change too much, Y where nonzero
        compare_tmp = _mm_cmpeq_epi8(compare_tmp, zero); // now ff where new value should be used, else 

        __m128i m1 = _mm_and_si128(dst, compare_tmp); //  use new value from dst for these pixels
        __m128i m2 = _mm_andnot_si128(compare_tmp, src_plus_limit); // use max value for these pixels
        __m128i dst_maxlimited = _mm_or_si128(m1, m2); // combine them, get result with limited  positive correction

        // here we use dst_maxlimited instead of dst
        __m128i src_minus_limit = _mm_subs_epu8(src, limits);   //  min possible
        // dst_maxlimited >= (src - limit)  ==>  0 >= src - dst_maxlimited - limit   ==> zero where dst was ok 
        compare_tmp = _mm_subs_epu8(src, dst_maxlimited); // orig - dst_maxlimited,   saturated to 0
        compare_tmp = _mm_subs_epu8(compare_tmp, src_minus_limit);  // did it change too much, Y where nonzero
        compare_tmp = _mm_cmpeq_epi8(compare_tmp, zero); // now ff where new value should be used, else 00

        m1 = _mm_and_si128(dst_maxlimited, compare_tmp); //  use new value for these pixels
        m2 = _mm_andnot_si128(compare_tmp, src_minus_limit); // use min value for these pixels
        res = _mm_or_si128(m1, m2); // combine them, get result with limited  negative correction

      }
      else {

        __m128i src_plus_limit = _mm_adds_epu16(src, limits);   //  max possible
                                                               // dst <= (src + limit)  ==>   dst - src - limit <= 0  ==> zero where dst was ok (under limit) (dst <= (src + limit) == true)
        __m128i compare_tmp = _mm_subs_epu16(dst, src); // dst - orig,   saturated to 0
        compare_tmp = _mm_subs_epu16(compare_tmp, src_plus_limit);  // did it change too much, Y where nonzero
        compare_tmp = _mm_cmpeq_epi16(compare_tmp, zero); // now ff where new value should be used, else 

        __m128i m1 = _mm_and_si128(dst, compare_tmp); //  use new value from dst for these pixels
        __m128i m2 = _mm_andnot_si128(compare_tmp, src_plus_limit); // use max value for these pixels
        __m128i dst_maxlimited = _mm_or_si128(m1, m2); // combine them, get result with limited  positive correction

                                                       // here we use dst_maxlimited instead of dst
        __m128i src_minus_limit = _mm_subs_epu16(src, limits);   //  min possible
                                                                // dst_maxlimited >= (src - limit)  ==>  0 >= src - dst_maxlimited - limit   ==> zero where dst was ok 
        compare_tmp = _mm_subs_epu16(src, dst_maxlimited); // orig - dst_maxlimited,   saturated to 0
        compare_tmp = _mm_subs_epu16(compare_tmp, src_minus_limit);  // did it change too much, Y where nonzero
        compare_tmp = _mm_cmpeq_epi16(compare_tmp, zero); // now ff where new value should be used, else 00

        m1 = _mm_and_si128(dst_maxlimited, compare_tmp); //  use new value for these pixels
        m2 = _mm_andnot_si128(compare_tmp, src_minus_limit); // use min value for these pixels
        res = _mm_or_si128(m1, m2); // combine them, get result with limited  negative correction
      }
      _mm_stream_si128((__m128i *)(pDst8 + x), res);
    }
    //  reinterpret_cast<pixel_t *>(pDst)[i] = 
    //  (pixel_t)clamp((int)reinterpret_cast<pixel_t *>(pDst)[i], (reinterpret_cast<const pixel_t *>(pSrc)[i]-nLimit), (reinterpret_cast<const pixel_t *>(pSrc)[i]+nLimit));
    pDst8 += nDstPitch;
    pSrc8 += nSrcPitch;
  }

#if 0
  original external asm from MVDegrains - a.asm
    .h_loopy:
  ; limit is no longer needed
    xor r6, r6

    .h_loopx:
  ; srcp and dstp should be aligned
    movdqa m0, [srcpq + r6]; src bytes
    movdqa m1, [dstpq + r6]; dest bytes
    movdqa m2, m5;/* copy limit */
  paddusb m2, m0;/* max possible (m0 is original) */
  movdqa m3, m1;/* (m1 is changed) */
  psubusb m3, m0;/* changed - orig,   saturated to 0 */
  psubusb m3, m5;/* did it change too much, Y where nonzero */
  pcmpeqb m3, m4;/* now ff where new value should be used, else 00 (m4=0)*/
  pand m1, m3;    /* use new value for these pixels */
  pandn m3, m2; /* use max value for these pixels */
  por m1, m3;/* combine them, get result with limited  positive correction */

  movdqa m3, m5;/* copy limit */
  movdqa m2, m0;/* copy orig */
  psubusb m2, m5;/* min possible */
  movdqa m3, m0;/* copy orig */
  psubusb m3, m1;/* orig - changed, saturated to 0 */
  psubusb m3, m5;/* did it change too much, Y where nonzero */
  pcmpeqb m3, m4;/* now ff where new value should be used, else 00 */

  pand m1, m3;/* use new value for these pixels */
  pandn m3, m2;/* use min value for these pixels */
  por m1, m3;/* combine them, get result with limited  negative correction */

  movdqa[dstpq + r6], m1
    add r6, 16
    cmp r6, widthq
    jl.h_loopx

    ; do not process rightmost rest bytes

    add srcpq, src_strideq
    add dstpq, dst_strideq
    dec heightq
    jnz.h_loopy

    RET
#endif
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
template void LimitChanges_sse2_new<uint8_t>(unsigned char *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, int nWidth, int nHeight, int nLimit);
template void LimitChanges_sse2_new<uint16_t>(unsigned char *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, int nWidth, int nHeight, int nLimit);

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

  for (int j=0; j<blockHeight; j++)
  {
    __m128i dst;
    __m128i win, src;

    if (sizeof(pixel_t) == 2) {
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
      else {
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
    func_overlaps[make_tuple(32, 32, 1, NO_SIMD)] = Overlaps_C<uint8_t, 32, 32>;
    func_overlaps[make_tuple(32, 16, 1, NO_SIMD)] = Overlaps_C<uint8_t, 32, 16>;
    func_overlaps[make_tuple(32, 8 , 1, NO_SIMD)] = Overlaps_C<uint8_t, 32, 8>;
    func_overlaps[make_tuple(16, 32, 1, NO_SIMD)] = Overlaps_C<uint8_t, 16, 32>;
    func_overlaps[make_tuple(16, 16, 1, NO_SIMD)] = Overlaps_C<uint8_t, 16, 16>;
    func_overlaps[make_tuple(16, 8 , 1, NO_SIMD)] = Overlaps_C<uint8_t, 16, 8>;
    func_overlaps[make_tuple(16, 4 , 1, NO_SIMD)] = Overlaps_C<uint8_t, 16, 4>;
    func_overlaps[make_tuple(16, 2 , 1, NO_SIMD)] = Overlaps_C<uint8_t, 16, 2>;
    func_overlaps[make_tuple(8 , 16, 1, NO_SIMD)] = Overlaps_C<uint8_t, 8 , 16>;
    func_overlaps[make_tuple(8 , 8 , 1, NO_SIMD)] = Overlaps_C<uint8_t, 8 , 8>;
    func_overlaps[make_tuple(8 , 4 , 1, NO_SIMD)] = Overlaps_C<uint8_t, 8 , 4>;
    func_overlaps[make_tuple(8 , 2 , 1, NO_SIMD)] = Overlaps_C<uint8_t, 8 , 2>;
    func_overlaps[make_tuple(8 , 1 , 1, NO_SIMD)] = Overlaps_C<uint8_t, 8 , 1>;
    func_overlaps[make_tuple(4 , 8 , 1, NO_SIMD)] = Overlaps_C<uint8_t, 4 , 8>;
    func_overlaps[make_tuple(4 , 4 , 1, NO_SIMD)] = Overlaps_C<uint8_t, 4 , 4>;
    func_overlaps[make_tuple(4 , 2 , 1, NO_SIMD)] = Overlaps_C<uint8_t, 4 , 2>;
    func_overlaps[make_tuple(2 , 4 , 1, NO_SIMD)] = Overlaps_C<uint8_t, 2 , 4>;
    func_overlaps[make_tuple(2 , 2 , 1, NO_SIMD)] = Overlaps_C<uint8_t, 2 , 2>;

    func_overlaps[make_tuple(32, 32, 2, NO_SIMD)] = Overlaps_C<uint16_t, 32, 32>;
    func_overlaps[make_tuple(32, 16, 2, NO_SIMD)] = Overlaps_C<uint16_t, 32, 16>;
    func_overlaps[make_tuple(32, 8 , 2, NO_SIMD)] = Overlaps_C<uint16_t, 32, 8>;
    func_overlaps[make_tuple(16, 32, 2, NO_SIMD)] = Overlaps_C<uint16_t, 16, 32>;
    func_overlaps[make_tuple(16, 16, 2, NO_SIMD)] = Overlaps_C<uint16_t, 16, 16>;
    func_overlaps[make_tuple(16, 8 , 2, NO_SIMD)] = Overlaps_C<uint16_t, 16, 8>;
    func_overlaps[make_tuple(16, 4 , 2, NO_SIMD)] = Overlaps_C<uint16_t, 16, 4>;
    func_overlaps[make_tuple(16, 2 , 2, NO_SIMD)] = Overlaps_C<uint16_t, 16, 2>;
    func_overlaps[make_tuple(8 , 16, 2, NO_SIMD)] = Overlaps_C<uint16_t, 8 , 16>;
    func_overlaps[make_tuple(8 , 8 , 2, NO_SIMD)] = Overlaps_C<uint16_t, 8 , 8>;
    func_overlaps[make_tuple(8 , 4 , 2, NO_SIMD)] = Overlaps_C<uint16_t, 8 , 4>;
    func_overlaps[make_tuple(8 , 2 , 2, NO_SIMD)] = Overlaps_C<uint16_t, 8 , 2>;
    func_overlaps[make_tuple(8 , 1 , 2, NO_SIMD)] = Overlaps_C<uint16_t, 8 , 1>;
    func_overlaps[make_tuple(4 , 8 , 2, NO_SIMD)] = Overlaps_C<uint16_t, 4 , 8>;
    func_overlaps[make_tuple(4 , 4 , 2, NO_SIMD)] = Overlaps_C<uint16_t, 4 , 4>;
    func_overlaps[make_tuple(4 , 2 , 2, NO_SIMD)] = Overlaps_C<uint16_t, 4 , 2>;
    func_overlaps[make_tuple(2 , 4 , 2, NO_SIMD)] = Overlaps_C<uint16_t, 2 , 4>;
    func_overlaps[make_tuple(2 , 2 , 2, NO_SIMD)] = Overlaps_C<uint16_t, 2 , 2>;

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
    
    func_overlaps[make_tuple(32, 32, 2, USE_SSE41)] = Overlaps_sse4<uint16_t, 32, 32>;
    func_overlaps[make_tuple(32, 16, 2, USE_SSE41)] = Overlaps_sse4<uint16_t, 32, 16>;
    func_overlaps[make_tuple(32, 8 , 2, USE_SSE41)] = Overlaps_sse4<uint16_t, 32, 8>;
    func_overlaps[make_tuple(16, 32, 2, USE_SSE41)] = Overlaps_sse4<uint16_t, 16, 32>;
    func_overlaps[make_tuple(16, 16, 2, USE_SSE41)] = Overlaps_sse4<uint16_t, 16, 16>;
    func_overlaps[make_tuple(16, 8 , 2, USE_SSE41)] = Overlaps_sse4<uint16_t, 16, 8>;
    func_overlaps[make_tuple(16, 4 , 2, USE_SSE41)] = Overlaps_sse4<uint16_t, 16, 4>;
    func_overlaps[make_tuple(16, 2 , 2, USE_SSE41)] = Overlaps_sse4<uint16_t, 16, 2>;
    func_overlaps[make_tuple(8 , 16, 2, USE_SSE41)] = Overlaps_sse4<uint16_t, 8 , 16>;
    func_overlaps[make_tuple(8 , 8 , 2, USE_SSE41)] = Overlaps_sse4<uint16_t, 8 , 8>;
    func_overlaps[make_tuple(8 , 4 , 2, USE_SSE41)] = Overlaps_sse4<uint16_t, 8 , 4>;
    func_overlaps[make_tuple(8 , 2 , 2, USE_SSE41)] = Overlaps_sse4<uint16_t, 8 , 2>;
    func_overlaps[make_tuple(8 , 1 , 2, USE_SSE41)] = Overlaps_sse4<uint16_t, 8 , 1>;
    func_overlaps[make_tuple(4 , 8 , 2, USE_SSE41)] = Overlaps_sse4<uint16_t, 4 , 8>;
    func_overlaps[make_tuple(4 , 4 , 2, USE_SSE41)] = Overlaps_sse4<uint16_t, 4 , 4>;
    func_overlaps[make_tuple(4 , 2 , 2, USE_SSE41)] = Overlaps_sse4<uint16_t, 4 , 2>;
    //func_overlaps[make_tuple(2 , 4 , 2, NO_SIMD)] = Overlaps_C<uint16_t, 2 , 4>;
    //func_overlaps[make_tuple(2 , 2 , 2, NO_SIMD)] = Overlaps_C<uint16_t, 2 , 2>;
    


#if 0
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
#endif
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
#if 0
    OverlapsFunction *result = func_overlaps[std::make_tuple(BlockX, BlockY, pixelsize, arch)];
    arch_t arch_orig = arch;
    // no AVX2 -> try AVX
    if (result == nullptr && (arch==USE_AVX2 || arch_orig==USE_AVX)) {
      arch = USE_AVX;
      result = func_overlaps[make_tuple(BlockX, BlockY, pixelsize, USE_AVX)];
    }
    // no AVX -> try SSE41
    if (result == nullptr && (arch==USE_AVX || arch_orig==USE_SSE41)) {
      arch = USE_SSE41;
      result = func_overlaps[make_tuple(BlockX, BlockY, pixelsize, USE_SSE41)];
    }
    // no SSE41 -> try SSE2
    if (result == nullptr && (arch==USE_SSE41 || arch_orig==USE_SSE2)) {
      arch = USE_SSE2;
      result = func_overlaps[make_tuple(BlockX, BlockY, pixelsize, USE_SSE2)];
    }
    // no SSE2 -> try C
    if (result == nullptr && (arch==USE_SSE2 || arch_orig==NO_SIMD)) {
      arch = NO_SIMD;
      /* C version variations are only working in SAD
      // priority: C version compiled to avx2, avx
      if(arch_orig==USE_AVX2)
      result = get_luma_avx2_C_function(BlockX, BlockY, pixelsize, NO_SIMD);
      else if(arch_orig==USE_AVX)
      result = get_luma_avx_C_function(BlockX, BlockY, pixelsize, NO_SIMD);
      */
      if(result == nullptr)
        result = func_overlaps[make_tuple(BlockX, BlockY, pixelsize, NO_SIMD)]; // fallback to C
    }
#endif
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

#if 0
    OverlapsLsbFunction *result = func_overlaps_lsb[std::make_tuple(BlockX, BlockY, pixelsize, arch)];
    if (result == nullptr)
        result = func_overlaps_lsb[std::make_tuple(BlockX, BlockY, pixelsize, NO_SIMD)]; // fallback to C
#endif
    return result;
}



