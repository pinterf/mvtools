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
#include <vector>
#include "avs/minmax.h"
#include <cassert>
#ifdef INTEL_INTRINSICS
#include <emmintrin.h>
#include <smmintrin.h>
#endif
#include "BlockSizes.h"

#ifndef min
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif

#ifndef max
#define max(a,b)            (((a) < (b)) ? (b) : (a))
#endif


//--------------------------------------------------------------------
/*
                            <----oh--->      <---oh---->       <---oh--->

                            <------------bh------------>       <------------bh------------>
           <------------bh------------>      <------------bh------------>

       ^   +--------------------------+      +--------------------------+
       |   |                +---------|----------------+       +--------------------------+
       |   |                |         |      |         |       |        |                 |
       |   |                |         |      |         |       |        |                 |
       |   |                |         |      |         |       |        |                 |
       |   |                |         |      |         |       |        |                 |
       bv  |                |         |      |         |       |        |                 |
       |   |                |         |      |         |       |        |                 |
 ^  ^  |   |+--------------------------+     |+--------------------------+                |
 |  |  |   ||               |+---------|----------------+      |+--------------------------+
 ov |  |   ||               ||        ||     ||        ||      ||       ||                ||
 |  |  |   ||               ||        ||     ||        ||      ||       ||                ||
 |  |  |   ||               ||        ||     ||        ||      ||       ||                ||
 v  |  v   +|----------------|--------+|     +|--------||------||-------+|                ||
    |       |               +|---------|------|--------+|      +|--------|----------------+|
    bv      |                |         |      |         |       |        |                 |
 ^  |      +--------------------------+|     +--------------------------+|                 |
 |  |  ^   ||               +---------|----------------+|      +--------------------------+|
 ov |  |   ||               ||        ||     ||        ||      ||       ||                ||
 |  |  |   ||               ||        ||     ||        ||      ||       ||                ||
 |  |  |   ||               ||        ||     ||        ||      ||       ||                ||
 v  v  |   |+---------------|---------|+     |+--------||------||-------|+                ||
       |   |                |+--------|------|---------|+      |+-------|-----------------|+
       |   |                |         |      |         |       |        |                 |
 ^  ^  bv  |+--------------------------+     |+--------------------------+                |
 |  |  |   ||               |+---------|----------------+      |+--------------------------+
 ov |  |   ||               ||        ||     ||        ||      ||       ||                ||
 |  |  |   ||               ||        ||     ||        ||      ||       ||                ||
 |  |  |   ||               ||        ||     ||        ||      ||       ||                ||
 v  |  |   +|----------------|--------+|     +|--------||------||-------+|                ||
    bv v    |               +|---------|------|--------+|      +|--------|----------------+|
    |       |                |         |      |         |       |        |                 |
    |       |                |         |      |         |       |        |                 |
    |       |                |         |      |         |       |        |                 |
    |       |                |         |      |         |       |        |                 |
    |       |                |         |      |         |       |        |                 |
    |       |                |         |      |         |       |        |                 |
    v       +--------------------------+      +---------|-------|--------+                 |
                             +--------------------------+       +--------------------------+

                  bh=blocksize horizontal    bv=blocksize vertical
                  oh=overlap horizntal       ov=overlap vertical

*/
// Overlap Windows class
// nx: blocksize horizontal
// ny: blocksize vertical
// ox: overlap horizontal
// oy: overlap vertical
OverlapWindows::OverlapWindows(int _nx, int _ny, int _ox, int _oy)
{
  nx = _nx;
  ny = _ny;
  ox = _ox;
  oy = _oy;
  size = nx*ny;

     // 1D weights of the overlapping left (top) zone: rising cos^2, 0..1.
     // The right (bottom) zone of a block is the complement of the next block's left (top) zone.
     // FFFFFFFF                LLLLLLLL   F=First block, L=Last block
     //       ========    ========
     // ========    ========    ========
     //       AABBBBCC                     A:0..ox-1 ; B:ox..nx-ox-1 ; C:nx-ox..nx-1
     // For BlkSize=8, Overlap=2           A:0..1  B:2..5  C:6..7
     std::vector<float> wx(ox), wy(oy);
     for (int i = 0; i < ox; i++) {
       wx[i] = float(cos(PI * (i - ox + 0.5f) / (ox * 2)));
       wx[i] = wx[i] * wx[i];
     }
     for (int i = 0; i < oy; i++) {
       wy[i] = float(cos(PI * (i - oy + 0.5f) / (oy * 2)));
       wy[i] = wy[i] * wy[i];
     }

    Overlap9Windows = new short[size*9];

    // short
    {
      short* winOverUVTL = Overlap9Windows;
      short* winOverUVTM = Overlap9Windows + size;
      short* winOverUVTR = Overlap9Windows + size * 2;
      short* winOverUVML = Overlap9Windows + size * 3;
      short* winOverUVMM = Overlap9Windows + size * 4;
      short* winOverUVMR = Overlap9Windows + size * 5;
      short* winOverUVBL = Overlap9Windows + size * 6;
      short* winOverUVBM = Overlap9Windows + size * 7;
      short* winOverUVBR = Overlap9Windows + size * 8;

      // Cos^2 based weigthing factors with 11 bits precision. (0-2048)
      // SourceBlocks overlap sample for Blocksize=4 Overlaps=2:
      // a1     a1   | a1+b1         a1+b1       | b1      b1    | b1+c1         b1+c1       | c1       c1   
      // a1     a1   | a1+b1         a1+b1       | b1      b1    | b1+c1         b1+c1       | c1      c1   
      // ------------+---------------------------+---------------+---------------------------+--------------
      // a1+a2  a1+a2| a1+a2+b1+b2   a1+a2+b1+b2 | b1+b2   b1+b2 | b1+b2+c1+c2   b1+b2+c1+c2 | c1+c2   c1+c2
      // a1+a2  a1+a2| a1+a2+b1+b2   a1+a2+b1+b2 | b1+b2   b1+b2 | b1+b2+c1+c2   b1+b2+c1+c2 | c1+c2   c1+c2
      // ------------+---------------------------+---------------+---------------------------+--------------
      // a2+a3  a2+a3| a2+a3+b2+b3   a2+a3+b2+b3 | b2+b3   b2+b3 | b2+b3+c2+c3   b2+b3+c2+c3 | c2+c3   c2+c3
      // a2+a3  a2+a3| a2+a3+b2+b3   a2+a3+b2+b3 | b2+b3   b2+b3 | b2+b3+c2+c3   b2+b3+c2+c3 | c2+c3   c2+c3
      // ...

      // Integer weights. Where blocks overlap, the weights of the 2 or 4 blocks covering a pixel must sum to exactly 2048,
      // otherwise a flat area changes (at 16 bit up to +-32, 2047..2049 sum when the 2D products were rounded one by one).
      // Rounded once per axis: Ix (left zone), Iy (top zone), the partner block gets the complement (2048 - Ix);
      // in the corners one product P is rounded, the other three are derived from it.
      // Same idea as the differential coefficients of the Avisynth+ resizers (sum of rounded differences), in 2D.
      // Zones per axis: 0: no overlap (weight 1), 1: left/top zone (rising), 2: right/bottom zone (falling)
      std::vector<int> Ix(ox), Iy(oy);
      for (int k = 0; k < ox; k++) Ix[k] = (int)(wx[k] * 2048 + 0.5f);
      for (int m = 0; m < oy; m++) Iy[m] = (int)(wy[m] * 2048 + 0.5f);
      // first: no overlap on the left/top side, last: no overlap on the right/bottom side
      auto zone = [](int i, int n, int o, bool first, bool last, int &k) {
        if (i < o) { k = i; return first ? 0 : 1; }
        if (i >= n - o) { k = i - (n - o); return last ? 0 : 2; }
        k = 0; return 0;
      };
      auto weight = [&](int i, int j, bool xfirst, bool xlast, bool yfirst, bool ylast) -> short {
        int k, m;
        const int zx = zone(i, nx, ox, xfirst, xlast, k);
        const int zy = zone(j, ny, oy, yfirst, ylast, m);
        if (zy == 0) return (short)(zx == 0 ? 2048 : zx == 1 ? Ix[k] : 2048 - Ix[k]);
        if (zx == 0) return (short)(zy == 1 ? Iy[m] : 2048 - Iy[m]);
        // left-top; limited so that none of the four weights is negative (SIMD overlaps read the window as unsigned)
        const int P = max(max(0, Ix[k] + Iy[m] - 2048), min(min(Ix[k], Iy[m]), (int)(wx[k] * wy[m] * 2048 + 0.5f)));
        if (zx == 1 && zy == 1) return (short)P;
        if (zx == 2 && zy == 1) return (short)(Iy[m] - P);
        if (zx == 1 && zy == 2) return (short)(Ix[k] - P);
        return (short)(2048 - Ix[k] - Iy[m] + P); // right-bottom
      };

      // 3x3 special areas: T/M/B: top, middle, bottom block row; L/M/R: left, middle, right block column
      for (int j = 0; j < ny; j++)
      {
        for (int i = 0; i < nx; i++)
        {
          winOverUVTL[i] = weight(i, j, true, false, true, false);   // Top Left
          winOverUVTM[i] = weight(i, j, false, false, true, false);  // Top Middle
          winOverUVTR[i] = weight(i, j, false, true, true, false);   // Top Right
          winOverUVML[i] = weight(i, j, true, false, false, false);  // Middle Left
          winOverUVMM[i] = weight(i, j, false, false, false, false); // Middle Middle
          winOverUVMR[i] = weight(i, j, false, true, false, false);  // Middle Right
          winOverUVBL[i] = weight(i, j, true, false, false, true);   // Bottom Left
          winOverUVBM[i] = weight(i, j, false, false, false, true);  // Bottom Middle
          winOverUVBR[i] = weight(i, j, false, true, false, true);   // Bottom Right
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
    } // end of short coeffs


}

OverlapWindows::~OverlapWindows()
{
  delete[] Overlap9Windows;
}

// v2.7.25 Let's round 1<<4
// Earlier there is >>6 with an originally +256 rounding during the calculation of an overlapped block
// The +256 rounding is not fine, though the asm code also contained a +256 rounding, so there must be a reason somewhere.
// Here originally there was no rounding at all
// So instead of: Sum((overlapped + 256)>>6) >> 5
// So we do: (Sum((overlapped + 32) >> 6) + 16) >> 5
#ifdef INTEL_INTRINSICS
void Short2Bytes_sse2(unsigned char *pDst, int nDstPitch, uint16_t *pDstShort, int dstShortPitch, int nWidth, int nHeight)
{
  // v.2.7.25-: round 16 here, round 32 earlier. See comments in C
  const int rounder_i = 1 << 4;
  auto rounder = _mm_set1_epi16(rounder_i);
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
      // total shift is 11: 6+5. Shift 6 is already done, we shift the rest 5. See 6+5
      // round, shift and limit
      __m128i res07 = _mm_srai_epi16(_mm_add_epi16(src07, rounder), 5);
      __m128i res8f = _mm_srai_epi16(_mm_add_epi16(src8f, rounder), 5);
      __m128i res = _mm_packus_epi16(res07, res8f);
      _mm_store_si128((__m128i *)(pDst8 + x), res);
    }
    if (wMod8 != wMod16) {
      __m128i src07 = _mm_loadu_si128((__m128i *)(pSrc8 + wMod16 * 2)); // 8 short pixels
      __m128i res07 = _mm_srai_epi16(_mm_add_epi16(src07, rounder), 5); // shift, round and limit
      __m128i res = _mm_packus_epi16(res07, res07);
      _mm_storel_epi64((__m128i *)(pDst8 + wMod16), res);
    }
    for (int x = wMod8; x < nWidth; x++) {
      int a = (reinterpret_cast<uint16_t *>(pSrc8)[x] + rounder_i) >> 5;
      pDst8[x] = min(255, a);
    }
    pDst8 += nDstPitch;
    pSrc8 += nSrcPitch;
  }
}
#endif // INTEL_INTRINSICS

void Short2Bytes(unsigned char *pDst, int nDstPitch, uint16_t *pDstShort, int dstShortPitch, int nWidth, int nHeight)
{
  for (int h=0; h<nHeight; h++)
  {
    for (int i=0; i<nWidth; i++)
    {
      int a = (pDstShort[i] + (1<<4)) >> 5; // pWin was of scale 11 bits, >> 6 was already done in overlaps
      pDst[i] = min(255, a);
    }
    pDst += nDstPitch;
    pDstShort += dstShortPitch;
  }
}

void Short2BytesLsb(unsigned char *pDst, unsigned char *pDstLsb, int nDstPitch, int *pDstInt, int dstIntPitch, int nWidth, int nHeight)
{
  for (int h = 0; h < nHeight; h++)
  {
    for (int i = 0; i < nWidth; i++)
    {
      // origin: overlap windows have 11 bits precision
      const int		a = (pDstInt[i] + (1 << 10)) >> 11; // v2.7.25 round
      pDst[i] = a >> 8;
      pDstLsb[i] = (unsigned char)(a);
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
      // origin: overlap windows have 11 bits precision
      const int		a = (pDstInt [i] + (1 << 10)) >> 11; // v2.7.25 round
      pDst [i] = min(a, max_pixel_value);
    }
    pDst += nDstPitch/sizeof(uint16_t);
    pDstInt += dstIntPitch; // this pitch is int granularity
  }
}

#ifdef INTEL_INTRINSICS
void Short2Bytes_Int32toWord16_sse4(uint16_t *pDst, int nDstPitch, int *pDstInt, int dstIntPitch, int nWidth, int nHeight, int bits_per_pixel)
{
  const int max_pixel_value = (1 << bits_per_pixel) - 1;

  __m128i limits16;
  limits16 = _mm_set1_epi16(max_pixel_value);
  // origin: overlap windows have 11 bits precision
  auto rounder = _mm_set1_epi32(1 << 10);

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
      src03 = _mm_add_epi32(src03, rounder);
      src47 = _mm_add_epi32(src47, rounder);
      __m128i res03 = _mm_srai_epi32(src03, 11); // shift and limit
      __m128i res47 = _mm_srai_epi32(src47, 11); // shift and limit
      __m128i res = _mm_packus_epi32(res03, res47); // sse4 int->uint16_t, already limiting to 65535
      res = _mm_min_epu16(res, limits16); // 10,12,14 bits can be lesser
      _mm_store_si128((__m128i *)(pDst8 + x), res);
    }
    pDst8 += nDstPitch;
    pSrc8 += nSrcPitch;
  }
}
#endif // INTEL_INTRINSICS

void Short2Bytes_FloatInInt32ArrayToFloat(float *pDst, int nDstPitch, float *pDstInt, int dstIntPitch, int nWidth, int nHeight)
{
  // fixme: this is simple BitBlt
  for (int h=0; h<nHeight; h++)
  {
    for (int i=0; i<nWidth; i++)
    {
      pDst[i] = pDstInt[i]; // no clamp! min(pDstIntF[i], 1.0f);
    }
    pDst += nDstPitch/sizeof(float);
    pDstInt += dstIntPitch; // this pitch is int/float (4 byte) granularity
  }
}

#ifdef INTEL_INTRINSICS
MV_FORCEINLINE __m128i _MM_CMPLE_EPU16(__m128i x, __m128i y)
{
  // Returns 0xFFFF where x <= y:
  return _mm_cmpeq_epi16(_mm_subs_epu16(x, y), _mm_setzero_si128());
}

MV_FORCEINLINE __m128i _MM_BLENDV_SI128(__m128i x, __m128i y, __m128i mask)
{
  // Replace bit in x with bit in y when matching bit in mask is set:
  return _mm_or_si128(_mm_andnot_si128(mask, x), _mm_and_si128(mask, y));
}

// sse2 simulation of SSE4's _mm_min_epu16
MV_FORCEINLINE __m128i _MM_MIN_EPU16(__m128i x, __m128i y)
{
  // Returns x where x <= y, else y:
  return _MM_BLENDV_SI128(y, x, _MM_CMPLE_EPU16(x, y));
}

// sse2 simulation of SSE4's _mm_max_epu16
MV_FORCEINLINE __m128i _MM_MAX_EPU16(__m128i x, __m128i y)
{
  // Returns x where x >= y, else y:
  return _MM_BLENDV_SI128(x, y, _MM_CMPLE_EPU16(x, y));
}

// MVDegrains-a.asm ported to intrinsics + 16 bit PF 161002
template<typename pixel_t, bool hasSSE41>
void LimitChanges_sse2_new(unsigned char *pDst8, int nDstPitch, const unsigned char *pSrc8, int nSrcPitch, const int nWidth, int nHeight, float nLimit_f)
{
  const int nLimit = (int)(nLimit_f + 0.5f);
  __m128i limits;
  if constexpr(sizeof(pixel_t) == 1)
    limits = _mm_set1_epi8(nLimit);
  else
    limits = _mm_set1_epi16(nLimit);

  const int stride = nWidth * sizeof(pixel_t); // back to byte size

  for (int y = 0; y < nHeight; y++)
  {
    for (int x = 0; x < stride; x += 16) {
      __m128i src = _mm_load_si128((__m128i *)(pSrc8 + x));
      __m128i dst = _mm_load_si128((__m128i *)(pDst8 + x));
      __m128i res;
      if constexpr(sizeof(pixel_t) == 1) {
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
#endif // INTEL_INTRINSICS

// out16=true case
void LimitChanges_src8_target16_c(unsigned char *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, int nWidth, int nHeight, float nLimit_f)
{
  const int nLimit = (int)(nLimit_f + 0.5f);
  for (int h = 0; h<nHeight; h++)
  {
    // compare 16bit target with 8 bit source
    for (int i = 0; i < nWidth; i++)
      reinterpret_cast<uint16_t *>(pDst)[i] =
      (uint16_t)clamp((int)reinterpret_cast<uint16_t *>(pDst)[i], (pSrc[i] << 8) - nLimit, (pSrc[i] << 8) + nLimit);
    pDst += nDstPitch;
    pSrc += nSrcPitch;
  }
}

template<typename pixel_t>
void LimitChanges_c(unsigned char *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, int nWidth, int nHeight, float nLimit_f)
{
  const int nLimit = (int)(nLimit_f + 0.5f);
  for (int h=0; h<nHeight; h++)
  {
    for (int i=0; i<nWidth; i++)
      reinterpret_cast<pixel_t *>(pDst)[i] = 
        (pixel_t)clamp((int)reinterpret_cast<pixel_t *>(pDst)[i], (reinterpret_cast<const pixel_t *>(pSrc)[i]-nLimit), (reinterpret_cast<const pixel_t *>(pSrc)[i]+nLimit));
    pDst += nDstPitch;
    pSrc += nSrcPitch;
  }
}

void LimitChanges_float_c(unsigned char *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, int nWidth, int nHeight, float nLimit_f)
{
  for (int h=0; h<nHeight; h++)
  {
    for (int i=0; i<nWidth; i++)
      reinterpret_cast<float *>(pDst)[i] = 
        clamp(reinterpret_cast<float *>(pDst)[i], (reinterpret_cast<const float *>(pSrc)[i]-nLimit_f), (reinterpret_cast<const float *>(pSrc)[i]+nLimit_f));
    pDst += nDstPitch;
    pSrc += nSrcPitch;
  }
}

// if a non-static template function is in cpp, we have to instantiate it
template void LimitChanges_c<uint8_t>(unsigned char *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, int nWidth, int nHeight, float nLimit);
template void LimitChanges_c<uint16_t>(unsigned char *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, int nWidth, int nHeight, float nLimit);
#ifdef INTEL_INTRINSICS
template void LimitChanges_sse2_new<uint8_t,0>(unsigned char *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, int nWidth, int nHeight, float nLimit);
template void LimitChanges_sse2_new<uint16_t,0>(unsigned char *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, int nWidth, int nHeight, float nLimit);
template void LimitChanges_sse2_new<uint16_t,1>(unsigned char *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, int nWidth, int nHeight, float nLimit);
#endif // INTEL_INTRINSICS

#ifdef INTEL_INTRINSICS
template <int blockWidth, int blockHeight>
// pDst is short* for 8 bit, int * for 16 bit
// works for blockWidth >= 4 && uint16_t
// only src_pitch is byte-level
// dstPitch knows int* or short* 
// winPitch knows short *
void Overlaps_uint16_t_sse4(uint16_t *pDst0, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch)
{
  // pWin from 0 to 2048, 11 bit integer arithmetic
  // when pixel_t == uint16_t, dst should be int*
  int *pDst = reinterpret_cast<int *>(pDst0);
  __m128i zero = _mm_setzero_si128();

  for (int j = 0; j < blockHeight; j++)
  {
    __m128i dst;
    __m128i win, src;

    if constexpr (blockWidth == 4) // half of 1x16 byte
    {
      win = _mm_loadl_epi64(reinterpret_cast<__m128i*>(pWin)); // 4x16 short: Window
      src = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(pSrc)); // 4x16 uint16_t: source pixels

      __m128i reslo = _mm_mullo_epi32(_mm_cvtepu16_epi32(src), _mm_cvtepu16_epi32(win)); // sse4 unpacklo
      dst = _mm_loadu_si128(reinterpret_cast<__m128i*>(pDst)); // 4x32 int: destination pixels
      dst = _mm_add_epi32(dst, reslo);
      _mm_storeu_si128(reinterpret_cast<__m128i*>(pDst), dst);

    }
    else if constexpr (blockWidth == 8) // exact 1x16 byte
    {
      win = _mm_loadu_si128(reinterpret_cast<__m128i*>(pWin)); // 8x16 short: Window
      src = _mm_loadu_si128(reinterpret_cast<const __m128i*>(pSrc)); // 8x16 uint16_t: source pixels

      __m128i reslo = _mm_mullo_epi32(_mm_cvtepu16_epi32(src), _mm_cvtepu16_epi32(win)); // sse4 unpacklo
      dst = _mm_loadu_si128(reinterpret_cast<__m128i*>(pDst)); // 4x32 int: destination pixels
      dst = _mm_add_epi32(dst, reslo);
      _mm_storeu_si128(reinterpret_cast<__m128i*>(pDst), dst);

      __m128i reshi = _mm_mullo_epi32(_mm_unpackhi_epi16(src, zero), _mm_unpackhi_epi16(win, zero));
      dst = _mm_loadu_si128(reinterpret_cast<__m128i*>(pDst + 4)); // next 4x32 int: destination pixels
      dst = _mm_add_epi32(dst, reshi);
      _mm_storeu_si128(reinterpret_cast<__m128i*>(pDst + 4), dst);
    }
    else if constexpr (blockWidth == 16) // 2x16 byte: 2x8 pixels
    {
      win = _mm_loadu_si128(reinterpret_cast<__m128i*>(pWin)); // 8x16 short: Window
      src = _mm_loadu_si128(reinterpret_cast<const __m128i*>(pSrc)); // 8x16 uint16_t: source pixels

      __m128i reslo = _mm_mullo_epi32(_mm_cvtepu16_epi32(src), _mm_cvtepu16_epi32(win)); // sse4 unpacklo
      dst = _mm_loadu_si128(reinterpret_cast<__m128i*>(pDst)); // 4x32 int: destination pixels
      dst = _mm_add_epi32(dst, reslo);
      _mm_storeu_si128(reinterpret_cast<__m128i*>(pDst), dst);

      __m128i reshi = _mm_mullo_epi32(_mm_unpackhi_epi16(src, zero), _mm_unpackhi_epi16(win, zero));
      dst = _mm_loadu_si128(reinterpret_cast<const __m128i*>(pDst + 4)); // next 4x32 int: destination pixels
      dst = _mm_add_epi32(dst, reshi);
      _mm_storeu_si128(reinterpret_cast<__m128i*>(pDst + 4), dst);

      win = _mm_loadu_si128(reinterpret_cast<__m128i*>(pWin + 8)); // next 8x16 short: Window
      src = _mm_loadu_si128(reinterpret_cast<const __m128i*>(pSrc + 16)); // next 8x16 uint16_t: source pixels

      // once again
      reslo = _mm_mullo_epi32(_mm_cvtepu16_epi32(src), _mm_cvtepu16_epi32(win)); // sse4 unpacklo
      dst = _mm_loadu_si128(reinterpret_cast<__m128i*>(pDst + 8)); // 4x32 int: destination pixels
      dst = _mm_add_epi32(dst, reslo);
      _mm_storeu_si128(reinterpret_cast<__m128i*>(pDst + 8), dst);

      reshi = _mm_mullo_epi32(_mm_unpackhi_epi16(src, zero), _mm_unpackhi_epi16(win, zero));
      dst = _mm_loadu_si128(reinterpret_cast<__m128i*>(pDst + 8 + 4)); // next 4x32 int: destination pixels
      dst = _mm_add_epi32(dst, reshi);
      _mm_storeu_si128(reinterpret_cast<__m128i*>(pDst + 8 + 4), dst);
    }
    else if constexpr (blockWidth % 8 == 0) {
      // mod8 block width for uint16
      for (int x = 0; x < blockWidth; x += 8) {
        win = _mm_loadu_si128(reinterpret_cast<__m128i*>(pWin + x)); // 8x16 short: Window
        src = _mm_loadu_si128(reinterpret_cast<const __m128i*>(pSrc + x * 2)); // 8x16 uint16_t: source pixels

        __m128i reslo = _mm_mullo_epi32(_mm_cvtepu16_epi32(src), _mm_cvtepu16_epi32(win)); // sse4 unpacklo
        dst = _mm_loadu_si128(reinterpret_cast<__m128i*>(pDst + x)); // 4x32 int: destination pixels
        dst = _mm_add_epi32(dst, reslo);
        _mm_storeu_si128(reinterpret_cast<__m128i*>(pDst + x), dst);

        __m128i reshi = _mm_mullo_epi32(_mm_unpackhi_epi16(src, zero), _mm_unpackhi_epi16(win, zero));
        dst = _mm_loadu_si128(reinterpret_cast<__m128i*>(pDst + x + 4)); // next 4x32 int: destination pixels
        dst = _mm_add_epi32(dst, reshi);
        _mm_storeu_si128(reinterpret_cast<__m128i*>(pDst + x + 4), dst);
      }
    }
    else if constexpr (blockWidth % 4 == 0) {
      // mod4 block width for uint16. Exact 4 was handled specially, this covers only 12 from valid set
      for (int x = 0; x < blockWidth; x += 4) {
        win = _mm_loadl_epi64(reinterpret_cast<__m128i*>(pWin + x)); // 4x16 short: Window
        src = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(pSrc + x * 2)); // 4x16 uint16_t: source pixels

        __m128i reslo = _mm_mullo_epi32(_mm_cvtepu16_epi32(src), _mm_cvtepu16_epi32(win)); // sse4 unpacklo
        dst = _mm_loadu_si128(reinterpret_cast<__m128i*>(pDst + x)); // 4x32 int: destination pixels
        dst = _mm_add_epi32(dst, reslo);
        _mm_storeu_si128(reinterpret_cast<__m128i*>(pDst + x), dst);
      }
    }
    else if constexpr (blockWidth % 2 == 0) {
      // mod2 block width for uint16. 2, (4), 6
      for (int x = 0; x < blockWidth; x += 2) {
        win = _mm_cvtsi32_si128(*reinterpret_cast<const uint32_t*>(pWin + x)); // 2x16 short: Window
        src = _mm_cvtsi32_si128(*reinterpret_cast<const uint32_t*>(pSrc + x * 2)); // 2x16 uint16_t: source pixels

        __m128i reslo = _mm_mullo_epi32(_mm_cvtepu16_epi32(src), _mm_cvtepu16_epi32(win)); // sse4 unpacklo
        dst = _mm_loadl_epi64(reinterpret_cast<__m128i*>(pDst + x)); // 2x32 int: destination pixels
        dst = _mm_add_epi32(dst, reslo);
        _mm_storel_epi64(reinterpret_cast<__m128i*>(pDst + x), dst);
      }
    }
    else {
      assert(0);
    }

    // pDst[i] = ( pDst[i] + ((reinterpret_cast<const pixel_t *>(pSrc)[i]*pWin[i])));

    pDst += nDstPitch;
    pSrc += nSrcPitch;
    pWin += nWinPitch;
  }
}

template <int blockWidth, int blockHeight>
// pDst is short* for 8 bit, int * for 16 bit
// works for blockWidth >= 4 && uint16_t
// only src_pitch is byte-level
// dstPitch knows int* or short* 
// winPitch knows short *
void Overlaps_uint8_t_sse4(uint16_t* pDst0, int nDstPitch, const unsigned char* pSrc, int nSrcPitch, short* pWin, int nWinPitch)
{
  // pWin from 0 to 2048, 11 bit integer arithmetic
  // when pixel_t == uint16_t, dst should be int*
  uint16_t* pDst = pDst0;
  __m128i zero = _mm_setzero_si128();

  __m128i rounder_32 = _mm_set1_epi32(32); // 1 << 5 rounder

  for (int j = 0; j < blockHeight; j++)
  {
    __m128i dst;
    __m128i win, src;

    // pDst[i] = pDst[i] + ((val * win + (1 << 5)) >> 6);

    if constexpr (blockWidth % 8 == 0) {
      // 16, 24, 32, 48, 64
      for (int x = 0; x < blockWidth; x += 8) {
        // 2nd 8
        win = _mm_loadu_si128(reinterpret_cast<__m128i*>(pWin + x)); // 8x short: Window - always positives
        src = _mm_cvtepu8_epi16(_mm_loadl_epi64(reinterpret_cast<const __m128i*>(pSrc + x))); // 8x uint8_t: source pixels
#if 0
        auto res32lo = _mm_mullo_epi32(_mm_cvtepu16_epi32(src), _mm_cvtepu16_epi32(win));
        auto res32hi = _mm_mullo_epi32(_mm_cvtepu16_epi32(_mm_srli_si128(src, 8)), _mm_cvtepu16_epi32(_mm_srli_si128(win, 8)));
#else
        auto reslo = _mm_mullo_epi16(src, win); // lower 16 bit of result
        auto reshi = _mm_mulhi_epi16(src, win); // upper 32 bit of result
        auto res32lo = _mm_unpacklo_epi16(reslo, reshi);
        auto res32hi = _mm_unpackhi_epi16(reslo, reshi);
#endif
        res32lo = _mm_srli_epi32(_mm_add_epi32(res32lo, rounder_32), 6);
        res32hi = _mm_srli_epi32(_mm_add_epi32(res32hi, rounder_32), 6);
        auto res = _mm_packs_epi32(res32lo, res32hi);
        dst = _mm_loadu_si128(reinterpret_cast<__m128i*>(pDst + x)); // 8x short: destination pixels
        dst = _mm_add_epi16(dst, res);
        _mm_storeu_si128(reinterpret_cast<__m128i*>(pDst + x), dst);
      }
    }
    else if constexpr (blockWidth % 4 == 0) {
      // width==4,(8),12
      for (int x = 0; x < blockWidth; x += 4) {
        win = _mm_loadl_epi64(reinterpret_cast<__m128i*>(pWin + x)); // 4x short: Window
        src = _mm_cvtepu8_epi16(_mm_cvtsi32_si128(*reinterpret_cast<const uint32_t*>(pSrc + x))); // 4x uint8_t: source pixels
#if 0
        auto  res32lo = _mm_mullo_epi32(_mm_cvtepu16_epi32(src), _mm_cvtepu16_epi32(win));
#else
        auto reslo = _mm_mullo_epi16(src, win); // lower 16 bit of result
        auto reshi = _mm_mulhi_epi16(src, win); // upper 32 bit of result
        auto res32lo = _mm_unpacklo_epi16(reslo, reshi);
#endif
        res32lo = _mm_srli_epi32(_mm_add_epi32(res32lo, rounder_32), 6);
        auto res = _mm_packs_epi32(res32lo, res32lo);
        dst = _mm_loadl_epi64(reinterpret_cast<__m128i*>(pDst + x)); // 4x short: destination pixels
        dst = _mm_add_epi16(dst, res);
        _mm_storel_epi64(reinterpret_cast<__m128i*>(pDst + x), dst);
      }
    }
    else if constexpr (blockWidth == 6) {
      // 1st 4
      win = _mm_loadl_epi64(reinterpret_cast<__m128i*>(pWin)); // 4x short: Window
      src = _mm_cvtepu8_epi16(_mm_cvtsi32_si128(*reinterpret_cast<const uint32_t*>(pSrc))); // 4x uint8_t: source pixels
#if 0
      auto  res32lo = _mm_mullo_epi32(_mm_cvtepu16_epi32(src), _mm_cvtepu16_epi32(win));
#else
      auto reslo = _mm_mullo_epi16(src, win); // lower 16 bit of result
      auto reshi = _mm_mulhi_epi16(src, win); // upper 32 bit of result
      auto res32lo = _mm_unpacklo_epi16(reslo, reshi);
#endif
      res32lo = _mm_srli_epi32(_mm_add_epi32(res32lo, rounder_32), 6);
      auto res = _mm_packs_epi32(res32lo, res32lo);
      dst = _mm_loadl_epi64(reinterpret_cast<__m128i*>(pDst)); // 4x short: destination pixels
      dst = _mm_add_epi16(dst, res);
      _mm_storel_epi64(reinterpret_cast<__m128i*>(pDst), dst);

      // last 2 pixels
      win = _mm_cvtsi32_si128(*reinterpret_cast<const uint32_t*>(pWin + 4)); // 2x short: Window
      src = _mm_cvtepu8_epi16(_mm_cvtsi32_si128(*reinterpret_cast<const uint16_t*>(pSrc + 4))); // 2x uint8_t: source pixels
#if 0
      res32lo = _mm_mullo_epi32(_mm_cvtepu16_epi32(src), _mm_cvtepu16_epi32(win));
#else
      reslo = _mm_mullo_epi16(src, win); // lower 16 bit of result
      reshi = _mm_mulhi_epi16(src, win); // upper 32 bit of result
      res32lo = _mm_unpacklo_epi16(reslo, reshi);
#endif
      res32lo = _mm_srli_epi32(_mm_add_epi32(res32lo, rounder_32), 6);
      res = _mm_packs_epi32(res32lo, res32lo);
      dst = _mm_cvtsi32_si128(*reinterpret_cast<uint32_t*>(pDst + 4)); // 2x short: destination pixels
      dst = _mm_add_epi16(dst, res);
      *reinterpret_cast<uint32_t*>(pDst+4) = _mm_cvtsi128_si32(dst);
    }
    else if constexpr (blockWidth == 2) {
      // 2 pixels
      win = _mm_cvtsi32_si128(*reinterpret_cast<const uint32_t*>(pWin)); // 2x short: Window
      src = _mm_cvtepu8_epi16(_mm_cvtsi32_si128(*reinterpret_cast<const uint16_t*>(pSrc))); // 2x uint8_t: source pixels
#if 0
      auto  res32lo = _mm_mullo_epi32(_mm_cvtepu16_epi32(src), _mm_cvtepu16_epi32(win));
#else
      auto reslo = _mm_mullo_epi16(src, win); // lower 16 bit of result
      auto reshi = _mm_mulhi_epi16(src, win); // upper 32 bit of result
      auto res32lo = _mm_unpacklo_epi16(reslo, reshi);
#endif
      res32lo = _mm_srli_epi32(_mm_add_epi32(res32lo, rounder_32), 6);
      auto res = _mm_packs_epi32(res32lo, res32lo);
      dst = _mm_cvtsi32_si128(*reinterpret_cast<uint32_t*>(pDst)); // 2x short: destination pixels
      dst = _mm_add_epi16(dst, res);
      *reinterpret_cast<uint32_t*>(pDst) = _mm_cvtsi128_si32(dst);
    }
    else {
      assert(0);
    }

    pDst += nDstPitch;
    pSrc += nSrcPitch;
    pWin += nWinPitch;
  }
}
#endif // INTEL_INTRINSICS

OverlapsFunction *get_overlaps_function(int BlockX, int BlockY, int pixelsize, arch_t arch)
{
    //---------- OVERLAPS
    // BlkSizeX, BlkSizeY, pixelsize, arch_t
    std::map<std::tuple<int, int, int, arch_t>, OverlapsFunction*> func_overlaps;
    using std::make_tuple;
    // define C for 8/16/32 bits
#define MAKE_OVR_FN(x, y) \
func_overlaps[make_tuple(x, y, 1, NO_SIMD)] = Overlaps_C<uint8_t, x, y>; \
func_overlaps[make_tuple(x, y, 2, NO_SIMD)] = Overlaps_C<uint16_t, x, y>; \
func_overlaps[make_tuple(x, y, 4, NO_SIMD)] = Overlaps_float_C<x, y>;
    MVTOOLS_BLOCKSIZES(MAKE_OVR_FN) // BlockSizes.h: the same list for all C tables
#undef MAKE_OVR_FN

#ifdef USE_OVERLAPS_ASM
    // in overlap-a.asm
    func_overlaps[make_tuple(64, 64, 1, USE_SSE2)] = Overlaps64x64_sse2;
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
#endif
#ifdef INTEL_INTRINSICS
    // define sse4 for 16 bits
#ifdef USE_OVERLAPS_ASM
#define MAKE_OVR_FN(x, y) \
func_overlaps[make_tuple(x, y, 2, USE_SSE41)] = Overlaps_uint16_t_sse4<x, y>;
#else
    // plus: 8 bit SSE internally everybody has at least sse4
#define MAKE_OVR_FN(x, y) \
func_overlaps[make_tuple(x, y, 2, USE_SSE41)] = Overlaps_uint16_t_sse4<x, y>; \
func_overlaps[make_tuple(x, y, 1, USE_SSE41)] = Overlaps_uint8_t_sse4<x, y>;
#endif
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
      MAKE_OVR_FN(4, 16)
      MAKE_OVR_FN(4, 8)
      MAKE_OVR_FN(4, 4)
      MAKE_OVR_FN(4, 2)
      MAKE_OVR_FN(4, 1)
      MAKE_OVR_FN(2, 8)
      MAKE_OVR_FN(2, 4)
      MAKE_OVR_FN(2, 2)
      MAKE_OVR_FN(2, 1)
#undef MAKE_OVR_FN
#endif // INTEL_INTRINSICS

    OverlapsFunction *result = nullptr;

    arch_t archlist[] = { USE_NEON, USE_AVX2, USE_AVX, USE_SSE41, USE_SSE2, NO_SIMD };
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

// lsb: old stacked16 heritage
OverlapsLsbFunction *get_overlaps_lsb_function(int BlockX, int BlockY, int pixelsize, arch_t arch)
{
    // 8 bit only (pixelsize==1)
    // BlkSizeX, BlkSizeY, pixelsize, arch_t
    std::map<std::tuple<int, int, int, arch_t>, OverlapsLsbFunction*> func_overlaps_lsb;
    using std::make_tuple;
    // define C for 8 bits
#define MAKE_OVR_FN(x, y) func_overlaps_lsb[make_tuple(x, y, 1, NO_SIMD)] = OverlapsLsb_C<x, y>;
    MVTOOLS_BLOCKSIZES(MAKE_OVR_FN) // BlockSizes.h: the same list for all C tables
#undef MAKE_OVR_FN

    OverlapsLsbFunction *result = nullptr;
    arch_t archlist[] = { USE_NEON, USE_AVX2, USE_AVX, USE_SSE41, USE_SSE2, NO_SIMD };
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



