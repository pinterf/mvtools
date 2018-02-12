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
#include "SuperParams64Bits.h"


MVFlowBlur::MVFlowBlur(PClip _child, PClip super, PClip _mvbw, PClip _mvfw, int _blur256, int _prec,
  int nSCD1, int nSCD2, bool _isse, bool _planar, IScriptEnvironment* env) :
  GenericVideoFilter(_child),
  MVFilter(_mvfw, "MFlowBlur", env, 1, 0),
  mvClipB(_mvbw, nSCD1, nSCD2, env, 1, 0),
  mvClipF(_mvfw, nSCD1, nSCD2, env, 1, 0)
{
  blur256 = _blur256;
  prec = _prec;
  cpuFlags = _isse ? env->GetCPUFlags() : 0;
  planar = _planar;

  CheckSimilarity(mvClipB, "mvbw", env);
  CheckSimilarity(mvClipF, "mvfw", env);

  if (mvClipB.GetDeltaFrame() <= 0 || mvClipF.GetDeltaFrame() <= 0)
    env->ThrowError("MFlowBlur: cannot use motion vectors with absolute frame references.");

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


  nHeightUV = nHeight / yRatioUV;
  nWidthUV = nWidth / xRatioUV;// orig: /2 for YV12
  nHPaddingUV = nHPadding / xRatioUV;
  nVPaddingUV = nHPadding / yRatioUV;

  VPitchY = nWidth;
  VPitchUV = nWidthUV;

  VXFullYB = (short*)_aligned_malloc(2 * nHeight*VPitchY + 128, 128);
  VXFullUVB = (short*)_aligned_malloc(2 * nHeightUV*VPitchUV + 128, 128);
  VYFullYB = (short*)_aligned_malloc(2 * nHeight*VPitchY + 128, 128);
  VYFullUVB = (short*)_aligned_malloc(2 * nHeightUV*VPitchUV + 128, 128);

  VXFullYF = (short*)_aligned_malloc(2 * nHeight*VPitchY + 128, 128);
  VXFullUVF = (short*)_aligned_malloc(2 * nHeightUV*VPitchUV + 128, 128);
  VYFullYF = (short*)_aligned_malloc(2 * nHeight*VPitchY + 128, 128);
  VYFullUVF = (short*)_aligned_malloc(2 * nHeightUV*VPitchUV + 128, 128);

  VXSmallYB = (short*)_aligned_malloc(2 * nBlkX*nBlkY + 128, 128);
  VYSmallYB = (short*)_aligned_malloc(2 * nBlkX*nBlkY + 128, 128);
  VXSmallUVB = (short*)_aligned_malloc(2 * nBlkX*nBlkY + 128, 128);
  VYSmallUVB = (short*)_aligned_malloc(2 * nBlkX*nBlkY + 128, 128);

  VXSmallYF = (short*)_aligned_malloc(2 * nBlkX*nBlkY + 128, 128);
  VYSmallYF = (short*)_aligned_malloc(2 * nBlkX*nBlkY + 128, 128);
  VXSmallUVF = (short*)_aligned_malloc(2 * nBlkX*nBlkY + 128, 128);
  VYSmallUVF = (short*)_aligned_malloc(2 * nBlkX*nBlkY + 128, 128);

  MaskSmallB = (unsigned char*)_aligned_malloc(nBlkX*nBlkY + 128, 128);
  MaskFullYB = (unsigned char*)_aligned_malloc(nHeight*VPitchY + 128, 128);
  MaskFullUVB = (unsigned char*)_aligned_malloc(nHeightUV*VPitchUV + 128, 128);

  MaskSmallF = (unsigned char*)_aligned_malloc(nBlkX*nBlkY + 128, 128);
  MaskFullYF = (unsigned char*)_aligned_malloc(nHeight*VPitchY + 128, 128);
  MaskFullUVF = (unsigned char*)_aligned_malloc(nHeightUV*VPitchUV + 128, 128);

  upsizer = new SimpleResize(nWidth, nHeight, nBlkX, nBlkY, cpuFlags);
  upsizerUV = new SimpleResize(nWidthUV, nHeightUV, nBlkX, nBlkY, cpuFlags);

  if ((pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2 && !planar)
  {
    DstPlanes = new YUY2Planes(nWidth, nHeight);
  }
}

MVFlowBlur::~MVFlowBlur()
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

  // very slow, but precise motion blur
  for (int h = 0; h < height; h++)
  {
    int heightLimitRel = ((height - h) << nLOGPEL) - 1;

    for (int w = 0; w < width; w++)
    {
      int rel_x, rel_y;

      int bluredsum = pref[w << nLOGPEL];

      // 2.7.24: limit x and y to prevent overflow in pel ref frame indexing
      // valid pref[x;y] is [0..(height<<nLogPel)-1 ; 0..(width<<nLogPel)-1]

      // forward
      rel_x = VXFullF[w];

      if (rel_x >= (width - w) << nLOGPEL)
        rel_x = ((width - w) << nLOGPEL) - 1;
      else if (rel_x + (w << nLOGPEL) < 0)
        rel_x = -(w << nLOGPEL);

      rel_y = VYFullF[w];

      if (rel_y > heightLimitRel)
        rel_y = heightLimitRel;
      else if (rel_y + (h << 1) < 0)
        rel_y = -(h << 1);

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
          int dstF = pref[(vyF >> 8)*ref_pitch + (vxF >> 8) + (w << nLOGPEL)];
          bluredsum += dstF;
          vxF += vxF0;
          vyF += vyF0;
        }
      }

      // backward
      rel_x = VXFullB[w];

      if (rel_x >= (width - w) << nLOGPEL)
        rel_x = ((width - w) << nLOGPEL) - 1;
      else if (rel_x + (w << nLOGPEL) < 0)
        rel_x = -(w << nLOGPEL);

      rel_y = VYFullB[w];

      if (rel_y > heightLimitRel)
        rel_y = heightLimitRel;
      else if (rel_y + (h << nLOGPEL) < 0)
        rel_y = -(h << nLOGPEL);

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
          int dstB = pref[(vyB >> 8)*ref_pitch + (vxB >> 8) + (w << nLOGPEL)];
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
  unsigned char *pDstYUY2;
  int nDstPitchYUY2;

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
    dst = env->NewVideoFrame(vi);

    if ((pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2)
    {
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
    }

    // refPitches[] is already knowing about nPel
    // but nHPadding and nVPadding need to be corrected accordingly
    int nOffsetY = nRefPitches[0] * (nVPadding*nPel) + (nHPadding*nPel)*pixelsize;
    int nOffsetUV = nRefPitches[1] * (nVPaddingUV*nPel) + (nHPaddingUV*nPel)*pixelsize;


    // make  vector vx and vy small masks
    MakeVectorSmallMasks(mvClipB, nBlkX, nBlkY, VXSmallYB, nBlkX, VYSmallYB, nBlkX);
    VectorSmallMaskYToHalfUV(VXSmallYB, nBlkX, nBlkY, VXSmallUVB, xRatioUV);
    VectorSmallMaskYToHalfUV(VYSmallYB, nBlkX, nBlkY, VYSmallUVB, yRatioUV);

    MakeVectorSmallMasks(mvClipF, nBlkX, nBlkY, VXSmallYF, nBlkX, VYSmallYF, nBlkX);
    VectorSmallMaskYToHalfUV(VXSmallYF, nBlkX, nBlkY, VXSmallUVF, xRatioUV);
    VectorSmallMaskYToHalfUV(VYSmallYF, nBlkX, nBlkY, VYSmallUVF, yRatioUV);

      // analyse vectors field to detect occlusion

      // upsize (bilinear interpolate) vector masks to fullframe size


    int dummyplane = PLANAR_Y; // use luma plane resizer code for all planes if we resize from luma small mask
    upsizer->SimpleResizeDo_uint16(VXFullYB, nWidth, nHeight, VPitchY, VXSmallYB, nBlkX, nBlkX);
    upsizer->SimpleResizeDo_uint16(VYFullYB, nWidth, nHeight, VPitchY, VYSmallYB, nBlkX, nBlkX);
    upsizerUV->SimpleResizeDo_uint16(VXFullUVB, nWidthUV, nHeightUV, VPitchUV, VXSmallUVB, nBlkX, nBlkX);
    upsizerUV->SimpleResizeDo_uint16(VYFullUVB, nWidthUV, nHeightUV, VPitchUV, VYSmallUVB, nBlkX, nBlkX);

    upsizer->SimpleResizeDo_uint16(VXFullYF, nWidth, nHeight, VPitchY, VXSmallYF, nBlkX, nBlkX);
    upsizer->SimpleResizeDo_uint16(VYFullYF, nWidth, nHeight, VPitchY, VYSmallYF, nBlkX, nBlkX);
    upsizerUV->SimpleResizeDo_uint16(VXFullUVF, nWidthUV, nHeightUV, VPitchUV, VXSmallUVF, nBlkX, nBlkX);
    upsizerUV->SimpleResizeDo_uint16(VYFullUVF, nWidthUV, nHeightUV, VPitchUV, VYSmallUVF, nBlkX, nBlkX);


    if (pixelsize == 1) {
      if (nPel == 1) {
        FlowBlur<uint8_t, 0>(pDst[0], nDstPitches[0], pRef[0] + nOffsetY, nRefPitches[0],
          VXFullYB, VXFullYF, VYFullYB, VYFullYF, VPitchY,
          nWidth, nHeight, blur256, prec);
        FlowBlur<uint8_t, 0>(pDst[1], nDstPitches[1], pRef[1] + nOffsetUV, nRefPitches[1],
          VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, VPitchUV,
          nWidthUV, nHeightUV, blur256, prec);
        FlowBlur<uint8_t, 0>(pDst[2], nDstPitches[2], pRef[2] + nOffsetUV, nRefPitches[2],
          VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, VPitchUV,
          nWidthUV, nHeightUV, blur256, prec);
      }
      else if(nPel == 2) {
        FlowBlur<uint8_t, 1>(pDst[0], nDstPitches[0], pRef[0] + nOffsetY, nRefPitches[0],
          VXFullYB, VXFullYF, VYFullYB, VYFullYF, VPitchY,
          nWidth, nHeight, blur256, prec);
        FlowBlur<uint8_t, 1>(pDst[1], nDstPitches[1], pRef[1] + nOffsetUV, nRefPitches[1],
          VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, VPitchUV,
          nWidthUV, nHeightUV, blur256, prec);
        FlowBlur<uint8_t, 1>(pDst[2], nDstPitches[2], pRef[2] + nOffsetUV, nRefPitches[2],
          VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, VPitchUV,
          nWidthUV, nHeightUV, blur256, prec);
      }
      else if (nPel == 4) {
        FlowBlur<uint8_t, 2>(pDst[0], nDstPitches[0], pRef[0] + nOffsetY, nRefPitches[0],
          VXFullYB, VXFullYF, VYFullYB, VYFullYF, VPitchY,
          nWidth, nHeight, blur256, prec);
        FlowBlur<uint8_t, 2>(pDst[1], nDstPitches[1], pRef[1] + nOffsetUV, nRefPitches[1],
          VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, VPitchUV,
          nWidthUV, nHeightUV, blur256, prec);
        FlowBlur<uint8_t, 2>(pDst[2], nDstPitches[2], pRef[2] + nOffsetUV, nRefPitches[2],
          VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, VPitchUV,
          nWidthUV, nHeightUV, blur256, prec);
      }
    }
    else { // pixelsize == 2
      if (nPel == 1) {
        FlowBlur<uint16_t, 0>(pDst[0], nDstPitches[0], pRef[0] + nOffsetY, nRefPitches[0],
          VXFullYB, VXFullYF, VYFullYB, VYFullYF, VPitchY,
          nWidth, nHeight, blur256, prec);
        FlowBlur<uint16_t, 0>(pDst[1], nDstPitches[1], pRef[1] + nOffsetUV, nRefPitches[1],
          VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, VPitchUV,
          nWidthUV, nHeightUV, blur256, prec);
        FlowBlur<uint16_t, 0>(pDst[2], nDstPitches[2], pRef[2] + nOffsetUV, nRefPitches[2],
          VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, VPitchUV,
          nWidthUV, nHeightUV, blur256, prec);
      }
      else if (nPel == 2) {
        FlowBlur<uint16_t, 1>(pDst[0], nDstPitches[0], pRef[0] + nOffsetY, nRefPitches[0],
          VXFullYB, VXFullYF, VYFullYB, VYFullYF, VPitchY,
          nWidth, nHeight, blur256, prec);
        FlowBlur<uint16_t, 1>(pDst[1], nDstPitches[1], pRef[1] + nOffsetUV, nRefPitches[1],
          VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, VPitchUV,
          nWidthUV, nHeightUV, blur256, prec);
        FlowBlur<uint16_t, 1>(pDst[2], nDstPitches[2], pRef[2] + nOffsetUV, nRefPitches[2],
          VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, VPitchUV,
          nWidthUV, nHeightUV, blur256, prec);
      }
      else if (nPel == 4) {
        FlowBlur<uint16_t, 2>(pDst[0], nDstPitches[0], pRef[0] + nOffsetY, nRefPitches[0],
          VXFullYB, VXFullYF, VYFullYB, VYFullYF, VPitchY,
          nWidth, nHeight, blur256, prec);
        FlowBlur<uint16_t, 2>(pDst[1], nDstPitches[1], pRef[1] + nOffsetUV, nRefPitches[1],
          VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, VPitchUV,
          nWidthUV, nHeightUV, blur256, prec);
        FlowBlur<uint16_t, 2>(pDst[2], nDstPitches[2], pRef[2] + nOffsetUV, nRefPitches[2],
          VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, VPitchUV,
          nWidthUV, nHeightUV, blur256, prec);
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
    return child->GetFrame(n, env);
  }

}
