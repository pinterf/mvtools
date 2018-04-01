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
  // P.F. commented out 2.5.11.22 ,timeclip (_timeclip)
{/*
  if (_timeclip != 0)
  {
    const ::VideoInfo &	vi_tst = timeclip->GetVideoInfo ();
    const bool		same_format_flag = (   vi_tst.height     == vi.height
                                  && vi_tst.width      == vi.width
                                  && vi_tst.num_frames == vi.num_frames
                                  && vi_tst.IsSameColorspace (vi));
    if (! same_format_flag)
    {
      env->ThrowError("MFlow: tclip format is different from the main clip.");
    }
  }
  */
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
//	AVSValue cache_args[1] = { finest };
//	finest = env->Invoke("InternalCache", AVSValue(cache_args,1)).AsClip(); // add cache for speed

//	if (nWidth  != vi.width  || (nWidth  + nHPadding*2)*nPel != finest->GetVideoInfo().width ||
//	    nHeight != vi.height || (nHeight + nVPadding*2)*nPel != finest->GetVideoInfo().height )
//		env->ThrowError("MVFlow: wrong source of finest frame size");

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
  // for YV12
  nWidthPUV = nWidthP / xRatioUV;
  nHeightPUV = nHeightP / yRatioUV;

  nHeightUV = nHeight / yRatioUV;
  nWidthUV = nWidth / xRatioUV;

  nHPaddingUV = nHPadding / xRatioUV;
  nVPaddingUV = nVPadding / yRatioUV;


  VPitchY = AlignNumber(nWidthP, 16);
  VPitchUV = AlignNumber(nWidthPUV, 16);
//	char debugbuf[128];
//	wsprintf(debugbuf,"MVFlow: nBlkX=%d, nOverlap=%d, nBlkXP=%d, nWidth=%d, nWidthP=%d, VPitchY=%d",nBlkX, nOverlap, nBlkXP, nWidth, nWidthP, VPitchY);
//	OutputDebugString(debugbuf);

  // 128 align: something about pentium cache line
  VXFullY = (short*)_aligned_malloc(2 * nHeightP*VPitchY + 128, 128);
  VXFullUV = (short*)_aligned_malloc(2 * nHeightPUV*VPitchUV + 128, 128);

  VYFullY = (short*)_aligned_malloc(2 * nHeightP*VPitchY + 128, 128);
  VYFullUV = (short*)_aligned_malloc(2 * nHeightPUV*VPitchUV + 128, 128);

  VXSmallY = (short*)_aligned_malloc(2 * nBlkXP*nBlkYP + 128, 128);
  VYSmallY = (short*)_aligned_malloc(2 * nBlkXP*nBlkYP + 128, 128);
  VXSmallUV = (short*)_aligned_malloc(2 * nBlkXP*nBlkYP + 128, 128);
  VYSmallUV = (short*)_aligned_malloc(2 * nBlkXP*nBlkYP + 128, 128);

  upsizer = new SimpleResize(nWidthP, nHeightP, nBlkXP, nBlkYP, cpuFlags);
  upsizerUV = new SimpleResize(nWidthPUV, nHeightPUV, nBlkXP, nBlkYP, cpuFlags);

  /* 2.5.11.22 no LUT
  if (timeclip == 0)
  {
    LUTV = new VectLut [1];
    Create_LUTV(time256, LUTV [0]);
  }
  else
  {
    LUTV = new VectLut [256];
    for (int t256 = 0; t256 < 256; ++t256)
    {
      Create_LUTV(t256, LUTV [t256]);
    }
  }
  */

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
  delete upsizerUV;

  _aligned_free(VXFullY);
  _aligned_free(VXFullUV);
  _aligned_free(VYFullY);
  _aligned_free(VYFullUV);
  _aligned_free(VXSmallY);
  _aligned_free(VYSmallY);
  _aligned_free(VXSmallUV);
  _aligned_free(VYSmallUV);

}

/* not any more 2.5.11.22
void MVFlow::Create_LUTV(int t256, VectLut plut)
{
  for (int v=0; v<VECT_AMP; v++)
    plut [v] = ((v-128)*t256)/256;
}
*/

// 2.5.11.22 
//void MVFlow::Shift(BYTE * pdst, int dst_pitch, const BYTE *pref, int ref_pitch, short *VXFull, int VXPitch, short *VYFull, int VYPitch, int width, int height, int time256)
// 
// old:
// int vx = -((VXFull[w] - 128)*time256) / 256;
// int vy = -((VYFull[w] - 128)*time256) / 256;
// new:
// int vx = (-VXFull[w] * time256 + 128) >> 8;
// int vy = (-VYFull[w] * time256 + 128) >> 8;


// 2.6.0.5 nice templates
// but does not fit into 2.5.11.22 logic
//template <class T256P>
//void MVFlow::Shift(BYTE * pdst, int dst_pitch, const BYTE *pref, int ref_pitch,  short *VXFull, int VXPitch,  short *VYFull, int VYPitch, int width, int height, T256P &t256_provider)
template<typename pixel_t>
void MVFlow::Shift(BYTE * pdst, int dst_pitch, const BYTE *pref, int ref_pitch, short *VXFull, int VXPitch, short *VYFull, int VYPitch, int width, int height, int time256)
{
  // shift mode
  if (nPel == 1)
  {
    Shift_NPel <pixel_t,/*T256P,*/0>(pdst, dst_pitch, pref, ref_pitch, VXFull, VXPitch, VYFull, VYPitch, width, height, time256/*t256_provider*/);
  }
  else if (nPel == 2)
  {
    Shift_NPel <pixel_t,/*T256P,*/ 1>(pdst, dst_pitch, pref, ref_pitch, VXFull, VXPitch, VYFull, VYPitch, width, height, time256/*t256_provider*/);
  }
  else if (nPel == 4)
  {
    Shift_NPel <pixel_t,/*T256P,*/ 2>(pdst, dst_pitch, pref, ref_pitch, VXFull, VXPitch, VYFull, VYPitch, width, height, time256/*t256_provider*/);
  }
}

template <typename pixel_t, int NPELL2>
void MVFlow::Shift_NPel(BYTE * pdst8, int dst_pitch, const BYTE *pref8, int ref_pitch, short *VXFull, int VXPitch, short *VYFull, int VYPitch, int width, int height, int time256)
//template <class T256P, int NPELL2>
//void MVFlow::Shift_NPel(BYTE * pdst, int dst_pitch, const BYTE *pref, int ref_pitch,  short *VXFull, int VXPitch,  short *VYFull, int VYPitch, int width, int height, T256P &t256_provider)
{
  dst_pitch /= sizeof(pixel_t);
  ref_pitch /= sizeof(pixel_t);
  pixel_t *pdst = reinterpret_cast<pixel_t *>(pdst8);
  const pixel_t *pref = reinterpret_cast<const pixel_t *>(pref8);

  for (int h = 0; h < height; h++)
  {
    for (int w = 0; w < width; w++)
    {
      // time provider 2.6.0.5 const int		time256 = t256_provider.get_t (w);

      // very simple half-pixel using,  must be by image interpolation really (later)
      // pre 2.5.11.22 int vx = -((VXFull[w]-128)*time256) / (256 << NPELL2);
      // pre 2.5.11.22 int vy = -((VYFull[w]-128)*time256) / (256 << NPELL2);
      int vx = (-VXFull[w] * time256 + (128 << NPELL2)) / (256 << NPELL2);
      int vy = (-VYFull[w] * time256 + (128 << NPELL2)) / (256 << NPELL2);
      int href = h + vy;
      int wref = w + vx;
      if (href >= 0 && href < height && wref >= 0 && wref < width)// bound check if not padded
        pdst[vy*dst_pitch + vx + w] = pref[w] << NPELL2; // 2.5.11.22 
    }
    pref += ref_pitch << NPELL2; // 2.5.11.22
    pdst += dst_pitch;
    // time provider 2.6.0.5 t256_provider.jump_to_next_row ();
    VXFull += VXPitch;
    VYFull += VYPitch;
  }
}



template<typename pixel_t>
void MVFlow::Fetch(BYTE * pdst, int dst_pitch, const BYTE *pref, int ref_pitch, short *VXFull, int VXPitch, short *VYFull, int VYPitch, int width, int height, int time256)
/*template <class T256P>
void MVFlow::Fetch(BYTE * pdst, int dst_pitch, const BYTE *pref, int ref_pitch,  short *VXFull, int VXPitch,  short *VYFull, int VYPitch, int width, int height, T256P &t256_provider)
*/ {
  if (nPel == 1)
  {
    Fetch_NPel <pixel_t,/*T256P,*/ 0>(pdst, dst_pitch, pref, ref_pitch, VXFull, VXPitch, VYFull, VYPitch, width, height, time256 /*t256_provider*/);
  }
  else if (nPel == 2)
  {
    Fetch_NPel <pixel_t,/*T256P,*/ 1>(pdst, dst_pitch, pref, ref_pitch, VXFull, VXPitch, VYFull, VYPitch, width, height, time256 /*t256_provider*/);
  }
  else if (nPel == 4)
  {
    Fetch_NPel <pixel_t,/*T256P, */2>(pdst, dst_pitch, pref, ref_pitch, VXFull, VXPitch, VYFull, VYPitch, width, height, time256 /*t256_provider*/);
  }
}

//template <class T256P, int NPELL2>
//void MVFlow::Fetch_NPel(BYTE * pdst, int dst_pitch, const BYTE *pref, int ref_pitch,  short *VXFull, int VXPitch,  short *VYFull, int VYPitch, int width, int height, T256P &t256_provider)
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
      // ex 2.6.0.5 const int		time256 = t256_provider.get_t (w);

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
      /* 2.6.0.5
      int vx = t256_provider.get_vect_f (time256, VXFull[w]);
      int vy = t256_provider.get_vect_f (time256, VYFull[w]);
      */
      int vx = ((VXFull[w])*time256 + 128) >> 8; // 2.5.11.22
      int vy = ((VYFull[w])*time256 + 128) >> 8; // 2.5.11.22 

      pdst[w] = pref[vy*ref_pitch + vx + (w << NPELL2)];
    }
    pref += (ref_pitch) << NPELL2;
    pdst += dst_pitch;
    // do nothing t256_provider.jump_to_next_row ();
    VXFull += VXPitch;
    VYFull += VYPitch;
  }
}



//-------------------------------------------------------------------------
PVideoFrame __stdcall MVFlow::GetFrame(int n, IScriptEnvironment* env)
{
  PVideoFrame dst, ref, t256;
  BYTE *pDst[3];
  const BYTE *pRef[3];
  int nDstPitches[3], nRefPitches[3];
  //const BYTE *pt256[3];
  //int nt256Pitches[3];
  unsigned char *pDstYUY2;
  int nDstPitchYUY2;

  bool				usable_flag = mvClip.IsUsable(); // in 2 5.11.22 later
  int				nref;


  // 2.6.0.5 mvClip.use_ref_frame(nref, usable_flag, finest, n, env);
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
  mvn = 0;

  if (usable_flag)
  {
    ref = finest->GetFrame(nref, env);//  ref for  compensation
    dst = env->NewVideoFrame(vi);
    /* comment out. 2.6.0.5 specific? 2.5.11.22
    if (timeclip != 0)
    {
      t256 = timeclip->GetFrame(n, env);
    }
    */

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
      /* comment oout 2.6.0.5 specific timeclip
      if (timeclip != 0)
      {
        pt256[0] = t256->GetReadPtr();
        pt256[1] = pt256[0] + t256->GetRowSize() / 2;
        pt256[2] = pt256[1] + t256->GetRowSize() / 4;
        nt256Pitches[0] = t256->GetPitch();
        nt256Pitches[1] = nt256Pitches[0];
        nt256Pitches[2] = nt256Pitches[0];
      }
      */
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

      /* comment oout 2.6.0.5 specific timeclip
      if (timeclip != 0)
      {
        pt256[0] = YRPLAN(t256);
        pt256[1] = URPLAN(t256);
        pt256[2] = VRPLAN(t256);
        nt256Pitches[0] = YPITCH(t256);
        nt256Pitches[1] = UPITCH(t256);
        nt256Pitches[2] = VPITCH(t256);
      }
      */
    }


    int nOffsetY = nRefPitches[0] * nVPadding*nPel + nHPadding*nPel*pixelsize;
    int nOffsetUV = nRefPitches[1] * nVPaddingUV*nPel + nHPaddingUV*nPel*pixelsize;

    MakeVectorSmallMasks(mvClip, nBlkX, nBlkY, VXSmallY, nBlkXP, VYSmallY, nBlkXP);
    if (nBlkXP > nBlkX) // fill right
    {
      for (int j = 0; j < nBlkY; j++)
      {
        VXSmallY[j*nBlkXP + nBlkX] = std::min(VXSmallY[j*nBlkXP + nBlkX - 1], short(0)); // not positive
        VYSmallY[j*nBlkXP + nBlkX] = VYSmallY[j*nBlkXP + nBlkX - 1];
      }
    }
    if (nBlkYP > nBlkY) // fill bottom
    {
      for (int i = 0; i < nBlkXP; i++)
      {
        VXSmallY[nBlkXP*nBlkY + i] = VXSmallY[nBlkXP*(nBlkY - 1) + i];
        VYSmallY[nBlkXP*nBlkY + i] = std::min(VYSmallY[nBlkXP*(nBlkY - 1) + i], short(0)); // not positive
      }
    }

    const int		fieldShift =
      ClipFnc::compute_fieldshift(finest, fields, nPel, n, nref);

    for (int j = 0; j < nBlkYP; j++)
    {
      for (int i = 0; i < nBlkXP; i++)
      {
        VYSmallY[j*nBlkXP + i] += fieldShift;
      }
    }

    VectorSmallMaskYToHalfUV(VXSmallY, nBlkXP, nBlkYP, VXSmallUV, xRatioUV);
    VectorSmallMaskYToHalfUV(VYSmallY, nBlkXP, nBlkYP, VYSmallUV, yRatioUV);

    // upsize (bilinear interpolate) vector masks to fullframe size

    int dummyplane = PLANAR_Y; // use luma plane resizer code for all planes if we resize from luma small mask
    upsizer->SimpleResizeDo_uint16(VXFullY, nWidthP, nHeightP, VPitchY, VXSmallY, nBlkXP, nBlkXP);
    upsizer->SimpleResizeDo_uint16(VYFullY, nWidthP, nHeightP, VPitchY, VYSmallY, nBlkXP, nBlkXP);
    upsizerUV->SimpleResizeDo_uint16(VXFullUV, nWidthPUV, nHeightPUV, VPitchUV, VXSmallUV, nBlkXP, nBlkXP);
    upsizerUV->SimpleResizeDo_uint16(VYFullUV, nWidthPUV, nHeightPUV, VPitchUV, VYSmallUV, nBlkXP, nBlkXP);

    if (mode == 1)
    {
      MemZoneSet(pDst[0], 0, nWidth*pixelsize, nHeight, 0, 0, nDstPitches[0]);
      MemZoneSet(pDst[1], 0, nWidthUV*pixelsize, nHeightUV, 0, 0, nDstPitches[1]);
      MemZoneSet(pDst[2], 0, nWidthUV*pixelsize, nHeightUV, 0, 0, nDstPitches[2]);
    }

        // Time as a script parameter
    /* 2.5.11.22 no timeclip
    if (timeclip == 0)
    {
      const Time256ProviderCst	t256_prov_cst(time256, LUTV[0], LUTV[0]);
      if (mode == 0) // fetch mode
      {
        Fetch(pDst[0], nDstPitches[0], pRef[0] + nOffsetY, nRefPitches[0], VXFullY, VPitchY, VYFullY, VPitchY, nWidth, nHeight, t256_prov_cst); //padded
        Fetch(pDst[1], nDstPitches[1], pRef[1] + nOffsetUV, nRefPitches[1], VXFullUV, VPitchUV, VYFullUV, VPitchUV, nWidthUV, nHeightUV, t256_prov_cst);
        Fetch(pDst[2], nDstPitches[2], pRef[2] + nOffsetUV, nRefPitches[2], VXFullUV, VPitchUV, VYFullUV, VPitchUV, nWidthUV, nHeightUV, t256_prov_cst);
      }
      else if (mode == 1) // shift mode
      {
        Shift(pDst[0], nDstPitches[0], pRef[0] + nOffsetY, nRefPitches[0], VXFullY, VPitchY, VYFullY, VPitchY, nWidth, nHeight, t256_prov_cst);
        Shift(pDst[1], nDstPitches[1], pRef[1] + nOffsetUV, nRefPitches[1], VXFullUV, VPitchUV, VYFullUV, VPitchUV, nWidthUV, nHeightUV, t256_prov_cst);
        Shift(pDst[2], nDstPitches[2], pRef[2] + nOffsetUV, nRefPitches[2], VXFullUV, VPitchUV, VYFullUV, VPitchUV, nWidthUV, nHeightUV, t256_prov_cst);
      }
    }

    // Time as a clip
    else
    */
    {
      if (mode == 0) // fetch mode
      {
        if (pixelsize == 1) {
          Fetch<uint8_t>(pDst[0], nDstPitches[0], pRef[0] + nOffsetY, nRefPitches[0], VXFullY, VPitchY, VYFullY, VPitchY, nWidth, nHeight, time256); //padded
          Fetch<uint8_t>(pDst[1], nDstPitches[1], pRef[1] + nOffsetUV, nRefPitches[1], VXFullUV, VPitchUV, VYFullUV, VPitchUV, nWidthUV, nHeightUV, time256);
          Fetch<uint8_t>(pDst[2], nDstPitches[2], pRef[2] + nOffsetUV, nRefPitches[2], VXFullUV, VPitchUV, VYFullUV, VPitchUV, nWidthUV, nHeightUV, time256);
        }
        else {
          Fetch<uint16_t>(pDst[0], nDstPitches[0], pRef[0] + nOffsetY, nRefPitches[0], VXFullY, VPitchY, VYFullY, VPitchY, nWidth, nHeight, time256); //padded
          Fetch<uint16_t>(pDst[1], nDstPitches[1], pRef[1] + nOffsetUV, nRefPitches[1], VXFullUV, VPitchUV, VYFullUV, VPitchUV, nWidthUV, nHeightUV, time256);
          Fetch<uint16_t>(pDst[2], nDstPitches[2], pRef[2] + nOffsetUV, nRefPitches[2], VXFullUV, VPitchUV, VYFullUV, VPitchUV, nWidthUV, nHeightUV, time256);
        }
        /*
        Fetch(pDst[0], nDstPitches[0], pRef[0] + nOffsetY, nRefPitches[0],
          VXFullY, VPitchY, VYFullY, VPitchY, nWidth, nHeight,
          Time256ProviderPlane(pt256[0], nt256Pitches[0], LUTV, LUTV)); //padded
        Fetch(pDst[1], nDstPitches[1], pRef[1] + nOffsetUV, nRefPitches[1],
          VXFullUV, VPitchUV, VYFullUV, VPitchUV, nWidthUV, nHeightUV,
          Time256ProviderPlane(pt256[1], nt256Pitches[1], LUTV, LUTV));
        Fetch(pDst[2], nDstPitches[2], pRef[2] + nOffsetUV, nRefPitches[2],
          VXFullUV, VPitchUV, VYFullUV, VPitchUV, nWidthUV, nHeightUV,
          Time256ProviderPlane(pt256[2], nt256Pitches[2], LUTV, LUTV));
          */
      }
      else if (mode == 1) // shift mode
      {
        if (pixelsize == 1) {
          MemZoneSet(pDst[0], 255, nWidth, nHeight, 0, 0, nDstPitches[0]);
          MemZoneSet(pDst[1], 255, nWidthUV, nHeightUV, 0, 0, nDstPitches[1]);
          MemZoneSet(pDst[2], 255, nWidthUV, nHeightUV, 0, 0, nDstPitches[2]);
          Shift<uint8_t>(pDst[0], nDstPitches[0], pRef[0] + nOffsetY, nRefPitches[0], VXFullY, VPitchY, VYFullY, VPitchY, nWidth, nHeight, time256);
          Shift<uint8_t>(pDst[1], nDstPitches[1], pRef[1] + nOffsetUV, nRefPitches[1], VXFullUV, VPitchUV, VYFullUV, VPitchUV, nWidthUV, nHeightUV, time256);
          Shift<uint8_t>(pDst[2], nDstPitches[2], pRef[2] + nOffsetUV, nRefPitches[2], VXFullUV, VPitchUV, VYFullUV, VPitchUV, nWidthUV, nHeightUV, time256);
        }
        else { // pixelsize == 2
          int max_pixel_value = (1 << bits_per_pixel) - 1;
          fill_plane<uint16_t>(pDst[0], nHeight, nDstPitches[0], max_pixel_value);
          fill_plane<uint16_t>(pDst[1], nHeight, nDstPitches[0], max_pixel_value);
          fill_plane<uint16_t>(pDst[2], nHeight, nDstPitches[0], max_pixel_value);
          Shift<uint16_t>(pDst[0], nDstPitches[0], pRef[0] + nOffsetY, nRefPitches[0], VXFullY, VPitchY, VYFullY, VPitchY, nWidth, nHeight, time256);
          Shift<uint16_t>(pDst[1], nDstPitches[1], pRef[1] + nOffsetUV, nRefPitches[1], VXFullUV, VPitchUV, VYFullUV, VPitchUV, nWidthUV, nHeightUV, time256);
          Shift<uint16_t>(pDst[2], nDstPitches[2], pRef[2] + nOffsetUV, nRefPitches[2], VXFullUV, VPitchUV, VYFullUV, VPitchUV, nWidthUV, nHeightUV, time256);
        }
        /*
        Shift(pDst[0], nDstPitches[0], pRef[0] + nOffsetY, nRefPitches[0],
          VXFullY, VPitchY, VYFullY, VPitchY, nWidth, nHeight,
          Time256ProviderPlane(pt256[0], nt256Pitches[0], LUTV, LUTV));
        Shift(pDst[1], nDstPitches[1], pRef[1] + nOffsetUV, nRefPitches[1],
          VXFullUV, VPitchUV, VYFullUV, VPitchUV, nWidthUV, nHeightUV,
          Time256ProviderPlane(pt256[1], nt256Pitches[1], LUTV, LUTV));
        Shift(pDst[2], nDstPitches[2], pRef[2] + nOffsetUV, nRefPitches[2],
          VXFullUV, VPitchUV, VYFullUV, VPitchUV, nWidthUV, nHeightUV,
          Time256ProviderPlane(pt256[2], nt256Pitches[2], LUTV, LUTV));
          */
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
