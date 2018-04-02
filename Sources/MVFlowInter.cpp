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
  int nSuperHeight = super->GetVideoInfo().height;

  if (nHeight != nHeightS
    || nWidth != nSuperWidth - nSuperHPad * 2
    || nPel != nSuperPel)
  {
    env->ThrowError("MFlowInter : wrong super frame clip");
  }

  if (nPel == 1)
    finest = super; // v2.0.9.1
  else
  {
    finest = new MVFinest(super, _isse, env);
    AVSValue cache_args[1] = { finest };
    finest = env->Invoke("InternalCache", AVSValue(cache_args, 1)).AsClip(); // add cache for speed
  }

//	if (nWidth  != vi.width || (nWidth + nHPadding*2)*nPel != finest->GetVideoInfo().width ||
//	    nHeight  != vi.height || (nHeight + nVPadding*2)*nPel != finest->GetVideoInfo().height )
//		env->ThrowError("MVFlowInter: wrong source or finest frame size");

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

  nWidthPUV = nWidthP / xRatioUV;
  nHeightPUV = nHeightP / yRatioUV;
  nHeightUV = nHeight / yRatioUV;
  nWidthUV = nWidth / xRatioUV;

  nHPaddingUV = nHPadding / xRatioUV;
  nVPaddingUV = nVPadding / yRatioUV;

  VPitchY = AlignNumber(nWidthP, 16);
  VPitchUV = AlignNumber(nWidthPUV, 16);

  VXFullYB = (short*)_aligned_malloc(2 * nHeightP*VPitchY + 128, 128);
  VXFullUVB = (short*)_aligned_malloc(2 * nHeightPUV*VPitchUV + 128, 128);
  VYFullYB = (short*)_aligned_malloc(2 * nHeightP*VPitchY + 128, 128);
  VYFullUVB = (short*)_aligned_malloc(2 * nHeightPUV*VPitchUV + 128, 128);

  VXFullYF = (short*)_aligned_malloc(2 * nHeightP*VPitchY + 128, 128);
  VXFullUVF = (short*)_aligned_malloc(2 * nHeightPUV*VPitchUV + 128, 128);
  VYFullYF = (short*)_aligned_malloc(2 * nHeightP*VPitchY + 128, 128);
  VYFullUVF = (short*)_aligned_malloc(2 * nHeightPUV*VPitchUV + 128, 128);

  VXSmallYB = (short*)_aligned_malloc(2 * nBlkXP*nBlkYP + 128, 128);
  VYSmallYB = (short*)_aligned_malloc(2 * nBlkXP*nBlkYP + 128, 128);
  VXSmallUVB = (short*)_aligned_malloc(2 * nBlkXP*nBlkYP + 128, 128);
  VYSmallUVB = (short*)_aligned_malloc(2 * nBlkXP*nBlkYP + 128, 128);

  VXSmallYF = (short*)_aligned_malloc(2 * nBlkXP*nBlkYP + 128, 128);
  VYSmallYF = (short*)_aligned_malloc(2 * nBlkXP*nBlkYP + 128, 128);
  VXSmallUVF = (short*)_aligned_malloc(2 * nBlkXP*nBlkYP + 128, 128);
  VYSmallUVF = (short*)_aligned_malloc(2 * nBlkXP*nBlkYP + 128, 128);

  VXFullYBB = (short*)_aligned_malloc(2 * nHeightP*VPitchY + 128, 128);
  VXFullUVBB = (short*)_aligned_malloc(2 * nHeightPUV*VPitchUV + 128, 128);
  VYFullYBB = (short*)_aligned_malloc(2 * nHeightP*VPitchY + 128, 128);
  VYFullUVBB = (short*)_aligned_malloc(2 * nHeightPUV*VPitchUV + 128, 128);

  VXFullYFF = (short*)_aligned_malloc(2 * nHeightP*VPitchY + 128, 128);
  VXFullUVFF = (short*)_aligned_malloc(2 * nHeightPUV*VPitchUV + 128, 128);
  VYFullYFF = (short*)_aligned_malloc(2 * nHeightP*VPitchY + 128, 128);
  VYFullUVFF = (short*)_aligned_malloc(2 * nHeightPUV*VPitchUV + 128, 128);

  VXSmallYBB = (short*)_aligned_malloc(2 * nBlkXP*nBlkYP + 128, 128);
  VYSmallYBB = (short*)_aligned_malloc(2 * nBlkXP*nBlkYP + 128, 128);
  VXSmallUVBB = (short*)_aligned_malloc(2 * nBlkXP*nBlkYP + 128, 128);
  VYSmallUVBB = (short*)_aligned_malloc(2 * nBlkXP*nBlkYP + 128, 128);

  VXSmallYFF = (short*)_aligned_malloc(2 * nBlkXP*nBlkYP + 128, 128);
  VYSmallYFF = (short*)_aligned_malloc(2 * nBlkXP*nBlkYP + 128, 128);
  VXSmallUVFF = (short*)_aligned_malloc(2 * nBlkXP*nBlkYP + 128, 128);
  VYSmallUVFF = (short*)_aligned_malloc(2 * nBlkXP*nBlkYP + 128, 128);

  MaskSmallB = (unsigned char*)_aligned_malloc(nBlkXP*nBlkYP + 128, 128);
  MaskFullYB = (unsigned char*)_aligned_malloc(nHeightP*VPitchY + 128, 128);
  MaskFullUVB = (unsigned char*)_aligned_malloc(nHeightPUV*VPitchUV + 128, 128);

  MaskSmallF = (unsigned char*)_aligned_malloc(nBlkXP*nBlkYP + 128, 128);
  MaskFullYF = (unsigned char*)_aligned_malloc(nHeightP*VPitchY + 128, 128);
  MaskFullUVF = (unsigned char*)_aligned_malloc(nHeightPUV*VPitchUV + 128, 128);

  SADMaskSmallB = (unsigned char*)_aligned_malloc(nBlkXP*nBlkYP + 128, 128);
  SADMaskSmallF = (unsigned char*)_aligned_malloc(nBlkXP*nBlkYP + 128, 128);

  upsizer = new SimpleResize(nWidthP, nHeightP, nBlkXP, nBlkYP, cpuFlags);
  upsizerUV = new SimpleResize(nWidthPUV, nHeightPUV, nBlkXP, nBlkYP, cpuFlags);

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
  delete upsizerUV;

  _aligned_free(VXFullYB);
  _aligned_free(VXFullUVB);
  _aligned_free(VYFullYB);
  _aligned_free(VYFullUVB);
  _aligned_free(VXSmallYB);
  _aligned_free(VYSmallYB);
  _aligned_free(VXSmallUVB);
  _aligned_free(VYSmallUVB);
  _aligned_free(VXFullYF);
  _aligned_free(VXFullUVF);
  _aligned_free(VYFullYF);
  _aligned_free(VYFullUVF);
  _aligned_free(VXSmallYF);
  _aligned_free(VYSmallYF);
  _aligned_free(VXSmallUVF);
  _aligned_free(VYSmallUVF);

  _aligned_free(MaskSmallB);
  _aligned_free(MaskFullYB);
  _aligned_free(MaskFullUVB);
  _aligned_free(MaskSmallF);
  _aligned_free(MaskFullYF);
  _aligned_free(MaskFullUVF);

  _aligned_free(VXFullYBB);
  _aligned_free(VXFullUVBB);
  _aligned_free(VYFullYBB);
  _aligned_free(VYFullUVBB);
  _aligned_free(VXSmallYBB);
  _aligned_free(VYSmallYBB);
  _aligned_free(VXSmallUVBB);
  _aligned_free(VYSmallUVBB);
  _aligned_free(VXFullYFF);
  _aligned_free(VXFullUVFF);
  _aligned_free(VYFullYFF);
  _aligned_free(VYFullUVFF);
  _aligned_free(VXSmallYFF);
  _aligned_free(VYSmallYFF);
  _aligned_free(VXSmallUVFF);
  _aligned_free(VYSmallUVFF);

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
  const BYTE *pt256[3];
  int nt256Pitches[3];
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
    }

    else
    {
      pDst[0] = YWPLAN(dst);
      pDst[1] = UWPLAN(dst);
      pDst[2] = VWPLAN(dst);
      nDstPitches[0] = YPITCH(dst);
      nDstPitches[1] = UPITCH(dst);
      nDstPitches[2] = VPITCH(dst);

      pRef[0] = YRPLAN(ref);
      pRef[1] = URPLAN(ref);
      pRef[2] = VRPLAN(ref);
      nRefPitches[0] = YPITCH(ref);
      nRefPitches[1] = UPITCH(ref);
      nRefPitches[2] = VPITCH(ref);

      pSrc[0] = YRPLAN(src);
      pSrc[1] = URPLAN(src);
      pSrc[2] = VRPLAN(src);
      nSrcPitches[0] = YPITCH(src);
      nSrcPitches[1] = UPITCH(src);
      nSrcPitches[2] = VPITCH(src);
    }

    int nOffsetY = nRefPitches[0] * nVPadding*nPel + nHPadding*nPel*pixelsize;
    int nOffsetUV = nRefPitches[1] * nVPaddingUV*nPel + nHPaddingUV*nPel*pixelsize;


    // make  vector vx and vy small masks
    MakeVectorSmallMasks(mvClipB, nBlkX, nBlkY, VXSmallYB, nBlkXP, VYSmallYB, nBlkXP);
    MakeVectorSmallMasks(mvClipF, nBlkX, nBlkY, VXSmallYF, nBlkXP, VYSmallYF, nBlkXP);
    if (nBlkXP > nBlkX) // fill right
    {
      for (int j = 0; j < nBlkY; j++)
      {
        VXSmallYB[j*nBlkXP + nBlkX] = std::min(VXSmallYB[j*nBlkXP + nBlkX - 1], short(0)); // 2.5.11.22
        VYSmallYB[j*nBlkXP + nBlkX] = VYSmallYB[j*nBlkXP + nBlkX - 1];
        VXSmallYF[j*nBlkXP + nBlkX] = std::min(VXSmallYF[j*nBlkXP + nBlkX - 1], short(0)); // 2.5.11.22
        VYSmallYF[j*nBlkXP + nBlkX] = VYSmallYF[j*nBlkXP + nBlkX - 1];
      }
    }
    if (nBlkYP > nBlkY) // fill bottom
    {
      for (int i = 0; i < nBlkXP; i++)
      {
        VXSmallYB[nBlkXP*nBlkY + i] = VXSmallYB[nBlkXP*(nBlkY - 1) + i];
        VYSmallYB[nBlkXP*nBlkY + i] = std::min(VYSmallYB[nBlkXP*(nBlkY - 1) + i], short(0)); // 2.5.11.22
        VXSmallYF[nBlkXP*nBlkY + i] = VXSmallYF[nBlkXP*(nBlkY - 1) + i];
        VYSmallYF[nBlkXP*nBlkY + i] = std::min(VYSmallYF[nBlkXP*(nBlkY - 1) + i], short(0)); // 2.5.11.22
      }
    }
    VectorSmallMaskYToHalfUV(VXSmallYB, nBlkXP, nBlkYP, VXSmallUVB, xRatioUV);
    VectorSmallMaskYToHalfUV(VYSmallYB, nBlkXP, nBlkYP, VYSmallUVB, yRatioUV);
    VectorSmallMaskYToHalfUV(VXSmallYF, nBlkXP, nBlkYP, VXSmallUVF, xRatioUV);
    VectorSmallMaskYToHalfUV(VYSmallYF, nBlkXP, nBlkYP, VYSmallUVF, yRatioUV);

    // analyse vectors field to detect occlusion

    //	  double occNormB = (256-time256)/(256*ml);
    MakeVectorOcclusionMaskTime(mvClipB, nBlkX, nBlkY, ml, 1.0,
      nPel, MaskSmallB, nBlkXP, (256 - time256),
      nBlkSizeX - nOverlapX, nBlkSizeY - nOverlapY);
    //	  double occNormF = time256/(256*ml);
    MakeVectorOcclusionMaskTime(mvClipF, nBlkX, nBlkY, ml, 1.0,
      nPel, MaskSmallF, nBlkXP, time256,
      nBlkSizeX - nOverlapX, nBlkSizeY - nOverlapY);

    if (nBlkXP > nBlkX) // fill right
    {
      for (int j = 0; j < nBlkY; j++)
      {
        MaskSmallB[j*nBlkXP + nBlkX] = MaskSmallB[j*nBlkXP + nBlkX - 1];
        MaskSmallF[j*nBlkXP + nBlkX] = MaskSmallF[j*nBlkXP + nBlkX - 1];
      }
    }
    if (nBlkYP > nBlkY) // fill bottom
    {
      for (int i = 0; i < nBlkXP; i++)
      {
        MaskSmallB[nBlkXP*nBlkY + i] = MaskSmallB[nBlkXP*(nBlkY - 1) + i];
        MaskSmallF[nBlkXP*nBlkY + i] = MaskSmallF[nBlkXP*(nBlkY - 1) + i];
      }
    }
    // upsize (bilinear interpolate) vector masks to fullframe size


    int dummyplane = PLANAR_Y; // use luma plane resizer code for all planes if we resize from luma small mask
    upsizer->SimpleResizeDo_uint16(VXFullYB, nWidthP, nHeightP, VPitchY, VXSmallYB, nBlkXP, nBlkXP);
    upsizer->SimpleResizeDo_uint16(VYFullYB, nWidthP, nHeightP, VPitchY, VYSmallYB, nBlkXP, nBlkXP);
    upsizerUV->SimpleResizeDo_uint16(VXFullUVB, nWidthPUV, nHeightPUV, VPitchUV, VXSmallUVB, nBlkXP, nBlkXP);
    upsizerUV->SimpleResizeDo_uint16(VYFullUVB, nWidthPUV, nHeightPUV, VPitchUV, VYSmallUVB, nBlkXP, nBlkXP);

    upsizer->SimpleResizeDo_uint16(VXFullYF, nWidthP, nHeightP, VPitchY, VXSmallYF, nBlkXP, nBlkXP);
    upsizer->SimpleResizeDo_uint16(VYFullYF, nWidthP, nHeightP, VPitchY, VYSmallYF, nBlkXP, nBlkXP);
    upsizerUV->SimpleResizeDo_uint16(VXFullUVF, nWidthPUV, nHeightPUV, VPitchUV, VXSmallUVF, nBlkXP, nBlkXP);
    upsizerUV->SimpleResizeDo_uint16(VYFullUVF, nWidthPUV, nHeightPUV, VPitchUV, VYSmallUVF, nBlkXP, nBlkXP);

    upsizer->SimpleResizeDo_uint8(MaskFullYB, nWidthP, nHeightP, VPitchY, MaskSmallB, nBlkXP, nBlkXP, dummyplane);
    upsizerUV->SimpleResizeDo_uint8(MaskFullUVB, nWidthPUV, nHeightPUV, VPitchUV, MaskSmallB, nBlkXP, nBlkXP, dummyplane);

    upsizer->SimpleResizeDo_uint8(MaskFullYF, nWidthP, nHeightP, VPitchY, MaskSmallF, nBlkXP, nBlkXP, dummyplane);
    upsizerUV->SimpleResizeDo_uint8(MaskFullUVF, nWidthPUV, nHeightPUV, VPitchUV, MaskSmallF, nBlkXP, nBlkXP, dummyplane);


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
      if (nBlkXP > nBlkX) // fill right
      {
        for (int j = 0; j < nBlkY; j++)
        {
          VXSmallYBB[j*nBlkXP + nBlkX] = std::min(VXSmallYBB[j*nBlkXP + nBlkX - 1], short(0)); // 2.5.11.22
          VYSmallYBB[j*nBlkXP + nBlkX] = VYSmallYBB[j*nBlkXP + nBlkX - 1];
          VXSmallYFF[j*nBlkXP + nBlkX] = std::min(VXSmallYFF[j*nBlkXP + nBlkX - 1], short(0)); // 2.5.11.22
          VYSmallYFF[j*nBlkXP + nBlkX] = VYSmallYFF[j*nBlkXP + nBlkX - 1];
        }
      }
      if (nBlkYP > nBlkY) // fill bottom
      {
        for (int i = 0; i < nBlkXP; i++)
        {
          VXSmallYBB[nBlkXP*nBlkY + i] = VXSmallYBB[nBlkXP*(nBlkY - 1) + i];
          VYSmallYBB[nBlkXP*nBlkY + i] = std::min(VYSmallYBB[nBlkXP*(nBlkY - 1) + i], short(0)); // 2.5.11.22
          VXSmallYFF[nBlkXP*nBlkY + i] = VXSmallYFF[nBlkXP*(nBlkY - 1) + i];
          VYSmallYFF[nBlkXP*nBlkY + i] = std::min(VYSmallYFF[nBlkXP*(nBlkY - 1) + i], short(0)); // 2.5.11.22
        }
      }
      VectorSmallMaskYToHalfUV(VXSmallYBB, nBlkXP, nBlkYP, VXSmallUVBB, xRatioUV);
      VectorSmallMaskYToHalfUV(VYSmallYBB, nBlkXP, nBlkYP, VYSmallUVBB, yRatioUV);
      VectorSmallMaskYToHalfUV(VXSmallYFF, nBlkXP, nBlkYP, VXSmallUVFF, xRatioUV);
      VectorSmallMaskYToHalfUV(VYSmallYFF, nBlkXP, nBlkYP, VYSmallUVFF, yRatioUV);

      // upsize vectors to full frame
      upsizer->SimpleResizeDo_uint16(VXFullYBB, nWidthP, nHeightP, VPitchY, VXSmallYBB, nBlkXP, nBlkXP);
      upsizer->SimpleResizeDo_uint16(VYFullYBB, nWidthP, nHeightP, VPitchY, VYSmallYBB, nBlkXP, nBlkXP);
      upsizerUV->SimpleResizeDo_uint16(VXFullUVBB, nWidthPUV, nHeightPUV, VPitchUV, VXSmallUVBB, nBlkXP, nBlkXP);
      upsizerUV->SimpleResizeDo_uint16(VYFullUVBB, nWidthPUV, nHeightPUV, VPitchUV, VYSmallUVBB, nBlkXP, nBlkXP);

      upsizer->SimpleResizeDo_uint16(VXFullYFF, nWidthP, nHeightP, VPitchY, VXSmallYFF, nBlkXP, nBlkXP);
      upsizer->SimpleResizeDo_uint16(VYFullYFF, nWidthP, nHeightP, VPitchY, VYSmallYFF, nBlkXP, nBlkXP);
      upsizerUV->SimpleResizeDo_uint16(VXFullUVFF, nWidthPUV, nHeightPUV, VPitchUV, VXSmallUVFF, nBlkXP, nBlkXP);
      upsizerUV->SimpleResizeDo_uint16(VYFullUVFF, nWidthPUV, nHeightPUV, VPitchUV, VYSmallUVFF, nBlkXP, nBlkXP);

      if (pixelsize == 1) {
        FlowInterExtra<uint8_t>(pDst[0], nDstPitches[0], pRef[0] + nOffsetY, pSrc[0] + nOffsetY, nRefPitches[0],
          VXFullYB, VXFullYF, VYFullYB, VYFullYF, MaskFullYB, MaskFullYF, VPitchY,
          nWidth, nHeight, time256, nPel, VXFullYBB, VXFullYFF, VYFullYBB, VYFullYFF);
        FlowInterExtra<uint8_t>(pDst[1], nDstPitches[1], pRef[1] + nOffsetUV, pSrc[1] + nOffsetUV, nRefPitches[1],
          VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, MaskFullUVB, MaskFullUVF, VPitchUV,
          nWidthUV, nHeightUV, time256, nPel, VXFullUVBB, VXFullUVFF, VYFullUVBB, VYFullUVFF);
        FlowInterExtra<uint8_t>(pDst[2], nDstPitches[2], pRef[2] + nOffsetUV, pSrc[2] + nOffsetUV, nRefPitches[2],
          VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, MaskFullUVB, MaskFullUVF, VPitchUV,
          nWidthUV, nHeightUV, time256, nPel, VXFullUVBB, VXFullUVFF, VYFullUVBB, VYFullUVFF);
      }
      else {
        FlowInterExtra<uint16_t>(pDst[0], nDstPitches[0], pRef[0] + nOffsetY, pSrc[0] + nOffsetY, nRefPitches[0],
          VXFullYB, VXFullYF, VYFullYB, VYFullYF, MaskFullYB, MaskFullYF, VPitchY,
          nWidth, nHeight, time256, nPel, VXFullYBB, VXFullYFF, VYFullYBB, VYFullYFF);
        FlowInterExtra<uint16_t>(pDst[1], nDstPitches[1], pRef[1] + nOffsetUV, pSrc[1] + nOffsetUV, nRefPitches[1],
          VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, MaskFullUVB, MaskFullUVF, VPitchUV,
          nWidthUV, nHeightUV, time256, nPel, VXFullUVBB, VXFullUVFF, VYFullUVBB, VYFullUVFF);
        FlowInterExtra<uint16_t>(pDst[2], nDstPitches[2], pRef[2] + nOffsetUV, pSrc[2] + nOffsetUV, nRefPitches[2],
          VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, MaskFullUVB, MaskFullUVF, VPitchUV,
          nWidthUV, nHeightUV, time256, nPel, VXFullUVBB, VXFullUVFF, VYFullUVBB, VYFullUVFF);
      }
    }
    else // bad extra frames, use old method without extra frames
    {
      if (pixelsize == 1) {
        FlowInter<uint8_t>(pDst[0], nDstPitches[0], pRef[0] + nOffsetY, pSrc[0] + nOffsetY, nRefPitches[0],
          VXFullYB, VXFullYF, VYFullYB, VYFullYF, MaskFullYB, MaskFullYF, VPitchY,
          nWidth, nHeight, time256, nPel);
        FlowInter<uint8_t>(pDst[1], nDstPitches[1], pRef[1] + nOffsetUV, pSrc[1] + nOffsetUV, nRefPitches[1],
          VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, MaskFullUVB, MaskFullUVF, VPitchUV,
          nWidthUV, nHeightUV, time256, nPel);
        FlowInter<uint8_t>(pDst[2], nDstPitches[2], pRef[2] + nOffsetUV, pSrc[2] + nOffsetUV, nRefPitches[2],
          VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, MaskFullUVB, MaskFullUVF, VPitchUV,
          nWidthUV, nHeightUV, time256, nPel);
      }
      else { // pixelsize == 2
        FlowInter<uint16_t>(pDst[0], nDstPitches[0], pRef[0] + nOffsetY, pSrc[0] + nOffsetY, nRefPitches[0],
          VXFullYB, VXFullYF, VYFullYB, VYFullYF, MaskFullYB, MaskFullYF, VPitchY,
          nWidth, nHeight, time256, nPel);
        FlowInter<uint16_t>(pDst[1], nDstPitches[1], pRef[1] + nOffsetUV, pSrc[1] + nOffsetUV, nRefPitches[1],
          VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, MaskFullUVB, MaskFullUVF, VPitchUV,
          nWidthUV, nHeightUV, time256, nPel);
        FlowInter<uint16_t>(pDst[2], nDstPitches[2], pRef[2] + nOffsetUV, pSrc[2] + nOffsetUV, nRefPitches[2],
          VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, MaskFullUVB, MaskFullUVF, VPitchUV,
          nWidthUV, nHeightUV, time256, nPel);
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
        pDst[0] = YWPLAN(dst);
        pDst[1] = UWPLAN(dst);
        pDst[2] = VWPLAN(dst);
        nDstPitches[0] = YPITCH(dst);
        nDstPitches[1] = UPITCH(dst);
        nDstPitches[2] = VPITCH(dst);

        pRef[0] = YRPLAN(ref);
        pRef[1] = URPLAN(ref);
        pRef[2] = VRPLAN(ref);
        nRefPitches[0] = YPITCH(ref);
        nRefPitches[1] = UPITCH(ref);
        nRefPitches[2] = VPITCH(ref);

        pSrc[0] = YRPLAN(src);
        pSrc[1] = URPLAN(src);
        pSrc[2] = VRPLAN(src);
        nSrcPitches[0] = YPITCH(src);
        nSrcPitches[1] = UPITCH(src);
        nSrcPitches[2] = VPITCH(src);

        // blend with time weight
        if (pixelsize == 1) {
          Blend<uint8_t>(pDst[0], pSrc[0], pRef[0], nHeight, nWidth, nDstPitches[0], nSrcPitches[0], nRefPitches[0], time256, cpuFlags);
          Blend<uint8_t>(pDst[1], pSrc[1], pRef[1], nHeightUV, nWidthUV, nDstPitches[1], nSrcPitches[1], nRefPitches[1], time256, cpuFlags);
          Blend<uint8_t>(pDst[2], pSrc[2], pRef[2], nHeightUV, nWidthUV, nDstPitches[2], nSrcPitches[2], nRefPitches[2], time256, cpuFlags);
        }
        else { // pixelsize == 2
          Blend<uint16_t>(pDst[0], pSrc[0], pRef[0], nHeight, nWidth, nDstPitches[0], nSrcPitches[0], nRefPitches[0], time256, cpuFlags);
          Blend<uint16_t>(pDst[1], pSrc[1], pRef[1], nHeightUV, nWidthUV, nDstPitches[1], nSrcPitches[1], nRefPitches[1], time256, cpuFlags);
          Blend<uint16_t>(pDst[2], pSrc[2], pRef[2], nHeightUV, nWidthUV, nDstPitches[2], nSrcPitches[2], nRefPitches[2], time256, cpuFlags);
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
