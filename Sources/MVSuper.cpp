// MVTools v2

// Copyright(c)2008 A.G.Balakhnin aka Fizick
// Prepare super clip (hierachical levels data) for MVAnalyse

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

#include	"debugprintf.h"
#include "ClipFnc.h"
#include "MVFrame.h"
#include "MVGroupOfFrames.h"
#include "MVPlane.h"
#include "MVSuper.h"
#include	"profile.h"
#include "SuperParams.h"
#include <stdint.h>
#include "CopyCode.h"

#include <cmath>

// move profile init here from MVCore.cpp (it is not quite correct for several mvsuper)
#ifdef MOTION_PROFILE
int ProfTimeTable[MOTION_PROFILE_COUNT];
int ProfResults[MOTION_PROFILE_COUNT * 2];
int64_t ProfCumulatedResults[MOTION_PROFILE_COUNT * 2];
#endif


MVSuper::MVSuper(
  PClip _child, int _hPad, int _vPad, int _pel, int _levels, bool _chroma,
  int _sharp, int _rfilter, PClip _pelclip, bool _isse,
  bool mt_flag, bool yuy2_source, int _edgepad, int _edgepadV, IScriptEnvironment* env
)
  : GenericVideoFilter(_child)
  , pelclip(_pelclip)
  , _mt_flag(mt_flag)
{
  has_at_least_v8 = true;
  try { env->CheckVersion(8); }
  catch (const AvisynthError&) { has_at_least_v8 = false; }


  nWidth = vi.width;

  nHeight = vi.height;

  // 2.7.35 planar RGB support
  if (!ClipFnc::is_yuv_any(vi) && !vi.IsPlanarRGB() && !vi.IsPlanarRGBA()) // YUY2 is also YUV but let's see what is supported
  {
    env->ThrowError("MSuper: Clip must be Y(A), YUV(A), YUY2 or planar RGB(A)");
  }

  nPel = _pel;
  if ((nPel != 1) && (nPel != 2) && (nPel != 4))
  {
    env->ThrowError("MSuper: pel has to be 1 or 2 or 4");
  }

  nHPad = _hPad;
  nVPad = _vPad;
  rfilter = _rfilter;
  sharp = _sharp; // pel2 interpolation type
  cpuFlags = _isse ? env->GetCPUFlags() : 0;

  chroma = _chroma;
  if (vi.IsRGB())
    chroma = true; // Chroma means: all channels, for RGB: compulsory
  if (ClipFnc::is_grey(vi))
    chroma = false; // Grey support, no U V plane usage

  nModeYUV = chroma ? YUVPLANES : YPLANE;

  pixelType = vi.pixel_type;
  if (!ClipFnc::is_grey(vi) && !vi.IsRGB()) {
    yRatioUV = (1 << vi.GetPlaneHeightSubsampling(PLANAR_U));
    xRatioUV = (1 << vi.GetPlaneWidthSubsampling(PLANAR_U)); // for YV12 and YUY2, really do not used and assumed to 2
  }
  else {
    yRatioUV = 1; // n/a
    xRatioUV = 1; // n/a
  }
  pixelsize = vi.ComponentSize();
  bits_per_pixel = vi.BitsPerComponent();

  // The chroma planes get hpad / xRatioUV and vpad / yRatioUV padding (MVFrame). Not divisible, it would be rounded
  // down: the luma vectors are limited by the luma padding, a vector reaching +-hpad (+-vpad) scaled to chroma would
  // then read beyond the chroma padding (neighbour row, or the super frame area outside the plane).
  if (chroma && (nHPad % xRatioUV != 0 || nVPad % yRatioUV != 0))
    env->ThrowError("MSuper: hpad and vpad must be multiples of the chroma subsampling (%dx%d), e.g. even hpad for 4:2:0 and 4:2:2, even vpad for 4:2:0",
      xRatioUV, yRatioUV);

  // Edge padding (edgepad, edgepadV).
  // Motion analysis works on whole blocks. When (width - overlap) is not a multiple of (blksize - overlap), the
  // block grid ends before the right frame edge and the remaining columns (rows) are not processed (#28).
  // A block grid covering them reaches beyond the frame by at most (blksize - overlap - 1) pixels, and a search
  // around such a block needs the usual hpad / vpad beyond that. So the super planes get a wider right / bottom
  // border: the super is laid out for the frame size plus the edge padding (the "layout" size), every level and
  // sub-pel plane is built from the real frame exactly as without edgepad, and its padding (repeated edge pixels)
  // then reaches over the edge padding area too (MVPlane). Readers get the real size and the padding from
  // SuperParams. Inside the hpad / vpad border the super is the same as without edgepad.
  // 64: the largest blksize, so any blksize / overlap fits. Rounded up to whole chroma pixels.
  if (_edgepad < 0 || _edgepad > 64 || _edgepadV < 0 || _edgepadV > 64)
    env->ThrowError("MSuper: edgepad and edgepadV must be between 0 and 64");
  nEdgePadX = AlignNumber(_edgepad, xRatioUV);
  nEdgePadY = AlignNumber(_edgepadV, yRatioUV);
  nRealWidth = nWidth;
  nRealHeight = nHeight;
  // from here nWidth / nHeight are the layout size
  nWidth += nEdgePadX;
  nHeight += nEdgePadY;

  nLevels = _levels;
  int nLevelsMax = 0;
  // from the real size: the same levels as without edgepad
  while (PlaneHeightLuma(nRealHeight, nLevelsMax, yRatioUV, nVPad) >= yRatioUV * 2 &&
    PlaneWidthLuma(nRealWidth, nLevelsMax, xRatioUV, nHPad) >= xRatioUV * 2) // at last two pixels width and height of chroma
  {
    nLevelsMax++;
  }
  if (nLevels <= 0 || nLevels > nLevelsMax) nLevels = nLevelsMax;

  usePelClip = false;
  if (pelclip && (nPel >= 2))
  {
    if (pelclip->GetVideoInfo().width == vi.width*nPel &&
      pelclip->GetVideoInfo().height == vi.height*nPel)
    {
      usePelClip = true;
      isPelClipPadded = false;
    }
    else if (pelclip->GetVideoInfo().width == (vi.width + nHPad * 2)*nPel &&
      pelclip->GetVideoInfo().height == (vi.height + nVPad * 2)*nPel)
    {
      usePelClip = true;
      isPelClipPadded = true;
    }
    else
    {
      env->ThrowError("MSuper: pelclip frame size must be Pel of source!");
    }
    // A padded pelclip is copied with its border as it is, its border cannot be widened. An unpadded pelclip is
    // copied over the real area and padded like the source.
    if (isPelClipPadded && (nEdgePadX > 0 || nEdgePadY > 0))
      env->ThrowError("MSuper: with edgepad, pelclip must not be padded");
  }

  nSuperWidth = nWidth + 2 * nHPad;
  if (nSuperWidth % xRatioUV != 0) // 4:1:1, 4:1:0 with odd hpad: the frame width must be a multiple of 4
    env->ThrowError("MSuper: hpad must be even for 4:1:1 and 4:1:0 (width + 2 * hpad must be a multiple of 4)");
  nSuperHeight = PlaneSuperOffset(false, nHeight, nLevels, nPel, nVPad, nSuperWidth*pixelsize, yRatioUV) / (nSuperWidth*pixelsize);
  // height must be a multiple of the vertical chroma subsampling (4:2:0, 4:4:0: 2, 4:1:0: 4)
  nSuperHeight = (nSuperHeight + yRatioUV - 1) / yRatioUV * yRatioUV;
  vi.width = nSuperWidth;
  vi.height = nSuperHeight;


  SuperParams params;

  params.nWidth = nRealWidth;
  params.nHeight = nRealHeight;
  params.nHPad = nHPad;
  params.nVPad = nVPad;
  params.nPel = nPel;
  params.nModeYUV = nModeYUV;
  params.nLevels = nLevels;
  params.param = yuy2_source ? SUPER_PARAM_YUY2_SOURCE : 0;
  params.nEdgePadX = nEdgePadX;
  params.nEdgePadY = nEdgePadY;

  // parameters to the fake audio properties, kills audio
  SetSuperParams(vi, params, env);

  // LDS: why not nModeYUV?
//	pSrcGOF = new MVGroupOfFrames(nLevels, nWidth, nHeight, nPel, nHPad, nVPad, nModeYUV, isse, yRatioUV, mt_flag);
  pSrcGOF = new MVGroupOfFrames(nLevels, nWidth, nHeight, nPel, nHPad, nVPad, YUVPLANES, cpuFlags, xRatioUV, yRatioUV, pixelsize, bits_per_pixel, mt_flag, nRealWidth, nRealHeight);

  pSrcGOF->set_interp(nModeYUV, rfilter, sharp);

  PROFILE_INIT();
}

MVSuper::~MVSuper()
{
  delete pSrcGOF;

  PROFILE_SHOW();
}

PVideoFrame __stdcall MVSuper::GetFrame(int n, IScriptEnvironment* env)
{
  const unsigned char *pSrc[3];
  unsigned char *pDst[3];
  const unsigned char *pSrcPel[3];
  int nSrcPitch[3];
  int nDstPitch[3];
  int nSrcPelPitch[3];
  
  int planecount;
  
    //DebugPrintf("MSuper: Get src frame %d clip %d",n,child);

  PVideoFrame src = child->GetFrame(n, env);
  PVideoFrame srcPel;
  if (usePelClip)
    srcPel = pelclip->GetFrame(n, env);

  PVideoFrame dst = has_at_least_v8 ? env->NewVideoFrameP(vi, &src) : env->NewVideoFrame(vi); // frame property support

  int planes_y[4] = { PLANAR_Y, PLANAR_U, PLANAR_V, PLANAR_A };
  int planes_r[4] = { PLANAR_G, PLANAR_B, PLANAR_R, PLANAR_A };
  int *planes = (vi.IsYUV() || vi.IsYUVA()) ? planes_y : planes_r;
  planecount = ClipFnc::nbr_planes_processed(vi);

  for (int p = 0; p < planecount; ++p) {
    const int plane = planes[p];
    pSrc[p] = src->GetReadPtr(plane);
    nSrcPitch[p] = src->GetPitch(plane);
    if (usePelClip)
    {
      pSrcPel[p] = srcPel->GetReadPtr(plane);
      nSrcPelPitch[p] = srcPel->GetPitch(plane);
    }
    pDst[p] = dst->GetWritePtr(plane);
    nDstPitch[p] = dst->GetPitch(plane);
  }
  // P.F. 170519 debug: clear super area
  // todo: check how it affects for non-modulo blocksize vs. supersize_padded
  // maybe some garbage is read outside the area by MAnalyse later?
  /*
  MemZoneSet(pDstY, 0, dst->GetRowSize(PLANAR_Y_ALIGNED), vi.height, 0, 0, nDstPitchY);
  if (!ClipFnc::is_grey(vi)) {
    MemZoneSet(pDstU, 0, dst->GetRowSize(PLANAR_U_ALIGNED), vi.height >> vi.GetPlaneHeightSubsampling(PLANAR_U), 0, 0, nDstPitchUV);
    MemZoneSet(pDstV, 0, dst->GetRowSize(PLANAR_V_ALIGNED), vi.height >> vi.GetPlaneHeightSubsampling(PLANAR_V), 0, 0, nDstPitchUV);
  }
  */


  PROFILE_START(MOTION_PROFILE_INTERPOLATION);

  pSrcGOF->Update(YUVPLANES, pDst[0], nDstPitch[0], pDst[1], nDstPitch[1], pDst[2], nDstPitch[2]);
  // constant name is Y U and V but for MVFrame this is good for RGB
  const MVPlaneSet mvplanes[3] = { YPLANE, UPLANE, VPLANE };

  for (int p = 0; p < planecount; ++p) {
    const MVPlaneSet plane = mvplanes[p];
    pSrcGOF->SetPlane(pSrc[p], nSrcPitch[p], plane);
  }

  // level 0 is padded first: with odd (chroma) dimensions the reduction to level 1 reads its padding
  pSrcGOF->Pad(nModeYUV);
  pSrcGOF->Reduce(nModeYUV);

  if (usePelClip)
  {
    MVFrame *srcFrames = pSrcGOF->GetFrame(0);

    for (int p = 0; p < planecount; ++p) {
      const MVPlaneSet plane = mvplanes[p];
      MVPlane *srcPlane = srcFrames->GetPlane(plane);

      if (nModeYUV & plane) {
        if (pixelsize == 1)
          srcPlane->RefineExt<uint8_t>(pSrcPel[p], nSrcPelPitch[p], isPelClipPadded);
        else if (pixelsize == 2)
          srcPlane->RefineExt<uint16_t>(pSrcPel[p], nSrcPelPitch[p], isPelClipPadded);
        else
          srcPlane->RefineExt<float>(pSrcPel[p], nSrcPelPitch[p], isPelClipPadded);
      }
    }
  }
  else
  {
    pSrcGOF->Refine(nModeYUV);
  }

  PROFILE_STOP(MOTION_PROFILE_INTERPOLATION);


  /*

  */

  PROFILE_CUMULATE();

  return dst;
}
