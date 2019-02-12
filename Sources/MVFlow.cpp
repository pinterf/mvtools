// Pixels flow motion function
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

#include	"ClipFnc.h"
#include "CopyCode.h"
#include "MaskFun.h"
#include "MVFinest.h"
#include "MVFlow.h"
#include "SuperParams64Bits.h"
#include "commonfunctions.h"

MVFlow::MVFlow(PClip _child, PClip super, PClip _mvec, int _time256, int _mode, bool _fields,
  sad_t nSCD1, int nSCD2, bool _isse, bool _planar, PClip _timeclip, IScriptEnvironment* env) :
  GenericVideoFilter(_child),
  MVFilter(_mvec, "MFlow", env, 1, 0),
  mvClip(_mvec, nSCD1, nSCD2, env, 1, 0)
{
  time256 = _time256;
  mode = _mode;

  cpuFlags = _isse ? env->GetCPUFlags() : 0;
  fields = _fields;
  planar = _planar;

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
    env->ThrowError("MFlow: input and super clip bit depth is different");

  if (nHeight != nHeightS
    || nWidth != nSuperWidth - nSuperHPad * 2
    || nPel != nSuperPel)
  {
    env->ThrowError("MFlow : wrong super frame clip");
  }

  if (nPel == 1)
    finest = super; // v2.0.9.1
  else
    finest = new MVFinest(super, _isse, env);

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
  //needDistinctChroma = !is444 && !isGrey && !isRGB;

  // 128 align: something about pentium cache line
  VXFullY = (short*)_aligned_malloc(2 * nHeightP*VPitchY + 128, 128);
  VYFullY = (short*)_aligned_malloc(2 * nHeightP*VPitchY + 128, 128);
  if (!isGrey) {
    VYFullUV = (short*)_aligned_malloc(2 * nHeightPUV*VPitchUV + 128, 128);
    VXFullUV = (short*)_aligned_malloc(2 * nHeightPUV*VPitchUV + 128, 128);
  }

  VXSmallY = (short*)_aligned_malloc(2 * nBlkXP*nBlkYP + 128, 128);
  VYSmallY = (short*)_aligned_malloc(2 * nBlkXP*nBlkYP + 128, 128);
  if (!isGrey) {
    VXSmallUV = (short*)_aligned_malloc(2 * nBlkXP*nBlkYP + 128, 128);
    VYSmallUV = (short*)_aligned_malloc(2 * nBlkXP*nBlkYP + 128, 128);
  }

  upsizer = new SimpleResize(nWidthP, nHeightP, nBlkXP, nBlkYP, cpuFlags);
  if(!isGrey)
    upsizerUV = new SimpleResize(nWidthPUV, nHeightPUV, nBlkXP, nBlkYP, cpuFlags);

  if ((pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2 && !planar)
  {
    DstPlanes = new YUY2Planes(nWidth, nHeight);
  }
}

MVFlow::~MVFlow()
{
  if ((pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2 && !planar)
  {
    delete DstPlanes;
  }

  delete upsizer;
  if(!isGrey)
    delete upsizerUV;

  _aligned_free(VXFullY);
  _aligned_free(VYFullY);
  if (!isGrey) {
    _aligned_free(VXFullUV);
    _aligned_free(VYFullUV);
  }
  _aligned_free(VXSmallY);
  _aligned_free(VYSmallY);
  if (!isGrey) {
    _aligned_free(VXSmallUV);
    _aligned_free(VYSmallUV);
  }
}

template<typename pixel_t>
void MVFlow::Shift(BYTE * pdst, int dst_pitch, const BYTE *pref, int ref_pitch, short *VXFull, int VXPitch, short *VYFull, int VYPitch, int width, int height, int time256)
{
  // shift mode
  if (nPel == 1)
  {
    Shift_NPel <pixel_t, 0>(pdst, dst_pitch, pref, ref_pitch, VXFull, VXPitch, VYFull, VYPitch, width, height, time256);
  }
  else if (nPel == 2)
  {
    Shift_NPel <pixel_t, 1>(pdst, dst_pitch, pref, ref_pitch, VXFull, VXPitch, VYFull, VYPitch, width, height, time256);
  }
  else if (nPel == 4)
  {
    Shift_NPel <pixel_t, 2>(pdst, dst_pitch, pref, ref_pitch, VXFull, VXPitch, VYFull, VYPitch, width, height, time256);
  }
}

template <typename pixel_t, int NPELL2>
void MVFlow::Shift_NPel(BYTE * pdst8, int dst_pitch, const BYTE *pref8, int ref_pitch, short *VXFull, int VXPitch, short *VYFull, int VYPitch, int width, int height, int time256)
{
  dst_pitch /= sizeof(pixel_t);
  ref_pitch /= sizeof(pixel_t);
  pixel_t *pdst = reinterpret_cast<pixel_t *>(pdst8);
  const pixel_t *pref = reinterpret_cast<const pixel_t *>(pref8);

  for (int h = 0; h < height; h++)
  {
    for (int w = 0; w < width; w++)
    {
      // very simple half-pixel using,  must be by image interpolation really (later)
      int vx = (-VXFull[w] * time256 + (128 << NPELL2)) / (256 << NPELL2);
      int vy = (-VYFull[w] * time256 + (128 << NPELL2)) / (256 << NPELL2);
      int href = h + vy;
      int wref = w + vx;
      if (href >= 0 && href < height && wref >= 0 && wref < width)// bound check if not padded
          // Fixme: PF 20190212 ??? Value of source pixel multiplied by NPELL???
        if constexpr(sizeof(pixel_t) == 4)
          pdst[vy*dst_pitch + vx + w] = pref[w] * (1 << NPELL2);
        else
          pdst[vy*dst_pitch + vx + w] = pref[w] << NPELL2; // 2.5.11.22 
    }
    pref += ref_pitch << NPELL2; // 2.5.11.22
    pdst += dst_pitch;
    VXFull += VXPitch;
    VYFull += VYPitch;
  }
}



template<typename pixel_t>
void MVFlow::Fetch(BYTE * pdst, int dst_pitch, const BYTE *pref, int ref_pitch, short *VXFull, int VXPitch, short *VYFull, int VYPitch, int width, int height, int time256)
{
  if (nPel == 1)
  {
    Fetch_NPel <pixel_t,/*T256P,*/ 0>(pdst, dst_pitch, pref, ref_pitch, VXFull, VXPitch, VYFull, VYPitch, width, height, time256);
  }
  else if (nPel == 2)
  {
    Fetch_NPel <pixel_t,/*T256P,*/ 1>(pdst, dst_pitch, pref, ref_pitch, VXFull, VXPitch, VYFull, VYPitch, width, height, time256);
  }
  else if (nPel == 4)
  {
    Fetch_NPel <pixel_t,/*T256P, */2>(pdst, dst_pitch, pref, ref_pitch, VXFull, VXPitch, VYFull, VYPitch, width, height, time256);
  }
}

template <typename pixel_t, int NPELL2>
void MVFlow::Fetch_NPel(BYTE * pdst8, int dst_pitch, const BYTE *pref8, int ref_pitch, short *VXFull, int VXPitch, short *VYFull, int VYPitch, int width, int height, int time256)
{
  dst_pitch /= sizeof(pixel_t);
  ref_pitch /= sizeof(pixel_t);
  pixel_t *pdst = reinterpret_cast<pixel_t *>(pdst8);
  const pixel_t *pref = reinterpret_cast<const pixel_t *>(pref8);

  for (int h = 0; h < height; h++)
  {
    for (int w = 0; w < width; w++)
    {
/*   2.5.11.22:
     pel==1
      int vx = ((VXFull[w])*time256 + 128) >> 8;
      int vy = ((VYFull[w])*time256 + 128) >> 8;
      pdst[w] = pref[vy*ref_pitch + vx + w];
     pel==2 use interpolated image
      int vx = ((VXFull[w])*time256 + 128)>>8;
      int vy = ((VYFull[w])*time256 + 128)>>8;
      pdst[w] = pref[vy*ref_pitch + vx + (w<<1)];
     pel==4
      int vx = ((VXFull[w])*time256 + 128)>>8;
      int vy = ((VYFull[w])*time256 + 128)>>8;
      pdst[w] = pref[vy*ref_pitch + vx + (w<<2)];
      */
      int vx = ((VXFull[w])*time256 + 128) >> 8; // 2.5.11.22
      int vy = ((VYFull[w])*time256 + 128) >> 8; // 2.5.11.22 

      pdst[w] = pref[vy*ref_pitch + vx + (w << NPELL2)];
    }
    pref += (ref_pitch) << NPELL2;
    pdst += dst_pitch;
    VXFull += VXPitch;
    VYFull += VYPitch;
  }
}



//-------------------------------------------------------------------------
PVideoFrame __stdcall MVFlow::GetFrame(int n, IScriptEnvironment* env)
{
  PVideoFrame dst, ref;
  BYTE *pDst[3];
  const BYTE *pRef[3];
  int nDstPitches[3], nRefPitches[3];
  unsigned char *pDstYUY2;
  int nDstPitchYUY2;

  int nref;

  int off = mvClip.GetDeltaFrame(); // integer offset of reference frame
  if (off < 0)
  {
    nref = -off;// special static mode
  }
  else
    if (mvClip.IsBackward())
    {
      nref = n + off;
    }
    else
    {
      nref = n - off;
    }

  PVideoFrame mvn = mvClip.GetFrame(n, env);
  mvClip.Update(mvn, env);// backward from next to current
  
  bool usable_flag = mvClip.IsUsable(); // moved after mvclip update in 2.7.31 like fixed in 2.5.11.22

  mvn = 0;

  if (usable_flag)
  {
    ref = finest->GetFrame(nref, env);//  ref for  compensation
    dst = env->NewVideoFrame(vi);

    if ((pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2)
    {
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
        // planar data packed to interleaved format (same as interleved2planar by kassandro) - v2.0.0.5
        pDst[0] = dst->GetWritePtr();
        pDst[1] = pDst[0] + dst->GetRowSize() / 2;
        pDst[2] = pDst[1] + dst->GetRowSize() / 4;
        nDstPitches[0] = dst->GetPitch();
        nDstPitches[1] = nDstPitches[0];
        nDstPitches[2] = nDstPitches[0];
      }

      pRef[0] = ref->GetReadPtr();
      pRef[1] = pRef[0] + ref->GetRowSize() / 2;
      pRef[2] = pRef[1] + ref->GetRowSize() / 4;
      nRefPitches[0] = ref->GetPitch();
      nRefPitches[1] = nRefPitches[0];
      nRefPitches[2] = nRefPitches[0];
    }
    else
    {
      int planes_y[4] = { PLANAR_Y, PLANAR_U, PLANAR_V, PLANAR_A };
      int planes_r[4] = { PLANAR_G, PLANAR_B, PLANAR_R, PLANAR_A };
      int *planes = (vi.IsYUV() || vi.IsYUVA()) ? planes_y : planes_r;
      planecount = std::min(vi.NumComponents(), 3);
      for (int p = 0; p < planecount; ++p) {
        const int plane = planes[p];

        pRef[p] = ref->GetReadPtr(plane);
        nRefPitches[p] = ref->GetPitch(plane);

        pDst[p] = dst->GetWritePtr(plane);
        nDstPitches[p] = dst->GetPitch(plane);
      }
    }


    int nOffsetY = nRefPitches[0] * nVPadding*nPel + nHPadding*nPel*pixelsize_super;
    int nOffsetUV = nRefPitches[1] * nVPaddingUV*nPel + nHPaddingUV*nPel*pixelsize_super;

    MakeVectorSmallMasks(mvClip, nBlkX, nBlkY, VXSmallY, nBlkXP, VYSmallY, nBlkXP);

    CheckAndPadSmallY(VXSmallY, VYSmallY, nBlkXP, nBlkYP, nBlkX, nBlkY);

    const int fieldShift =
      ClipFnc::compute_fieldshift(finest, fields, nPel, n, nref);

    for (int j = 0; j < nBlkYP; j++)
    {
      for (int i = 0; i < nBlkXP; i++)
      {
        VYSmallY[j*nBlkXP + i] += fieldShift;
      }
    }

    if (!isGrey) {
      VectorSmallMaskYToHalfUV(VXSmallY, nBlkXP, nBlkYP, VXSmallUV, xRatioUVs[1]);
      VectorSmallMaskYToHalfUV(VYSmallY, nBlkXP, nBlkYP, VYSmallUV, yRatioUVs[1]);
    }

    // upsize (bilinear interpolate) vector masks to fullframe size

    const int nPel = 1;
    upsizer->SimpleResizeDo_int16(VXFullY, nWidthP, nHeightP, VPitchY, VXSmallY, nBlkXP, nBlkXP, nPel, true, nWidth, nHeight);
    upsizer->SimpleResizeDo_int16(VYFullY, nWidthP, nHeightP, VPitchY, VYSmallY, nBlkXP, nBlkXP, nPel, false, nWidth, nHeight);
    if (!isGrey) {
      upsizerUV->SimpleResizeDo_int16(VXFullUV, nWidthPUV, nHeightPUV, VPitchUV, VXSmallUV, nBlkXP, nBlkXP, nPel, true, nWidthUV, nHeightUV);
      upsizerUV->SimpleResizeDo_int16(VYFullUV, nWidthPUV, nHeightPUV, VPitchUV, VYSmallUV, nBlkXP, nBlkXP, nPel, false, nWidthUV, nHeightUV);
    }

    if (mode == 1)
    {
      MemZoneSet(pDst[0], 0, nWidth*pixelsize_super, nHeight, 0, 0, nDstPitches[0]);
      if (!isGrey) {
        MemZoneSet(pDst[1], 0, nWidthUV*pixelsize_super, nHeightUV, 0, 0, nDstPitches[1]);
        MemZoneSet(pDst[2], 0, nWidthUV*pixelsize_super, nHeightUV, 0, 0, nDstPitches[2]);
      }
    }

    if (mode == 0) // fetch mode
    {
      if (pixelsize_super == 1) {
        Fetch<uint8_t>(pDst[0], nDstPitches[0], pRef[0] + nOffsetY, nRefPitches[0], VXFullY, VPitchY, VYFullY, VPitchY, nWidth, nHeight, time256); //padded
        if (!isGrey) {
          Fetch<uint8_t>(pDst[1], nDstPitches[1], pRef[1] + nOffsetUV, nRefPitches[1], VXFullUV, VPitchUV, VYFullUV, VPitchUV, nWidthUV, nHeightUV, time256);
          Fetch<uint8_t>(pDst[2], nDstPitches[2], pRef[2] + nOffsetUV, nRefPitches[2], VXFullUV, VPitchUV, VYFullUV, VPitchUV, nWidthUV, nHeightUV, time256);
        }
      }
      else if(pixelsize_super == 2) {
        Fetch<uint16_t>(pDst[0], nDstPitches[0], pRef[0] + nOffsetY, nRefPitches[0], VXFullY, VPitchY, VYFullY, VPitchY, nWidth, nHeight, time256); //padded
        if (!isGrey) {
          Fetch<uint16_t>(pDst[1], nDstPitches[1], pRef[1] + nOffsetUV, nRefPitches[1], VXFullUV, VPitchUV, VYFullUV, VPitchUV, nWidthUV, nHeightUV, time256);
          Fetch<uint16_t>(pDst[2], nDstPitches[2], pRef[2] + nOffsetUV, nRefPitches[2], VXFullUV, VPitchUV, VYFullUV, VPitchUV, nWidthUV, nHeightUV, time256);
        }
      }
      else if (pixelsize_super == 4) {
        Fetch<float>(pDst[0], nDstPitches[0], pRef[0] + nOffsetY, nRefPitches[0], VXFullY, VPitchY, VYFullY, VPitchY, nWidth, nHeight, time256); //padded
        if (!isGrey) {
          Fetch<float>(pDst[1], nDstPitches[1], pRef[1] + nOffsetUV, nRefPitches[1], VXFullUV, VPitchUV, VYFullUV, VPitchUV, nWidthUV, nHeightUV, time256);
          Fetch<float>(pDst[2], nDstPitches[2], pRef[2] + nOffsetUV, nRefPitches[2], VXFullUV, VPitchUV, VYFullUV, VPitchUV, nWidthUV, nHeightUV, time256);
        }
      }
    }
    else if (mode == 1) // shift mode
    {
      if (pixelsize_super == 1) {
        MemZoneSet(pDst[0], 255, nWidth, nHeight, 0, 0, nDstPitches[0]);
        if (!isGrey) {
          MemZoneSet(pDst[1], 255, nWidthUV, nHeightUV, 0, 0, nDstPitches[1]);
          MemZoneSet(pDst[2], 255, nWidthUV, nHeightUV, 0, 0, nDstPitches[2]);
        }
        Shift<uint8_t>(pDst[0], nDstPitches[0], pRef[0] + nOffsetY, nRefPitches[0], VXFullY, VPitchY, VYFullY, VPitchY, nWidth, nHeight, time256);
        if (!isGrey) {
          Shift<uint8_t>(pDst[1], nDstPitches[1], pRef[1] + nOffsetUV, nRefPitches[1], VXFullUV, VPitchUV, VYFullUV, VPitchUV, nWidthUV, nHeightUV, time256);
          Shift<uint8_t>(pDst[2], nDstPitches[2], pRef[2] + nOffsetUV, nRefPitches[2], VXFullUV, VPitchUV, VYFullUV, VPitchUV, nWidthUV, nHeightUV, time256);
        }
      }
      else if(pixelsize_super == 2) {
        int max_pixel_value = (1 << bits_per_pixel_super) - 1;
        fill_plane<uint16_t>(pDst[0], nHeight, nDstPitches[0], max_pixel_value);
        if (!isGrey) {
          fill_plane<uint16_t>(pDst[1], nHeightUV, nDstPitches[1], max_pixel_value);
          fill_plane<uint16_t>(pDst[2], nHeightUV, nDstPitches[2], max_pixel_value);
        }
        Shift<uint16_t>(pDst[0], nDstPitches[0], pRef[0] + nOffsetY, nRefPitches[0], VXFullY, VPitchY, VYFullY, VPitchY, nWidth, nHeight, time256);
        if (!isGrey) {
          Shift<uint16_t>(pDst[1], nDstPitches[1], pRef[1] + nOffsetUV, nRefPitches[1], VXFullUV, VPitchUV, VYFullUV, VPitchUV, nWidthUV, nHeightUV, time256);
          Shift<uint16_t>(pDst[2], nDstPitches[2], pRef[2] + nOffsetUV, nRefPitches[2], VXFullUV, VPitchUV, VYFullUV, VPitchUV, nWidthUV, nHeightUV, time256);
        }
      }
      else if (pixelsize_super == 4) {
        float max_pixel_value = 1.0f; // base values where none of the vectors point
        float max_pixel_value_chroma = 0.5f;
        fill_plane<float>(pDst[0], nHeight, nDstPitches[0], max_pixel_value);
        if (!isGrey) {
          fill_plane<float>(pDst[1], nHeightUV, nDstPitches[1], max_pixel_value_chroma);
          fill_plane<float>(pDst[2], nHeightUV, nDstPitches[2], max_pixel_value_chroma);
        }
        Shift<float>(pDst[0], nDstPitches[0], pRef[0] + nOffsetY, nRefPitches[0], VXFullY, VPitchY, VYFullY, VPitchY, nWidth, nHeight, time256);
        if (!isGrey) {
          Shift<float>(pDst[1], nDstPitches[1], pRef[1] + nOffsetUV, nRefPitches[1], VXFullUV, VPitchUV, VYFullUV, VPitchUV, nWidthUV, nHeightUV, time256);
          Shift<float>(pDst[2], nDstPitches[2], pRef[2] + nOffsetUV, nRefPitches[2], VXFullUV, VPitchUV, VYFullUV, VPitchUV, nWidthUV, nHeightUV, time256);
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
    PVideoFrame	src = child->GetFrame(n, env);

    return src;
  }
}
