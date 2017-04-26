// Create an overlay mask with the motion vectors
// Author: Manao
// Copyright(c)2006 A.G.Balakhnin aka Fizick - YUY2, occlusion
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

#include "ClipFnc.h"
#include "CopyCode.h"
#include "MaskFun.h"
#include "MVClip.h"
#include "MVFilter.h"
#include "MVMask.h"

#include <cmath>
//#include <intrin.h>



MVMask::MVMask(
  PClip _child, PClip vectors, double ml, double gm, int _kind, double _time100, int Ysc,
  sad_t nSCD1, int nSCD2, bool _isse, bool _planar, IScriptEnvironment* env
)
  : GenericVideoFilter(_child)
  , mvClip(vectors, nSCD1, nSCD2, env, 1, 0)
  , MVFilter(vectors, "MMask", env, 1, 0)
{
  isse = _isse;
  fMaskNormFactor = 1.0f / ml; // Fizick
  fMaskNormFactor2 = fMaskNormFactor*fMaskNormFactor;
  
  // base: YY12 luma+chroma 8x8 -> 2, proportionally bigger with ((1<<bits_per_pixel)-8) and (nBlkSizeX*nBlkSizeY)
  fSADMaskNormFactor = (double)mvClip.GetThSCD1() / nSCD1 / 3 * 8*8 * 2; // 2.7.17.22 correction for YV16/YV24 (4:2:2/4:4:4)

//	nLengthMax = pow((double(ml),gm);

  fGamma = gm;
  if (gm < 0)
    fGamma = 1;
  fHalfGamma = fGamma*0.5f;

  kind = _kind; // inplace of showsad

  if (_time100 < 0 || _time100 > 100)
    env->ThrowError("MMask: time must be 0.0 to 100.0");
  time256 = int(256 * _time100 / 100);

  nSceneChangeValue = (Ysc < 0) ? 0 : ((Ysc > 255) ? 255 : Ysc);
  planar = _planar;

  smallMask = new unsigned char[nBlkX * nBlkY];
  smallMaskV = new unsigned char[nBlkX * nBlkY];

  nWidthB = nBlkX*(nBlkSizeX - nOverlapX) + nOverlapX;
  nHeightB = nBlkY*(nBlkSizeY - nOverlapY) + nOverlapY;

  nHeightUV = nHeight / yRatioUV;
  nWidthUV = nWidth / xRatioUV;// for YV12
  nHeightBUV = nHeightB / yRatioUV;
  nWidthBUV = nWidthB / xRatioUV;

  int CPUF_Resize = env->GetCPUFlags();
  if (!isse) CPUF_Resize = (CPUF_Resize & !CPUF_INTEGER_SSE) & !CPUF_SSE2;
 // old upsizer replaced by Fizick
  upsizer = new SimpleResize(nWidthB, nHeightB, nBlkX, nBlkY, CPUF_Resize);
  upsizerUV = new SimpleResize(nWidthBUV, nHeightBUV, nBlkX, nBlkY, CPUF_Resize);


  if ((pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2 && !planar)
  {
    DstPlanes = new YUY2Planes(nWidth, nHeight);
    SrcPlanes = new YUY2Planes(nWidth, nHeight);
  }

}


MVMask::~MVMask()
{
  if ((pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2 && !planar)
  {
    delete DstPlanes;
    delete SrcPlanes;
  }
  delete[] smallMask;
  delete[] smallMaskV;
  delete upsizer;
  delete upsizerUV;
}

unsigned char MVMask::Length(VECTOR v, unsigned char pel)
{
  double norme = double(v.x * v.x + v.y * v.y) / (pel * pel);

//	double l = 255 * pow(norme,nGamma) / nLengthMax;
  double l = 255 * pow(norme*fMaskNormFactor2, fHalfGamma); //Fizick - simply rewritten

  return (unsigned char)((l > 255) ? 255 : l);
}

/*
unsigned char MVMask::SAD(unsigned int s)
{
//	double l = 255 * pow(s, nGamma) / nLengthMax);
  double l = 255 * pow((s*4*fMaskNormFactor)/(nBlkSizeX*nBlkSizeY), fGamma); // Fizick - now linear for gm=1
  return (unsigned char)((l > 255) ? 255 : l);
}
*/
PVideoFrame __stdcall MVMask::GetFrame(int n, IScriptEnvironment* env)
{
  PVideoFrame	src = child->GetFrame(n, env);
  PVideoFrame	dst = env->NewVideoFrame(vi);
  int dummyplane = PLANAR_Y; // use luma plane resizer code for all planes if we resize from luma small mask
  const BYTE *pSrc[3];
  BYTE *pDst[3];
  int nDstPitches[3];
  int nSrcPitches[3];
  unsigned char *pDstYUY2;
  const unsigned char *pSrcYUY2;
  int nDstPitchYUY2;
  int nSrcPitchYUY2;

  if ((pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2)
  {
    if (!planar)
    {
      pSrcYUY2 = src->GetReadPtr();
      nSrcPitchYUY2 = src->GetPitch();
      pSrc[0] = SrcPlanes->GetPtr();
      pSrc[1] = SrcPlanes->GetPtrU();
      pSrc[2] = SrcPlanes->GetPtrV();
      nSrcPitches[0] = SrcPlanes->GetPitch();
      nSrcPitches[1] = SrcPlanes->GetPitchUV();
      nSrcPitches[2] = SrcPlanes->GetPitchUV();
      YUY2ToPlanes(pSrcYUY2, nSrcPitchYUY2, nWidth, nHeight,
        pSrc[0], nSrcPitches[0], pSrc[1], pSrc[2], nSrcPitches[1], isse);
      pDst[0] = DstPlanes->GetPtr();
      pDst[1] = DstPlanes->GetPtrU();
      pDst[2] = DstPlanes->GetPtrV();
      nDstPitches[0] = DstPlanes->GetPitch();
      nDstPitches[1] = DstPlanes->GetPitchUV();
      nDstPitches[2] = DstPlanes->GetPitchUV();
//			YUY2ToPlanes(pDstYUY2, nDstPitchYUY2, nWidth, nHeight,
//				pDst[0], nDstPitches[0], pDst[1], pDst[2], nDstPitches[1], isse);
    }
    else
    {
      // planar data packed to interleaved format (same as interleved2planar by kassandro) - v2.0.0.5
      pSrc[0] = src->GetReadPtr();
      pSrc[1] = pSrc[0] + nWidth;
      pSrc[2] = pSrc[1] + nWidth / 2;
      nSrcPitches[0] = src->GetPitch();
      nSrcPitches[1] = nSrcPitches[0];
      nSrcPitches[2] = nSrcPitches[0];
      pDst[0] = dst->GetWritePtr();
      pDst[1] = pDst[0] + nWidth;
      pDst[2] = pDst[1] + nWidth / 2; // YUY2
      nDstPitches[0] = dst->GetPitch();
      nDstPitches[1] = nDstPitches[0];
      nDstPitches[2] = nDstPitches[0];
    }
  }
  else
  {
    pSrc[0] = YRPLAN(src);
    pSrc[1] = URPLAN(src);
    pSrc[2] = VRPLAN(src);
    nSrcPitches[0] = YPITCH(src);
    nSrcPitches[1] = UPITCH(src);
    nSrcPitches[2] = VPITCH(src);
    pDst[0] = YWPLAN(dst);
    pDst[1] = UWPLAN(dst);
    pDst[2] = VWPLAN(dst);
    nDstPitches[0] = YPITCH(dst);
    nDstPitches[1] = UPITCH(dst);
    nDstPitches[2] = VPITCH(dst);
  }

  PVideoFrame mvn = mvClip.GetFrame(n, env);
  mvClip.Update(mvn, env);
#ifndef _M_X64
  _mm_empty();
#endif

  if (mvClip.IsUsable())
  {
    // smallMask is 8 bits, this is the target
    if (kind == 0) // vector length mask
    {
      for (int j = 0; j < nBlkCount; j++)
        smallMask[j] = Length(mvClip.GetBlock(0, j).GetMV(), mvClip.GetPel());
    }
    else if (kind == 1) // SAD mask
    {
      //for ( int j = 0; j < nBlkCount; j++)
      //	smallMask[j] = SAD(mvClip.GetBlock(0, j).GetSAD()); 
      double factor_corrected = 4.0*fMaskNormFactor / fSADMaskNormFactor; // normalize for other format's (4:2:2/4:4:4) bigger luma part and/or chroma=false. Base: YV12's luma+chroma SAD
      double factor_old = 4.0*fMaskNormFactor / (nBlkSizeX*nBlkSizeY) / (1 << (bits_per_pixel - 8)); // kept for reference. factor_corrected is the same for old YV12 (compatibility)

      MakeSADMaskTime(mvClip, nBlkX, nBlkY, factor_corrected, fGamma, nPel, smallMask, nBlkX, time256, nBlkSizeX - nOverlapX, nBlkSizeY - nOverlapY);
    }
    else if (kind == 2) // occlusion mask
    {
      //MakeVectorOcclusionMaskTime(mvClip, nBlkX, nBlkY, fMaskNormFactor, fGamma, nPel, smallMask, nBlkX, 256, nBlkSizeX - nOverlapX, nBlkSizeY - nOverlapY) ;
      MakeVectorOcclusionMaskTime(mvClip, nBlkX, nBlkY, 1.0 / fMaskNormFactor, fGamma, nPel, smallMask, nBlkX, time256, nBlkSizeX - nOverlapX, nBlkSizeY - nOverlapY);
    }
    else if (kind == 3) // vector x mask
    {
      for (int j = 0; j < nBlkCount; j++)
        smallMask[j] = std::max(int(0), std::min(255, int(mvClip.GetBlock(0, j).GetMV().x * fMaskNormFactor * 100 + 128))); // shited by 128 for signed support
      //smallMask[j] = mvClip.GetBlock(0, j).GetMV().x + 128; // shited by 128 for signed support
    }
    else if (kind == 4) // vector y mask
    {
      for (int j = 0; j < nBlkCount; j++)
        smallMask[j] = std::max(int(0), std::min(255, int(mvClip.GetBlock(0, j).GetMV().y * fMaskNormFactor * 100 + 128))); // shited by 128 for signed support
      //smallMask[j] = mvClip.GetBlock(0, j).GetMV().y + 128; // shited by 128 for signed support
    }
    else if (kind == 5) // vector x mask in U, y mask in V
    {
      for (int j = 0; j < nBlkCount; j++) {
        VECTOR v = mvClip.GetBlock(0, j).GetMV();
      //smallMask[j] = v.x + 128; // shited by 128 for signed support
      //smallMaskV[j] = v.y + 128; // shited by 128 for signed support
        smallMask[j] = std::max(0, std::min(255, int(v.x * fMaskNormFactor * 100 + 128))); // shifted by 128 for signed support
        smallMaskV[j] = std::max(0, std::min(255, int(v.y * fMaskNormFactor * 100 + 128))); // shifted by 128 for signed support
      }
    }

    if (kind == 5) { // do not change luma for kind=5
      env->BitBlt(pDst[0], nDstPitches[0], pSrc[0], nSrcPitches[0], nWidth*pixelsize, nHeight);
    }
    else {
      // different upsizers to scale smallMask from 8 bit-only to bits_per_pixel
      if (pixelsize == 1) {
        upsizer->SimpleResizeDo_uint8(pDst[0], nWidthB, nHeightB, nDstPitches[0], smallMask, nBlkX, nBlkX, dummyplane);
        if (nWidth > nWidthB)
          for (int h = 0; h < nHeight; h++)
            for (int w = nWidthB; w < nWidth; w++)
              *(pDst[0] + h*nDstPitches[0] + w) = *(pDst[0] + h*nDstPitches[0] + (nWidthB - 1));
        if (nHeight > nHeightB)
          env->BitBlt(pDst[0] + nHeightB*nDstPitches[0], nDstPitches[0], pDst[0] + (nHeightB - 1)*nDstPitches[0], nDstPitches[0], nWidth, nHeight - nHeightB);
      }
      else if (pixelsize == 2) {
        // 8 bit source, 10-16 bit target
        upsizer->SimpleResizeDo_uint8_to_uint16(pDst[0], nWidthB, nHeightB, nDstPitches[0], smallMask, nBlkX, nBlkX, dummyplane, bits_per_pixel);
        if (nWidth > nWidthB)
          for (int h = 0; h < nHeight; h++)
            for (int w = nWidthB; w < nWidth; w++)
              *(uint16_t *)(pDst[0] + h*nDstPitches[0] + w*pixelsize) = *(uint16_t *)(pDst[0] + h*nDstPitches[0] + (nWidthB - 1) * pixelsize);
        if (nHeight > nHeightB)
          env->BitBlt(pDst[0] + nHeightB*nDstPitches[0], nDstPitches[0], pDst[0] + (nHeightB - 1)*nDstPitches[0], nDstPitches[0], nWidth*pixelsize, nHeight - nHeightB);
      }
    }


    if (pixelsize == 1) {
      // chroma
      upsizerUV->SimpleResizeDo_uint8(pDst[1], nWidthBUV, nHeightBUV, nDstPitches[1], smallMask, nBlkX, nBlkX, dummyplane);

      if (kind == 5)
        upsizerUV->SimpleResizeDo_uint8(pDst[2], nWidthBUV, nHeightBUV, nDstPitches[2], smallMaskV, nBlkX, nBlkX, dummyplane);
      else
        upsizerUV->SimpleResizeDo_uint8(pDst[2], nWidthBUV, nHeightBUV, nDstPitches[2], smallMask, nBlkX, nBlkX, dummyplane);
      if (nWidthUV > nWidthBUV)
        for (int h = 0; h < nHeightUV; h++)
          for (int w = nWidthBUV; w < nWidthUV; w++)
          {
            *(pDst[1] + h*nDstPitches[1] + w) = *(pDst[1] + h*nDstPitches[1] + nWidthBUV - 1);
            *(pDst[2] + h*nDstPitches[2] + w) = *(pDst[2] + h*nDstPitches[2] + nWidthBUV - 1);
          }
    }
    else {
      // chroma
      upsizerUV->SimpleResizeDo_uint8_to_uint16(pDst[1], nWidthBUV, nHeightBUV, nDstPitches[1], smallMask, nBlkX, nBlkX, dummyplane, bits_per_pixel);

      if (kind == 5)
        upsizerUV->SimpleResizeDo_uint8_to_uint16(pDst[2], nWidthBUV, nHeightBUV, nDstPitches[2], smallMaskV, nBlkX, nBlkX, dummyplane, bits_per_pixel);
      else
        upsizerUV->SimpleResizeDo_uint8_to_uint16(pDst[2], nWidthBUV, nHeightBUV, nDstPitches[2], smallMask, nBlkX, nBlkX, dummyplane, bits_per_pixel);
      if (nWidthUV > nWidthBUV)
        for (int h = 0; h < nHeightUV; h++)
          for (int w = nWidthBUV; w < nWidthUV; w++)
          {
            *(uint16_t *)(pDst[1] + h*nDstPitches[1] + w*pixelsize) = *(uint16_t *)(pDst[1] + h*nDstPitches[1] + (nWidthBUV - 1)*pixelsize);
            *(uint16_t *)(pDst[2] + h*nDstPitches[2] + w*pixelsize) = *(uint16_t *)(pDst[2] + h*nDstPitches[2] + (nWidthBUV - 1)*pixelsize);
          }
    }
    if (nHeightUV > nHeightBUV)
    {
      env->BitBlt(pDst[1] + nHeightBUV*nDstPitches[1], nDstPitches[1], pDst[1] + (nHeightBUV - 1)*nDstPitches[1], nDstPitches[1], nWidthUV*pixelsize, nHeightUV - nHeightBUV);
      env->BitBlt(pDst[2] + nHeightBUV*nDstPitches[2], nDstPitches[2], pDst[2] + (nHeightBUV - 1)*nDstPitches[2], nDstPitches[2], nWidthUV*pixelsize, nHeightUV - nHeightBUV);
    }


  }
  else {
    if (kind == 5)
      env->BitBlt(pDst[0], nDstPitches[0], pSrc[0], nSrcPitches[0], nWidth*pixelsize, nHeight);
    else
      MemZoneSet(pDst[0], nSceneChangeValue, nWidth*pixelsize, nHeight, 0, 0, nDstPitches[0]);

    MemZoneSet(pDst[1], nSceneChangeValue, nWidthUV*pixelsize, nHeightUV, 0, 0, nDstPitches[1]);
    MemZoneSet(pDst[2], nSceneChangeValue, nWidthUV*pixelsize, nHeightUV, 0, 0, nDstPitches[2]);
  }


  if ((pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2 && !planar)
  {
    pDstYUY2 = dst->GetWritePtr();
    nDstPitchYUY2 = dst->GetPitch();
    YUY2FromPlanes(pDstYUY2, nDstPitchYUY2, nWidth, nHeight,
      pDst[0], nDstPitches[0], pDst[1], pDst[2], nDstPitches[1], isse);
  }
  return dst;
}
