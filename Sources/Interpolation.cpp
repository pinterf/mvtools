// Functions that interpolates a frame
// Author: Manao
// Copyright(c)2006 A.G.Balakhnin aka Fizick - bicubic, Wiener, separable

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

#define NOGDI
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include "Interpolation.h"

#include <emmintrin.h>
#include <immintrin.h>
#include	<algorithm>
#include	<cassert>
#include <stdint.h>
#include "def.h"
#include "avs/cpuid.h"

/*
// doesn't work with referenced template type - go macro
template<typename pixel_t>
void RB2_jumpX(int y_new, int &y, pixel_t * &pDst, pixel_t * &pSrc, int nDstPitch, int nSrcPitch)
{
  const int		dif = y_new - y;
  pDst += nDstPitch / sizeof(pixel_t) * dif;
  pSrc += nSrcPitch / sizeof(pixel_t) * dif * 2;
  y     = y_new;
}
*/
#define RB2_jump(y_new, y, pDst, pSrc, nDstPitch, nSrcPitch) \
{ const int dif = y_new - y; \
  pDst += nDstPitch / sizeof(*pDst) * dif; \
  pSrc += nSrcPitch / sizeof(*pSrc) * dif * 2; \
  y = y_new; \
}

#define RB2_jump_1(y_new, y, pDst, nDstPitch) \
{ const int dif = y_new - y; \
  pDst += nDstPitch / sizeof(*pDst) * dif; \
  y = y_new; \
}

// 8-32bits
template<typename pixel_t>
void RB2F_C(
  pixel_t *pDst, const pixel_t *pSrc, int nDstPitch, int nSrcPitch,
  int nWidth, int nHeight, int y_beg, int y_end)
{
  assert(y_beg >= 0);
  assert(y_end <= nHeight);

  int				y = 0;
  RB2_jump(y_beg, y, pDst, pSrc, nDstPitch, nSrcPitch);
  for (; y < y_end; ++y)
  {
    for (int x = 0; x < nWidth; x++)
    {
      if constexpr(sizeof(pixel_t) <= 2)
        pDst[x] = (pSrc[x * 2] + pSrc[x * 2 + 1]
          + pSrc[x * 2 + nSrcPitch / sizeof(pixel_t)] + pSrc[x * 2 + nSrcPitch / sizeof(pixel_t) + 1] + 2) / 4; // int
      else
        pDst[x] = (pSrc[x * 2] + pSrc[x * 2 + 1]
          + pSrc[x * 2 + nSrcPitch / sizeof(pixel_t)] + pSrc[x * 2 + nSrcPitch / sizeof(pixel_t) + 1]) * (1.0f / 4.0f); // float
    }
    pDst += nDstPitch / sizeof(pixel_t);
    pSrc += nSrcPitch / sizeof(pixel_t) * 2;
  }
}

// 8-32bits
template<typename pixel_t>
void RB2F(
  unsigned char *pDst8, const unsigned char *pSrc8, int nDstPitch, int nSrcPitch,
  int nWidth, int nHeight, int y_beg, int y_end, int cpuFlags)
{
  assert(y_beg >= 0);
  assert(y_end <= nHeight);

  pixel_t *    pDst = reinterpret_cast<pixel_t *>(pDst8);
  const pixel_t *pSrc = reinterpret_cast<const pixel_t *>(pSrc8);

  bool isse2 = !!(cpuFlags & CPUF_SSE2);

  if (isse2 && (sizeof(pixel_t) == 1))
  {
    int				y = 0;

    RB2_jump(y_beg, y, pDst, pSrc, nDstPitch, nSrcPitch);
    RB2F_iSSE((uint8_t *)pDst, (uint8_t *)pSrc, nDstPitch, nSrcPitch, nWidth, y_end - y_beg);
  }
  else
  {
    RB2F_C<pixel_t>(pDst, pSrc, nDstPitch, nSrcPitch, nWidth, nHeight, y_beg, y_end);
  }
}

/*
void RB2Filtered_C(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch,
            int nSrcPitch, int nWidth, int nHeight)
{ // sort of Reduceby2 with 1/4, 1/2, 1/4 filter for smoothing - Fizick v.1.10.3

    for ( int x = 0; x < nWidth; x++ )
            pDst[x] = (pSrc[x*2] + pSrc[x*2+1] + pSrc[x*2+nSrcPitch+1] + pSrc[x*2+nSrcPitch] + 2) / 4;
    pDst += nDstPitch;
    pSrc += nSrcPitch * 2;

  for ( int y = 1; y < nHeight; y++ )
  {
        int x = 0;
            pDst[x] = (2*pSrc[x*2-nSrcPitch] + 2*pSrc[x*2-nSrcPitch+1] +
                        4*pSrc[x*2] + 4*pSrc[x*2+1] +
                        2*pSrc[x*2+nSrcPitch+1] + 2*pSrc[x*2+nSrcPitch] + 8) / 16;

    for ( x = 1; x < nWidth; x++ )
            pDst[x] = (pSrc[x*2-nSrcPitch-1] + pSrc[x*2-nSrcPitch]*2 + pSrc[x*2-nSrcPitch+1] +
                       pSrc[x*2-1]*2 + pSrc[x*2]*4 + pSrc[x*2+1]*2 +
                       pSrc[x*2+nSrcPitch-1] + pSrc[x*2+nSrcPitch]*2 + pSrc[x*2+nSrcPitch+1] + 8) / 16;
    pDst += nDstPitch;
    pSrc += nSrcPitch * 2;
  }
}

void RB2BilinearFiltered_C(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch,
            int nSrcPitch, int nWidth, int nHeight)
{ // filtered bilinear with 1/8, 3/8, 3/8, 1/8 filter for smoothing and anti-aliasing - Fizick v.2.3.1

  for ( int y = 0; y < 1; y++ )
  {
    for ( int x = 0; x < nWidth; x++ )
            pDst[x] = (pSrc[x*2] + pSrc[x*2+1] + pSrc[x*2+nSrcPitch+1] + pSrc[x*2+nSrcPitch] + 2) / 4;
    pDst += nDstPitch;
    pSrc += nSrcPitch * 2;
  }

  for ( int y = 1; y < nHeight-1; y++ )
  {
    for ( int x = 0; x < 1; x++ )
           pDst[x] = (pSrc[x*2] + pSrc[x*2+1] + pSrc[x*2+nSrcPitch+1] + pSrc[x*2+nSrcPitch] + 2) / 4;

    for ( x = 1; x < nWidth-1; x++ )
            pDst[x] = (unsigned int)(pSrc[x*2-nSrcPitch-1] + pSrc[x*2-nSrcPitch]*3 + pSrc[x*2-nSrcPitch+1]*3 + pSrc[x*2-nSrcPitch+2] +
                       pSrc[x*2-1]*3 + pSrc[x*2]*9 + pSrc[x*2+1]*9 + pSrc[x*2+2]*3 +
                       pSrc[x*2+nSrcPitch-1]*3 + pSrc[x*2+nSrcPitch]*9 + pSrc[x*2+nSrcPitch+1]*9 + pSrc[x*2+nSrcPitch+2]*3 +
                       pSrc[x*2+nSrcPitch*2-1] + pSrc[x*2+nSrcPitch*2]*3 + pSrc[x*2+nSrcPitch*2+1]*3 + pSrc[x*2+nSrcPitch*2+2] + 32) / 64;

    for ( int x = max(nWidth-1,1); x < nWidth; x++ )
           pDst[x] = (pSrc[x*2] + pSrc[x*2+1] + pSrc[x*2+nSrcPitch+1] + pSrc[x*2+nSrcPitch] + 2) / 4;

    pDst += nDstPitch;
    pSrc += nSrcPitch * 2;
  }
  for ( int y = max(nHeight-1,1); y < nHeight; y++ )
  {
    for ( int x = 0; x < nWidth; x++ )
            pDst[x] = (pSrc[x*2] + pSrc[x*2+1] + pSrc[x*2+nSrcPitch+1] + pSrc[x*2+nSrcPitch] + 2) / 4;
    pDst += nDstPitch;
    pSrc += nSrcPitch * 2;
  }

}

void RB2Quadratic_C(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch,
            int nSrcPitch, int nWidth, int nHeight)
{ // filtered quadratic with 1/64, 9/64, 22/64, 22/64, 9/64, 1/64 filter for smoothing and anti-aliasing - Fizick v.2.3.1

  for ( int y = 0; y < 1; y++ )
  {
    for ( int x = 0; x < nWidth; x++ )
            pDst[x] = (pSrc[x*2] + pSrc[x*2+1] + pSrc[x*2+nSrcPitch+1] + pSrc[x*2+nSrcPitch] + 2) / 4;
    pDst += nDstPitch;
    pSrc += nSrcPitch * 2;
  }

  for ( int y = 1; y < nHeight-1; y++ )
  {
    for ( int x = 0; x < 1; x++ )
           pDst[x] = (pSrc[x*2] + pSrc[x*2+1] + pSrc[x*2+nSrcPitch+1] + pSrc[x*2+nSrcPitch] + 2) / 4;

    for ( int x = 1; x < nWidth-1; x++ )
            pDst[x] =
            (pSrc[x*2-nSrcPitch*2-2] + pSrc[x*2-nSrcPitch*2-1]*9 + pSrc[x*2-nSrcPitch*2]*22 + pSrc[x*2-nSrcPitch*2+1]*22 + pSrc[x*2-nSrcPitch*2+2]*9 + pSrc[x*2-nSrcPitch*2+3] +
            pSrc[x*2-nSrcPitch-2]*9 + pSrc[x*2-nSrcPitch-1]*81 + pSrc[x*2-nSrcPitch]*198 + pSrc[x*2-nSrcPitch+1]*198 + pSrc[x*2-nSrcPitch+2]*81 + pSrc[x*2-nSrcPitch+3]*9 +
            pSrc[x*2-2]*22 + pSrc[x*2-1]*198 + pSrc[x*2]*484 + pSrc[x*2+1]*484 + pSrc[x*2+2]*198 + pSrc[x*2+3]*22 +
            pSrc[x*2+nSrcPitch-2]*22 + pSrc[x*2+nSrcPitch-1]*198 + pSrc[x*2+nSrcPitch]*484 + pSrc[x*2+nSrcPitch+1]*484 + pSrc[x*2+nSrcPitch+2]*198 + pSrc[x*2+nSrcPitch+3]*22 +
            pSrc[x*2+nSrcPitch*2-2]*9 + pSrc[x*2+nSrcPitch*2-1]*81 + pSrc[x*2+nSrcPitch*2]*198 + pSrc[x*2+nSrcPitch*2+1]*198 + pSrc[x*2+nSrcPitch*2+2]*81 + pSrc[x*2+nSrcPitch*2+3]*9 +
            pSrc[x*2+nSrcPitch*3-2] + pSrc[x*2+nSrcPitch*3-1]*9 + pSrc[x*2+nSrcPitch*3]*22 + pSrc[x*2+nSrcPitch*3+1]*22 + pSrc[x*2+nSrcPitch*3+2]*9 + pSrc[x*2+nSrcPitch*3+3] + 2048) /4096;

    for ( int x = max(nWidth-1,1); x < nWidth; x++ )
           pDst[x] = (pSrc[x*2] + pSrc[x*2+1] + pSrc[x*2+nSrcPitch+1] + pSrc[x*2+nSrcPitch] + 2) / 4;

    pDst += nDstPitch;
    pSrc += nSrcPitch * 2;
  }
  for ( int y = max(nHeight-1,1); y < nHeight; y++ )
  {
    for ( int x = 0; x < nWidth; x++ )
            pDst[x] = (pSrc[x*2] + pSrc[x*2+1] + pSrc[x*2+nSrcPitch+1] + pSrc[x*2+nSrcPitch] + 2) / 4;
    pDst += nDstPitch;
    pSrc += nSrcPitch * 2;
  }

}

void RB2Cubic_C(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch,
            int nSrcPitch, int nWidth, int nHeight)
{ // filtered qubic with 1/32, 5/32, 10/32, 10/32, 5/32, 1/32 filter for smoothing and anti-aliasing - Fizick v.2.3.1

  for ( int y = 0; y < 1; y++ )
  {
    for ( int x = 0; x < nWidth; x++ )
            pDst[x] = (pSrc[x*2] + pSrc[x*2+1] + pSrc[x*2+nSrcPitch+1] + pSrc[x*2+nSrcPitch] + 2) / 4;
    pDst += nDstPitch;
    pSrc += nSrcPitch * 2;
  }

  for ( int y = 1; y < nHeight-1; y++ )
  {
    for ( int x = 0; x < 1; x++ )
           pDst[x] = (pSrc[x*2] + pSrc[x*2+1] + pSrc[x*2+nSrcPitch+1] + pSrc[x*2+nSrcPitch] + 2) / 4;

    for ( int x = 1; x < nWidth-1; x++ )
            pDst[x] =
            (pSrc[x*2-nSrcPitch*2-2] + pSrc[x*2-nSrcPitch*2-1]*5 + pSrc[x*2-nSrcPitch*2]*10 + pSrc[x*2-nSrcPitch*2+1]*10 + pSrc[x*2-nSrcPitch*2+2]*5 + pSrc[x*2-nSrcPitch*2+3] +
            pSrc[x*2-nSrcPitch-2]*5 + pSrc[x*2-nSrcPitch-1]*25 + pSrc[x*2-nSrcPitch]*50 + pSrc[x*2-nSrcPitch+1]*50 + pSrc[x*2-nSrcPitch+2]*25 + pSrc[x*2-nSrcPitch+3]*5 +
            pSrc[x*2-2]*10 + pSrc[x*2-1]*50 + pSrc[x*2]*100 + pSrc[x*2+1]*100 + pSrc[x*2+2]*50 + pSrc[x*2+3]*10 +
            pSrc[x*2+nSrcPitch-2]*10 + pSrc[x*2+nSrcPitch-1]*50 + pSrc[x*2+nSrcPitch]*100 + pSrc[x*2+nSrcPitch+1]*100 + pSrc[x*2+nSrcPitch+2]*50 + pSrc[x*2+nSrcPitch+3]*10 +
            pSrc[x*2+nSrcPitch*2-2]*5 + pSrc[x*2+nSrcPitch*2-1]*25 + pSrc[x*2+nSrcPitch*2]*50 + pSrc[x*2+nSrcPitch*2+1]*50 + pSrc[x*2+nSrcPitch*2+2]*25 + pSrc[x*2+nSrcPitch*2+3]*5 +
            pSrc[x*2+nSrcPitch*3-2] + pSrc[x*2+nSrcPitch*3-1]*5 + pSrc[x*2+nSrcPitch*3]*10 + pSrc[x*2+nSrcPitch*3+1]*10 + pSrc[x*2+nSrcPitch*3+2]*5 + pSrc[x*2+nSrcPitch*3+3] + 512) /1024;

    for ( int x = max(nWidth-1,1); x < nWidth; x++ )
           pDst[x] = (pSrc[x*2] + pSrc[x*2+1] + pSrc[x*2+nSrcPitch+1] + pSrc[x*2+nSrcPitch] + 2) / 4;

    pDst += nDstPitch;
    pSrc += nSrcPitch * 2;
  }
  for ( int y = max(nHeight-1,1); y < nHeight; y++ )
  {
    for ( int x = 0; x < nWidth; x++ )
            pDst[x] = (pSrc[x*2] + pSrc[x*2+1] + pSrc[x*2+nSrcPitch+1] + pSrc[x*2+nSrcPitch] + 2) / 4;
    pDst += nDstPitch;
    pSrc += nSrcPitch * 2;
  }

}
*/

//8-32 bits
// Filtered with 1/4, 1/2, 1/4 filter for smoothing and anti-aliasing - Fizick
// nHeight is dst height which is reduced by 2 source height
template<typename pixel_t>
void RB2FilteredVertical(
  unsigned char *pDst8, const unsigned char *pSrc8, int nDstPitch, int nSrcPitch,
  int nWidth, int nHeight, int y_beg, int y_end, int cpuFlags)
{

  bool isse2 = !!(cpuFlags & CPUF_SSE2);

  pixel_t *pDst = reinterpret_cast<pixel_t *>(pDst8);
  const pixel_t *pSrc = reinterpret_cast<const pixel_t *>(pSrc8);

  int				nWidthMMX = (nWidth / 4) * 4;
  const int		y_loop_b = std::max(y_beg, 1);
  int				y = 0;

  if (y_beg < y_loop_b)
  {
    for (int x = 0; x < nWidth; x++)
    {
      if constexpr(sizeof(pixel_t) <= 2)
        pDst[x] = (pSrc[x] + pSrc[x + nSrcPitch / sizeof(pixel_t)] + 1) / 2;
      else
        pDst[x] = (pSrc[x] + pSrc[x + nSrcPitch / sizeof(pixel_t)]) * 0.5f;
    }
  }

  RB2_jump(y_loop_b, y, pDst, pSrc, nDstPitch, nSrcPitch);

  if ((sizeof(pixel_t) == 1) && isse2 && nWidthMMX >= 4)
  {
    for (; y < y_end; ++y)
    {
      RB2FilteredVerticalLine_SSE((uint8_t *)pDst, (uint8_t *)pSrc, nSrcPitch, nWidthMMX);
#ifndef _M_X64
      _mm_empty();
#endif

      for (int x = nWidthMMX; x < nWidth; x++)
      {
        pDst[x] = (pSrc[x - nSrcPitch / sizeof(pixel_t)] + pSrc[x] * 2 + pSrc[x + nSrcPitch / sizeof(pixel_t)] + 2) / 4;
      }

      pDst += nDstPitch / sizeof(pixel_t);
      pSrc += nSrcPitch / sizeof(pixel_t) * 2;
    }
  }
  else
  {
    for (; y < y_end; ++y)
    {
      for (int x = 0; x < nWidth; x++)
      {
        if constexpr(sizeof(pixel_t) <= 2)
          pDst[x] = (pSrc[x - nSrcPitch / sizeof(pixel_t)] + pSrc[x] * 2 + pSrc[x + nSrcPitch / sizeof(pixel_t)] + 2) / 4;
        else
          pDst[x] = (pSrc[x - nSrcPitch / sizeof(pixel_t)] + pSrc[x] * 2 + pSrc[x + nSrcPitch / sizeof(pixel_t)]) * (1.0f / 4.0f);
      }

      pDst += nDstPitch / sizeof(pixel_t);
      pSrc += nSrcPitch / sizeof(pixel_t) * 2;
    }
  }
}

//8-32bits
// Filtered with 1/4, 1/2, 1/4 filter for smoothing and anti-aliasing - Fizick
// nWidth is dst height which is reduced by 2 source width
template<typename pixel_t>
void RB2FilteredHorizontalInplace(
  unsigned char *pSrc8, int nSrcPitch,
  int nWidth, int nHeight, int y_beg, int y_end, int cpuFlags)
{

  bool isse2 = !!(cpuFlags & CPUF_SSE2);

  int				nWidthMMX = 1 + ((nWidth - 2) / 4) * 4;
  int				y = 0;

  pixel_t *pSrc = reinterpret_cast<pixel_t *>(pSrc8);

  RB2_jump_1(y_beg, y, pSrc, nSrcPitch);

  for (; y < y_end; ++y)
  {
    const int x = 0;
    pixel_t pSrc0;
    if constexpr(sizeof(pixel_t) <= 2)
      pSrc0 = (pSrc[x * 2] + pSrc[x * 2 + 1] + 1) / 2;
    else
      pSrc0 = (pSrc[x * 2] + pSrc[x * 2 + 1]) * 0.5f;

    if (sizeof(pixel_t) == 1 && isse2)
    {
      RB2FilteredHorizontalInplaceLine_SSE((uint8_t *)pSrc, nWidthMMX); // very first is skipped
#ifndef _M_X64
      _mm_empty();
#endif
      for (int x = nWidthMMX; x < nWidth; x++)
      {
        pSrc[x] = (pSrc[x * 2 - 1] + pSrc[x * 2] * 2 + pSrc[x * 2 + 1] + 2) / 4;
      }
    }
    else
    {
      for (int x = 1; x < nWidth; x++)
      {
        if constexpr(sizeof(pixel_t) <= 2)
          pSrc[x] = (pSrc[x * 2 - 1] + pSrc[x * 2] * 2 + pSrc[x * 2 + 1] + 2) / 4;
        else
          pSrc[x] = (pSrc[x * 2 - 1] + pSrc[x * 2] * 2 + pSrc[x * 2 + 1]) * (1.0f / 4.0f);
      }
    }
    pSrc[0] = pSrc0;

    pSrc += nSrcPitch / sizeof(pixel_t);
  }
}

//8-32bits
// separable Filtered with 1/4, 1/2, 1/4 filter for smoothing and anti-aliasing - Fizick v.2.5.2
// assume he have enough horizontal dimension for intermediate results (double as final)
template<typename pixel_t>
void RB2Filtered(
  unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch,
  int nWidth, int nHeight, int y_beg, int y_end, int cpuFlags)
{
  RB2FilteredVertical<pixel_t>(pDst, pSrc, nDstPitch, nSrcPitch, nWidth * 2, nHeight, y_beg, y_end, cpuFlags); // intermediate half height
  RB2FilteredHorizontalInplace<pixel_t>(pDst, nDstPitch, nWidth, nHeight, y_beg, y_end, cpuFlags); // inpace width reduction
}



// 8-32bits
// BilinearFiltered with 1/8, 3/8, 3/8, 1/8 filter for smoothing and anti-aliasing - Fizick
// nHeight is dst height which is reduced by 2 source height
template<typename pixel_t>
void RB2BilinearFilteredVertical(
  unsigned char *pDst8, const unsigned char *pSrc8, int nDstPitch, int nSrcPitch,
  int nWidth, int nHeight, int y_beg, int y_end, int cpuFlags)
{

  bool isse2 = !!(cpuFlags & CPUF_SSE2);

  int				nWidthMMX = (nWidth / 4) * 4;
  const int		y_loop_b = std::max(y_beg, 1);
  const int		y_loop_e = std::min(y_end, nHeight - 1);
  int				y = 0;

  pixel_t *pDst = reinterpret_cast<pixel_t *>(pDst8);
  const pixel_t *pSrc = reinterpret_cast<const pixel_t *>(pSrc8);

  if (y_beg < y_loop_b)
  {
    for (int x = 0; x < nWidth; x++)
    {
      if constexpr(sizeof(pixel_t) <= 2)
        pDst[x] = (pSrc[x] + pSrc[x + nSrcPitch / sizeof(pixel_t)] + 1) / 2;
      else
        pDst[x] = (pSrc[x] + pSrc[x + nSrcPitch / sizeof(pixel_t)]) * 0.5f;
    }
  }

  RB2_jump(y_loop_b, y, pDst, pSrc, nDstPitch, nSrcPitch);

  if (sizeof(pixel_t) == 1 && isse2 && nWidthMMX >= 4)
  {
    for (; y < y_loop_e; ++y)
    {
      RB2BilinearFilteredVerticalLine_SSE((uint8_t *)pDst, (uint8_t *)pSrc, nSrcPitch, nWidthMMX);
#ifndef _M_X64
      _mm_empty();
#endif

      for (int x = nWidthMMX; x < nWidth; x++)
      {
        pDst[x] = (pSrc[x - nSrcPitch / sizeof(pixel_t)]
          + pSrc[x] * 3
          + pSrc[x + nSrcPitch / sizeof(pixel_t)] * 3
          + pSrc[x + nSrcPitch / sizeof(pixel_t) * 2] + 4) / 8;
      }

      pDst += nDstPitch / sizeof(pixel_t);
      pSrc += nSrcPitch / sizeof(pixel_t) * 2;
    }
  }
  else
  {
    for (; y < y_loop_e; ++y)
    {
      for (int x = 0; x < nWidth; x++)
      {
        if constexpr(sizeof(pixel_t) <= 2)
          pDst[x] = (pSrc[x - nSrcPitch / sizeof(pixel_t)]
            + pSrc[x] * 3
            + pSrc[x + nSrcPitch / sizeof(pixel_t)] * 3
            + pSrc[x + nSrcPitch / sizeof(pixel_t) * 2] + 4) / 8;
        else
          pDst[x] = (pSrc[x - nSrcPitch / sizeof(pixel_t)]
            + pSrc[x] * 3.0f
            + pSrc[x + nSrcPitch / sizeof(pixel_t)] * 3.0f
            + pSrc[x + nSrcPitch / sizeof(pixel_t) * 2]) * (1.0f / 8.0f);
      }

      pDst += nDstPitch / sizeof(pixel_t);
      pSrc += nSrcPitch / sizeof(pixel_t) * 2;
    }
  }

  RB2_jump(std::max(y_loop_e, 1), y, pDst, pSrc, nDstPitch, nSrcPitch);

  for (; y < y_end; ++y)
  {
    for (int x = 0; x < nWidth; x++)
    {
      if constexpr(sizeof(pixel_t) <= 2)
        pDst[x] = (pSrc[x] + pSrc[x + nSrcPitch / sizeof(pixel_t)] + 1) / 2;
      else
        pDst[x] = (pSrc[x] + pSrc[x + nSrcPitch / sizeof(pixel_t)]) * 0.5f;
    }
    pDst += nDstPitch / sizeof(pixel_t);
    pSrc += nSrcPitch / sizeof(pixel_t) * 2;
  }
}

// 8-32bits
// BilinearFiltered with 1/8, 3/8, 3/8, 1/8 filter for smoothing and anti-aliasing - Fizick
// nWidth is dst height which is reduced by 2 source width
template<typename pixel_t>
void RB2BilinearFilteredHorizontalInplace(
  unsigned char *pSrc8, int nSrcPitch,
  int nWidth, int nHeight, int y_beg, int y_end, int cpuFlags)
{
  bool isse2 = !!(cpuFlags & CPUF_SSE2);

  int				nWidthMMX = 1 + ((nWidth - 2) / 4) * 4;
  int				y = 0;

  pixel_t *pSrc = reinterpret_cast<pixel_t *>(pSrc8);

  RB2_jump_1(y_beg, y, pSrc, nSrcPitch);

  for (; y < y_end; ++y)
  {
    int x = 0;
    pixel_t pSrc0;
    if constexpr(sizeof(pixel_t) <= 2)
      pSrc0 = (pSrc[x * 2] + pSrc[x * 2 + 1] + 1) / 2;
    else
      pSrc0 = (pSrc[x * 2] + pSrc[x * 2 + 1]) * 0.5f;

    if (sizeof(pixel_t) == 1 && isse2)
    {
      RB2BilinearFilteredHorizontalInplaceLine_SSE((uint8_t *)pSrc, nWidthMMX); // very first is skipped
#ifndef _M_X64
      _mm_empty();
#endif
      for (int x = nWidthMMX; x < nWidth - 1; x++)
      {
        pSrc[x] = (pSrc[x * 2 - 1] + pSrc[x * 2] * 3 + pSrc[x * 2 + 1] * 3 + pSrc[x * 2 + 2] + 4) / 8;
      }
    }
    else
    {
      for (int x = 1; x < nWidth - 1; x++)
      {
        if constexpr(sizeof(pixel_t) <= 2)
          pSrc[x] = (pSrc[x * 2 - 1] + pSrc[x * 2] * 3 + pSrc[x * 2 + 1] * 3 + pSrc[x * 2 + 2] + 4) / 8;
        else
          pSrc[x] = (pSrc[x * 2 - 1] + pSrc[x * 2] * 3.0f + pSrc[x * 2 + 1] * 3.0f + pSrc[x * 2 + 2]) * (1.0f / 8.0f);
      }
    }
    pSrc[0] = pSrc0;

    for (int x = std::max(nWidth - 1, 1); x < nWidth; x++)
    {
      if constexpr(sizeof(pixel_t) <= 2)
        pSrc[x] = (pSrc[x * 2] + pSrc[x * 2 + 1] + 1) / 2;
      else
        pSrc[x] = (pSrc[x * 2] + pSrc[x * 2 + 1]) * 0.5f;
    }

    pSrc += nSrcPitch / sizeof(pixel_t);
  }
}

// 8-32bits
// separable BilinearFiltered with 1/8, 3/8, 3/8, 1/8 filter for smoothing and anti-aliasing - Fizick v.2.5.2
// assume he have enough horizontal dimension for intermediate results (double as final)
template<typename pixel_t>
void RB2BilinearFiltered(
  unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch,
  int nWidth, int nHeight, int y_beg, int y_end, int cpuFlags)
{
  RB2BilinearFilteredVertical<pixel_t>(pDst, pSrc, nDstPitch, nSrcPitch, nWidth * 2, nHeight, y_beg, y_end, cpuFlags); // intermediate half height
  RB2BilinearFilteredHorizontalInplace<pixel_t>(pDst, nDstPitch, nWidth, nHeight, y_beg, y_end, cpuFlags); // inpace width reduction
}


//8-32bits
// filtered Quadratic with 1/64, 9/64, 22/64, 22/64, 9/64, 1/64 filter for smoothing and anti-aliasing - Fizick
// nHeight is dst height which is reduced by 2 source height
template<typename pixel_t>
void RB2QuadraticVertical(
  unsigned char *pDst8, const unsigned char *pSrc8, int nDstPitch, int nSrcPitch,
  int nWidth, int nHeight, int y_beg, int y_end, int cpuFlags)
{
  bool isse2 = !!(cpuFlags & CPUF_SSE2);

  int				nWidthMMX = (nWidth / 4) * 4;
  const int		y_loop_b = std::max(y_beg, 1);
  const int		y_loop_e = std::min(y_end, nHeight - 1);
  int				y = 0;

  pixel_t *pDst = reinterpret_cast<pixel_t *>(pDst8);
  const pixel_t *pSrc = reinterpret_cast<const pixel_t *>(pSrc8);

  if (y_beg < y_loop_b)
  {
    for (int x = 0; x < nWidth; x++)
    {
      if constexpr(sizeof(pixel_t) <= 2)
        pDst[x] = (pSrc[x] + pSrc[x + nSrcPitch / sizeof(pixel_t)] + 1) / 2;
      else
        pDst[x] = (pSrc[x] + pSrc[x + nSrcPitch / sizeof(pixel_t)]) * 0.5f;
    }
  }

  RB2_jump(y_loop_b, y, pDst, pSrc, nDstPitch, nSrcPitch);

  if (sizeof(pixel_t) == 1 && isse2 && nWidthMMX >= 4)
  {
    for (; y < y_loop_e; ++y)
    {
      RB2QuadraticVerticalLine_SSE((uint8_t *)pDst, (uint8_t *)pSrc, nSrcPitch, nWidthMMX);
#ifndef _M_X64
      _mm_empty();
#endif

      for (int x = nWidthMMX; x < nWidth; x++)
      {
        pDst[x] = (pSrc[x - nSrcPitch / sizeof(pixel_t) * 2]
          + pSrc[x - nSrcPitch / sizeof(pixel_t)] * 9
          + pSrc[x] * 22
          + pSrc[x + nSrcPitch / sizeof(pixel_t)] * 22
          + pSrc[x + nSrcPitch / sizeof(pixel_t) * 2] * 9
          + pSrc[x + nSrcPitch / sizeof(pixel_t) * 3] + 32) / 64;
      }

      pDst += nDstPitch / sizeof(pixel_t);
      pSrc += nSrcPitch / sizeof(pixel_t) * 2;
    }
  }
  else
  {
    for (; y < y_loop_e; ++y)
    {
      for (int x = 0; x < nWidth; x++)
      {
        if constexpr(sizeof(pixel_t) <= 2)
          pDst[x] = (pSrc[x - nSrcPitch / sizeof(pixel_t) * 2]
          + pSrc[x - nSrcPitch / sizeof(pixel_t)] * 9
          + pSrc[x] * 22
          + pSrc[x + nSrcPitch / sizeof(pixel_t)] * 22
          + pSrc[x + nSrcPitch / sizeof(pixel_t) * 2] * 9
          + pSrc[x + nSrcPitch / sizeof(pixel_t) * 3] + 32) / 64;
        else
          pDst[x] = (pSrc[x - nSrcPitch / sizeof(pixel_t) * 2]
            + pSrc[x - nSrcPitch / sizeof(pixel_t)] * 9
            + pSrc[x] * 22
            + pSrc[x + nSrcPitch / sizeof(pixel_t)] * 22
            + pSrc[x + nSrcPitch / sizeof(pixel_t) * 2] * 9
            + pSrc[x + nSrcPitch / sizeof(pixel_t) * 3]) * (1.0f / 64.0f);
      }

      pDst += nDstPitch / sizeof(pixel_t);
      pSrc += nSrcPitch / sizeof(pixel_t) * 2;
    }
  }

  RB2_jump(std::max(y_loop_e, 1), y, pDst, pSrc, nDstPitch, nSrcPitch);

  for (; y < y_end; ++y)
  {
    for (int x = 0; x < nWidth; x++)
    {
      if constexpr(sizeof(pixel_t) <= 2)
        pDst[x] = (pSrc[x] + pSrc[x + nSrcPitch / sizeof(pixel_t)] + 1) / 2;
      else
        pDst[x] = (pSrc[x] + pSrc[x + nSrcPitch / sizeof(pixel_t)]) * 0.5f;
    }
    pDst += nDstPitch / sizeof(pixel_t);
    pSrc += nSrcPitch / sizeof(pixel_t) * 2;
  }
}

// 8-32bits
// filtered Quadratic with 1/64, 9/64, 22/64, 22/64, 9/64, 1/64 filter for smoothing and anti-aliasing - Fizick
// nWidth is dst height which is reduced by 2 source width
template<typename pixel_t>
void RB2QuadraticHorizontalInplace(
  unsigned char *pSrc8, int nSrcPitch,
  int nWidth, int nHeight, int y_beg, int y_end, int cpuFlags)
{
  bool isse2 = !!(cpuFlags & CPUF_SSE2);

  int				nWidthMMX = 1 + ((nWidth - 2) / 4) * 4;
  int				y = 0;

  pixel_t *pSrc = reinterpret_cast<pixel_t *>(pSrc8);

  RB2_jump_1(y_beg, y, pSrc, nSrcPitch);

  for (; y < y_end; ++y)
  {
    int x = 0;
    pixel_t pSrc0;
    if constexpr(sizeof(pixel_t) <= 2)
      pSrc0 = (pSrc[x * 2] + pSrc[x * 2 + 1] + 1) / 2; // store temporary
    else
      pSrc0 = (pSrc[x * 2] + pSrc[x * 2 + 1]) * 0.5f; // store temporary

    if (sizeof(pixel_t) == 1 && isse2)
    {
      RB2QuadraticHorizontalInplaceLine_SSE((uint8_t *)pSrc, nWidthMMX);
#ifndef _M_X64
      _mm_empty();
#endif
      for (int x = nWidthMMX; x < nWidth - 1; x++)
      {
        pSrc[x] = (pSrc[x * 2 - 2] + pSrc[x * 2 - 1] * 9 + pSrc[x * 2] * 22
          + pSrc[x * 2 + 1] * 22 + pSrc[x * 2 + 2] * 9 + pSrc[x * 2 + 3] + 32) / 64;
      }
    }
    else
    {
      for (int x = 1; x < nWidth - 1; x++)
      {
        if constexpr(sizeof(pixel_t) <= 2)
          pSrc[x] = (pSrc[x * 2 - 2] + pSrc[x * 2 - 1] * 9 + pSrc[x * 2] * 22
          + pSrc[x * 2 + 1] * 22 + pSrc[x * 2 + 2] * 9 + pSrc[x * 2 + 3] + 32) / 64;
        else
          pSrc[x] = (pSrc[x * 2 - 2] + pSrc[x * 2 - 1] * 9 + pSrc[x * 2] * 22
            + pSrc[x * 2 + 1] * 22 + pSrc[x * 2 + 2] * 9 + pSrc[x * 2 + 3]) * (1.0f / 64.0f);
      }
    }
    pSrc[0] = pSrc0;

    for (int x = std::max(nWidth - 1, 1); x < nWidth; x++)
    {
      if constexpr(sizeof(pixel_t) <= 2)
        pSrc[x] = (pSrc[x * 2] + pSrc[x * 2 + 1] + 1) / 2;
      else
        pSrc[x] = (pSrc[x * 2] + pSrc[x * 2 + 1]) * 0.5f;
    }

    pSrc += nSrcPitch / sizeof(pixel_t);
  }
}

// 8-32bits
// separable filtered Quadratic with 1/64, 9/64, 22/64, 22/64, 9/64, 1/64 filter for smoothing and anti-aliasing - Fizick v.2.5.2
// assume he have enough horizontal dimension for intermediate results (double as final)
template<typename pixel_t>
void RB2Quadratic(
  unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch,
  int nWidth, int nHeight, int y_beg, int y_end, int cpuFlags)
{
  RB2QuadraticVertical<pixel_t>(pDst, pSrc, nDstPitch, nSrcPitch, nWidth * 2, nHeight, y_beg, y_end, cpuFlags); // intermediate half height
  RB2QuadraticHorizontalInplace<pixel_t>(pDst, nDstPitch, nWidth, nHeight, y_beg, y_end, cpuFlags); // inpace width reduction
}



// 8-32bits
// filtered qubic with 1/32, 5/32, 10/32, 10/32, 5/32, 1/32 filter for smoothing and anti-aliasing - Fizick
// nHeight is dst height which is reduced by 2 source height
template<typename pixel_t>
void RB2CubicVertical(
  unsigned char *pDst8, const unsigned char *pSrc8, int nDstPitch, int nSrcPitch,
  int nWidth, int nHeight, int y_beg, int y_end, int cpuFlags)
{
  bool isse2 = !!(cpuFlags & CPUF_SSE2);

  int				nWidthMMX = (nWidth / 4) * 4;
  const int		y_loop_b = std::max(y_beg, 1);
  const int		y_loop_e = std::min(y_end, nHeight - 1);
  int				y = 0;

  pixel_t *pDst = reinterpret_cast<pixel_t *>(pDst8);
  const pixel_t *pSrc = reinterpret_cast<const pixel_t *>(pSrc8);

  if (y_beg < y_loop_b)
  {
    for (int x = 0; x < nWidth; x++)
    {
      if constexpr(sizeof(pixel_t) <= 2)
        pDst[x] = (pSrc[x] + pSrc[x + nSrcPitch / sizeof(pixel_t)] + 1) / 2;
      else
        pDst[x] = (pSrc[x] + pSrc[x + nSrcPitch / sizeof(pixel_t)]) * 0.5f;
    }
  }

  RB2_jump(y_loop_b, y, pDst, pSrc, nDstPitch, nSrcPitch);

  if (sizeof(pixel_t) == 1 && isse2 && nWidthMMX >= 4)
  {
    for (; y < y_loop_e; ++y)
    {
      RB2CubicVerticalLine_SSE((uint8_t *)pDst, (uint8_t *)pSrc, nSrcPitch, nWidthMMX);
#ifndef _M_X64
      _mm_empty();
#endif

      for (int x = nWidthMMX; x < nWidth; x++)
      {
        pDst[x] = (pSrc[x - nSrcPitch / sizeof(pixel_t) * 2]
          + pSrc[x - nSrcPitch / sizeof(pixel_t)] * 5
          + pSrc[x] * 10
          + pSrc[x + nSrcPitch / sizeof(pixel_t)] * 10
          + pSrc[x + nSrcPitch / sizeof(pixel_t) * 2] * 5
          + pSrc[x + nSrcPitch / sizeof(pixel_t) * 3] + 16) / 32;
      }

      pDst += nDstPitch / sizeof(pixel_t);
      pSrc += nSrcPitch / sizeof(pixel_t) * 2;
    }
  }
  else
  {
    for (; y < y_loop_e; ++y)
    {
      for (int x = 0; x < nWidth; x++)
      {
        if constexpr(sizeof(pixel_t) <= 2)
          pDst[x] = (pSrc[x - nSrcPitch / sizeof(pixel_t) * 2]
          + pSrc[x - nSrcPitch / sizeof(pixel_t)] * 5
          + pSrc[x] * 10
          + pSrc[x + nSrcPitch / sizeof(pixel_t)] * 10
          + pSrc[x + nSrcPitch / sizeof(pixel_t) * 2] * 5
          + pSrc[x + nSrcPitch / sizeof(pixel_t) * 3] + 16) / 32;
        else
          pDst[x] = (pSrc[x - nSrcPitch / sizeof(pixel_t) * 2]
            + pSrc[x - nSrcPitch / sizeof(pixel_t)] * 5
            + pSrc[x] * 10
            + pSrc[x + nSrcPitch / sizeof(pixel_t)] * 10
            + pSrc[x + nSrcPitch / sizeof(pixel_t) * 2] * 5
            + pSrc[x + nSrcPitch / sizeof(pixel_t) * 3]) * (1.0f / 32.0f);
      }

      pDst += nDstPitch / sizeof(pixel_t);
      pSrc += nSrcPitch / sizeof(pixel_t) * 2;
    }
  }

  RB2_jump(std::max(y_loop_e, 1), y, pDst, pSrc, nDstPitch, nSrcPitch);

  for (; y < y_end; ++y)
  {
    for (int x = 0; x < nWidth; x++)
    {
      if constexpr(sizeof(pixel_t) <= 2)
        pDst[x] = (pSrc[x] + pSrc[x + nSrcPitch / sizeof(pixel_t)] + 1) / 2;
      else
        pDst[x] = (pSrc[x] + pSrc[x + nSrcPitch / sizeof(pixel_t)]) * 0.5f;
    }
    pDst += nDstPitch / sizeof(pixel_t);
    pSrc += nSrcPitch / sizeof(pixel_t) * 2;
  }
}

// 8-32 bits
// filtered qubic with 1/32, 5/32, 10/32, 10/32, 5/32, 1/32 filter for smoothing and anti-aliasing - Fizick
// nWidth is dst height which is reduced by 2 source width
template<typename pixel_t>
void RB2CubicHorizontalInplace(
  unsigned char *pSrc8, int nSrcPitch,
  int nWidth, int nHeight, int y_beg, int y_end, int cpuFlags)
{
  bool isse2 = !!(cpuFlags & CPUF_SSE2);

  int				nWidthMMX = 1 + ((nWidth - 2) / 4) * 4;
  int				y = 0;

  pixel_t *pSrc = reinterpret_cast<pixel_t *>(pSrc8);

  RB2_jump_1(y_beg, y, pSrc, nSrcPitch);

  for (; y < y_end; ++y)
  {
    int x = 0;
    pixel_t pSrcw0;
    if constexpr(sizeof(pixel_t) <= 2)
      pSrcw0 = (pSrc[x * 2] + pSrc[x * 2 + 1] + 1) / 2; // store temporary
    else
      pSrcw0 = (pSrc[x * 2] + pSrc[x * 2 + 1]) * 0.5f; // store temporary
    if (sizeof(pixel_t) == 1 && isse2)
    {
      RB2CubicHorizontalInplaceLine_SSE((uint8_t *)pSrc, nWidthMMX);
#ifndef _M_X64
      _mm_empty();
#endif
      for (int x = nWidthMMX; x < nWidth - 1; x++)
      {
        pSrc[x] = (pSrc[x * 2 - 2] + pSrc[x * 2 - 1] * 5 + pSrc[x * 2] * 10
          + pSrc[x * 2 + 1] * 10 + pSrc[x * 2 + 2] * 5 + pSrc[x * 2 + 3] + 16) / 32;
      }
    }
    else
    {
      for (int x = 1; x < nWidth - 1; x++)
      {
        if constexpr(sizeof(pixel_t) <= 2)
          pSrc[x] = (pSrc[x * 2 - 2] + pSrc[x * 2 - 1] * 5 + pSrc[x * 2] * 10
          + pSrc[x * 2 + 1] * 10 + pSrc[x * 2 + 2] * 5 + pSrc[x * 2 + 3] + 16) / 32;
        else
          pSrc[x] = (pSrc[x * 2 - 2] + pSrc[x * 2 - 1] * 5 + pSrc[x * 2] * 10
            + pSrc[x * 2 + 1] * 10 + pSrc[x * 2 + 2] * 5 + pSrc[x * 2 + 3]) * (1.0f / 32.0f);
      }
    }
    pSrc[0] = pSrcw0;

    for (int x = std::max(nWidth - 1, 1); x < nWidth; x++)
    {
      if constexpr(sizeof(pixel_t) <= 2)
        pSrc[x] = (pSrc[x * 2] + pSrc[x * 2 + 1] + 1) / 2;
      else
        pSrc[x] = (pSrc[x * 2] + pSrc[x * 2 + 1]) * 0.5f;
    }

    pSrc += nSrcPitch / sizeof(pixel_t);
  }

  if (isse2)
  {
#ifndef _M_X64
    _mm_empty();
#endif
  }
}

// 8-32bits
// separable filtered cubic with 1/32, 5/32, 10/32, 10/32, 5/32, 1/32 filter for smoothing and anti-aliasing - Fizick v.2.5.2
// assume he have enough horizontal dimension for intermediate results (double as final)
template<typename pixel_t>
void RB2Cubic(
  unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch,
  int nWidth, int nHeight, int y_beg, int y_end, int cpuFlags)
{
  RB2CubicVertical<pixel_t>(pDst, pSrc, nDstPitch, nSrcPitch, nWidth * 2, nHeight, y_beg, y_end, cpuFlags); // intermediate half height
  RB2CubicHorizontalInplace<pixel_t>(pDst, nDstPitch, nWidth, nHeight, y_beg, y_end, cpuFlags); // inpace width reduction
}



// -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -



// 8-32 bits
template<typename pixel_t>
void VerticalBilin(unsigned char *pDst8, const unsigned char *pSrc8, int nDstPitch,
  int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel)
{
  pixel_t *pDst = reinterpret_cast<pixel_t *>(pDst8);
  const pixel_t *pSrc = reinterpret_cast<const pixel_t *>(pSrc8);

  nSrcPitch /= sizeof(pixel_t);
  nDstPitch /= sizeof(pixel_t);

  for (int j = 0; j < nHeight - 1; j++)
  {
    for (int i = 0; i < nWidth; i++)
      if constexpr(sizeof(pixel_t) <= 2)
        pDst[i] = (pSrc[i] + pSrc[i + nSrcPitch] + 1) >> 1; // int
      else
        pDst[i] = (pSrc[i] + pSrc[i + nSrcPitch]) * 0.5f; // float
    pDst += nDstPitch;
    pSrc += nSrcPitch;
  }
  // last row
  for (int i = 0; i < nWidth; i++)
    pDst[i] = pSrc[i];
}

template<typename pixel_t>
void VerticalBilin_sse2(unsigned char *pDst8, const unsigned char *pSrc8, int nDstPitch,
  int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel) {
  (void)bits_per_pixel; // not used

  for (int y = 0; y < nHeight - 1; y++) {
    for (int x = 0; x < nWidth; x += 16) {
      __m128i m0 = _mm_loadu_si128((const __m128i *)&pSrc8[x]);
      __m128i m1 = _mm_loadu_si128((const __m128i *)&pSrc8[x + nSrcPitch]);

      if(sizeof(pixel_t) == 1)
        m0 = _mm_avg_epu8(m0, m1);
      else
        m0 = _mm_avg_epu16(m0, m1);
      _mm_storeu_si128((__m128i *)&pDst8[x], m0);
    }

    pSrc8 += nSrcPitch;
    pDst8 += nDstPitch;
  }
  // last row
  for (int x = 0; x < nWidth; x++)
    reinterpret_cast<pixel_t *>(pDst8)[x] = reinterpret_cast<const pixel_t *>(pSrc8)[x];
}


// 8-32 bits
template<typename pixel_t>
void HorizontalBilin(unsigned char *pDst8, const unsigned char *pSrc8, int nDstPitch,
  int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel)
{
  pixel_t *pDst = reinterpret_cast<pixel_t *>(pDst8);
  const pixel_t *pSrc = reinterpret_cast<const pixel_t *>(pSrc8);

  nSrcPitch /= sizeof(pixel_t);
  nDstPitch /= sizeof(pixel_t);

  for (int j = 0; j < nHeight; j++)
  {
    for (int i = 0; i < nWidth - 1; i++)
      if constexpr(sizeof(pixel_t) <= 2)
        pDst[i] = (pSrc[i] + pSrc[i + 1] + 1) >> 1; // int
      else
        pDst[i] = (pSrc[i] + pSrc[i + 1]) * 0.5f; // float

    pDst[nWidth - 1] = pSrc[nWidth - 1];
    pDst += nDstPitch;
    pSrc += nSrcPitch;
  }
}


template<typename pixel_t>
void HorizontalBilin_sse2(unsigned char *pDst8, const unsigned char *pSrc8, int nDstPitch,
  int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel) {
  (void)bits_per_pixel; // not used

  for (int y = 0; y < nHeight; y++) {
    for (int x = 0; x < nWidth; x += 16) {
      __m128i m0 = _mm_loadu_si128((const __m128i *)&pSrc8[x]);
      __m128i m1 = _mm_loadu_si128((const __m128i *)&pSrc8[x + 1]);

      if(sizeof(pixel_t) == 1)
        m0 = _mm_avg_epu8(m0, m1);
      else
        m0 = _mm_avg_epu16(m0, m1);
      _mm_storeu_si128((__m128i *)&pDst8[x], m0);
    }

    reinterpret_cast<pixel_t *>(pDst8)[nWidth - 1] = reinterpret_cast<const pixel_t *>(pSrc8)[nWidth - 1];

    pSrc8 += nSrcPitch;
    pDst8 += nDstPitch;
  }
}


// 8-32 bits
template<typename pixel_t>
void DiagonalBilin(unsigned char *pDst8, const unsigned char *pSrc8, int nDstPitch,
  int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel)
{
  pixel_t *pDst = reinterpret_cast<pixel_t *>(pDst8);
  const pixel_t *pSrc = reinterpret_cast<const pixel_t *>(pSrc8);

  nSrcPitch /= sizeof(pixel_t);
  nDstPitch /= sizeof(pixel_t);

  for (int j = 0; j < nHeight - 1; j++)
  {
    for (int i = 0; i < nWidth - 1; i++)
      if constexpr(sizeof(pixel_t) <= 2) {
        pDst[i] = (pSrc[i] + pSrc[i + 1] + pSrc[i + nSrcPitch] + pSrc[i + nSrcPitch + 1] + 2) >> 2; // int
        pDst[nWidth - 1] = (pSrc[nWidth - 1] + pSrc[nWidth + nSrcPitch - 1] + 1) >> 1;
      }
      else {
        pDst[i] = (pSrc[i] + pSrc[i + 1] + pSrc[i + nSrcPitch] + pSrc[i + nSrcPitch + 1]) * 0.25f; // float
        pDst[nWidth - 1] = (pSrc[nWidth - 1] + pSrc[nWidth + nSrcPitch - 1]) * 0.5f;
      }

    pDst += nDstPitch;
    pSrc += nSrcPitch;
  }
  for (int i = 0; i < nWidth - 1; i++)
    if constexpr(sizeof(pixel_t) <= 2)
      pDst[i] = (pSrc[i] + pSrc[i + 1] + 1) >> 1; // int
    else
      pDst[i] = (pSrc[i] + pSrc[i + 1]) * 0.5f; // float
  pDst[nWidth - 1] = pSrc[nWidth - 1];
}

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4309)
#endif
// fake _mm_packus_epi32 (orig is SSE4.1 only)
static MV_FORCEINLINE __m128i _MM_PACKUS_EPI32(__m128i a, __m128i b)
{
  const static __m128i val_32 = _mm_set1_epi32(0x8000);
  const static __m128i val_16 = _mm_set1_epi16(0x8000);

  a = _mm_sub_epi32(a, val_32);
  b = _mm_sub_epi32(b, val_32);
  a = _mm_packs_epi32(a, b);
  a = _mm_add_epi16(a, val_16);
  return a;
}
#ifdef _MSC_VER
#pragma warning(pop)
#endif

template<typename pixel_t, bool hasSSE41>
void DiagonalBilin_sse2(unsigned char *pDst8, const unsigned char *pSrc8, int nDstPitch,
  int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel)
{
  (void)bits_per_pixel; // not used

  auto zeroes = _mm_setzero_si128();

  for (int y = 0; y < nHeight - 1; y++) {
    for (int x = 0; x < nWidth; x += 8) {
      __m128i m0 = _mm_loadl_epi64((const __m128i *)&pSrc8[x]);
      __m128i m1 = _mm_loadl_epi64((const __m128i *)&pSrc8[x + 1]);
      __m128i m2 = _mm_loadl_epi64((const __m128i *)&pSrc8[x + nSrcPitch]);
      __m128i m3 = _mm_loadl_epi64((const __m128i *)&pSrc8[x + nSrcPitch + 1]);

      if (sizeof(pixel_t) == 1) {
        m0 = _mm_unpacklo_epi8(m0, zeroes);
        m1 = _mm_unpacklo_epi8(m1, zeroes);
        m2 = _mm_unpacklo_epi8(m2, zeroes);
        m3 = _mm_unpacklo_epi8(m3, zeroes);

        m0 = _mm_add_epi16(m0, m1);
        m2 = _mm_add_epi16(m2, m3);
        m0 = _mm_add_epi16(m0, _mm_set1_epi16(2)); // rounding
        m0 = _mm_add_epi16(m0, m2);

        m0 = _mm_srli_epi16(m0, 2);

        m0 = _mm_packus_epi16(m0, m0);
      }
      else {
        m0 = _mm_unpacklo_epi16(m0, zeroes);
        m1 = _mm_unpacklo_epi16(m1, zeroes);
        m2 = _mm_unpacklo_epi16(m2, zeroes);
        m3 = _mm_unpacklo_epi16(m3, zeroes);

        m0 = _mm_add_epi32(m0, m1);
        m2 = _mm_add_epi32(m2, m3);
        m0 = _mm_add_epi32(m0, _mm_set1_epi16(2)); // rounding
        m0 = _mm_add_epi32(m0, m2);

        m0 = _mm_srli_epi32(m0, 2);
        if(hasSSE41)
          m0 = _mm_packus_epi32(m0, m0);
        else
          m0 = _MM_PACKUS_EPI32(m0, m0);
      }
      _mm_storel_epi64((__m128i *)&pDst8[x], m0);
    }

    reinterpret_cast<pixel_t *>(pDst8)[nWidth - 1] = (reinterpret_cast<const pixel_t *>(pSrc8)[nWidth - 1] + reinterpret_cast<const pixel_t *>(pSrc8)[nWidth - 1 + nSrcPitch] + 1) >> 1;

    pSrc8 += nSrcPitch;
    pDst8 += nDstPitch;
  }

  for (int x = 0; x < nWidth; x += 8) {
    __m128i m0 = _mm_loadl_epi64((const __m128i *)&pSrc8[x]);
    __m128i m1 = _mm_loadl_epi64((const __m128i *)&pSrc8[x + 1]);

    if (sizeof(pixel_t) == 1)
      m0 = _mm_avg_epu8(m0, m1);
    else
      m0 = _mm_avg_epu16(m0, m1);
    _mm_storel_epi64((__m128i *)&pDst8[x], m0);
  }

  reinterpret_cast<pixel_t *>(pDst8)[nWidth - 1] = reinterpret_cast<const pixel_t *>(pSrc8)[nWidth - 1];
}

// so called Wiener interpolation. (sharp, similar to Lanczos ?)
// invarint simplified, 6 taps. Weights: (1, -5, 20, 20, -5, 1)/32 - added by Fizick
// 8-32 bits
template<typename pixel_t>
void VerticalWiener(unsigned char *pDst8, const unsigned char *pSrc8, int nDstPitch,
  int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel)
{
  pixel_t *pDst = reinterpret_cast<pixel_t *>(pDst8);
  const pixel_t *pSrc = reinterpret_cast<const pixel_t *>(pSrc8);

  nSrcPitch /= sizeof(pixel_t);
  nDstPitch /= sizeof(pixel_t);

  const int max_pixel_value = sizeof(pixel_t) == 1 ? 255 : (1 << bits_per_pixel) - 1;

  for (int j = 0; j < 2; j++)
  {
    for (int i = 0; i < nWidth; i++)
      if constexpr(sizeof(pixel_t) <= 2)
        pDst[i] = (pSrc[i] + pSrc[i + nSrcPitch] + 1) >> 1; // int
      else
        pDst[i] = (pSrc[i] + pSrc[i + nSrcPitch]) * 0.5f; // float
    pDst += nDstPitch;
    pSrc += nSrcPitch;
  }
  for (int j = 2; j < nHeight - 4; j++)
  {
    for (int i = 0; i < nWidth; i++)
    {
      if constexpr(sizeof(pixel_t) <= 2)
        pDst[i] = std::min(max_pixel_value, std::max(0,
        ((pSrc[i - nSrcPitch * 2])
          + (-(pSrc[i - nSrcPitch]) + (pSrc[i] << 2) + (pSrc[i + nSrcPitch] << 2) - (pSrc[i + nSrcPitch * 2])) * 5
          + (pSrc[i + nSrcPitch * 3]) + 16) >> 5));
      else
        pDst[i] =
        (
        (pSrc[i - nSrcPitch * 2])
          + (-(pSrc[i - nSrcPitch]) + (pSrc[i] * 4.0f) + (pSrc[i + nSrcPitch] * 4.0f) - (pSrc[i + nSrcPitch * 2])) * 5.0f // weight 30
          + (pSrc[i + nSrcPitch * 3])
          ) * (1.0f / 32.0f); // no clamp for float!
    }
    pDst += nDstPitch;
    pSrc += nSrcPitch;
  }
  for (int j = nHeight - 4; j < nHeight - 1; j++)
  {
    for (int i = 0; i < nWidth; i++)
    {
      if constexpr(sizeof(pixel_t) <= 2)
        pDst[i] = (pSrc[i] + pSrc[i + nSrcPitch] + 1) >> 1;
      else
        pDst[i] = (pSrc[i] + pSrc[i + nSrcPitch]) * 0.5f;
    }

    pDst += nDstPitch;
    pSrc += nSrcPitch;
  }
  // last row
  for (int i = 0; i < nWidth; i++)
    pDst[i] = pSrc[i];
}

// 8-32 bits
template<typename pixel_t>
void HorizontalWiener(unsigned char *pDst8, const unsigned char *pSrc8, int nDstPitch,
  int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel)
{
  pixel_t *pDst = reinterpret_cast<pixel_t *>(pDst8);
  const pixel_t *pSrc = reinterpret_cast<const pixel_t *>(pSrc8);

  nSrcPitch /= sizeof(pixel_t);
  nDstPitch /= sizeof(pixel_t);

  const int max_pixel_value = sizeof(pixel_t) == 1 ? 255 : (1 << bits_per_pixel) - 1;

  for (int j = 0; j < nHeight; j++)
  {
    if constexpr(sizeof(pixel_t) <= 2) {
      pDst[0] = (pSrc[0] + pSrc[1] + 1) >> 1;
      pDst[1] = (pSrc[1] + pSrc[2] + 1) >> 1;
    }
    else {
      // float
      pDst[0] = (pSrc[0] + pSrc[1]) * 0.5f;
      pDst[1] = (pSrc[1] + pSrc[2]) * 0.5f;
    }
    for (int i = 2; i < nWidth - 4; i++)
    {
      if constexpr(sizeof(pixel_t) <= 2)
        pDst[i] = std::min(max_pixel_value, std::max(0, ((pSrc[i - 2]) + (-(pSrc[i - 1]) + (pSrc[i] << 2)
          + (pSrc[i + 1] << 2) - (pSrc[i + 2])) * 5 + (pSrc[i + 3]) + 16) >> 5));
      else
        pDst[i] = ((pSrc[i - 2]) + (-(pSrc[i - 1]) + (pSrc[i] * 4.0f)
          + (pSrc[i + 1] * 4.0f) - (pSrc[i + 2])) * 5.0f + (pSrc[i + 3])) * (1.0f / 32.0f);
    }
    for (int i = nWidth - 4; i < nWidth - 1; i++)
      if constexpr(sizeof(pixel_t) <= 2)
        pDst[i] = (pSrc[i] + pSrc[i + 1] + 1) >> 1;
      else
        pDst[i] = (pSrc[i] + pSrc[i + 1]) * 0.5f;

    pDst[nWidth - 1] = pSrc[nWidth - 1];
    pDst += nDstPitch;
    pSrc += nSrcPitch;
  }
}

#if 0 // not used
template<typename pixel_t>
void DiagonalWiener(unsigned char *pDst8, const unsigned char *pSrc8, int nDstPitch,
  int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel)
{
  pixel_t *pDst = reinterpret_cast<pixel_t *>(pDst8);
  const pixel_t *pSrc = reinterpret_cast<const pixel_t *>(pSrc8);

  nSrcPitch /= sizeof(pixel_t);
  nDstPitch /= sizeof(pixel_t);

  const int max_pixel_value = sizeof(pixel_t) == 1 ? 255 : (1 << bits_per_pixel) - 1;

  for (int j = 0; j < 2; j++)
  {
    for (int i = 0; i < nWidth - 1; i++)
      pDst[i] = (pSrc[i] + pSrc[i + 1] + pSrc[i + nSrcPitch] + pSrc[i + nSrcPitch + 1] + 2) >> 2;

    pDst[nWidth - 1] = (pSrc[nWidth - 1] + pSrc[nWidth + nSrcPitch - 1] + 1) >> 1;
    pDst += nDstPitch;
    pSrc += nSrcPitch;
  }
  for (int j = 2; j < nHeight - 4; j++)
  {
    for (int i = 0; i < 2; i++)
      pDst[i] = (pSrc[i] + pSrc[i + 1] + pSrc[i + nSrcPitch] + pSrc[i + nSrcPitch + 1] + 2) >> 2;
    for (int i = 2; i < nWidth - 4; i++)
    {
      pDst[i] = std::min(max_pixel_value, std::max(0,
        ((pSrc[i - 2 - nSrcPitch * 2]) + (-(pSrc[i - 1 - nSrcPitch]) + (pSrc[i] << 2)
          + (pSrc[i + 1 + nSrcPitch] << 2) - (pSrc[i + 2 + nSrcPitch * 2] << 2)) * 5 + (pSrc[i + 3 + nSrcPitch * 3])
          + (pSrc[i + 3 - nSrcPitch * 2]) + (-(pSrc[i + 2 - nSrcPitch]) + (pSrc[i + 1] << 2)
            + (pSrc[i + nSrcPitch] << 2) - (pSrc[i - 1 + nSrcPitch * 2])) * 5 + (pSrc[i - 2 + nSrcPitch * 3])
          + 32) >> 6));
    }
    for (int i = nWidth - 4; i < nWidth - 1; i++)
      pDst[i] = (pSrc[i] + pSrc[i + 1] + pSrc[i + nSrcPitch] + pSrc[i + nSrcPitch + 1] + 2) >> 2;

    pDst[nWidth - 1] = (pSrc[nWidth - 1] + pSrc[nWidth + nSrcPitch - 1] + 1) >> 1;
    pDst += nDstPitch;
    pSrc += nSrcPitch;
  }
  for (int j = nHeight - 4; j < nHeight - 1; j++)
  {
    for (int i = 0; i < nWidth - 1; i++)
      pDst[i] = (pSrc[i] + pSrc[i + 1] + pSrc[i + nSrcPitch] + pSrc[i + nSrcPitch + 1] + 2) >> 2;

    pDst[nWidth - 1] = (pSrc[nWidth - 1] + pSrc[nWidth + nSrcPitch - 1] + 1) >> 1;
    pDst += nDstPitch;
    pSrc += nSrcPitch;
  }
  for (int i = 0; i < nWidth - 1; i++)
    pDst[i] = (pSrc[i] + pSrc[i + 1] + 1) >> 1;
  pDst[nWidth - 1] = pSrc[nWidth - 1];
}
#endif

// 8-32 bits
// bicubic (Catmull-Rom 4 taps interpolation)
template<typename pixel_t>
void VerticalBicubic(unsigned char *pDst8, const unsigned char *pSrc8, int nDstPitch,
  int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel)
{
  pixel_t *pDst = reinterpret_cast<pixel_t *>(pDst8);
  const pixel_t *pSrc = reinterpret_cast<const pixel_t *>(pSrc8);

  nSrcPitch /= sizeof(pixel_t);
  nDstPitch /= sizeof(pixel_t);

  const int max_pixel_value = sizeof(pixel_t) == 1 ? 255 : (1 << bits_per_pixel) - 1;

  for (int j = 0; j < 1; j++)
  {
    for (int i = 0; i < nWidth; i++)
      if constexpr(sizeof(pixel_t) <= 2)
        pDst[i] = (pSrc[i] + pSrc[i + nSrcPitch] + 1) >> 1; // int
      else
        pDst[i] = (pSrc[i] + pSrc[i + nSrcPitch]) * 0.5f; // float
    pDst += nDstPitch;
    pSrc += nSrcPitch;
  }
  for (int j = 1; j < nHeight - 3; j++)
  {
    for (int i = 0; i < nWidth; i++)
    {
      if constexpr(sizeof(pixel_t) <= 2)
        pDst[i] = std::min(max_pixel_value, std::max(0,
        (-pSrc[i - nSrcPitch] - pSrc[i + nSrcPitch * 2] + (pSrc[i] + pSrc[i + nSrcPitch]) * 9 + 8) >> 4));
      else
        pDst[i] = 
        (-pSrc[i - nSrcPitch] - pSrc[i + nSrcPitch * 2] + (pSrc[i] + pSrc[i + nSrcPitch]) * 9.0f) * (1.0f / 16.0f);
    }
    pDst += nDstPitch;
    pSrc += nSrcPitch;
  }
  for (int j = nHeight - 3; j < nHeight - 1; j++)
  {
    for (int i = 0; i < nWidth; i++)
    {
      if constexpr(sizeof(pixel_t) <= 2)
        pDst[i] = (pSrc[i] + pSrc[i + nSrcPitch] + 1) >> 1; // int
      else
        pDst[i] = (pSrc[i] + pSrc[i + nSrcPitch]) * 0.5f; // float
    }

    pDst += nDstPitch;
    pSrc += nSrcPitch;
  }
  // last row
  for (int i = 0; i < nWidth; i++)
    pDst[i] = pSrc[i];
}

// 8-32bits
template<typename pixel_t>
void HorizontalBicubic(unsigned char *pDst8, const unsigned char *pSrc8, int nDstPitch,
  int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel)
{
  pixel_t *pDst = reinterpret_cast<pixel_t *>(pDst8);
  const pixel_t *pSrc = reinterpret_cast<const pixel_t *>(pSrc8);

  nSrcPitch /= sizeof(pixel_t);
  nDstPitch /= sizeof(pixel_t);

  const int max_pixel_value = sizeof(pixel_t) == 1 ? 255 : (1 << bits_per_pixel) - 1;

  for (int j = 0; j < nHeight; j++)
  {
    if constexpr(sizeof(pixel_t) <= 2)
      pDst[0] = (pSrc[0] + pSrc[1] + 1) >> 1; // int
    else
      pDst[0] = (pSrc[0] + pSrc[1] + 1) * 0.5f; // float
    for (int i = 1; i < nWidth - 3; i++)
    {
      if constexpr(sizeof(pixel_t) <= 2)
        pDst[i] = std::min(max_pixel_value, std::max(0,
        (-(pSrc[i - 1] + pSrc[i + 2]) + (pSrc[i] + pSrc[i + 1]) * 9 + 8) >> 4));
      else
        pDst[i] = 
        (-(pSrc[i - 1] + pSrc[i + 2]) + (pSrc[i] + pSrc[i + 1]) * 9.0f) * (1.0f / 16.0f); // no clamp for float
    }
    for (int i = nWidth - 3; i < nWidth - 1; i++)
      if constexpr(sizeof(pixel_t) <= 2)
        pDst[i] = (pSrc[i] + pSrc[i + 1] + 1) >> 1; // int
      else
        pDst[i] = (pSrc[i] + pSrc[i + 1]) * 0.5f; // float

    pDst[nWidth - 1] = pSrc[nWidth - 1];
    pDst += nDstPitch;
    pSrc += nSrcPitch;
  }
}

#if 0
// not used
template<typename pixel_t>
void DiagonalBicubic(unsigned char *pDst8, const unsigned char *pSrc8, int nDstPitch,
  int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel)
{
  pixel_t *pDst = reinterpret_cast<pixel_t *>(pDst8);
  const pixel_t *pSrc = reinterpret_cast<const pixel_t *>(pSrc8);

  nSrcPitch /= sizeof(pixel_t);
  nDstPitch /= sizeof(pixel_t);

  const int max_pixel_value = sizeof(pixel_t) == 1 ? 255 : (1 << bits_per_pixel) - 1;

  for (int j = 0; j < 1; j++)
  {
    for (int i = 0; i < nWidth - 1; i++)
      pDst[i] = (pSrc[i] + pSrc[i + 1] + pSrc[i + nSrcPitch] + pSrc[i + nSrcPitch + 1] + 2) >> 2;

    pDst[nWidth - 1] = (pSrc[nWidth - 1] + pSrc[nWidth + nSrcPitch - 1] + 1) >> 1;
    pDst += nDstPitch;
    pSrc += nSrcPitch;
  }
  for (int j = 1; j < nHeight - 3; j++)
  {
    for (int i = 0; i < 1; i++)
      pDst[i] = (pSrc[i] + pSrc[i + 1] + pSrc[i + nSrcPitch] + pSrc[i + nSrcPitch + 1] + 2) >> 2;
    for (int i = 1; i < nWidth - 3; i++)
    {
      pDst[i] = std::min(max_pixel_value, std::max(0,
        (-pSrc[i - 1 - nSrcPitch] - pSrc[i + 2 + nSrcPitch * 2] + (pSrc[i] + pSrc[i + 1 + nSrcPitch]) * 9
          - pSrc[i - 1 + nSrcPitch * 2] - pSrc[i + 2 - nSrcPitch] + (pSrc[i + 1] + pSrc[i + nSrcPitch]) * 9
          + 16) >> 5));
    }
    for (int i = nWidth - 3; i < nWidth - 1; i++)
      pDst[i] = (pSrc[i] + pSrc[i + 1] + pSrc[i + nSrcPitch] + pSrc[i + nSrcPitch + 1] + 2) >> 2;

    pDst[nWidth - 1] = (pSrc[nWidth - 1] + pSrc[nWidth + nSrcPitch - 1] + 1) >> 1;
    pDst += nDstPitch;
    pSrc += nSrcPitch;
  }
  for (int j = nHeight - 3; j < nHeight - 1; j++)
  {
    for (int i = 0; i < nWidth - 1; i++)
      pDst[i] = (pSrc[i] + pSrc[i + 1] + pSrc[i + nSrcPitch] + pSrc[i + nSrcPitch + 1] + 2) >> 2;

    pDst[nWidth - 1] = (pSrc[nWidth - 1] + pSrc[nWidth + nSrcPitch - 1] + 1) >> 1;
    pDst += nDstPitch;
    pSrc += nSrcPitch;
  }
  for (int i = 0; i < nWidth - 1; i++)
    pDst[i] = (pSrc[i] + pSrc[i + 1] + 1) >> 1;
  pDst[nWidth - 1] = pSrc[nWidth - 1];
}
#endif


template<typename pixel_t>
void Average2_sse2(unsigned char *pDst8, const unsigned char *pSrc1_8, const unsigned char *pSrc2_8,
  int nPitch, int nWidth, int nHeight) {
  for (int y = 0; y < nHeight; y++) {
    for (int x = 0; x < nWidth; x += 16) {
      __m128i m0 = _mm_loadu_si128((const __m128i *)&pSrc1_8[x]);
      __m128i m1 = _mm_loadu_si128((const __m128i *)&pSrc2_8[x]);

      if(sizeof(pixel_t) == 1)
        m0 = _mm_avg_epu8(m0, m1);
      else // uint16_t
        m0 = _mm_avg_epu16(m0, m1);
      _mm_storeu_si128((__m128i *)&pDst8[x], m0);
    }

    pSrc1_8 += nPitch;
    pSrc2_8 += nPitch;
    pDst8 += nPitch;
  }
}

// 8-32 bits
template<typename pixel_t>
void Average2(unsigned char *pDst8, const unsigned char *pSrc1_8, const unsigned char *pSrc2_8,
  int nPitch, int nWidth, int nHeight)
{ // assume all pitches equal

  pixel_t *pDst = reinterpret_cast<pixel_t *>(pDst8);
  const pixel_t *pSrc1 = reinterpret_cast<const pixel_t *>(pSrc1_8);
  const pixel_t *pSrc2 = reinterpret_cast<const pixel_t *>(pSrc2_8);

  nPitch /= sizeof(pixel_t);

  for (int j = 0; j < nHeight; j++)
  {
    for (int i = 0; i < nWidth; i++)
      if constexpr(sizeof(pixel_t) <= 2)
        pDst[i] = (pSrc1[i] + pSrc2[i] + 1) >> 1;
      else
        pDst[i] = (pSrc1[i] + pSrc2[i]) * 0.5f;

    pDst += nPitch;
    pSrc1 += nPitch;
    pSrc2 += nPitch;
  }
}


// instantiate templates defined in cpp
template void VerticalBilin<uint8_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeigh, int bits_per_pixelt);
template void VerticalBilin<uint16_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template void VerticalBilin<float>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);

template void VerticalBilin_sse2<uint8_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeigh, int bits_per_pixelt);
template void VerticalBilin_sse2<uint16_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);

template void HorizontalBilin<uint8_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template void HorizontalBilin<uint16_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template void HorizontalBilin<float>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);

template void HorizontalBilin_sse2<uint8_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template void HorizontalBilin_sse2<uint16_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);

template void DiagonalBilin<uint8_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template void DiagonalBilin<uint16_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template void DiagonalBilin<float>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);

template void DiagonalBilin_sse2<uint8_t, 0>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template void DiagonalBilin_sse2<uint16_t, 0>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template void DiagonalBilin_sse2<uint16_t, 1>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);

template void RB2F<uint8_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int y_beg, int y_end, int cpuFlags);
template void RB2F<uint16_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int y_beg, int y_end, int cpuFlags);
template void RB2F<float>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int y_beg, int y_end, int cpuFlags);

template void RB2Filtered<uint8_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int y_beg, int y_end, int cpuFlags);
template void RB2Filtered<uint16_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int y_beg, int y_end, int cpuFlags);
template void RB2Filtered<float>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int y_beg, int y_end, int cpuFlags);

template void RB2BilinearFiltered<uint8_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int y_beg, int y_end, int cpuFlags);
template void RB2BilinearFiltered<uint16_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int y_beg, int y_end, int cpuFlags);
template void RB2BilinearFiltered<float>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int y_beg, int y_end, int cpuFlags);

template void RB2Quadratic<uint8_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int y_beg, int y_end, int cpuFlags);
template void RB2Quadratic<uint16_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int y_beg, int y_end, int cpuFlags);
template void RB2Quadratic<float>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int y_beg, int y_end, int cpuFlags);

template void RB2Cubic<uint8_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int y_beg, int y_end, int cpuFlags);
template void RB2Cubic<uint16_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int y_beg, int y_end, int cpuFlags);
template void RB2Cubic<float>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int y_beg, int y_end, int cpuFlags);

template void VerticalWiener<uint8_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template void VerticalWiener<uint16_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template void VerticalWiener<float>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);

template void HorizontalWiener<uint8_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template void HorizontalWiener<uint16_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template void HorizontalWiener<float>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);

#if 0 // not used
template void DiagonalWiener<uint8_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template void DiagonalWiener<uint16_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
#endif

template void VerticalBicubic<uint8_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template void VerticalBicubic<uint16_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template void VerticalBicubic<float>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);

template void HorizontalBicubic<uint8_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template void HorizontalBicubic<uint16_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template void HorizontalBicubic<float>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);

#if 0 // not used
template void DiagonalBicubic<uint8_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template void DiagonalBicubic<uint16_t>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template void DiagonalBicubic<float>(unsigned char *pDst, const unsigned char *pSrc, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
#endif

template void Average2<uint8_t>(unsigned char *pDst, const unsigned char *pSrc1, const unsigned char *pSrc2, int nPitch, int nWidth, int nHeight);
template void Average2<uint16_t>(unsigned char *pDst, const unsigned char *pSrc1, const unsigned char *pSrc2, int nPitch, int nWidth, int nHeight);
template void Average2<float>(unsigned char *pDst, const unsigned char *pSrc1, const unsigned char *pSrc2, int nPitch, int nWidth, int nHeight);

template void Average2_sse2<uint8_t>(unsigned char *pDst, const unsigned char *pSrc1, const unsigned char *pSrc2, int nPitch, int nWidth, int nHeight);
template void Average2_sse2<uint16_t>(unsigned char *pDst, const unsigned char *pSrc1, const unsigned char *pSrc2, int nPitch, int nWidth, int nHeight);

