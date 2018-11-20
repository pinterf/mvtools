// MVTOOLS plugin for Avisynth
// Block motion interpolation function
// Copyright(c)2005-2016 A.G.Balakhnin aka Fizick

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
#include "commonfunctions.h"
#include "MaskFun.h"
#include "MVBlockFps.h"
#include "MVFrame.h"
#include	"MVGroupOfFrames.h"
#include "MVPlane.h"
#include "Padding.h"
#include "profile.h"
#include "SuperParams64Bits.h"
#include "Time256ProviderCst.h"

//#include <intrin.h>
#include "math.h"

MVBlockFps::MVBlockFps(
  PClip _child, PClip _super, PClip mvbw, PClip mvfw,
  unsigned int _num, unsigned int _den, int _mode, double _ml, bool _blend,
  sad_t nSCD1, int nSCD2, bool _isse2, bool _planar, bool mt_flag,
  IScriptEnvironment* env
)
  : GenericVideoFilter(_child)
  , MVFilter(mvbw, "MBlockFps", env, 1, 0)
  , mvClipB(mvbw, nSCD1, nSCD2, env, 1, 0)
  , mvClipF(mvfw, nSCD1, nSCD2, env, 1, 0)
  , super(_super)
{
  cpuFlags = _isse2 ? env->GetCPUFlags() : 0;

  if (!vi.IsYUV() && !vi.IsYUVA())
    env->ThrowError("MBlockFps: Clip must be YUV or YUY2");
  if (vi.BitsPerComponent() > 16)
    env->ThrowError("MBlockFps: only 8-16 bit clips are allowed");

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

  mode = _mode;
  if (mode < 0 || mode >8)
    env->ThrowError("MBlockFps: mode from 0 to 8");
  ml = _ml;
  planar = _planar;
  blend = _blend;

  if (mvClipB.GetDeltaFrame() <= 0 || mvClipF.GetDeltaFrame() <= 0)
    env->ThrowError("MBlockFPS: cannot use motion vectors with absolute frame references.");

  // get parameters of prepared super clip - v2.0
  SuperParams64Bits params;
  memcpy(&params, &super->GetVideoInfo().num_audio_samples, 8);
  int nHeightS = params.nHeight;
  nSuperHPad = params.nHPad;
  nSuperVPad = params.nVPad;
  int nSuperPel = params.nPel;
  nSuperModeYUV = params.nModeYUV;
  int nSuperLevels = params.nLevels;

  // todo: super_xRatioUV?
  pRefBGOF = new MVGroupOfFrames(nSuperLevels, nWidth, nHeight, nSuperPel, nSuperHPad, nSuperVPad, nSuperModeYUV, cpuFlags, xRatioUV, yRatioUV, pixelsize, bits_per_pixel, mt_flag);
  pRefFGOF = new MVGroupOfFrames(nSuperLevels, nWidth, nHeight, nSuperPel, nSuperHPad, nSuperVPad, nSuperModeYUV, cpuFlags, xRatioUV, yRatioUV, pixelsize, bits_per_pixel, mt_flag);
  int nSuperWidth = super->GetVideoInfo().width;
  int nSuperHeight = super->GetVideoInfo().height;

  if (nHeight != nHeightS
    || nHeight != vi.height
    || nWidth != nSuperWidth - nSuperHPad * 2
    || nWidth != vi.width
    || nPel != nSuperPel)
  {
    env->ThrowError("MBlockFps : wrong source or super frame size");
  }

  // in overlaps.h
  // OverlapsLsbFunction
  // OverlapsFunction
  // in M(V)DegrainX: DenoiseXFunction
  arch_t arch;
  if ((cpuFlags & CPUF_AVX2) != 0)
    arch = USE_AVX2;
  else if ((cpuFlags & CPUF_AVX) != 0)
    arch = USE_AVX;
  else if ((cpuFlags & CPUF_SSE4_1) != 0)
    arch = USE_SSE41;
  else if ((cpuFlags & CPUF_SSE2) != 0)
    arch = USE_SSE2;
  /*  else if ((pixelsize == 1) && _isse_flag) // PF no MMX support
  arch = USE_MMX;*/
  else
    arch = NO_SIMD;

  OVERSLUMA = get_overlaps_function(nBlkSizeX, nBlkSizeY, pixelsize, arch);
  OVERSCHROMA = get_overlaps_function(nBlkSizeX / xRatioUV, nBlkSizeY / yRatioUV, pixelsize, arch);
  BLITLUMA = get_copy_function(nBlkSizeX, nBlkSizeY, pixelsize, arch);
  BLITCHROMA = get_copy_function(nBlkSizeX / xRatioUV, nBlkSizeY / yRatioUV, pixelsize, arch);
  // 161115
  OVERSLUMA16 = get_overlaps_function(nBlkSizeX, nBlkSizeY, sizeof(uint16_t), arch);
  OVERSCHROMA16 = get_overlaps_function(nBlkSizeX >> nLogxRatioUV, nBlkSizeY >> nLogyRatioUV, sizeof(uint16_t), arch);

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
  // for YV12 and any type
  nWidthPUV = nWidthP / xRatioUV;
  nHeightPUV = nHeightP / yRatioUV;
  nHeightUV = nHeight / yRatioUV;
  nWidthUV = nWidth / xRatioUV;

  nPitchY = AlignNumber(nWidthP, 16);
  nPitchUV = AlignNumber(nWidthPUV, 16);

  // PF remark: masks are 8 bits
  MaskFullYB = new BYTE[nHeightP*nPitchY];
  MaskFullUVB = new BYTE[nHeightPUV*nPitchUV];
  MaskFullYF = new BYTE[nHeightP*nPitchY];
  MaskFullUVF = new BYTE[nHeightPUV*nPitchUV];

  MaskOccY = new BYTE[nHeightP*nPitchY];
  MaskOccUV = new BYTE[nHeightPUV*nPitchUV];

  smallMaskF = new BYTE[nBlkXP*nBlkYP];
  smallMaskB = new BYTE[nBlkXP*nBlkYP];
  smallMaskO = new BYTE[nBlkXP*nBlkYP];

  const int tmpBlkAlign = 16;

  nBlkPitch = AlignNumber(nBlkSizeX, tmpBlkAlign); // padded to 16 , 2.5.11.22
  TmpBlock = (BYTE *)_aligned_malloc(nBlkPitch*nBlkSizeY*pixelsize, tmpBlkAlign); // new BYTE[nBlkPitch*nBlkSizeY*pixelsize]; // may be more padding?

  int CPUF_Resize = env->GetCPUFlags();

  upsizer = new SimpleResize(nWidthP, nHeightP, nBlkXP, nBlkYP, cpuFlags);
  upsizerUV = new SimpleResize(nWidthPUV, nHeightPUV, nBlkXP, nBlkYP, cpuFlags);

  if ((pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2 && !planar)
  {
    DstPlanes = new YUY2Planes(nWidth, nHeight);
  }

  const int tmpDstAlign = 16;
  dstShortPitch = AlignNumber(nWidth, tmpDstAlign); // 2.5.11.22
  dstShortPitchUV = AlignNumber(nWidth / xRatioUV, tmpDstAlign);

  if (nOverlapX > 0 || nOverlapY > 0)
  {
    OverWins = new OverlapWindows(nBlkSizeX, nBlkSizeY, nOverlapX, nOverlapY);
    OverWinsUV = new OverlapWindows(nBlkSizeX / xRatioUV, nBlkSizeY / yRatioUV, nOverlapX / xRatioUV, nOverlapY / yRatioUV);
    // *pixelsize: allow reuse DstShort for DstInt
    // todo: rename to DstTemp and make BYTE *, cast later.
    DstShort = (unsigned short *)_aligned_malloc(dstShortPitch*nHeight * sizeof(short)*pixelsize, tmpDstAlign); // PF aligned
    DstShortU = (unsigned short *)_aligned_malloc(dstShortPitchUV*nHeight * sizeof(short)*pixelsize, tmpDstAlign);
    DstShortV = (unsigned short *)_aligned_malloc(dstShortPitchUV*nHeight * sizeof(short)*pixelsize, tmpDstAlign);
  }
}

MVBlockFps::~MVBlockFps()
{
  delete upsizer;
  delete upsizerUV;

  delete[] MaskFullYB;
  delete[] MaskFullUVB;
  delete[] MaskFullYF;
  delete[] MaskFullUVF;
  delete[] MaskOccY;
  delete[] MaskOccUV;
  delete[] smallMaskF;
  delete[] smallMaskB;
  delete[] smallMaskO;

  _aligned_free(TmpBlock); // PF 161116


  if ((pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2 && !planar)
  {
    delete DstPlanes;
  }
  delete pRefBGOF;
  delete pRefFGOF;
  if (nOverlapX > 0 || nOverlapY > 0)
  {
    delete OverWins;
    delete OverWinsUV;
    _aligned_free(DstShort); // PF 161116
    _aligned_free(DstShortU);
    _aligned_free(DstShortV);
  }
}

/*
void MVBlockFps::MakeSmallMask(BYTE *image, int imagePitch, BYTE *smallmask, int nBlkX, int nBlkY, int nBlkSizeX, int nBlkSizeY, int threshold)
{
  // it can be MMX

  // count occlusions in blocks
  BYTE *psmallmask = smallmask;

  for (int ny = 0; ny <nBlkY; ny++)
  {
    for (int nx = 0; nx <nBlkX; nx++)
    {
      psmallmask[nx] = 0;
      for (int j=0; j<nBlkSizeY; j++)
      {
        for (int i=0; i<nBlkSizeX; i++)
        {
          if(image[i]==0) // 0 is mark of occlusion
            psmallmask[nx]++; // count
        }
        image += imagePitch;
      }
      image += -imagePitch*nBlkSizeY + nBlkSizeX - nOverlapX;
    }
    image += imagePitch*(nBlkSizeY - nOverlapY) -nBlkX*(nBlkSizeX-nOverlapX);
    psmallmask += nBlkX;
  }

  // make small binary mask
  psmallmask = smallmask;

  for (int ny = 0; ny <nBlkY; ny++)
  {
    for (int nx = 0; nx <nBlkX; nx++)
    {
      if (psmallmask[nx] >= threshold)
        psmallmask[nx] = 255;
      else
        psmallmask[nx] = 0;

    }
    psmallmask += nBlkX;
  }

}

void MVBlockFps::InflateMask(BYTE *smallmask, int nBlkX, int nBlkY)
{

  // inflate mask
  BYTE *psmallmask = smallmask + nBlkX;

  for (int ny = 1; ny <nBlkY-1; ny++) // skip edges
  {
    for (int nx = 1; nx <nBlkX-1; nx++)// skip edges
    {
      if (psmallmask[nx] == 255)
      {
        psmallmask[nx-1] = 192;
        psmallmask[nx+1] = 192;
        psmallmask[nx-nBlkX-1] = 144;
        psmallmask[nx-nBlkX] = 192;
        psmallmask[nx-nBlkX+1] = 144;
        psmallmask[nx+nBlkX-1] = 144;
        psmallmask[nx+nBlkX] = 192;
        psmallmask[nx+nBlkX+1] = 144;
      }
    }
    psmallmask += nBlkX;
  }

}
*/

// masks are 8 bit
void MVBlockFps::MultMasks(BYTE *smallmaskF, BYTE *smallmaskB, BYTE *smallmaskO, int nBlkX, int nBlkY)
{
  for (int j = 0; j < nBlkY; j++)
  {
    for (int i = 0; i < nBlkX; i++) {
      smallmaskO[i] = (smallmaskF[i] * smallmaskB[i]) / 255;
    }
    smallmaskF += nBlkX;
    smallmaskB += nBlkX;
    smallmaskO += nBlkX;
  }
}

template<typename pixel_t>
inline pixel_t MEDIAN(pixel_t a, pixel_t b, pixel_t c)
{
  pixel_t			mn = std::min(a, b);
  pixel_t			mx = std::max(a, b);
  pixel_t			m = std::min(mx, c);
  m = std::max(mn, m);

  return m;
}

template<typename pixel_t>
void MVBlockFps::ResultBlock(BYTE *pDst8, int dst_pitch, const BYTE * pMCB8, int MCB_pitch, const BYTE * pMCF8, int MCF_pitch,
  const BYTE * pRef8, int ref_pitch, const BYTE * pSrc8, int src_pitch, BYTE *maskB, int mask_pitch, BYTE *maskF,
  BYTE *pOcc, int nBlkSizeX, int nBlkSizeY, int time256, int mode, int bits_per_pixel)
{
  pixel_t *pDst = reinterpret_cast<pixel_t *>(pDst8);
  const pixel_t *pMCB = reinterpret_cast<const pixel_t *>(pMCB8);
  const pixel_t *pMCF = reinterpret_cast<const pixel_t *>(pMCF8);
  const pixel_t *pRef = reinterpret_cast<const pixel_t *>(pRef8);
  const pixel_t *pSrc = reinterpret_cast<const pixel_t *>(pSrc8);
  dst_pitch /= sizeof(pixel_t);
  src_pitch /= sizeof(pixel_t);
  ref_pitch /= sizeof(pixel_t);
  MCB_pitch /= sizeof(pixel_t);
  MCF_pitch /= sizeof(pixel_t);

  if (mode == 0)
  {
    for (int h = 0; h < nBlkSizeY; h++)
    {
      for (int w = 0; w < nBlkSizeX; w++)
      {
        int mca = (pMCB[w] * time256 + pMCF[w] * (256 - time256)) >> 8; // MC fetched average
        pDst[w] = mca;
      }
      pDst += dst_pitch;
      pMCB += MCB_pitch;
      pMCF += MCF_pitch;
    }
  }
  else if (mode == 1) // default, best working mode
  {
    for (int h = 0; h < nBlkSizeY; h++)
    {
      for (int w = 0; w < nBlkSizeX; w++)
      {
        int mca = (pMCB[w] * time256 + pMCF[w] * (256 - time256)) >> 8; // MC fetched average
        int sta = MEDIAN<pixel_t>(pRef[w], pSrc[w], mca); // static median
        pDst[w] = sta;

      }
      pDst += dst_pitch;
      pMCB += MCB_pitch;
      pMCF += MCF_pitch;
      pRef += ref_pitch;
      pSrc += src_pitch;
    }
  }
  else if (mode == 2) // default, best working mode
  {
    for (int h = 0; h < nBlkSizeY; h++)
    {
      for (int w = 0; w < nBlkSizeX; w++)
      {
        int avg = (pRef[w] * time256 + pSrc[w] * (256 - time256)) >> 8; // simple temporal non-MC average
        int dyn = MEDIAN<pixel_t>(avg, pMCB[w], pMCF[w]); // dynamic median
        pDst[w] = dyn;
      }
      pDst += dst_pitch;
      pMCB += MCB_pitch;
      pMCF += MCF_pitch;
      pRef += ref_pitch;
      pSrc += src_pitch;
    }
  }
  else if (mode == 3 || mode == 6)
  {
    for (int h = 0; h < nBlkSizeY; h++)
    {
      for (int w = 0; w < nBlkSizeX; w++)
      {
        // remark: maskF/maskB are 8 bits!
        pDst[w] = (((maskB[w] * pMCF[w] + (255 - maskB[w])*pMCB[w] + 255) >> 8)*time256 +
          ((maskF[w] * pMCB[w] + (255 - maskF[w])*pMCF[w] + 255) >> 8)*(256 - time256)) >> 8;
      }
      pDst += dst_pitch;
      pMCB += MCB_pitch;
      pMCF += MCF_pitch;
//			pRef += ref_pitch;
//			pSrc += src_pitch;
      maskB += mask_pitch;
      maskF += mask_pitch;
    }
  }
  else if (mode == 4 || mode == 7)
  {
    for (int h = 0; h < nBlkSizeY; h++)
    {
      for (int w = 0; w < nBlkSizeX; w++)
      {
        // remark: maskF/maskB,pOcc are 8 bits!
        int f = (maskF[w] * pMCB[w] + (255 - maskF[w])*pMCF[w] + 255) >> 8;
        int b = (maskB[w] * pMCF[w] + (255 - maskB[w])*pMCB[w] + 255) >> 8;
        int avg = (pRef[w] * time256 + pSrc[w] * (256 - time256) + 255) >> 8; // simple temporal non-MC average
        int m = (b*time256 + f*(256 - time256)) >> 8;
        pDst[w] = (avg * pOcc[w] + m * (255 - pOcc[w]) + 255) >> 8;
      }
      pDst += dst_pitch;
      pMCB += MCB_pitch;
      pMCF += MCF_pitch;
      pRef += ref_pitch;
      pSrc += src_pitch;
      maskB += mask_pitch;
      maskF += mask_pitch;
      pOcc += mask_pitch;
    }
  }
  else if (mode == 5 || mode == 8) // debug modes show mask
  {
    for (int h = 0; h < nBlkSizeY; h++)
    {
      for (int w = 0; w < nBlkSizeX; w++)
      {
        // remark: maskF/maskB,pOcc are 8 bits!
        if (sizeof(pixel_t) == 1)
          pDst[w] = pOcc[w];
        else
          pDst[w] = (pixel_t)(pOcc[w]) << (bits_per_pixel - 8);
      }
      pDst += dst_pitch;
      pOcc += mask_pitch;
    }
  }
}


PVideoFrame __stdcall MVBlockFps::GetFrame(int n, IScriptEnvironment* env)
{
  int nWidth_B = nBlkX*(nBlkSizeX - nOverlapX) + nOverlapX;
  int nHeight_B = nBlkY*(nBlkSizeY - nOverlapY) + nOverlapY;
  int nHeightUV = nHeight / yRatioUV;
  int nWidthUV = nWidth / xRatioUV;

#ifndef _M_X64
  _mm_empty();  // paranoya
#endif
  // intermediate product may be very large! Now I know how to multiply int64
  int nleft = (int)(__int64(n)* fa / fb);
  int time256 = int((double(n)*double(fa) / double(fb) - nleft) * 256 + 0.5);

  PVideoFrame dst;
  BYTE *pDst[3];
  const BYTE *pRef[3], *pSrc[3];
  int nDstPitches[3], nRefPitches[3], nSrcPitches[3];
  unsigned char *pDstYUY2;
  int nDstPitchYUY2;

  unsigned short *pDstShort;
  unsigned short *pDstShortU;
  unsigned short *pDstShortV;

  int off = mvClipB.GetDeltaFrame(); // integer offset of reference frame
  if (off <= 0)
  {
    env->ThrowError("MBlockFps: cannot use motion vectors with absolute frame references.");
  }
  // usually off must be = 1
  if (off > 1)
    time256 = time256 / off;

  int nright = nleft + off;


  // v2.0
  if (time256 == 0) {
    dst = child->GetFrame(nleft, env); // simply left
    return dst;
  }
  else if (time256 == 256) {
    dst = child->GetFrame(nright, env); // simply right
    return dst;
  }

  PVideoFrame mvF = mvClipF.GetFrame(nright, env);
  mvClipF.Update(mvF, env);// forward from current to next
  mvF = 0;

  PVideoFrame mvB = mvClipB.GetFrame(nleft, env);
  mvClipB.Update(mvB, env);// backward from next to current
  mvB = 0;

  PVideoFrame	src = super->GetFrame(nleft, env);
  PVideoFrame ref = super->GetFrame(nright, env);//  ref for backward compensation

  dst = env->NewVideoFrame(vi);

  bool needProcessPlanes[3] = { true, !!(nSuperModeYUV & UPLANE), !!(nSuperModeYUV & VPLANE) };


  if (mvClipB.IsUsable() && mvClipF.IsUsable())
  {

    PROFILE_START(MOTION_PROFILE_YUY2CONVERT);

    if ((pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2)
    {
      pSrc[0] = src->GetReadPtr();
      pSrc[1] = pSrc[0] + src->GetRowSize() / 2;
      pSrc[2] = pSrc[1] + src->GetRowSize() / 4;
      nSrcPitches[0] = src->GetPitch();
      nSrcPitches[1] = nSrcPitches[0];
      nSrcPitches[2] = nSrcPitches[0];

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
        pDst[1] = pDst[0] + nWidth;
        pDst[2] = pDst[1] + nWidth / 2;
        nDstPitches[0] = dst->GetPitch();
        nDstPitches[1] = nDstPitches[0];
        nDstPitches[2] = nDstPitches[0];
      }
    }
    else
    {
      bool isRGB = vi.IsPlanarRGB() || vi.IsPlanarRGBA();
      const int planesRGB[] = { PLANAR_G, PLANAR_B, PLANAR_R };
      const int planesYUV[] = { PLANAR_Y, PLANAR_U, PLANAR_V };
      const int *planes = isRGB ? planesRGB : planesYUV;
      const int planeCount = vi.IsY() ? 1 : 3;
      for (int p = 0; p < planeCount; p++) { // grey support
        int plane = planes[p];
        pDst[p] = dst->GetWritePtr(plane);
        nDstPitches[p] = dst->GetPitch(plane);
        pRef[p] = ref->GetReadPtr(plane);
        nRefPitches[p] = ref->GetPitch(plane);
        pSrc[p] = src->GetReadPtr(plane);
        nSrcPitches[p] = src->GetPitch(plane);
      }
    }
    PROFILE_STOP(MOTION_PROFILE_YUY2CONVERT);

    BYTE *pDstSave[3];
    pDstSave[0] = pDst[0];
    pDstSave[1] = pDst[1];
    pDstSave[2] = pDst[2];

    pRefBGOF->Update(YUVPLANES, (BYTE*)pRef[0], nRefPitches[0], (BYTE*)pRef[1], nRefPitches[1], (BYTE*)pRef[2], nRefPitches[2]);// v2.0
    pRefFGOF->Update(YUVPLANES, (BYTE*)pSrc[0], nSrcPitches[0], (BYTE*)pSrc[1], nSrcPitches[1], (BYTE*)pSrc[2], nSrcPitches[2]);

    MVPlane *pPlanesB[3];
    MVPlane *pPlanesF[3];

    pPlanesB[0] = pRefBGOF->GetFrame(0)->GetPlane(YPLANE);
    pPlanesB[1] = pRefBGOF->GetFrame(0)->GetPlane(UPLANE);
    pPlanesB[2] = pRefBGOF->GetFrame(0)->GetPlane(VPLANE);

    pPlanesF[0] = pRefFGOF->GetFrame(0)->GetPlane(YPLANE);
    pPlanesF[1] = pRefFGOF->GetFrame(0)->GetPlane(UPLANE);
    pPlanesF[2] = pRefFGOF->GetFrame(0)->GetPlane(VPLANE);

    // Masks are 8 bit
    MemZoneSet(MaskFullYB, 0, nWidthP, nHeightP, 0, 0, nPitchY); // put zeros
    MemZoneSet(MaskFullYF, 0, nWidthP, nHeightP, 0, 0, nPitchY);

    PROFILE_START(MOTION_PROFILE_COMPENSATION);
    int blocks = mvClipB.GetBlkCount();

    // int maxoffset = nPitchY*(nHeightP-nBlkSizeY)-nBlkSizeX; not used

    if (mode >= 3 && mode <= 8) {

      PROFILE_START(MOTION_PROFILE_MASK);
      if (mode <= 5)
        MakeVectorOcclusionMaskTime(mvClipF, nBlkX, nBlkY, ml, 1.0, nPel, smallMaskF, nBlkXP, time256, nBlkSizeX - nOverlapX, nBlkSizeY - nOverlapY);
      else // 6 to 8  // PF 161115 bits_per_pixel scale through dSADNormFactor
        MakeSADMaskTime(mvClipF, nBlkX, nBlkY, 4.0 / (ml*nBlkSizeX*nBlkSizeY) / (1 << (bits_per_pixel - 8)), 1.0, nPel, smallMaskF, nBlkXP, time256, nBlkSizeX - nOverlapX, nBlkSizeY - nOverlapY);

      CheckAndPadMaskSmall(smallMaskF, nBlkXP, nBlkYP, nBlkX, nBlkY);

      PROFILE_STOP(MOTION_PROFILE_MASK);

      PROFILE_START(MOTION_PROFILE_RESIZE);
      // upsize (bilinear interpolate) vector masks to fullframe size
      upsizer->SimpleResizeDo_uint8(MaskFullYF, nWidthP, nHeightP, nPitchY, smallMaskF, nBlkXP, nBlkXP);
      upsizerUV->SimpleResizeDo_uint8(MaskFullUVF, nWidthPUV, nHeightPUV, nPitchUV, smallMaskF, nBlkXP, nBlkXP);
      // now we have forward fullframe blured occlusion mask in maskF arrays
      PROFILE_STOP(MOTION_PROFILE_RESIZE);
      PROFILE_START(MOTION_PROFILE_MASK);
      if (mode <= 5)
        MakeVectorOcclusionMaskTime(mvClipB, nBlkX, nBlkY, ml, 1.0, nPel, smallMaskB, nBlkXP, (256 - time256), nBlkSizeX - nOverlapX, nBlkSizeY - nOverlapY);
      else // 6 to 8  // PF 161115 bits_per_pixel scale through dSADNormFactor
        MakeSADMaskTime(mvClipB, nBlkX, nBlkY, 4.0 / (ml*nBlkSizeX*nBlkSizeY) / (1 << (bits_per_pixel - 8)), 1.0, nPel, smallMaskB, nBlkXP, 256 - time256, nBlkSizeX - nOverlapX, nBlkSizeY - nOverlapY);

      CheckAndPadMaskSmall(smallMaskB, nBlkXP, nBlkYP, nBlkX, nBlkY);

      PROFILE_STOP(MOTION_PROFILE_MASK);
      PROFILE_START(MOTION_PROFILE_RESIZE);
      // upsize (bilinear interpolate) vector masks to fullframe size
      upsizer->SimpleResizeDo_uint8(MaskFullYB, nWidthP, nHeightP, nPitchY, smallMaskB, nBlkXP, nBlkXP);
      upsizerUV->SimpleResizeDo_uint8(MaskFullUVB, nWidthPUV, nHeightPUV, nPitchUV, smallMaskB, nBlkXP, nBlkXP);
      PROFILE_STOP(MOTION_PROFILE_RESIZE);
    }
    if (mode == 4 || mode == 5 || mode == 7 || mode == 8)
    {
      // make final (both directions) occlusion mask
      MultMasks(smallMaskF, smallMaskB, smallMaskO, nBlkXP, nBlkYP);
      //InflateMask(smallMaskO, nBlkXP, nBlkYP);
      // upsize small mask to full frame size
      upsizer->SimpleResizeDo_uint8(MaskOccY, nWidthP, nHeightP, nPitchY, smallMaskO, nBlkXP, nBlkXP);
      upsizerUV->SimpleResizeDo_uint8(MaskOccUV, nWidthPUV, nHeightPUV, nPitchUV, smallMaskO, nBlkXP, nBlkXP);
    }

    // pointers
    BYTE * pMaskFullYB = MaskFullYB;
    BYTE * pMaskFullYF = MaskFullYF;
    BYTE * pMaskFullUVB = MaskFullUVB;
    BYTE * pMaskFullUVF = MaskFullUVF;
    BYTE * pMaskOccY = MaskOccY;
    BYTE * pMaskOccUV = MaskOccUV;

    pSrc[0] += nSuperHPad*pixelsize + nSrcPitches[0] * nSuperVPad; // add offset source in super
    pSrc[1] += (nSuperHPad >> nLogxRatioUV)*pixelsize + nSrcPitches[1] * (nSuperVPad >> nLogyRatioUV);
    pSrc[2] += (nSuperHPad >> nLogxRatioUV)*pixelsize + nSrcPitches[2] * (nSuperVPad >> nLogyRatioUV);
    pRef[0] += nSuperHPad*pixelsize + nRefPitches[0] * nSuperVPad;
    pRef[1] += (nSuperHPad >> nLogxRatioUV)*pixelsize + nRefPitches[1] * (nSuperVPad >> nLogyRatioUV);
    pRef[2] += (nSuperHPad >> nLogxRatioUV)*pixelsize + nRefPitches[2] * (nSuperVPad >> nLogyRatioUV);

    // -----------------------------------------------------------------------------
    const int nBlkSizeX_UV = nBlkSizeX >> nLogxRatioUV;
    const int nBlkSizeY_UV = nBlkSizeY >> nLogyRatioUV;
    
    if (nOverlapX == 0 && nOverlapY == 0)
    {

    // fetch image blocks
      for (int i = 0; i < blocks; i++)
      {
        const FakeBlockData &blockB = mvClipB.GetBlock(0, i);
        const FakeBlockData &blockF = mvClipF.GetBlock(0, i);

        int refxB = blockB.GetX() * nPel + ((blockB.GetMV().x*(256 - time256)) >> 8);
        int refyB = blockB.GetY() * nPel + ((blockB.GetMV().y*(256 - time256)) >> 8);
        int refxF = blockF.GetX() * nPel + ((blockF.GetMV().x*time256) >> 8);
        int refyF = blockF.GetY() * nPel + ((blockF.GetMV().y*time256) >> 8);
        for (int p = 0; p < 3; p++)
        {
          if (needProcessPlanes[p]) {
            if (pixelsize == 1) {
              ResultBlock<uint8_t>(pDst[p], nDstPitches[p],
                pPlanesB[p]->GetPointer(p == 0 ? refxB : refxB >> nLogxRatioUV, p == 0 ? refyB : refyB >> nLogyRatioUV),
                pPlanesB[p]->GetPitch(),
                pPlanesF[p]->GetPointer(p == 0 ? refxF : refxF >> nLogxRatioUV, p == 0 ? refyF : refyF >> nLogyRatioUV),
                pPlanesF[p]->GetPitch(),
                pRef[p], nRefPitches[p],
                pSrc[p], nSrcPitches[p],
                p == 0 ? pMaskFullYB : pMaskFullUVB,
                p == 0 ? nPitchY : nPitchUV,
                p == 0 ? pMaskFullYF : pMaskFullUVF,
                p == 0 ? pMaskOccY : pMaskOccUV,
                p == 0 ? nBlkSizeX : nBlkSizeX >> nLogxRatioUV,
                p == 0 ? nBlkSizeY : nBlkSizeY >> nLogyRatioUV,
                time256
                , mode, bits_per_pixel);
            }
            else if (pixelsize == 2)
            {
              ResultBlock<uint16_t>(pDst[p], nDstPitches[p],
                pPlanesB[p]->GetPointer(p == 0 ? refxB : refxB >> nLogxRatioUV, p == 0 ? refyB : refyB >> nLogyRatioUV),
                pPlanesB[p]->GetPitch(),
                pPlanesF[p]->GetPointer(p == 0 ? refxF : refxF >> nLogxRatioUV, p == 0 ? refyF : refyF >> nLogyRatioUV),
                pPlanesF[p]->GetPitch(),
                pRef[p], nRefPitches[p],
                pSrc[p], nSrcPitches[p],
                p == 0 ? pMaskFullYB : pMaskFullUVB,
                p == 0 ? nPitchY : nPitchUV,
                p == 0 ? pMaskFullYF : pMaskFullUVF,
                p == 0 ? pMaskOccY : pMaskOccUV,
                p == 0 ? nBlkSizeX : nBlkSizeX_UV,
                p == 0 ? nBlkSizeY : nBlkSizeY_UV,
                time256
                , mode, bits_per_pixel);
            }
          }
        }
        // update pDsts
        pDst[0] += nBlkSizeX*pixelsize;
        pDst[1] += nBlkSizeX_UV * pixelsize;
        pDst[2] += nBlkSizeX_UV * pixelsize;
        pRef[0] += nBlkSizeX*pixelsize;
        pRef[1] += nBlkSizeX_UV * pixelsize;
        pRef[2] += nBlkSizeX_UV * pixelsize;
        pSrc[0] += nBlkSizeX *pixelsize;
        pSrc[1] += nBlkSizeX_UV * pixelsize;
        pSrc[2] += nBlkSizeX_UV * pixelsize;
        pMaskFullYB += nBlkSizeX;
        pMaskFullUVB += nBlkSizeX_UV;
        pMaskFullYF += nBlkSizeX;
        pMaskFullUVF += nBlkSizeX_UV;
        pMaskOccY += nBlkSizeX;
        pMaskOccUV += nBlkSizeX_UV;


        if (!((i + 1) % nBlkX))
        {
          for (int p = 0; p < 3; p++)
          {
            if (needProcessPlanes[p]) {
              // blend rest right with time weight
              if (pixelsize == 1)
                Blend<uint8_t>(pDst[p], pSrc[p], pRef[p],
                  p == 0 ? nBlkSizeY : nBlkSizeY_UV,
                  p == 0 ? nWidth - nBlkSizeX*nBlkX : nWidthUV - nBlkSizeX_UV*nBlkX,
                  nDstPitches[p], nSrcPitches[p], nRefPitches[p], time256 /*t256_prov_cst*/, cpuFlags);
              else if (pixelsize == 2)
                Blend<uint16_t>(pDst[p], pSrc[p], pRef[p],
                  p == 0 ? nBlkSizeY : nBlkSizeY_UV,
                  p == 0 ? nWidth - nBlkSizeX*nBlkX : nWidthUV - nBlkSizeX_UV*nBlkX,
                  nDstPitches[p], nSrcPitches[p], nRefPitches[p], time256 /*t256_prov_cst*/, cpuFlags);
            }
          }

          pDst[0] += nBlkSizeY    * nDstPitches[0] - nBlkSizeX   *nBlkX*pixelsize;
          pDst[1] += nBlkSizeY_UV * nDstPitches[1] - nBlkSizeX_UV*nBlkX*pixelsize;
          pDst[2] += nBlkSizeY_UV * nDstPitches[2] - nBlkSizeX_UV*nBlkX*pixelsize;
          pRef[0] += nBlkSizeY    * nRefPitches[0] - nBlkSizeX   *nBlkX*pixelsize;
          pRef[1] += nBlkSizeY_UV * nRefPitches[1] - nBlkSizeX_UV*nBlkX*pixelsize;
          pRef[2] += nBlkSizeY_UV * nRefPitches[2] - nBlkSizeX_UV*nBlkX*pixelsize;
          pSrc[0] += nBlkSizeY    * nSrcPitches[0] - nBlkSizeX   *nBlkX*pixelsize;
          pSrc[1] += nBlkSizeY_UV * nSrcPitches[1] - nBlkSizeX_UV*nBlkX*pixelsize;
          pSrc[2] += nBlkSizeY_UV * nSrcPitches[2] - nBlkSizeX_UV*nBlkX*pixelsize;
          pMaskFullYB  += nBlkSizeY    * nPitchY  - nBlkSizeX    *nBlkX;
          pMaskFullUVB += nBlkSizeY_UV * nPitchUV - nBlkSizeX_UV *nBlkX;
          pMaskFullYF  += nBlkSizeY    * nPitchY  - nBlkSizeX    *nBlkX;
          pMaskFullUVF += nBlkSizeY_UV * nPitchUV - nBlkSizeX_UV *nBlkX;
          pMaskOccY    += nBlkSizeY    * nPitchY  - nBlkSizeX    *nBlkX;
          pMaskOccUV   += nBlkSizeY_UV * nPitchUV - nBlkSizeX_UV *nBlkX;
        }
      }
      // blend rest bottom with time weight
      for (int p = 0; p < 3; p++)
      {
        if (needProcessPlanes[p]) {
          if (pixelsize == 1)
            Blend<uint8_t>(pDst[p], pSrc[p], pRef[p],
              p == 0 ? nHeight - nBlkSizeY*nBlkY : nHeightUV - nBlkSizeY_UV*nBlkY,
              p == 0 ? nWidth : nWidthUV,
              nDstPitches[p], nSrcPitches[p], nRefPitches[p], time256, cpuFlags);
          else if (pixelsize == 2)
            Blend<uint16_t>(pDst[p], pSrc[p], pRef[p],
              p == 0 ? nHeight - nBlkSizeY*nBlkY : nHeightUV - nBlkSizeY_UV*nBlkY,
              p == 0 ? nWidth : nWidthUV,
              nDstPitches[p], nSrcPitches[p], nRefPitches[p], time256, cpuFlags);
        }
      }
    } // overlapx,y == 0
    else // overlap
    {
      const int nWidth_B_UV = nWidth_B >> nLogxRatioUV;
      const int nHeight_B_UV = nHeight_B >> nLogyRatioUV;

      for (int p = 0; p < 3; p++)
      {
        if (needProcessPlanes[p]) {
          // blend rest right with time weight
          // P.F. 161115 2.5.1.22 bug: pDst[p] + nDstPitches[p]*()
          // was:  0 (for U and V)
          // need: nWidth_B / xRatioUV (for U and V) (2.5.11.22 bug)
          if (pixelsize == 1)
            Blend<uint8_t>(pDst[p] + pixelsize*(p == 0 ? nWidth_B : nWidth_B_UV),
              pSrc[p] + pixelsize*(p == 0 ? nWidth_B : nWidth_B_UV),   // -"-
              pRef[p] + pixelsize*(p == 0 ? nWidth_B : nWidth_B_UV),   // -"-
              p == 0 ? nHeight_B : nHeight_B_UV, // P.F. nHeight_B is enough, bottom will take care of buttom right edge
              p == 0 ? nWidth - nWidth_B : nWidthUV - nWidth_B_UV,
              nDstPitches[p], nSrcPitches[p], nRefPitches[p], time256, cpuFlags);
          else if (pixelsize == 2)
            Blend<uint16_t>(pDst[p] + pixelsize*(p == 0 ? nWidth_B : nWidth_B_UV),   // PF 161115 nWidth_B / xRatioUV instead of 0 for UV (2.5.11.22 bug)
              pSrc[p] + pixelsize*(p == 0 ? nWidth_B : nWidth_B_UV),   // -"-
              pRef[p] + pixelsize*(p == 0 ? nWidth_B : nWidth_B_UV),   // -"-
              p == 0 ? nHeight_B : nHeight_B_UV,
              p == 0 ? nWidth - nWidth_B : nWidthUV - nWidth_B_UV,
              nDstPitches[p], nSrcPitches[p], nRefPitches[p], time256, cpuFlags);
        }
      }
      // blend rest right with time weight

      for (int p = 0; p < 3; p++)
      {
        if (needProcessPlanes[p]) {
          // blend rest bottom with time weight (full width, right fill was full height minus bottom corner)
          // P.F. 161116 2.5.1.22 bug: pDst[p] + nDstPitches[p]*()
          // was: (nHeight - nHeight_B), e.g. 720-716 = 4th row not O.K.
          // need: nHeight_B (that is really the bottom) = 716th row O.K.
          if (pixelsize == 1)
            Blend<uint8_t>(pDst[p] + nDstPitches[p] * (p == 0 ? (/*nHeight -*/ nHeight_B) : (/*nHeight -*/ nHeight_B) / yRatioUV),
              pSrc[p] + nSrcPitches[p] * (p == 0 ? nHeight_B : nHeight_B_UV),
              pRef[p] + nRefPitches[p] * (p == 0 ? nHeight_B : nHeight_B_UV),
              p == 0 ? nHeight - nHeight_B : nHeightUV - nHeight_B_UV,
              p == 0 ? nWidth : nWidthUV,
              nDstPitches[p], nSrcPitches[p], nRefPitches[p], time256, cpuFlags);
          else if (pixelsize == 2)
            Blend<uint16_t>(pDst[p] + nDstPitches[p] * (p == 0 ? nHeight_B : nHeight_B_UV),
              pSrc[p] + nSrcPitches[p] * (p == 0 ? nHeight_B : nHeight_B_UV),
              pRef[p] + nRefPitches[p] * (p == 0 ? nHeight_B : nHeight_B_UV),
              p == 0 ? nHeight - nHeight_B : nHeightUV - nHeight_B_UV,
              p == 0 ? nWidth : nWidthUV,
              nDstPitches[p], nSrcPitches[p], nRefPitches[p], time256, cpuFlags);
        }
      }
      
      MemZoneSet(reinterpret_cast<unsigned char*>(DstShort), 0, nWidth_B * sizeof(short)*pixelsize, nHeight_B, 0, 0, dstShortPitch * sizeof(short)*pixelsize);
      if (nSuperModeYUV & UPLANE) MemZoneSet(reinterpret_cast<unsigned char*>(DstShortU), 0, nWidth_B_UV * sizeof(short) *pixelsize, nHeight_B_UV, 0, 0, dstShortPitchUV * sizeof(short)*pixelsize);
      if (nSuperModeYUV & VPLANE) MemZoneSet(reinterpret_cast<unsigned char*>(DstShortV), 0, nWidth_B_UV * sizeof(short) *pixelsize, nHeight_B_UV, 0, 0, dstShortPitchUV * sizeof(short)*pixelsize);
      
      pDstShort = DstShort;   // PF: can be used for Int array instead of short for 8+ bits
      pDstShortU = DstShortU;
      pDstShortV = DstShortV;

      for (int by = 0; by < nBlkY; by++)
      {
        // indexing overlap windows weighting table: top=0 middle=3 bottom=6
        /*
        0 = Top Left    1 = Top Middle    2 = Top Right
        3 = Middle Left 4 = Middle Middle 5 = Middle Right
        6 = Bottom Left 7 = Bottom Middle 8 = Bottom Right
        */

        int wby = (by == 0) ? 0 * 3 : (by == nBlkY - 1) ? 2 * 3 : 1 * 3; // 0 for very first, 2*3 for very last, 1*3 for all others in the middle
        int xx = 0; // logical offset. Mul by 2 for pixelsize_super==2. Don't mul for indexing int* array
        int xxUV = 0;
        for (int bx = 0; bx < nBlkX; ++bx)
        {
          // select window
          // indexing overlap windows weighting table: left=+0 middle=+1 rightmost=+2
          int wbx = (bx == 0) ? 0 : (bx == nBlkX - 1) ? 2 : 1; // 0 for very first, 2 for very last, 1 for all others in the middle
          winOver = OverWins->GetWindow(wby + wbx);
          winOverUV = OverWinsUV->GetWindow(wby + wbx);

          int i = by*nBlkX + bx;

          const FakeBlockData &blockB = mvClipB.GetBlock(0, i);
          const FakeBlockData &blockF = mvClipF.GetBlock(0, i);

          int refxB = blockB.GetX() * nPel + ((blockB.GetMV().x*(256 - time256)) >> 8);
          int refyB = blockB.GetY() * nPel + ((blockB.GetMV().y*(256 - time256)) >> 8);
          int refxF = blockF.GetX() * nPel + ((blockF.GetMV().x*time256) >> 8);
          int refyF = blockF.GetY() * nPel + ((blockF.GetMV().y*time256) >> 8);

          // firstly calculate result block and write it to temporary place, not to dst
          // luma
          for (int p = 0; p < 3; p++)
          {
            if (needProcessPlanes[p]) {
              int current_xx;
              if (p == 0) current_xx = xx; else current_xx = xxUV;
              if (pixelsize == 1) {
                ResultBlock<uint8_t>(TmpBlock, nBlkPitch*pixelsize,
                  pPlanesB[p]->GetPointer(p == 0 ? refxB : refxB >> nLogxRatioUV, p == 0 ? refyB : refyB >> nLogyRatioUV),
                  pPlanesB[p]->GetPitch(),
                  pPlanesF[p]->GetPointer(p == 0 ? refxF : refxF >> nLogxRatioUV, p == 0 ? refyF : refyF >> nLogyRatioUV),
                  pPlanesF[p]->GetPitch(),
                  pRef[p] + current_xx*pixelsize, nRefPitches[p],
                  pSrc[p] + current_xx*pixelsize, nSrcPitches[p],
                  (p == 0 ? pMaskFullYB : pMaskFullUVB) + current_xx,
                  (p == 0 ? nPitchY : nPitchUV),
                  (p == 0 ? pMaskFullYF : pMaskFullUVF) + current_xx,
                  (p == 0 ? pMaskOccY : pMaskOccUV) + current_xx,
                  (p == 0 ? nBlkSizeX : nBlkSizeX_UV),
                  (p == 0 ? nBlkSizeY : nBlkSizeY_UV),
                  time256, mode, bits_per_pixel);
                // now write result block to short dst with overlap window weight
                switch (p) {
                case 0: OVERSLUMA(pDstShort + current_xx, dstShortPitch, TmpBlock, nBlkPitch * pixelsize, winOver, nBlkSizeX); break;
                case 1: OVERSCHROMA(pDstShortU + current_xx, dstShortPitchUV, TmpBlock, nBlkPitch * pixelsize, winOverUV, nBlkSizeX_UV); break;
                case 2: OVERSCHROMA(pDstShortV + current_xx, dstShortPitchUV, TmpBlock, nBlkPitch * pixelsize, winOverUV, nBlkSizeX_UV); break;
                }
              }
              else if (pixelsize == 2) {
                ResultBlock<uint16_t>(TmpBlock, nBlkPitch*pixelsize,
                  pPlanesB[p]->GetPointer(p == 0 ? refxB : refxB >> nLogxRatioUV, p == 0 ? refyB : refyB >> nLogyRatioUV),
                  pPlanesB[p]->GetPitch(),
                  pPlanesF[p]->GetPointer(p == 0 ? refxF : refxF >> nLogxRatioUV, p == 0 ? refyF : refyF >> nLogyRatioUV),
                  pPlanesF[p]->GetPitch(),
                  pRef[p] + current_xx*pixelsize, nRefPitches[p],
                  pSrc[p] + current_xx*pixelsize, nSrcPitches[p],
                  (p == 0 ? pMaskFullYB : pMaskFullUVB) + current_xx,
                  (p == 0 ? nPitchY : nPitchUV),
                  (p == 0 ? pMaskFullYF : pMaskFullUVF) + current_xx,
                  (p == 0 ? pMaskOccY : pMaskOccUV) + current_xx,
                  (p == 0 ? nBlkSizeX : nBlkSizeX_UV),
                  (p == 0 ? nBlkSizeY : nBlkSizeY_UV),
                  time256, mode, bits_per_pixel);
                // now write result block to short dst with overlap window weight
                switch (p) { // pitch of TmpBlock is byte-level only, regardless of 8/16 bit 
                case 0: OVERSLUMA16((unsigned short *)((int *)(pDstShort)+current_xx), dstShortPitch, TmpBlock, nBlkPitch * pixelsize, winOver, nBlkSizeX); break;
                case 1: OVERSCHROMA16((unsigned short *)((int *)(pDstShortU)+current_xx), dstShortPitchUV, TmpBlock, nBlkPitch * pixelsize, winOverUV, nBlkSizeX / xRatioUV); break;
                case 2: OVERSCHROMA16((unsigned short *)((int *)(pDstShortV)+current_xx), dstShortPitchUV, TmpBlock, nBlkPitch * pixelsize, winOverUV, nBlkSizeX / xRatioUV); break;
                }
              } // pixelsize
            } // needprocess
          } // planes

          xx += (nBlkSizeX - nOverlapX);
          xxUV += (nBlkSizeX - nOverlapX) >> nLogxRatioUV; // * pixelsize is at the usage place

        } // for bx
        // update pDsts
        const int nBlkSizeYOverlapDiff = (nBlkSizeY - nOverlapY);
        const int nBlkSizeYOverlapDiff_UV = (nBlkSizeY - nOverlapY) >> nLogyRatioUV;

        pDstShort += dstShortPitch*nBlkSizeYOverlapDiff*pixelsize; // pointer is short, 10-16bit: really int *, so mul 2x
        pDstShortU += dstShortPitchUV*nBlkSizeYOverlapDiff_UV*pixelsize;
        pDstShortV += dstShortPitchUV*(nBlkSizeY - nOverlapY) / yRatioUV*pixelsize;
        pDst[0] += nDstPitches[0] * nBlkSizeYOverlapDiff;
        pDst[1] += nDstPitches[1] * nBlkSizeYOverlapDiff_UV;
        pDst[2] += nDstPitches[2] * nBlkSizeYOverlapDiff_UV;
        pRef[0] += nRefPitches[0] * nBlkSizeYOverlapDiff;
        pRef[1] += nRefPitches[1] * nBlkSizeYOverlapDiff_UV;
        pRef[2] += nRefPitches[2] * nBlkSizeYOverlapDiff_UV;
        pSrc[0] += nSrcPitches[0] * nBlkSizeYOverlapDiff;
        pSrc[1] += nSrcPitches[1] * nBlkSizeYOverlapDiff_UV;
        pSrc[2] += nSrcPitches[2] * nBlkSizeYOverlapDiff_UV;
        pMaskFullYB += nPitchY*nBlkSizeYOverlapDiff;
        pMaskFullUVB += nPitchUV*nBlkSizeYOverlapDiff_UV;
        pMaskFullYF += nPitchY*nBlkSizeYOverlapDiff;
        pMaskFullUVF += nPitchUV*nBlkSizeYOverlapDiff_UV;
        pMaskOccY += nPitchY*nBlkSizeYOverlapDiff;
        pMaskOccUV += nPitchUV*nBlkSizeYOverlapDiff_UV;
      }  // for by
        // post 2.5.1.22 bug: the copy from internal 16 bit array to destination was missing for overlaps!
      if (pixelsize == 1) {
        // nWidth_B and nHeight_B, right and bottom was blended
        Short2Bytes(pDstSave[0], nDstPitches[0], DstShort, dstShortPitch, nWidth_B, nHeight_B);
        Short2Bytes(pDstSave[1], nDstPitches[1], DstShortU, dstShortPitchUV, nWidth_B_UV, nHeight_B_UV);
        Short2Bytes(pDstSave[2], nDstPitches[2], DstShortV, dstShortPitchUV, nWidth_B_UV, nHeight_B_UV);
      }
      else if (pixelsize == 2)
      {
        Short2Bytes_Int32toWord16((uint16_t *)(pDstSave[0]), nDstPitches[0], (int *)DstShort, dstShortPitch, nWidth_B, nHeight_B, bits_per_pixel);
        Short2Bytes_Int32toWord16((uint16_t *)(pDstSave[1]), nDstPitches[1], (int *)DstShortU, dstShortPitchUV, nWidth_B_UV, nHeight_B_UV, bits_per_pixel);
        Short2Bytes_Int32toWord16((uint16_t *)(pDstSave[2]), nDstPitches[2], (int *)DstShortV, dstShortPitchUV, nWidth_B_UV, nHeight_B_UV, bits_per_pixel);
      }
    }
    PROFILE_STOP(MOTION_PROFILE_COMPENSATION);

    PROFILE_START(MOTION_PROFILE_YUY2CONVERT);
    if ((pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2 && !planar)
    {
      YUY2FromPlanes(pDstYUY2, nDstPitchYUY2, nWidth, nHeight,
        pDstSave[0], nDstPitches[0], pDstSave[1], pDstSave[2], nDstPitches[1], cpuFlags);
    }
    PROFILE_STOP(MOTION_PROFILE_YUY2CONVERT);

    return dst;
  }

  else // bad
  {
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

        for (int p = 0; p < 3; p++)
        {
          if (needProcessPlanes[p]) {
            if (pixelsize == 1) {
              Blend<uint8_t>(pDst[p], pSrc[p], pRef[p],
                p == 0 ? nHeight : nHeightUV,
                p == 0 ? nWidth : nWidthUV,
                nDstPitches[p], nSrcPitches[p], nRefPitches[p], time256, cpuFlags);
            }
            else if (pixelsize == 2) {
              Blend<uint16_t>(pDst[p], pSrc[p], pRef[p],
                p == 0 ? nHeight : nHeightUV,
                p == 0 ? nWidth : nWidthUV,
                nDstPitches[p], nSrcPitches[p], nRefPitches[p], time256, cpuFlags);
            }
          }
        }
      }
      PROFILE_STOP(MOTION_PROFILE_FLOWINTER);

      return dst;
    }
    else
    {
      return src; // like ChangeFPS
    }
  }

}
