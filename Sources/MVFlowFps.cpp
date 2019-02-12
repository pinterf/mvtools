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

  if (!vi.IsYUV() && !vi.IsYUVA() && !vi.IsPlanarRGB() && !vi.IsPlanarRGBA())
    env->ThrowError("MFlowFps: Clip must be Y, YUV or YUY2 or planar RGB/RGBA");

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
  int super_pixelsize = super->GetVideoInfo().ComponentSize();
  int vectors_pixelsize = pixelsize;
  int input_pixelsize = child->GetVideoInfo().ComponentSize();

  if (super_pixelsize != input_pixelsize)
    env->ThrowError("MFlowFps: input and super clip bit depth is different");

  if (nHeight != nHeightS
    || nWidth != nSuperWidth - nSuperHPad * 2
    || nPel != nSuperPel)
  {
    env->ThrowError("MFlowFps : wrong super frame clip");
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
  // 2.7.27- sometimes +1 is not enough 
  nBlkXP = nBlkX;
  while (nBlkXP*(nBlkSizeX - nOverlapX) + nOverlapX < nWidth)
    nBlkXP++;
  nBlkYP = nBlkY;
  while (nBlkYP*(nBlkSizeY - nOverlapY) + nOverlapY < nHeight)
    nBlkYP++;

  nWidthP = nBlkXP*(nBlkSizeX - nOverlapX) + nOverlapX;
  nHeightP = nBlkYP*(nBlkSizeY - nOverlapY) + nOverlapY;

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
  needDistinctChroma = !is444 && !isGrey && !isRGB;

  // 2*: sizeof(short)
  VXFullYB = (short*)_aligned_malloc(2 * nHeightP*VPitchY + 128, 128);
  VYFullYB = (short*)_aligned_malloc(2 * nHeightP*VPitchY + 128, 128);
  if (needDistinctChroma) {
    VYFullUVB = (short*)_aligned_malloc(2 * nHeightPUV*VPitchUV + 128, 128);
    VXFullUVB = (short*)_aligned_malloc(2 * nHeightPUV*VPitchUV + 128, 128);
  }

  VXFullYF = (short*)_aligned_malloc(2 * nHeightP*VPitchY + 128, 128);
  VYFullYF = (short*)_aligned_malloc(2 * nHeightP*VPitchY + 128, 128);
  if (needDistinctChroma) {
    VXFullUVF = (short*)_aligned_malloc(2 * nHeightPUV*VPitchUV + 128, 128);
    VYFullUVF = (short*)_aligned_malloc(2 * nHeightPUV*VPitchUV + 128, 128);
  }

  VXSmallYB = (short*)_aligned_malloc(2 * nBlkXP*nBlkYP + 128, 128);
  VYSmallYB = (short*)_aligned_malloc(2 * nBlkXP*nBlkYP + 128, 128);
  if (needDistinctChroma) {
    VXSmallUVB = (short*)_aligned_malloc(2 * nBlkXP*nBlkYP + 128, 128);
    VYSmallUVB = (short*)_aligned_malloc(2 * nBlkXP*nBlkYP + 128, 128);
  }

  VXSmallYF = (short*)_aligned_malloc(2 * nBlkXP*nBlkYP + 128, 128);
  VYSmallYF = (short*)_aligned_malloc(2 * nBlkXP*nBlkYP + 128, 128);
  if (needDistinctChroma) {
    VXSmallUVF = (short*)_aligned_malloc(2 * nBlkXP*nBlkYP + 128, 128);
    VYSmallUVF = (short*)_aligned_malloc(2 * nBlkXP*nBlkYP + 128, 128);
  }

  VXFullYBB = (short*)_aligned_malloc(2 * nHeightP*VPitchY + 128, 128);
  VYFullYBB = (short*)_aligned_malloc(2 * nHeightP*VPitchY + 128, 128);
  if (needDistinctChroma) {
    VXFullUVBB = (short*)_aligned_malloc(2 * nHeightPUV*VPitchUV + 128, 128);
    VYFullUVBB = (short*)_aligned_malloc(2 * nHeightPUV*VPitchUV + 128, 128);
  }

  VXFullYFF = (short*)_aligned_malloc(2 * nHeightP*VPitchY + 128, 128);
  VYFullYFF = (short*)_aligned_malloc(2 * nHeightP*VPitchY + 128, 128);
  if (needDistinctChroma) {
    VXFullUVFF = (short*)_aligned_malloc(2 * nHeightPUV*VPitchUV + 128, 128);
    VYFullUVFF = (short*)_aligned_malloc(2 * nHeightPUV*VPitchUV + 128, 128);
  }

  VXSmallYBB = (short*)_aligned_malloc(2 * nBlkXP*nBlkYP + 128, 128);
  VYSmallYBB = (short*)_aligned_malloc(2 * nBlkXP*nBlkYP + 128, 128);
  if (needDistinctChroma) {
    VXSmallUVBB = (short*)_aligned_malloc(2 * nBlkXP*nBlkYP + 128, 128);
    VYSmallUVBB = (short*)_aligned_malloc(2 * nBlkXP*nBlkYP + 128, 128);
  }

  VXSmallYFF = (short*)_aligned_malloc(2 * nBlkXP*nBlkYP + 128, 128);
  VYSmallYFF = (short*)_aligned_malloc(2 * nBlkXP*nBlkYP + 128, 128);
  if (needDistinctChroma) {
    VXSmallUVFF = (short*)_aligned_malloc(2 * nBlkXP*nBlkYP + 128, 128);
    VYSmallUVFF = (short*)_aligned_malloc(2 * nBlkXP*nBlkYP + 128, 128);
  }

  // PF remark: masks are 8 bits
  MaskSmallB = (unsigned char*)_aligned_malloc(nBlkXP*nBlkYP + 128, 128);
  MaskFullYB = (unsigned char*)_aligned_malloc(nHeightP*VPitchY + 128, 128);
  if(needDistinctChroma)
    MaskFullUVB = (unsigned char*)_aligned_malloc(nHeightPUV*VPitchUV + 128, 128);

  MaskSmallF = (unsigned char*)_aligned_malloc(nBlkXP*nBlkYP + 128, 128);
  MaskFullYF = (unsigned char*)_aligned_malloc(nHeightP*VPitchY + 128, 128);
  if(needDistinctChroma)
    MaskFullUVF = (unsigned char*)_aligned_malloc(nHeightPUV*VPitchUV + 128, 128);

  SADMaskSmallB = (unsigned char*)_aligned_malloc(nBlkXP*nBlkYP + 128, 128);
  SADMaskSmallF = (unsigned char*)_aligned_malloc(nBlkXP*nBlkYP + 128, 128);

  upsizer = new SimpleResize(nWidthP, nHeightP, nBlkXP, nBlkYP, cpuFlags);
  if(needDistinctChroma)
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
  if(needDistinctChroma)
    delete upsizerUV;

  _aligned_free(MaskSmallB);
  _aligned_free(MaskFullYB);
  if(needDistinctChroma)
    _aligned_free(MaskFullUVB);
  _aligned_free(MaskSmallF);
  _aligned_free(MaskFullYF);
  if (needDistinctChroma)
    _aligned_free(MaskFullUVF);

  _aligned_free(VXFullYBB);
  _aligned_free(VYFullYBB);
  if (needDistinctChroma) {
    _aligned_free(VXFullUVBB);
    _aligned_free(VYFullUVBB);
  }

  _aligned_free(VXSmallYBB);
  _aligned_free(VYSmallYBB);
  if (needDistinctChroma) {
    _aligned_free(VXSmallUVBB);
    _aligned_free(VYSmallUVBB);
  }

  _aligned_free(VXFullYFF);
  _aligned_free(VYFullYFF);
  if (needDistinctChroma) {
    _aligned_free(VXFullUVFF);
    _aligned_free(VYFullUVFF);
  }


  _aligned_free(VXSmallYFF);
  _aligned_free(VYSmallYFF);
  if (needDistinctChroma) {
    _aligned_free(VXSmallUVFF);
    _aligned_free(VYSmallUVFF);
  }

  _aligned_free(SADMaskSmallB);
  _aligned_free(SADMaskSmallF);

}

//-------------------------------------------------------------------------
PVideoFrame __stdcall MVFlowFps::GetFrame(int n, IScriptEnvironment* env)
{
  if (reentrancy_check) {
    env->ThrowError("MFlowFps: error in frame %d: Previous GetFrame did not finished properly. There was a crash or filter was set to reentrant multithread mode!", n);
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

    PROFILE_STOP(MOTION_PROFILE_YUY2CONVERT);

    int nOffsetY = nRefPitches[0] * nVPadding*nPel + nHPadding*nPel*pixelsize_super;
    int nOffsetUV = nRefPitches[1] * nVPaddingUV*nPel + nHPaddingUV*nPel*pixelsize_super;

    if (nright != nrightLast)
    {
      PROFILE_START(MOTION_PROFILE_MASK);
      // make  vector vx and vy small masks
      MakeVectorSmallMasks(mvClipB, nBlkX, nBlkY, VXSmallYB, nBlkXP, VYSmallYB, nBlkXP);

      CheckAndPadSmallY(VXSmallYB, VYSmallYB, nBlkXP, nBlkYP, nBlkX, nBlkY);

      if (needDistinctChroma) {
        VectorSmallMaskYToHalfUV(VXSmallYB, nBlkXP, nBlkYP, VXSmallUVB, xRatioUVs[1]);
        VectorSmallMaskYToHalfUV(VYSmallYB, nBlkXP, nBlkYP, VYSmallUVB, yRatioUVs[1]);
      }

      PROFILE_STOP(MOTION_PROFILE_MASK);
      // upsize (bilinear interpolate) vector masks to fullframe size
      PROFILE_START(MOTION_PROFILE_RESIZE);

      upsizer->SimpleResizeDo_int16(VXFullYB, nWidthP, nHeightP, VPitchY, VXSmallYB, nBlkXP, nBlkXP, nPel, true, nWidth, nHeight);
      upsizer->SimpleResizeDo_int16(VYFullYB, nWidthP, nHeightP, VPitchY, VYSmallYB, nBlkXP, nBlkXP, nPel, false, nWidth, nHeight);
      if (needDistinctChroma) {
        upsizerUV->SimpleResizeDo_int16(VXFullUVB, nWidthPUV, nHeightPUV, VPitchUV, VXSmallUVB, nBlkXP, nBlkXP, nPel, true, nWidthUV, nHeightUV);
        upsizerUV->SimpleResizeDo_int16(VYFullUVB, nWidthPUV, nHeightPUV, VPitchUV, VYSmallUVB, nBlkXP, nBlkXP, nPel, false, nWidthUV, nHeightUV);
      }
      PROFILE_STOP(MOTION_PROFILE_RESIZE);

    }
   // analyse vectors field to detect occlusion
   // Backward part
    PROFILE_START(MOTION_PROFILE_MASK);
    MakeVectorOcclusionMaskTime(mvClipB, nBlkX, nBlkY, ml, 1.0, nPel, MaskSmallB, nBlkXP, (256 - time256), nBlkSizeX - nOverlapX, nBlkSizeY - nOverlapY);

    CheckAndPadMaskSmall(MaskSmallB, nBlkXP, nBlkYP, nBlkX, nBlkY);

    PROFILE_STOP(MOTION_PROFILE_MASK);
    PROFILE_START(MOTION_PROFILE_RESIZE);
  // upsize (bilinear interpolate) vector masks to fullframe size
    upsizer->SimpleResizeDo_uint8(MaskFullYB, nWidthP, nHeightP, VPitchY, MaskSmallB, nBlkXP, nBlkXP);
    if(needDistinctChroma)
      upsizerUV->SimpleResizeDo_uint8(MaskFullUVB, nWidthPUV, nHeightPUV, VPitchUV, MaskSmallB, nBlkXP, nBlkXP);
    PROFILE_STOP(MOTION_PROFILE_RESIZE);

    nrightLast = nright;

    // Forward part
    if (nleft != nleftLast)
    {
     // make  vector vx and vy small masks
      PROFILE_START(MOTION_PROFILE_MASK);
      MakeVectorSmallMasks(mvClipF, nBlkX, nBlkY, VXSmallYF, nBlkXP, VYSmallYF, nBlkXP);

      CheckAndPadSmallY(VXSmallYF, VYSmallYF, nBlkXP, nBlkYP, nBlkX, nBlkY);

      if (needDistinctChroma) {
        VectorSmallMaskYToHalfUV(VXSmallYF, nBlkXP, nBlkYP, VXSmallUVF, xRatioUVs[1]);
        VectorSmallMaskYToHalfUV(VYSmallYF, nBlkXP, nBlkYP, VYSmallUVF, yRatioUVs[1]);
      }

      PROFILE_STOP(MOTION_PROFILE_MASK);
      // upsize (bilinear interpolate) vector masks to fullframe size
      PROFILE_START(MOTION_PROFILE_RESIZE);

      upsizer->SimpleResizeDo_int16(VXFullYF, nWidthP, nHeightP, VPitchY, VXSmallYF, nBlkXP, nBlkXP, nPel, true, nWidth, nHeight);
      upsizer->SimpleResizeDo_int16(VYFullYF, nWidthP, nHeightP, VPitchY, VYSmallYF, nBlkXP, nBlkXP, nPel, false, nWidth, nHeight);
      if (needDistinctChroma) {
        upsizerUV->SimpleResizeDo_int16(VXFullUVF, nWidthPUV, nHeightPUV, VPitchUV, VXSmallUVF, nBlkXP, nBlkXP, nPel, true, nWidthUV, nHeightUV);
        upsizerUV->SimpleResizeDo_int16(VYFullUVF, nWidthPUV, nHeightPUV, VPitchUV, VYSmallUVF, nBlkXP, nBlkXP, nPel, false, nWidthUV, nHeightUV);
      }
      PROFILE_STOP(MOTION_PROFILE_RESIZE);

    }
   // analyse vectors field to detect occlusion
   // Forward part
    PROFILE_START(MOTION_PROFILE_MASK);
    MakeVectorOcclusionMaskTime(mvClipF, nBlkX, nBlkY, ml, 1.0, nPel, MaskSmallF, nBlkXP, time256, nBlkSizeX - nOverlapX, nBlkSizeY - nOverlapY);

    CheckAndPadMaskSmall(MaskSmallF, nBlkXP, nBlkYP, nBlkX, nBlkY);

    PROFILE_STOP(MOTION_PROFILE_MASK);
    PROFILE_START(MOTION_PROFILE_RESIZE);
  // upsize (bilinear interpolate) vector masks to fullframe size
    upsizer->SimpleResizeDo_uint8(MaskFullYF, nWidthP, nHeightP, VPitchY, MaskSmallF, nBlkXP, nBlkXP);
    if(needDistinctChroma)
      upsizerUV->SimpleResizeDo_uint8(MaskFullUVF, nWidthPUV, nHeightPUV, VPitchUV, MaskSmallF, nBlkXP, nBlkXP);
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

      CheckAndPadSmallY_BF(VXSmallYBB, VXSmallYFF, VYSmallYBB, VYSmallYFF, nBlkXP, nBlkYP, nBlkX, nBlkY);

      if (needDistinctChroma) {
        VectorSmallMaskYToHalfUV(VXSmallYBB, nBlkXP, nBlkYP, VXSmallUVBB, xRatioUVs[1]);
        VectorSmallMaskYToHalfUV(VYSmallYBB, nBlkXP, nBlkYP, VYSmallUVBB, yRatioUVs[1]);
        VectorSmallMaskYToHalfUV(VXSmallYFF, nBlkXP, nBlkYP, VXSmallUVFF, xRatioUVs[1]);
        VectorSmallMaskYToHalfUV(VYSmallYFF, nBlkXP, nBlkYP, VYSmallUVFF, yRatioUVs[1]);
      }
      PROFILE_STOP(MOTION_PROFILE_MASK);

      PROFILE_START(MOTION_PROFILE_RESIZE);
    // upsize vectors to full frame
      upsizer->SimpleResizeDo_int16(VXFullYBB, nWidthP, nHeightP, VPitchY, VXSmallYBB, nBlkXP, nBlkXP, nPel, true, nWidth, nHeight);
      upsizer->SimpleResizeDo_int16(VYFullYBB, nWidthP, nHeightP, VPitchY, VYSmallYBB, nBlkXP, nBlkXP, nPel, false, nWidth, nHeight);
      if (needDistinctChroma) {
        upsizerUV->SimpleResizeDo_int16(VXFullUVBB, nWidthPUV, nHeightPUV, VPitchUV, VXSmallUVBB, nBlkXP, nBlkXP, nPel, true, nWidthUV, nHeightUV);
        upsizerUV->SimpleResizeDo_int16(VYFullUVBB, nWidthPUV, nHeightPUV, VPitchUV, VYSmallUVBB, nBlkXP, nBlkXP, nPel, false, nWidthUV, nHeightUV);
      }

      upsizer->SimpleResizeDo_int16(VXFullYFF, nWidthP, nHeightP, VPitchY, VXSmallYFF, nBlkXP, nBlkXP, nPel, true, nWidth, nHeight);
      upsizer->SimpleResizeDo_int16(VYFullYFF, nWidthP, nHeightP, VPitchY, VYSmallYFF, nBlkXP, nBlkXP, nPel, false, nWidth, nHeight);
      if (needDistinctChroma) {
        upsizerUV->SimpleResizeDo_int16(VXFullUVFF, nWidthPUV, nHeightPUV, VPitchUV, VXSmallUVFF, nBlkXP, nBlkXP, nPel, true, nWidthUV, nHeightUV);
        upsizerUV->SimpleResizeDo_int16(VYFullUVFF, nWidthPUV, nHeightPUV, VPitchUV, VYSmallUVFF, nBlkXP, nBlkXP, nPel, false, nWidthUV, nHeightUV);
      }
      PROFILE_STOP(MOTION_PROFILE_RESIZE);

      PROFILE_START(MOTION_PROFILE_FLOWINTER);
      {
        if (pixelsize_super == 1) {
          FlowInterExtra<uint8_t>(pDst[0], nDstPitches[0], pRef[0] + nOffsetY, pSrc[0] + nOffsetY, nRefPitches[0],
            VXFullYB, VXFullYF, VYFullYB, VYFullYF, MaskFullYB, MaskFullYF, VPitchY,
            nWidth, nHeight, time256, nPel, VXFullYBB, VXFullYFF, VYFullYBB, VYFullYFF);
          if (!isGrey) {
            if (needDistinctChroma) {
              FlowInterExtra<uint8_t>(pDst[1], nDstPitches[1], pRef[1] + nOffsetUV, pSrc[1] + nOffsetUV, nRefPitches[1],
                VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, MaskFullUVB, MaskFullUVF, VPitchUV,
                nWidthUV, nHeightUV, time256, nPel, VXFullUVBB, VXFullUVFF, VYFullUVBB, VYFullUVFF);
              FlowInterExtra<uint8_t>(pDst[2], nDstPitches[2], pRef[2] + nOffsetUV, pSrc[2] + nOffsetUV, nRefPitches[2],
                VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, MaskFullUVB, MaskFullUVF, VPitchUV,
                nWidthUV, nHeightUV, time256, nPel, VXFullUVBB, VXFullUVFF, VYFullUVBB, VYFullUVFF);
            }
            else {
              FlowInterExtra<uint8_t>(pDst[1], nDstPitches[1], pRef[1] + nOffsetY, pSrc[1] + nOffsetY, nRefPitches[1],
                VXFullYB, VXFullYF, VYFullYB, VYFullYF, MaskFullYB, MaskFullYF, VPitchY,
                nWidth, nHeight, time256, nPel, VXFullYBB, VXFullYFF, VYFullYBB, VYFullYFF);
              FlowInterExtra<uint8_t>(pDst[2], nDstPitches[2], pRef[2] + nOffsetY, pSrc[2] + nOffsetY, nRefPitches[2],
                VXFullYB, VXFullYF, VYFullYB, VYFullYF, MaskFullYB, MaskFullYF, VPitchY,
                nWidth, nHeight, time256, nPel, VXFullYBB, VXFullYFF, VYFullYBB, VYFullYFF);
            }
          }
        }
        else if (pixelsize_super == 2) {
          FlowInterExtra<uint16_t>(pDst[0], nDstPitches[0], pRef[0] + nOffsetY, pSrc[0] + nOffsetY, nRefPitches[0],
            VXFullYB, VXFullYF, VYFullYB, VYFullYF, MaskFullYB, MaskFullYF, VPitchY,
            nWidth, nHeight, time256, nPel, VXFullYBB, VXFullYFF, VYFullYBB, VYFullYFF);
          if (!isGrey) {
            if (needDistinctChroma) {
              FlowInterExtra<uint16_t>(pDst[1], nDstPitches[1], pRef[1] + nOffsetUV, pSrc[1] + nOffsetUV, nRefPitches[1],
                VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, MaskFullUVB, MaskFullUVF, VPitchUV,
                nWidthUV, nHeightUV, time256, nPel, VXFullUVBB, VXFullUVFF, VYFullUVBB, VYFullUVFF);
              FlowInterExtra<uint16_t>(pDst[2], nDstPitches[2], pRef[2] + nOffsetUV, pSrc[2] + nOffsetUV, nRefPitches[2],
                VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, MaskFullUVB, MaskFullUVF, VPitchUV,
                nWidthUV, nHeightUV, time256, nPel, VXFullUVBB, VXFullUVFF, VYFullUVBB, VYFullUVFF);
            }
            else {
              FlowInterExtra<uint16_t>(pDst[1], nDstPitches[1], pRef[1] + nOffsetY, pSrc[1] + nOffsetY, nRefPitches[1],
                VXFullYB, VXFullYF, VYFullYB, VYFullYF, MaskFullYB, MaskFullYF, VPitchY,
                nWidth, nHeight, time256, nPel, VXFullYBB, VXFullYFF, VYFullYBB, VYFullYFF);
              FlowInterExtra<uint16_t>(pDst[2], nDstPitches[2], pRef[2] + nOffsetY, pSrc[2] + nOffsetY, nRefPitches[2],
                VXFullYB, VXFullYF, VYFullYB, VYFullYF, MaskFullYB, MaskFullYF, VPitchY,
                nWidth, nHeight, time256, nPel, VXFullYBB, VXFullYFF, VYFullYBB, VYFullYFF);
            }
          }
        }
        else if (pixelsize_super == 4) {
          FlowInterExtra<float>(pDst[0], nDstPitches[0], pRef[0] + nOffsetY, pSrc[0] + nOffsetY, nRefPitches[0],
            VXFullYB, VXFullYF, VYFullYB, VYFullYF, MaskFullYB, MaskFullYF, VPitchY,
            nWidth, nHeight, time256, nPel, VXFullYBB, VXFullYFF, VYFullYBB, VYFullYFF);
          if (!isGrey) {
            if (needDistinctChroma) {
              FlowInterExtra<float>(pDst[1], nDstPitches[1], pRef[1] + nOffsetUV, pSrc[1] + nOffsetUV, nRefPitches[1],
                VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, MaskFullUVB, MaskFullUVF, VPitchUV,
                nWidthUV, nHeightUV, time256, nPel, VXFullUVBB, VXFullUVFF, VYFullUVBB, VYFullUVFF);
              FlowInterExtra<float>(pDst[2], nDstPitches[2], pRef[2] + nOffsetUV, pSrc[2] + nOffsetUV, nRefPitches[2],
                VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, MaskFullUVB, MaskFullUVF, VPitchUV,
                nWidthUV, nHeightUV, time256, nPel, VXFullUVBB, VXFullUVFF, VYFullUVBB, VYFullUVFF);
            }
            else {
              FlowInterExtra<float>(pDst[1], nDstPitches[1], pRef[1] + nOffsetY, pSrc[1] + nOffsetY, nRefPitches[1],
                VXFullYB, VXFullYF, VYFullYB, VYFullYF, MaskFullYB, MaskFullYF, VPitchY,
                nWidth, nHeight, time256, nPel, VXFullYBB, VXFullYFF, VYFullYBB, VYFullYFF);
              FlowInterExtra<float>(pDst[2], nDstPitches[2], pRef[2] + nOffsetY, pSrc[2] + nOffsetY, nRefPitches[2],
                VXFullYB, VXFullYF, VYFullYB, VYFullYF, MaskFullYB, MaskFullYF, VPitchY,
                nWidth, nHeight, time256, nPel, VXFullYBB, VXFullYFF, VYFullYBB, VYFullYFF);
            }
          }
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
        if (pixelsize_super == 1) {
          FlowInter<uint8_t>(pDst[0], nDstPitches[0], pRef[0] + nOffsetY, pSrc[0] + nOffsetY, nRefPitches[0],
            VXFullYB, VXFullYF, VYFullYB, VYFullYF, MaskFullYB, MaskFullYF, VPitchY,
            nWidth, nHeight, time256, nPel);
          if (!isGrey) {
            if (needDistinctChroma) {
              FlowInter<uint8_t>(pDst[1], nDstPitches[1], pRef[1] + nOffsetUV, pSrc[1] + nOffsetUV, nRefPitches[1],
                VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, MaskFullUVB, MaskFullUVF, VPitchUV,
                nWidthUV, nHeightUV, time256, nPel);
              FlowInter<uint8_t>(pDst[2], nDstPitches[2], pRef[2] + nOffsetUV, pSrc[2] + nOffsetUV, nRefPitches[2],
                VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, MaskFullUVB, MaskFullUVF, VPitchUV,
                nWidthUV, nHeightUV, time256, nPel);
            }
            else {
              FlowInter<uint8_t>(pDst[1], nDstPitches[1], pRef[1] + nOffsetY, pSrc[1] + nOffsetY, nRefPitches[1],
                VXFullYB, VXFullYF, VYFullYB, VYFullYF, MaskFullYB, MaskFullYF, VPitchY,
                nWidth, nHeight, time256, nPel);
              FlowInter<uint8_t>(pDst[2], nDstPitches[2], pRef[2] + nOffsetY, pSrc[2] + nOffsetY, nRefPitches[2],
                VXFullYB, VXFullYF, VYFullYB, VYFullYF, MaskFullYB, MaskFullYF, VPitchY,
                nWidth, nHeight, time256, nPel);
            }
          }
        }
        else if (pixelsize_super == 2) {
          FlowInter<uint16_t>(pDst[0], nDstPitches[0], pRef[0] + nOffsetY, pSrc[0] + nOffsetY, nRefPitches[0],
            VXFullYB, VXFullYF, VYFullYB, VYFullYF, MaskFullYB, MaskFullYF, VPitchY,
            nWidth, nHeight, time256, nPel);
          if (!isGrey) {
            if (needDistinctChroma) {
              FlowInter<uint16_t>(pDst[1], nDstPitches[1], pRef[1] + nOffsetUV, pSrc[1] + nOffsetUV, nRefPitches[1],
                VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, MaskFullUVB, MaskFullUVF, VPitchUV,
                nWidthUV, nHeightUV, time256, nPel);
              FlowInter<uint16_t>(pDst[2], nDstPitches[2], pRef[2] + nOffsetUV, pSrc[2] + nOffsetUV, nRefPitches[2],
                VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, MaskFullUVB, MaskFullUVF, VPitchUV,
                nWidthUV, nHeightUV, time256, nPel);
            }
            else {
              FlowInter<uint16_t>(pDst[1], nDstPitches[1], pRef[1] + nOffsetY, pSrc[1] + nOffsetY, nRefPitches[1],
                VXFullYB, VXFullYF, VYFullYB, VYFullYF, MaskFullYB, MaskFullYF, VPitchY,
                nWidth, nHeight, time256, nPel);
              FlowInter<uint16_t>(pDst[2], nDstPitches[2], pRef[2] + nOffsetY, pSrc[2] + nOffsetY, nRefPitches[2],
                VXFullYB, VXFullYF, VYFullYB, VYFullYF, MaskFullYB, MaskFullYF, VPitchY,
                nWidth, nHeight, time256, nPel);
            }
          }
        }
        else if (pixelsize_super == 4) {
          FlowInter<float>(pDst[0], nDstPitches[0], pRef[0] + nOffsetY, pSrc[0] + nOffsetY, nRefPitches[0],
            VXFullYB, VXFullYF, VYFullYB, VYFullYF, MaskFullYB, MaskFullYF, VPitchY,
            nWidth, nHeight, time256, nPel);
          if (!isGrey) {
            if (needDistinctChroma) {
              FlowInter<float>(pDst[1], nDstPitches[1], pRef[1] + nOffsetUV, pSrc[1] + nOffsetUV, nRefPitches[1],
                VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, MaskFullUVB, MaskFullUVF, VPitchUV,
                nWidthUV, nHeightUV, time256, nPel);
              FlowInter<float>(pDst[2], nDstPitches[2], pRef[2] + nOffsetUV, pSrc[2] + nOffsetUV, nRefPitches[2],
                VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, MaskFullUVB, MaskFullUVF, VPitchUV,
                nWidthUV, nHeightUV, time256, nPel);
            }
            else {
              FlowInter<float>(pDst[1], nDstPitches[1], pRef[1] + nOffsetY, pSrc[1] + nOffsetY, nRefPitches[1],
                VXFullYB, VXFullYF, VYFullYB, VYFullYF, MaskFullYB, MaskFullYF, VPitchY,
                nWidth, nHeight, time256, nPel);
              FlowInter<float>(pDst[2], nDstPitches[2], pRef[2] + nOffsetY, pSrc[2] + nOffsetY, nRefPitches[2],
                VXFullYB, VXFullYF, VYFullYB, VYFullYF, MaskFullYB, MaskFullYF, VPitchY,
                nWidth, nHeight, time256, nPel);
            }
          }
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
        if (pixelsize_super == 1) {
          FlowInterSimple<uint8_t>(pDst[0], nDstPitches[0], pRef[0] + nOffsetY, pSrc[0] + nOffsetY, nRefPitches[0],
            VXFullYB, VXFullYF, VYFullYB, VYFullYF, MaskFullYB, MaskFullYF, VPitchY,
            nWidth, nHeight, time256, nPel);
          if (!isGrey) {
            if (needDistinctChroma) {
              FlowInterSimple<uint8_t>(pDst[1], nDstPitches[1], pRef[1] + nOffsetUV, pSrc[1] + nOffsetUV, nRefPitches[1],
                VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, MaskFullUVB, MaskFullUVF, VPitchUV,
                nWidthUV, nHeightUV, time256, nPel);
              FlowInterSimple<uint8_t>(pDst[2], nDstPitches[2], pRef[2] + nOffsetUV, pSrc[2] + nOffsetUV, nRefPitches[2],
                VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, MaskFullUVB, MaskFullUVF, VPitchUV,
                nWidthUV, nHeightUV, time256, nPel); // 2.5.11.22 Line 598
            }
            else {
              FlowInterSimple<uint8_t>(pDst[1], nDstPitches[1], pRef[1] + nOffsetY, pSrc[1] + nOffsetY, nRefPitches[1],
                VXFullYB, VXFullYF, VYFullYB, VYFullYF, MaskFullYB, MaskFullYF, VPitchY,
                nWidth, nHeight, time256, nPel);
              FlowInterSimple<uint8_t>(pDst[2], nDstPitches[2], pRef[2] + nOffsetY, pSrc[2] + nOffsetY, nRefPitches[2],
                VXFullYB, VXFullYF, VYFullYB, VYFullYF, MaskFullYB, MaskFullYF, VPitchY,
                nWidth, nHeight, time256, nPel);
            }
          }
        }
        else if (pixelsize_super == 2) {
          FlowInterSimple<uint16_t>(pDst[0], nDstPitches[0], pRef[0] + nOffsetY, pSrc[0] + nOffsetY, nRefPitches[0],
            VXFullYB, VXFullYF, VYFullYB, VYFullYF, MaskFullYB, MaskFullYF, VPitchY,
            nWidth, nHeight, time256, nPel);
          if (!isGrey) {
            if (needDistinctChroma) {
              FlowInterSimple<uint16_t>(pDst[1], nDstPitches[1], pRef[1] + nOffsetUV, pSrc[1] + nOffsetUV, nRefPitches[1],
                VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, MaskFullUVB, MaskFullUVF, VPitchUV,
                nWidthUV, nHeightUV, time256, nPel);
              FlowInterSimple<uint16_t>(pDst[2], nDstPitches[2], pRef[2] + nOffsetUV, pSrc[2] + nOffsetUV, nRefPitches[2],
                VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, MaskFullUVB, MaskFullUVF, VPitchUV,
                nWidthUV, nHeightUV, time256, nPel); // 2.5.11.22 Line 598
            }
            else {
              FlowInterSimple<uint16_t>(pDst[1], nDstPitches[1], pRef[1] + nOffsetY, pSrc[1] + nOffsetY, nRefPitches[1],
                VXFullYB, VXFullYF, VYFullYB, VYFullYF, MaskFullYB, MaskFullYF, VPitchY,
                nWidth, nHeight, time256, nPel);
              FlowInterSimple<uint16_t>(pDst[2], nDstPitches[2], pRef[2] + nOffsetY, pSrc[2] + nOffsetY, nRefPitches[2],
                VXFullYB, VXFullYF, VYFullYB, VYFullYF, MaskFullYB, MaskFullYF, VPitchY,
                nWidth, nHeight, time256, nPel);
            }
          }
        }
        else if (pixelsize_super == 4) {
          FlowInterSimple<float>(pDst[0], nDstPitches[0], pRef[0] + nOffsetY, pSrc[0] + nOffsetY, nRefPitches[0],
            VXFullYB, VXFullYF, VYFullYB, VYFullYF, MaskFullYB, MaskFullYF, VPitchY,
            nWidth, nHeight, time256, nPel);
          if (!isGrey) {
            if (needDistinctChroma) {
              FlowInterSimple<float>(pDst[1], nDstPitches[1], pRef[1] + nOffsetUV, pSrc[1] + nOffsetUV, nRefPitches[1],
                VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, MaskFullUVB, MaskFullUVF, VPitchUV,
                nWidthUV, nHeightUV, time256, nPel);
              FlowInterSimple<float>(pDst[2], nDstPitches[2], pRef[2] + nOffsetUV, pSrc[2] + nOffsetUV, nRefPitches[2],
                VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, MaskFullUVB, MaskFullUVF, VPitchUV,
                nWidthUV, nHeightUV, time256, nPel); // 2.5.11.22 Line 598
            }
            else {
              FlowInterSimple<float>(pDst[1], nDstPitches[1], pRef[1] + nOffsetY, pSrc[1] + nOffsetY, nRefPitches[1],
                VXFullYB, VXFullYF, VYFullYB, VYFullYF, MaskFullYB, MaskFullYF, VPitchY,
                nWidth, nHeight, time256, nPel);
              FlowInterSimple<float>(pDst[2], nDstPitches[2], pRef[2] + nOffsetY, pSrc[2] + nOffsetY, nRefPitches[2],
                VXFullYB, VXFullYF, VYFullYB, VYFullYF, MaskFullYB, MaskFullYF, VPitchY,
                nWidth, nHeight, time256, nPel);
            }
          }
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
            if (needDistinctChroma) {
              Blend<uint8_t>(pDst[1], pSrc[1], pRef[1], nHeightUV, nWidthUV, nDstPitches[1], nSrcPitches[1], nRefPitches[1], time256, cpuFlags);
              Blend<uint8_t>(pDst[2], pSrc[2], pRef[2], nHeightUV, nWidthUV, nDstPitches[2], nSrcPitches[2], nRefPitches[2], time256, cpuFlags);
            }
            else {
              Blend<uint8_t>(pDst[1], pSrc[1], pRef[1], nHeight, nWidth, nDstPitches[1], nSrcPitches[1], nRefPitches[1], time256, cpuFlags);
              Blend<uint8_t>(pDst[2], pSrc[2], pRef[2], nHeight, nWidth, nDstPitches[2], nSrcPitches[2], nRefPitches[2], time256, cpuFlags);
            }
          }
        }
        else if (pixelsize_super == 2) {
          Blend<uint16_t>(pDst[0], pSrc[0], pRef[0], nHeight, nWidth, nDstPitches[0], nSrcPitches[0], nRefPitches[0], time256, cpuFlags);
          if (!isGrey) {
            if (needDistinctChroma) {
              Blend<uint16_t>(pDst[1], pSrc[1], pRef[1], nHeightUV, nWidthUV, nDstPitches[1], nSrcPitches[1], nRefPitches[1], time256, cpuFlags);
              Blend<uint16_t>(pDst[2], pSrc[2], pRef[2], nHeightUV, nWidthUV, nDstPitches[2], nSrcPitches[2], nRefPitches[2], time256, cpuFlags);
            }
            else {
              Blend<uint16_t>(pDst[1], pSrc[1], pRef[1], nHeight, nWidth, nDstPitches[1], nSrcPitches[1], nRefPitches[1], time256, cpuFlags);
              Blend<uint16_t>(pDst[2], pSrc[2], pRef[2], nHeight, nWidth, nDstPitches[2], nSrcPitches[2], nRefPitches[2], time256, cpuFlags);
            }
          }
        }
        else if (pixelsize_super == 4) {
          Blend<float>(pDst[0], pSrc[0], pRef[0], nHeight, nWidth, nDstPitches[0], nSrcPitches[0], nRefPitches[0], time256, cpuFlags);
          if (!isGrey) {
            if (needDistinctChroma) {
              Blend<float>(pDst[1], pSrc[1], pRef[1], nHeightUV, nWidthUV, nDstPitches[1], nSrcPitches[1], nRefPitches[1], time256, cpuFlags);
              Blend<float>(pDst[2], pSrc[2], pRef[2], nHeightUV, nWidthUV, nDstPitches[2], nSrcPitches[2], nRefPitches[2], time256, cpuFlags);
            }
            else {
              Blend<float>(pDst[1], pSrc[1], pRef[1], nHeight, nWidth, nDstPitches[1], nSrcPitches[1], nRefPitches[1], time256, cpuFlags);
              Blend<float>(pDst[2], pSrc[2], pRef[2], nHeight, nWidth, nDstPitches[2], nSrcPitches[2], nRefPitches[2], time256, cpuFlags);
            }
          }
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
