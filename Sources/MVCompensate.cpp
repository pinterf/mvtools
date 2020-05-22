// Make a motion compensate temporal denoiser
// Author: Manao
// Copyright(c)2006 A.G.Balakhnin aka Fizick (YUY2, overlap, edges processing)
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

#include	"ClipFnc.h"
#include "commonfunctions.h"
#include "MaskFun.h"
#include "MVCompensate.h"
#include "MVFrame.h"
#include	"MVGroupOfFrames.h"
#include "MVPlane.h"
#include "profile.h"
#include "SuperParams64Bits.h"
#include "Time256ProviderCst.h"

#include	<mmintrin.h>



MVCompensate::MVCompensate(
  PClip _child, PClip _super, PClip vectors, bool sc, double _recursionPercent,
  sad_t _thsad, bool _fields, double _time100, sad_t _nSCD1, int _nSCD2, bool _isse2, bool _planar,
  bool mt_flag, int trad, bool center_flag, PClip cclip_sptr, sad_t _thsad2,
  IScriptEnvironment* env_ptr
)
  : GenericVideoFilter(_child)
  , _mv_clip_arr(1)
  , MVFilter(vectors, "MCompensate", env_ptr, 1, 0)
  , super(_super)
  , _trad(trad)
  , _cclip_sptr((cclip_sptr != 0) ? cclip_sptr : _child)
  , _multi_flag(trad > 0)
  , _center_flag(center_flag)
  , _mt_flag(mt_flag)
  //,	nLogxRatioUV (( xRatioUV == 2) ? 1 : 0) MVFilter has nLogxRatioUV
  //,	nLogyRatioUV ((yRatioUV == 2) ? 1 : 0)
  , _boundary_cnt_arr()
{
  has_at_least_v8 = true;
  try { env_ptr->CheckVersion(8); }
  catch (const AvisynthError&) { has_at_least_v8 = false; }

  cpuFlags = _isse2 ? env_ptr->GetCPUFlags() : 0;

  // actual format can differ from the base clip of vectors
  pixelsize_super = super->GetVideoInfo().ComponentSize();
  bits_per_pixel_super = super->GetVideoInfo().BitsPerComponent();
  pixelsize_super_shift = ilog2(pixelsize_super);
  ovrBufferElementSize = pixelsize_super == 1 ? sizeof(short) : sizeof(int); // for 10-16 bits and float: 32 bit int or float tmp buffer
  ovrBufferElementSize_shift = ilog2(ovrBufferElementSize);

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

  if (trad < 0)
  {
    env_ptr->ThrowError(
      "MCompensate: temporal radius must greater or equal to 0."
    );
  }

  if (_trad <= 0)
  {
    _mv_clip_arr[0]._clip_sptr =
      MVClipSPtr(new MVClip(vectors, _nSCD1, _nSCD2, env_ptr, 1, 0)); // MVClip limit to valid thSCD ranges
  }
  else
  {
    _mv_clip_arr.resize(_trad * 2);
    for (int k = 0; k < _trad * 2; ++k)
    {
      _mv_clip_arr[k]._clip_sptr = MVClipSPtr(
        new MVClip(vectors, _nSCD1, _nSCD2, env_ptr, _trad * 2, k)
      );
      CheckSimilarity(*(_mv_clip_arr[k]._clip_sptr), "vectors", env_ptr);
    }

    int tbsize = _trad * 2;
    if (_center_flag)
    {
      ++tbsize;
    }

    vi.num_frames *= tbsize;
    vi.fps_numerator *= tbsize;
  }

  scBehavior = sc;
  recursion = std::max(0, std::min(256, int(_recursionPercent / 100 * 256))); // convert to int scaled 0 to 256
  fields = _fields;
  planar = _planar;

  if (_time100 < 0 || _time100 > 100)
    env_ptr->ThrowError("MCompensate: time must be 0.0 to 100.0");
  time256 = int(256 * _time100 / 100);

  if (recursion > 0 && nPel > 1)
  {
    env_ptr->ThrowError("MCompensate: recursion is not supported for Pel>1");
  }

  if (fields && nPel < 2 && !vi.IsFieldBased())
  {
    env_ptr->ThrowError("MCompensate: fields option is for fieldbased video and pel > 1");
  }

  // limit parameters to valid ranges to avoid overflow 2.7.37-
  int nSCD1 = std::min(_nSCD1, 8 * 8 * (255 - 0)); // max for 8 bits, normalized to 8x8 blocksize, avoid overflow later
  int thsad = std::min(_thsad, 8 * 8 * (255 - 0));
  int thsad2 = std::min(_thsad2, 8 * 8 * (255 - 0));

  // Normalize to block SAD
  for (int k = 0; k < int(_mv_clip_arr.size()); ++k)
  {
    MvClipInfo & c_info = _mv_clip_arr[k];
    const sad_t thscd1 = c_info._clip_sptr->GetThSCD1();
    const sad_t thsadn = (uint64_t)thsad * thscd1 / nSCD1; // PF 18.11.23 thSAD 100000 thSCD1 65280 nSCD1 100000
    const sad_t thsadn2 = (uint64_t)thsad2 * thscd1 / nSCD1;
    const int d = k / 2 + 1;
    c_info._thsad = ClipFnc::interpolate_thsad(thsadn, thsadn2, d, _trad); // P.F. when testing, d=0, _trad=1, will assert inside.
  }

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


  OVERSLUMA = get_overlaps_function(nBlkSizeX, nBlkSizeY, sizeof(uint8_t), arch);
  OVERSCHROMA = get_overlaps_function(nBlkSizeX >> nLogxRatioUVs[1], nBlkSizeY >> nLogyRatioUVs[1], sizeof(uint8_t), arch);

  BLITLUMA = get_copy_function(nBlkSizeX, nBlkSizeY, pixelsize_super, arch);
  BLITCHROMA = get_copy_function(nBlkSizeX >> nLogxRatioUVs[1], nBlkSizeY >> nLogyRatioUVs[1], pixelsize_super, arch);

  OVERSLUMA16 = get_overlaps_function(nBlkSizeX, nBlkSizeY, sizeof(uint16_t), arch);
  OVERSCHROMA16 = get_overlaps_function(nBlkSizeX >> nLogxRatioUVs[1], nBlkSizeY >> nLogyRatioUVs[1], sizeof(uint16_t), arch);

  OVERSLUMA32 = get_overlaps_function(nBlkSizeX, nBlkSizeY, sizeof(float), arch);
  OVERSCHROMA32 = get_overlaps_function(nBlkSizeX >> nLogxRatioUVs[1], nBlkSizeY >> nLogyRatioUVs[1], sizeof(float), arch);


  // get parameters of prepared super clip - v2.0
  SuperParams64Bits params;
  memcpy(&params, &super->GetVideoInfo().num_audio_samples, 8);
  int nHeightS = params.nHeight;
  nSuperHPad = params.nHPad;
  nSuperVPad = params.nVPad;
  int nSuperPel = params.nPel;
  int nSuperModeYUV = params.nModeYUV;
  int nSuperLevels = params.nLevels;

  if (!vi.IsSameColorspace(_super->GetVideoInfo()))
    env_ptr->ThrowError("MCompensate : source and super clip video format is different!");

  pRefGOF = new MVGroupOfFrames(nSuperLevels, nWidth, nHeight, nSuperPel, nSuperHPad, nSuperVPad, nSuperModeYUV, cpuFlags, xRatioUVs[1], yRatioUVs[1], pixelsize_super, bits_per_pixel_super, mt_flag);
  pSrcGOF = new MVGroupOfFrames(nSuperLevels, nWidth, nHeight, nSuperPel, nSuperHPad, nSuperVPad, nSuperModeYUV, cpuFlags, xRatioUVs[1], yRatioUVs[1], pixelsize_super, bits_per_pixel_super, mt_flag);
  nSuperWidth = super->GetVideoInfo().width;
  nSuperHeight = super->GetVideoInfo().height;

  if (nHeight != nHeightS
    || nHeight != vi.height
    || nWidth != nSuperWidth - nSuperHPad * 2
    || nWidth != vi.width
    || nPel != nSuperPel)
  {
    env_ptr->ThrowError("MCompensate : wrong source or super frame size");
  }


  if ((pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2 && !planar)
  {
    DstPlanes = new YUY2Planes(nWidth, nHeight);
  }
  // pitch: not byte* but short* or int*/float* granurality, no need pixelsize correction
  dstShortPitch = AlignNumber(nWidth, 16);
  dstShortPitchUV = AlignNumber(nWidth >> nLogxRatioUVs[1], 16);
  if (nOverlapX > 0 || nOverlapY > 0)
  {
    OverWins = new OverlapWindows(nBlkSizeX, nBlkSizeY, nOverlapX, nOverlapY);
    if (!vi.IsY())
      OverWinsUV = new OverlapWindows(nBlkSizeX >> nLogxRatioUVs[1], nBlkSizeY >> nLogyRatioUVs[1], nOverlapX >> nLogxRatioUVs[1], nOverlapY >> nLogyRatioUVs[1]);

    DstShort = (BYTE *)_aligned_malloc(dstShortPitch * nHeight *  ovrBufferElementSize, 32);
    if (!vi.IsY()) {
      DstShortU = (BYTE *)_aligned_malloc(dstShortPitchUV * nHeight * ovrBufferElementSize, 32);
      DstShortV = (BYTE *)_aligned_malloc(dstShortPitchUV * nHeight * ovrBufferElementSize, 32);
    }
  }

  if (nOverlapY > 0)
  {
    _boundary_cnt_arr.resize(nBlkY);
  }

  if (recursion > 0)
  {
    if ((pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2)
    {
      nLoopPitches[0] = AlignNumber(nSuperWidth * 2, 16);
      nLoopPitches[1] = nLoopPitches[0];
      nLoopPitches[2] = nLoopPitches[1];
      pLoop[0] = (unsigned char *)_aligned_malloc(nLoopPitches[0] * nSuperHeight, 32);
      pLoop[1] = pLoop[0] + nSuperWidth;
      pLoop[2] = pLoop[1] + nSuperWidth / 2;
    }
    else
    {
      nLoopPitches[0] = AlignNumber(nSuperWidth*pixelsize_super, 32);
      nLoopPitches[1] = nLoopPitches[2] = AlignNumber((nSuperWidth >> nLogxRatioUVs[1])*pixelsize_super, 32);
      pLoop[0] = (unsigned char *)_aligned_malloc(nLoopPitches[0] * nSuperHeight, 32);
      pLoop[1] = (unsigned char *)_aligned_malloc(nLoopPitches[1] * nSuperHeight, 32);
      pLoop[2] = (unsigned char *)_aligned_malloc(nLoopPitches[2] * nSuperHeight, 32);
    }
  }

}

MVCompensate::~MVCompensate()
{
  if ((pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2 && !planar)
  {
    delete DstPlanes;
  }
  if (nOverlapX > 0 || nOverlapY > 0)
  {
    delete OverWins;
    delete OverWinsUV;
    _aligned_free(DstShort);
    _aligned_free(DstShortU);
    _aligned_free(DstShortV);
  }
  delete pRefGOF; // v2.0
  delete pSrcGOF;

  if (recursion > 0)
  {
    _aligned_free(pLoop[0]);
    if ((pixelType & VideoInfo::CS_YUY2) != VideoInfo::CS_YUY2)
    {
      _aligned_free(pLoop[1]);
      _aligned_free(pLoop[2]);
    }
  }
}

PVideoFrame __stdcall MVCompensate::GetFrame(int n, IScriptEnvironment* env_ptr)
{
  int nsrc;
  int nvec;
  int vindex;
  const bool comp_flag = compute_src_frame(nsrc, nvec, vindex, n);
  if (!comp_flag)
  {
    return (_cclip_sptr->GetFrame(nsrc, env_ptr));
  }
  MvClipInfo &info = _mv_clip_arr[vindex];
  _mv_clip_ptr = info._clip_sptr.get();
  _thsad = info._thsad;

  int nWidth_B = nBlkX * (nBlkSizeX - nOverlapX) + nOverlapX;
  int nHeight_B = nBlkY * (nBlkSizeY - nOverlapY) + nOverlapY;

  const BYTE *pRef[3];
  int nRefPitches[3];
  unsigned char *pDstYUY2;
  int nDstPitchYUY2;
  int nOffset[3];

  if (recursion > 0) {
    nOffset[0] = (nHPadding << pixelsize_super_shift) + nVPadding * nLoopPitches[0];
    nOffset[1] = nOffset[2] = ((nHPadding >> nLogxRatioUVs[1]) << pixelsize_super_shift) + (nVPadding >> nLogyRatioUVs[1]) * nLoopPitches[1];
  }

  PVideoFrame mvn = _mv_clip_ptr->GetFrame(nvec, env_ptr);
  _mv_clip_ptr->Update(mvn, env_ptr);
  mvn = 0; // free

  PVideoFrame src = super->GetFrame(nsrc, env_ptr);
  PVideoFrame dst = env_ptr->NewVideoFrame(vi); // frame property support later
  bool usable_flag = _mv_clip_ptr->IsUsable();
  int nref;
  _mv_clip_ptr->use_ref_frame(nref, usable_flag, super, nsrc, env_ptr);

  const int planes_y[4] = { PLANAR_Y, PLANAR_U, PLANAR_V, PLANAR_A };
  const int planes_r[4] = { PLANAR_G, PLANAR_B, PLANAR_R, PLANAR_A };
  const int *planes = (vi.IsYUV() || vi.IsYUVA()) ? planes_y : planes_r;

  if (usable_flag)
  {
    if (has_at_least_v8) env_ptr->copyFrameProps(src, dst); // frame property copy V8
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
        pDst[0] = dst->GetWritePtr();
        pDst[1] = pDst[0] + nWidth;
        pDst[2] = pDst[1] + nWidth / 2;
        nDstPitches[0] = dst->GetPitch();
        nDstPitches[1] = nDstPitches[0];
        nDstPitches[2] = nDstPitches[0];
      }
      pSrc[0] = src->GetReadPtr();
      pSrc[1] = pSrc[0] + nSuperWidth;
      pSrc[2] = pSrc[1] + nSuperWidth / 2;
      nSrcPitches[0] = src->GetPitch();
      nSrcPitches[1] = nSrcPitches[0];
      nSrcPitches[2] = nSrcPitches[0];
    }

    else
    {
      for (int p = 0; p < planecount; ++p) {
        const int plane = planes[p];
        pSrc[p] = src->GetReadPtr(plane);
        nSrcPitches[p] = src->GetPitch(plane);
        pDst[p] = dst->GetWritePtr(plane);
        nDstPitches[p] = dst->GetPitch(plane);
      }
    }

    PVideoFrame ref = super->GetFrame(nref, env_ptr);

    if ((pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2)
    {
      pRef[0] = ref->GetReadPtr();
      pRef[1] = pRef[0] + nSuperWidth;
      pRef[2] = pRef[1] + nSuperWidth / 2; // xRatioUV YUY2
      nRefPitches[0] = ref->GetPitch();
      nRefPitches[1] = nRefPitches[0];
      nRefPitches[2] = nRefPitches[0];
    }
    else
    {
      for (int p = 0; p < planecount; ++p) {
        const int plane = planes[p];
        pRef[p] = ref->GetReadPtr(plane);
        nRefPitches[p] = ref->GetPitch(plane);
      }
    }

    if (recursion > 0)
    {
      for (int i = 0; i < planecount; i++) {
        if (pixelsize == 1)
          Blend<uint8_t>(pLoop[i], pLoop[i], pRef[i], nSuperHeight >> nLogyRatioUVs[i], nSuperWidth >> nLogxRatioUVs[i], nLoopPitches[i], nLoopPitches[i], nRefPitches[i], time256, cpuFlags);
        else if (pixelsize == 2)
          Blend<uint16_t>(pLoop[i], pLoop[i], pRef[i], nSuperHeight >> nLogyRatioUVs[i], nSuperWidth >> nLogxRatioUVs[i], nLoopPitches[i], nLoopPitches[i], nRefPitches[i], time256, cpuFlags);
        else // pixelsize == 4
          Blend<float>(pLoop[i], pLoop[i], pRef[i], nSuperHeight >> nLogyRatioUVs[i], nSuperWidth >> nLogxRatioUVs[i], nLoopPitches[i], nLoopPitches[i], nRefPitches[i], time256, cpuFlags);
      }
      pRefGOF->Update(YUVPLANES, (BYTE*)pLoop[0], nLoopPitches[0], (BYTE*)pLoop[1], nLoopPitches[1], (BYTE*)pLoop[2], nLoopPitches[2]);
    }
    else
    {
      pRefGOF->Update(YUVPLANES, (BYTE*)pRef[0], nRefPitches[0], (BYTE*)pRef[1], nRefPitches[1], (BYTE*)pRef[2], nRefPitches[2]);// v2.0
    }
    pSrcGOF->Update(YUVPLANES, (BYTE*)pSrc[0], nSrcPitches[0], (BYTE*)pSrc[1], nSrcPitches[1], (BYTE*)pSrc[2], nSrcPitches[2]);

    pPlanes[0] = pRefGOF->GetFrame(0)->GetPlane(YPLANE);
    pSrcPlanes[0] = pSrcGOF->GetFrame(0)->GetPlane(YPLANE);
    if (planecount > 1) {
      pPlanes[1] = pRefGOF->GetFrame(0)->GetPlane(UPLANE);
      pPlanes[2] = pRefGOF->GetFrame(0)->GetPlane(VPLANE);

      pSrcPlanes[1] = pSrcGOF->GetFrame(0)->GetPlane(UPLANE);
      pSrcPlanes[2] = pSrcGOF->GetFrame(0)->GetPlane(VPLANE);
    }

    /*
    int fieldShift = 0;
    if (fields && nPel > 1 && ((nref - n) % 2 != 0))
    {
      bool paritySrc = child->GetParity(n);
      bool parityRef = child->GetParity(nref);
      fieldShift = (paritySrc && !parityRef) ? nPel / 2 : ((parityRef && !paritySrc) ? -(nPel / 2) : 0);
      // vertical shift of fields for fieldbased video at finest level pel2
    } more elegantly:
    */
    fieldShift = ClipFnc::compute_fieldshift(child, fields, nPel, nsrc, nref);

    PROFILE_START(MOTION_PROFILE_COMPENSATION);

    Slicer         slicer(_mt_flag); // prepare internal avstp multithreading

    // No overlap
    if (nOverlapX == 0 && nOverlapY == 0)
    {
      slicer.start(nBlkY, *this, &MVCompensate::compensate_slice_normal);
      slicer.wait();
    }

    // Overlap
    else
    {
      if (nOverlapY > 0)
      {
        memset(
          &_boundary_cnt_arr[0],
          0,
          _boundary_cnt_arr.size() * sizeof(_boundary_cnt_arr[0])
        );
      }

      MemZoneSet(reinterpret_cast<unsigned char*>(DstShort), 0, nWidth_B * ovrBufferElementSize, nHeight_B, 0, 0, dstShortPitch * ovrBufferElementSize);
      if (pPlanes[1])
        MemZoneSet(reinterpret_cast<unsigned char*>(DstShortU), 0, (nWidth_B * ovrBufferElementSize) >> nLogxRatioUVs[1], nHeight_B >> nLogyRatioUVs[1], 0, 0, dstShortPitchUV * ovrBufferElementSize);
      if (pPlanes[2])
        MemZoneSet(reinterpret_cast<unsigned char*>(DstShortV), 0, (nWidth_B * ovrBufferElementSize) >> nLogxRatioUVs[2], nHeight_B >> nLogyRatioUVs[2], 0, 0, dstShortPitchUV * ovrBufferElementSize);

      slicer.start(nBlkY, *this, &MVCompensate::compensate_slice_overlap, 2);
      slicer.wait();

      if (pixelsize_super == 1) {
        // nWidth_B and nHeight_B, right and bottom was blended
        if ((cpuFlags & CPUF_SSE2) != 0) {
          Short2Bytes_sse2(pDst[0], nDstPitches[0], (uint16_t *)DstShort, dstShortPitch, nWidth_B, nHeight_B);
          if (pPlanes[1])
            Short2Bytes_sse2(pDst[1], nDstPitches[1], (uint16_t *)DstShortU, dstShortPitchUV, nWidth_B >> nLogxRatioUVs[1], nHeight_B >> nLogyRatioUVs[1]);
          if (pPlanes[2])
            Short2Bytes_sse2(pDst[2], nDstPitches[2], (uint16_t *)DstShortV, dstShortPitchUV, nWidth_B >> nLogxRatioUVs[2], nHeight_B >> nLogyRatioUVs[2]);
        }
        else {
          Short2Bytes(pDst[0], nDstPitches[0], (uint16_t *)DstShort, dstShortPitch, nWidth_B, nHeight_B);
          if (pPlanes[1])
            Short2Bytes(pDst[1], nDstPitches[1], (uint16_t *)DstShortU, dstShortPitchUV, nWidth_B >> nLogxRatioUVs[1], nHeight_B >> nLogyRatioUVs[1]);
          if (pPlanes[2])
            Short2Bytes(pDst[2], nDstPitches[2], (uint16_t *)DstShortV, dstShortPitchUV, nWidth_B >> nLogxRatioUVs[2], nHeight_B >> nLogyRatioUVs[2]);
        }
      }
      else if (pixelsize_super == 2)
      {
        if ((cpuFlags & CPUF_SSE4_1) != 0) {
          Short2Bytes_Int32toWord16_sse4((uint16_t *)(pDst[0]), nDstPitches[0], (int *)DstShort, dstShortPitch, nWidth_B, nHeight_B, bits_per_pixel_super);
          if (pPlanes[1])
            Short2Bytes_Int32toWord16_sse4((uint16_t *)(pDst[1]), nDstPitches[1], (int *)DstShortU, dstShortPitchUV, nWidth_B >> nLogxRatioUVs[1], nHeight_B >> nLogyRatioUVs[1], bits_per_pixel_super);
          if (pPlanes[2])
            Short2Bytes_Int32toWord16_sse4((uint16_t *)(pDst[2]), nDstPitches[2], (int *)DstShortV, dstShortPitchUV, nWidth_B >> nLogxRatioUVs[2], nHeight_B >> nLogyRatioUVs[2], bits_per_pixel_super);
        }
        else {
          Short2Bytes_Int32toWord16((uint16_t *)(pDst[0]), nDstPitches[0], (int *)DstShort, dstShortPitch, nWidth_B, nHeight_B, bits_per_pixel_super);
          if (pPlanes[1])
            Short2Bytes_Int32toWord16((uint16_t *)(pDst[1]), nDstPitches[1], (int *)DstShortU, dstShortPitchUV, nWidth_B >> nLogxRatioUVs[1], nHeight_B >> nLogyRatioUVs[1], bits_per_pixel_super);
          if (pPlanes[2])
            Short2Bytes_Int32toWord16((uint16_t *)(pDst[2]), nDstPitches[2], (int *)DstShortV, dstShortPitchUV, nWidth_B >> nLogxRatioUVs[2], nHeight_B >> nLogyRatioUVs[2], bits_per_pixel_super);
        }
      }
      else { // pixelsize_super == 4
        Short2Bytes_FloatInInt32ArrayToFloat((float *)(pDst[0]), nDstPitches[0], (float *)DstShort, dstShortPitch, nWidth_B, nHeight_B);
        if (pPlanes[1])
          Short2Bytes_FloatInInt32ArrayToFloat((float *)(pDst[1]), nDstPitches[1], (float *)DstShortU, dstShortPitchUV, nWidth_B >> nLogxRatioUVs[1], nHeight_B >> nLogyRatioUVs[1]);
        if (pPlanes[2])
          Short2Bytes_FloatInInt32ArrayToFloat((float *)(pDst[2]), nDstPitches[2], (float *)DstShortV, dstShortPitchUV, nWidth_B >> nLogxRatioUVs[2], nHeight_B >> nLogyRatioUVs[2]);
      }
    }

    // padding of the non-covered regions
    const BYTE **  pSrcMapped = (scBehavior) ? pSrc : pRef;
    int *          pPitchesMapped = (scBehavior) ? nSrcPitches : nRefPitches;

    if (nWidth_B < nWidth) // Right padding
    {
      for (int i = 0; i < planecount; i++) {
        if (pPlanes[i])
          BitBlt(pDst[i] + ((nWidth_B >> nLogxRatioUVs[i]) << pixelsize_super_shift), nDstPitches[i],
            pSrcMapped[i] + (((nWidth_B >> nLogxRatioUVs[i]) + (nHPadding >> nLogxRatioUVs[i])) << pixelsize_super_shift) + (nVPadding >> nLogyRatioUVs[i]) * pPitchesMapped[i], pPitchesMapped[i],
            ((nWidth - nWidth_B) >> nLogxRatioUVs[i]) << pixelsize_super_shift, nHeight_B >> nLogyRatioUVs[i]);
      }
    }

    if (nHeight_B < nHeight) // Bottom padding
    {
      for (int i = 0; i < planecount; i++) {
        if (pPlanes[i])
          BitBlt(pDst[i] + (nHeight_B >> nLogyRatioUVs[i])*nDstPitches[i], nDstPitches[i],
            pSrcMapped[i] + ((nHPadding >> nLogxRatioUVs[i]) << pixelsize_super_shift) + ((nHeight_B + nVPadding) >> nLogyRatioUVs[i]) * pPitchesMapped[i], pPitchesMapped[i],
            (nWidth >> nLogxRatioUVs[i]) << pixelsize_super_shift, (nHeight - nHeight_B) >> nLogyRatioUVs[i]);
      }
        // PF fix 161117: nHPadding was not shifted for bottom padding, exists in 2.5.11.22??
        // BitBlt(pDst[1] + (nHeight_B>>nLogyRatioUV)*nDstPitches[1], nDstPitches[1], pSrcMapped[1] + nHPadding + ((nHeight_B + nVPadding)>>nLogyRatioUV) * pPitchesMapped[1], pPitchesMapped[1], nWidth>>nLogxRatioUV, (nHeight-nHeight_B)>>nLogyRatioUV, isse_flag);
    }

    PROFILE_STOP(MOTION_PROFILE_COMPENSATION);

    // if we're in in-loop recursive mode, we copy the frame
    if (recursion > 0)
    {
      for (int i = 0; i < planecount; i++) {
        if (pLoop[i])
          env_ptr->BitBlt(pLoop[i] + nOffset[i], nLoopPitches[i], pDst[i], nDstPitches[i], (nWidth >> nLogxRatioUVs[i]) << pixelsize_super_shift, nHeight >> nLogyRatioUVs[i]);
      }
    }

    if ((pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2 && !planar)
    {
      YUY2FromPlanes(
        pDstYUY2, nDstPitchYUY2, nWidth, nHeight,
        pDst[0], nDstPitches[0], pDst[1], pDst[2], nDstPitches[1], cpuFlags
      );
    }
  }

  // ! usable_flag
  else
  {
    if (!scBehavior && (nref < vi.num_frames) && (nref >= 0))
    {
      src = super->GetFrame(nref, env_ptr);
    }
    else
    {
      src = super->GetFrame(nsrc, env_ptr);
    }
    if (has_at_least_v8) env_ptr->copyFrameProps(src, dst); // frame property support v8

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
        pDst[0] = dst->GetWritePtr();
        pDst[1] = pDst[0] + nWidth;
        pDst[2] = pDst[1] + nWidth / 2; // YUY2
        nDstPitches[0] = dst->GetPitch();
        nDstPitches[1] = nDstPitches[0];
        nDstPitches[2] = nDstPitches[0];
      }
      pSrc[0] = src->GetReadPtr();
      pSrc[1] = pSrc[0] + nSuperWidth;
      pSrc[2] = pSrc[1] + nSuperWidth / 2; // YUY2
      nSrcPitches[0] = src->GetPitch();
      nSrcPitches[1] = nSrcPitches[0];
      nSrcPitches[2] = nSrcPitches[0];
    }
    else
    {
      for (int p = 0; p < planecount; ++p) {
        const int plane = planes[p];
        pSrc[p] = src->GetReadPtr(plane);
        nSrcPitches[p] = src->GetPitch(plane);
        pDst[p] = dst->GetWritePtr(plane);
        nDstPitches[p] = dst->GetPitch(plane);
      }
    }

    nOffset[0] = nHPadding * pixelsize_super + nVPadding * nSrcPitches[0];
    nOffset[1] = nOffset[2] = (nHPadding >> nLogxRatioUV) * pixelsize_super + (nVPadding >> nLogyRatioUVs[1]) * nSrcPitches[1];

    for (int i = 0; i < planecount; i++) {
      env_ptr->BitBlt(pDst[i], nDstPitches[i], pSrc[i] + nOffset[i], nSrcPitches[i], (nWidth >> nLogxRatioUVs[i]) << pixelsize_super_shift, nHeight >> nLogyRatioUVs[i]);
    }

    if ((pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2 && !planar)
    {
      YUY2FromPlanes(pDstYUY2, nDstPitchYUY2, nWidth, nHeight,
        pDst[0], nDstPitches[0], pDst[1], pDst[2], nDstPitches[1], cpuFlags);
    }

    if (recursion > 0)
    {
      for (int i = 0; i < planecount; i++) {
        if (pLoop[i])
          env_ptr->BitBlt(pLoop[i], nLoopPitches[i], pSrc[i], nSrcPitches[i], (nWidth >> nLogxRatioUVs[i]) << pixelsize_super_shift, nHeight >> nLogyRatioUVs[i]);
      }
    }
  }

#ifndef _M_X64
  _mm_empty(); // (we may use double-float somewhere) Fizick
#endif

  _mv_clip_ptr = 0;

  return dst;
}



void	MVCompensate::compensate_slice_normal(Slicer::TaskData &td)
{
  assert(&td != 0);

  BYTE *         pDstCur[3];
  const BYTE *   pSrcCur[3];
  ptrdiff_t      DstCurPitches[3];
  ptrdiff_t      SrcCurPitches[3];

  for (int i = 0; i < planecount; i++) {
    int rowsize = nBlkSizeY >> nLogyRatioUVs[i];
    pDstCur[i] = pDst[i] + td._y_beg * rowsize * nDstPitches[i];
    DstCurPitches[i] = rowsize * nDstPitches[i];
    pSrcCur[i] = pSrc[i] + td._y_beg * rowsize * nSrcPitches[i];
    SrcCurPitches[i] = rowsize * nSrcPitches[i];
  }

  for (int by = td._y_beg; by < td._y_end; ++by)
  {
    int xx = 0; // pixelsize-aware, no need to multiply inside
    for (int bx = 0; bx < nBlkX; ++bx)
    {
      const int index = by * nBlkX + bx;
      const FakeBlockData &block = _mv_clip_ptr->GetBlock(0, index);
      /*
      blx = block.GetX() * nPel + block.GetMV().x * time256 / 256;
      bly = block.GetY() * nPel + block.GetMV().y * time256 / 256 + fieldShift;
      */
      const int      blx = block.GetX() * nPel + block.GetMV().x * time256 / 256; // 2.5.11.22
      const int      bly = block.GetY() * nPel + block.GetMV().y * time256 / 256 + fieldShift; // 2.5.11.22
      if (block.GetSAD() < _thsad)
      {
        // luma
        BLITLUMA(
          pDstCur[0] + xx, nDstPitches[0],
          pPlanes[0]->GetPointer(blx, bly), pPlanes[0]->GetPitch()
        );
        for (int i = 1; i < planecount; i++) {
          if (pPlanes[i])
          {
            BLITCHROMA(pDstCur[i] + (xx >> nLogxRatioUVs[i]), nDstPitches[i],
              pPlanes[i]->GetPointer(blx >> nLogxRatioUVs[i], bly >> nLogyRatioUVs[i]), pPlanes[i]->GetPitch()
            );
          }
        }
      }
      else
      {
        int blxsrc = bx * nBlkSizeX * nPel;
        int blysrc = by * nBlkSizeY * nPel + fieldShift;

        BLITLUMA(
          pDstCur[0] + xx, nDstPitches[0],
          pSrcPlanes[0]->GetPointer(blxsrc, blysrc), pSrcPlanes[0]->GetPitch()
        );
        for (int i = 1; i < planecount; i++) {
          if (pSrcPlanes[i])
            BLITCHROMA(
              pDstCur[i] + (xx >> nLogxRatioUVs[i]), nDstPitches[i],
              pSrcPlanes[i]->GetPointer(blxsrc >> nLogxRatioUVs[i], blysrc >> nLogyRatioUVs[i]), pSrcPlanes[i]->GetPitch()
            );
        }
      }

      xx += nBlkSizeX << pixelsize_super_shift;
    } // for bx

    pDstCur[0] += DstCurPitches[0];
    pDstCur[1] += DstCurPitches[1];
    pDstCur[2] += DstCurPitches[2];
    pSrcCur[0] += SrcCurPitches[0];
    pSrcCur[1] += SrcCurPitches[1];
    pSrcCur[2] += SrcCurPitches[2];
  } // for by
}




void	MVCompensate::compensate_slice_overlap(Slicer::TaskData &td)
{
  assert(&td != 0);

  if (nOverlapY == 0
    || (td._y_beg == 0 && td._y_end == nBlkY))
  {
    compensate_slice_overlap(td._y_beg, td._y_end);
  }

  else
  {
    assert(td._y_end - td._y_beg >= 2);

    compensate_slice_overlap(td._y_beg, td._y_end - 1);

    const conc::AioAdd <int> inc_ftor(+1);

    const int		cnt_top = conc::AtomicIntOp::exec_new(
      _boundary_cnt_arr[td._y_beg],
      inc_ftor
    );
    if (td._y_beg > 0 && cnt_top == 2)
    {
      compensate_slice_overlap(td._y_beg - 1, td._y_beg);
    }

    int cnt_bot = 2;
    if (td._y_end < nBlkY)
    {
      cnt_bot = conc::AtomicIntOp::exec_new(
        _boundary_cnt_arr[td._y_end],
        inc_ftor
      );
    }
    if (cnt_bot == 2)
    {
      compensate_slice_overlap(td._y_end - 1, td._y_end);
    }
  }
}



void	MVCompensate::compensate_slice_overlap(int y_beg, int y_end)
{
  int rowsizes[3];

  BYTE *pDstShorts[3];
  BYTE *         pDstCur[3];
  const BYTE *   pSrcCur[3];

  BYTE *DstShorts[3];
  DstShorts[0] = (BYTE *)DstShort;
  DstShorts[1] = (BYTE *)DstShortU;
  DstShorts[2] = (BYTE *)DstShortV;

  int dstShortPitches[3];
  dstShortPitches[0] = dstShortPitch;
  dstShortPitches[1] = dstShortPitchUV;
  dstShortPitches[2] = dstShortPitchUV;

  for (int i = 0; i < planecount; i++) {
    int rowsize = (nBlkSizeY - nOverlapY) >> nLogyRatioUVs[i];
    rowsizes[i] = rowsize;
    pDstShorts[i] = (BYTE *)DstShorts[i] + y_beg * rowsize * (dstShortPitches[i] << ovrBufferElementSize_shift);
    pDstCur[i] = pDst[i] + y_beg * rowsize * nDstPitches[i];
    pSrcCur[i] = pSrc[i] + y_beg * rowsize * nSrcPitches[i];
  }

  for (int by = y_beg; by < y_end; ++by)
  {
    // indexing overlap windows weighting table: top=0 middle=3 bottom=6
    /*
    0 = Top Left    1 = Top Middle    2 = Top Right
    3 = Middle Left 4 = Middle Middle 5 = Middle Right
    6 = Bottom Left 7 = Bottom Middle 8 = Bottom Right
    */

    int wby = (by == 0) ? 0 * 3 : (by == nBlkY - 1) ? 2 * 3 : 1 * 3; // 0 for very first, 2*3 for very last, 1*3 for all others in the middle
    int xx = 0; // xx is pixelsize-aware
    for (int bx = 0; bx < nBlkX; ++bx)
    {
      // select window
      // indexing overlap windows weighting table: left=+0 middle=+1 rightmost=+2
      int wbx = (bx == 0) ? 0 : (bx == nBlkX - 1) ? 2 : 1; // 0 for very first, 2 for very last, 1 for all others in the middle
      short *winOver = OverWins->GetWindow(wby + wbx);
      short *winOverUV = OverWinsUV->GetWindow(wby + wbx);

      const int index = by * nBlkX + bx;
      const FakeBlockData & block = _mv_clip_ptr->GetBlock(0, index);

      /*
     blx = block.GetX() * nPel + block.GetMV().x * time256 / 256;
     bly = block.GetY() * nPel + block.GetMV().y * time256 / 256 + fieldShift;
     */
      const int blx = block.GetX() * nPel + block.GetMV().x * time256 / 256; // 2.5.11.22
      const int bly = block.GetY() * nPel + block.GetMV().y * time256 / 256 + fieldShift; // 2.5.11.22

      if (block.GetSAD() < _thsad)
      {
        if (pixelsize_super == 1) {
          // luma
          OVERSLUMA(
            (uint16_t *)(pDstShorts[0] + xx), dstShortPitches[0],
            pPlanes[0]->GetPointer(blx, bly), pPlanes[0]->GetPitch(),
            winOver, nBlkSizeX
          );
          for (int i = 1; i < planecount; i++) {
            if (pPlanes[i])
              OVERSCHROMA(
              (uint16_t *)(pDstShorts[i] + (xx >> nLogxRatioUVs[i])), dstShortPitches[i],
                pPlanes[i]->GetPointer(blx >> nLogxRatioUVs[i], bly >> nLogyRatioUVs[i]), pPlanes[i]->GetPitch(),
                winOverUV, nBlkSizeX >> nLogxRatioUVs[i]
              );
          }
        }
        else if (pixelsize_super == 2) {
          // luma
          OVERSLUMA16(
            (uint16_t *)(pDstShorts[0] + xx), dstShortPitches[0],
            pPlanes[0]->GetPointer(blx, bly), pPlanes[0]->GetPitch(),
            winOver, nBlkSizeX
          );
          // chroma uv
          for (int i = 1; i < planecount; i++) {
            if (pPlanes[i])
              OVERSCHROMA16(
              (uint16_t *)(pDstShorts[i] + (xx >> nLogxRatioUVs[i])), dstShortPitches[i],
                pPlanes[i]->GetPointer(blx >> nLogxRatioUVs[i], bly >> nLogyRatioUVs[i]), pPlanes[i]->GetPitch(),
                winOverUV, nBlkSizeX >> nLogxRatioUVs[i]
              );
          }
        }
        else { // pixelsize_super == 4
     // luma
          OVERSLUMA32(
            (uint16_t *)(pDstShorts[0] + xx), dstShortPitches[0],
            pPlanes[0]->GetPointer(blx, bly), pPlanes[0]->GetPitch(),
            winOver, nBlkSizeX
          );
          // chroma uv
          for (int i = 1; i < planecount; i++) {
            if (pPlanes[i])
              OVERSCHROMA32(
              (uint16_t *)(pDstShorts[i] + (xx >> nLogxRatioUVs[i])), dstShortPitches[i],
                pPlanes[i]->GetPointer(blx >> nLogxRatioUVs[i], bly >> nLogyRatioUVs[i]), pPlanes[i]->GetPitch(),
                winOverUV, nBlkSizeX >> nLogxRatioUVs[i]
              );
          }
        }
      }

      // bad compensation, use src
      else
      {
        int blxsrc = bx * (nBlkSizeX - nOverlapX) * nPel;
        int blysrc = by * (nBlkSizeY - nOverlapY) * nPel + fieldShift;

        if (pixelsize_super == 1) {
          OVERSLUMA(
            (uint16_t *)(pDstShorts[0] + xx), dstShortPitches[0],
            pSrcPlanes[0]->GetPointer(blxsrc, blysrc), pSrcPlanes[0]->GetPitch(),
            winOver, nBlkSizeX
          );
          // chroma uv
          for (int i = 1; i < planecount; i++) {
            if (pSrcPlanes[i])
              OVERSCHROMA(
              (uint16_t *)(pDstShorts[i] + (xx >> nLogxRatioUVs[i])), dstShortPitches[i],
                pSrcPlanes[i]->GetPointer(blxsrc >> nLogxRatioUVs[i], blysrc >> nLogyRatioUVs[i]), pSrcPlanes[i]->GetPitch(),
                winOverUV, nBlkSizeX >> nLogxRatioUVs[i]
              );
          }
        }
        else if (pixelsize_super == 2){
          // pixelsize == 2
          OVERSLUMA16(
            (uint16_t *)(pDstShorts[0] + xx), dstShortPitches[0],
            pSrcPlanes[0]->GetPointer(blxsrc, blysrc), pSrcPlanes[0]->GetPitch(),
            winOver, nBlkSizeX
          );
          // chroma uv
          for (int i = 1; i < planecount; i++) {
            if (pSrcPlanes[i])
              OVERSCHROMA16(
              (uint16_t *)(pDstShorts[i] + (xx >> nLogxRatioUVs[i])), dstShortPitches[i],
                pSrcPlanes[i]->GetPointer(blxsrc >> nLogxRatioUVs[i], blysrc >> nLogyRatioUVs[i]), pSrcPlanes[i]->GetPitch(),
                winOverUV, nBlkSizeX >> nLogxRatioUVs[i]
              );
          }
        }
        else { // if (pixelsize_super == 4)
          OVERSLUMA32(
            (uint16_t *)(pDstShorts[0] + xx), dstShortPitches[0],
            pSrcPlanes[0]->GetPointer(blxsrc, blysrc), pSrcPlanes[0]->GetPitch(),
            winOver, nBlkSizeX
          );
          // chroma uv
          for (int i = 1; i < planecount; i++) {
            if (pSrcPlanes[i])
              OVERSCHROMA32(
              (uint16_t *)(pDstShorts[i] + (xx >> nLogxRatioUVs[i])), dstShortPitches[i],
              pSrcPlanes[i]->GetPointer(blxsrc >> nLogxRatioUVs[i], blysrc >> nLogyRatioUVs[i]), pSrcPlanes[i]->GetPitch(),
              winOverUV, nBlkSizeX >> nLogxRatioUVs[i]
            );
          }
        }
      }

      xx += (nBlkSizeX - nOverlapX) << ovrBufferElementSize_shift;
      // really if xx is a simple counter or byte pointer, this should be *2 (pixelsize==1) or *4 (pixelsize==2 and pixelsize==4)
    } // for bx

    for (int i = 0; i < planecount; i++) {
      pDstShorts[i] += rowsizes[i] * (dstShortPitches[i] << ovrBufferElementSize_shift);
      pDstCur[i] += rowsizes[i] * nDstPitches[i];
      pSrcCur[i] += rowsizes[i] * nSrcPitches[i];
    }
  } // for by
}



// Returns false if center frame should be used.
bool MVCompensate::compute_src_frame(int &nsrc, int &nvec, int &vindex, int n) const
{
  assert(n >= 0);

  bool comp_flag = true;

  nsrc = n;
  nvec = n;
  vindex = 0;

  if (_multi_flag)
  {
    int tbsize = _trad * 2;
    if (_center_flag)
    {
      ++tbsize;

      const int base = n / tbsize;
      const int offset = n - base * tbsize;
      if (offset == _trad)
      {
        comp_flag = false;
        vindex = 0;
        nvec = 0;
      }
      else
      {
        const int bf = (offset > _trad) ? 1 - 2 : 0 - 2;
        const int td = abs(offset - _trad);
        vindex = td * 2 + bf;
      }
      nsrc = base;
      nvec = base;
    }
    else
    {
      nsrc = n / tbsize;
      vindex = n % tbsize;
    }
  }

  return (comp_flag);
}



