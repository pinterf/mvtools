// Pixels flow motion interplolation function
// Copyright(c)2005-2006 A.G.Balakhnin aka Fizick

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

#include "ClipFnc.h"
#include "MVFlowInter.h"
#include "MaskFun.h"
#include "MVFinest.h"
#include "SuperParams64Bits.h"
//#include "Time256ProviderCst.h"
//#include "Time256ProviderPlane.h"
#include "commonfunctions.h"

MVFlowInter::MVFlowInter(PClip _child, PClip super, PClip _mvbw, PClip _mvfw, int _time256, double _ml,
  bool _blend, sad_t nSCD1, int nSCD2, bool _isse, bool _planar, IScriptEnvironment* env) :
  GenericVideoFilter(_child),
  MVFilter(_mvfw, "MFlowInter", env, 1, 0),
  mvClipB(_mvbw, nSCD1, nSCD2, env, 1, 0),
  mvClipF(_mvfw, nSCD1, nSCD2, env, 1, 0)
{
  time256 = _time256;
  ml = _ml;
  cpuFlags = _isse ? env->GetCPUFlags() : 0;
  planar = _planar;
  blend = _blend;

  CheckSimilarity(mvClipB, "mvbw", env);
  CheckSimilarity(mvClipF, "mvfw", env);

  if (mvClipB.GetDeltaFrame() <= 0 || mvClipB.GetDeltaFrame() <= 0)
    env->ThrowError("MFlowInter: cannot use motion vectors with absolute frame references.");

  SuperParams64Bits params;
  memcpy(&params, &super->GetVideoInfo().num_audio_samples, 8);
  int nHeightS = params.nHeight;
  int nSuperHPad = params.nHPad;
  int nSuperVPad = params.nVPad;
  int nSuperPel = params.nPel;
  int nSuperModeYUV = params.nModeYUV;
  int nSuperLevels = params.nLevels;
  int nSuperWidth = super->GetVideoInfo().width; // really super
  //int nSuperHeight = super->GetVideoInfo().height;

  if (!super->GetVideoInfo().IsSameColorspace(child->GetVideoInfo()))
    env->ThrowError("MFlowInter: input and super clip format is different");

  if (nHeight != nHeightS
    || nWidth != nSuperWidth - nSuperHPad * 2
    || nPel != nSuperPel)
  {
    env->ThrowError("MFlowInter : wrong super frame clip");
  }

  if (nPel == 1)
  {
    finest = super; // v2.0.9.1
  }
  else
  {
    finest = new MVFinest(super, _isse, env);
    AVSValue cache_args[1] = { finest };
    finest = env->Invoke("InternalCache", AVSValue(cache_args, 1)).AsClip(); // add cache for speed
  }

  // actual format can differ from the base clip of vectors
  pixelsize_super = super->GetVideoInfo().ComponentSize();
  bits_per_pixel_super = super->GetVideoInfo().BitsPerComponent();
  pixelsize_super_shift = ilog2(pixelsize_super);

  planecount = vi.IsYUY2() ? 3 : std::min(vi.NumComponents(), 3);

  xRatioUVs[0] = xRatioUVs[1] = xRatioUVs[2] = 1;
  yRatioUVs[0] = yRatioUVs[1] = yRatioUVs[2] = 1;

  if (!vi.IsY() && !vi.IsRGB()) {
    xRatioUVs[1] = xRatioUVs[2] = vi.IsYUY2() ? 2 : (1 << vi.GetPlaneWidthSubsampling(PLANAR_U));
    yRatioUVs[1] = yRatioUVs[2] = vi.IsYUY2() ? 1 : (1 << vi.GetPlaneHeightSubsampling(PLANAR_U));
  }

  for (int i = 0; i < planecount; i++) {
    nLogxRatioUVs[i] = ilog2(xRatioUVs[i]);
    nLogyRatioUVs[i] = ilog2(yRatioUVs[i]);
  }

  // may be padded for full frame cover
  /*
  nBlkXP = (nBlkX*(nBlkSizeX - nOverlapX) + nOverlapX < nWidth) ? nBlkX + 1 : nBlkX;
  nBlkYP = (nBlkY*(nBlkSizeY - nOverlapY) + nOverlapY < nHeight) ? nBlkY + 1 : nBlkY;
  */
  // e.g. BlkSizeY==8, nOverLapY=2, nHeight==1080, nBlkY==178 -> nBlkYP = 179 would still result in nHeightP == 1076
  // fix in 2.7.29- sometimes +1 is not enough 
  nBlkXP = nBlkX;
  while (nBlkXP*(nBlkSizeX - nOverlapX) + nOverlapX < nWidth)
    nBlkXP++;
  nBlkYP = nBlkY;
  while (nBlkYP*(nBlkSizeY - nOverlapY) + nOverlapY < nHeight)
    nBlkYP++;

  nWidthP = nBlkXP * (nBlkSizeX - nOverlapX) + nOverlapX;
  nHeightP = nBlkYP * (nBlkSizeY - nOverlapY) + nOverlapY;

  nWidthPUV = nWidthP / xRatioUVs[1];
  nHeightPUV = nHeightP / yRatioUVs[1];
  nHeightUV = nHeight / yRatioUVs[1];
  nWidthUV = nWidth / xRatioUVs[1];

  nHPaddingUV = nHPadding / xRatioUVs[1];
  nVPaddingUV = nVPadding / yRatioUVs[1];

  VPitchY = AlignNumber(nWidthP, 16);
  VPitchUV = AlignNumber(nWidthPUV, 16);

  is444 = vi.Is444();
  isGrey = vi.IsY();
  isRGB = vi.IsPlanarRGB() || vi.IsPlanarRGBA(); // planar only
  //needDistinctChroma = !is444 && !isGrey && !isRGB;

  // v2.7.34 common full size buffers for all planes, eliminates ten large buffer
  VXFull_B = (short*)_aligned_malloc(2 * nHeightP*VPitchY + 128, 128);
  VYFull_B = (short*)_aligned_malloc(2 * nHeightP*VPitchY + 128, 128);

  VXFull_F = (short*)_aligned_malloc(2 * nHeightP*VPitchY + 128, 128);
  VYFull_F = (short*)_aligned_malloc(2 * nHeightP*VPitchY + 128, 128);

  VXSmallYB = (short*)_aligned_malloc(2 * nBlkXP*nBlkYP + 128, 128);
  VYSmallYB = (short*)_aligned_malloc(2 * nBlkXP*nBlkYP + 128, 128);
  if (!isGrey) {
    VXSmallUVB = (short*)_aligned_malloc(2 * nBlkXP*nBlkYP + 128, 128);
    VYSmallUVB = (short*)_aligned_malloc(2 * nBlkXP*nBlkYP + 128, 128);
  }

  VXSmallYF = (short*)_aligned_malloc(2 * nBlkXP*nBlkYP + 128, 128);
  VYSmallYF = (short*)_aligned_malloc(2 * nBlkXP*nBlkYP + 128, 128);
  if (!isGrey) {
    VXSmallUVF = (short*)_aligned_malloc(2 * nBlkXP*nBlkYP + 128, 128);
    VYSmallUVF = (short*)_aligned_malloc(2 * nBlkXP*nBlkYP + 128, 128);
  }

  VXFull_BB = (short*)_aligned_malloc(2 * nHeightP*VPitchY + 128, 128);
  VYFull_BB = (short*)_aligned_malloc(2 * nHeightP*VPitchY + 128, 128);

  VXFull_FF = (short*)_aligned_malloc(2 * nHeightP*VPitchY + 128, 128);
  VYFull_FF = (short*)_aligned_malloc(2 * nHeightP*VPitchY + 128, 128);

  VXSmallYBB = (short*)_aligned_malloc(2 * nBlkXP*nBlkYP + 128, 128);
  VYSmallYBB = (short*)_aligned_malloc(2 * nBlkXP*nBlkYP + 128, 128);
  if (!isGrey) {
    VXSmallUVBB = (short*)_aligned_malloc(2 * nBlkXP*nBlkYP + 128, 128);
    VYSmallUVBB = (short*)_aligned_malloc(2 * nBlkXP*nBlkYP + 128, 128);
  }

  VXSmallYFF = (short*)_aligned_malloc(2 * nBlkXP*nBlkYP + 128, 128);
  VYSmallYFF = (short*)_aligned_malloc(2 * nBlkXP*nBlkYP + 128, 128);
  if (!isGrey) {
    VXSmallUVFF = (short*)_aligned_malloc(2 * nBlkXP*nBlkYP + 128, 128);
    VYSmallUVFF = (short*)_aligned_malloc(2 * nBlkXP*nBlkYP + 128, 128);
  }

  MaskSmallB = (unsigned char*)_aligned_malloc(nBlkXP*nBlkYP + 128, 128);
  MaskFull_B = (unsigned char*)_aligned_malloc(nHeightP*VPitchY + 128, 128);

  MaskSmallF = (unsigned char*)_aligned_malloc(nBlkXP*nBlkYP + 128, 128);
  MaskFull_F = (unsigned char*)_aligned_malloc(nHeightP*VPitchY + 128, 128);

  SADMaskSmallB = (unsigned char*)_aligned_malloc(nBlkXP*nBlkYP + 128, 128);
  SADMaskSmallF = (unsigned char*)_aligned_malloc(nBlkXP*nBlkYP + 128, 128);

  upsizer = new SimpleResize(nWidthP, nHeightP, nBlkXP, nBlkYP, cpuFlags);
  if (!isGrey) {
    upsizerUV = new SimpleResize(nWidthPUV, nHeightPUV, nBlkXP, nBlkYP, cpuFlags);
  }

  if ((pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2 && !planar)
  {
    DstPlanes = new YUY2Planes(nWidth, nHeight);
  }

}

MVFlowInter::~MVFlowInter()
{
  if ((pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2 && !planar)
  {
    delete DstPlanes;
  }

  delete upsizer;
  if(!isGrey)
    delete upsizerUV;

  _aligned_free(VXFull_B);
  _aligned_free(VYFull_B);
  _aligned_free(VXSmallYB);
  _aligned_free(VYSmallYB);
  if (!isGrey) {
    _aligned_free(VXSmallUVB);
    _aligned_free(VYSmallUVB);
  }

  _aligned_free(VXFull_F);
  _aligned_free(VYFull_F);
  _aligned_free(VXSmallYF);
  _aligned_free(VYSmallYF);
  if (!isGrey) {
    _aligned_free(VXSmallUVF);
    _aligned_free(VYSmallUVF);
  }

  _aligned_free(MaskSmallB);
  _aligned_free(MaskFull_B);
  _aligned_free(MaskSmallF);
  _aligned_free(MaskFull_F);

  _aligned_free(VXFull_BB);
  _aligned_free(VYFull_BB);
  _aligned_free(VXSmallYBB);
  _aligned_free(VYSmallYBB);
  if (!isGrey) {
    _aligned_free(VXSmallUVBB);
    _aligned_free(VYSmallUVBB);
  }

  _aligned_free(VXFull_FF);
  _aligned_free(VYFull_FF);
  _aligned_free(VXSmallYFF);
  _aligned_free(VYSmallYFF);
  if (!isGrey) {
    _aligned_free(VXSmallUVFF);
    _aligned_free(VYSmallUVFF);
  }

  _aligned_free(SADMaskSmallB);
  _aligned_free(SADMaskSmallF);
}

//-------------------------------------------------------------------------
PVideoFrame __stdcall MVFlowInter::GetFrame(int n, IScriptEnvironment* env)
{
  PVideoFrame dst;
  BYTE *pDst[3];
  const BYTE *pRef[3], *pSrc[3];
  int nDstPitches[3], nRefPitches[3], nSrcPitches[3];
  unsigned char *pDstYUY2;
  int nDstPitchYUY2;

  const int		off = mvClipB.GetDeltaFrame(); // integer offset of reference frame
  if (off <= 0)
  {
    env->ThrowError("MFlowInter: cannot use motion vectors with absolute frame references.");
  }
  const int		nref = n + off;

  PVideoFrame mvF = mvClipF.GetFrame(nref, env);
  mvClipF.Update(mvF, env);// forward from current to next
  mvF = 0;
  PVideoFrame mvB = mvClipB.GetFrame(n, env);
  mvClipB.Update(mvB, env);// backward from next to current
  mvB = 0;

  // Checked here instead of the constructor to allow using multi-vector
  // clips, because the backward flag is not reliable before the vector
  // data are actually read from the frame.
  if (!mvClipB.IsBackward())
    env->ThrowError("MFlowInter: wrong backward vectors");
  if (mvClipF.IsBackward())
    env->ThrowError("MFlowInter: wrong forward vectors");

//	int sharp = mvClipB.GetSharp();
  PVideoFrame	src = finest->GetFrame(n, env);
  PVideoFrame ref = finest->GetFrame(nref, env);//  ref for  compensation
  dst = env->NewVideoFrame(vi);

  if (mvClipB.IsUsable() && mvClipF.IsUsable())
  {
    if ((pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2)
    {
      // planar data packed to interleaved format (same as interleved2planar by kassandro) - v2.0.0.5
      pSrc[0] = src->GetReadPtr();
      pSrc[1] = pSrc[0] + src->GetRowSize() / 2;
      pSrc[2] = pSrc[1] + src->GetRowSize() / 4;
      nSrcPitches[0] = src->GetPitch();
      nSrcPitches[1] = nSrcPitches[0];
      nSrcPitches[2] = nSrcPitches[0];
      // planar data packed to interleaved format (same as interleved2planar by kassandro) - v2.0.0.5
      pRef[0] = ref->GetReadPtr();
      pRef[1] = pRef[0] + ref->GetRowSize() / 2;
      pRef[2] = pRef[1] + ref->GetRowSize() / 4;
      nRefPitches[0] = ref->GetPitch();
      nRefPitches[1] = nRefPitches[0];
      nRefPitches[2] = nRefPitches[0];

      if (!planar)
      {
        pDstYUY2 = dst->GetWritePtr();
        nDstPitchYUY2 = dst->GetPitch();
        pDst[0] = DstPlanes->GetPtr();
        pDst[1] = DstPlanes->GetPtrU();
        pDst[2] = DstPlanes->GetPtrV();
        nDstPitches[0] = DstPlanes->GetPitch();
        nDstPitches[1] = DstPlanes->GetPitchUV();
        nDstPitches[2] = DstPlanes->GetPitchUV();
      }
      else
      {
        pDst[0] = dst->GetWritePtr();
        pDst[1] = pDst[0] + dst->GetRowSize() / 2;
        pDst[2] = pDst[1] + dst->GetRowSize() / 4;;
        nDstPitches[0] = dst->GetPitch();
        nDstPitches[1] = nDstPitches[0];
        nDstPitches[2] = nDstPitches[0];
      }
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
        nSrcPitches[p] = src->GetPitch(plane);

        pRef[p] = ref->GetReadPtr(plane);
        nRefPitches[p] = ref->GetPitch(plane);

        pDst[p] = dst->GetWritePtr(plane);
        nDstPitches[p] = dst->GetPitch(plane);
      }

    }

    int nOffsetY = nRefPitches[0] * nVPadding*nPel + nHPadding*nPel*pixelsize_super;
    int nOffsetUV = nRefPitches[1] * nVPaddingUV*nPel + nHPaddingUV*nPel*pixelsize_super;

    // make  vector vx and vy small masks
    MakeVectorSmallMasks(mvClipB, nBlkX, nBlkY, VXSmallYB, nBlkXP, VYSmallYB, nBlkXP);
    MakeVectorSmallMasks(mvClipF, nBlkX, nBlkY, VXSmallYF, nBlkXP, VYSmallYF, nBlkXP);

    CheckAndPadSmallY_BF(VXSmallYB, VXSmallYF, VYSmallYB, VYSmallYF, nBlkXP, nBlkYP, nBlkX, nBlkY);

    if (!isGrey) {
      VectorSmallMaskYToHalfUV(VXSmallYB, nBlkXP, nBlkYP, VXSmallUVB, xRatioUVs[1]);
      VectorSmallMaskYToHalfUV(VYSmallYB, nBlkXP, nBlkYP, VYSmallUVB, yRatioUVs[1]);
      VectorSmallMaskYToHalfUV(VXSmallYF, nBlkXP, nBlkYP, VXSmallUVF, xRatioUVs[1]);
      VectorSmallMaskYToHalfUV(VYSmallYF, nBlkXP, nBlkYP, VYSmallUVF, yRatioUVs[1]);
    }

    // analyse vectors field to detect occlusion

    //	  double occNormB = (256-time256)/(256*ml);
    MakeVectorOcclusionMaskTime(mvClipB, nBlkX, nBlkY, ml, 1.0,
      nPel, MaskSmallB, nBlkXP, (256 - time256),
      nBlkSizeX - nOverlapX, nBlkSizeY - nOverlapY);
    //	  double occNormF = time256/(256*ml);
    MakeVectorOcclusionMaskTime(mvClipF, nBlkX, nBlkY, ml, 1.0,
      nPel, MaskSmallF, nBlkXP, time256,
      nBlkSizeX - nOverlapX, nBlkSizeY - nOverlapY);

    CheckAndPadMaskSmall_BF(MaskSmallB, MaskSmallF, nBlkXP, nBlkYP, nBlkX, nBlkY);

    // upsize (bilinear interpolate) vector masks to fullframe size

    // v2.7.34- fullframe upsize later, in order to have common buffer for Y and U/V
    // VXFullYB VXFullUVB -> VXFull_B
    // VYFullYB VYFullUVB -> VYFull_B
    // VXFullYF VXFullUVF -> VXFull_F
    // VYFullYF VYFullUVF -> VYFull_F
    // VXFullYBB VXFullUVBB -> VXFull_BB
    // VYFullYBB VYFullUVBB -> VYFull_BB
    // VXFullYFF VXFullUVFF -> VXFull_FF
    // VYFullYFF VYFullUVFF -> VYFull_FF

    // Get motion info from more frames for occlusion areas
    PVideoFrame mvFF = mvClipF.GetFrame(n, env);
    mvClipF.Update(mvFF, env);// forward from prev to cur
    mvFF = 0;
    PVideoFrame mvBB = mvClipB.GetFrame(nref, env);
    mvClipB.Update(mvBB, env);// backward from next next to next
    mvBB = 0;

    if (mvClipB.IsUsable() && mvClipF.IsUsable())
    {
      // get vector mask from extra frames
      MakeVectorSmallMasks(mvClipB, nBlkX, nBlkY, VXSmallYBB, nBlkXP, VYSmallYBB, nBlkXP);
      MakeVectorSmallMasks(mvClipF, nBlkX, nBlkY, VXSmallYFF, nBlkXP, VYSmallYFF, nBlkXP);

      CheckAndPadSmallY_BF(VXSmallYBB, VXSmallYFF, VYSmallYBB, VYSmallYFF, nBlkXP, nBlkYP, nBlkX, nBlkY);

      if (!isGrey) {
        VectorSmallMaskYToHalfUV(VXSmallYBB, nBlkXP, nBlkYP, VXSmallUVBB, xRatioUVs[1]);
        VectorSmallMaskYToHalfUV(VYSmallYBB, nBlkXP, nBlkYP, VYSmallUVBB, yRatioUVs[1]);
        VectorSmallMaskYToHalfUV(VXSmallYFF, nBlkXP, nBlkYP, VXSmallUVFF, xRatioUVs[1]);
        VectorSmallMaskYToHalfUV(VYSmallYFF, nBlkXP, nBlkYP, VYSmallUVFF, yRatioUVs[1]);
      }

      // Upsize Y: B and F vectors and mask to full frame (same for MFlowInter and MFlowInterExtra)
      upsizer->SimpleResizeDo_int16(VXFull_B, nWidthP, nHeightP, VPitchY, VXSmallYB, nBlkXP, nBlkXP, nPel, true, nWidth, nHeight);
      upsizer->SimpleResizeDo_int16(VYFull_B, nWidthP, nHeightP, VPitchY, VYSmallYB, nBlkXP, nBlkXP, nPel, false, nWidth, nHeight);
      upsizer->SimpleResizeDo_int16(VXFull_F, nWidthP, nHeightP, VPitchY, VXSmallYF, nBlkXP, nBlkXP, nPel, true, nWidth, nHeight);
      upsizer->SimpleResizeDo_int16(VYFull_F, nWidthP, nHeightP, VPitchY, VYSmallYF, nBlkXP, nBlkXP, nPel, false, nWidth, nHeight);

      upsizer->SimpleResizeDo_uint8(MaskFull_B, nWidthP, nHeightP, VPitchY, MaskSmallB, nBlkXP, nBlkXP);
      upsizer->SimpleResizeDo_uint8(MaskFull_F, nWidthP, nHeightP, VPitchY, MaskSmallF, nBlkXP, nBlkXP);

      // Upsize Y: BB and FF vectors to full frame (MFlowInterExtra only)
      upsizer->SimpleResizeDo_int16(VXFull_BB, nWidthP, nHeightP, VPitchY, VXSmallYBB, nBlkXP, nBlkXP, nPel, true, nWidth, nHeight);
      upsizer->SimpleResizeDo_int16(VYFull_BB, nWidthP, nHeightP, VPitchY, VYSmallYBB, nBlkXP, nBlkXP, nPel, false, nWidth, nHeight);
      upsizer->SimpleResizeDo_int16(VXFull_FF, nWidthP, nHeightP, VPitchY, VXSmallYFF, nBlkXP, nBlkXP, nPel, true, nWidth, nHeight);
      upsizer->SimpleResizeDo_int16(VYFull_FF, nWidthP, nHeightP, VPitchY, VYSmallYFF, nBlkXP, nBlkXP, nPel, false, nWidth, nHeight);

      // FlowInterExtra Y
      if (pixelsize_super == 1) {
        FlowInterExtra<uint8_t>(pDst[0], nDstPitches[0], pRef[0] + nOffsetY, pSrc[0] + nOffsetY, nRefPitches[0],
          VXFull_B, VXFull_F, VYFull_B, VYFull_F, MaskFull_B, MaskFull_F, VPitchY,
          nWidth, nHeight, time256, nPel, VXFull_BB, VXFull_FF, VYFull_BB, VYFull_FF);
      }
      else if (pixelsize_super == 2) {
        FlowInterExtra<uint16_t>(pDst[0], nDstPitches[0], pRef[0] + nOffsetY, pSrc[0] + nOffsetY, nRefPitches[0],
          VXFull_B, VXFull_F, VYFull_B, VYFull_F, MaskFull_B, MaskFull_F, VPitchY,
          nWidth, nHeight, time256, nPel, VXFull_BB, VXFull_FF, VYFull_BB, VYFull_FF);
      }
      else if (pixelsize_super == 4) {
        FlowInterExtra<float>(pDst[0], nDstPitches[0], pRef[0] + nOffsetY, pSrc[0] + nOffsetY, nRefPitches[0],
          VXFull_B, VXFull_F, VYFull_B, VYFull_F, MaskFull_B, MaskFull_F, VPitchY,
          nWidth, nHeight, time256, nPel, VXFull_BB, VXFull_FF, VYFull_BB, VYFull_FF);
      }

      // Upsize UV: B and F vectors and mask to full frame (same for MFlowInter and MFlowInterExtra)
      if (!isGrey) {
        upsizerUV->SimpleResizeDo_int16(VXFull_B, nWidthPUV, nHeightPUV, VPitchUV, VXSmallUVB, nBlkXP, nBlkXP, nPel, true, nWidthUV, nHeightUV);
        upsizerUV->SimpleResizeDo_int16(VYFull_B, nWidthPUV, nHeightPUV, VPitchUV, VYSmallUVB, nBlkXP, nBlkXP, nPel, false, nWidthUV, nHeightUV);
        upsizerUV->SimpleResizeDo_int16(VXFull_F, nWidthPUV, nHeightPUV, VPitchUV, VXSmallUVF, nBlkXP, nBlkXP, nPel, true, nWidthUV, nHeightUV);
        upsizerUV->SimpleResizeDo_int16(VYFull_F, nWidthPUV, nHeightPUV, VPitchUV, VYSmallUVF, nBlkXP, nBlkXP, nPel, false, nWidthUV, nHeightUV);

        upsizerUV->SimpleResizeDo_uint8(MaskFull_B, nWidthPUV, nHeightPUV, VPitchUV, MaskSmallB, nBlkXP, nBlkXP);
        upsizerUV->SimpleResizeDo_uint8(MaskFull_F, nWidthPUV, nHeightPUV, VPitchUV, MaskSmallF, nBlkXP, nBlkXP);

        // Upsize UV: BB and FF vectors to full frame (MFlowInterExtra only)
        upsizerUV->SimpleResizeDo_int16(VXFull_BB, nWidthPUV, nHeightPUV, VPitchUV, VXSmallUVBB, nBlkXP, nBlkXP, nPel, true, nWidthUV, nHeightUV);
        upsizerUV->SimpleResizeDo_int16(VYFull_BB, nWidthPUV, nHeightPUV, VPitchUV, VYSmallUVBB, nBlkXP, nBlkXP, nPel, false, nWidthUV, nHeightUV);
        upsizerUV->SimpleResizeDo_int16(VXFull_FF, nWidthPUV, nHeightPUV, VPitchUV, VXSmallUVFF, nBlkXP, nBlkXP, nPel, true, nWidthUV, nHeightUV);
        upsizerUV->SimpleResizeDo_int16(VYFull_FF, nWidthPUV, nHeightPUV, VPitchUV, VYSmallUVFF, nBlkXP, nBlkXP, nPel, false, nWidthUV, nHeightUV);

        // FlowInterExtra U/V
        if (pixelsize_super == 1) {
          FlowInterExtra<uint8_t>(pDst[1], nDstPitches[1], pRef[1] + nOffsetUV, pSrc[1] + nOffsetUV, nRefPitches[1],
            VXFull_B, VXFull_F, VYFull_B, VYFull_F, MaskFull_B, MaskFull_F, VPitchUV,
            nWidthUV, nHeightUV, time256, nPel, VXFull_BB, VXFull_FF, VYFull_BB, VYFull_FF);
          FlowInterExtra<uint8_t>(pDst[2], nDstPitches[2], pRef[2] + nOffsetUV, pSrc[2] + nOffsetUV, nRefPitches[2],
            VXFull_B, VXFull_F, VYFull_B, VYFull_F, MaskFull_B, MaskFull_F, VPitchUV,
            nWidthUV, nHeightUV, time256, nPel, VXFull_BB, VXFull_FF, VYFull_BB, VYFull_FF);
        }
        else if (pixelsize_super == 2) {
          FlowInterExtra<uint16_t>(pDst[1], nDstPitches[1], pRef[1] + nOffsetUV, pSrc[1] + nOffsetUV, nRefPitches[1],
            VXFull_B, VXFull_F, VYFull_B, VYFull_F, MaskFull_B, MaskFull_F, VPitchUV,
            nWidthUV, nHeightUV, time256, nPel, VXFull_BB, VXFull_FF, VYFull_BB, VYFull_FF);
          FlowInterExtra<uint16_t>(pDst[2], nDstPitches[2], pRef[2] + nOffsetUV, pSrc[2] + nOffsetUV, nRefPitches[2],
            VXFull_B, VXFull_F, VYFull_B, VYFull_F, MaskFull_B, MaskFull_F, VPitchUV,
            nWidthUV, nHeightUV, time256, nPel, VXFull_BB, VXFull_FF, VYFull_BB, VYFull_FF);
        }
        else if (pixelsize_super == 4) {
          FlowInterExtra<float>(pDst[1], nDstPitches[1], pRef[1] + nOffsetUV, pSrc[1] + nOffsetUV, nRefPitches[1],
            VXFull_B, VXFull_F, VYFull_B, VYFull_F, MaskFull_B, MaskFull_F, VPitchUV,
            nWidthUV, nHeightUV, time256, nPel, VXFull_BB, VXFull_FF, VYFull_BB, VYFull_FF);
          FlowInterExtra<float>(pDst[2], nDstPitches[2], pRef[2] + nOffsetUV, pSrc[2] + nOffsetUV, nRefPitches[2],
            VXFull_B, VXFull_F, VYFull_B, VYFull_F, MaskFull_B, MaskFull_F, VPitchUV,
            nWidthUV, nHeightUV, time256, nPel, VXFull_BB, VXFull_FF, VYFull_BB, VYFull_FF);
        }
      }
    }
    else // bad extra frames, use old method without extra frames
    {
      // Upsize Y: B and F vectors and mask to full frame (same for MFlowInter and MFlowInterExtra)
      upsizer->SimpleResizeDo_int16(VXFull_B, nWidthP, nHeightP, VPitchY, VXSmallYB, nBlkXP, nBlkXP, nPel, true, nWidth, nHeight);
      upsizer->SimpleResizeDo_int16(VYFull_B, nWidthP, nHeightP, VPitchY, VYSmallYB, nBlkXP, nBlkXP, nPel, false, nWidth, nHeight);
      upsizer->SimpleResizeDo_int16(VXFull_F, nWidthP, nHeightP, VPitchY, VXSmallYF, nBlkXP, nBlkXP, nPel, true, nWidth, nHeight);
      upsizer->SimpleResizeDo_int16(VYFull_F, nWidthP, nHeightP, VPitchY, VYSmallYF, nBlkXP, nBlkXP, nPel, false, nWidth, nHeight);

      upsizer->SimpleResizeDo_uint8(MaskFull_B, nWidthP, nHeightP, VPitchY, MaskSmallB, nBlkXP, nBlkXP);
      upsizer->SimpleResizeDo_uint8(MaskFull_F, nWidthP, nHeightP, VPitchY, MaskSmallF, nBlkXP, nBlkXP);

      // FlowInter Y
      if (pixelsize_super == 1) {
        FlowInter<uint8_t>(pDst[0], nDstPitches[0], pRef[0] + nOffsetY, pSrc[0] + nOffsetY, nRefPitches[0],
          VXFull_B, VXFull_F, VYFull_B, VYFull_F, MaskFull_B, MaskFull_F, VPitchY,
          nWidth, nHeight, time256, nPel);
      }
      else if (pixelsize_super == 2) {
        FlowInter<uint16_t>(pDst[0], nDstPitches[0], pRef[0] + nOffsetY, pSrc[0] + nOffsetY, nRefPitches[0],
          VXFull_B, VXFull_F, VYFull_B, VYFull_F, MaskFull_B, MaskFull_F, VPitchY,
          nWidth, nHeight, time256, nPel);
      }
      else if (pixelsize_super == 4) {
        FlowInter<float>(pDst[0], nDstPitches[0], pRef[0] + nOffsetY, pSrc[0] + nOffsetY, nRefPitches[0],
          VXFull_B, VXFull_F, VYFull_B, VYFull_F, MaskFull_B, MaskFull_F, VPitchY,
          nWidth, nHeight, time256, nPel);
      }

      // Upsize UV: B and F vectors and mask to full frame (same for MFlowInter and MFlowInterExtra)
      if (!isGrey) {
        upsizerUV->SimpleResizeDo_int16(VXFull_B, nWidthPUV, nHeightPUV, VPitchUV, VXSmallUVB, nBlkXP, nBlkXP, nPel, true, nWidthUV, nHeightUV);
        upsizerUV->SimpleResizeDo_int16(VYFull_B, nWidthPUV, nHeightPUV, VPitchUV, VYSmallUVB, nBlkXP, nBlkXP, nPel, false, nWidthUV, nHeightUV);
        upsizerUV->SimpleResizeDo_int16(VXFull_F, nWidthPUV, nHeightPUV, VPitchUV, VXSmallUVF, nBlkXP, nBlkXP, nPel, true, nWidthUV, nHeightUV);
        upsizerUV->SimpleResizeDo_int16(VYFull_F, nWidthPUV, nHeightPUV, VPitchUV, VYSmallUVF, nBlkXP, nBlkXP, nPel, false, nWidthUV, nHeightUV);

        upsizerUV->SimpleResizeDo_uint8(MaskFull_B, nWidthPUV, nHeightPUV, VPitchUV, MaskSmallB, nBlkXP, nBlkXP);
        upsizerUV->SimpleResizeDo_uint8(MaskFull_F, nWidthPUV, nHeightPUV, VPitchUV, MaskSmallF, nBlkXP, nBlkXP);

        // FlowInter U/V
        if (pixelsize_super == 1) {
          FlowInter<uint8_t>(pDst[1], nDstPitches[1], pRef[1] + nOffsetUV, pSrc[1] + nOffsetUV, nRefPitches[1],
            VXFull_B, VXFull_F, VYFull_B, VYFull_F, MaskFull_B, MaskFull_F, VPitchUV,
            nWidthUV, nHeightUV, time256, nPel);
          FlowInter<uint8_t>(pDst[2], nDstPitches[2], pRef[2] + nOffsetUV, pSrc[2] + nOffsetUV, nRefPitches[2],
            VXFull_B, VXFull_F, VYFull_B, VYFull_F, MaskFull_B, MaskFull_F, VPitchUV,
            nWidthUV, nHeightUV, time256, nPel);
        }
        else if (pixelsize_super == 2) {
          FlowInter<uint16_t>(pDst[1], nDstPitches[1], pRef[1] + nOffsetUV, pSrc[1] + nOffsetUV, nRefPitches[1],
            VXFull_B, VXFull_F, VYFull_B, VYFull_F, MaskFull_B, MaskFull_F, VPitchUV,
            nWidthUV, nHeightUV, time256, nPel);
          FlowInter<uint16_t>(pDst[2], nDstPitches[2], pRef[2] + nOffsetUV, pSrc[2] + nOffsetUV, nRefPitches[2],
            VXFull_B, VXFull_F, VYFull_B, VYFull_F, MaskFull_B, MaskFull_F, VPitchUV,
            nWidthUV, nHeightUV, time256, nPel);
        }
        else if (pixelsize_super == 4) {
          FlowInter<float>(pDst[1], nDstPitches[1], pRef[1] + nOffsetUV, pSrc[1] + nOffsetUV, nRefPitches[1],
            VXFull_B, VXFull_F, VYFull_B, VYFull_F, MaskFull_B, MaskFull_F, VPitchUV,
            nWidthUV, nHeightUV, time256, nPel);
          FlowInter<float>(pDst[2], nDstPitches[2], pRef[2] + nOffsetUV, pSrc[2] + nOffsetUV, nRefPitches[2],
            VXFull_B, VXFull_F, VYFull_B, VYFull_F, MaskFull_B, MaskFull_F, VPitchUV,
            nWidthUV, nHeightUV, time256, nPel);
        }
      }
    }

    if ((pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2 && !planar)
    {
      YUY2FromPlanes(pDstYUY2, nDstPitchYUY2, nWidth, nHeight,
        pDst[0], nDstPitches[0], pDst[1], pDst[2], nDstPitches[1], cpuFlags);
    }
    return dst;
  }

  else
  {
    // poor estimation

    // prepare pointers
    PVideoFrame src = child->GetFrame(n, env); // it is easy to use child here - v2.0

    if (blend) //let's blend src with ref frames like ConvertFPS
    {
      PVideoFrame ref = child->GetFrame(nref, env);

      if ((pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2)
      {
        pSrc[0] = src->GetReadPtr(); // we can blend YUY2
        nSrcPitches[0] = src->GetPitch();
        pRef[0] = ref->GetReadPtr();
        nRefPitches[0] = ref->GetPitch();
        pDstYUY2 = dst->GetWritePtr();
        nDstPitchYUY2 = dst->GetPitch();

        Blend<uint8_t>(pDstYUY2, pSrc[0], pRef[0], nHeight, nWidth * 2, nDstPitchYUY2, nSrcPitches[0], nRefPitches[0], time256, cpuFlags);
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
          nSrcPitches[p] = src->GetPitch(plane);

          pRef[p] = ref->GetReadPtr(plane);
          nRefPitches[p] = ref->GetPitch(plane);

          pDst[p] = dst->GetWritePtr(plane);
          nDstPitches[p] = dst->GetPitch(plane);
        }

        // blend with time weight
        if (pixelsize_super == 1) {
          Blend<uint8_t>(pDst[0], pSrc[0], pRef[0], nHeight, nWidth, nDstPitches[0], nSrcPitches[0], nRefPitches[0], time256, cpuFlags);
          if (!isGrey) {
            Blend<uint8_t>(pDst[1], pSrc[1], pRef[1], nHeightUV, nWidthUV, nDstPitches[1], nSrcPitches[1], nRefPitches[1], time256, cpuFlags);
            Blend<uint8_t>(pDst[2], pSrc[2], pRef[2], nHeightUV, nWidthUV, nDstPitches[2], nSrcPitches[2], nRefPitches[2], time256, cpuFlags);
          }
        }
        else if (pixelsize_super == 2) {
          Blend<uint16_t>(pDst[0], pSrc[0], pRef[0], nHeight, nWidth, nDstPitches[0], nSrcPitches[0], nRefPitches[0], time256, cpuFlags);
          if (!isGrey) {
            Blend<uint16_t>(pDst[1], pSrc[1], pRef[1], nHeightUV, nWidthUV, nDstPitches[1], nSrcPitches[1], nRefPitches[1], time256, cpuFlags);
            Blend<uint16_t>(pDst[2], pSrc[2], pRef[2], nHeightUV, nWidthUV, nDstPitches[2], nSrcPitches[2], nRefPitches[2], time256, cpuFlags);
          }
        }
        else if (pixelsize_super == 4) {
          Blend<float>(pDst[0], pSrc[0], pRef[0], nHeight, nWidth, nDstPitches[0], nSrcPitches[0], nRefPitches[0], time256, cpuFlags);
          if (!isGrey) {
            Blend<float>(pDst[1], pSrc[1], pRef[1], nHeightUV, nWidthUV, nDstPitches[1], nSrcPitches[1], nRefPitches[1], time256, cpuFlags);
            Blend<float>(pDst[2], pSrc[2], pRef[2], nHeightUV, nWidthUV, nDstPitches[2], nSrcPitches[2], nRefPitches[2], time256, cpuFlags);
          }
        }
      }
      return dst;
    }
    else
    {
      return src; // like ChangeFPS
    }
  }
}
