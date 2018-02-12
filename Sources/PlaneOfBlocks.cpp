// Author: Manao
// Copyright(c)2006 A.G.Balakhnin aka Fizick - global motion, overlap,  mode, refineMVs
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



#include "AnaFlags.h"
#include "AvstpWrapper.h"
#include "commonfunctions.h"
#include "DCTClass.h"
#include "DCTFactory.h"
#include "debugprintf.h"
#include "FakePlaneOfBlocks.h"
#include "MVClip.h"
#include "MVFrame.h"
#include "MVPlane.h"
#include "PlaneOfBlocks.h"
#include "Padding.h"
#include "profile.h"

#include <mmintrin.h>

#include	<algorithm>
#include	<stdexcept>
#include <stdint.h>

PlaneOfBlocks::PlaneOfBlocks(int _nBlkX, int _nBlkY, int _nBlkSizeX, int _nBlkSizeY, int _nPel, int _nLevel, int _nFlags, int _nOverlapX, int _nOverlapY, int _xRatioUV, int _yRatioUV, int _pixelsize, int _bits_per_pixel, conc::ObjPool <DCTClass> *dct_pool_ptr, bool mt_flag, int _chromaSADscale, IScriptEnvironment* env)
  : nBlkX(_nBlkX)
  , nBlkY(_nBlkY)
  , nBlkSizeX(_nBlkSizeX)
  , nBlkSizeY(_nBlkSizeY)
  , nBlkCount(_nBlkX * _nBlkY)
  , nPel(_nPel)
  , nLogPel(ilog2(_nPel))	// nLogPel=0 for nPel=1, 1 for nPel=2, 2 for nPel=4, i.e. (x*nPel) = (x<<nLogPel)
  , nLogScale(_nLevel)
  , nScale(iexp2(_nLevel))
  , nFlags(_nFlags)
  , nOverlapX(_nOverlapX)
  , nOverlapY(_nOverlapY)
  , xRatioUV(_xRatioUV) // PF
  , nLogxRatioUV(ilog2(_xRatioUV))
  , yRatioUV(_yRatioUV)
  , nLogyRatioUV(ilog2(_yRatioUV))
  , pixelsize(_pixelsize) // PF
  , pixelsize_shift(ilog2(pixelsize)) // 161201
  , bits_per_pixel(_bits_per_pixel) // PF
  , _mt_flag(mt_flag)
  , SAD(0)
  , LUMA(0)
  , VAR(0)
  , BLITLUMA(0)
  , BLITCHROMA(0)
  , SADCHROMA(0)
  , SATD(0)
  , vectors(nBlkCount)
  , smallestPlane((_nFlags & MOTION_SMALLEST_PLANE) != 0)
  //,	mmx ((_nFlags & MOTION_USE_MMX) != 0)
  , isse((_nFlags & MOTION_USE_ISSE) != 0)
  , chroma((_nFlags & MOTION_USE_CHROMA_MOTION) != 0)
  , dctpitch(AlignNumber(_nBlkSizeX, 16) * _pixelsize)
  , _dct_pool_ptr(dct_pool_ptr)
  , verybigSAD(3 * _nBlkSizeX * _nBlkSizeY * (pixelsize == 4 ? 1 : (1 << bits_per_pixel))) // * 256, pixelsize==2 -> 65536. Float:1
  , freqArray()
  , dctmode(0)
  , _workarea_fact(nBlkSizeX, nBlkSizeY, dctpitch, nLogxRatioUV, xRatioUV, nLogyRatioUV, yRatioUV, pixelsize, bits_per_pixel)
  , _workarea_pool()
  , _gvect_estim_ptr(0)
  , _gvect_result_count(0)
  , chromaSADscale(
    _chromaSADscale
  )
{
  _workarea_pool.set_factory(_workarea_fact);

  // half must be more than max vector length, which is (framewidth + Padding) * nPel
  freqArray[0].resize(8192 * _nPel * 2);
  freqArray[1].resize(8192 * _nPel * 2);
  // for nFlags, we use CPU_xxxx constants instead of Avisynth's CPUF_xxx values, because there are extra bits here
  bool sse2 = (bool)(nFlags & CPU_SSE2); // no tricks for really old processors. If SSE2 is reported, use it
  bool sse41 = (bool)(nFlags & CPU_SSE4);
  bool avx = (bool)(nFlags & CPU_AVX);
  bool avx2 = (bool)(nFlags & CPU_AVX2);
//  bool ssd = (bool)(nFlags & MOTION_USE_SSD);
//  bool satd = (bool)(nFlags & MOTION_USE_SATD);

  // New experiment from 2.7.18.22: keep LumaSAD:chromaSAD ratio to 4:2

  //      luma SAD : chroma SAD
  // YV12 4:(1+1) = 4:2 (this 4:2 is the new standard from 2.7.18.22 even for YV24)
  // YV16 4:(2+2) = 4:4
  // YV24 4:(4+4) = 4:8

  // that means that nSCD1 should be normalize not by subsampling but with user's chromaSADscale

  //                          YV12  YV16   YV24
  // nLogXRatioUV              1      1     0   
  // nLogYRatioUV              1      0     0   
  // effective_chromaSADscales: (shift right chromaSAD)
  // chromaSADscale=0  ->      0      1     2  // default. YV12:no change. YV24: chroma SAD is divided by 4 (shift right 2)
  //               =1  ->     -1      0     1  // YV12: shift right -1 (=left 1, =*2) YV24: divide by 2 (shift right 1)
  //               =2  ->     -2     -1     0  // YV12: shift right -2 (=left 2, =*4) YV24: no change
  effective_chromaSADscale = (2 - (nLogxRatioUV + nLogyRatioUV));
  effective_chromaSADscale -= chromaSADscale; // user parameter to have larger magnitude for chroma SAD
                                              // effective effective_chromaSADscale can be -2..2.
                                              // when chromaSADscale is zero (default), effective_chromaSADscale is 0..2
  // effective_chromaSADscale = 0; pre-2.7.18.22 format specific chroma SAD weight
  //	ssd=false;
  //	satd=false;

  //	globalMVPredictor.x = zeroMV.x;
  //	globalMVPredictor.y = zeroMV.y;
  //	globalMVPredictor.sad = zeroMV.sad;

  memset(&vectors[0], 0, vectors.size() * sizeof(vectors[0]));

  // function's pointers 
  // Sad_C: SadFunction.cpp
  // Var_c: Variance.h   PF nowhere used!!!
  // Luma_c: Variance.h 
  // Copy_C: CopyCode


  SATD = SadDummy; //for now disable SATD if default functions are used

                     // in overlaps.h
                     // OverlapsLsbFunction
                     // OverlapsFunction
                     // in M(V)DegrainX: DenoiseXFunction
  arch_t arch;
  if (isse && avx2)
    arch = USE_AVX2;
  else if (isse && avx)
    arch = USE_AVX;
  else if (isse && sse41)
    arch = USE_SSE41;
  else if (isse && sse2)
    arch = USE_SSE2;
  else
    arch = NO_SIMD;

  SAD = get_sad_function(nBlkSizeX, nBlkSizeY, pixelsize, arch);
  SADCHROMA = get_sad_function(nBlkSizeX / xRatioUV, nBlkSizeY / yRatioUV, pixelsize, arch);
  BLITLUMA = get_copy_function(nBlkSizeX, nBlkSizeY, pixelsize, arch);
  BLITCHROMA = get_copy_function(nBlkSizeX / xRatioUV, nBlkSizeY / yRatioUV, pixelsize, arch);
  //VAR        = get_var_function(nBlkSizeX/xRatioUV, nBlkSizeY/yRatioUV, pixelsize, arch); // variance.h PF: no VAR
  LUMA = get_luma_function(nBlkSizeX, nBlkSizeY, pixelsize, arch); // variance.h
  SATD = get_satd_function(nBlkSizeX, nBlkSizeY, pixelsize, arch); // P.F. 2.7.0.22d SATD made live
  if (SATD == nullptr)
    SATD = SadDummy;
  if (chroma) {
    if (BLITCHROMA == nullptr) {
      // we don't have env ptr here
      env->ThrowError("MVTools: no BLITCHROMA function for block size %dx%d", nBlkSizeX / xRatioUV, nBlkSizeY / yRatioUV);
    }
    if (SADCHROMA == nullptr) {
      // we don't have env ptr here
      env->ThrowError("MVTools: no SADCHROMA function for block size %dx%d", nBlkSizeX / xRatioUV, nBlkSizeY / yRatioUV);
    }
  }
  if (!chroma)
  {
    SADCHROMA = SadDummy;
  }

  // for debug:
  //         SAD = x264_pixel_sad_4x4_mmx2;
  //         VAR = Var_C<8>;
  //         LUMA = Luma_C<8>;
  //         BLITLUMA = Copy_C<16,16>;
  //		 BLITCHROMA = Copy_C<8,8>; // idem
  //		 SADCHROMA = x264_pixel_sad_8x8_mmx2;

#ifdef ALLOW_DCT
  if (_dct_pool_ptr != 0)
  {
    DCTFactory &	dct_fact =
      dynamic_cast <DCTFactory &> (_dct_pool_ptr->use_factory());
    dctmode = dct_fact.get_dctmode();

    // Preallocate DCT objects using FFTW, to make sure they are allocated
    // during the plug-in construction stage, to avoid as possible
    // concurrency with FFTW instantiations from other plug-ins.
    if (dct_fact.use_fftw())
    {
      const int		nbr_threads =
        (_mt_flag)
        ? AvstpWrapper::use_instance().get_nbr_threads()
        : 1;

      std::vector <DCTClass *>	dct_stack;
      dct_stack.reserve(nbr_threads);
      bool				err_flag = false;
      for (int dct_cnt = 0; dct_cnt < nbr_threads && !err_flag; ++dct_cnt)
      {
        DCTClass *			dct_ptr = _dct_pool_ptr->take_obj();
        if (dct_ptr == 0)
        {
          err_flag = true;
        }
        else
        {
          dct_stack.push_back(dct_ptr);
        }
      }
      while (!dct_stack.empty())
      {
        DCTClass *			dct_ptr = dct_stack.back();
        _dct_pool_ptr->return_obj(*dct_ptr);
        dct_stack.pop_back();
      }
      if (err_flag)
      {
        throw std::runtime_error(
          "MVTools: error while trying to allocate DCT objects using FFTW."
        );
      }
    }
  }
#endif
}



PlaneOfBlocks::~PlaneOfBlocks()
{
  // Nothing
}



void PlaneOfBlocks::SearchMVs(
  MVFrame *_pSrcFrame, MVFrame *_pRefFrame,
  SearchType st, int stp, int lambda, sad_t lsad, int pnew,
  int plevel, int flags, sad_t *out, const VECTOR * globalMVec,
  short *outfilebuf, int fieldShift, sad_t * pmeanLumaChange,
  int divideExtra, int _pzero, int _pglobal, sad_t _badSAD, int _badrange, bool meander, int *vecPrev, bool _tryMany
)
{
  // -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
  // Frame- and plane-related data preparation

  zeroMVfieldShifted.x = 0;
  zeroMVfieldShifted.y = fieldShift;
  zeroMVfieldShifted.sad = 0; // vs
#ifdef ALLOW_DCT
  // pMeanLumaChange is scaled by bits_per_pixel
  // bit we keep this factor in the ~16 range
  dctweight16 = std::min((sad_t)16, (abs(*pmeanLumaChange) >> (bits_per_pixel-8)) / (nBlkSizeX*nBlkSizeY)); //equal dct and spatial weights for meanLumaChange=8 (empirical)
#endif	// ALLOW_DCT

  badSAD = _badSAD;
  badrange = _badrange;
  _glob_mv_pred_def.x = globalMVec->x * nPel;	// v1.8.2
  _glob_mv_pred_def.y = globalMVec->y * nPel + fieldShift;
  _glob_mv_pred_def.sad = globalMVec->sad;

  //	int nOutPitchY = nBlkX * (nBlkSizeX - nOverlapX) + nOverlapX;
  //	int nOutPitchUV = (nBlkX * (nBlkSizeX - nOverlapX) + nOverlapX) / 2; // xRatioUV=2
  //	char debugbuf[128];
  //	wsprintf(debugbuf,"MVCOMP1: nOutPitchUV=%d, nOverlap=%d, nBlkX=%d, nBlkSize=%d",nOutPitchUV, nOverlap, nBlkX, nBlkSize);
  //	OutputDebugString(debugbuf);

    // write the plane's header
  WriteHeaderToArray(out);

  nFlags |= flags;

  pSrcFrame = _pSrcFrame;
  pRefFrame = _pRefFrame;

#if (ALIGN_SOURCEBLOCK > 1)
  nSrcPitch_plane[0] = pSrcFrame->GetPlane(YPLANE)->GetPitch();
  if (chroma)
  {
    nSrcPitch_plane[1] = pSrcFrame->GetPlane(UPLANE)->GetPitch();
    nSrcPitch_plane[2] = pSrcFrame->GetPlane(VPLANE)->GetPitch();
  }
  nSrcPitch[0] = pixelsize * nBlkSizeX;
  nSrcPitch[1] = pixelsize * nBlkSizeX / xRatioUV; // PF xRatio instead of /2: after 2.7.0.22c;
  nSrcPitch[2] = pixelsize * nBlkSizeX / xRatioUV;
  for (int i = 0; i < 3; i++) {
    nSrcPitch[i] = AlignNumber(nSrcPitch[i], ALIGN_SOURCEBLOCK); // e.g. align reference block pitch to mod16 e.g. at blksize 24
  }
#else	// ALIGN_SOURCEBLOCK
  nSrcPitch[0] = pSrcFrame->GetPlane(YPLANE)->GetPitch();
  if (chroma)
  {
    nSrcPitch[1] = pSrcFrame->GetPlane(UPLANE)->GetPitch();
    nSrcPitch[2] = pSrcFrame->GetPlane(VPLANE)->GetPitch();
  }
#endif	// ALIGN_SOURCEBLOCK
  nRefPitch[0] = pRefFrame->GetPlane(YPLANE)->GetPitch();
  if (chroma)
  {
    nRefPitch[1] = pRefFrame->GetPlane(UPLANE)->GetPitch();
    nRefPitch[2] = pRefFrame->GetPlane(VPLANE)->GetPitch();
  }

  searchType = st;		// ( nLogScale == 0 ) ? st : EXHAUSTIVE;
  nSearchParam = stp;	// *nPel;	// v1.8.2 - redesigned in v1.8.5

  _lambda_level = lambda / (nPel * nPel);
  if (plevel == 1)
  {
    _lambda_level *= nScale;	// scale lambda - Fizick
  }
  else if (plevel == 2)
  {
    _lambda_level *= nScale*nScale;
  }

  temporal = (vecPrev != 0);
  if (vecPrev)
  {
    vecPrev += 1; // Just skips the header
  }

  penaltyZero = _pzero;
  pglobal = _pglobal;
  badcount = 0;
  tryMany = _tryMany;
  planeSAD = 0;
  sumLumaChange = 0;

  _out = out;
  _outfilebuf = outfilebuf;
  _vecPrev = vecPrev;
  _meander_flag = meander;
  _pnew = pnew;
  _lsad = lsad;

  // -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -

  Slicer			slicer(_mt_flag);
  if(bits_per_pixel == 8)
    slicer.start(nBlkY, *this, &PlaneOfBlocks::search_mv_slice<uint8_t>, 4);
  else
    slicer.start(nBlkY, *this, &PlaneOfBlocks::search_mv_slice<uint16_t>, 4);
  slicer.wait();

  // -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -

  if (smallestPlane)
  {
    *pmeanLumaChange = (sad_t)(sumLumaChange / nBlkCount); // for all finer planes
  }

}



void PlaneOfBlocks::RecalculateMVs(
  MVClip & mvClip, MVFrame *_pSrcFrame, MVFrame *_pRefFrame,
  SearchType st, int stp, int lambda, sad_t lsad, int pnew,
  int flags, int *out,
  short *outfilebuf, int fieldShift, sad_t thSAD, int divideExtra, int smooth, bool meander
)
{
  // -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
  // Frame- and plane-related data preparation

  zeroMVfieldShifted.x = 0;
  zeroMVfieldShifted.y = fieldShift;
  zeroMVfieldShifted.sad = 0; // vs
#ifdef ALLOW_DCT
  dctweight16 = 8;//min(16,abs(*pmeanLumaChange)/(nBlkSizeX*nBlkSizeY)); //equal dct and spatial weights for meanLumaChange=8 (empirical)
#endif	// ALLOW_DCT

  // Actually the global predictor is not used in RecalculateMVs().
  _glob_mv_pred_def.x = 0;
  _glob_mv_pred_def.y = fieldShift;
  _glob_mv_pred_def.sad = 9999999; // P.F. will be good for floats, too

  //	int nOutPitchY = nBlkX * (nBlkSizeX - nOverlapX) + nOverlapX;
  //	int nOutPitchUV = (nBlkX * (nBlkSizeX - nOverlapX) + nOverlapX) / 2; // xRatioUV=2
  //	char debugbuf[128];
  //	wsprintf(debugbuf,"MVCOMP1: nOutPitchUV=%d, nOverlap=%d, nBlkX=%d, nBlkSize=%d",nOutPitchUV, nOverlap, nBlkX, nBlkSize);
  //	OutputDebugString(debugbuf);

    // write the plane's header
  WriteHeaderToArray(out);

  nFlags |= flags;

  pSrcFrame = _pSrcFrame;
  pRefFrame = _pRefFrame;

#if (ALIGN_SOURCEBLOCK > 1)
  nSrcPitch_plane[0] = pSrcFrame->GetPlane(YPLANE)->GetPitch();
  if (chroma)
  {
    nSrcPitch_plane[1] = pSrcFrame->GetPlane(UPLANE)->GetPitch();
    nSrcPitch_plane[2] = pSrcFrame->GetPlane(VPLANE)->GetPitch();
  }
  nSrcPitch[0] = pixelsize * nBlkSizeX;
  nSrcPitch[1] = pixelsize * nBlkSizeX / xRatioUV; // PF after 2.7.0.22c
  nSrcPitch[2] = pixelsize * nBlkSizeX / xRatioUV; // PF after 2.7.0.22c
  for (int i = 0; i < 3; i++) {
      nSrcPitch[i] = AlignNumber(nSrcPitch[i], ALIGN_SOURCEBLOCK); // e.g. align reference block pitch to mod16 e.g. at blksize 24
  }
#else	// ALIGN_SOURCEBLOCK
  nSrcPitch[0] = pSrcFrame->GetPlane(YPLANE)->GetPitch();
  if (chroma)
  {
    nSrcPitch[1] = pSrcFrame->GetPlane(UPLANE)->GetPitch();
    nSrcPitch[2] = pSrcFrame->GetPlane(VPLANE)->GetPitch();
  }
#endif	// ALIGN_SOURCEBLOCK
  nRefPitch[0] = pRefFrame->GetPlane(YPLANE)->GetPitch();
  if (chroma)
  {
    nRefPitch[1] = pRefFrame->GetPlane(UPLANE)->GetPitch();
    nRefPitch[2] = pRefFrame->GetPlane(VPLANE)->GetPitch();
  }

  searchType = st;
  nSearchParam = stp;//*nPel; // v1.8.2 - redesigned in v1.8.5

  _lambda_level = lambda / (nPel * nPel);
  //	if (plevel==1)
  //	{
  //		_lambda_level *= nScale;// scale lambda - Fizick
  //	}
  //	else if (plevel==2)
  //	{
  //		_lambda_level *= nScale*nScale;
  //	}

  planeSAD = 0;
  sumLumaChange = 0;

  _out = out;
  _outfilebuf = outfilebuf;
  _meander_flag = meander;
  _pnew = pnew;
  _lsad = lsad;
  _mv_clip_ptr = &mvClip;
  _smooth = smooth;
  _thSAD = thSAD;

  // -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -

  Slicer			slicer(_mt_flag);
  if(pixelsize==1)
    slicer.start(nBlkY, *this, &PlaneOfBlocks::recalculate_mv_slice<uint8_t>, 4);
  else
    slicer.start(nBlkY, *this, &PlaneOfBlocks::recalculate_mv_slice<uint16_t>, 4);
  slicer.wait();

  // -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
}




template<typename pixel_t>
void PlaneOfBlocks::InterpolatePrediction(const PlaneOfBlocks &pob)
{
  int normFactor = 3 - nLogPel + pob.nLogPel;
  int mulFactor = (normFactor < 0) ? -normFactor : 0;
  normFactor = (normFactor < 0) ? 0 : normFactor;
  int normov = (nBlkSizeX - nOverlapX)*(nBlkSizeY - nOverlapY);
  int aoddx = (nBlkSizeX * 3 - nOverlapX * 2);
  int aevenx = (nBlkSizeX * 3 - nOverlapX * 4);
  int aoddy = (nBlkSizeY * 3 - nOverlapY * 2);
  int aeveny = (nBlkSizeY * 3 - nOverlapY * 4);
  // note: overlapping is still (v2.5.7) not processed properly
  // PF todo make faster

  // 2.7.19.22 max safe: BlkX*BlkY: sqrt(2147483647 / 3 / 255) = 1675 ,(2147483647 = 0x7FFFFFFF)
  bool bNoOverlap = (nOverlapX == 0 && nOverlapY == 0);
  bool isSafeBlkSizeFor8bits = (nBlkSizeX*nBlkSizeY) < 1675;
  bool bSmallOverlap = nOverlapX <= (nBlkSizeX >> 1) && nOverlapY <= (nBlkSizeY >> 1);

  for (int l = 0, index = 0; l < nBlkY; l++)
  {
    for (int k = 0; k < nBlkX; k++, index++)
    {
      VECTOR v1, v2, v3, v4;
      int i = k;
      int j = l;
      if (i >= 2 * pob.nBlkX)
      {
        i = 2 * pob.nBlkX - 1;
      }
      if (j >= 2 * pob.nBlkY)
      {
        j = 2 * pob.nBlkY - 1;
      }
      int offy = -1 + 2 * (j % 2);
      int offx = -1 + 2 * (i % 2);
      int iper2 = i / 2;
      int jper2 = j / 2;

      if ((i == 0) || (i >= 2 * pob.nBlkX - 1))
      {
        if ((j == 0) || (j >= 2 * pob.nBlkY - 1))
        {
          v1 = v2 = v3 = v4 = pob.vectors[iper2 + (jper2) * pob.nBlkX];
        }
        else
        {
          v1 = v2 = pob.vectors[iper2 + (jper2) * pob.nBlkX];
          v3 = v4 = pob.vectors[iper2 + (jper2 + offy) * pob.nBlkX];
        }
      }
      else if ((j == 0) || (j >= 2 * pob.nBlkY - 1))
      {
        v1 = v2 = pob.vectors[iper2 + (jper2) * pob.nBlkX];
        v3 = v4 = pob.vectors[iper2 + offx + (jper2) * pob.nBlkX];
      }
      else
      {
        v1 = pob.vectors[iper2 + (jper2) * pob.nBlkX];
        v2 = pob.vectors[iper2 + offx + (jper2) * pob.nBlkX];
        v3 = pob.vectors[iper2 + (jper2 + offy) * pob.nBlkX];
        v4 = pob.vectors[iper2 + offx + (jper2 + offy) * pob.nBlkX];
      }
      typedef typename std::conditional < sizeof(pixel_t) == 1, sad_t, bigsad_t >::type safe_sad_t;
      safe_sad_t tmp_sad; // 16 bit worst case: 16 * sad_max: 16 * 3x32x32x65536 = 4+5+5+16 > 2^31 over limit
      // in case of BlockSize > 32, e.g. 128x128x65536 is even more: 7+7+16=30 bits

      if (bNoOverlap)
      {
        vectors[index].x = 9 * v1.x + 3 * v2.x + 3 * v3.x + v4.x;
        vectors[index].y = 9 * v1.y + 3 * v2.y + 3 * v3.y + v4.y;
        tmp_sad = 9 * (safe_sad_t)v1.sad + 3 * (safe_sad_t)v2.sad + 3 * (safe_sad_t)v3.sad + (safe_sad_t)v4.sad + 8;
      
      }
      else if (bSmallOverlap) // corrected in v1.4.11
      {
        int	ax1 = (offx > 0) ? aoddx : aevenx;
        int ax2 = (nBlkSizeX - nOverlapX) * 4 - ax1;
        int ay1 = (offy > 0) ? aoddy : aeveny;
        int ay2 = (nBlkSizeY - nOverlapY) * 4 - ay1;
        int a11 = ax1*ay1, a12 = ax1*ay2, a21 = ax2*ay1, a22 = ax2*ay2;
        vectors[index].x = (a11*v1.x + a21*v2.x + a12*v3.x + a22*v4.x) / normov;
        vectors[index].y = (a11*v1.y + a21*v2.y + a12*v3.y + a22*v4.y) / normov;
        if (isSafeBlkSizeFor8bits && sizeof(pixel_t)==1) {
          // old max blkSize==32 worst case: 
          //   normov = (32-2)*(32-2) 
          //   sad = 32x32x255 *3 (3 planes) // 705,024,000 < 2^31 OK
          // blkSize == 48 worst case:
          //   normov = (48-2)*(48-2) = 2116
          //   sad = 48x48x255 * 3 // 3,729,576,960 not OK, already fails in 8 bits
          // max safe: BlkX*BlkY: sqrt(0x7FFFFFF / 3 / 255) = 1675
          tmp_sad = ((safe_sad_t)a11*v1.sad + (safe_sad_t)a21*v2.sad + (safe_sad_t)a12*v3.sad + (safe_sad_t)a22*v4.sad) / normov;
        }
        else {
          // safe multiplication
          tmp_sad = ((bigsad_t)a11*v1.sad + (bigsad_t)a21*v2.sad + (bigsad_t)a12*v3.sad + (bigsad_t)a22*v4.sad) / normov;
        }
      }
      else // large overlap. Weights are not quite correct but let it be
      {
        vectors[index].x = (v1.x + v2.x + v3.x + v4.x) << 2;
        vectors[index].y = (v1.y + v2.y + v3.y + v4.y) << 2;
        tmp_sad = ((safe_sad_t)v1.sad + v2.sad + v3.sad + v4.sad + 2) << 2;
      }
      vectors[index].x = (vectors[index].x >> normFactor) << mulFactor;
      vectors[index].y = (vectors[index].y >> normFactor) << mulFactor;
      vectors[index].sad = (sad_t)(tmp_sad >> 4);
    }	// for k < nBlkX
  }	// for l < nBlkY
}

// instantiate
template void PlaneOfBlocks::InterpolatePrediction<uint8_t>(const PlaneOfBlocks &pob);
template void PlaneOfBlocks::InterpolatePrediction<uint16_t>(const PlaneOfBlocks &pob);


void PlaneOfBlocks::WriteHeaderToArray(int *array)
{
  array[0] = nBlkCount * N_PER_BLOCK + 1;
}



int PlaneOfBlocks::WriteDefaultToArray(int *array, int divideMode)
{
  array[0] = nBlkCount * N_PER_BLOCK + 1;
  //	int verybigSAD = nBlkSizeX*nBlkSizeY*256*  bits_per_pixel_factor;
  for (int i = 0; i < nBlkCount*N_PER_BLOCK; i += N_PER_BLOCK)
  {
    array[i + 1] = 0;
    array[i + 2] = 0;
    array[i + 3] = verybigSAD; // float or int!!
    //*(sad_t *)(&array[i + 3]) = verybigSAD; // float or int!!
  }

  if (nLogScale == 0)
  {
    array += array[0];
    if (divideMode)
    {
      // reserve space for divided subblocks extra level
      array[0] = nBlkCount * N_PER_BLOCK * 4 + 1; // 4 subblocks
      for (int i = 0; i < nBlkCount * 4 * N_PER_BLOCK; i += N_PER_BLOCK)
      {
        array[i + 1] = 0;
        array[i + 2] = 0;
        array[i + 3] = verybigSAD; // float or int!!
        //*(sad_t *)(&array[i + 3]) = verybigSAD; // float or int
      }
      array += array[0];
    }
  }
  return GetArraySize(divideMode);
}



int PlaneOfBlocks::GetArraySize(int divideMode)
{
  int size = 0;
  size += 1;              // mb data size storage
  size += nBlkCount * N_PER_BLOCK;  // vectors, sad, luma src, luma ref, var

  if (nLogScale == 0)
  {
    if (divideMode)
    {
      size += 1 + nBlkCount * N_PER_BLOCK * 4; // reserve space for divided subblocks extra level
    }
  }

  return size;
}




template<typename pixel_t>
void PlaneOfBlocks::FetchPredictors(WorkingArea &workarea)
{
  // Left (or right) predictor
  if ((workarea.blkScanDir == 1 && workarea.blkx > 0) || (workarea.blkScanDir == -1 && workarea.blkx < nBlkX - 1))
  {
    workarea.predictors[1] = ClipMV(workarea, vectors[workarea.blkIdx - workarea.blkScanDir]);
  }
  else
  {
    workarea.predictors[1] = ClipMV(workarea, zeroMVfieldShifted); // v1.11.1 - values instead of pointer
  }

  // Up predictor
  if (workarea.blky > workarea.blky_beg)
  {
    workarea.predictors[2] = ClipMV(workarea, vectors[workarea.blkIdx - nBlkX]);
  }
  else
  {
    workarea.predictors[2] = ClipMV(workarea, zeroMVfieldShifted);
  }

  // bottom-right pridictor (from coarse level)
  if ((workarea.blky < workarea.blky_end - 1) && ((workarea.blkScanDir == 1 && workarea.blkx < nBlkX - 1) || (workarea.blkScanDir == -1 && workarea.blkx > 0)))
  {
    workarea.predictors[3] = ClipMV(workarea, vectors[workarea.blkIdx + nBlkX + workarea.blkScanDir]);
  }
  // Up-right predictor
  else if ((workarea.blky > workarea.blky_beg) && ((workarea.blkScanDir == 1 && workarea.blkx < nBlkX - 1) || (workarea.blkScanDir == -1 && workarea.blkx > 0)))
  {
    workarea.predictors[3] = ClipMV(workarea, vectors[workarea.blkIdx - nBlkX + workarea.blkScanDir]);
  }
  else
  {
    workarea.predictors[3] = ClipMV(workarea, zeroMVfieldShifted);
  }

  // Median predictor
  if (workarea.blky > workarea.blky_beg) // replaced 1 by 0 - Fizick
  {
    workarea.predictors[0].x = Median(workarea.predictors[1].x, workarea.predictors[2].x, workarea.predictors[3].x);
    workarea.predictors[0].y = Median(workarea.predictors[1].y, workarea.predictors[2].y, workarea.predictors[3].y);
    //		workarea.predictors[0].sad = Median(workarea.predictors[1].sad, workarea.predictors[2].sad, workarea.predictors[3].sad);
        // but it is not true median vector (x and y may be mixed) and not its sad ?!
        // we really do not know SAD, here is more safe estimation especially for phaseshift method - v1.6.0
    workarea.predictors[0].sad = std::max(workarea.predictors[1].sad, std::max(workarea.predictors[2].sad, workarea.predictors[3].sad));
  }
  else
  {
    //		workarea.predictors[0].x = (workarea.predictors[1].x + workarea.predictors[2].x + workarea.predictors[3].x);
    //		workarea.predictors[0].y = (workarea.predictors[1].y + workarea.predictors[2].y + workarea.predictors[3].y);
    //		workarea.predictors[0].sad = (workarea.predictors[1].sad + workarea.predictors[2].sad + workarea.predictors[3].sad);
        // but for top line we have only left workarea.predictor[1] - v1.6.0
    workarea.predictors[0].x = workarea.predictors[1].x;
    workarea.predictors[0].y = workarea.predictors[1].y;
    workarea.predictors[0].sad = workarea.predictors[1].sad;
  }

  // if there are no other planes, predictor is the median
  if (smallestPlane)
  {
    workarea.predictor = workarea.predictors[0];
  }
  /*
    else
    {
      if ( workarea.predictors[0].sad < workarea.predictor.sad )// disabled by Fizick (hierarchy only!)
      {
        workarea.predictors[4] = workarea.predictor;
        workarea.predictor = workarea.predictors[0];
        workarea.predictors[0] = workarea.predictors[4];
      }
    }
  */
  //	if ( workarea.predictor.sad > LSAD ) { workarea.nLambda = 0; } // generalized (was LSAD=400) by Fizick
  typedef typename std::conditional < sizeof(pixel_t) == 1, sad_t, bigsad_t >::type safe_sad_t;
  workarea.nLambda = workarea.nLambda*(safe_sad_t)LSAD / ((safe_sad_t)LSAD + (workarea.predictor.sad >> 1))*LSAD / ((safe_sad_t)LSAD + (workarea.predictor.sad >> 1));
  // replaced hard threshold by soft in v1.10.2 by Fizick (a liitle complex expression to avoid overflow)
  //	int a = LSAD/(LSAD + (workarea.predictor.sad>>1));
  //	workarea.nLambda = workarea.nLambda*a*a;
}



template<typename pixel_t>
void PlaneOfBlocks::Refine(WorkingArea &workarea)
{
  // then, we refine, according to the search type
  switch (searchType) {
  case ONETIME:
    for (int i = nSearchParam; i > 0; i /= 2)
    {
      OneTimeSearch<pixel_t>(workarea, i);
    }
    break;
  case NSTEP:
    NStepSearch<pixel_t>(workarea, nSearchParam);
    break;
  case LOGARITHMIC:
    for (int i = nSearchParam; i > 0; i /= 2)
    {
      DiamondSearch<pixel_t>(workarea, i);
    }
    break;
  case EXHAUSTIVE: {
    //		ExhaustiveSearch(nSearchParam);
    int mvx = workarea.bestMV.x;
    int mvy = workarea.bestMV.y;
    for (int i = 1; i <= nSearchParam; i++)// region is same as exhaustive, but ordered by radius (from near to far)
    {
      ExpandingSearch<pixel_t>(workarea, i, 1, mvx, mvy);
    }
  }
                   break;

                   //	if ( searchType & SQUARE )
                   //	{
                   //		SquareSearch();
                   //	}
  case HEX2SEARCH:
    Hex2Search<pixel_t>(workarea, nSearchParam);
    break;
  case UMHSEARCH:
    UMHSearch<pixel_t>(workarea, nSearchParam, workarea.bestMV.x, workarea.bestMV.y);
    break;
  case HSEARCH:
  {
    int mvx = workarea.bestMV.x;
    int mvy = workarea.bestMV.y;
    for (int i = 1; i <= nSearchParam; i++)// region is same as exhaustive, but ordered by radius (from near to far)
    {
      CheckMV<pixel_t>(workarea, mvx - i, mvy);
      CheckMV<pixel_t>(workarea, mvx + i, mvy);
    }
  }
  break;
  case VSEARCH:
  {
    int mvx = workarea.bestMV.x;
    int mvy = workarea.bestMV.y;
    for (int i = 1; i <= nSearchParam; i++)// region is same as exhaustive, but ordered by radius (from near to far)
    {
      CheckMV<pixel_t>(workarea, mvx, mvy - i);
      CheckMV<pixel_t>(workarea, mvx, mvy + i);
    }
  }
  break;
  }
}



template<typename pixel_t>
void PlaneOfBlocks::PseudoEPZSearch(WorkingArea &workarea)
{
  typedef typename std::conditional < sizeof(pixel_t) == 1, sad_t, bigsad_t >::type safe_sad_t;
  FetchPredictors<pixel_t>(workarea);

  sad_t sad;
  sad_t saduv;
#ifdef ALLOW_DCT
  if (dctmode != 0) // DCT method (luma only - currently use normal spatial SAD chroma)
  {
    // make dct of source block
    if (dctmode <= 4) //don't do the slow dct conversion if SATD used
    {
      workarea.DCT->DCTBytes2D(workarea.pSrc[0], nSrcPitch[0], &workarea.dctSrc[0], dctpitch);
      // later, workarea.dctSrc is used as a reference block
    }
  }
  if (dctmode >= 3) // most use it and it should be fast anyway //if (dctmode == 3 || dctmode == 4) // check it
  {
    workarea.srcLuma = LUMA(workarea.pSrc[0], nSrcPitch[0]);
  }
#endif	// ALLOW_DCT

  // We treat zero alone
  // Do we bias zero with not taking into account distorsion ?
  workarea.bestMV.x = zeroMVfieldShifted.x;
  workarea.bestMV.y = zeroMVfieldShifted.y;
  saduv = (chroma) ? 
    ScaleSadChroma(SADCHROMA(workarea.pSrc[1], nSrcPitch[1], GetRefBlockU(workarea, 0, 0), nRefPitch[1])
    + SADCHROMA(workarea.pSrc[2], nSrcPitch[2], GetRefBlockV(workarea, 0, 0), nRefPitch[2]), effective_chromaSADscale) : 0;
  sad = LumaSAD<pixel_t>(workarea, GetRefBlock(workarea, 0, zeroMVfieldShifted.y));
  sad += saduv;
  workarea.bestMV.sad = sad;
  workarea.nMinCost = sad + ((penaltyZero*(safe_sad_t)sad) >> 8); // v.1.11.0.2

  VECTOR bestMVMany[8];
  int nMinCostMany[8];

  if (tryMany)
  {
    //  refine around zero
    Refine<pixel_t>(workarea);
    bestMVMany[0] = workarea.bestMV;    // save bestMV
    nMinCostMany[0] = workarea.nMinCost;
  }

  // Global MV predictor  - added by Fizick
  workarea.globalMVPredictor = ClipMV(workarea, workarea.globalMVPredictor);
  //	if ( workarea.IsVectorOK(workarea.globalMVPredictor.x, workarea.globalMVPredictor.y ) )
  {
    saduv = (chroma) ? 
      ScaleSadChroma(SADCHROMA(workarea.pSrc[1], nSrcPitch[1], GetRefBlockU(workarea, workarea.globalMVPredictor.x, workarea.globalMVPredictor.y), nRefPitch[1])
      + SADCHROMA(workarea.pSrc[2], nSrcPitch[2], GetRefBlockV(workarea, workarea.globalMVPredictor.x, workarea.globalMVPredictor.y), nRefPitch[2]), effective_chromaSADscale) : 0;
    sad = LumaSAD<pixel_t>(workarea, GetRefBlock(workarea, workarea.globalMVPredictor.x, workarea.globalMVPredictor.y));
    sad += saduv;
    sad_t cost = sad + ((pglobal*(safe_sad_t)sad) >> 8);

    if (cost < workarea.nMinCost || tryMany)
    {
      workarea.bestMV.x = workarea.globalMVPredictor.x;
      workarea.bestMV.y = workarea.globalMVPredictor.y;
      workarea.bestMV.sad = sad;
      workarea.nMinCost = cost;
    }
    if (tryMany)
    {
      // refine around global
      Refine<pixel_t>(workarea);    // reset bestMV
      bestMVMany[1] = workarea.bestMV;    // save bestMV
      nMinCostMany[1] = workarea.nMinCost;
    }
    //	}
    //	Then, the predictor :
    //	if (   (( workarea.predictor.x != zeroMVfieldShifted.x ) || ( workarea.predictor.y != zeroMVfieldShifted.y ))
    //	    && (( workarea.predictor.x != workarea.globalMVPredictor.x ) || ( workarea.predictor.y != workarea.globalMVPredictor.y )))
    //	{
    saduv = (chroma) ? ScaleSadChroma(SADCHROMA(workarea.pSrc[1], nSrcPitch[1], GetRefBlockU(workarea, workarea.predictor.x, workarea.predictor.y), nRefPitch[1])
      + SADCHROMA(workarea.pSrc[2], nSrcPitch[2], GetRefBlockV(workarea, workarea.predictor.x, workarea.predictor.y), nRefPitch[2]), effective_chromaSADscale) : 0;
    sad = LumaSAD<pixel_t>(workarea, GetRefBlock(workarea, workarea.predictor.x, workarea.predictor.y));
    sad += saduv;
    cost = sad;

    if (cost < workarea.nMinCost || tryMany)
    {
      workarea.bestMV.x = workarea.predictor.x;
      workarea.bestMV.y = workarea.predictor.y;
      workarea.bestMV.sad = sad;
      workarea.nMinCost = cost;
    }
  }

  if (tryMany)
  {
    // refine around predictor
    Refine<pixel_t>(workarea);    // reset bestMV
    bestMVMany[2] = workarea.bestMV;    // save bestMV
    nMinCostMany[2] = workarea.nMinCost;
  }

  // then all the other predictors
  int npred = (temporal) ? 5 : 4;

  for (int i = 0; i < npred; i++)
  {
    if (tryMany)
    {
      workarea.nMinCost = verybigSAD + 1;
    }
    CheckMV0<pixel_t>(workarea, workarea.predictors[i].x, workarea.predictors[i].y);
    if (tryMany)
    {
      // refine around predictor
      Refine<pixel_t>(workarea);    // reset bestMV
      bestMVMany[i + 3] = workarea.bestMV;    // save bestMV
      nMinCostMany[i + 3] = workarea.nMinCost;
    }
  }	// for i

  if (tryMany)
  {
    // select best of multi best
    workarea.nMinCost = verybigSAD + 1;
    for (int i = 0; i < npred + 3; i++)
    {
      if (nMinCostMany[i] < workarea.nMinCost)
      {
        workarea.bestMV = bestMVMany[i];
        workarea.nMinCost = nMinCostMany[i];
      }
    }
  }
  else
  {
    // then, we refine, according to the search type
    Refine<pixel_t>(workarea);
  }

  sad_t foundSAD = workarea.bestMV.sad;

  const int		BADCOUNT_LIMIT = 16;

  // bad vector, try wide search
  if (workarea.blkIdx > 1 + workarea.blky_beg * nBlkX
    && foundSAD > (badSAD + badSAD*badcount / BADCOUNT_LIMIT))
  {
    // with some soft limit (BADCOUNT_LIMIT) of bad cured vectors (time consumed)
    ++badcount;

    DebugPrintf(
      "bad  blk=%d x=%d y=%d sad=%d mean=%d iter=%d",
      workarea.blkIdx,
      workarea.bestMV.x,
      workarea.bestMV.y,
      workarea.bestMV.sad,
      workarea.planeSAD / (workarea.blkIdx - workarea.blky_beg * nBlkX),
      workarea.iter
    );
    /*
        int mvx0 = workarea.bestMV.x; // store for comparing
        int mvy0 = workarea.bestMV.y;
        int msad0 = workarea.bestMV.sad;
        int mcost0 = workarea.nMinCost;
    */
    if (badrange > 0) // UMH
    {

      //			UMHSearch(badrange*nPel, workarea.bestMV.x, workarea.bestMV.y);

      //			if (workarea.bestMV.sad > foundSAD/2)
      {
        // rathe good is not found, lets try around zero
//				UMHSearch(workarea, badSADRadius, abs(mvx0)%4 - 2, abs(mvy0)%4 - 2);
        UMHSearch<pixel_t>(workarea, badrange*nPel, 0, 0);
      }
    }

    else if (badrange < 0) // ESA
    {
      /*
            workarea.bestMV.x = mvx0; // restore  for comparing
            workarea.bestMV.y = mvy0;
            workarea.bestMV.sad = msad0;
            workarea.nMinCost = mcost0;

            int mvx = workarea.bestMV.x; // store to not move the search center!
            int mvy = workarea.bestMV.y;
            int msad = workarea.bestMV.sad;

            for ( int i = 1; i < -badrange*nPel; i+=nPel )// at radius
            {
              ExpandingSearch(i, nPel, mvx, mvy);
              if (workarea.bestMV.sad < foundSAD/4)
              {
                break; // stop search
              }
            }

            if (workarea.bestMV.sad > foundSAD/2 && abs(mvx)+abs(mvy) > badSADRadius/2)
            {
              // rathe good is not found, lets try around zero
              mvx = 0; // store to not move the search center!
              mvy = 0;
      */
      for (int i = 1; i < -badrange*nPel; i += nPel)// at radius
      {
        ExpandingSearch<pixel_t>(workarea, i, nPel, 0, 0);
        if (workarea.bestMV.sad < foundSAD / 4)
        {
          break; // stop search if rathe good is found
        }
      }	// for i
    }	// badrange < 0

    int mvx = workarea.bestMV.x; // refine in small area
    int mvy = workarea.bestMV.y;
    for (int i = 1; i < nPel; i++)// small radius
    {
      ExpandingSearch<pixel_t>(workarea, i, 1, mvx, mvy);
    }
    DebugPrintf("best blk=%d x=%d y=%d sad=%d iter=%d", workarea.blkIdx, workarea.bestMV.x, workarea.bestMV.y, workarea.bestMV.sad, workarea.iter);
  }	// bad vector, try wide search

  // we store the result
  vectors[workarea.blkIdx].x = workarea.bestMV.x;
  vectors[workarea.blkIdx].y = workarea.bestMV.y;
  vectors[workarea.blkIdx].sad = workarea.bestMV.sad;

  workarea.planeSAD += workarea.bestMV.sad; // todo PF check int overflow
}



template<typename pixel_t>
void PlaneOfBlocks::DiamondSearch(WorkingArea &workarea, int length)
{
  // The meaning of the directions are the following :
  //		* 1 means right
  //		* 2 means left
  //		* 4 means down
  //		* 8 means up
  // So 1 + 4 means down right, and so on...

  int dx;
  int dy;

  // We begin by making no assumption on which direction to search.
  int direction = 15;

  int lastDirection;

  while (direction > 0)
  {
    dx = workarea.bestMV.x;
    dy = workarea.bestMV.y;
    lastDirection = direction;
    direction = 0;

    // First, we look the directions that were hinted by the previous step
    // of the algorithm. If we find one, we add it to the set of directions
    // we'll test next
    if (lastDirection & 1) CheckMV2<pixel_t>(workarea, dx + length, dy, &direction, 1);
    if (lastDirection & 2) CheckMV2<pixel_t>(workarea, dx - length, dy, &direction, 2);
    if (lastDirection & 4) CheckMV2<pixel_t>(workarea, dx, dy + length, &direction, 4);
    if (lastDirection & 8) CheckMV2<pixel_t>(workarea, dx, dy - length, &direction, 8);

    // If one of the directions improves the SAD, we make further tests
    // on the diagonals
    if (direction)
    {
      lastDirection = direction;
      dx = workarea.bestMV.x;
      dy = workarea.bestMV.y;

      if (lastDirection & 3)
      {
        CheckMV2<pixel_t>(workarea, dx, dy + length, &direction, 4);
        CheckMV2<pixel_t>(workarea, dx, dy - length, &direction, 8);
      }
      else
      {
        CheckMV2<pixel_t>(workarea, dx + length, dy, &direction, 1);
        CheckMV2<pixel_t>(workarea, dx - length, dy, &direction, 2);
      }
    }

    // If not, we do not stop here. We infer from the last direction the
    // diagonals to be checked, because we might be lucky.
    else
    {
      switch (lastDirection)
      {
      case 1:
        CheckMV2<pixel_t>(workarea, dx + length, dy + length, &direction, 1 + 4);
        CheckMV2<pixel_t>(workarea, dx + length, dy - length, &direction, 1 + 8);
        break;
      case 2:
        CheckMV2<pixel_t>(workarea, dx - length, dy + length, &direction, 2 + 4);
        CheckMV2<pixel_t>(workarea, dx - length, dy - length, &direction, 2 + 8);
        break;
      case 4:
        CheckMV2<pixel_t>(workarea, dx + length, dy + length, &direction, 1 + 4);
        CheckMV2<pixel_t>(workarea, dx - length, dy + length, &direction, 2 + 4);
        break;
      case 8:
        CheckMV2<pixel_t>(workarea, dx + length, dy - length, &direction, 1 + 8);
        CheckMV2<pixel_t>(workarea, dx - length, dy - length, &direction, 2 + 8);
        break;
      case 1 + 4:
        CheckMV2<pixel_t>(workarea, dx + length, dy + length, &direction, 1 + 4);
        CheckMV2<pixel_t>(workarea, dx - length, dy + length, &direction, 2 + 4);
        CheckMV2<pixel_t>(workarea, dx + length, dy - length, &direction, 1 + 8);
        break;
      case 2 + 4:
        CheckMV2<pixel_t>(workarea, dx + length, dy + length, &direction, 1 + 4);
        CheckMV2<pixel_t>(workarea, dx - length, dy + length, &direction, 2 + 4);
        CheckMV2<pixel_t>(workarea, dx - length, dy - length, &direction, 2 + 8);
        break;
      case 1 + 8:
        CheckMV2<pixel_t>(workarea, dx + length, dy + length, &direction, 1 + 4);
        CheckMV2<pixel_t>(workarea, dx - length, dy - length, &direction, 2 + 8);
        CheckMV2<pixel_t>(workarea, dx + length, dy - length, &direction, 1 + 8);
        break;
      case 2 + 8:
        CheckMV2<pixel_t>(workarea, dx - length, dy - length, &direction, 2 + 8);
        CheckMV2<pixel_t>(workarea, dx - length, dy + length, &direction, 2 + 4);
        CheckMV2<pixel_t>(workarea, dx + length, dy - length, &direction, 1 + 8);
        break;
      default:
        // Even the default case may happen, in the first step of the
        // algorithm for example.
        CheckMV2<pixel_t>(workarea, dx + length, dy + length, &direction, 1 + 4);
        CheckMV2<pixel_t>(workarea, dx - length, dy + length, &direction, 2 + 4);
        CheckMV2<pixel_t>(workarea, dx + length, dy - length, &direction, 1 + 8);
        CheckMV2<pixel_t>(workarea, dx - length, dy - length, &direction, 2 + 8);
        break;
      }
    }	// if ! direction
  }	// while direction > 0
}



/*
void PlaneOfBlocks::SquareSearch(WorkingArea &workarea)
{
  ExhaustiveSearch(workarea, 1);
}



void PlaneOfBlocks::ExhaustiveSearch(WorkingArea &workarea, int s)// diameter = 2*s - 1
{
  int i, j;
  VECTOR mv = workarea.bestMV;

  for ( i = -s + 1; i < 0; i++ )
    for ( j = -s + 1; j < s; j++ )
      CheckMV(workarea, mv.x + i, mv.y + j);

  for ( i = 1; i < s; i++ )
    for ( j = -s + 1; j < s; j++ )
      CheckMV(workarea, mv.x + i, mv.y + j);

  for ( j = -s + 1; j < 0; j++ )
    CheckMV(workarea, mv.x, mv.y + j);

  for ( j = 1; j < s; j++ )
    CheckMV(workarea, mv.x, mv.y + j);

}
*/



template<typename pixel_t>
void PlaneOfBlocks::NStepSearch(WorkingArea &workarea, int stp)
{
  int dx, dy;
  int length = stp;
  while (length > 0)
  {
    dx = workarea.bestMV.x;
    dy = workarea.bestMV.y;

    CheckMV<pixel_t>(workarea, dx + length, dy + length);
    CheckMV<pixel_t>(workarea, dx + length, dy);
    CheckMV<pixel_t>(workarea, dx + length, dy - length);
    CheckMV<pixel_t>(workarea, dx, dy - length);
    CheckMV<pixel_t>(workarea, dx, dy + length);
    CheckMV<pixel_t>(workarea, dx - length, dy + length);
    CheckMV<pixel_t>(workarea, dx - length, dy);
    CheckMV<pixel_t>(workarea, dx - length, dy - length);

    length--;
  }
}




template<typename pixel_t>
void PlaneOfBlocks::OneTimeSearch(WorkingArea &workarea, int length)
{
  int direction = 0;
  int dx = workarea.bestMV.x;
  int dy = workarea.bestMV.y;

  CheckMV2<pixel_t>(workarea, dx - length, dy, &direction, 2);
  CheckMV2<pixel_t>(workarea, dx + length, dy, &direction, 1);

  if (direction == 1)
  {
    while (direction)
    {
      direction = 0;
      dx += length;
      CheckMV2<pixel_t>(workarea, dx + length, dy, &direction, 1);
    }
  }
  else if (direction == 2)
  {
    while (direction)
    {
      direction = 0;
      dx -= length;
      CheckMV2<pixel_t>(workarea, dx - length, dy, &direction, 1);
    }
  }

  CheckMV2<pixel_t>(workarea, dx, dy - length, &direction, 2);
  CheckMV2<pixel_t>(workarea, dx, dy + length, &direction, 1);

  if (direction == 1)
  {
    while (direction)
    {
      direction = 0;
      dy += length;
      CheckMV2<pixel_t>(workarea, dx, dy + length, &direction, 1);
    }
  }
  else if (direction == 2)
  {
    while (direction)
    {
      direction = 0;
      dy -= length;
      CheckMV2<pixel_t>(workarea, dx, dy - length, &direction, 1);
    }
  }
}



template<typename pixel_t>
void PlaneOfBlocks::ExpandingSearch(WorkingArea &workarea, int r, int s, int mvx, int mvy) // diameter = 2*r + 1, step=s
{ // part of true enhaustive search (thin expanding square) around mvx, mvy
  int i, j;
  //	VECTOR mv = workarea.bestMV; // bug: it was pointer assignent, not values, so iterative! - v2.1

    // sides of square without corners
  for (i = -r + s; i < r; i += s) // without corners! - v2.1
  {
    CheckMV<pixel_t>(workarea, mvx + i, mvy - r);
    CheckMV<pixel_t>(workarea, mvx + i, mvy + r);
  }

  for (j = -r + s; j < r; j += s)
  {
    CheckMV<pixel_t>(workarea, mvx - r, mvy + j);
    CheckMV<pixel_t>(workarea, mvx + r, mvy + j);
  }

  // then corners - they are more far from cenrer
  CheckMV<pixel_t>(workarea, mvx - r, mvy - r);
  CheckMV<pixel_t>(workarea, mvx - r, mvy + r);
  CheckMV<pixel_t>(workarea, mvx + r, mvy - r);
  CheckMV<pixel_t>(workarea, mvx + r, mvy + r);
}



/* (x-1)%6 */
static const int mod6m1[8] = { 5,0,1,2,3,4,5,0 };
/* radius 2 hexagon. repeated entries are to avoid having to compute mod6 every time. */
static const int hex2[8][2] = { {-1,-2}, {-2,0}, {-1,2}, {1,2}, {2,0}, {1,-2}, {-1,-2}, {-2,0} };

template<typename pixel_t>
void PlaneOfBlocks::Hex2Search(WorkingArea &workarea, int i_me_range)
{
  // adopted from x264
  int dir = -2;
  int bmx = workarea.bestMV.x;
  int bmy = workarea.bestMV.y;

  if (i_me_range > 1)
  {
    /* hexagon */
//		COST_MV_X3_DIR( -2,0, -1, 2,  1, 2, costs   );
//		COST_MV_X3_DIR(  2,0,  1,-2, -1,-2, costs+3 );
//		COPY2_IF_LT( bcost, costs[0], dir, 0 );
//		COPY2_IF_LT( bcost, costs[1], dir, 1 );
//		COPY2_IF_LT( bcost, costs[2], dir, 2 );
//		COPY2_IF_LT( bcost, costs[3], dir, 3 );
//		COPY2_IF_LT( bcost, costs[4], dir, 4 );
//		COPY2_IF_LT( bcost, costs[5], dir, 5 );
    CheckMVdir<pixel_t>(workarea, bmx - 2, bmy, &dir, 0);
    CheckMVdir<pixel_t>(workarea, bmx - 1, bmy + 2, &dir, 1);
    CheckMVdir<pixel_t>(workarea, bmx + 1, bmy + 2, &dir, 2);
    CheckMVdir<pixel_t>(workarea, bmx + 2, bmy, &dir, 3);
    CheckMVdir<pixel_t>(workarea, bmx + 1, bmy - 2, &dir, 4);
    CheckMVdir<pixel_t>(workarea, bmx - 1, bmy - 2, &dir, 5);


    if (dir != -2)
    {
      bmx += hex2[dir + 1][0];
      bmy += hex2[dir + 1][1];
      /* half hexagon, not overlapping the previous iteration */
      for (int i = 1; i < i_me_range / 2 && workarea.IsVectorOK(bmx, bmy); i++)
      {
        const int odir = mod6m1[dir + 1];
        //				COST_MV_X3_DIR (hex2[odir+0][0], hex2[odir+0][1],
        //				                hex2[odir+1][0], hex2[odir+1][1],
        //				                hex2[odir+2][0], hex2[odir+2][1],
        //				                costs);

        dir = -2;
        //				COPY2_IF_LT( bcost, costs[0], dir, odir-1 );
        //				COPY2_IF_LT( bcost, costs[1], dir, odir   );
        //				COPY2_IF_LT( bcost, costs[2], dir, odir+1 );

        CheckMVdir<pixel_t>(workarea, bmx + hex2[odir + 0][0], bmy + hex2[odir + 0][1], &dir, odir - 1);
        CheckMVdir<pixel_t>(workarea, bmx + hex2[odir + 1][0], bmy + hex2[odir + 1][1], &dir, odir);
        CheckMVdir<pixel_t>(workarea, bmx + hex2[odir + 2][0], bmy + hex2[odir + 2][1], &dir, odir + 1);
        if (dir == -2)
        {
          break;
        }
        bmx += hex2[dir + 1][0];
        bmy += hex2[dir + 1][1];
      }
    }

    workarea.bestMV.x = bmx;
    workarea.bestMV.y = bmy;
  }

  // square refine
//	omx = bmx; omy = bmy;
//	COST_MV_X4(  0,-1,  0,1, -1,0, 1,0 );
//	COST_MV_X4( -1,-1, -1,1, 1,-1, 1,1 );
  ExpandingSearch<pixel_t>(workarea, 1, 1, bmx, bmy);
}


template<typename pixel_t>
void PlaneOfBlocks::CrossSearch(WorkingArea &workarea, int start, int x_max, int y_max, int mvx, int mvy)
{
  // part of umh  search
  for (int i = start; i < x_max; i += 2)
  {
    CheckMV<pixel_t>(workarea, mvx - i, mvy);
    CheckMV<pixel_t>(workarea, mvx + i, mvy);
  }

  for (int j = start; j < y_max; j += 2)
  {
    CheckMV<pixel_t>(workarea, mvx, mvy + j);
    CheckMV<pixel_t>(workarea, mvx, mvy + j);
  }
}

#if 0 // x265
// this part comes as a sample from the x265 project, to find any similarities
// and study if star search method can be applied.
int MotionEstimate::motionEstimate(ReferencePlanes *ref,
const MV &       mvmin,
const MV &       mvmax,
const MV &       qmvp,
int              numCandidates,
const MV *       mvc,
int              merange,
MV &             outQMv,
uint32_t         maxSlices,
pixel *          srcReferencePlane)
{
ALIGN_VAR_16(int, costs[16]);
if (ctuAddr >= 0)
blockOffset = ref->reconPic->getLumaAddr(ctuAddr, absPartIdx) - ref->reconPic->getLumaAddr(0);
intptr_t stride = ref->lumaStride;
pixel* fenc = fencPUYuv.m_buf[0];
pixel* fref = srcReferencePlane == 0 ? ref->fpelPlane[0] + blockOffset : srcReferencePlane + blockOffset;

setMVP(qmvp);

MV qmvmin = mvmin.toQPel();
MV qmvmax = mvmax.toQPel();

/* The term cost used here means satd/sad values for that particular search.
* The costs used in ME integer search only includes the SAD cost of motion
* residual and sqrtLambda times MVD bits.  The subpel refine steps use SATD
* cost of residual and sqrtLambda * MVD bits.  Mode decision will be based
* on video distortion cost (SSE/PSNR) plus lambda times all signaling bits
* (mode + MVD bits). */

// measure SAD cost at clipped QPEL MVP
MV pmv = qmvp.clipped(qmvmin, qmvmax);
MV bestpre = pmv;
int bprecost;

if (ref->isLowres)
bprecost = ref->lowresQPelCost(fenc, blockOffset, pmv, sad);
else
bprecost = subpelCompare(ref, pmv, sad);

/* re-measure full pel rounded MVP with SAD as search start point */
MV bmv = pmv.roundToFPel();
int bcost = bprecost;
if (pmv.isSubpel())
bcost = sad(fenc, FENC_STRIDE, fref + bmv.x + bmv.y * stride, stride) + mvcost(bmv << 2);

// measure SAD cost at MV(0) if MVP is not zero
if (pmv.notZero())
{
  int cost = sad(fenc, FENC_STRIDE, fref, stride) + mvcost(MV(0, 0));
  if (cost < bcost)
  {
    bcost = cost;
    bmv = 0;
    bmv.y = X265_MAX(X265_MIN(0, mvmax.y), mvmin.y);
  }
}

X265_CHECK(!(ref->isLowres && numCandidates), "lowres motion candidates not allowed\n")
// measure SAD cost at each QPEL motion vector candidate
for (int i = 0; i < numCandidates; i++)
{
  MV m = mvc[i].clipped(qmvmin, qmvmax);
  if (m.notZero() & (m != pmv ? 1 : 0) & (m != bestpre ? 1 : 0)) // check already measured
  {
    int cost = subpelCompare(ref, m, sad) + mvcost(m);
    if (cost < bprecost)
    {
      bprecost = cost;
      bestpre = m;
    }
  }
}

pmv = pmv.roundToFPel();
MV omv = bmv;  // current search origin or starting point

switch (searchMethod)
{
case X265_DIA_SEARCH:
{
  /* diamond search, radius 1 */
  bcost <<= 4;
  int i = merange;
  do
  {
    COST_MV_X4_DIR(0, -1, 0, 1, -1, 0, 1, 0, costs);
    if ((bmv.y - 1 >= mvmin.y) & (bmv.y - 1 <= mvmax.y))
      COPY1_IF_LT(bcost, (costs[0] << 4) + 1);
    if ((bmv.y + 1 >= mvmin.y) & (bmv.y + 1 <= mvmax.y))
      COPY1_IF_LT(bcost, (costs[1] << 4) + 3);
    COPY1_IF_LT(bcost, (costs[2] << 4) + 4);
    COPY1_IF_LT(bcost, (costs[3] << 4) + 12);
    if (!(bcost & 15))
      break;
    bmv.x -= (bcost << 28) >> 30;
    bmv.y -= (bcost << 30) >> 30;
    bcost &= ~15;
  } while (--i && bmv.checkRange(mvmin, mvmax));
  bcost >>= 4;
  break;
}

case X265_HEX_SEARCH:
{
me_hex2:
  /* hexagon search, radius 2 */
#if 0
  for (int i = 0; i < merange / 2; i++)
  {
    omv = bmv;
    COST_MV(omv.x - 2, omv.y);
    COST_MV(omv.x - 1, omv.y + 2);
    COST_MV(omv.x + 1, omv.y + 2);
    COST_MV(omv.x + 2, omv.y);
    COST_MV(omv.x + 1, omv.y - 2);
    COST_MV(omv.x - 1, omv.y - 2);
    if (omv == bmv)
      break;
    if (!bmv.checkRange(mvmin, mvmax))
      break;
  }

#else // if 0
  /* equivalent to the above, but eliminates duplicate candidates */
  COST_MV_X3_DIR(-2, 0, -1, 2, 1, 2, costs);
  bcost <<= 3;
  if ((bmv.y >= mvmin.y) & (bmv.y <= mvmax.y))
    COPY1_IF_LT(bcost, (costs[0] << 3) + 2);
  if ((bmv.y + 2 >= mvmin.y) & (bmv.y + 2 <= mvmax.y))
  {
    COPY1_IF_LT(bcost, (costs[1] << 3) + 3);
    COPY1_IF_LT(bcost, (costs[2] << 3) + 4);
  }

  COST_MV_X3_DIR(2, 0, 1, -2, -1, -2, costs);
  if ((bmv.y >= mvmin.y) & (bmv.y <= mvmax.y))
    COPY1_IF_LT(bcost, (costs[0] << 3) + 5);
  if ((bmv.y - 2 >= mvmin.y) & (bmv.y - 2 <= mvmax.y))
  {
    COPY1_IF_LT(bcost, (costs[1] << 3) + 6);
    COPY1_IF_LT(bcost, (costs[2] << 3) + 7);
  }

  if (bcost & 7)
  {
    int dir = (bcost & 7) - 2;

    if ((bmv.y + hex2[dir + 1].y >= mvmin.y) & (bmv.y + hex2[dir + 1].y <= mvmax.y))
    {
      bmv += hex2[dir + 1];

      /* half hexagon, not overlapping the previous iteration */
      for (int i = (merange >> 1) - 1; i > 0 && bmv.checkRange(mvmin, mvmax); i--)
      {
        COST_MV_X3_DIR(hex2[dir + 0].x, hex2[dir + 0].y,
          hex2[dir + 1].x, hex2[dir + 1].y,
          hex2[dir + 2].x, hex2[dir + 2].y,
          costs);
        bcost &= ~7;

        if ((bmv.y + hex2[dir + 0].y >= mvmin.y) & (bmv.y + hex2[dir + 0].y <= mvmax.y))
          COPY1_IF_LT(bcost, (costs[0] << 3) + 1);

        if ((bmv.y + hex2[dir + 1].y >= mvmin.y) & (bmv.y + hex2[dir + 1].y <= mvmax.y))
          COPY1_IF_LT(bcost, (costs[1] << 3) + 2);

        if ((bmv.y + hex2[dir + 2].y >= mvmin.y) & (bmv.y + hex2[dir + 2].y <= mvmax.y))
          COPY1_IF_LT(bcost, (costs[2] << 3) + 3);

        if (!(bcost & 7))
          break;

        dir += (bcost & 7) - 2;
        dir = mod6m1[dir + 1];
        bmv += hex2[dir + 1];
      }
    } // if ((bmv.y + hex2[dir + 1].y >= mvmin.y) & (bmv.y + hex2[dir + 1].y <= mvmax.y))
  }
  bcost >>= 3;
#endif // if 0

  /* square refine */
  int dir = 0;
  COST_MV_X4_DIR(0, -1, 0, 1, -1, 0, 1, 0, costs);
  if ((bmv.y - 1 >= mvmin.y) & (bmv.y - 1 <= mvmax.y))
    COPY2_IF_LT(bcost, costs[0], dir, 1);
  if ((bmv.y + 1 >= mvmin.y) & (bmv.y + 1 <= mvmax.y))
    COPY2_IF_LT(bcost, costs[1], dir, 2);
  COPY2_IF_LT(bcost, costs[2], dir, 3);
  COPY2_IF_LT(bcost, costs[3], dir, 4);
  COST_MV_X4_DIR(-1, -1, -1, 1, 1, -1, 1, 1, costs);
  if ((bmv.y - 1 >= mvmin.y) & (bmv.y - 1 <= mvmax.y))
    COPY2_IF_LT(bcost, costs[0], dir, 5);
  if ((bmv.y + 1 >= mvmin.y) & (bmv.y + 1 <= mvmax.y))
    COPY2_IF_LT(bcost, costs[1], dir, 6);
  if ((bmv.y - 1 >= mvmin.y) & (bmv.y - 1 <= mvmax.y))
    COPY2_IF_LT(bcost, costs[2], dir, 7);
  if ((bmv.y + 1 >= mvmin.y) & (bmv.y + 1 <= mvmax.y))
    COPY2_IF_LT(bcost, costs[3], dir, 8);
  bmv += square1[dir];
  break;
}

case X265_UMH_SEARCH:
{
  int ucost1, ucost2;
  int16_t cross_start = 1;

  /* refine predictors */
  omv = bmv;
  ucost1 = bcost;
  X265_CHECK(((pmv.y >= mvmin.y) & (pmv.y <= mvmax.y)), "pmv outside of search range!");
  DIA1_ITER(pmv.x, pmv.y);
  if (pmv.notZero())
    DIA1_ITER(0, 0);

  ucost2 = bcost;
  if (bmv.notZero() && bmv != pmv)
    DIA1_ITER(bmv.x, bmv.y);
  if (bcost == ucost2)
    cross_start = 3;

  /* Early Termination */
  omv = bmv;
  if (bcost == ucost2 && SAD_THRESH(2000))
  {
    COST_MV_X4(0, -2, -1, -1, 1, -1, -2, 0);
    COST_MV_X4(2, 0, -1, 1, 1, 1, 0, 2);
    if (bcost == ucost1 && SAD_THRESH(500))
      break;
    if (bcost == ucost2)
    {
      int16_t range = (int16_t)(merange >> 1) | 1;
      CROSS(3, range, range);
      COST_MV_X4(-1, -2, 1, -2, -2, -1, 2, -1);
      COST_MV_X4(-2, 1, 2, 1, -1, 2, 1, 2);
      if (bcost == ucost2)
        break;
      cross_start = range + 2;
    }
  }

  // TODO: Need to study x264's logic for building mvc list to understand why they
  //       have special cases here for 16x16, and whether they apply to HEVC CTU

  // adaptive search range based on mvc variability
  if (numCandidates)
  {
    /* range multipliers based on casual inspection of some statistics of
    * average distance between current predictor and final mv found by ESA.
    * these have not been tuned much by actual encoding. */
    static const uint8_t range_mul[4][4] =
    {
      { 3, 3, 4, 4 },
      { 3, 4, 4, 4 },
      { 4, 4, 4, 5 },
      { 4, 4, 5, 6 },
    };

    int mvd;
    int sad_ctx, mvd_ctx;
    int denom = 1;

    if (numCandidates == 1)
    {
      if (LUMA_64x64 == partEnum)
        /* mvc is probably the same as mvp, so the difference isn't meaningful.
        * but prediction usually isn't too bad, so just use medium range */
        mvd = 25;
      else
        mvd = abs(qmvp.x - mvc[0].x) + abs(qmvp.y - mvc[0].y);
    }
    else
    {
      /* calculate the degree of agreement between predictors. */

      /* in 64x64, mvc includes all the neighbors used to make mvp,
      * so don't count mvp separately. */

      denom = numCandidates - 1;
      mvd = 0;
      if (partEnum != LUMA_64x64)
      {
        mvd = abs(qmvp.x - mvc[0].x) + abs(qmvp.y - mvc[0].y);
        denom++;
      }
      mvd += predictorDifference(mvc, numCandidates);
    }

    sad_ctx = SAD_THRESH(1000) ? 0
      : SAD_THRESH(2000) ? 1
      : SAD_THRESH(4000) ? 2 : 3;
    mvd_ctx = mvd < 10 * denom ? 0
      : mvd < 20 * denom ? 1
      : mvd < 40 * denom ? 2 : 3;

    merange = (merange * range_mul[mvd_ctx][sad_ctx]) >> 2;
  }

  /* FIXME if the above DIA2/OCT2/CROSS found a new mv, it has not updated omx/omy.
  * we are still centered on the same place as the DIA2. is this desirable? */
  CROSS(cross_start, merange, merange >> 1);
  COST_MV_X4(-2, -2, -2, 2, 2, -2, 2, 2);

  /* hexagon grid */
  omv = bmv;
  const uint16_t *p_cost_omvx = m_cost_mvx + omv.x * 4;
  const uint16_t *p_cost_omvy = m_cost_mvy + omv.y * 4;
  uint16_t i = 1;
  do
  {
    if (4 * i > X265_MIN4(mvmax.x - omv.x, omv.x - mvmin.x,
      mvmax.y - omv.y, omv.y - mvmin.y))
    {
      for (int j = 0; j < 16; j++)
      {
        MV mv = omv + (hex4[j] * i);
        if (mv.checkRange(mvmin, mvmax))
          COST_MV(mv.x, mv.y);
      }
    }
    else
    {
      int16_t dir = 0;
      pixel *fref_base = fref + omv.x + (omv.y - 4 * i) * stride;
      size_t dy = (size_t)i * stride;
#define SADS(k, x0, y0, x1, y1, x2, y2, x3, y3) \
    sad_x4(fenc, \
           fref_base x0 * i + (y0 - 2 * k + 4) * dy, \
           fref_base x1 * i + (y1 - 2 * k + 4) * dy, \
           fref_base x2 * i + (y2 - 2 * k + 4) * dy, \
           fref_base x3 * i + (y3 - 2 * k + 4) * dy, \
           stride, costs + 4 * k); \
    fref_base += 2 * dy;
#define ADD_MVCOST(k, x, y) costs[k] += p_cost_omvx[x * 4 * i] + p_cost_omvy[y * 4 * i]
#define MIN_MV(k, dx, dy)     if ((omv.y + (dy) >= mvmin.y) & (omv.y + (dy) <= mvmax.y)) { COPY2_IF_LT(bcost, costs[k], dir, dx * 16 + (dy & 15)) }

      SADS(0, +0, -4, +0, +4, -2, -3, +2, -3);
      SADS(1, -4, -2, +4, -2, -4, -1, +4, -1);
      SADS(2, -4, +0, +4, +0, -4, +1, +4, +1);
      SADS(3, -4, +2, +4, +2, -2, +3, +2, +3);
      ADD_MVCOST(0, 0, -4);
      ADD_MVCOST(1, 0, 4);
      ADD_MVCOST(2, -2, -3);
      ADD_MVCOST(3, 2, -3);
      ADD_MVCOST(4, -4, -2);
      ADD_MVCOST(5, 4, -2);
      ADD_MVCOST(6, -4, -1);
      ADD_MVCOST(7, 4, -1);
      ADD_MVCOST(8, -4, 0);
      ADD_MVCOST(9, 4, 0);
      ADD_MVCOST(10, -4, 1);
      ADD_MVCOST(11, 4, 1);
      ADD_MVCOST(12, -4, 2);
      ADD_MVCOST(13, 4, 2);
      ADD_MVCOST(14, -2, 3);
      ADD_MVCOST(15, 2, 3);
      MIN_MV(0, 0, -4);
      MIN_MV(1, 0, 4);
      MIN_MV(2, -2, -3);
      MIN_MV(3, 2, -3);
      MIN_MV(4, -4, -2);
      MIN_MV(5, 4, -2);
      MIN_MV(6, -4, -1);
      MIN_MV(7, 4, -1);
      MIN_MV(8, -4, 0);
      MIN_MV(9, 4, 0);
      MIN_MV(10, -4, 1);
      MIN_MV(11, 4, 1);
      MIN_MV(12, -4, 2);
      MIN_MV(13, 4, 2);
      MIN_MV(14, -2, 3);
      MIN_MV(15, 2, 3);
#undef SADS
#undef ADD_MVCOST
#undef MIN_MV
      if (dir)
      {
        bmv.x = omv.x + i * (dir >> 4);
        bmv.y = omv.y + i * ((dir << 28) >> 28);
      }
    }
  } while (++i <= merange >> 2);
  if (bmv.checkRange(mvmin, mvmax))
    goto me_hex2;
  break;
}

case X265_STAR_SEARCH: // Adapted from HM ME
{
  int bPointNr = 0;
  int bDistance = 0;

  const int EarlyExitIters = 3;
  StarPatternSearch(ref, mvmin, mvmax, bmv, bcost, bPointNr, bDistance, EarlyExitIters, merange);
  if (bDistance == 1)
  {
    // if best distance was only 1, check two missing points.  If no new point is found, stop
    if (bPointNr)
    {
      /* For a given direction 1 to 8, check nearest two outer X pixels
      X   X
      X 1 2 3 X
      4 * 5
      X 6 7 8 X
      X   X
      */
      int saved = bcost;
      const MV mv1 = bmv + offsets[(bPointNr - 1) * 2];
      const MV mv2 = bmv + offsets[(bPointNr - 1) * 2 + 1];
      if (mv1.checkRange(mvmin, mvmax))
      {
        COST_MV(mv1.x, mv1.y);
      }
      if (mv2.checkRange(mvmin, mvmax))
      {
        COST_MV(mv2.x, mv2.y);
      }
      if (bcost == saved)
        break;
    }
    else
      break;
  }

  const int RasterDistance = 5;
  if (bDistance > RasterDistance)
  {
    // raster search refinement if original search distance was too big
    MV tmv;
    for (tmv.y = mvmin.y; tmv.y <= mvmax.y; tmv.y += RasterDistance)
    {
      for (tmv.x = mvmin.x; tmv.x <= mvmax.x; tmv.x += RasterDistance)
      {
        if (tmv.x + (RasterDistance * 3) <= mvmax.x)
        {
          pixel *pix_base = fref + tmv.y * stride + tmv.x;
          sad_x4(fenc,
            pix_base,
            pix_base + RasterDistance,
            pix_base + RasterDistance * 2,
            pix_base + RasterDistance * 3,
            stride, costs);
          costs[0] += mvcost(tmv << 2);
          COPY2_IF_LT(bcost, costs[0], bmv, tmv);
          tmv.x += RasterDistance;
          costs[1] += mvcost(tmv << 2);
          COPY2_IF_LT(bcost, costs[1], bmv, tmv);
          tmv.x += RasterDistance;
          costs[2] += mvcost(tmv << 2);
          COPY2_IF_LT(bcost, costs[2], bmv, tmv);
          tmv.x += RasterDistance;
          costs[3] += mvcost(tmv << 3);
          COPY2_IF_LT(bcost, costs[3], bmv, tmv);
        }
        else
          COST_MV(tmv.x, tmv.y);
      }
    }
  }

  while (bDistance > 0)
  {
    // center a new search around current best
    bDistance = 0;
    bPointNr = 0;
    const int MaxIters = 32;
    StarPatternSearch(ref, mvmin, mvmax, bmv, bcost, bPointNr, bDistance, MaxIters, merange);

    if (bDistance == 1)
    {
      if (!bPointNr)
        break;

      /* For a given direction 1 to 8, check nearest 2 outer X pixels
      X   X
      X 1 2 3 X
      4 * 5
      X 6 7 8 X
      X   X
      */
      const MV mv1 = bmv + offsets[(bPointNr - 1) * 2];
      const MV mv2 = bmv + offsets[(bPointNr - 1) * 2 + 1];
      if (mv1.checkRange(mvmin, mvmax))
      {
        COST_MV(mv1.x, mv1.y);
      }
      if (mv2.checkRange(mvmin, mvmax))
      {
        COST_MV(mv2.x, mv2.y);
      }
      break;
    }
  }

  break;
}

case X265_SEA:
{
  // Successive Elimination Algorithm
  const int16_t minX = X265_MAX(omv.x - (int16_t)merange, mvmin.x);
  const int16_t minY = X265_MAX(omv.y - (int16_t)merange, mvmin.y);
  const int16_t maxX = X265_MIN(omv.x + (int16_t)merange, mvmax.x);
  const int16_t maxY = X265_MIN(omv.y + (int16_t)merange, mvmax.y);
  const uint16_t *p_cost_mvx = m_cost_mvx - qmvp.x;
  const uint16_t *p_cost_mvy = m_cost_mvy - qmvp.y;
  int16_t* meScratchBuffer = NULL;
  int scratchSize = merange * 2 + 4;
  if (scratchSize)
  {
    meScratchBuffer = X265_MALLOC(int16_t, scratchSize);
    memset(meScratchBuffer, 0, sizeof(int16_t)* scratchSize);
  }

  /* SEA is fastest in multiples of 4 */
  int meRangeWidth = (maxX - minX + 3) & ~3;
  int w = 0, h = 0;                    // Width and height of the PU
  ALIGN_VAR_32(pixel, zero[64 * FENC_STRIDE]) = { 0 };
  ALIGN_VAR_32(int, encDC[4]);
  uint16_t *fpelCostMvX = m_fpelMvCosts[-qmvp.x & 3] + (-qmvp.x >> 2);
  sizesFromPartition(partEnum, &w, &h);
  int deltaX = (w <= 8) ? (w) : (w >> 1);
  int deltaY = (h <= 8) ? (h) : (h >> 1);

  /* Check if very small rectangular blocks which cannot be sub-divided anymore */
  bool smallRectPartition = partEnum == LUMA_4x4 || partEnum == LUMA_16x12 ||
    partEnum == LUMA_12x16 || partEnum == LUMA_16x4 || partEnum == LUMA_4x16;
  /* Check if vertical partition */
  bool verticalRect = partEnum == LUMA_32x64 || partEnum == LUMA_16x32 || partEnum == LUMA_8x16 ||
    partEnum == LUMA_4x8;
  /* Check if horizontal partition */
  bool horizontalRect = partEnum == LUMA_64x32 || partEnum == LUMA_32x16 || partEnum == LUMA_16x8 ||
    partEnum == LUMA_8x4;
  /* Check if assymetric vertical partition */
  bool assymetricVertical = partEnum == LUMA_12x16 || partEnum == LUMA_4x16 || partEnum == LUMA_24x32 ||
    partEnum == LUMA_8x32 || partEnum == LUMA_48x64 || partEnum == LUMA_16x64;
  /* Check if assymetric horizontal partition */
  bool assymetricHorizontal = partEnum == LUMA_16x12 || partEnum == LUMA_16x4 || partEnum == LUMA_32x24 ||
    partEnum == LUMA_32x8 || partEnum == LUMA_64x48 || partEnum == LUMA_64x16;

  int tempPartEnum = 0;

  /* If a vertical rectangular partition, it is horizontally split into two, for ads_x2() */
  if (verticalRect)
    tempPartEnum = partitionFromSizes(w, h >> 1);
  /* If a horizontal rectangular partition, it is vertically split into two, for ads_x2() */
  else if (horizontalRect)
    tempPartEnum = partitionFromSizes(w >> 1, h);
  /* We have integral planes introduced to account for assymetric partitions.
  * Hence all assymetric partitions except those which cannot be split into legal sizes,
  * are split into four for ads_x4() */
  else if (assymetricVertical || assymetricHorizontal)
    tempPartEnum = smallRectPartition ? partEnum : partitionFromSizes(w >> 1, h >> 1);
  /* General case: Square partitions. All partitions with width > 8 are split into four
  * for ads_x4(), for 4x4 and 8x8 we do ads_x1() */
  else
    tempPartEnum = (w <= 8) ? partEnum : partitionFromSizes(w >> 1, h >> 1);

  /* Successive elimination by comparing DC before a full SAD,
  * because sum(abs(diff)) >= abs(diff(sum)). */
  primitives.pu[tempPartEnum].sad_x4(zero,
    fenc,
    fenc + deltaX,
    fenc + deltaY * FENC_STRIDE,
    fenc + deltaX + deltaY * FENC_STRIDE,
    FENC_STRIDE,
    encDC);

  /* Assigning appropriate integral plane */
  uint32_t *sumsBase = NULL;
  switch (deltaX)
  {
  case 32: if (deltaY % 24 == 0)
    sumsBase = integral[1];
           else if (deltaY == 8)
             sumsBase = integral[2];
           else
             sumsBase = integral[0];
    break;
  case 24: sumsBase = integral[3];
    break;
  case 16: if (deltaY % 12 == 0)
    sumsBase = integral[5];
           else if (deltaY == 4)
             sumsBase = integral[6];
           else
             sumsBase = integral[4];
    break;
  case 12: sumsBase = integral[7];
    break;
  case 8: if (deltaY == 32)
    sumsBase = integral[8];
          else
            sumsBase = integral[9];
    break;
  case 4: if (deltaY == 16)
    sumsBase = integral[10];
          else
            sumsBase = integral[11];
    break;
  default: sumsBase = integral[11];
    break;
  }

  if (partEnum == LUMA_64x64 || partEnum == LUMA_32x32 || partEnum == LUMA_16x16 ||
    partEnum == LUMA_32x64 || partEnum == LUMA_16x32 || partEnum == LUMA_8x16 ||
    partEnum == LUMA_4x8 || partEnum == LUMA_12x16 || partEnum == LUMA_4x16 ||
    partEnum == LUMA_24x32 || partEnum == LUMA_8x32 || partEnum == LUMA_48x64 ||
    partEnum == LUMA_16x64)
    deltaY *= (int)stride;

  if (verticalRect)
    encDC[1] = encDC[2];

  if (horizontalRect)
    deltaY = deltaX;

  /* ADS and SAD */
  MV tmv;
  for (tmv.y = minY; tmv.y <= maxY; tmv.y++)
  {
    int i, xn;
    int ycost = p_cost_mvy[tmv.y] << 2;
    if (bcost <= ycost)
      continue;
    bcost -= ycost;

    /* ADS_4 for 16x16, 32x32, 64x64, 24x32, 32x24, 48x64, 64x48, 32x8, 8x32, 64x16, 16x64 partitions
    * ADS_1 for 4x4, 8x8, 16x4, 4x16, 16x12, 12x16 partitions
    * ADS_2 for all other rectangular partitions */
    xn = ads(encDC,
      sumsBase + minX + tmv.y * stride,
      deltaY,
      fpelCostMvX + minX,
      meScratchBuffer,
      meRangeWidth,
      bcost);

    for (i = 0; i < xn - 2; i += 3)
      COST_MV_X3_ABS(minX + meScratchBuffer[i], tmv.y,
        minX + meScratchBuffer[i + 1], tmv.y,
        minX + meScratchBuffer[i + 2], tmv.y);

    bcost += ycost;
    for (; i < xn; i++)
      COST_MV(minX + meScratchBuffer[i], tmv.y);
  }
  if (meScratchBuffer)
    x265_free(meScratchBuffer);
  break;
}

case X265_FULL_SEARCH:
{
  // dead slow exhaustive search, but at least it uses sad_x4()
  MV tmv;
  for (tmv.y = mvmin.y; tmv.y <= mvmax.y; tmv.y++)
  {
    for (tmv.x = mvmin.x; tmv.x <= mvmax.x; tmv.x++)
    {
      if (tmv.x + 3 <= mvmax.x)
      {
        pixel *pix_base = fref + tmv.y * stride + tmv.x;
        sad_x4(fenc,
          pix_base,
          pix_base + 1,
          pix_base + 2,
          pix_base + 3,
          stride, costs);
        costs[0] += mvcost(tmv << 2);
        COPY2_IF_LT(bcost, costs[0], bmv, tmv);
        tmv.x++;
        costs[1] += mvcost(tmv << 2);
        COPY2_IF_LT(bcost, costs[1], bmv, tmv);
        tmv.x++;
        costs[2] += mvcost(tmv << 2);
        COPY2_IF_LT(bcost, costs[2], bmv, tmv);
        tmv.x++;
        costs[3] += mvcost(tmv << 2);
        COPY2_IF_LT(bcost, costs[3], bmv, tmv);
      }
      else
        COST_MV(tmv.x, tmv.y);
    }
  }

  break;
}

default:
  X265_CHECK(0, "invalid motion estimate mode\n");
  break;
}

if (bprecost < bcost)
{
  bmv = bestpre;
  bcost = bprecost;
}
else
bmv = bmv.toQPel(); // promote search bmv to qpel

const SubpelWorkload& wl = workload[this->subpelRefine];

// check mv range for slice bound
if ((maxSlices > 1) & ((bmv.y < qmvmin.y) | (bmv.y > qmvmax.y)))
{
  bmv.y = x265_min(x265_max(bmv.y, qmvmin.y), qmvmax.y);
  bcost = subpelCompare(ref, bmv, satd) + mvcost(bmv);
}

if (!bcost)
{
  /* if there was zero residual at the clipped MVP, we can skip subpel
  * refine, but we do need to include the mvcost in the returned cost */
  bcost = mvcost(bmv);
}
else if (ref->isLowres)
{
  int bdir = 0;
  for (int i = 1; i <= wl.hpel_dirs; i++)
  {
    MV qmv = bmv + square1[i] * 2;

    /* skip invalid range */
    if ((qmv.y < qmvmin.y) | (qmv.y > qmvmax.y))
      continue;

    int cost = ref->lowresQPelCost(fenc, blockOffset, qmv, sad) + mvcost(qmv);
    COPY2_IF_LT(bcost, cost, bdir, i);
  }

  bmv += square1[bdir] * 2;
  bcost = ref->lowresQPelCost(fenc, blockOffset, bmv, satd) + mvcost(bmv);

  bdir = 0;
  for (int i = 1; i <= wl.qpel_dirs; i++)
  {
    MV qmv = bmv + square1[i];

    /* skip invalid range */
    if ((qmv.y < qmvmin.y) | (qmv.y > qmvmax.y))
      continue;

    int cost = ref->lowresQPelCost(fenc, blockOffset, qmv, satd) + mvcost(qmv);
    COPY2_IF_LT(bcost, cost, bdir, i);
  }

  bmv += square1[bdir];
}
else
{
  pixelcmp_t hpelcomp;

  if (wl.hpel_satd)
  {
    bcost = subpelCompare(ref, bmv, satd) + mvcost(bmv);
    hpelcomp = satd;
  }
  else
    hpelcomp = sad;

  for (int iter = 0; iter < wl.hpel_iters; iter++)
  {
    int bdir = 0;
    for (int i = 1; i <= wl.hpel_dirs; i++)
    {
      MV qmv = bmv + square1[i] * 2;

      // check mv range for slice bound
      if ((qmv.y < qmvmin.y) | (qmv.y > qmvmax.y))
        continue;

      int cost = subpelCompare(ref, qmv, hpelcomp) + mvcost(qmv);
      COPY2_IF_LT(bcost, cost, bdir, i);
    }

    if (bdir)
      bmv += square1[bdir] * 2;
    else
      break;
  }

  /* if HPEL search used SAD, remeasure with SATD before QPEL */
  if (!wl.hpel_satd)
    bcost = subpelCompare(ref, bmv, satd) + mvcost(bmv);

  for (int iter = 0; iter < wl.qpel_iters; iter++)
  {
    int bdir = 0;
    for (int i = 1; i <= wl.qpel_dirs; i++)
    {
      MV qmv = bmv + square1[i];

      // check mv range for slice bound
      if ((qmv.y < qmvmin.y) | (qmv.y > qmvmax.y))
        continue;

      int cost = subpelCompare(ref, qmv, satd) + mvcost(qmv);
      COPY2_IF_LT(bcost, cost, bdir, i);
    }

    if (bdir)
      bmv += square1[bdir];
    else
      break;
  }
}

// check mv range for slice bound
X265_CHECK(((bmv.y >= qmvmin.y) & (bmv.y <= qmvmax.y)), "mv beyond range!");

x265_emms();
outQMv = bmv;
return bcost;
}
#endif // 0 x265

template<typename pixel_t>
void PlaneOfBlocks::UMHSearch(WorkingArea &workarea, int i_me_range, int omx, int omy) // radius
{
  // Uneven-cross Multi-Hexagon-grid Search (see x264)
  /* hexagon grid */

//	int omx = workarea.bestMV.x;
//	int omy = workarea.bestMV.y;
  // my mod: do not shift the center after Cross
  CrossSearch<pixel_t>(workarea, 1, i_me_range, i_me_range, omx, omy);

  int i = 1;
  do
  {
  /*   -4 -2  0  2  4
 -4           x
 -3        x     x
 -2     x           x
 -1     x           x
  0     x           x
  1     x           x
  2     x           x
  3        x     x
  4           x
  */
    static const int hex4[16][2] =
    {
      {-4, 2}, {-4, 1}, {-4, 0}, {-4,-1}, {-4,-2},
      { 4,-2}, { 4,-1}, { 4, 0}, { 4, 1}, { 4, 2},
      { 2, 3}, { 0, 4}, {-2, 3},
      {-2,-3}, { 0,-4}, { 2,-3},
    };

    for (int j = 0; j < 16; j++)
    {
      int mx = omx + hex4[j][0] * i;
      int my = omy + hex4[j][1] * i;
      CheckMV<pixel_t>(workarea, mx, my);
    }
  } while (++i <= i_me_range / 4);

  //	if( bmy <= mv_y_max )
  // {
  //		goto me_hex2;
  //	}

  Hex2Search<pixel_t>(workarea, i_me_range);
}



//----------------------------------------------------------------



// estimate global motion from current plane vectors data for using on next plane - added by Fizick
// on input globalMVec is prev estimation
// on output globalMVec is doubled for next scale plane using
//
// use very simple but robust method
// more advanced method (like MVDepan) can be implemented later
void PlaneOfBlocks::EstimateGlobalMVDoubled(VECTOR *globalMVec, Slicer &slicer)
{
  assert(globalMVec != 0);
  assert(&slicer != 0);

  _gvect_result_count = 2;
  _gvect_estim_ptr = globalMVec;

  slicer.start(2, *this, &PlaneOfBlocks::estimate_global_mv_doubled_slice);
}



void	PlaneOfBlocks::estimate_global_mv_doubled_slice(Slicer::TaskData &td)
{
  bool				both_done_flag = false;
  for (int y = td._y_beg; y < td._y_end; ++y)
  {
    std::vector <int> &	freq_arr = freqArray[y];

    const int      freqSize = int(freq_arr.size());
    memset(&freq_arr[0], 0, freqSize * sizeof(freq_arr[0])); // reset

    int            indmin = freqSize - 1;
    int            indmax = 0;

    // find most frequent x
    if (y == 0)
    {
      for (int i = 0; i < nBlkCount; i++)
      {
        int ind = (freqSize >> 1) + vectors[i].x;
        if (ind >= 0 && ind < freqSize)
        {
          ++freq_arr[ind];
          if (ind > indmax)
          {
            indmax = ind;
          }
          if (ind < indmin)
          {
            indmin = ind;
          }
        }
      }
    }

    // find most frequent y
    else
    {
      for (int i = 0; i < nBlkCount; i++)
      {
        int ind = (freqSize >> 1) + vectors[i].y;
        if (ind >= 0 && ind < freqSize)
        {
          ++freq_arr[ind];
          if (ind > indmax)
          {
            indmax = ind;
          }
          if (ind < indmin)
          {
            indmin = ind;
          }
        }
      }	// i < nBlkCount
    }

    int count = freq_arr[indmin];
    int index = indmin;
    for (int i = indmin + 1; i <= indmax; i++)
    {
      if (freq_arr[i] > count)
      {
        count = freq_arr[i];
        index = i;
      }
    }

    // most frequent value
    _gvect_estim_ptr->coord[y] = index - (freqSize >> 1);

    conc::AioSub <int>   dec_ftor(1);
    const int            new_count =
      conc::AtomicIntOp::exec_new(_gvect_result_count, dec_ftor);
    both_done_flag = (new_count <= 0);
  }

  // iteration to increase precision
  if (both_done_flag)
  {
    int medianx = _gvect_estim_ptr->x;
    int mediany = _gvect_estim_ptr->y;
    int meanvx = 0;
    int meanvy = 0;
    int num = 0;
    for (int i = 0; i < nBlkCount; i++)
    {
      if (abs(vectors[i].x - medianx) < 6
        && abs(vectors[i].y - mediany) < 6)
      {
        meanvx += vectors[i].x;
        meanvy += vectors[i].y;
        num += 1;
      }
    }

    // output vectors must be doubled for next (finer) scale level
    if (num > 0)
    {
      _gvect_estim_ptr->x = 2 * meanvx / num;
      _gvect_estim_ptr->y = 2 * meanvy / num;
    }
    else
    {
      _gvect_estim_ptr->x = 2 * medianx;
      _gvect_estim_ptr->y = 2 * mediany;
    }
  }

  //	char debugbuf[100];
  //	sprintf(debugbuf,"MVAnalyse: nx=%d ny=%d next global vx=%d vy=%d", nBlkX, nBlkY, globalMVec->x, globalMVec->y);
  //	OutputDebugString(debugbuf);
}



//----------------------------------------------------------------



/* fetch the block in the reference frame, which is pointed by the vector (vx, vy) */
MV_FORCEINLINE const uint8_t *	PlaneOfBlocks::GetRefBlock(WorkingArea &workarea, int nVx, int nVy)
{
  //	return pRefFrame->GetPlane(YPLANE)->GetAbsolutePointer((workarea.x[0]<<nLogPel) + nVx, (workarea.y[0]<<nLogPel) + nVy);
  return (nPel == 2) ? pRefFrame->GetPlane(YPLANE)->GetAbsolutePointerPel <1>((workarea.x[0] << 1) + nVx, (workarea.y[0] << 1) + nVy) :
    (nPel == 1) ? pRefFrame->GetPlane(YPLANE)->GetAbsolutePointerPel <0>((workarea.x[0]) + nVx, (workarea.y[0]) + nVy) :
    pRefFrame->GetPlane(YPLANE)->GetAbsolutePointerPel <2>((workarea.x[0] << 2) + nVx, (workarea.y[0] << 2) + nVy);
}

MV_FORCEINLINE const uint8_t *	PlaneOfBlocks::GetRefBlockU(WorkingArea &workarea, int nVx, int nVy)
{
  //	return pRefFrame->GetPlane(UPLANE)->GetAbsolutePointer((workarea.x[1]<<nLogPel) + (nVx >> 1), (workarea.y[1]<<nLogPel) + (yRatioUV==1 ? nVy : nVy>>1) ); //v.1.2.1
  // 161130 bitshifts instead of ternary operator
  return (nPel == 2) ? pRefFrame->GetPlane(UPLANE)->GetAbsolutePointerPel <1>((workarea.x[1] << 1) + (nVx >> nLogxRatioUV), (workarea.y[1] << 1) + (nVy >> nLogyRatioUV)) :
    (nPel == 1) ? pRefFrame->GetPlane(UPLANE)->GetAbsolutePointerPel <0>((workarea.x[1]) + (nVx >> nLogxRatioUV), (workarea.y[1]) + (nVy >> nLogyRatioUV)) :
    pRefFrame->GetPlane(UPLANE)->GetAbsolutePointerPel <2>((workarea.x[1] << 2) + (nVx >> nLogxRatioUV), (workarea.y[1] << 2) + (nVy >> nLogyRatioUV));
  // xRatioUV fix after 2.7.0.22c
}

MV_FORCEINLINE const uint8_t *	PlaneOfBlocks::GetRefBlockV(WorkingArea &workarea, int nVx, int nVy)
{
  //	return pRefFrame->GetPlane(VPLANE)->GetAbsolutePointer((workarea.x[2]<<nLogPel) + (nVx >> 1), (workarea.y[2]<<nLogPel) + (yRatioUV==1 ? nVy : nVy>>1) );
  return (nPel == 2) ? pRefFrame->GetPlane(VPLANE)->GetAbsolutePointerPel <1>((workarea.x[2] << 1) + (nVx >> nLogxRatioUV), (workarea.y[2] << 1) + (nVy >> nLogyRatioUV)) :
    (nPel == 1) ? pRefFrame->GetPlane(VPLANE)->GetAbsolutePointerPel <0>((workarea.x[2]) + (nVx >> nLogxRatioUV), (workarea.y[2]) + (nVy >> nLogyRatioUV)) :
    pRefFrame->GetPlane(VPLANE)->GetAbsolutePointerPel <2>((workarea.x[2] << 2) + (nVx >> nLogxRatioUV), (workarea.y[2] << 2) + (nVy >> nLogyRatioUV));
  // xRatioUV fix after 2.7.0.22c
}

MV_FORCEINLINE const uint8_t *	PlaneOfBlocks::GetSrcBlock(int nX, int nY)
{
  return pSrcFrame->GetPlane(YPLANE)->GetAbsolutePelPointer(nX, nY);
}

template<typename pixel_t>
sad_t	PlaneOfBlocks::LumaSADx(WorkingArea &workarea, const unsigned char *pRef0)
{
  sad_t sad;
  sad_t refLuma;
  typedef typename std::conditional < sizeof(pixel_t) == 1, sad_t, bigsad_t >::type safe_sad_t;
  switch (dctmode)
  {
  case 1: // dct SAD
    workarea.DCT->DCTBytes2D(pRef0, nRefPitch[0], &workarea.dctRef[0], dctpitch);
    sad = ((safe_sad_t)SAD(&workarea.dctSrc[0], dctpitch, &workarea.dctRef[0], dctpitch) +
           abs(reinterpret_cast<pixel_t *>(&workarea.dctSrc[0])[0] - reinterpret_cast<pixel_t *>(&workarea.dctRef[0])[0]) * 3)*nBlkSizeX / 2; //correct reduced DC component
    break;
  case 2: //  globally (lumaChange) weighted spatial and DCT
    sad = SAD(workarea.pSrc[0], nSrcPitch[0], pRef0, nRefPitch[0]);
    if (dctweight16 > 0)
    {
      workarea.DCT->DCTBytes2D(pRef0, nRefPitch[0], &workarea.dctRef[0], dctpitch);
      sad_t dctsad = ((safe_sad_t)SAD(&workarea.dctSrc[0], dctpitch, &workarea.dctRef[0], dctpitch) +
        abs(reinterpret_cast<pixel_t *>(&workarea.dctSrc[0])[0] - reinterpret_cast<pixel_t *>(&workarea.dctRef[0])[0]) * 3)*nBlkSizeX / 2;
      sad = (sad*(16 - dctweight16) + dctsad*dctweight16) / 16;
    }
    break;
  case 3: // per block adaptive switched from spatial to equal mixed SAD (faster)
    refLuma = LUMA(pRef0, nRefPitch[0]);
    sad = SAD(workarea.pSrc[0], nSrcPitch[0], pRef0, nRefPitch[0]);
    if (abs((int)workarea.srcLuma - (int)refLuma) > ((int)workarea.srcLuma + (int)refLuma) >> 5)
    {
      workarea.DCT->DCTBytes2D(pRef0, nRefPitch[0], &workarea.dctRef[0], dctpitch);
      sad_t dctsad = (safe_sad_t)SAD(&workarea.dctSrc[0], dctpitch, &workarea.dctRef[0], dctpitch)*nBlkSizeX / 2;
      sad = sad / 2 + dctsad / 2;
    }
    break;
  case 4: //  per block adaptive switched from spatial to mixed SAD with more weight of DCT (best?)
    refLuma = LUMA(pRef0, nRefPitch[0]);
    sad = SAD(workarea.pSrc[0], nSrcPitch[0], pRef0, nRefPitch[0]);
    if (abs((int)workarea.srcLuma - (int)refLuma) > ((int)workarea.srcLuma + (int)refLuma) >> 5)
    {
      workarea.DCT->DCTBytes2D(pRef0, nRefPitch[0], &workarea.dctRef[0], dctpitch);
      sad_t dctsad = (safe_sad_t)SAD(&workarea.dctSrc[0], dctpitch, &workarea.dctRef[0], dctpitch)*nBlkSizeX / 2;
      sad = sad / 4 + dctsad / 2 + dctsad / 4;
    }
    break;
  case 5: // dct SAD (SATD)
    sad = SATD(workarea.pSrc[0], nSrcPitch[0], pRef0, nRefPitch[0]);
    // buggy? PF QTGMC(dct=5). 20160816 No! SATD function was linked to Dummy, did nothing. Made live again from 2.7.0.22d
    break;
  case 6: //  globally (lumaChange) weighted spatial and DCT (better estimate)
    sad = SAD(workarea.pSrc[0], nSrcPitch[0], pRef0, nRefPitch[0]);
    if (dctweight16 > 0)
    {
      sad_t dctsad = SATD(workarea.pSrc[0], nSrcPitch[0], pRef0, nRefPitch[0]);
      sad = ((safe_sad_t)sad*(16 - dctweight16) + dctsad*dctweight16) / 16;
    }
    break;
  case 7: // per block adaptive switched from spatial to equal mixed SAD (faster?)
    refLuma = LUMA(pRef0, nRefPitch[0]);
    sad = SAD(workarea.pSrc[0], nSrcPitch[0], pRef0, nRefPitch[0]);
    if (abs((int)workarea.srcLuma - (int)refLuma) > (workarea.srcLuma + refLuma) >> 5)
    {
      sad_t dctsad = SATD(workarea.pSrc[0], nSrcPitch[0], pRef0, nRefPitch[0]);
      sad = sad / 2 + dctsad / 2;
    }
    break;
  case 8: //  per block adaptive switched from spatial to mixed SAD with more weight of DCT (faster?)
    refLuma = LUMA(pRef0, nRefPitch[0]);
    sad = SAD(workarea.pSrc[0], nSrcPitch[0], pRef0, nRefPitch[0]);
    if (abs((int)workarea.srcLuma - (int)refLuma) > (workarea.srcLuma + refLuma) >> 5)
    {
      sad_t dctsad = SATD(workarea.pSrc[0], nSrcPitch[0], pRef0, nRefPitch[0]);
      sad = sad / 4 + dctsad / 2 + dctsad / 4;
    }
    break;
  case 9: //  globally (lumaChange) weighted spatial and DCT (better estimate, only half weight on SATD)
    sad = SAD(workarea.pSrc[0], nSrcPitch[0], pRef0, nRefPitch[0]);
    if (dctweight16 > 1)
    {
      int dctweighthalf = dctweight16 / 2;
      sad_t dctsad = SATD(workarea.pSrc[0], nSrcPitch[0], pRef0, nRefPitch[0]);
      sad = ((safe_sad_t)sad*(16 - dctweighthalf) + dctsad*dctweighthalf) / 16;
    }
    break;
  case 10: // per block adaptive switched from spatial to mixed SAD, weighted to SAD (faster)
    refLuma = LUMA(pRef0, nRefPitch[0]);
    sad = SAD(workarea.pSrc[0], nSrcPitch[0], pRef0, nRefPitch[0]);
    if (abs((int)workarea.srcLuma - (int)refLuma) > ((int)workarea.srcLuma + (int)refLuma) >> 4)
    {
      sad_t dctsad = SATD(workarea.pSrc[0], nSrcPitch[0], pRef0, nRefPitch[0]);
      sad = sad / 2 + dctsad / 4 + sad / 4;
    }
    break;
  default:
    sad = SAD(workarea.pSrc[0], nSrcPitch[0], pRef0, nRefPitch[0]);
  }
  return sad;
}

template<typename pixel_t>
MV_FORCEINLINE sad_t	PlaneOfBlocks::LumaSAD(WorkingArea &workarea, const unsigned char *pRef0)
{
#ifdef MOTION_DEBUG
  workarea.iter++;
#endif
#ifdef ALLOW_DCT
  // made simple SAD more prominent (~1% faster) while keeping DCT support (TSchniede)
  return !dctmode ? SAD(workarea.pSrc[0], nSrcPitch[0], pRef0, nRefPitch[0]) : LumaSADx<pixel_t>(workarea, pRef0);
#else
  return SAD(workarea.pSrc[0], nSrcPitch[0], pRef0, nRefPitch[0]);
#endif
}


/* check if the vector (vx, vy) is better than the best vector found so far without penalty new - renamed in v.2.11*/
template<typename pixel_t>
MV_FORCEINLINE void	PlaneOfBlocks::CheckMV0(WorkingArea &workarea, int vx, int vy)
{		//here the chance for default values are high especially for zeroMVfieldShifted (on left/top border)
  if (
#ifdef ONLY_CHECK_NONDEFAULT_MV
  ((vx != 0) || (vy != zeroMVfieldShifted.y)) &&
    ((vx != workarea.predictor.x) || (vy != workarea.predictor.y)) &&
    ((vx != workarea.globalMVPredictor.x) || (vy != workarea.globalMVPredictor.y)) &&
#endif
    workarea.IsVectorOK(vx, vy))
  {
#if 0
    sad_t saduv = (chroma) ? ScaleSadChroma(SADCHROMA(workarea.pSrc[1], nSrcPitch[1], GetRefBlockU(workarea, vx, vy), nRefPitch[1])
      + SADCHROMA(workarea.pSrc[2], nSrcPitch[2], GetRefBlockV(workarea, vx, vy), nRefPitch[2]), effective_chromaSADscale) : 0;
    sad_t sad = LumaSAD<pixel_t>(workarea, GetRefBlock(workarea, vx, vy));
    sad += saduv;
    sad_t cost = sad + workarea.MotionDistorsion(vx, vy);
    //		int cost = sad + sad*workarea.MotionDistorsion(vx, vy)/(nBlkSizeX*nBlkSizeY*4);
    //		if (sad>bigSAD) { DebugPrintf("%d %d %d %d %d %d", workarea.blkIdx, vx, vy, workarea.nMinCost, cost, sad);}
    if (cost < workarea.nMinCost)
    {
      workarea.bestMV.x = vx;
      workarea.bestMV.y = vy;
      workarea.nMinCost = cost;
      workarea.bestMV.sad = sad;
    }
#else
    // from 2.5.11.9-SVP: no additional SAD calculations if partial sum is already above minCost
    sad_t cost=workarea.MotionDistorsion<pixel_t>(vx, vy);
    if(cost>=workarea.nMinCost) return;

    sad_t sad=LumaSAD<pixel_t>(workarea, GetRefBlock(workarea, vx, vy));
    cost+=sad;
    if(cost>=workarea.nMinCost) return;

    sad_t saduv = (chroma) ? ScaleSadChroma(SADCHROMA(workarea.pSrc[1], nSrcPitch[1], GetRefBlockU(workarea, vx, vy), nRefPitch[1])
      + SADCHROMA(workarea.pSrc[2], nSrcPitch[2], GetRefBlockV(workarea, vx, vy), nRefPitch[2]), effective_chromaSADscale) : 0;
    cost += saduv;
    if(cost>=workarea.nMinCost) return;

    workarea.bestMV.x = vx;
    workarea.bestMV.y = vy;
    workarea.nMinCost = cost;
    workarea.bestMV.sad = sad+saduv;

#endif
  }
}

/* check if the vector (vx, vy) is better than the best vector found so far */
template<typename pixel_t>
MV_FORCEINLINE void	PlaneOfBlocks::CheckMV(WorkingArea &workarea, int vx, int vy)
{		//here the chance for default values are high especially for zeroMVfieldShifted (on left/top border)
  if (
#ifdef ONLY_CHECK_NONDEFAULT_MV
  ((vx != 0) || (vy != zeroMVfieldShifted.y)) &&
    ((vx != workarea.predictor.x) || (vy != workarea.predictor.y)) &&
    ((vx != workarea.globalMVPredictor.x) || (vy != workarea.globalMVPredictor.y)) &&
#endif
    workarea.IsVectorOK(vx, vy))
  {
#if 0
    sad_t saduv =
      !(chroma) ? 0 :
      ScaleSadChroma(SADCHROMA(workarea.pSrc[1], nSrcPitch[1], GetRefBlockU(workarea, vx, vy), nRefPitch[1])
      + SADCHROMA(workarea.pSrc[2], nSrcPitch[2], GetRefBlockV(workarea, vx, vy), nRefPitch[2]), effective_chromaSADscale);
    sad_t sad = LumaSAD<pixel_t>(workarea, GetRefBlock(workarea, vx, vy));
    sad += saduv;
    sad_t cost = sad + workarea.MotionDistorsion(vx, vy) + ((penaltyNew*(bigsad_t)sad) >> 8); //v2
//		int cost = sad + sad*workarea.MotionDistorsion(vx, vy)/(nBlkSizeX*nBlkSizeY*4);
//		if (sad>bigSAD) { DebugPrintf("%d %d %d %d %d %d", workarea.blkIdx, vx, vy, workarea.nMinCost, cost, sad);}
    if (cost < workarea.nMinCost)
    {
      workarea.bestMV.x = vx;
      workarea.bestMV.y = vy;
      workarea.nMinCost = cost;
      workarea.bestMV.sad = sad;
    }
#else
    // from 2.5.11.9-SVP: no additional SAD calculations if partial sum is already above minCost
    sad_t cost=workarea.MotionDistorsion<pixel_t>(vx, vy);
    if(cost>=workarea.nMinCost) return;

    typedef typename std::conditional < sizeof(pixel_t) == 1, sad_t, bigsad_t >::type safe_sad_t;

    sad_t sad=LumaSAD<pixel_t>(workarea, GetRefBlock(workarea, vx, vy));
    cost += sad + ((penaltyNew*(safe_sad_t)sad) >> 8);
    if(cost>=workarea.nMinCost) return;

    sad_t saduv = (chroma) ? ScaleSadChroma(SADCHROMA(workarea.pSrc[1], nSrcPitch[1], GetRefBlockU(workarea, vx, vy), nRefPitch[1])
      + SADCHROMA(workarea.pSrc[2], nSrcPitch[2], GetRefBlockV(workarea, vx, vy), nRefPitch[2]), effective_chromaSADscale) : 0;
    cost += saduv + ((penaltyNew*(safe_sad_t)saduv) >> 8);
    if(cost>=workarea.nMinCost) return;

    workarea.bestMV.x = vx;
    workarea.bestMV.y = vy;
    workarea.nMinCost = cost;
    workarea.bestMV.sad = sad+saduv;
#endif
  }
}

/* check if the vector (vx, vy) is better, and update dir accordingly */
template<typename pixel_t>
MV_FORCEINLINE void	PlaneOfBlocks::CheckMV2(WorkingArea &workarea, int vx, int vy, int *dir, int val)
{
  if (
#ifdef ONLY_CHECK_NONDEFAULT_MV
  ((vx != 0) || (vy != zeroMVfieldShifted.y)) &&
    ((vx != workarea.predictor.x) || (vy != workarea.predictor.y)) &&
    ((vx != workarea.globalMVPredictor.x) || (vy != workarea.globalMVPredictor.y)) &&
#endif
    workarea.IsVectorOK(vx, vy))
  {
#if 0
    sad_t saduv =
      !(chroma) ? 0 :
      ScaleSadChroma(SADCHROMA(workarea.pSrc[1], nSrcPitch[1], GetRefBlockU(workarea, vx, vy), nRefPitch[1])
      + SADCHROMA(workarea.pSrc[2], nSrcPitch[2], GetRefBlockV(workarea, vx, vy), nRefPitch[2]), effective_chromaSADscale);
    sad_t sad = LumaSAD<pixel_t>(workarea, GetRefBlock(workarea, vx, vy));
    sad += saduv;
    sad_t cost = sad + workarea.MotionDistorsion(vx, vy) + ((penaltyNew*(bigsad_t)sad) >> 8); // v1.5.8
//		if (sad > LSAD/4) DebugPrintf("%d %d %d %d %d %d %d", workarea.blkIdx, vx, vy, val, workarea.nMinCost, cost, sad);
//		int cost = sad + sad*workarea.MotionDistorsion(vx, vy)/(nBlkSizeX*nBlkSizeY*4) + ((penaltyNew*sad)>>8); // v1.5.8
    if (cost < workarea.nMinCost)
    {
      workarea.bestMV.x = vx;
      workarea.bestMV.y = vy;
      workarea.bestMV.sad = sad;
      workarea.nMinCost = cost;
      *dir = val;
    }
#else
    // from 2.5.11.9-SVP: no additional SAD calculations if partial sum is already above minCost
    sad_t cost=workarea.MotionDistorsion<pixel_t>(vx, vy);
    if(cost>=workarea.nMinCost) return;

    typedef typename std::conditional < sizeof(pixel_t) == 1, sad_t, bigsad_t >::type safe_sad_t;

    sad_t sad=LumaSAD<pixel_t>(workarea, GetRefBlock(workarea, vx, vy));
    cost += sad + ((penaltyNew*(safe_sad_t)sad) >> 8);
    if(cost>=workarea.nMinCost) return;

    sad_t saduv = (chroma) ? ScaleSadChroma(SADCHROMA(workarea.pSrc[1], nSrcPitch[1], GetRefBlockU(workarea, vx, vy), nRefPitch[1])
      + SADCHROMA(workarea.pSrc[2], nSrcPitch[2], GetRefBlockV(workarea, vx, vy), nRefPitch[2]), effective_chromaSADscale) : 0;
    cost += saduv+((penaltyNew*(safe_sad_t)saduv) >> 8);
    if(cost>=workarea.nMinCost) return;

    workarea.bestMV.x = vx;
    workarea.bestMV.y = vy;
    workarea.nMinCost = cost;
    workarea.bestMV.sad = sad + saduv;
    *dir = val;
#endif
  }
}

/* check if the vector (vx, vy) is better, and update dir accordingly, but not workarea.bestMV.x, y */
template<typename pixel_t>
MV_FORCEINLINE void	PlaneOfBlocks::CheckMVdir(WorkingArea &workarea, int vx, int vy, int *dir, int val)
{
  if (
#ifdef ONLY_CHECK_NONDEFAULT_MV
  ((vx != 0) || (vy != zeroMVfieldShifted.y)) &&
    ((vx != workarea.predictor.x) || (vy != workarea.predictor.y)) &&
    ((vx != workarea.globalMVPredictor.x) || (vy != workarea.globalMVPredictor.y)) &&
#endif
    workarea.IsVectorOK(vx, vy))
  {
#if 0
    sad_t saduv = (chroma) ? ScaleSadChroma(SADCHROMA(workarea.pSrc[1], nSrcPitch[1], GetRefBlockU(workarea, vx, vy), nRefPitch[1])
      + SADCHROMA(workarea.pSrc[2], nSrcPitch[2], GetRefBlockV(workarea, vx, vy), nRefPitch[2]), effective_chromaSADscale) : 0;
    sad_t sad = LumaSAD<pixel_t>(workarea, GetRefBlock(workarea, vx, vy));
    sad += saduv;
    sad_t cost = sad + workarea.MotionDistorsion(vx, vy) + ((penaltyNew*(bigsad_t)sad) >> 8); // v1.5.8
//		if (sad > LSAD/4) DebugPrintf("%d %d %d %d %d %d %d", workarea.blkIdx, vx, vy, val, workarea.nMinCost, cost, sad);
//		int cost = sad + sad*workarea.MotionDistorsion(vx, vy)/(nBlkSizeX*nBlkSizeY*4) + ((penaltyNew*sad)>>8); // v1.5.8
    if (cost < workarea.nMinCost)
    {
      workarea.bestMV.sad = sad;
      workarea.nMinCost = cost;
      *dir = val;
    }
#else
    // from 2.5.11.9-SVP: no additional SAD calculations if partial sum is already above minCost
    sad_t cost=workarea.MotionDistorsion<pixel_t>(vx, vy);
    if(cost>=workarea.nMinCost) return;

    typedef typename std::conditional < sizeof(pixel_t) == 1, sad_t, bigsad_t >::type safe_sad_t;

    sad_t sad=LumaSAD<pixel_t>(workarea, GetRefBlock(workarea, vx, vy));
    cost += sad + ((penaltyNew*(safe_sad_t)sad) >> 8);
    if(cost>=workarea.nMinCost) return;

    sad_t saduv = (chroma) ? ScaleSadChroma(SADCHROMA(workarea.pSrc[1], nSrcPitch[1], GetRefBlockU(workarea, vx, vy), nRefPitch[1])
      + SADCHROMA(workarea.pSrc[2], nSrcPitch[2], GetRefBlockV(workarea, vx, vy), nRefPitch[2]), effective_chromaSADscale) : 0;
    cost += saduv+((penaltyNew*(safe_sad_t)saduv) >> 8);
    if(cost>=workarea.nMinCost) return;

    workarea.nMinCost = cost;
    workarea.bestMV.sad = sad + saduv;
    *dir = val;
#endif
  }
}

/* clip a vector to the horizontal boundaries */
MV_FORCEINLINE int	PlaneOfBlocks::ClipMVx(WorkingArea &workarea, int vx)
{
  //	return imin(workarea.nDxMax - 1, imax(workarea.nDxMin, vx));
  if (vx < workarea.nDxMin) return workarea.nDxMin;
  else if (vx >= workarea.nDxMax) return workarea.nDxMax - 1;
  else return vx;
}

/* clip a vector to the vertical boundaries */
MV_FORCEINLINE int	PlaneOfBlocks::ClipMVy(WorkingArea &workarea, int vy)
{
  //	return imin(workarea.nDyMax - 1, imax(workarea.nDyMin, vy));
  if (vy < workarea.nDyMin) return workarea.nDyMin;
  else if (vy >= workarea.nDyMax) return workarea.nDyMax - 1;
  else return vy;
}

/* clip a vector to the search boundaries */
MV_FORCEINLINE VECTOR	PlaneOfBlocks::ClipMV(WorkingArea &workarea, VECTOR v)
{
  VECTOR v2;
  v2.x = ClipMVx(workarea, v.x);
  v2.y = ClipMVy(workarea, v.y);
  v2.sad = v.sad;
  return v2;
}


/* find the median between a, b and c */
MV_FORCEINLINE int	PlaneOfBlocks::Median(int a, int b, int c)
{
  //	return a + b + c - imax(a, imax(b, c)) - imin(c, imin(a, b));
  if (a < b)
  {
    if (b < c) return b;
    else if (a < c) return c;
    else return a;
  }
  else {
    if (a < c) return a;
    else if (b < c) return c;
    else return b;
  }
}

/* computes square distance between two vectors */
#if 0
// not used
MV_FORCEINLINE unsigned int	PlaneOfBlocks::SquareDifferenceNorm(const VECTOR& v1, const VECTOR& v2)
{
  return (v1.x - v2.x) * (v1.x - v2.x) + (v1.y - v2.y) * (v1.y - v2.y);
}
#endif
/* computes square distance between two vectors */
MV_FORCEINLINE unsigned int	PlaneOfBlocks::SquareDifferenceNorm(const VECTOR& v1, const int v2x, const int v2y)
{
  return (v1.x - v2x) * (v1.x - v2x) + (v1.y - v2y) * (v1.y - v2y);
}

/* check if an index is inside the block's min and max indexes */
MV_FORCEINLINE bool	PlaneOfBlocks::IsInFrame(int i)
{
  return ((i >= 0) && (i < nBlkCount));
}



template<typename pixel_t>
void	PlaneOfBlocks::search_mv_slice(Slicer::TaskData &td)
{
  assert(&td != 0);

  short *			outfilebuf = _outfilebuf;

  WorkingArea &	workarea = *(_workarea_pool.take_obj());
  assert(&workarea != 0);

  workarea.blky_beg = td._y_beg;
  workarea.blky_end = td._y_end;

  workarea.DCT = 0;
#ifdef ALLOW_DCT
  if (_dct_pool_ptr != 0)
  {
    workarea.DCT = _dct_pool_ptr->take_obj();
  }
#endif	// ALLOW_DCT

  int *pBlkData = _out + 1 + workarea.blky_beg * nBlkX*N_PER_BLOCK;
  if (outfilebuf != NULL)
  {
    outfilebuf += workarea.blky_beg * nBlkX * 4;// 4 short word per block
  }

  workarea.y[0] = pSrcFrame->GetPlane(YPLANE)->GetVPadding();
  workarea.y[0] += workarea.blky_beg * (nBlkSizeY - nOverlapY);

  if (pSrcFrame->GetMode() & UPLANE)
  {
    workarea.y[1] = pSrcFrame->GetPlane(UPLANE)->GetVPadding();
    workarea.y[1] += workarea.blky_beg * ((nBlkSizeY - nOverlapY) >> nLogyRatioUV);
  }
  if (pSrcFrame->GetMode() & VPLANE)
  {
    workarea.y[2] = pSrcFrame->GetPlane(VPLANE)->GetVPadding();
    workarea.y[2] += workarea.blky_beg * ((nBlkSizeY - nOverlapY) >> nLogyRatioUV);
  }

  workarea.planeSAD = 0;
  workarea.sumLumaChange = 0;

  // Functions using float must not be used here

  int nBlkSizeX_Ovr[3] = { (nBlkSizeX - nOverlapX), (nBlkSizeX - nOverlapX) >> nLogxRatioUV, (nBlkSizeX - nOverlapX) >> nLogxRatioUV };
  int nBlkSizeY_Ovr[3] = { (nBlkSizeY - nOverlapY), (nBlkSizeY - nOverlapY) >> nLogyRatioUV, (nBlkSizeY - nOverlapY) >> nLogyRatioUV };

  for (workarea.blky = workarea.blky_beg; workarea.blky < workarea.blky_end; workarea.blky++)
  {
    workarea.blkScanDir = (workarea.blky % 2 == 0 || !_meander_flag) ? 1 : -1;
    // meander (alternate) scan blocks (even row left to right, odd row right to left)
    int blkxStart = (workarea.blky % 2 == 0 || !_meander_flag) ? 0 : nBlkX - 1;
    if (workarea.blkScanDir == 1) // start with leftmost block
    {
      workarea.x[0] = pSrcFrame->GetPlane(YPLANE)->GetHPadding();
      if (chroma)
      {
        workarea.x[1] = pSrcFrame->GetPlane(UPLANE)->GetHPadding();
        workarea.x[2] = pSrcFrame->GetPlane(VPLANE)->GetHPadding();
      }
    }
    else // start with rightmost block, but it is already set at prev row
    {
      workarea.x[0] = pSrcFrame->GetPlane(YPLANE)->GetHPadding() + nBlkSizeX_Ovr[0]*(nBlkX - 1);
      if (chroma)
      {
        workarea.x[1] = pSrcFrame->GetPlane(UPLANE)->GetHPadding() + nBlkSizeX_Ovr[1]*(nBlkX - 1);
        workarea.x[2] = pSrcFrame->GetPlane(VPLANE)->GetHPadding() + nBlkSizeX_Ovr[2]*(nBlkX - 1);
      }
    }

    for (int iblkx = 0; iblkx < nBlkX; iblkx++)
    {
      workarea.blkx = blkxStart + iblkx*workarea.blkScanDir;
      workarea.blkIdx = workarea.blky*nBlkX + workarea.blkx;
      workarea.iter = 0;
      //			DebugPrintf("BlkIdx = %d \n", workarea.blkIdx);
      PROFILE_START(MOTION_PROFILE_ME);

      // Resets the global predictor (it may have been clipped during the
      // previous block scan)
      workarea.globalMVPredictor = _glob_mv_pred_def;

#if (ALIGN_SOURCEBLOCK > 1)
      //store the pitch
      const BYTE *pY = pSrcFrame->GetPlane(YPLANE)->GetAbsolutePelPointer(workarea.x[0], workarea.y[0]);
      //create aligned copy
      BLITLUMA(workarea.pSrc_temp[0], nSrcPitch[0], pY, nSrcPitch_plane[0]);
      //set the to the aligned copy
      workarea.pSrc[0] = workarea.pSrc_temp[0];
      if (chroma)
      {
        workarea.pSrc[1] = pSrcFrame->GetPlane(UPLANE)->GetAbsolutePelPointer(workarea.x[1], workarea.y[1]);
        BLITCHROMA(workarea.pSrc_temp[1], nSrcPitch[1], workarea.pSrc[1], nSrcPitch_plane[1]);
        workarea.pSrc[1] = workarea.pSrc_temp[1];
        workarea.pSrc[2] = pSrcFrame->GetPlane(VPLANE)->GetAbsolutePelPointer(workarea.x[2], workarea.y[2]);
        BLITCHROMA(workarea.pSrc_temp[2], nSrcPitch[2], workarea.pSrc[2], nSrcPitch_plane[2]);
        workarea.pSrc[2] = workarea.pSrc_temp[2];
      }
#else	// ALIGN_SOURCEBLOCK
      workarea.pSrc[0] = pSrcFrame->GetPlane(YPLANE)->GetAbsolutePelPointer(workarea.x[0], workarea.y[0]);
      if (chroma)
      {
        workarea.pSrc[1] = pSrcFrame->GetPlane(UPLANE)->GetAbsolutePelPointer(workarea.x[1], workarea.y[1]);
        workarea.pSrc[2] = pSrcFrame->GetPlane(VPLANE)->GetAbsolutePelPointer(workarea.x[2], workarea.y[2]);
      }
#endif	// ALIGN_SOURCEBLOCK

      if (workarea.blky == workarea.blky_beg)
      {
        workarea.nLambda = 0;
      }
      else
      {
        workarea.nLambda = _lambda_level;
      }

      penaltyNew = _pnew; // penalty for new vector
      LSAD = _lsad;    // SAD limit for lambda using
      // may be they must be scaled by nPel ?

      // decreased padding of coarse levels
      int nHPaddingScaled = pSrcFrame->GetPlane(YPLANE)->GetHPadding() >> nLogScale;
      int nVPaddingScaled = pSrcFrame->GetPlane(YPLANE)->GetVPadding() >> nLogScale;
      /* computes search boundaries */
      workarea.nDxMax = nPel * (pSrcFrame->GetPlane(YPLANE)->GetExtendedWidth() - workarea.x[0] - nBlkSizeX - pSrcFrame->GetPlane(YPLANE)->GetHPadding() + nHPaddingScaled);
      workarea.nDyMax = nPel * (pSrcFrame->GetPlane(YPLANE)->GetExtendedHeight() - workarea.y[0] - nBlkSizeY - pSrcFrame->GetPlane(YPLANE)->GetVPadding() + nVPaddingScaled);
      workarea.nDxMin = -nPel * (workarea.x[0] - pSrcFrame->GetPlane(YPLANE)->GetHPadding() + nHPaddingScaled);
      workarea.nDyMin = -nPel * (workarea.y[0] - pSrcFrame->GetPlane(YPLANE)->GetVPadding() + nVPaddingScaled);

      /* search the mv */
      workarea.predictor = ClipMV(workarea, vectors[workarea.blkIdx]);
      if (temporal)
      {
        workarea.predictors[4] = ClipMV(workarea, *reinterpret_cast<VECTOR*>(&_vecPrev[workarea.blkIdx*N_PER_BLOCK])); // temporal predictor
      }
      else
      {
        workarea.predictors[4] = ClipMV(workarea, zeroMV);
      }

      PseudoEPZSearch<pixel_t>(workarea);
      //			workarea.bestMV = zeroMV; // debug

      if (outfilebuf != NULL) // write vector to outfile
      {
        outfilebuf[workarea.blkx * 4 + 0] = workarea.bestMV.x;
        outfilebuf[workarea.blkx * 4 + 1] = workarea.bestMV.y;
        outfilebuf[workarea.blkx * 4 + 2] = (*(uint32_t *)(&workarea.bestMV.sad) & 0x0000ffff); // low word
        outfilebuf[workarea.blkx * 4 + 3] = (*(uint32_t *)(&workarea.bestMV.sad) >> 16);     // high word, usually null
      }

      /* write the results */
      pBlkData[workarea.blkx*N_PER_BLOCK + 0] = workarea.bestMV.x;
      pBlkData[workarea.blkx*N_PER_BLOCK + 1] = workarea.bestMV.y;
      pBlkData[workarea.blkx*N_PER_BLOCK + 2] = *(uint32_t *)(&workarea.bestMV.sad);

      PROFILE_STOP(MOTION_PROFILE_ME);


      if (smallestPlane)
      {
        /*
        int64_t i64_1 = 0;
        int64_t i64_2 = 0;
        int32_t i32 = 0;
        unsigned int a1 = 200;
        unsigned int a2 = 201;

        i64_1 += a1 - a2; // 0x00000000 FFFFFFFF   !!!!!
        i64_2 = i64_2 + a1 - a2; // 0xFFFFFFFF FFFFFFFF O.K.!
        i32 += a1 - a2; // 0xFFFFFFFF
        */

        // int64_t += uint32_t - uint32_t is not ok, if diff would be negative
        // LUMA diff can be negative! we should cast from uint32_t
        // 64 bit cast or else: int64_t += uint32t - uint32_t results in int64_t += (uint32_t)(uint32t - uint32_t)
        // which is baaaad 0x00000000 FFFFFFFF instead of 0xFFFFFFFF FFFFFFFF

        // 161204 todo check: why is it not abs(lumadiff)?
        typedef typename std::conditional < sizeof(pixel_t) == 1, sad_t, bigsad_t >::type safe_sad_t;
        workarea.sumLumaChange += (safe_sad_t)LUMA(GetRefBlock(workarea, 0, 0), nRefPitch[0]) - (safe_sad_t)LUMA(workarea.pSrc[0], nSrcPitch[0]);
      }

      /* increment indexes & pointers */
      if (iblkx < nBlkX - 1)
      {
        workarea.x[0] += nBlkSizeX_Ovr[0]*workarea.blkScanDir;
        workarea.x[1] += nBlkSizeX_Ovr[1]*workarea.blkScanDir;
        workarea.x[2] += nBlkSizeX_Ovr[2]*workarea.blkScanDir;
      }
    }	// for iblkx

    pBlkData += nBlkX*N_PER_BLOCK;
    if (outfilebuf != NULL) // write vector to outfile
    {
      outfilebuf += nBlkX * 4;// 4 short word per block
    }

    workarea.y[0] += nBlkSizeY_Ovr[0];
    workarea.y[1] += nBlkSizeY_Ovr[1];
    workarea.y[2] += nBlkSizeY_Ovr[2];
  }	// for workarea.blky

  planeSAD += workarea.planeSAD; // PF todo check int overflow
  sumLumaChange += workarea.sumLumaChange;

  if (isse)
  {
#ifndef _M_X64
    _mm_empty();
#endif
  }

#ifdef ALLOW_DCT
  if (_dct_pool_ptr != 0)
  {
    _dct_pool_ptr->return_obj(*(workarea.DCT));
    workarea.DCT = 0;
  }
#endif

  _workarea_pool.return_obj(workarea);
}



template<typename pixel_t>
void	PlaneOfBlocks::recalculate_mv_slice(Slicer::TaskData &td)
{
  assert(&td != 0);

  short *			outfilebuf = _outfilebuf;

  WorkingArea &	workarea = *(_workarea_pool.take_obj());
  assert(&workarea != 0);

  workarea.blky_beg = td._y_beg;
  workarea.blky_end = td._y_end;

  workarea.DCT = 0;
#ifdef ALLOW_DCT
  if (_dct_pool_ptr != 0)
  {
    workarea.DCT = _dct_pool_ptr->take_obj();
  }
#endif	// ALLOW_DCT
  workarea.globalMVPredictor = _glob_mv_pred_def;

  int *pBlkData = _out + 1 + workarea.blky_beg * nBlkX*N_PER_BLOCK;
  if (outfilebuf != NULL)
  {
    outfilebuf += workarea.blky_beg * nBlkX * 4;// 4 short word per block
  }

  workarea.y[0] = pSrcFrame->GetPlane(YPLANE)->GetVPadding();
  workarea.y[0] += workarea.blky_beg * (nBlkSizeY - nOverlapY);
  if (chroma)
  {
    workarea.y[1] = pSrcFrame->GetPlane(UPLANE)->GetVPadding();
    workarea.y[2] = pSrcFrame->GetPlane(VPLANE)->GetVPadding();
    workarea.y[1] += workarea.blky_beg * ((nBlkSizeY - nOverlapY) >> nLogyRatioUV);
    workarea.y[2] += workarea.blky_beg * ((nBlkSizeY - nOverlapY) >> nLogyRatioUV);
  }

  workarea.planeSAD = 0;
  workarea.sumLumaChange = 0;

  // get old vectors plane
  const FakePlaneOfBlocks &plane = _mv_clip_ptr->GetPlane(0);
  int nBlkXold = plane.GetReducedWidth();
  int nBlkYold = plane.GetReducedHeight();
  int nBlkSizeXold = plane.GetBlockSizeX();
  int nBlkSizeYold = plane.GetBlockSizeY();
  int nOverlapXold = plane.GetOverlapX();
  int nOverlapYold = plane.GetOverlapY();
  int nStepXold = nBlkSizeXold - nOverlapXold;
  int nStepYold = nBlkSizeYold - nOverlapYold;
  int nPelold = plane.GetPel();
  int nLogPelold = ilog2(nPelold);

  int nBlkSizeXoldMulYold = nBlkSizeXold * nBlkSizeYold;
  int nBlkSizeXMulY = nBlkSizeX *nBlkSizeY;

  int nBlkSizeX_Ovr[3] = { (nBlkSizeX - nOverlapX), (nBlkSizeX - nOverlapX) >> nLogxRatioUV, (nBlkSizeX - nOverlapX) >> nLogxRatioUV };
  int nBlkSizeY_Ovr[3] = { (nBlkSizeY - nOverlapY), (nBlkSizeY - nOverlapY) >> nLogyRatioUV, (nBlkSizeY - nOverlapY) >> nLogyRatioUV };

  // 2.7.19.22
  // 32 bit safe: max_sad * (nBlkSizeX*nBlkSizeY) < 0x7FFFFFFF -> (3_planes*nBlkSizeX*nBlkSizeY*max_pixel_value) * (nBlkSizeX*nBlkSizeY) < 0x7FFFFFFF
  // 8 bit: nBlkSizeX*nBlkSizeY < sqrt(0x7FFFFFFF / 3 / 255), that is < sqrt(1675), above approx 40x40 is not OK even in 8 bits
  bool safeBlockAreaFor32bitCalc = nBlkSizeXMulY < 1675;

  // Functions using float must not be used here
  for (workarea.blky = workarea.blky_beg; workarea.blky < workarea.blky_end; workarea.blky++)
  {
    workarea.blkScanDir = (workarea.blky % 2 == 0 || !_meander_flag) ? 1 : -1;
    // meander (alternate) scan blocks (even row left to right, odd row right to left)
    int blkxStart = (workarea.blky % 2 == 0 || !_meander_flag) ? 0 : nBlkX - 1;
    if (workarea.blkScanDir == 1) // start with leftmost block
    {
      workarea.x[0] = pSrcFrame->GetPlane(YPLANE)->GetHPadding();
      if (chroma)
      {
        workarea.x[1] = pSrcFrame->GetPlane(UPLANE)->GetHPadding();
        workarea.x[2] = pSrcFrame->GetPlane(VPLANE)->GetHPadding();
      }
    }
    else // start with rightmost block, but it is already set at prev row
    {
      workarea.x[0] = pSrcFrame->GetPlane(YPLANE)->GetHPadding() + nBlkSizeX_Ovr[0]*(nBlkX - 1);
      if (chroma)
      {
        workarea.x[1] = pSrcFrame->GetPlane(UPLANE)->GetHPadding() + nBlkSizeX_Ovr[1]*(nBlkX - 1);
        workarea.x[2] = pSrcFrame->GetPlane(VPLANE)->GetHPadding() + nBlkSizeX_Ovr[2]*(nBlkX - 1);
      }
    }

    for (int iblkx = 0; iblkx < nBlkX; iblkx++)
    {
      workarea.blkx = blkxStart + iblkx*workarea.blkScanDir;
      workarea.blkIdx = workarea.blky*nBlkX + workarea.blkx;
      //		DebugPrintf("BlkIdx = %d \n", workarea.blkIdx);
      PROFILE_START(MOTION_PROFILE_ME);

#if (ALIGN_SOURCEBLOCK > 1)
      //store the pitch
      workarea.pSrc[0] = pSrcFrame->GetPlane(YPLANE)->GetAbsolutePelPointer(workarea.x[0], workarea.y[0]);
      //create aligned copy
      BLITLUMA(workarea.pSrc_temp[0], nSrcPitch[0], workarea.pSrc[0], nSrcPitch_plane[0]);
      //set the to the aligned copy
      workarea.pSrc[0] = workarea.pSrc_temp[0];
      if (chroma)
      {
        workarea.pSrc[1] = pSrcFrame->GetPlane(UPLANE)->GetAbsolutePelPointer(workarea.x[1], workarea.y[1]);
        workarea.pSrc[2] = pSrcFrame->GetPlane(VPLANE)->GetAbsolutePelPointer(workarea.x[2], workarea.y[2]);
        BLITCHROMA(workarea.pSrc_temp[1], nSrcPitch[1], workarea.pSrc[1], nSrcPitch_plane[1]);
        BLITCHROMA(workarea.pSrc_temp[2], nSrcPitch[2], workarea.pSrc[2], nSrcPitch_plane[2]);
        workarea.pSrc[1] = workarea.pSrc_temp[1];
        workarea.pSrc[2] = workarea.pSrc_temp[2];
      }
#else	// ALIGN_SOURCEBLOCK
      workarea.pSrc[0] = pSrcFrame->GetPlane(YPLANE)->GetAbsolutePelPointer(workarea.x[0], workarea.y[0]);
      if (chroma)
      {
        workarea.pSrc[1] = pSrcFrame->GetPlane(UPLANE)->GetAbsolutePelPointer(workarea.x[1], workarea.y[1]);
        workarea.pSrc[2] = pSrcFrame->GetPlane(VPLANE)->GetAbsolutePelPointer(workarea.x[2], workarea.y[2]);
      }
#endif	// ALIGN_SOURCEBLOCK

      if (workarea.blky == workarea.blky_beg)
      {
        workarea.nLambda = 0;
      }
      else
      {
        workarea.nLambda = _lambda_level;
      }

      penaltyNew = _pnew; // penalty for new vector
      LSAD = _lsad;    // SAD limit for lambda using
      // may be they must be scaled by nPel ?

      /* computes search boundaries */
      workarea.nDxMax = nPel * (pSrcFrame->GetPlane(YPLANE)->GetExtendedWidth() - workarea.x[0] - nBlkSizeX);
      workarea.nDyMax = nPel * (pSrcFrame->GetPlane(YPLANE)->GetExtendedHeight() - workarea.y[0] - nBlkSizeY);
      workarea.nDxMin = -nPel * workarea.x[0];
      workarea.nDyMin = -nPel * workarea.y[0];

      // get and interplolate old vectors
      int centerX = nBlkSizeX / 2 + (nBlkSizeX - nOverlapX)*workarea.blkx; // center of new block
      int blkxold = (centerX - nBlkSizeXold / 2) / nStepXold; // centerXold less or equal to new
      int centerY = nBlkSizeY / 2 + (nBlkSizeY - nOverlapY)*workarea.blky;
      int blkyold = (centerY - nBlkSizeYold / 2) / nStepYold;

      int deltaX = std::max(0, centerX - (nBlkSizeXold / 2 + nStepXold*blkxold)); // distance from old to new
      int deltaY = std::max(0, centerY - (nBlkSizeYold / 2 + nStepYold*blkyold));

      int blkxold1 = std::min(nBlkXold - 1, std::max(0, blkxold));
      int blkxold2 = std::min(nBlkXold - 1, std::max(0, blkxold + 1));
      int blkyold1 = std::min(nBlkYold - 1, std::max(0, blkyold));
      int blkyold2 = std::min(nBlkYold - 1, std::max(0, blkyold + 1));

      VECTOR vectorOld; // interpolated or nearest

      typedef typename std::conditional < sizeof(pixel_t) == 1, sad_t, bigsad_t >::type safe_sad_t;

      if (_smooth == 1) // interpolate
      {
        VECTOR vectorOld1 = _mv_clip_ptr->GetBlock(0, blkxold1 + blkyold1*nBlkXold).GetMV(); // 4 old nearest vectors (may coinside)
        VECTOR vectorOld2 = _mv_clip_ptr->GetBlock(0, blkxold2 + blkyold1*nBlkXold).GetMV();
        VECTOR vectorOld3 = _mv_clip_ptr->GetBlock(0, blkxold1 + blkyold2*nBlkXold).GetMV();
        VECTOR vectorOld4 = _mv_clip_ptr->GetBlock(0, blkxold2 + blkyold2*nBlkXold).GetMV();

        // interpolate
        int vector1_x = vectorOld1.x*nStepXold + deltaX*(vectorOld2.x - vectorOld1.x); // scaled by nStepXold to skip slow division
        int vector1_y = vectorOld1.y*nStepXold + deltaX*(vectorOld2.y - vectorOld1.y);
        safe_sad_t vector1_sad = (safe_sad_t)vectorOld1.sad*nStepXold + deltaX*((safe_sad_t)vectorOld2.sad - vectorOld1.sad);

        int vector2_x = vectorOld3.x*nStepXold + deltaX*(vectorOld4.x - vectorOld3.x);
        int vector2_y = vectorOld3.y*nStepXold + deltaX*(vectorOld4.y - vectorOld3.y);
        safe_sad_t vector2_sad = (safe_sad_t)vectorOld3.sad*nStepXold + deltaX*((safe_sad_t)vectorOld4.sad - vectorOld3.sad);

        vectorOld.x = (vector1_x + deltaY*(vector2_x - vector1_x) / nStepYold) / nStepXold;
        vectorOld.y = (vector1_y + deltaY*(vector2_y - vector1_y) / nStepYold) / nStepXold;
        vectorOld.sad = (sad_t)((vector1_sad + deltaY*(vector2_sad - vector1_sad) / nStepYold) / nStepXold);
      }

      else // nearest
      {
        if (deltaX * 2 < nStepXold && deltaY * 2 < nStepYold)
        {
          vectorOld = _mv_clip_ptr->GetBlock(0, blkxold1 + blkyold1*nBlkXold).GetMV();
        }
        else if (deltaX * 2 >= nStepXold && deltaY * 2 < nStepYold)
        {
          vectorOld = _mv_clip_ptr->GetBlock(0, blkxold2 + blkyold1*nBlkXold).GetMV();
        }
        else if (deltaX * 2 < nStepXold && deltaY * 2 >= nStepYold)
        {
          vectorOld = _mv_clip_ptr->GetBlock(0, blkxold1 + blkyold2*nBlkXold).GetMV();
        }
        else //(deltaX*2>=nStepXold && deltaY*2>=nStepYold )
        {
          vectorOld = _mv_clip_ptr->GetBlock(0, blkxold2 + blkyold2*nBlkXold).GetMV();
        }
      }

      // scale vector to new nPel
      vectorOld.x = (vectorOld.x << nLogPel) >> nLogPelold;
      vectorOld.y = (vectorOld.y << nLogPel) >> nLogPelold;

      workarea.predictor = ClipMV(workarea, vectorOld); // predictor
      if(safeBlockAreaFor32bitCalc && sizeof(pixel_t)==1)
        workarea.predictor.sad = (sad_t)((safe_sad_t)vectorOld.sad * nBlkSizeXMulY / nBlkSizeXoldMulYold); // normalized to new block size
      else // 16 bit or unsafe blocksize
        workarea.predictor.sad = (sad_t)((bigsad_t)vectorOld.sad * nBlkSizeXMulY / nBlkSizeXoldMulYold); // normalized to new block size

//			workarea.bestMV = workarea.predictor; // by pointer?
      workarea.bestMV.x = workarea.predictor.x;
      workarea.bestMV.y = workarea.predictor.y;
      workarea.bestMV.sad = workarea.predictor.sad;

      // update SAD
#ifdef ALLOW_DCT
      if (dctmode != 0) // DCT method (luma only - currently use normal spatial SAD chroma)
      {
        // make dct of source block
        if (dctmode <= 4) //don't do the slow dct conversion if SATD used
        {
          workarea.DCT->DCTBytes2D(workarea.pSrc[0], nSrcPitch[0], &workarea.dctSrc[0], dctpitch);
        }
      }
      if (dctmode >= 3) // most use it and it should be fast anyway //if (dctmode == 3 || dctmode == 4) // check it
      {
        workarea.srcLuma = LUMA(workarea.pSrc[0], nSrcPitch[0]);
      }
#endif	// ALLOW_DCT

      sad_t saduv = (chroma) ? ScaleSadChroma(SADCHROMA(workarea.pSrc[1], nSrcPitch[1], GetRefBlockU(workarea, workarea.predictor.x, workarea.predictor.y), nRefPitch[1])
        + SADCHROMA(workarea.pSrc[2], nSrcPitch[2], GetRefBlockV(workarea, workarea.predictor.x, workarea.predictor.y), nRefPitch[2]), effective_chromaSADscale) : 0;
      sad_t sad = LumaSAD<pixel_t>(workarea, GetRefBlock(workarea, workarea.predictor.x, workarea.predictor.y));
      sad += saduv;
      workarea.bestMV.sad = sad;
      workarea.nMinCost = sad;

      if (workarea.bestMV.sad > _thSAD)// if old interpolated vector is bad
      {
        //				CheckMV(vectorOld1.x, vectorOld1.y);
        //				CheckMV(vectorOld2.x, vectorOld2.y);
        //				CheckMV(vectorOld3.x, vectorOld3.y);
        //				CheckMV(vectorOld4.x, vectorOld4.y);
                // then, we refine, according to the search type

        // todo PF: consider switch and not bitfield searchType
        if (searchType & ONETIME)
        {
          for (int i = nSearchParam; i > 0; i /= 2)
          {
            OneTimeSearch<pixel_t>(workarea, i);
          }
        }

        if (searchType & NSTEP)
        {
          NStepSearch<pixel_t>(workarea, nSearchParam);
        }

        if (searchType & LOGARITHMIC)
        {
          for (int i = nSearchParam; i > 0; i /= 2)
          {
            DiamondSearch<pixel_t>(workarea, i);
          }
        }

        if (searchType & EXHAUSTIVE)
        {
          //       ExhaustiveSearch(nSearchParam);
          int mvx = workarea.bestMV.x;
          int mvy = workarea.bestMV.y;
          for (int i = 1; i <= nSearchParam; i++)// region is same as exhaustive, but ordered by radius (from near to far)
          {
            ExpandingSearch<pixel_t>(workarea, i, 1, mvx, mvy);
          }
        }

        if (searchType & HEX2SEARCH)
        {
          Hex2Search<pixel_t>(workarea, nSearchParam);
        }

        if (searchType & UMHSEARCH)
        {
          UMHSearch<pixel_t>(workarea, nSearchParam, workarea.bestMV.x, workarea.bestMV.y);
        }

        if (searchType & HSEARCH)
        {
          int mvx = workarea.bestMV.x;
          int mvy = workarea.bestMV.y;
          for (int i = 1; i <= nSearchParam; i++)// region is same as exhaustive, but ordered by radius (from near to far)
          {
            CheckMV<pixel_t>(workarea, mvx - i, mvy);
            CheckMV<pixel_t>(workarea, mvx + i, mvy);
          }
        }

        if (searchType & VSEARCH)
        {
          int mvx = workarea.bestMV.x;
          int mvy = workarea.bestMV.y;
          for (int i = 1; i <= nSearchParam; i++)// region is same as exhaustive, but ordered by radius (from near to far)
          {
            CheckMV<pixel_t>(workarea, mvx, mvy - i);
            CheckMV<pixel_t>(workarea, mvx, mvy + i);
          }
        }
      }	// if bestMV.sad > thSAD

      // we store the result
      vectors[workarea.blkIdx].x = workarea.bestMV.x;
      vectors[workarea.blkIdx].y = workarea.bestMV.y;
      vectors[workarea.blkIdx].sad = workarea.bestMV.sad;

      if (outfilebuf != NULL) // write vector to outfile
      {
        outfilebuf[workarea.blkx * 4 + 0] = workarea.bestMV.x;
        outfilebuf[workarea.blkx * 4 + 1] = workarea.bestMV.y;
        // todo: SAD as int when it is float
        outfilebuf[workarea.blkx * 4 + 2] = (workarea.bestMV.sad & 0x0000ffff); // low word
        outfilebuf[workarea.blkx * 4 + 3] = (workarea.bestMV.sad >> 16);     // high word, usually null
      }

      /* write the results */
      pBlkData[workarea.blkx*N_PER_BLOCK + 0] = workarea.bestMV.x;
      pBlkData[workarea.blkx*N_PER_BLOCK + 1] = workarea.bestMV.y;
      pBlkData[workarea.blkx*N_PER_BLOCK + 2] = workarea.bestMV.sad;


      PROFILE_STOP(MOTION_PROFILE_ME);

      if (smallestPlane)
      {
        // int64_t += uint32_t - uint32_t is not ok, if diff would be negative
        // 161204 todo check: why is it not abs(lumadiff)?
        workarea.sumLumaChange += (safe_sad_t)LUMA(GetRefBlock(workarea, 0, 0), nRefPitch[0]) - (safe_sad_t)LUMA(workarea.pSrc[0], nSrcPitch[0]);
      }

      if (iblkx < nBlkX - 1)
      {
        workarea.x[0] += nBlkSizeX_Ovr[0]*workarea.blkScanDir;
        workarea.x[1] += nBlkSizeX_Ovr[1]*workarea.blkScanDir;
        workarea.x[2] += nBlkSizeX_Ovr[2]*workarea.blkScanDir;
      }
    }	// for workarea.blkx

    pBlkData += nBlkX*N_PER_BLOCK;
    if (outfilebuf != NULL) // write vector to outfile
    {
      outfilebuf += nBlkX * 4;// 4 short word per block
    }

    workarea.y[0] += nBlkSizeY_Ovr[0];
    workarea.y[1] += nBlkSizeY_Ovr[1];
    workarea.y[2] += nBlkSizeY_Ovr[2];
  }	// for workarea.blky

  planeSAD += workarea.planeSAD;
  sumLumaChange += workarea.sumLumaChange;

  if (isse)
  {
#ifndef _M_X64
    _mm_empty();
#endif
  }

#ifdef ALLOW_DCT
  if (_dct_pool_ptr != 0)
  {
    _dct_pool_ptr->return_obj(*(workarea.DCT));
    workarea.DCT = 0;
  }
#endif

  _workarea_pool.return_obj(workarea);
}



// -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -



PlaneOfBlocks::WorkingArea::WorkingArea(int nBlkSizeX, int nBlkSizeY, int dctpitch, int nLogxRatioUV, int nLogyRatioUV, int _pixelsize, int _bits_per_pixel)
  : dctSrc(nBlkSizeY*dctpitch) // dctpitch is pixelsize aware
  , dctRef(nBlkSizeY*dctpitch)
  , DCT(0)
  , pixelsize(_pixelsize)
  , bits_per_pixel(_bits_per_pixel)
{
#if (ALIGN_SOURCEBLOCK > 1)
  int xPitch = AlignNumber(nBlkSizeX*pixelsize, ALIGN_SOURCEBLOCK);  // for memory allocation pixelsize needed
  int xPitchUV = AlignNumber((nBlkSizeX*pixelsize) >> nLogxRatioUV, ALIGN_SOURCEBLOCK);
  int blocksize = xPitch*nBlkSizeY;
  int UVblocksize = xPitchUV * (nBlkSizeY >> nLogyRatioUV); // >> nx >> ny
  int sizeAlignedBlock = blocksize + 2 * UVblocksize;
  // int sizeAlignedBlock=blocksize+(ALIGN_SOURCEBLOCK-(blocksize%ALIGN_SOURCEBLOCK))+
  //                         2*((blocksize/2)>>nLogyRatioUV)+(ALIGN_SOURCEBLOCK-(((blocksize/2)/yRatioUV)%ALIGN_SOURCEBLOCK)); // why >>Logy then /y?
  pSrc_temp_base.resize(sizeAlignedBlock);
  pSrc_temp[0] = &pSrc_temp_base[0];
  pSrc_temp[1] = (uint8_t *)(pSrc_temp[0] + blocksize);
  pSrc_temp[2] = (uint8_t *)(pSrc_temp[1] + UVblocksize);
#endif	// ALIGN_SOURCEBLOCK
}



PlaneOfBlocks::WorkingArea::~WorkingArea()
{
  // Nothing
}




/* check if a vector is inside search boundaries */
MV_FORCEINLINE bool	PlaneOfBlocks::WorkingArea::IsVectorOK(int vx, int vy) const
{
  return (
    (vx >= nDxMin)
    && (vy >= nDyMin)
    && (vx < nDxMax)
    && (vy < nDyMax)
    );
}

/* computes the cost of a vector (vx, vy) */
template<typename pixel_t>
sad_t PlaneOfBlocks::WorkingArea::MotionDistorsion(int vx, int vy) const
{
  int dist = SquareDifferenceNorm(predictor, vx, vy);
  if(sizeof(pixel_t) == 1)
    return (nLambda * dist) >> 8; // 8 bit: faster
  else
    return (nLambda * dist) >> (16 - bits_per_pixel) /*8*/; // PF scaling because it appears as a sad addition 
}

/* computes the length cost of a vector (vx, vy) */
//MV_FORCEINLINE int LengthPenalty(int vx, int vy)
//{
//	return ( (vx*vx + vy*vy)*nLambdaLen) >> 8;
//}



PlaneOfBlocks::WorkingAreaFactory::WorkingAreaFactory(int nBlkSizeX, int nBlkSizeY, int dctpitch, int nLogxRatioUV, int xRatioUV, int nLogyRatioUV, int yRatioUV, int pixelsize, int bits_per_pixel)
  : _blk_size_x(nBlkSizeX)
  , _blk_size_y(nBlkSizeY)
  , _dctpitch(dctpitch)
  , _x_ratio_uv_log(nLogxRatioUV)
  , _x_ratio_uv(xRatioUV)
  , _y_ratio_uv_log(nLogyRatioUV)
  , _y_ratio_uv(yRatioUV)
  , _pixelsize(pixelsize)
  , _bits_per_pixel(bits_per_pixel)
{
  // Nothing
}



PlaneOfBlocks::WorkingArea *PlaneOfBlocks::WorkingAreaFactory::do_create()
{
  return (new WorkingArea(
    _blk_size_x,
    _blk_size_y,
    _dctpitch,
    _x_ratio_uv_log,
    _y_ratio_uv_log,
    _pixelsize,
    _bits_per_pixel
  ));
}


