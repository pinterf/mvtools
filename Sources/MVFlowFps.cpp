// Pixels flow motion interpolation function to change FPS
// Copyright(c)2005 A.G.Balakhnin aka Fizick

// See legal notice in Copying.txt for more information

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; version 2 of the License.
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
#include "commonfunctions.h"
#include "MaskFun.h"
#include "MVFinest.h"
#include "MVFlowFps.h"
#include "profile.h"
#include "SuperParams64Bits.h"
#include "Time256ProviderCst.h"
#include "info.h"


MVFlowFps::MVFlowFps(PClip _child, PClip super, PClip _mvbw, PClip _mvfw, unsigned int _num, unsigned int _den, int _maskmode, double _ml,
  bool _blend, sad_t nSCD1, int nSCD2, bool _isse, bool _planar, int _optDebug, IScriptEnvironment* env) :
  GenericVideoFilter(_child),
  MVFilter(_mvfw, "MFlowFps", env, 1, 0),
  mvClipB(_mvbw, nSCD1, nSCD2, env, 1, 0),
  mvClipF(_mvfw, nSCD1, nSCD2, env, 1, 0),
  optDebug(_optDebug)
{
  static int id = 0; _instance_id = id++;
  reentrancy_check = false;
  _RPT1(0, "MVFlowFps.Create id=%d\n", _instance_id);

  if (!vi.IsYUV() && !vi.IsYUVA())
    env->ThrowError("MFlowFps: Clip must be YUV or YUY2");
  if (vi.BitsPerComponent() > 16)
    env->ThrowError("MFlowFps: only 8-16 bit clips are allowed");

  numeratorOld = vi.fps_numerator;
  denominatorOld = vi.fps_denominator;

  if (_num != 0 && _den != 0)
  {
    numerator = _num;
    denominator = _den;
  }
  else if (numeratorOld < (1 << 30))
  {
    numerator = (numeratorOld << 1); // double fps by default
    denominator = denominatorOld;
  }
  else // very big numerator
  {
    numerator = numeratorOld;
    denominator = (denominatorOld >> 1);// double fps by default
  }

  //  safe for big numbers since v2.1
  fa = __int64(denominator)*__int64(numeratorOld);
  fb = __int64(numerator)*__int64(denominatorOld);
  __int64 fgcd = gcd(fa, fb); // general common divisor
  fa /= fgcd;
  fb /= fgcd;

  vi.SetFPS(numerator, denominator);

  vi.num_frames = (int)(1 + __int64(vi.num_frames - 1) * fb / fa);

  maskmode = _maskmode; // speed mode
  ml = _ml;
//   nIdx = _nIdx;
  cpuFlags = _isse ? env->GetCPUFlags() : 0;
  planar = _planar;
  blend = _blend;

  CheckSimilarity(mvClipB, "mvbw", env);
  CheckSimilarity(mvClipF, "mvfw", env);

  if (nWidth != vi.width || nHeight != vi.height)
    env->ThrowError("MFlowFps: inconsistent source and vector frame size");


  if (!((nWidth + nHPadding * 2) == super->GetVideoInfo().width && (nHeight + nVPadding * 2) <= super->GetVideoInfo().height))
    env->ThrowError("MFlowFps: inconsistent clips frame size!");

//	if (isSuper)
//	{
    // get parameters of prepared super clip - v2.0
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
//		pRefBGOF = new MVGroupOfFrames(nSuperLevels, nWidth, nHeight, nSuperPel, nSuperHPad, nSuperVPad, nSuperModeYUV, isse, yRatioUV);
//		pRefFGOF = new MVGroupOfFrames(nSuperLevels, nWidth, nHeight, nSuperPel, nSuperHPad, nSuperVPad, nSuperModeYUV, isse, yRatioUV);

  if (nHeight != nHeightS
    || nWidth != nSuperWidth - nSuperHPad * 2
    || nPel != nSuperPel)
  {
    env->ThrowError("MFlowFps : wrong super frame clip");
  }
//	}

  if (nPel == 1)
  {
    finest = super; // v2.0.9.1
  }
  else
  {
    finest = new MVFinest(super, _isse, env);
//		AVSValue finest_args[2] = { super, isse };
//		finest = env->Invoke("MVFinest", AVSValue(finest_args,2)).AsClip();
    AVSValue cache_args[1] = { finest };
    finest = env->Invoke("InternalCache", AVSValue(cache_args, 1)).AsClip(); // add cache for speed
  }

  // may be padded for full frame cover
  nBlkXP = (nBlkX*(nBlkSizeX - nOverlapX) + nOverlapX < nWidth) ? nBlkX + 1 : nBlkX;
  nBlkYP = (nBlkY*(nBlkSizeY - nOverlapY) + nOverlapY < nHeight) ? nBlkY + 1 : nBlkY;
  nWidthP = nBlkXP*(nBlkSizeX - nOverlapX) + nOverlapX;
  nHeightP = nBlkYP*(nBlkSizeY - nOverlapY) + nOverlapY;
  // for YV12 // NO! for every format
  nWidthPUV = nWidthP / xRatioUV;   // PF160923 yet another /2
  nHeightPUV = nHeightP / yRatioUV;
  nHeightUV = nHeight / yRatioUV;
  nWidthUV = nWidth / xRatioUV; // PF160923 yet another /2

  nHPaddingUV = nHPadding / xRatioUV; // PF160923 yet another /2
  nVPaddingUV = nVPadding / yRatioUV;

  VPitchY = AlignNumber(nWidthP, 16);
  VPitchUV = AlignNumber(nWidthPUV, 16);

  // 2.5.11.22: 
  // old: VXFullYB = new BYTE [nHeightP*VPitchY]
  // new: VXFullYB = (short*) _aligned_malloc(2*nHeightP*VPitchY+128, 128);
  // 2*: sizeof(short)
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

  // PF remark: masks are 8 bits
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

  nleftLast = -1000;
  nrightLast = -1000;

  if ((pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2 && !planar)
  {
    DstPlanes = new YUY2Planes(nWidth, nHeight);
  }

}

MVFlowFps::~MVFlowFps()
{
  if ((pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2 && !planar)
  {
    delete DstPlanes;
  }

  delete upsizer;
  delete upsizerUV;

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
PVideoFrame __stdcall MVFlowFps::GetFrame(int n, IScriptEnvironment* env)
{
  if (reentrancy_check) {
    env->ThrowError("MFlowFps: cannot work in reentrant multithread mode!");
  }
  reentrancy_check = true;

#ifndef _M_X64
  _mm_empty();
#endif
  _RPT2(0, "MFlowFPS GetFrame, frame=%d id=%d\n", n, _instance_id);

  int nleft = (int)(__int64(n)* fa / fb);
  // intermediate product may be very large! Now I know how to multiply int64
  int time256 = int((double(n)*double(fa) / double(fb) - nleft) * 256 + 0.5);

  PVideoFrame dst;
  BYTE *pDst[3];
  const BYTE *pRef[3], *pSrc[3];
  int nDstPitches[3], nRefPitches[3], nSrcPitches[3];
  unsigned char *pDstYUY2;
  int nDstPitchYUY2;


  int off = mvClipB.GetDeltaFrame(); // integer offset of reference frame
  if (off <= 0)
  {
    env->ThrowError("MFlowFps: cannot use motion vectors with absolute frame references.");
  }
  // usually off must be = 1
  if (off > 1)
    time256 = time256 / off;

  int nright = nleft + off;

// v2.0
  if (time256 == 0) {
    dst = child->GetFrame(nleft, env); // simply left
    if (optDebug != 0) {
      env->MakeWritable(&dst);
      char buf[2048];
      sprintf_s(buf, "FRAME %d time256=%d off=%d, nleft=%d, nright=%d, fa=%d, fb=%d, using left!", n, time256, off, nleft, nright, (int)fa, (int)fb);
      DrawString(dst, vi, 0, 0, buf);
    }
    reentrancy_check = false;
    return dst;
  }
  else if (time256 == 256) {
    dst = child->GetFrame(nright, env); // simply right
    if (optDebug != 0) {
      env->MakeWritable(&dst);
      char buf[2048];
      sprintf_s(buf, "FRAME %d time256=%d off=%d, nleft=%d, nright=%d, fa=%d, fb=%d, using left!", n, time256, off, nleft, nright, (int)fa, (int)fb);
      DrawString(dst, vi, 0, 0, buf);
    }
    reentrancy_check = false;
    return dst;
  }

  _RPT3(0, "Before mvClipF GetFrame frame %d, nright=%d id=%d\n", n, nright, _instance_id);
  PVideoFrame mvF = mvClipF.GetFrame(nright, env);
  mvClipF.Update(mvF, env);// forward from current to next
  mvF = 0;
  _RPT3(0, "Before mvClipB GetFrame frame %d, nleft=%d id=%d\n", n, nleft, _instance_id);
  PVideoFrame mvB = mvClipB.GetFrame(nleft, env);
  mvClipB.Update(mvB, env);// backward from next to current
  mvB = 0;

  // Checked here instead of the constructor to allow using multi-vector
  // clips, because the backward flag is not reliable before the vector
  // data are actually read from the frame.
  if (!mvClipB.IsBackward())
    env->ThrowError("MFlowFps: wrong backward vectors");
  if (mvClipF.IsBackward())
    env->ThrowError("MFlowFps: wrong forward vectors");

  PVideoFrame	src = finest->GetFrame(nleft, env); // move here - v2.0
  PVideoFrame ref = finest->GetFrame(nright, env);//  right frame for  compensation

// 2.5.11.22	Create_LUTV(time256, LUTVB, LUTVF); // lookup table
// 2.5.11.22		const Time256ProviderCst	t256_prov_cst (time256, LUTVB, LUTVF);

  dst = env->NewVideoFrame(vi);

  bool isUsableB = mvClipB.IsUsable();
  bool isUsableF = mvClipF.IsUsable();

  if (isUsableB && isUsableF)
  {

    PROFILE_START(MOTION_PROFILE_YUY2CONVERT);

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
        pDst[2] = pDst[1] + dst->GetRowSize() / 4;
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

    PROFILE_STOP(MOTION_PROFILE_YUY2CONVERT);

    int nOffsetY = nRefPitches[0] * nVPadding*nPel + nHPadding*nPel*pixelsize;
    int nOffsetUV = nRefPitches[1] * nVPaddingUV*nPel + nHPaddingUV*nPel*pixelsize;

    int dummyplane = PLANAR_Y; // use luma plane resizer code for all planes if we resize from luma small mask

    if (nright != nrightLast)
    {
      PROFILE_START(MOTION_PROFILE_MASK);
      // make  vector vx and vy small masks
      MakeVectorSmallMasks(mvClipB, nBlkX, nBlkY, VXSmallYB, nBlkXP, VYSmallYB, nBlkXP);
      if (nBlkXP > nBlkX) // fill right
      {
        for (int j = 0; j < nBlkY; j++)
        {
          VXSmallYB[j*nBlkXP + nBlkX] = std::min(VXSmallYB[j*nBlkXP + nBlkX - 1], short(0)); // 2.5.11.22
          //VXSmallYB[j*nBlkXP + nBlkX] = std::min(VXSmallYB[j*nBlkXP + nBlkX-1], uint8_t (128));
          VYSmallYB[j*nBlkXP + nBlkX] = VYSmallYB[j*nBlkXP + nBlkX - 1];
        }
      }
      if (nBlkYP > nBlkY) // fill bottom
      {
        for (int i = 0; i < nBlkXP; i++)
        {
          VXSmallYB[nBlkXP*nBlkY + i] = VXSmallYB[nBlkXP*(nBlkY - 1) + i];
        //	VYSmallYB[nBlkXP*nBlkY +i] = std::min(VYSmallYB[nBlkXP*(nBlkY-1) +i], uint8_t (128));
          VYSmallYB[nBlkXP*nBlkY + i] = std::min(VYSmallYB[nBlkXP*(nBlkY - 1) + i], short(0));  // 2.5.11.22
        }
      }
      VectorSmallMaskYToHalfUV(VXSmallYB, nBlkXP, nBlkYP, VXSmallUVB, xRatioUV);
      VectorSmallMaskYToHalfUV(VYSmallYB, nBlkXP, nBlkYP, VYSmallUVB, yRatioUV);

      PROFILE_STOP(MOTION_PROFILE_MASK);
      // upsize (bilinear interpolate) vector masks to fullframe size
      PROFILE_START(MOTION_PROFILE_RESIZE);

      upsizer->SimpleResizeDo_uint16(VXFullYB, nWidthP, nHeightP, VPitchY, VXSmallYB, nBlkXP, nBlkXP);
      upsizer->SimpleResizeDo_uint16(VYFullYB, nWidthP, nHeightP, VPitchY, VYSmallYB, nBlkXP, nBlkXP);
      upsizerUV->SimpleResizeDo_uint16(VXFullUVB, nWidthPUV, nHeightPUV, VPitchUV, VXSmallUVB, nBlkXP, nBlkXP);
      upsizerUV->SimpleResizeDo_uint16(VYFullUVB, nWidthPUV, nHeightPUV, VPitchUV, VYSmallUVB, nBlkXP, nBlkXP);
      PROFILE_STOP(MOTION_PROFILE_RESIZE);

    }
   // analyse vectors field to detect occlusion
   // Backward part
    PROFILE_START(MOTION_PROFILE_MASK);
//		double occNormB = (256-time256)/(256*ml);
//		MakeVectorOcclusionMask(mvClipB, nBlkX, nBlkY, occNormB, 1.0, nPel, MaskSmallB, nBlkXP);
    MakeVectorOcclusionMaskTime(mvClipB, nBlkX, nBlkY, ml, 1.0, nPel, MaskSmallB, nBlkXP, (256 - time256), nBlkSizeX - nOverlapX, nBlkSizeY - nOverlapY);
    if (nBlkXP > nBlkX) // fill right
    {
      for (int j = 0; j < nBlkY; j++)
      {
        MaskSmallB[j*nBlkXP + nBlkX] = MaskSmallB[j*nBlkXP + nBlkX - 1];
      }
    }
    if (nBlkYP > nBlkY) // fill bottom
    {
      for (int i = 0; i < nBlkXP; i++)
      {
        MaskSmallB[nBlkXP*nBlkY + i] = MaskSmallB[nBlkXP*(nBlkY - 1) + i];
      }
    }
    PROFILE_STOP(MOTION_PROFILE_MASK);
    PROFILE_START(MOTION_PROFILE_RESIZE);
  // upsize (bilinear interpolate) vector masks to fullframe size
    upsizer->SimpleResizeDo_uint8(MaskFullYB, nWidthP, nHeightP, VPitchY, MaskSmallB, nBlkXP, nBlkXP, dummyplane);
    upsizerUV->SimpleResizeDo_uint8(MaskFullUVB, nWidthPUV, nHeightPUV, VPitchUV, MaskSmallB, nBlkXP, nBlkXP, dummyplane);
    PROFILE_STOP(MOTION_PROFILE_RESIZE);

    nrightLast = nright;

    // Forward part
    if (nleft != nleftLast)
    {
     // make  vector vx and vy small masks
      PROFILE_START(MOTION_PROFILE_MASK);
      MakeVectorSmallMasks(mvClipF, nBlkX, nBlkY, VXSmallYF, nBlkXP, VYSmallYF, nBlkXP);
      if (nBlkXP > nBlkX) // fill right
      {
        for (int j = 0; j < nBlkY; j++)
        {
          VXSmallYF[j*nBlkXP + nBlkX] = std::min(VXSmallYF[j*nBlkXP + nBlkX - 1], short(0)); // 2.5.11.22
          //VXSmallYF[j*nBlkXP + nBlkX] = std::min(VXSmallYF[j*nBlkXP + nBlkX-1], uint8_t (128));
          VYSmallYF[j*nBlkXP + nBlkX] = VYSmallYF[j*nBlkXP + nBlkX - 1];
        }
      }
      if (nBlkYP > nBlkY) // fill bottom
      {
        for (int i = 0; i < nBlkXP; i++)
        {
          VXSmallYF[nBlkXP*nBlkY + i] = VXSmallYF[nBlkXP*(nBlkY - 1) + i];
          //VYSmallYF[nBlkXP*nBlkY +i] = std::min(VYSmallYF[nBlkXP*(nBlkY-1) +i], uint8_t (128));
          VYSmallYF[nBlkXP*nBlkY + i] = std::min(VYSmallYF[nBlkXP*(nBlkY - 1) + i], short(0)); // 2.5.11.22 Line 458
        }
      }
      VectorSmallMaskYToHalfUV(VXSmallYF, nBlkXP, nBlkYP, VXSmallUVF, xRatioUV);
      VectorSmallMaskYToHalfUV(VYSmallYF, nBlkXP, nBlkYP, VYSmallUVF, yRatioUV);

      PROFILE_STOP(MOTION_PROFILE_MASK);
      // upsize (bilinear interpolate) vector masks to fullframe size
      PROFILE_START(MOTION_PROFILE_RESIZE);

      upsizer->SimpleResizeDo_uint16(VXFullYF, nWidthP, nHeightP, VPitchY, VXSmallYF, nBlkXP, nBlkXP);
      upsizer->SimpleResizeDo_uint16(VYFullYF, nWidthP, nHeightP, VPitchY, VYSmallYF, nBlkXP, nBlkXP);
      upsizerUV->SimpleResizeDo_uint16(VXFullUVF, nWidthPUV, nHeightPUV, VPitchUV, VXSmallUVF, nBlkXP, nBlkXP);
      upsizerUV->SimpleResizeDo_uint16(VYFullUVF, nWidthPUV, nHeightPUV, VPitchUV, VYSmallUVF, nBlkXP, nBlkXP);
      PROFILE_STOP(MOTION_PROFILE_RESIZE);

    }
   // analyse vectors field to detect occlusion
   // Forward part
    PROFILE_START(MOTION_PROFILE_MASK);
    MakeVectorOcclusionMaskTime(mvClipF, nBlkX, nBlkY, ml, 1.0, nPel, MaskSmallF, nBlkXP, time256, nBlkSizeX - nOverlapX, nBlkSizeY - nOverlapY);
    if (nBlkXP > nBlkX) // fill right
    {
      for (int j = 0; j < nBlkY; j++)
      {
        MaskSmallF[j*nBlkXP + nBlkX] = MaskSmallF[j*nBlkXP + nBlkX - 1];
      }
    }
    if (nBlkYP > nBlkY) // fill bottom
    {
      for (int i = 0; i < nBlkXP; i++)
      {
        MaskSmallF[nBlkXP*nBlkY + i] = MaskSmallF[nBlkXP*(nBlkY - 1) + i];
      }
    }
    PROFILE_STOP(MOTION_PROFILE_MASK);
    PROFILE_START(MOTION_PROFILE_RESIZE);
  // upsize (bilinear interpolate) vector masks to fullframe size
    upsizer->SimpleResizeDo_uint8(MaskFullYF, nWidthP, nHeightP, VPitchY, MaskSmallF, nBlkXP, nBlkXP, dummyplane);
    upsizerUV->SimpleResizeDo_uint8(MaskFullUVF, nWidthPUV, nHeightPUV, VPitchUV, MaskSmallF, nBlkXP, nBlkXP, dummyplane);
    PROFILE_STOP(MOTION_PROFILE_RESIZE);

    nleftLast = nleft;

    // Backward and forward is ready

  // Get motion info from more frames for occlusion areas
    PVideoFrame mvFF = mvClipF.GetFrame(nleft, env);
    mvClipF.Update(mvFF, env);// forward from prev to cur
    mvFF = 0;

    PVideoFrame mvBB = mvClipB.GetFrame(nright, env);
    mvClipB.Update(mvBB, env);// backward from next next to next
    mvBB = 0;

    bool isUsableB = mvClipB.IsUsable();
    bool isUsableF = mvClipF.IsUsable();
    _RPT5(0, "part#2 IsUsableB=%d IsUsableF=%d frame=%d,nleft=%d,nright=%d\n", isUsableB ? 1 : 0, isUsableF ? 1 : 0, n, nleft, nright);

    if (maskmode == 2 && isUsableB && isUsableF) // slow method with extra frames
    {
     // get vector mask from extra frames
      PROFILE_START(MOTION_PROFILE_MASK);
      MakeVectorSmallMasks(mvClipB, nBlkX, nBlkY, VXSmallYBB, nBlkXP, VYSmallYBB, nBlkXP);
      MakeVectorSmallMasks(mvClipF, nBlkX, nBlkY, VXSmallYFF, nBlkXP, VYSmallYFF, nBlkXP);
      if (nBlkXP > nBlkX) // fill right
      {
        for (int j = 0; j < nBlkY; j++)
        {
          VXSmallYBB[j*nBlkXP + nBlkX] = std::min(VXSmallYBB[j*nBlkXP + nBlkX - 1], short(0)); // 2.5.11.22
          //VXSmallYBB[j*nBlkXP + nBlkX] = std::min(VXSmallYBB[j*nBlkXP + nBlkX-1], uint8_t (128));
          VYSmallYBB[j*nBlkXP + nBlkX] = VYSmallYBB[j*nBlkXP + nBlkX - 1];
          VXSmallYFF[j*nBlkXP + nBlkX] = std::min(VXSmallYFF[j*nBlkXP + nBlkX - 1], short(0)); // 2.5.11.22 Line 522
          //VXSmallYFF[j*nBlkXP + nBlkX] = std::min(VXSmallYFF[j*nBlkXP + nBlkX-1], uint8_t (128));
          VYSmallYFF[j*nBlkXP + nBlkX] = VYSmallYFF[j*nBlkXP + nBlkX - 1];
        }
      }
      if (nBlkYP > nBlkY) // fill bottom
      {
        for (int i = 0; i < nBlkXP; i++)
        {
          VXSmallYBB[nBlkXP*nBlkY + i] = VXSmallYBB[nBlkXP*(nBlkY - 1) + i];
          //VYSmallYBB[nBlkXP*nBlkY +i] = std::min(VYSmallYBB[nBlkXP*(nBlkY-1) +i], uint8_t (128));
          VYSmallYBB[nBlkXP*nBlkY + i] = std::min(VYSmallYBB[nBlkXP*(nBlkY - 1) + i], short(0)); // 2.5.11.22 typecast needed in vs2015
          VXSmallYFF[nBlkXP*nBlkY + i] = VXSmallYFF[nBlkXP*(nBlkY - 1) + i];
          //VYSmallYFF[nBlkXP*nBlkY +i] = std::min(VYSmallYFF[nBlkXP*(nBlkY-1) +i], uint8_t (128));
          VYSmallYFF[nBlkXP*nBlkY + i] = std::min(VYSmallYFF[nBlkXP*(nBlkY - 1) + i], short(0)); // 2.5.11.22 typecast needed in vs2015
        }
      }
      VectorSmallMaskYToHalfUV(VXSmallYBB, nBlkXP, nBlkYP, VXSmallUVBB, xRatioUV);
      VectorSmallMaskYToHalfUV(VYSmallYBB, nBlkXP, nBlkYP, VYSmallUVBB, yRatioUV);
      VectorSmallMaskYToHalfUV(VXSmallYFF, nBlkXP, nBlkYP, VXSmallUVFF, xRatioUV);
      VectorSmallMaskYToHalfUV(VYSmallYFF, nBlkXP, nBlkYP, VYSmallUVFF, yRatioUV);
      PROFILE_STOP(MOTION_PROFILE_MASK);

      PROFILE_START(MOTION_PROFILE_RESIZE);
    // upsize vectors to full frame
      upsizer->SimpleResizeDo_uint16(VXFullYBB, nWidthP, nHeightP, VPitchY, VXSmallYBB, nBlkXP, nBlkXP);
      upsizer->SimpleResizeDo_uint16(VYFullYBB, nWidthP, nHeightP, VPitchY, VYSmallYBB, nBlkXP, nBlkXP);
      upsizerUV->SimpleResizeDo_uint16(VXFullUVBB, nWidthPUV, nHeightPUV, VPitchUV, VXSmallUVBB, nBlkXP, nBlkXP);
      upsizerUV->SimpleResizeDo_uint16(VYFullUVBB, nWidthPUV, nHeightPUV, VPitchUV, VYSmallUVBB, nBlkXP, nBlkXP);

      upsizer->SimpleResizeDo_uint16(VXFullYFF, nWidthP, nHeightP, VPitchY, VXSmallYFF, nBlkXP, nBlkXP);
      upsizer->SimpleResizeDo_uint16(VYFullYFF, nWidthP, nHeightP, VPitchY, VYSmallYFF, nBlkXP, nBlkXP);
      upsizerUV->SimpleResizeDo_uint16(VXFullUVFF, nWidthPUV, nHeightPUV, VPitchUV, VXSmallUVFF, nBlkXP, nBlkXP);
      upsizerUV->SimpleResizeDo_uint16(VYFullUVFF, nWidthPUV, nHeightPUV, VPitchUV, VYSmallUVFF, nBlkXP, nBlkXP);
      PROFILE_STOP(MOTION_PROFILE_RESIZE);

      PROFILE_START(MOTION_PROFILE_FLOWINTER);
      {
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
      PROFILE_STOP(MOTION_PROFILE_FLOWINTER);
      if (optDebug > 0) {
        char buf[2048];
        sprintf_s(buf, "FlowInter mode=2");
        DrawString(dst, vi, 0, 6, buf);
      }
    }
    else if (maskmode == 1) // old method without extra frames
    {
      PROFILE_START(MOTION_PROFILE_FLOWINTER);
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
        else { // pixelsize==2
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
      if (optDebug > 0) {
        char buf[2048];
        sprintf_s(buf, "FlowInter mode=1");
        DrawString(dst, vi, 0, 6, buf);
      }
      PROFILE_STOP(MOTION_PROFILE_FLOWINTER);
    }
    else // mode=0, faster simple method
    {

      PROFILE_START(MOTION_PROFILE_FLOWINTER);
      {
        if (pixelsize == 1) {
          FlowInterSimple<uint8_t>(pDst[0], nDstPitches[0], pRef[0] + nOffsetY, pSrc[0] + nOffsetY, nRefPitches[0],
            VXFullYB, VXFullYF, VYFullYB, VYFullYF, MaskFullYB, MaskFullYF, VPitchY,
            nWidth, nHeight, time256, nPel);
          FlowInterSimple<uint8_t>(pDst[1], nDstPitches[1], pRef[1] + nOffsetUV, pSrc[1] + nOffsetUV, nRefPitches[1],
            VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, MaskFullUVB, MaskFullUVF, VPitchUV,
            nWidthUV, nHeightUV, time256, nPel);
          FlowInterSimple<uint8_t>(pDst[2], nDstPitches[2], pRef[2] + nOffsetUV, pSrc[2] + nOffsetUV, nRefPitches[2],
            VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, MaskFullUVB, MaskFullUVF, VPitchUV,
            nWidthUV, nHeightUV, time256, nPel); // 2.5.11.22 Line 598
        }
        else { // pixelsize==2
          FlowInterSimple<uint16_t>(pDst[0], nDstPitches[0], pRef[0] + nOffsetY, pSrc[0] + nOffsetY, nRefPitches[0],
            VXFullYB, VXFullYF, VYFullYB, VYFullYF, MaskFullYB, MaskFullYF, VPitchY,
            nWidth, nHeight, time256, nPel);
          FlowInterSimple<uint16_t>(pDst[1], nDstPitches[1], pRef[1] + nOffsetUV, pSrc[1] + nOffsetUV, nRefPitches[1],
            VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, MaskFullUVB, MaskFullUVF, VPitchUV,
            nWidthUV, nHeightUV, time256, nPel);
          FlowInterSimple<uint16_t>(pDst[2], nDstPitches[2], pRef[2] + nOffsetUV, pSrc[2] + nOffsetUV, nRefPitches[2],
            VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, MaskFullUVB, MaskFullUVF, VPitchUV,
            nWidthUV, nHeightUV, time256, nPel); // 2.5.11.22 Line 598
        }
        if (optDebug > 0) {
          int sum_VXFullYB = 0;
          int sum_VXFullYF = 0;
          int sum_VYFullYB = 0;
          int sum_VYFullYF = 0;
          int sum_MaskFullYB = 0;
          int sum_MaskFullYF = 0;
          for (int y = 0; y < nHeight; y++)
            for (int x = 0; x < nWidth; x++) {
              sum_VXFullYB += VXFullYB[y * VPitchY + x];
              sum_VXFullYF += VXFullYF[y * VPitchY + x];
              sum_VYFullYB += VYFullYB[y * VPitchY + x];
              sum_VYFullYF += VYFullYF[y * VPitchY + x];
              sum_MaskFullYB += MaskFullYB[y * VPitchY + x];
              sum_MaskFullYF += MaskFullYF[y * VPitchY + x];
            }

          int sum_MaskSmallB = 0;
          int sum_MaskSmallF = 0;
          for (int y = 0; y < nBlkY; y++)
            for (int x = 0; x < nBlkX; x++) {
              sum_MaskSmallB += MaskSmallB[nBlkXP*y + x];
              sum_MaskSmallF += MaskSmallF[nBlkXP*y + x];
            }

          int sum_MaskSmallBP = 0;
          int sum_MaskSmallFP = 0;
          for (int y = 0; y < nBlkYP; y++)
            for (int x = 0; x < nBlkXP; x++) {
              sum_MaskSmallBP += MaskSmallB[nBlkXP*y + x];
              sum_MaskSmallFP += MaskSmallB[nBlkXP*y + x];
            }
          char buf[2048];
          sprintf_s(buf, "FlowInterSimple mode=0 or mode=2 not usable");
          DrawString(dst, vi, 0, 6, buf);
          sprintf_s(buf, "sum_VXFullYB=%d sum_VXFullYF=%d", sum_VXFullYB, sum_VXFullYF);
          DrawString(dst, vi, 0, 7, buf);
          sprintf_s(buf, "sum_VYFullYB=%d sum_VYFullYF=%d", sum_VYFullYB, sum_VYFullYF);
          DrawString(dst, vi, 0, 8, buf);
          sprintf_s(buf, "sum_MaskFullYB=%d sum_MaskFullYF=%d", sum_MaskFullYB, sum_MaskFullYF);
          DrawString(dst, vi, 0, 9, buf);
          sprintf_s(buf, "sum_MaskSmallBP=%d sum_MaskSmallFP=%d", sum_MaskSmallBP, sum_MaskSmallFP);
          DrawString(dst, vi, 0, 10, buf);
          sprintf_s(buf, "sum_MaskSmallB=%d sum_MaskSmallF=%d", sum_MaskSmallB, sum_MaskSmallF);
          DrawString(dst, vi, 0, 11, buf);
        }
        PROFILE_STOP(MOTION_PROFILE_FLOWINTER);
      }
      if (optDebug > 0) {
        char buf[2048];
        sprintf_s(buf, "FRAME %d time256=%d IsUsableB=%d IsUsableF=%d", n, time256, isUsableB ? 1 : 0, isUsableF ? 1 : 0);
        DrawString(dst, vi, 0, 0, buf);
        sprintf_s(buf, "B: BlkCount=%d OVx=%d OVy=%d thSCD1=%d thSCD2=%d", mvClipB.GetBlkCount(), mvClipB.GetOverlapX(), mvClipB.GetOverlapY(), mvClipB.GetThSCD1(), mvClipB.GetThSCD2());
        DrawString(dst, vi, 0, 1, buf);
        sprintf_s(buf, "F: BlkCount=%d OVx=%d OVy=%d thSCD1=%d thSCD2=%d", mvClipF.GetBlkCount(), mvClipF.GetOverlapX(), mvClipF.GetOverlapY(), mvClipF.GetThSCD1(), mvClipF.GetThSCD2());
        DrawString(dst, vi, 0, 2, buf);
        sprintf_s(buf, "nBlkX=%d nBlkY=%d nBlkXP=%d nBlkY=%d", nBlkX, nBlkY, nBlkXP, nBlkYP);
        DrawString(dst, vi, 0, 3, buf);
        sprintf_s(buf, "nright=%d nleft=%d", nright, nleft);
        DrawString(dst, vi, 0, 4, buf);
      }
    }

    PROFILE_START(MOTION_PROFILE_YUY2CONVERT);
    if ((pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2 && !planar)
    {
      YUY2FromPlanes(pDstYUY2, nDstPitchYUY2, nWidth, nHeight,
        pDst[0], nDstPitches[0], pDst[1], pDst[2], nDstPitches[1], cpuFlags);
    }
    PROFILE_STOP(MOTION_PROFILE_YUY2CONVERT);
    _RPT2(0, "MVFlowFPS GetFrame END, frame=%d id=%d\n", n, _instance_id);
    reentrancy_check = false;
    return dst;
  }
  else
  {
       // poor estimation

       // prepare pointers
    PVideoFrame src = child->GetFrame(nleft, env); // it is easy to use child here - v2.0

    if (blend) //let's blend src with ref frames like ConvertFPS
    {
      PVideoFrame ref = child->GetFrame(nright, env);
      PROFILE_START(MOTION_PROFILE_FLOWINTER);
      if ((pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2)
      {
        pSrc[0] = src->GetReadPtr(); // we can blend YUY2
        nSrcPitches[0] = src->GetPitch();
        pRef[0] = ref->GetReadPtr();
        nRefPitches[0] = ref->GetPitch();
        pDstYUY2 = dst->GetWritePtr();
        nDstPitchYUY2 = dst->GetPitch();
              //Blend(pDstYUY2, pSrc[0], pRef[0], nHeight, nWidth*2, nDstPitchYUY2, nSrcPitches[0], nRefPitches[0], t256_prov_cst, isse);
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
        /* 2.6.0.5: t256_prov_cst
       Blend(pDst[0], pSrc[0], pRef[0], nHeight, nWidth, nDstPitches[0], nSrcPitches[0], nRefPitches[0], t256_prov_cst, isse);
       Blend(pDst[1], pSrc[1], pRef[1], nHeightUV, nWidthUV, nDstPitches[1], nSrcPitches[1], nRefPitches[1], t256_prov_cst, isse);
       Blend(pDst[2], pSrc[2], pRef[2], nHeightUV, nWidthUV, nDstPitches[2], nSrcPitches[2], nRefPitches[2], t256_prov_cst, isse);
       */
        if (pixelsize == 1) {
          Blend<uint8_t>(pDst[0], pSrc[0], pRef[0], nHeight, nWidth, nDstPitches[0], nSrcPitches[0], nRefPitches[0], time256, cpuFlags);
          Blend<uint8_t>(pDst[1], pSrc[1], pRef[1], nHeightUV, nWidthUV, nDstPitches[1], nSrcPitches[1], nRefPitches[1], time256, cpuFlags);
          Blend<uint8_t>(pDst[2], pSrc[2], pRef[2], nHeightUV, nWidthUV, nDstPitches[2], nSrcPitches[2], nRefPitches[2], time256, cpuFlags);
        }
        else {
          Blend<uint16_t>(pDst[0], pSrc[0], pRef[0], nHeight, nWidth, nDstPitches[0], nSrcPitches[0], nRefPitches[0], time256, cpuFlags);
          Blend<uint16_t>(pDst[1], pSrc[1], pRef[1], nHeightUV, nWidthUV, nDstPitches[1], nSrcPitches[1], nRefPitches[1], time256, cpuFlags);
          Blend<uint16_t>(pDst[2], pSrc[2], pRef[2], nHeightUV, nWidthUV, nDstPitches[2], nSrcPitches[2], nRefPitches[2], time256, cpuFlags);
        }
      }
      PROFILE_STOP(MOTION_PROFILE_FLOWINTER);
      if (optDebug > 0) {
        char buf[2048];
        sprintf_s(buf, "BLEND %d time256=%d off=%d, nleft=%d, nright=%d, fa=%d, fb=%d, using left!", n, time256, off, nleft, nright, (int)fa, (int)fb);
        DrawString(dst, vi, 0, 0, buf);
        _RPT2(0, "MVFlowFPS GetFrame END BLEND, frame=%d id=%d\n", n, _instance_id);
      }

      reentrancy_check = false;
      return dst;
    }
    else
    {
      if (optDebug > 0) {
        env->MakeWritable(&src);
        char buf[2048];
        sprintf_s(buf, "POORNOBLEND %d time256=%d IsUsableB=%d IsUsableF=%d", n, time256, isUsableB ? 1 : 0, isUsableF ? 1 : 0);
        DrawString(src, vi, 0, 0, buf);
        sprintf_s(buf, "B: BlkCount=%d OVx=%d OVy=%d thSCD1=%d thSCD2=%d", mvClipB.GetBlkCount(), mvClipB.GetOverlapX(), mvClipB.GetOverlapY(), mvClipB.GetThSCD1(), mvClipB.GetThSCD2());
        DrawString(src, vi, 0, 1, buf);
        sprintf_s(buf, "F: BlkCount=%d OVx=%d OVy=%d thSCD1=%d thSCD2=%d", mvClipF.GetBlkCount(), mvClipF.GetOverlapX(), mvClipF.GetOverlapY(), mvClipF.GetThSCD1(), mvClipF.GetThSCD2());
        DrawString(src, vi, 0, 2, buf);
        sprintf_s(buf, "nright=%d nleft=%d", nright, nleft);
        DrawString(src, vi, 0, 4, buf);
        _RPT2(0, "MVFlowFPS GetFrame END POORNOBLEND, frame=%d id=%d\n", n, _instance_id);
      }
      reentrancy_check = false;
      return src; // like ChangeFPS
    }

  }

}
