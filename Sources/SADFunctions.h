// Functions that computes distances between blocks

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

/*! \file SADFunctions.h
 *  \brief Calculate the sum of absolute differences between two blocks.
 *
 *	The sum of absolute differences between two blocks represents the distance
 *	between these two blocks. The lower this value, the closer the blocks will look like
 *  There are two versions, one in plain C, one in iSSE assembler.
 */

#ifndef __SAD_FUNC__
#define __SAD_FUNC__

#include "types.h"
#include <stdint.h>

typedef unsigned int (SADFunction)(const uint8_t *pSrc, int nSrcPitch,
								    const uint8_t *pRef, int nRefPitch);
SADFunction* get_sad_function(int BlockX, int BlockY, int pixelsize, arch_t arch);

SADFunction* get_satd_function(int BlockX, int BlockY, int pixelsize, arch_t arch);

//x264_pixel_satd_##blksizex##x##blksizey##_sse2

inline unsigned int SADABS(int x) {	return ( x < 0 ) ? -x : x; }
//inline unsigned int SADABS(int x) {	return ( x < -16 ) ? 16 : ( x < 0 ) ? -x : ( x > 16) ? 16 : x; }

template<int nBlkWidth, int nBlkHeight, typename pixel_t>
unsigned int Sad_C(const uint8_t *pSrc, int nSrcPitch,const uint8_t *pRef,
					     int nRefPitch)
{
	unsigned int sum = 0; // int is probably enough for 32x32
	for ( int y = 0; y < nBlkHeight; y++ )
	{
		for ( int x = 0; x < nBlkWidth; x++ )
			sum += SADABS(reinterpret_cast<const pixel_t *>(pSrc)[x] - reinterpret_cast<const pixel_t *>(pRef)[x]);
      pSrc += nSrcPitch;
      pRef += nRefPitch;
	}
	return sum;
}

/*
// add 4 floats horizontally.
// sse3 (not ssse3)
float hsum_ps_sse3(__m128 v) {
  __m128 shuf = _mm_movehdup_ps(v);        // broadcast elements 3,1 to 2,0
  __m128 sums = _mm_add_ps(v, shuf);
  shuf        = _mm_movehl_ps(shuf, sums); // high half -> low half
  sums        = _mm_add_ss(sums, shuf);
  return        _mm_cvtss_f32(sums);
  
  or ssse3
  v = _mm_hadd_ps(v, v);
  v = _mm_hadd_ps(v, v);
  or
  const __m128 t = _mm_add_ps(v, _mm_movehl_ps(v, v));
  const __m128 sum = _mm_add_ss(t, _mm_shuffle_ps(t, t, 1));
}
*/

// above width==8 for uint16_t and width==16 for uint8_t
template<int nBlkWidth, int nBlkHeight, typename pixel_t>
unsigned int Sad16_sse2(const uint8_t *pSrc, int nSrcPitch,const uint8_t *pRef, int nRefPitch)
{
  /*
  if (is_ptr_aligned(pSrc, 16) && is_ptr_aligned(pref, 16)) {
    // aligned code. though newer procs don't care
  }
  */
  //__assume_aligned(piMblk, 16);
  //__assume_aligned(piRef, 16);

  __m128i zero = _mm_setzero_si128();
  __m128i sum = _mm_setzero_si128(); // 2x or 4x int is probably enough for 32x32

  for ( int y = 0; y < nBlkHeight; y++ )
  {
    for ( int x = 0; x < nBlkWidth; x+=16 )
    {
      __m128i src1, src2;
#if 0
      // later...
      if(aligned) { // unaligned
        src1 = _mm_load_si128((__m128i *) (pSrc + x)); // 16 bytes or 8 words
        src2 = _mm_load_si128((__m128i *) (pRef + x));
      }
      else { // unaligned
        src1 = _mm_loadu_si128((__m128i *) (pSrc + x));
        src2 = _mm_loadu_si128((__m128i *) (pRef + x));
      }
#else
      if ((sizeof(pixel_t) == 2 && nBlkWidth <= 4) || (sizeof(pixel_t) == 1 && nBlkWidth <= 8)) {
        // 8 bytes or 4 words
        src1 = _mm_loadl_epi64((__m128i *) (pSrc + x));
        src2 = _mm_loadl_epi64((__m128i *) (pRef + x));
      } else {
        src1 = _mm_loadu_si128((__m128i *) (pSrc + x));
        src2 = _mm_loadu_si128((__m128i *) (pRef + x));
      }
#endif
      if(sizeof(pixel_t) == 1) {
          // this is uint_16 specific, but will test on uint8_t against external .asm SAD functions)
        sum = _mm_add_epi32(sum, _mm_sad_epu8(src1, src2)); // yihhaaa, existing SIMD   sum1_32, 0, sum2_32, 0
        // result in two 32 bit areas at the upper and lower 64 bytes
      }
      else {
        __m128i greater_t = _mm_subs_epu16(src1, src2); // unsigned sub with saturation
        __m128i smaller_t = _mm_subs_epu16(src2, src1);
        __m128i absdiff = _mm_or_si128(greater_t, smaller_t); //abs(s1-s2)  == (satsub(s1,s2) | satsub(s2,s1))
        // 8 x uint16 absolute differences
        sum = _mm_add_epi32(sum, _mm_unpacklo_epi16(absdiff, zero));
        sum = _mm_add_epi32(sum, _mm_unpackhi_epi16(absdiff, zero));
        // sum1_32, sum2_32, sum3_32, sum4_32
      }
      // sum += SADABS(reinterpret_cast<const pixel_t *>(pSrc)[x] - reinterpret_cast<const pixel_t *>(pRef)[x]);
    }
    pSrc += nSrcPitch;
    pRef += nRefPitch;
  }
  /*
                                              [Low64, Hi64]
  _mm_unpacklo_epi64(_mm_setzero_si128(), x)  [0, x0]
  _mm_unpackhi_epi64(_mm_setzero_si128(), x)  [0, x1]
  _mm_move_epi64(x)                           [x0, 0]
  _mm_unpackhi_epi64(x, _mm_setzero_si128())  [x1, 0]
  */
  if(sizeof(pixel_t) == 2) {
    // at 16 bits: we have 4 integers for sum: a0 a1 a2 a3
    __m128i a0_a1 = _mm_unpacklo_epi32(sum, zero); // a0 0 a1 0
    __m128i a2_a3 = _mm_unpackhi_epi32(sum, zero); // a2 0 a3 0
      sum = _mm_add_epi32( a0_a1, a2_a3 ); // a0+a2, 0, a1+a3, 0
    /* SSSE3:
    sum = _mm_hadd_epi32(sum, zero);  // A1+A2, B1+B2, 0+0, 0+0
    sum = _mm_hadd_epi32(sum, zero);  // A1+A2+B1+B2, 0+0+0+0, 0+0+0+0, 0+0+0+0
    */
  }
  // sum here: sum1 0 sum2 0
  if(sizeof(pixel_t) == 1 && nBlkWidth >= 16) {
    // ignore upper, as input bytes 8..15 are not counted
    // uint16_t result structure is different -> not handled here
    __m128i sum_hi = _mm_unpackhi_epi64(sum, zero); // >= faster than _mm_srli_si128(sum, 8), but with
    sum = _mm_add_epi32(sum, sum_hi);
  }
  int result = _mm_cvtsi128_si32(sum);
  return result;
}


template<int nBlkSize, typename pixel_t>
unsigned int Sad_C(const uint8_t *pSrc, int nSrcPitch,const uint8_t *pRef,
                    int nRefPitch)
{
   return Sad_C<nBlkSize, nBlkSize, pixel_t>(pSrc, pRef, nSrcPitch, nRefPitch);
}
// a litle more fast unrolled (Fizick)
/* //seems to be dead code (TSchniede)
inline unsigned int Sad2_C(const uint8_t *pSrc, const uint8_t *pRef,int nSrcPitch,
					     int nRefPitch)
{
	unsigned int sum = 0;
		sum += SADABS(pSrc[0] - pRef[0]);
		sum += SADABS(pSrc[1] - pRef[1]);
      pSrc += nSrcPitch;
      pRef += nRefPitch;
		sum += SADABS(pSrc[0] - pRef[0]);
		sum += SADABS(pSrc[1] - pRef[1]);
	return sum;
}

inline unsigned int Sad2x4_C(const uint8_t *pSrc, const uint8_t *pRef,int nSrcPitch,
					     int nRefPitch)
{
	unsigned int sum = 0;
		sum += SADABS(pSrc[0] - pRef[0]);
		sum += SADABS(pSrc[1] - pRef[1]);
      pSrc += nSrcPitch;
      pRef += nRefPitch;
		sum += SADABS(pSrc[0] - pRef[0]);
		sum += SADABS(pSrc[1] - pRef[1]);
      pSrc += nSrcPitch;
      pRef += nRefPitch;
		sum += SADABS(pSrc[0] - pRef[0]);
		sum += SADABS(pSrc[1] - pRef[1]);
      pSrc += nSrcPitch;
      pRef += nRefPitch;
		sum += SADABS(pSrc[0] - pRef[0]);
		sum += SADABS(pSrc[1] - pRef[1]);
	return sum;
}
*/
#define MK_CFUNC(functionname) extern "C" unsigned int __cdecl functionname (const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch)

#define SAD_ISSE(blsizex, blsizey) extern "C" unsigned int __cdecl Sad##blsizex##x##blsizey##_iSSE(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch)
//Sad16x16_iSSE( x,y can be: 32 16 8 4 2
SAD_ISSE(32,32);
SAD_ISSE(16,32);
SAD_ISSE(32,16);
SAD_ISSE(32,8);
SAD_ISSE(16,16);
SAD_ISSE(16,8);
SAD_ISSE(16,4);
SAD_ISSE(16,2);
SAD_ISSE(16,1);
SAD_ISSE(8,16);
SAD_ISSE(8,8);
SAD_ISSE(8,4);
SAD_ISSE(8,2);
SAD_ISSE(8,1);
SAD_ISSE(4,8);
SAD_ISSE(4,4);
SAD_ISSE(4,2);
SAD_ISSE(2,4);
SAD_ISSE(2,2);

MK_CFUNC(Sad2x2_iSSE_T); //only work with packed, aligned source block copy
MK_CFUNC(Sad2x4_iSSE_T);
//MK_CFUNC(Sad2x2_iSSE_O);


#undef SAD_ISSE
/* //allows simple wrapping SAD functions, implementation in SADFunctions.h
#define MK_CPPWRAP(blkx, blky) unsigned int  Sad##blkx##x##blky##_wrap(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch)
MK_CPPWRAP(16,16);
MK_CPPWRAP(16,8);
MK_CPPWRAP(16,2);
MK_CPPWRAP(8,16);
MK_CPPWRAP(8,8);
MK_CPPWRAP(8,4);
MK_CPPWRAP(8,2);
MK_CPPWRAP(8,1);
MK_CPPWRAP(4,8);
MK_CPPWRAP(4,4);
MK_CPPWRAP(4,2);
MK_CPPWRAP(2,4);
MK_CPPWRAP(2,2);
#undef MK_CPPWRAP
*/
/* included from x264 */
#define SAD_x264(blsizex, blsizey) extern "C" unsigned int __cdecl x264_pixel_sad_##blsizex##x##blsizey##_mmx2(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch)
//x264_pixel_sad_16x16_mmx2(   x,y can be: 16 8 4
SAD_x264(16,16);
SAD_x264(16,8);
SAD_x264(8,16);
SAD_x264(8,8);
SAD_x264(8,4);
SAD_x264(4,8);
SAD_x264(4,4);
#undef SAD_x264
//parameter is function name
MK_CFUNC(x264_pixel_sad_16x16_sse2); //non optimized cache access, for AMD?
MK_CFUNC(x264_pixel_sad_16x8_sse2);	 //non optimized cache access, for AMD?
MK_CFUNC(x264_pixel_sad_16x16_sse3); //LDDQU Pentium4E (Core1?), not for Core2!
MK_CFUNC(x264_pixel_sad_16x8_sse3);  //LDDQU Pentium4E (Core1?), not for Core2!
MK_CFUNC(x264_pixel_sad_16x16_cache64_sse2);//core2 optimized
MK_CFUNC(x264_pixel_sad_16x8_cache64_sse2);//core2 optimized
MK_CFUNC(x264_pixel_sad_16x16_cache64_ssse3);//core2 optimized
MK_CFUNC(x264_pixel_sad_16x8_cache64_ssse3); //core2 optimized

//MK_CFUNC(x264_pixel_sad_16x16_cache32_mmx2);
//MK_CFUNC(x264_pixel_sad_16x8_cache32_mmx2);
MK_CFUNC(x264_pixel_sad_16x16_cache64_mmx2);
MK_CFUNC(x264_pixel_sad_16x8_cache64_mmx2);
MK_CFUNC(x264_pixel_sad_8x16_cache32_mmx2);
MK_CFUNC(x264_pixel_sad_8x8_cache32_mmx2);
MK_CFUNC(x264_pixel_sad_8x4_cache32_mmx2);
MK_CFUNC(x264_pixel_sad_8x16_cache64_mmx2);
MK_CFUNC(x264_pixel_sad_8x8_cache64_mmx2);
MK_CFUNC(x264_pixel_sad_8x4_cache64_mmx2);

//1.9.5.3: added ssd & SATD (TSchniede)
/* alternative to SAD - SSD: squared sum of differences, VERY sensitive to noise */
MK_CFUNC(x264_pixel_ssd_16x16_mmx);
MK_CFUNC(x264_pixel_ssd_16x8_mmx);
MK_CFUNC(x264_pixel_ssd_8x16_mmx);
MK_CFUNC(x264_pixel_ssd_8x8_mmx);
MK_CFUNC(x264_pixel_ssd_8x4_mmx);
MK_CFUNC(x264_pixel_ssd_4x8_mmx);
MK_CFUNC(x264_pixel_ssd_4x4_mmx);

/* SATD: Sum of Absolute Transformed Differences, more sensitive to noise, frequency domain based - replacement to dct/SAD */
MK_CFUNC(x264_pixel_satd_16x16_mmx2);
MK_CFUNC(x264_pixel_satd_16x8_mmx2);
MK_CFUNC(x264_pixel_satd_8x16_mmx2);
MK_CFUNC(x264_pixel_satd_8x8_mmx2);
MK_CFUNC(x264_pixel_satd_8x4_mmx2);
MK_CFUNC(x264_pixel_satd_4x8_mmx2);
MK_CFUNC(x264_pixel_satd_4x4_mmx2);

MK_CFUNC(x264_pixel_satd_32x32_mmx2);
MK_CFUNC(x264_pixel_satd_32x16_mmx2);

#define SATD_SSE(blsizex, blsizey, type) extern "C" unsigned int __cdecl x264_pixel_satd_##blsizex##x##blsizey##_##type##(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch)

//x264_pixel_satd_16x16_%1
SATD_SSE(16, 16, sse2);
SATD_SSE(16,  8, sse2);
SATD_SSE( 8, 16, sse2);
SATD_SSE( 8,  8, sse2);
SATD_SSE( 8,  4, sse2);
SATD_SSE(16, 16, ssse3);
SATD_SSE(16,  8, ssse3);
SATD_SSE( 8, 16, ssse3);
SATD_SSE( 8,  8, ssse3);
SATD_SSE( 8,  4, ssse3);
#if 0
SATD_SSE(16, 16, ssse3_phadd); //identical to ssse3, for Penryn useful only?
SATD_SSE(16,  8, ssse3_phadd); //identical to ssse3
SATD_SSE( 8, 16, ssse3_phadd);
SATD_SSE( 8,  8, ssse3_phadd);
SATD_SSE( 8,  4, ssse3_phadd);
#endif

SATD_SSE(32, 32, sse2);
SATD_SSE(32, 16, sse2);
SATD_SSE(32, 32, ssse3);
SATD_SSE(32, 16, ssse3);
SATD_SSE(32, 32, ssse3_phadd);
SATD_SSE(32, 16, ssse3_phadd);
#undef SATD_SSE

//dummy for testing and deactivate SAD
MK_CFUNC(SadDummy);
#undef MK_CFUNC


#endif
