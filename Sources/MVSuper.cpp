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
#include "MVFrame.h"
#include "MVGroupOfFrames.h"
#include "MVPlane.h"
#include "MVSuper.h"
#include	"profile.h"
#include "SuperParams64Bits.h"
#include <stdint.h>
#include "CopyCode.h"

#include <cmath>

// move profile init here from MVCore.cpp (it is not quite correct for several mvsuper)
#ifdef MOTION_PROFILE
int ProfTimeTable[MOTION_PROFILE_COUNT];
int ProfResults[MOTION_PROFILE_COUNT * 2];
__int64 ProfCumulatedResults[MOTION_PROFILE_COUNT * 2];
#endif


MVSuper::MVSuper(
  PClip _child, int _hPad, int _vPad, int _pel, int _levels, bool _chroma,
  int _sharp, int _rfilter, PClip _pelclip, bool _isse, bool _planar,
  bool mt_flag, IScriptEnvironment* env
)
  : GenericVideoFilter(_child)
  , pelclip(_pelclip)
  , _mt_flag(mt_flag)
{
  planar = _planar;

  nWidth = vi.width;

  nHeight = vi.height;

  // 2.7.35 planar RGB support
  if (!vi.IsYUV() && !vi.IsYUY2() && !vi.IsPlanarRGB() && !vi.IsPlanarRGBA()) // YUY2 is also YUV but let's see what is supported
  {
    env->ThrowError("MSuper: Clip must be YUV or YUY2 or planar RGB/RGBA");
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
  if (vi.IsY())
    chroma = false; // Grey support, no U V plane usage

  nModeYUV = chroma ? YUVPLANES : YPLANE;

  pixelType = vi.pixel_type;
  if (!vi.IsY() && !vi.IsRGB()) {
    yRatioUV = vi.IsYUY2() ? 1 : (1 << vi.GetPlaneHeightSubsampling(PLANAR_U));
    xRatioUV = vi.IsYUY2() ? 2 : (1 << vi.GetPlaneWidthSubsampling(PLANAR_U)); // for YV12 and YUY2, really do not used and assumed to 2
  }
  else {
    yRatioUV = 1; // n/a
    xRatioUV = 1; // n/a
  }
  pixelsize = vi.ComponentSize();
  bits_per_pixel = vi.BitsPerComponent();

  nLevels = _levels;
  int nLevelsMax = 0;
  while (PlaneHeightLuma(vi.height, nLevelsMax, yRatioUV, nVPad) >= yRatioUV * 2 &&
    PlaneWidthLuma(vi.width, nLevelsMax, xRatioUV, nHPad) >= xRatioUV * 2) // at last two pixels width and height of chroma
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
  }

  nSuperWidth = nWidth + 2 * nHPad;
  nSuperHeight = PlaneSuperOffset(false, nHeight, nLevels, nPel, nVPad, nSuperWidth*pixelsize, yRatioUV) / (nSuperWidth*pixelsize);
  if (yRatioUV == 2 && nSuperHeight & 1) nSuperHeight++; // even
  vi.width = nSuperWidth;
  vi.height = nSuperHeight;

  if ((pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2 && !planar)
  {
    SrcPlanes = new YUY2Planes(nWidth, nHeight);
    //		DstPlanes =  new YUY2Planes(nSuperWidth, nSuperHeight); // other size!
    if (usePelClip)
    {
      SrcPelPlanes = new YUY2Planes(pelclip->GetVideoInfo().width, pelclip->GetVideoInfo().height);
    }
  }

  SuperParams64Bits params;

  params.nHeight = nHeight;
  params.nHPad = nHPad;
  params.nVPad = nVPad;
  params.nPel = nPel;
  params.nModeYUV = nModeYUV;
  params.nLevels = nLevels;


  // pack parameters to fake audio properties
  memcpy(&vi.num_audio_samples, &params, 8); //nHeight + (nHPad<<16) + (nVPad<<24) + ((_int64)(nPel)<<32) + ((_int64)nModeYUV<<40) + ((_int64)nLevels<<48);
  vi.audio_samples_per_second = 0; // kill audio

  // LDS: why not nModeYUV?
//	pSrcGOF = new MVGroupOfFrames(nLevels, nWidth, nHeight, nPel, nHPad, nVPad, nModeYUV, isse, yRatioUV, mt_flag);
  pSrcGOF = new MVGroupOfFrames(nLevels, nWidth, nHeight, nPel, nHPad, nVPad, YUVPLANES, cpuFlags, xRatioUV, yRatioUV, pixelsize, bits_per_pixel, mt_flag);

  pSrcGOF->set_interp(nModeYUV, rfilter, sharp);

  PROFILE_INIT();
}

MVSuper::~MVSuper()
{
  if ((pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2 && !planar)
  {
    delete SrcPlanes;
    //		delete DstPlanes;
    if (usePelClip)
    {
      delete SrcPelPlanes;
    }
  }
  delete pSrcGOF;

  PROFILE_SHOW();
}

PVideoFrame __stdcall MVSuper::GetFrame(int n, IScriptEnvironment* env)
{
  const unsigned char *pSrc[3];
  unsigned char *pDst[3];
  const unsigned char *pSrcPel[3];
  //	unsigned char *pDstYUY2;
  int nSrcPitch[3];
  int nDstPitch[3];
  int nSrcPelPitch[3];
  
  int planecount;
  
    //DebugPrintf("MSuper: Get src frame %d clip %d",n,child);

  PVideoFrame src = child->GetFrame(n, env);
  PVideoFrame srcPel;
  if (usePelClip)
    srcPel = pelclip->GetFrame(n, env);

  PVideoFrame	dst = env->NewVideoFrame(vi);

  PROFILE_START(MOTION_PROFILE_YUY2CONVERT);
  if ((pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2)
  {
    if (!planar)
    {
      pSrc[0] = SrcPlanes->GetPtr();
      pSrc[1] = SrcPlanes->GetPtrU();
      pSrc[2] = SrcPlanes->GetPtrV();
      nSrcPitch[0] = SrcPlanes->GetPitch();
      nSrcPitch[1] = nSrcPitch[2] = SrcPlanes->GetPitchUV();
      YUY2ToPlanes(src->GetReadPtr(), src->GetPitch(), nWidth, nHeight,
        pSrc[0], nSrcPitch[0], pSrc[1], pSrc[1], nSrcPitch[1], cpuFlags);
      if (usePelClip)
      {
        pSrcPel[0] = SrcPelPlanes->GetPtr();
        pSrcPel[1] = SrcPelPlanes->GetPtrU();
        pSrcPel[2] = SrcPelPlanes->GetPtrV();
        nSrcPelPitch[0] = SrcPelPlanes->GetPitch();
        nSrcPelPitch[1] = nSrcPelPitch[2] = SrcPelPlanes->GetPitchUV();
        YUY2ToPlanes(srcPel->GetReadPtr(), srcPel->GetPitch(), srcPel->GetRowSize() / 2, srcPel->GetHeight(),
          pSrcPel[0], nSrcPelPitch[0], pSrcPel[1], pSrcPel[2], nSrcPelPitch[1], cpuFlags);
      }
    }
    else
    {
      pSrc[0] = src->GetReadPtr();
      pSrc[1] = pSrc[0] + src->GetRowSize() / 2;
      pSrc[2]= pSrc[1] + src->GetRowSize() / 4;
      nSrcPitch[0] = src->GetPitch();
      nSrcPitch[1] = nSrcPitch[2] = nSrcPitch[0];
      if (usePelClip)
      {
        pSrcPel[0] = srcPel->GetReadPtr();
        pSrcPel[1] = pSrcPel[0] + srcPel->GetRowSize() / 2;
        pSrcPel[2] = pSrcPel[1] + srcPel->GetRowSize() / 4;
        nSrcPelPitch[0] = srcPel->GetPitch();
        nSrcPelPitch[1] = nSrcPelPitch[2] = nSrcPelPitch[0];
      }
    }
    //		pDstY = DstPlanes->GetPtr();
    //		pDstU = DstPlanes->GetPtrU();
    //		pDstV = DstPlanes->GetPtrV();
    //		nDstPitchY  = DstPlanes->GetPitch();
    //		nDstPitchUV  = DstPlanes->GetPitchUV();
        // planer data packed to interleaved format (same as interleved2planar by kassandro) - v2.0.0.5
    pDst[0] = dst->GetWritePtr();
    pDst[1] = pDst[0] + nSuperWidth;
    pDst[2] = pDst[1] + nSuperWidth / 2; // YUY2
    nDstPitch[0] = dst->GetPitch();
    nDstPitch[1] = nDstPitch[2] = nDstPitch[0];
    planecount = 3;
  }
  else
  {
    int planes_y[4] = { PLANAR_Y, PLANAR_U, PLANAR_V, PLANAR_A };
    int planes_r[4] = { PLANAR_G, PLANAR_B, PLANAR_R, PLANAR_A };
    int *planes = (vi.IsYUV() || vi.IsYUVA()) ? planes_y : planes_r;
    planecount = std::min(vi.NumComponents(), 3);
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
  }
  // P.F. 170519 debug: clear super area
  // todo: check how it affects for non-modulo blocksize vs. supersize_padded
  // maybe some garbage is read outside the area by MAnalyse later?
  /*
  MemZoneSet(pDstY, 0, dst->GetRowSize(PLANAR_Y_ALIGNED), vi.height, 0, 0, nDstPitchY);
  if (!vi.IsY()) {
    MemZoneSet(pDstU, 0, dst->GetRowSize(PLANAR_U_ALIGNED), vi.height >> vi.GetPlaneHeightSubsampling(PLANAR_U), 0, 0, nDstPitchUV);
    MemZoneSet(pDstV, 0, dst->GetRowSize(PLANAR_V_ALIGNED), vi.height >> vi.GetPlaneHeightSubsampling(PLANAR_V), 0, 0, nDstPitchUV);
  }
  */

  PROFILE_STOP(MOTION_PROFILE_YUY2CONVERT);

  PROFILE_START(MOTION_PROFILE_INTERPOLATION);

  pSrcGOF->Update(YUVPLANES, pDst[0], nDstPitch[0], pDst[1], nDstPitch[1], pDst[2], nDstPitch[2]);
  // constant name is Y U and V but for MVFrame this is good for RGB
  MVPlaneSet planes[3] = { YPLANE, UPLANE, VPLANE };

  for (int p = 0; p < planecount; ++p) {
    const MVPlaneSet plane = planes[p];
    pSrcGOF->SetPlane(pSrc[p], nSrcPitch[p], plane);
  }

  pSrcGOF->Reduce(nModeYUV);
  pSrcGOF->Pad(nModeYUV);

  if (usePelClip)
  {
    MVFrame *srcFrames = pSrcGOF->GetFrame(0);

    for (int p = 0; p < planecount; ++p) {
      const MVPlaneSet plane = planes[p];
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
    PROFILE_START(MOTION_PROFILE_YUY2CONVERT);

    if ( (pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2 )
    {
      pDstYUY2 = dst->GetWritePtr();
      nDstPitchYUY2 = dst->GetPitch();
      YUY2FromPlanes(pDstYUY2, nDstPitchYUY2, nSuperWidth, nSuperHeight,
        pDstY, nDstPitchY, pDstU, pDstV, nDstPitchUV, isse);
    }
    PROFILE_STOP(MOTION_PROFILE_YUY2CONVERT);
  */

  PROFILE_CUMULATE();

  return dst;
}
