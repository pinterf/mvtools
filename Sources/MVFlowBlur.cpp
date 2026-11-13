// Pixels flow motion blur function
// Copyright(c)2005 A.G.Balakhnin aka Fizick

// See legal notice in Copying.txt for more information

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation;  version 2 of the License.
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
#include "MaskFun.h"
#include "MVFinest.h"
#include "MVFlowBlur.h"
#include "SuperParams.h"
#include <algorithm>
#include "commonfunctions.h"


MVFlowBlur::MVFlowBlur(PClip _child, PClip super, PClip _mvbw, PClip _mvfw, int _blur256, int _prec,
  int nSCD1, int nSCD2, bool _isse, IScriptEnvironment* env) :
  GenericVideoFilter(_child),
  MVFilter(_mvfw, "MFlowBlur", env, 1, 0, true), // edge blocks: the upsized vector field covers the block grid, the vectors are limited to the frame
  mvClipB(_mvbw, nSCD1, nSCD2, env, 1, 0),
  mvClipF(_mvfw, nSCD1, nSCD2, env, 1, 0)
{
  has_at_least_v8 = true;
  try { env->CheckVersion(8); }
  catch (const AvisynthError&) { has_at_least_v8 = false; }

  blur256 = _blur256;
  prec = _prec;
  cpuFlags = _isse ? env->GetCPUFlags() : 0;

  CheckSimilarity(mvClipB, "mvbw", env);
  CheckSimilarity(mvClipF, "mvfw", env);

  if (mvClipB.GetDeltaFrame() <= 0 || mvClipF.GetDeltaFrame() <= 0)
    env->ThrowError("MFlowBlur: cannot use motion vectors with absolute frame references.");

  const SuperParams params = GetSuperParams(super->GetVideoInfo());
  int nHeightS = params.nHeight;
  int nSuperHPad = params.nHPad;
  //int nSuperVPad = params.nVPad;
  int nSuperPel = params.nPel;
  //int nSuperModeYUV = params.nModeYUV;
  //int nSuperLevels = params.nLevels;

  if (!super->GetVideoInfo().IsSameColorspace(child->GetVideoInfo()))
    env->ThrowError("MFlowBlur: input and super clip format is different");

  // params.nWidth / nHeight: the real frame size, without the edgepad extension of the super planes
  if (nHeight != nHeightS
    || nWidth != params.nWidth
    || nPel != nSuperPel)
  {
    env->ThrowError("MFlowBlur : wrong super frame clip");
  }

  if (nPel == 1)
    finest = super; // v2.0.9.1
  else
  {
    finest = new MVFinest(super, _isse, env);
    AVSValue cache_args[1] = { finest };
    finest = env->Invoke("InternalCache", AVSValue(cache_args, 1)).AsClip(); // add cache for speed
  }

//	if (   nWidth  != vi.width  || (nWidth  + nHPadding*2)*nPel != finest->GetVideoInfo().width
//	    || nHeight != vi.height || (nHeight + nVPadding*2)*nPel != finest->GetVideoInfo().height)
//		env->ThrowError("MVFlowBlur: wrong source of finest frame size");

  // actual format can differ from the base clip of vectors
  pixelsize_super = super->GetVideoInfo().ComponentSize();
  bits_per_pixel_super = super->GetVideoInfo().BitsPerComponent();
  pixelsize_super_shift = ilog2(pixelsize_super);

  planecount = ClipFnc::nbr_planes_processed(vi);

  xRatioUVs[0] = xRatioUVs[1] = xRatioUVs[2] = 1;
  yRatioUVs[0] = yRatioUVs[1] = yRatioUVs[2] = 1;

  if (!ClipFnc::is_grey(vi) && !vi.IsRGB()) {
    xRatioUVs[1] = xRatioUVs[2] = (1 << vi.GetPlaneWidthSubsampling(PLANAR_U));
    yRatioUVs[1] = yRatioUVs[2] = (1 << vi.GetPlaneHeightSubsampling(PLANAR_U));
  }

  for (int i = 0; i < planecount; i++) {
    nLogxRatioUVs[i] = ilog2(xRatioUVs[i]);
    nLogyRatioUVs[i] = ilog2(yRatioUVs[i]);
  }

  nHeightUV = nHeight / yRatioUVs[1];
  nWidthUV = nWidth / xRatioUVs[1];
  nHPaddingUV = nHPadding / xRatioUVs[1];
  nVPaddingUV = nVPadding / yRatioUVs[1]; // up to 2.7.47 (already in Fizick's 2.5.11.3): nHPadding was used, wrong chroma offset when hpad != vpad

  // The small vector fields are upsized to full resolution (bilinear, one value per block centre). The upsized
  // field must have the size of the block grid, else the block centres are moved: with a grid ending left of / above
  // the frame edge the vectors would be stretched over the frame (each block shifted right / down a bit, towards
  // the uncovered strip). So the grid is padded with extra block columns / rows until it covers the frame
  // (nBlkXP, nBlkYP; CheckAndPadSmallY repeats the vectors of the last real column / row, with the component
  // pointing out of the frame clamped to 0, so the blur reads stay inside the frame), as in MFlow,
  // MFlowInter and MFlowFps; the upsized field is then nWidthP x nHeightP and only its frame part is used.
  // With edge blocks (MSuper edgepad) the grid already covers the frame: nBlkXP == nBlkX, nWidthP >= nWidth.
  nBlkXP = nBlkX;
  while (nBlkXP * (nBlkSizeX - nOverlapX) + nOverlapX < nWidth)
    nBlkXP++;
  nBlkYP = nBlkY;
  while (nBlkYP * (nBlkSizeY - nOverlapY) + nOverlapY < nHeight)
    nBlkYP++;

  nWidthP = nBlkXP * (nBlkSizeX - nOverlapX) + nOverlapX;
  nHeightP = nBlkYP * (nBlkSizeY - nOverlapY) + nOverlapY;
  nWidthPUV = nWidthP / xRatioUVs[1];
  nHeightPUV = nHeightP / yRatioUVs[1];

  VPitchY = nWidthP;
  VPitchUV = nWidthPUV;

  is444 = vi.Is444();
  isGrey = ClipFnc::is_grey(vi);
  isRGB = vi.IsPlanarRGB() || vi.IsPlanarRGBA(); // planar only
  //needDistinctChroma = !is444 && !isGrey && !isRGB;

  VXFullYB = (short*)_aligned_malloc(2 * nHeightP*VPitchY + 128, 128);
  VYFullYB = (short*)_aligned_malloc(2 * nHeightP*VPitchY + 128, 128);
  if (!isGrey) {
    VXFullUVB = (short*)_aligned_malloc(2 * nHeightPUV*VPitchUV + 128, 128);
    VYFullUVB = (short*)_aligned_malloc(2 * nHeightPUV*VPitchUV + 128, 128);
  }

  VXFullYF = (short*)_aligned_malloc(2 * nHeightP*VPitchY + 128, 128);
  VYFullYF = (short*)_aligned_malloc(2 * nHeightP*VPitchY + 128, 128);
  if (!isGrey) {
    VXFullUVF = (short*)_aligned_malloc(2 * nHeightPUV*VPitchUV + 128, 128);
    VYFullUVF = (short*)_aligned_malloc(2 * nHeightPUV*VPitchUV + 128, 128);
  }

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

  upsizer = new SimpleResize(nWidthP, nHeightP, nBlkXP, nBlkYP, cpuFlags);
  if (!isGrey) 
    upsizerUV = new SimpleResize(nWidthPUV, nHeightPUV, nBlkXP, nBlkYP, cpuFlags);

}

MVFlowBlur::~MVFlowBlur()
{

  delete upsizer;
  if(!isGrey)
    delete upsizerUV;

  _aligned_free(VXFullYB);
  _aligned_free(VYFullYB);
  if (!isGrey) {
    _aligned_free(VXFullUVB);
    _aligned_free(VYFullUVB);
  }
  _aligned_free(VXSmallYB);
  _aligned_free(VYSmallYB);
  if (!isGrey) {
    _aligned_free(VXSmallUVB);
    _aligned_free(VYSmallUVB);
  }
  _aligned_free(VXFullYF);
  _aligned_free(VYFullYF);
  if (!isGrey) {
    _aligned_free(VXFullUVF);
    _aligned_free(VYFullUVF);
  }
  _aligned_free(VXSmallYF);
  _aligned_free(VYSmallYF);
  if (!isGrey) {
    _aligned_free(VXSmallUVF);
    _aligned_free(VYSmallUVF);
  }
}

template<typename pixel_t, int nLOGPEL>
void MVFlowBlur::FlowBlur(BYTE * pdst8, int dst_pitch, const BYTE *pref8, int ref_pitch,
  short *VXFullB, short *VXFullF, short *VYFullB, short *VYFullF,
  int VPitch, int width, int height, int blur256, int prec)
{
  dst_pitch /= sizeof(pixel_t);
  ref_pitch /= sizeof(pixel_t);
  pixel_t *pdst = reinterpret_cast<pixel_t *>(pdst8);
  const pixel_t *pref = reinterpret_cast<const pixel_t *>(pref8);

  // type for sum of pixel_t pixels
  typedef typename std::conditional < sizeof(pixel_t) == 4, float, int>::type accum_t;

  // very slow, but precise motion blur
  for (int h = 0; h < height; h++)
  {
    for (int w = 0; w < width; w++)
    {
      int rel_x, rel_y;

      accum_t bluredsum = pref[w << nLOGPEL];

      // forward
      rel_x = VXFullF[w];
      rel_y = VYFullF[w];

      int vxF0 = (rel_x * blur256);
      int vyF0 = (rel_y * blur256);

      int mF = (std::max(abs(vxF0), abs(vyF0)) / prec) >> 8;
      if (mF > 0)
      {
        vxF0 /= mF;
        vyF0 /= mF;
        int vxF = vxF0;
        int vyF = vyF0;
        for (int i = 0; i < mF; i++)
        {
          pixel_t dstF = pref[(vyF >> 8)*ref_pitch + (vxF >> 8) + (w << nLOGPEL)];
          bluredsum += dstF;
          vxF += vxF0;
          vyF += vyF0;
        }
      }

      // backward
      rel_x = VXFullB[w];
      rel_y = VYFullB[w];

      int vxB0 = (rel_x * blur256);
      int vyB0 = (rel_y * blur256);
      int mB = (std::max(abs(vxB0), abs(vyB0)) / prec) >> 8;
      if (mB > 0)
      {
        vxB0 /= mB;
        vyB0 /= mB;
        int vxB = vxB0;
        int vyB = vyB0;
        for (int i = 0; i < mB; i++)
        {
          pixel_t dstB = pref[(vyB >> 8)*ref_pitch + (vxB >> 8) + (w << nLOGPEL)];
          bluredsum += dstB;
          vxB += vxB0;
          vyB += vyB0;
        }
      }
      pdst[w] = bluredsum / (mF + mB + 1);
    }
    pdst += dst_pitch;
    pref += (ref_pitch << nLOGPEL); // ref_pitch is already doubled e.g. for nLogPel=2 (nPel=2), but vertically we have to step by 2 to reach the same height
    VXFullB += VPitch;
    VYFullB += VPitch;
    VXFullF += VPitch;
    VYFullF += VPitch;
  }
}
//-------------------------------------------------------------------------
PVideoFrame __stdcall MVFlowBlur::GetFrame(int n, IScriptEnvironment* env)
{
  PVideoFrame dst;
  BYTE *pDst[3];
  const BYTE *pRef[3];
  int nDstPitches[3], nRefPitches[3];

  int off = mvClipB.GetDeltaFrame(); // integer offset of reference frame
  if (off <= 0)
  {
    env->ThrowError("MFlowBlur: cannot use motion vectors with absolute frame references.");
  }
  if (!mvClipB.IsBackward())
    off = -off;

  PVideoFrame mvF = mvClipF.GetFrame(n + off, env);
  mvClipF.Update(mvF, env);// forward from current to next
  mvF = 0;

  PVideoFrame mvB = mvClipB.GetFrame(n - off, env);
  mvClipB.Update(mvB, env);// backward from current to prev
  mvB = 0;

  if (mvClipB.IsUsable() && mvClipF.IsUsable())
  {
    PVideoFrame ref = finest->GetFrame(n, env);//  ref for  compensation
    dst = has_at_least_v8 ? env->NewVideoFrameP(vi, &ref) : env->NewVideoFrame(vi); // frame property support

    int planes_y[4] = { PLANAR_Y, PLANAR_U, PLANAR_V, PLANAR_A };
    int planes_r[4] = { PLANAR_G, PLANAR_B, PLANAR_R, PLANAR_A };
    int *planes = (vi.IsYUV() || vi.IsYUVA()) ? planes_y : planes_r;
    planecount = ClipFnc::nbr_planes_processed(vi);
    for (int p = 0; p < planecount; ++p) {
      const int plane = planes[p];

      pRef[p] = ref->GetReadPtr(plane);
      nRefPitches[p] = ref->GetPitch(plane);

      pDst[p] = dst->GetWritePtr(plane);
      nDstPitches[p] = dst->GetPitch(plane);
    }

    // refPitches[] is already knowing about nPel
    // but nHPadding and nVPadding need to be corrected accordingly
    int nOffsetY = nRefPitches[0] * (nVPadding*nPel) + (nHPadding*nPel)*pixelsize_super;
    int nOffsetUV = nRefPitches[1] * (nVPaddingUV*nPel) + (nHPaddingUV*nPel)*pixelsize_super;


    // make  vector vx and vy small masks
    // (row pitch nBlkXP: the padding columns / rows beyond the real grid are filled by CheckAndPadSmallY_BF)
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

      // upsize (bilinear interpolate) vector masks to fullframe size

    upsizer->SimpleResizeDo_int16(VXFullYB, nWidthP, nHeightP, VPitchY, VXSmallYB, nBlkXP, nBlkXP, nPel, true, nWidth, nHeight);
    upsizer->SimpleResizeDo_int16(VYFullYB, nWidthP, nHeightP, VPitchY, VYSmallYB, nBlkXP, nBlkXP, nPel, false, nWidth, nHeight);
    if (!isGrey) {
      upsizerUV->SimpleResizeDo_int16(VXFullUVB, nWidthPUV, nHeightPUV, VPitchUV, VXSmallUVB, nBlkXP, nBlkXP, nPel, true, nWidthUV, nHeightUV);
      upsizerUV->SimpleResizeDo_int16(VYFullUVB, nWidthPUV, nHeightPUV, VPitchUV, VYSmallUVB, nBlkXP, nBlkXP, nPel, false, nWidthUV, nHeightUV);
    }

    upsizer->SimpleResizeDo_int16(VXFullYF, nWidthP, nHeightP, VPitchY, VXSmallYF, nBlkXP, nBlkXP, nPel, true, nWidth, nHeight);
    upsizer->SimpleResizeDo_int16(VYFullYF, nWidthP, nHeightP, VPitchY, VYSmallYF, nBlkXP, nBlkXP, nPel, false, nWidth, nHeight);
    if (!isGrey) {
      upsizerUV->SimpleResizeDo_int16(VXFullUVF, nWidthPUV, nHeightPUV, VPitchUV, VXSmallUVF, nBlkXP, nBlkXP, nPel, true, nWidthUV, nHeightUV);
      upsizerUV->SimpleResizeDo_int16(VYFullUVF, nWidthPUV, nHeightPUV, VPitchUV, VYSmallUVF, nBlkXP, nBlkXP, nPel, false, nWidthUV, nHeightUV);
    }

    if (pixelsize_super == 1) {
      if (nPel == 1) {
        FlowBlur<uint8_t, 0>(pDst[0], nDstPitches[0], pRef[0] + nOffsetY, nRefPitches[0],
          VXFullYB, VXFullYF, VYFullYB, VYFullYF, VPitchY,
          nWidth, nHeight, blur256, prec);
        if (!isGrey) {
          FlowBlur<uint8_t, 0>(pDst[1], nDstPitches[1], pRef[1] + nOffsetUV, nRefPitches[1],
            VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, VPitchUV,
            nWidthUV, nHeightUV, blur256, prec);
          FlowBlur<uint8_t, 0>(pDst[2], nDstPitches[2], pRef[2] + nOffsetUV, nRefPitches[2],
            VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, VPitchUV,
            nWidthUV, nHeightUV, blur256, prec);
        }
      }
      else if(nPel == 2) {
        FlowBlur<uint8_t, 1>(pDst[0], nDstPitches[0], pRef[0] + nOffsetY, nRefPitches[0],
          VXFullYB, VXFullYF, VYFullYB, VYFullYF, VPitchY,
          nWidth, nHeight, blur256, prec);
        if (!isGrey) {
          FlowBlur<uint8_t, 1>(pDst[1], nDstPitches[1], pRef[1] + nOffsetUV, nRefPitches[1],
            VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, VPitchUV,
            nWidthUV, nHeightUV, blur256, prec);
          FlowBlur<uint8_t, 1>(pDst[2], nDstPitches[2], pRef[2] + nOffsetUV, nRefPitches[2],
            VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, VPitchUV,
            nWidthUV, nHeightUV, blur256, prec);
        }
      }
      else if (nPel == 4) {
        FlowBlur<uint8_t, 2>(pDst[0], nDstPitches[0], pRef[0] + nOffsetY, nRefPitches[0],
          VXFullYB, VXFullYF, VYFullYB, VYFullYF, VPitchY,
          nWidth, nHeight, blur256, prec);
        if (!isGrey) {
          FlowBlur<uint8_t, 2>(pDst[1], nDstPitches[1], pRef[1] + nOffsetUV, nRefPitches[1],
            VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, VPitchUV,
            nWidthUV, nHeightUV, blur256, prec);
          FlowBlur<uint8_t, 2>(pDst[2], nDstPitches[2], pRef[2] + nOffsetUV, nRefPitches[2],
            VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, VPitchUV,
            nWidthUV, nHeightUV, blur256, prec);
        }
      }
    }
    else if (pixelsize_super == 2) {
      if (nPel == 1) {
        FlowBlur<uint16_t, 0>(pDst[0], nDstPitches[0], pRef[0] + nOffsetY, nRefPitches[0],
          VXFullYB, VXFullYF, VYFullYB, VYFullYF, VPitchY,
          nWidth, nHeight, blur256, prec);
        if (!isGrey) {
          FlowBlur<uint16_t, 0>(pDst[1], nDstPitches[1], pRef[1] + nOffsetUV, nRefPitches[1],
            VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, VPitchUV,
            nWidthUV, nHeightUV, blur256, prec);
          FlowBlur<uint16_t, 0>(pDst[2], nDstPitches[2], pRef[2] + nOffsetUV, nRefPitches[2],
            VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, VPitchUV,
            nWidthUV, nHeightUV, blur256, prec);
        }
      }
      else if (nPel == 2) {
        FlowBlur<uint16_t, 1>(pDst[0], nDstPitches[0], pRef[0] + nOffsetY, nRefPitches[0],
          VXFullYB, VXFullYF, VYFullYB, VYFullYF, VPitchY,
          nWidth, nHeight, blur256, prec);
        if (!isGrey) {
          FlowBlur<uint16_t, 1>(pDst[1], nDstPitches[1], pRef[1] + nOffsetUV, nRefPitches[1],
            VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, VPitchUV,
            nWidthUV, nHeightUV, blur256, prec);
          FlowBlur<uint16_t, 1>(pDst[2], nDstPitches[2], pRef[2] + nOffsetUV, nRefPitches[2],
            VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, VPitchUV,
            nWidthUV, nHeightUV, blur256, prec);
        }
      }
      else if (nPel == 4) {
        FlowBlur<uint16_t, 2>(pDst[0], nDstPitches[0], pRef[0] + nOffsetY, nRefPitches[0],
          VXFullYB, VXFullYF, VYFullYB, VYFullYF, VPitchY,
          nWidth, nHeight, blur256, prec);
        if (!isGrey) {
          FlowBlur<uint16_t, 2>(pDst[1], nDstPitches[1], pRef[1] + nOffsetUV, nRefPitches[1],
            VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, VPitchUV,
            nWidthUV, nHeightUV, blur256, prec);
          FlowBlur<uint16_t, 2>(pDst[2], nDstPitches[2], pRef[2] + nOffsetUV, nRefPitches[2],
            VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, VPitchUV,
            nWidthUV, nHeightUV, blur256, prec);
        }
      }
    }
    else if (pixelsize_super == 4) {
      if (nPel == 1) {
        FlowBlur<float, 0>(pDst[0], nDstPitches[0], pRef[0] + nOffsetY, nRefPitches[0],
          VXFullYB, VXFullYF, VYFullYB, VYFullYF, VPitchY,
          nWidth, nHeight, blur256, prec);
        if (!isGrey) {
          FlowBlur<float, 0>(pDst[1], nDstPitches[1], pRef[1] + nOffsetUV, nRefPitches[1],
            VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, VPitchUV,
            nWidthUV, nHeightUV, blur256, prec);
          FlowBlur<float, 0>(pDst[2], nDstPitches[2], pRef[2] + nOffsetUV, nRefPitches[2],
            VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, VPitchUV,
            nWidthUV, nHeightUV, blur256, prec);
        }
      }
      else if (nPel == 2) {
        FlowBlur<float, 1>(pDst[0], nDstPitches[0], pRef[0] + nOffsetY, nRefPitches[0],
          VXFullYB, VXFullYF, VYFullYB, VYFullYF, VPitchY,
          nWidth, nHeight, blur256, prec);
        if (!isGrey) {
          FlowBlur<float, 1>(pDst[1], nDstPitches[1], pRef[1] + nOffsetUV, nRefPitches[1],
            VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, VPitchUV,
            nWidthUV, nHeightUV, blur256, prec);
          FlowBlur<float, 1>(pDst[2], nDstPitches[2], pRef[2] + nOffsetUV, nRefPitches[2],
            VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, VPitchUV,
            nWidthUV, nHeightUV, blur256, prec);
        }
      }
      else if (nPel == 4) {
        FlowBlur<float, 2>(pDst[0], nDstPitches[0], pRef[0] + nOffsetY, nRefPitches[0],
          VXFullYB, VXFullYF, VYFullYB, VYFullYF, VPitchY,
          nWidth, nHeight, blur256, prec);
        if (!isGrey) {
          FlowBlur<float, 2>(pDst[1], nDstPitches[1], pRef[1] + nOffsetUV, nRefPitches[1],
            VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, VPitchUV,
            nWidthUV, nHeightUV, blur256, prec);
          FlowBlur<float, 2>(pDst[2], nDstPitches[2], pRef[2] + nOffsetUV, nRefPitches[2],
            VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, VPitchUV,
            nWidthUV, nHeightUV, blur256, prec);
        }
      }
    }

    ClipFnc::copy_alpha_from(dst, child, n, vi, env); // alpha of the current frame
    return dst;
  }
  else
  {
    return child->GetFrame(n, env);
  }

}
