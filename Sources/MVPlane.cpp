// Author: Manao
// Copyright(c)2006 A.G.Balakhnin aka Fizick - bicubic, wiener
// See legal notice in Copying.txt for more information
//
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

/******************************************************************************
*                                                                             *
*  MVPlane : manages a single plane, allowing padding and refining            *
*                                                                             *
******************************************************************************/

/*
About the parallelisation of the refine operation:
We have a lot of dependencies between the planes that kills the parallelism.
A better optimisation would be slicing each plane. However this requires a
significant modification of the assembly code to handle correctly the plane
boundaries.
*/


#include "CopyCode.h"
#include "Interpolation.h"
#ifdef INTEL_INTRINSICS
#include "Interpolation_avx2.h"
#endif
#ifdef NEON_INTRINSICS
#include "Interpolation_neon.h"
#endif
#include "MVPlane.h"
#include "Padding.h"
#include <stdint.h>
#include <cstring>
#include <algorithm>
#include <commonfunctions.h>


MVPlane::MVPlane(int _nWidth, int _nHeight, int _nPel, int _nHPad, int _nVPad, int _pixelsize, int _bits_per_pixel, int _cpuFlags, bool mt_flag,
  int _nRealWidth, int _nRealHeight)
  : pPlane(new uint8_t*[_nPel * _nPel * _pixelsize])
  , nWidth(_nWidth)
  , nHeight(_nHeight)
  , nPitch(0)
  , nHPadding(_nHPad)
  , nVPadding(_nVPad)
  , nOffsetPadding(0)
  , nHPaddingPel(_nHPad * _nPel)
  , nVPaddingPel(_nVPad * _nPel)
  , nExtendedWidth(_nWidth + 2 * _nHPad)
  , nExtendedHeight(_nHeight + 2 * _nVPad)
  , nRealWidth(_nRealWidth < 0 ? _nWidth : _nRealWidth)
  , nRealHeight(_nRealHeight < 0 ? _nHeight : _nRealHeight)
  , nRefineWidth(nRealWidth + 2 * _nHPad)
  , nRefineHeight(nRealHeight + 2 * _nVPad)
  , nPel(_nPel)
  , pixelsize(_pixelsize)
  , pixelsize_shift((_pixelsize == 1) ? 0 : (_pixelsize == 2 ? 1 : 2)) // pixelsize 1/2/4: shift 0/1/2
  , bits_per_pixel(_bits_per_pixel)
  , nSharp(2)
  , cpuFlags(_cpuFlags)
  , _mt_flag(mt_flag)
  , isPadded(false)
  , isRefined(false)
  , isFilled(false)
  , _sched_refine(mt_flag)
  , _plan_refine()
  , _slicer_reduce(mt_flag)
  , _redp_ptr(0)
{
  if (pixelsize == 1) {
    _bilin_hor_ptr = HorizontalBilin<uint8_t>;
    _bilin_ver_ptr = VerticalBilin<uint8_t>;
    _bilin_dia_ptr = DiagonalBilin<uint8_t>;
    _bicubic_hor_ptr = HorizontalBicubic<uint8_t>;
    _bicubic_ver_ptr = VerticalBicubic<uint8_t>;
    _wiener_hor_ptr = HorizontalWiener<uint8_t>;
    _wiener_ver_ptr = VerticalWiener<uint8_t>;
    _average_ptr = Average2<uint8_t>;
    _reduce_ptr = &RB2BilinearFiltered<uint8_t>;
  }
  else if (pixelsize == 2) {
    _bilin_hor_ptr = HorizontalBilin<uint16_t>;
    _bilin_ver_ptr = VerticalBilin<uint16_t>;
    _bilin_dia_ptr = DiagonalBilin<uint16_t>;
    _bicubic_hor_ptr = HorizontalBicubic<uint16_t>;
    _bicubic_ver_ptr = VerticalBicubic<uint16_t>;
    _wiener_hor_ptr = HorizontalWiener<uint16_t>;
    _wiener_ver_ptr = VerticalWiener<uint16_t>;
    _average_ptr = Average2<uint16_t>;
    _reduce_ptr = &RB2BilinearFiltered<uint16_t>;
  }
  else {
    _bilin_hor_ptr = HorizontalBilin<float>;
    _bilin_ver_ptr = VerticalBilin<float>;
    _bilin_dia_ptr = DiagonalBilin<float>;
    _bicubic_hor_ptr = HorizontalBicubic<float>;
    _bicubic_ver_ptr = VerticalBicubic<float>;
    _wiener_hor_ptr = HorizontalWiener<float>;
    _wiener_ver_ptr = VerticalWiener<float>;
    _average_ptr = Average2<float>;
    _reduce_ptr = &RB2BilinearFiltered<float>;
  }

#ifdef INTEL_INTRINSICS
  if (cpuFlags & CPUF_SSE2) {
    if (pixelsize == 1) {
      _bilin_hor_ptr = HorizontalBilin_sse2<uint8_t>;
      _bilin_ver_ptr = VerticalBilin_sse2<uint8_t>;
      _bilin_dia_ptr = DiagonalBilin_sse2<uint8_t>;
      _bicubic_hor_ptr = HorizontalBicubic_sse2<uint8_t>;
      _bicubic_ver_ptr = VerticalBicubic_sse2<uint8_t>;
      _wiener_hor_ptr = HorizontalWiener_sse2<uint8_t>;
      _wiener_ver_ptr = VerticalWiener_sse2<uint8_t>;
      _average_ptr = Average2_sse2<uint8_t>;
    }
    else if (pixelsize == 2) {
      _bilin_hor_ptr = HorizontalBilin_sse2<uint16_t>;
      _bilin_ver_ptr = VerticalBilin_sse2<uint16_t>;
      _bilin_dia_ptr = DiagonalBilin_sse2<uint16_t>;
      _bicubic_hor_ptr = HorizontalBicubic_sse2<uint16_t>;
      _bicubic_ver_ptr = VerticalBicubic_sse2<uint16_t>;
      _wiener_hor_ptr = HorizontalWiener_sse2<uint16_t>;
      _wiener_ver_ptr = VerticalWiener_sse2<uint16_t>;
      _average_ptr = Average2_sse2<uint16_t>;
    }
  }
  if (cpuFlags & CPUF_AVX2) {
    if (pixelsize == 1) {
      _bicubic_hor_ptr = HorizontalBicubic_avx2<uint8_t>;
      _bicubic_ver_ptr = VerticalBicubic_avx2<uint8_t>;
      _wiener_hor_ptr = HorizontalWiener_avx2<uint8_t>;
      _wiener_ver_ptr = VerticalWiener_avx2<uint8_t>;
    }
    else if (pixelsize == 2) {
      _bicubic_hor_ptr = HorizontalBicubic_avx2<uint16_t>;
      _bicubic_ver_ptr = VerticalBicubic_avx2<uint16_t>;
      _wiener_hor_ptr = HorizontalWiener_avx2<uint16_t>;
      _wiener_ver_ptr = VerticalWiener_avx2<uint16_t>;
    }
  }
#endif
#ifdef NEON_INTRINSICS
  if (cpuFlags & CPUF_ARM_NEON) {
    if (pixelsize == 1) {
      _bilin_hor_ptr = HorizontalBilin_neon<uint8_t>;
      _bilin_ver_ptr = VerticalBilin_neon<uint8_t>;
      _bilin_dia_ptr = DiagonalBilin_neon<uint8_t>;
      _bicubic_hor_ptr = HorizontalBicubic_neon<uint8_t>;
      _bicubic_ver_ptr = VerticalBicubic_neon<uint8_t>;
      _wiener_hor_ptr = HorizontalWiener_neon<uint8_t>;
      _wiener_ver_ptr = VerticalWiener_neon<uint8_t>;
      _average_ptr = Average2_neon<uint8_t>;
    }
    else if (pixelsize == 2) {
      _bilin_hor_ptr = HorizontalBilin_neon<uint16_t>;
      _bilin_ver_ptr = VerticalBilin_neon<uint16_t>;
      _bilin_dia_ptr = DiagonalBilin_neon<uint16_t>;
      _bicubic_hor_ptr = HorizontalBicubic_neon<uint16_t>;
      _bicubic_ver_ptr = VerticalBicubic_neon<uint16_t>;
      _wiener_hor_ptr = HorizontalWiener_neon<uint16_t>;
      _wiener_ver_ptr = VerticalWiener_neon<uint16_t>;
      _average_ptr = Average2_neon<uint16_t>;
    }
  }
#endif
}



MVPlane::~MVPlane()
{
  delete[] pPlane;
  pPlane = 0;
}



void MVPlane::set_interp(int rfilter, int sharp)
{
  nSharp = sharp; // for pel>1
  MV_UNUSED(nRfilter); // not used

  switch (rfilter)
  {
  case 0: _reduce_ptr = (pixelsize == 1) ? &RB2F<uint8_t> : (pixelsize == 2 ? &RB2F<uint16_t> : &RB2F<float>); break;
  case 1: _reduce_ptr = (pixelsize == 1) ? &RB2Filtered<uint8_t> : (pixelsize == 2 ? &RB2Filtered<uint16_t> : &RB2Filtered<float>); break;
  case 2: _reduce_ptr = (pixelsize == 1) ? &RB2BilinearFiltered<uint8_t> : (pixelsize == 2 ? &RB2BilinearFiltered<uint16_t> : &RB2BilinearFiltered<float>); break;
  case 3: _reduce_ptr = (pixelsize == 1) ? &RB2Quadratic<uint8_t> : (pixelsize == 2 ? &RB2Quadratic<uint16_t> : &RB2Quadratic<float>); break;
  case 4: _reduce_ptr = (pixelsize == 1) ? &RB2Cubic<uint8_t> : (pixelsize == 2 ? &RB2Cubic<uint16_t> : &RB2Cubic<float>); break;
  default:
    assert(false);
    break;
  };

  _plan_refine.clear();
  if (nSharp == 0)
  {
    if (nPel == 2)
    {
      _plan_refine.add_dep(0, 1);
      _plan_refine.add_dep(0, 2);
      _plan_refine.add_dep(0, 3);
    }
    else if (nPel == 4)
    {
      _plan_refine.add_dep(0, 2);
      _plan_refine.add_dep(0, 8);
      _plan_refine.add_dep(0, 10);
    }
  }
  else	// nSharp == 1 or 2
  {
    if (nPel == 2)
    {
      _plan_refine.add_dep(0, 1);
      _plan_refine.add_dep(0, 2);
      _plan_refine.add_dep(2, 3);
    }
    else if (nPel == 4)
    {
      _plan_refine.add_dep(0, 2);
      _plan_refine.add_dep(0, 8);
      _plan_refine.add_dep(8, 10);
    }
  }
  if (nPel == 4)
  {
    _plan_refine.add_dep(0, 1);
    _plan_refine.add_dep(2, 1);
    _plan_refine.add_dep(0, 3);
    _plan_refine.add_dep(2, 3);
    _plan_refine.add_dep(0, 4);
    _plan_refine.add_dep(8, 4);
    _plan_refine.add_dep(0, 12);
    _plan_refine.add_dep(8, 12);
    _plan_refine.add_dep(8, 9);
    _plan_refine.add_dep(10, 9);
    _plan_refine.add_dep(8, 11);
    _plan_refine.add_dep(10, 11);
    _plan_refine.add_dep(2, 6);
    _plan_refine.add_dep(10, 6);
    _plan_refine.add_dep(2, 14);
    _plan_refine.add_dep(10, 14);
    _plan_refine.add_dep(4, 5);
    _plan_refine.add_dep(6, 5);
    _plan_refine.add_dep(4, 7);
    _plan_refine.add_dep(6, 7);
    _plan_refine.add_dep(12, 13);
    _plan_refine.add_dep(14, 13);
    _plan_refine.add_dep(12, 15);
    _plan_refine.add_dep(14, 15);
  }
}



void MVPlane::Update(uint8_t* pSrc, int _nPitch) //v2.0
{
  // npitch is pixelsize aware
  nPitch = _nPitch;

  nOffsetPadding = nPitch * nVPadding + (nHPadding << pixelsize_shift);

  for (int i = 0; i < nPel * nPel; i++)
  {
    pPlane[i] = pSrc + i * nPitch * nExtendedHeight;
  }

  ResetState();
}



void MVPlane::ChangePlane(const uint8_t *pNewPlane, int nNewPitch)
{
  if (!isFilled)
  {
    // noffsetPadding is pixelsize aware
    // the real area; the edgepad area is filled by Pad
    BitBlt(pPlane[0] + nOffsetPadding, nPitch, pNewPlane, nNewPitch, (nRealWidth << pixelsize_shift), nRealHeight);
    isFilled = true;
  }
}



void MVPlane::Pad()
{
  // npitch is pixelsize aware
  if (!isPadded)
  {
    pad_layout(0);
    isPadded = true;
  }
}



// Pads a (sub)plane from its real area: the left / top border is hpad / vpad, the right / bottom border also
// covers the edgepad area (layout size - real size). Without edgepad this is the usual symmetric padding.
void MVPlane::pad_layout(int subplane)
{
  const int hPadRight = nHPadding + nWidth - nRealWidth;
  const int vPadBottom = nVPadding + nHeight - nRealHeight;
  if (pixelsize == 1)
    Padding::PadReferenceFrame<uint8_t>(pPlane[subplane], nPitch, nHPadding, nVPadding, nRealWidth, nRealHeight, hPadRight, vPadBottom);
  else if (pixelsize == 2)
    Padding::PadReferenceFrame<uint16_t>(pPlane[subplane], nPitch, nHPadding, nVPadding, nRealWidth, nRealHeight, hPadRight, vPadBottom);
  else
    Padding::PadReferenceFrame<float>(pPlane[subplane], nPitch, nHPadding, nVPadding, nRealWidth, nRealHeight, hPadRight, vPadBottom);
}



// edgepad: the sub-pel planes are interpolated over the real area with its border (nRefineWidth x nRefineHeight),
// exactly as without edgepad. The rest up to the layout size with its border repeats their last column / row.
void MVPlane::extend_refined()
{
  if (nRefineWidth == nExtendedWidth && nRefineHeight == nExtendedHeight)
    return;
  for (int i = 1; i < nPel * nPel; i++)
  {
    uint8_t* p = pPlane[i];
    if (pixelsize == 1)
      extend_rows<uint8_t>(p);
    else if (pixelsize == 2)
      extend_rows<uint16_t>(p);
    else
      extend_rows<uint32_t>(p); // float: copied as bits
    const uint8_t* lastrow = p + (nRefineHeight - 1) * nPitch;
    for (int y = nRefineHeight; y < nExtendedHeight; y++)
      memcpy(p + y * nPitch, lastrow, nExtendedWidth << pixelsize_shift);
  }
}

// pel=4: the quarter-pel planes are averages of two neighbouring full / half-pel samples (refine_pel4). Where the
// neighbour is one pixel to the right (planes 3, 7, 11, 15) or below (12, 14), refine_pel4 leaves out the last
// column / row of the refined area, because the neighbour would be outside of it; planes 13 and 15 are averaged
// from 12 and 14, so their last row is built from those missing rows. Here these are computed with the neighbour
// clamped to the last refined pixel: the refined area ends in the hpad / vpad border, which repeats the frame
// edge pixel, so beyond it the plane would continue with the same value - the result is the exact average.
// Without this the values are whatever the frame buffer held. Without edgepad they are never read (the vector
// limits keep the blocks inside), with edgepad extend_refined copies them over the edge padding area, which the
// edge blocks do read.
// Same arithmetic as Average2: (a + b + 1) >> 1, float: (a + b) * 0.5f.
template<typename pixel_t>
void MVPlane::complete_pel4_edges()
{
  const int pitch = nPitch / sizeof(pixel_t);
  const int w = nRefineWidth;
  const int h = nRefineHeight;
  auto P = [&](int i) { return reinterpret_cast<pixel_t*>(pPlane[i]); };
  auto avg = [](pixel_t a, pixel_t b) -> pixel_t {
    if constexpr (sizeof(pixel_t) <= 2)
      return (pixel_t)((a + b + 1) >> 1);
    else
      return (a + b) * 0.5f;
  };
  // last column: dst[x] = avg(right[x + 1], left[x]), right[w] := right[w - 1]
  auto last_col = [&](int dst, int right, int left) {
    for (int y = 0; y < h; y++)
      P(dst)[y * pitch + w - 1] = avg(P(right)[y * pitch + w - 1], P(left)[y * pitch + w - 1]);
  };
  // last row: dst[y] = avg(below[y + 1], up[y]), below[h] := below[h - 1]
  auto last_row = [&](int dst, int below, int up) {
    const int o = (h - 1) * pitch;
    for (int x = 0; x < w; x++)
      P(dst)[o + x] = avg(P(below)[o + x], P(up)[o + x]);
  };
  last_col(3, 0, 2);
  last_col(7, 4, 6);
  last_col(11, 8, 10);
  last_row(12, 0, 8);
  last_row(14, 2, 10);
  // from the completed 12 and 14
  {
    const int o = (h - 1) * pitch;
    for (int x = 0; x < w; x++)
      P(13)[o + x] = avg(P(12)[o + x], P(14)[o + x]);
    for (int x = 0; x < w - 1; x++)
      P(15)[o + x] = avg(P(12)[o + x + 1], P(14)[o + x]);
  }
  last_col(15, 12, 14);
}

// extend_refined: the columns from nRefineWidth to nExtendedWidth repeat the last interpolated column, row by row
template<typename pixel_t>
void MVPlane::extend_rows(uint8_t* p)
{
  for (int y = 0; y < nRefineHeight; y++)
  {
    pixel_t* row = reinterpret_cast<pixel_t*>(p + y * nPitch);
    std::fill(row + nRefineWidth, row + nExtendedWidth, row[nRefineWidth - 1]);
  }
}



void MVPlane::refine_start()
{
  if (!isRefined)
  {
    if (nPel == 2)
    {
      _sched_refine.start(_plan_refine, *this, &MVPlane::refine_pel2);
    }
    else if (nPel == 4)
    {
      _sched_refine.start(_plan_refine, *this, &MVPlane::refine_pel4);
    }
  }
}



void MVPlane::refine_wait()
{
  if (!isRefined)
  {
    if (nPel > 1)
    {
      _sched_refine.wait();
      if (nPel == 4)
      {
        if (pixelsize == 1)
          complete_pel4_edges<uint8_t>();
        else if (pixelsize == 2)
          complete_pel4_edges<uint16_t>();
        else
          complete_pel4_edges<float>();
      }
      extend_refined();
    }

    isRefined = true;
  }
}


// if a non-static template function is in cpp, we have to instantiate it
template void MVPlane::RefineExt<uint8_t>(const uint8_t *pSrc2x_8, int nSrc2xPitch, bool isExtPadded);
template void MVPlane::RefineExt<uint16_t>(const uint8_t *pSrc2x_8, int nSrc2xPitch, bool isExtPadded);
template void MVPlane::RefineExt<float>(const uint8_t *pSrc2x_8, int nSrc2xPitch, bool isExtPadded);

template<typename pixel_t>
void MVPlane::RefineExt(const uint8_t *pSrc2x_8, int nSrc2xPitch, bool isExtPadded) // copy from external upsized clip
{
  const pixel_t *pSrc2x = reinterpret_cast<const pixel_t *>(pSrc2x_8);
  nSrc2xPitch /= sizeof(pixel_t);

  if ((nPel == 2) && (!isRefined))
  {
    // pel clip may be already padded (i.e. is finest clip)
    int offset = isExtPadded ? 0 : nPitch * nVPadding / sizeof(pixel_t) + nHPadding;
    pixel_t* pp1 = reinterpret_cast<pixel_t *>(pPlane[1]) + offset;
    pixel_t* pp2 = reinterpret_cast<pixel_t *>(pPlane[2]) + offset;
    pixel_t* pp3 = reinterpret_cast<pixel_t *>(pPlane[3]) + offset;

    for (int h = 0; h < nRealHeight; h++) // assembler optimization?
    {
      for (int w = 0; w < nRealWidth; w++)
      {
        pp1[w] = pSrc2x[(w << 1) + 1];
        pp2[w] = pSrc2x[(w << 1) + nSrc2xPitch];
        pp3[w] = pSrc2x[(w << 1) + nSrc2xPitch + 1];
      }
      pp1 += nPitch / sizeof(pixel_t);
      pp2 += nPitch / sizeof(pixel_t);
      pp3 += nPitch / sizeof(pixel_t);
      pSrc2x += nSrc2xPitch * 2;
    }
    if (!isExtPadded)
    {
      pad_layout(1);
      pad_layout(2);
      pad_layout(3);
    }
    isPadded = true;
  }
  else if ((nPel == 4) && (!isRefined))
  {
    // pel clip may be already padded (i.e. is finest clip)
    int offset = isExtPadded ? 0 : nPitch * nVPadding / sizeof(pixel_t) + nHPadding;
    pixel_t* pp1 = reinterpret_cast<pixel_t *>(pPlane[1]) + offset;
    pixel_t* pp2 = reinterpret_cast<pixel_t *>(pPlane[2]) + offset;
    pixel_t* pp3 = reinterpret_cast<pixel_t *>(pPlane[3]) + offset;
    pixel_t* pp4 = reinterpret_cast<pixel_t *>(pPlane[4]) + offset;
    pixel_t* pp5 = reinterpret_cast<pixel_t *>(pPlane[5]) + offset;
    pixel_t* pp6 = reinterpret_cast<pixel_t *>(pPlane[6]) + offset;
    pixel_t* pp7 = reinterpret_cast<pixel_t *>(pPlane[7]) + offset;
    pixel_t* pp8 = reinterpret_cast<pixel_t *>(pPlane[8]) + offset;
    pixel_t* pp9 = reinterpret_cast<pixel_t *>(pPlane[9]) + offset;
    pixel_t* pp10 = reinterpret_cast<pixel_t *>(pPlane[10]) + offset;
    pixel_t* pp11 = reinterpret_cast<pixel_t *>(pPlane[11]) + offset;
    pixel_t* pp12 = reinterpret_cast<pixel_t *>(pPlane[12]) + offset;
    pixel_t* pp13 = reinterpret_cast<pixel_t *>(pPlane[13]) + offset;
    pixel_t* pp14 = reinterpret_cast<pixel_t *>(pPlane[14]) + offset;
    pixel_t* pp15 = reinterpret_cast<pixel_t *>(pPlane[15]) + offset;

    for (int h = 0; h < nRealHeight; h++) // assembler optimization?
    {
      for (int w = 0; w < nRealWidth; w++)
      {
        pp1[w] = pSrc2x[(w << 2) + 1];
        pp2[w] = pSrc2x[(w << 2) + 2];
        pp3[w] = pSrc2x[(w << 2) + 3];
        pp4[w] = pSrc2x[(w << 2) + nSrc2xPitch];
        pp5[w] = pSrc2x[(w << 2) + nSrc2xPitch + 1];
        pp6[w] = pSrc2x[(w << 2) + nSrc2xPitch + 2];
        pp7[w] = pSrc2x[(w << 2) + nSrc2xPitch + 3];
        pp8[w] = pSrc2x[(w << 2) + nSrc2xPitch * 2];
        pp9[w] = pSrc2x[(w << 2) + nSrc2xPitch * 2 + 1];
        pp10[w] = pSrc2x[(w << 2) + nSrc2xPitch * 2 + 2];
        pp11[w] = pSrc2x[(w << 2) + nSrc2xPitch * 2 + 3];
        pp12[w] = pSrc2x[(w << 2) + nSrc2xPitch * 3];
        pp13[w] = pSrc2x[(w << 2) + nSrc2xPitch * 3 + 1];
        pp14[w] = pSrc2x[(w << 2) + nSrc2xPitch * 3 + 2];
        pp15[w] = pSrc2x[(w << 2) + nSrc2xPitch * 3 + 3];
      }
      pp1 += nPitch / sizeof(pixel_t);
      pp2 += nPitch / sizeof(pixel_t);
      pp3 += nPitch / sizeof(pixel_t);
      pp4 += nPitch / sizeof(pixel_t);
      pp5 += nPitch / sizeof(pixel_t);
      pp6 += nPitch / sizeof(pixel_t);
      pp7 += nPitch / sizeof(pixel_t);
      pp8 += nPitch / sizeof(pixel_t);
      pp9 += nPitch / sizeof(pixel_t);
      pp10 += nPitch / sizeof(pixel_t);
      pp11 += nPitch / sizeof(pixel_t);
      pp12 += nPitch / sizeof(pixel_t);
      pp13 += nPitch / sizeof(pixel_t);
      pp14 += nPitch / sizeof(pixel_t);
      pp15 += nPitch / sizeof(pixel_t);
      pSrc2x += nSrc2xPitch * 4;
    }
    if (!isExtPadded)
    {
      pad_layout(1);
      pad_layout(2);
      pad_layout(3);
      pad_layout(4);
      pad_layout(5);
      pad_layout(6);
      pad_layout(7);
      pad_layout(8);
      pad_layout(9);
      pad_layout(10);
      pad_layout(11);
      pad_layout(12);
      pad_layout(13);
      pad_layout(14);
      pad_layout(15);
    }
    isPadded = true;
  }
  isRefined = true;
}



void MVPlane::reduce_start(MVPlane *pReducedPlane)
{
  if (!pReducedPlane->isFilled)
  {
    _redp_ptr = pReducedPlane;
    _slicer_reduce.start(pReducedPlane->nRealHeight, *this, &MVPlane::reduce_slice, 4);
  }
}



void	MVPlane::reduce_wait()
{
  assert(_redp_ptr != 0);

  if (!_redp_ptr->isFilled)
  {
    _slicer_reduce.wait();

    _redp_ptr->isFilled = true;
    _redp_ptr = 0;
  }
}



void MVPlane::WritePlane(FILE *pFile)
{
  // noffsetPadding is pixelsize aware
  for (int i = 0; i < nHeight; i++)
  {
    fwrite(pPlane[0] + i * nPitch + nOffsetPadding, 1, (nWidth << pixelsize_shift), pFile);
  }
}



void MVPlane::refine_pel2(SchedulerRefine::TaskData &td)
{
  assert(&td != 0);

  switch (td._task_index)
  {
  case 0:  break;	// Nothing on the root node
  case 1:
    switch (nSharp)
    {
    case 0: _bilin_hor_ptr(pPlane[1], pPlane[0], nPitch, nPitch, nRefineWidth, nRefineHeight, bits_per_pixel); break;
    case 1: _bicubic_hor_ptr(pPlane[1], pPlane[0], nPitch, nPitch, nRefineWidth, nRefineHeight, bits_per_pixel); break;
    default: _wiener_hor_ptr(pPlane[1], pPlane[0], nPitch, nPitch, nRefineWidth, nRefineHeight, bits_per_pixel); break;
    }
  case 2:
    switch (nSharp)
    {
    case 0: _bilin_ver_ptr(pPlane[2], pPlane[0], nPitch, nPitch, nRefineWidth, nRefineHeight, bits_per_pixel); break;
    case 1: _bicubic_ver_ptr(pPlane[2], pPlane[0], nPitch, nPitch, nRefineWidth, nRefineHeight, bits_per_pixel); break;
    default: _wiener_ver_ptr(pPlane[2], pPlane[0], nPitch, nPitch, nRefineWidth, nRefineHeight, bits_per_pixel); break;
    }
    break;
  case 3:
    switch (nSharp)
    {
    case 0: _bilin_dia_ptr(pPlane[3], pPlane[0], nPitch, nPitch, nRefineWidth, nRefineHeight, bits_per_pixel); break;
    case 1: _bicubic_hor_ptr(pPlane[3], pPlane[2], nPitch, nPitch, nRefineWidth, nRefineHeight, bits_per_pixel); break;	// faster from ready-made horizontal
    default: _wiener_hor_ptr(pPlane[3], pPlane[2], nPitch, nPitch, nRefineWidth, nRefineHeight, bits_per_pixel); break;	// faster from ready-made horizontal
    }
    break;
  default:
    assert(false);
    break;
  }
}



void MVPlane::refine_pel4(SchedulerRefine::TaskData &td)
{
  assert(&td != 0);

  switch (td._task_index)
  {
  case 0:  break;	// Nothing on the root node
  case 1:  _average_ptr(pPlane[1], pPlane[0], pPlane[2], nPitch, nRefineWidth, nRefineHeight); break;
  case 2:
    switch (nSharp)
    {
    case 0: _bilin_hor_ptr(pPlane[2], pPlane[0], nPitch, nPitch, nRefineWidth, nRefineHeight, bits_per_pixel); break;
    case 1: _bicubic_hor_ptr(pPlane[2], pPlane[0], nPitch, nPitch, nRefineWidth, nRefineHeight, bits_per_pixel); break;
    default: _wiener_hor_ptr(pPlane[2], pPlane[0], nPitch, nPitch, nRefineWidth, nRefineHeight, bits_per_pixel); break;
    }
    break;
  case 3:  _average_ptr(pPlane[3], pPlane[0] + 1 * pixelsize, pPlane[2], nPitch, nRefineWidth - 1, nRefineHeight); break;
  case 4:  _average_ptr(pPlane[4], pPlane[0], pPlane[8], nPitch, nRefineWidth, nRefineHeight); break;
  case 5:  _average_ptr(pPlane[5], pPlane[4], pPlane[6], nPitch, nRefineWidth, nRefineHeight); break;
  case 6:  _average_ptr(pPlane[6], pPlane[2], pPlane[10], nPitch, nRefineWidth, nRefineHeight); break;
  case 7:  _average_ptr(pPlane[7], pPlane[4] + 1 * pixelsize, pPlane[6], nPitch, nRefineWidth - 1, nRefineHeight); break;
  case 8:
    switch (nSharp)
    {
    case 0: _bilin_ver_ptr(pPlane[8], pPlane[0], nPitch, nPitch, nRefineWidth, nRefineHeight, bits_per_pixel); break;
    case 1: _bicubic_ver_ptr(pPlane[8], pPlane[0], nPitch, nPitch, nRefineWidth, nRefineHeight, bits_per_pixel); break;
    default: _wiener_ver_ptr(pPlane[8], pPlane[0], nPitch, nPitch, nRefineWidth, nRefineHeight, bits_per_pixel); break;
    }
    break;
  case 9:  _average_ptr(pPlane[9], pPlane[8], pPlane[10], nPitch, nRefineWidth, nRefineHeight); break;
  case 10:
    switch (nSharp)
    {
    case 0: _bilin_dia_ptr(pPlane[10], pPlane[0], nPitch, nPitch, nRefineWidth, nRefineHeight, bits_per_pixel); break;
    case 1: _bicubic_hor_ptr(pPlane[10], pPlane[8], nPitch, nPitch, nRefineWidth, nRefineHeight, bits_per_pixel); break;	// faster from ready-made horizontal
    default: _wiener_hor_ptr(pPlane[10], pPlane[8], nPitch, nPitch, nRefineWidth, nRefineHeight, bits_per_pixel); break;	// faster from ready-made horizontal
    }
    break;
  case 11: _average_ptr(pPlane[11], pPlane[8] + 1*pixelsize, pPlane[10], nPitch, nRefineWidth - 1, nRefineHeight); break;
  case 12: _average_ptr(pPlane[12], pPlane[0] + nPitch, pPlane[8], nPitch, nRefineWidth, nRefineHeight - 1); break;
  case 13: _average_ptr(pPlane[13], pPlane[12], pPlane[14], nPitch, nRefineWidth, nRefineHeight); break;
  case 14: _average_ptr(pPlane[14], pPlane[2] + nPitch, pPlane[10], nPitch, nRefineWidth, nRefineHeight - 1); break;
  case 15: _average_ptr(pPlane[15], pPlane[12] + 1 * pixelsize, pPlane[14], nPitch, nRefineWidth - 1, nRefineHeight); break;
  default:
    assert(false);
    break;
  }
}



void MVPlane::reduce_slice(SlicerReduce::TaskData &td)
{
  assert(&td != 0);
  assert(_redp_ptr != 0);
  // noffsetPadding is pixelsize aware
  MVPlane &red = *_redp_ptr; // target (smaller dimension)
  _reduce_ptr(
    red.pPlane[0] + red.nOffsetPadding, // shrink to
    pPlane[0] + nOffsetPadding, // shrink from
    red.nPitch, nPitch,
    red.nRealWidth, red.nRealHeight, td._y_beg, td._y_end, // the real area; Pad adds the edgepad area
    cpuFlags
  );
}
