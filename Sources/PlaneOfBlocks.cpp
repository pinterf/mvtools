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


PlaneOfBlocks::PlaneOfBlocks(int _nBlkX, int _nBlkY, int _nBlkSizeX, int _nBlkSizeY, int _nPel, int _nLevel, int _nFlags, int _nOverlapX, int _nOverlapY, int _xRatioUV, int _yRatioUV, int _pixelsize, int _bits_per_pixel, conc::ObjPool <DCTClass> *dct_pool_ptr, bool mt_flag)
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
  , dctpitch(std::max(_nBlkSizeX, 16))
  , _dct_pool_ptr(dct_pool_ptr)
  , verybigSAD(_nBlkSizeX * _nBlkSizeY * (pixelsize == 4 ? 1 : (1 << bits_per_pixel))) // * 256, pixelsize==2 -> 65536. Float:1
  , freqArray()
  , dctmode(0)
  , _workarea_fact(nBlkSizeX, nBlkSizeY, dctpitch, nLogxRatioUV, xRatioUV, nLogyRatioUV, yRatioUV, pixelsize, bits_per_pixel)
  , _workarea_pool()
  , _gvect_estim_ptr(0)
  , _gvect_result_count(0)
{
  _workarea_pool.set_factory(_workarea_fact);

  // half must be more than max vector length, which is (framewidth + Padding) * nPel
  freqArray[0].resize(8192 * _nPel * 2);
  freqArray[1].resize(8192 * _nPel * 2);

  bool sse2 = (bool)(nFlags & CPU_SSE2); // no tricks for really old processors. If SSE2 is reported, use it
  bool sse41 = (bool)(nFlags & CPUF_SSE4_1); 
  bool avx = (bool)(nFlags & CPU_AVX);
  bool avx2 = (bool)(nFlags & CPU_AVX2);
  bool ssd = (bool)(nFlags & MOTION_USE_SSD);
  bool satd = (bool)(nFlags & MOTION_USE_SATD);

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
  LUMA = get_luma_function(nBlkSizeX / xRatioUV, nBlkSizeY / yRatioUV, pixelsize, arch); // variance.h
  SATD = get_satd_function(nBlkSizeX, nBlkSizeY, pixelsize, arch); // P.F. 2.7.0.22d SATD made live
  if (SATD == nullptr)
    SATD = SadDummy;

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
  dctweight16 = std::min((sad_t)16, abs(*pmeanLumaChange) / (nBlkSizeX*nBlkSizeY)); //equal dct and spatial weights for meanLumaChange=8 (empirical)
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
  slicer.start(nBlkY, *this, &PlaneOfBlocks::search_mv_slice, 4);
  slicer.wait();

  // -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -

  if (smallestPlane)
  {
    *pmeanLumaChange = sumLumaChange / nBlkCount; // for all finer planes
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
  slicer.start(nBlkY, *this, &PlaneOfBlocks::recalculate_mv_slice, 4);
  slicer.wait();

  // -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
}



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
      bigsad_t tmp_sad; // 16 bit worst case: 16 * sad_max: 16 * 3x32x32x65536 = 4+5+5+16 > 2^31 over limit
      // in case of BlockSize > 32, e.g. 128x128x65536 is even more: 7+7+16=30 bits

      if (nOverlapX == 0 && nOverlapY == 0)
      {
        vectors[index].x = 9 * v1.x + 3 * v2.x + 3 * v3.x + v4.x;
        vectors[index].y = 9 * v1.y + 3 * v2.y + 3 * v3.y + v4.y;
        tmp_sad = 9 * (bigsad_t)v1.sad + 3 * (bigsad_t)v2.sad + 3 * (bigsad_t)v3.sad + (bigsad_t)v4.sad + 8;
        
      }
      else if (nOverlapX <= (nBlkSizeX >> 1) && nOverlapY <= (nBlkSizeY >> 1)) // corrected in v1.4.11
      {
        int	ax1 = (offx > 0) ? aoddx : aevenx;
        int ax2 = (nBlkSizeX - nOverlapX) * 4 - ax1;
        int ay1 = (offy > 0) ? aoddy : aeveny;
        int ay2 = (nBlkSizeY - nOverlapY) * 4 - ay1;
        int a11 = ax1*ay1, a12 = ax1*ay2, a21 = ax2*ay1, a22 = ax2*ay2;
        vectors[index].x = (a11*v1.x + a21*v2.x + a12*v3.x + a22*v4.x) / normov;
        vectors[index].y = (a11*v1.y + a21*v2.y + a12*v3.y + a22*v4.y) / normov;
        tmp_sad = ((bigsad_t)a11*v1.sad + (bigsad_t)a21*v2.sad + (bigsad_t)a12*v3.sad + (bigsad_t)a22*v4.sad) / normov;
      }
      else // large overlap. Weights are not quite correct but let it be
      {
        vectors[index].x = (v1.x + v2.x + v3.x + v4.x) << 2;
        vectors[index].y = (v1.y + v2.y + v3.y + v4.y) << 2;
        tmp_sad = ((bigsad_t)v1.sad + v2.sad + v3.sad + v4.sad + 2) << 2;
      }
      vectors[index].x = (vectors[index].x >> normFactor) << mulFactor;
      vectors[index].y = (vectors[index].y >> normFactor) << mulFactor;
      vectors[index].sad = (sad_t)(tmp_sad >> 4);
    }	// for k < nBlkX
  }	// for l < nBlkY
}



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
    *(sad_t *)(&array[i + 3]) = verybigSAD; // float or int!!
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
        *(sad_t *)(&array[i + 3]) = verybigSAD; // float or int
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
  workarea.nLambda = workarea.nLambda*(bigsad_t)LSAD / ((bigsad_t)LSAD + (workarea.predictor.sad >> 1))*LSAD / ((bigsad_t)LSAD + (workarea.predictor.sad >> 1));
  // replaced hard threshold by soft in v1.10.2 by Fizick (a liitle complex expression to avoid overflow)
  //	int a = LSAD/(LSAD + (workarea.predictor.sad>>1));
  //	workarea.nLambda = workarea.nLambda*a*a;
}



void PlaneOfBlocks::Refine(WorkingArea &workarea)
{
  // then, we refine, according to the search type
  if (searchType & ONETIME)
  {
    for (int i = nSearchParam; i > 0; i /= 2)
    {
      OneTimeSearch(workarea, i);
    }
  }

  if (searchType & NSTEP)
  {
    NStepSearch(workarea, nSearchParam);
  }

  if (searchType & LOGARITHMIC)
  {
    for (int i = nSearchParam; i > 0; i /= 2)
    {
      DiamondSearch(workarea, i);
    }
  }

  if (searchType & EXHAUSTIVE)
  {
    //		ExhaustiveSearch(nSearchParam);
    int mvx = workarea.bestMV.x;
    int mvy = workarea.bestMV.y;
    for (int i = 1; i <= nSearchParam; i++)// region is same as exhaustive, but ordered by radius (from near to far)
    {
      ExpandingSearch(workarea, i, 1, mvx, mvy);
    }
  }

  //	if ( searchType & SQUARE )
  //	{
  //		SquareSearch();
  //	}

  if (searchType & HEX2SEARCH)
  {
    Hex2Search(workarea, nSearchParam);
  }

  if (searchType & UMHSEARCH)
  {
    UMHSearch(workarea, nSearchParam, workarea.bestMV.x, workarea.bestMV.y);
  }

  if (searchType & HSEARCH)
  {
    int mvx = workarea.bestMV.x;
    int mvy = workarea.bestMV.y;
    for (int i = 1; i <= nSearchParam; i++)// region is same as exhaustive, but ordered by radius (from near to far)
    {
      CheckMV(workarea, mvx - i, mvy);
      CheckMV(workarea, mvx + i, mvy);
    }
  }

  if (searchType & VSEARCH)
  {
    int mvx = workarea.bestMV.x;
    int mvy = workarea.bestMV.y;
    for (int i = 1; i <= nSearchParam; i++)// region is same as exhaustive, but ordered by radius (from near to far)
    {
      CheckMV(workarea, mvx, mvy - i);
      CheckMV(workarea, mvx, mvy + i);
    }
  }
}



void PlaneOfBlocks::PseudoEPZSearch(WorkingArea &workarea)
{
  FetchPredictors(workarea);

  sad_t sad;
  sad_t saduv;
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

  // We treat zero alone
  // Do we bias zero with not taking into account distorsion ?
  workarea.bestMV.x = zeroMVfieldShifted.x;
  workarea.bestMV.y = zeroMVfieldShifted.y;
  saduv = (chroma) ? SADCHROMA(workarea.pSrc[1], nSrcPitch[1], GetRefBlockU(workarea, 0, 0), nRefPitch[1])
    + SADCHROMA(workarea.pSrc[2], nSrcPitch[2], GetRefBlockV(workarea, 0, 0), nRefPitch[2]) : 0;
  sad = LumaSAD(workarea, GetRefBlock(workarea, 0, zeroMVfieldShifted.y));
  sad += saduv;
  workarea.bestMV.sad = sad;
  workarea.nMinCost = sad + ((penaltyZero*(bigsad_t)sad) >> 8); // v.1.11.0.2

  VECTOR bestMVMany[8];
  int nMinCostMany[8];

  if (tryMany)
  {
    //  refine around zero
    Refine(workarea);
    bestMVMany[0] = workarea.bestMV;    // save bestMV
    nMinCostMany[0] = workarea.nMinCost;
  }

  // Global MV predictor  - added by Fizick
  workarea.globalMVPredictor = ClipMV(workarea, workarea.globalMVPredictor);
  //	if ( workarea.IsVectorOK(workarea.globalMVPredictor.x, workarea.globalMVPredictor.y ) )
  {
    saduv = (chroma) ? SADCHROMA(workarea.pSrc[1], nSrcPitch[1], GetRefBlockU(workarea, workarea.globalMVPredictor.x, workarea.globalMVPredictor.y), nRefPitch[1])
      + SADCHROMA(workarea.pSrc[2], nSrcPitch[2], GetRefBlockV(workarea, workarea.globalMVPredictor.x, workarea.globalMVPredictor.y), nRefPitch[2]) : 0;
    sad = LumaSAD(workarea, GetRefBlock(workarea, workarea.globalMVPredictor.x, workarea.globalMVPredictor.y));
    sad += saduv;
    sad_t cost = sad + ((pglobal*(bigsad_t)sad) >> 8);

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
      Refine(workarea);    // reset bestMV
      bestMVMany[1] = workarea.bestMV;    // save bestMV
      nMinCostMany[1] = workarea.nMinCost;
    }
    //	}
    //	Then, the predictor :
    //	if (   (( workarea.predictor.x != zeroMVfieldShifted.x ) || ( workarea.predictor.y != zeroMVfieldShifted.y ))
    //	    && (( workarea.predictor.x != workarea.globalMVPredictor.x ) || ( workarea.predictor.y != workarea.globalMVPredictor.y )))
    //	{
    saduv = (chroma) ? SADCHROMA(workarea.pSrc[1], nSrcPitch[1], GetRefBlockU(workarea, workarea.predictor.x, workarea.predictor.y), nRefPitch[1])
      + SADCHROMA(workarea.pSrc[2], nSrcPitch[2], GetRefBlockV(workarea, workarea.predictor.x, workarea.predictor.y), nRefPitch[2]) : 0;
    sad = LumaSAD(workarea, GetRefBlock(workarea, workarea.predictor.x, workarea.predictor.y));
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
    Refine(workarea);    // reset bestMV
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
    CheckMV0(workarea, workarea.predictors[i].x, workarea.predictors[i].y);
    if (tryMany)
    {
      // refine around predictor
      Refine(workarea);    // reset bestMV
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
    Refine(workarea);
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
        UMHSearch(workarea, badrange*nPel, 0, 0);
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
        ExpandingSearch(workarea, i, nPel, 0, 0);
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
      ExpandingSearch(workarea, i, 1, mvx, mvy);
    }
    DebugPrintf("best blk=%d x=%d y=%d sad=%d iter=%d", workarea.blkIdx, workarea.bestMV.x, workarea.bestMV.y, workarea.bestMV.sad, workarea.iter);
  }	// bad vector, try wide search

  // we store the result
  vectors[workarea.blkIdx].x = workarea.bestMV.x;
  vectors[workarea.blkIdx].y = workarea.bestMV.y;
  vectors[workarea.blkIdx].sad = workarea.bestMV.sad;

  workarea.planeSAD += workarea.bestMV.sad; // todo PF check int overflow
}



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
    if (lastDirection & 1) CheckMV2(workarea, dx + length, dy, &direction, 1);
    if (lastDirection & 2) CheckMV2(workarea, dx - length, dy, &direction, 2);
    if (lastDirection & 4) CheckMV2(workarea, dx, dy + length, &direction, 4);
    if (lastDirection & 8) CheckMV2(workarea, dx, dy - length, &direction, 8);

    // If one of the directions improves the SAD, we make further tests
    // on the diagonals
    if (direction)
    {
      lastDirection = direction;
      dx = workarea.bestMV.x;
      dy = workarea.bestMV.y;

      if (lastDirection & 3)
      {
        CheckMV2(workarea, dx, dy + length, &direction, 4);
        CheckMV2(workarea, dx, dy - length, &direction, 8);
      }
      else
      {
        CheckMV2(workarea, dx + length, dy, &direction, 1);
        CheckMV2(workarea, dx - length, dy, &direction, 2);
      }
    }

    // If not, we do not stop here. We infer from the last direction the
    // diagonals to be checked, because we might be lucky.
    else
    {
      switch (lastDirection)
      {
      case 1:
        CheckMV2(workarea, dx + length, dy + length, &direction, 1 + 4);
        CheckMV2(workarea, dx + length, dy - length, &direction, 1 + 8);
        break;
      case 2:
        CheckMV2(workarea, dx - length, dy + length, &direction, 2 + 4);
        CheckMV2(workarea, dx - length, dy - length, &direction, 2 + 8);
        break;
      case 4:
        CheckMV2(workarea, dx + length, dy + length, &direction, 1 + 4);
        CheckMV2(workarea, dx - length, dy + length, &direction, 2 + 4);
        break;
      case 8:
        CheckMV2(workarea, dx + length, dy - length, &direction, 1 + 8);
        CheckMV2(workarea, dx - length, dy - length, &direction, 2 + 8);
        break;
      case 1 + 4:
        CheckMV2(workarea, dx + length, dy + length, &direction, 1 + 4);
        CheckMV2(workarea, dx - length, dy + length, &direction, 2 + 4);
        CheckMV2(workarea, dx + length, dy - length, &direction, 1 + 8);
        break;
      case 2 + 4:
        CheckMV2(workarea, dx + length, dy + length, &direction, 1 + 4);
        CheckMV2(workarea, dx - length, dy + length, &direction, 2 + 4);
        CheckMV2(workarea, dx - length, dy - length, &direction, 2 + 8);
        break;
      case 1 + 8:
        CheckMV2(workarea, dx + length, dy + length, &direction, 1 + 4);
        CheckMV2(workarea, dx - length, dy - length, &direction, 2 + 8);
        CheckMV2(workarea, dx + length, dy - length, &direction, 1 + 8);
        break;
      case 2 + 8:
        CheckMV2(workarea, dx - length, dy - length, &direction, 2 + 8);
        CheckMV2(workarea, dx - length, dy + length, &direction, 2 + 4);
        CheckMV2(workarea, dx + length, dy - length, &direction, 1 + 8);
        break;
      default:
        // Even the default case may happen, in the first step of the
        // algorithm for example.
        CheckMV2(workarea, dx + length, dy + length, &direction, 1 + 4);
        CheckMV2(workarea, dx - length, dy + length, &direction, 2 + 4);
        CheckMV2(workarea, dx + length, dy - length, &direction, 1 + 8);
        CheckMV2(workarea, dx - length, dy - length, &direction, 2 + 8);
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



void PlaneOfBlocks::NStepSearch(WorkingArea &workarea, int stp)
{
  int dx, dy;
  int length = stp;
  while (length > 0)
  {
    dx = workarea.bestMV.x;
    dy = workarea.bestMV.y;

    CheckMV(workarea, dx + length, dy + length);
    CheckMV(workarea, dx + length, dy);
    CheckMV(workarea, dx + length, dy - length);
    CheckMV(workarea, dx, dy - length);
    CheckMV(workarea, dx, dy + length);
    CheckMV(workarea, dx - length, dy + length);
    CheckMV(workarea, dx - length, dy);
    CheckMV(workarea, dx - length, dy - length);

    length--;
  }
}



void PlaneOfBlocks::OneTimeSearch(WorkingArea &workarea, int length)
{
  int direction = 0;
  int dx = workarea.bestMV.x;
  int dy = workarea.bestMV.y;

  CheckMV2(workarea, dx - length, dy, &direction, 2);
  CheckMV2(workarea, dx + length, dy, &direction, 1);

  if (direction == 1)
  {
    while (direction)
    {
      direction = 0;
      dx += length;
      CheckMV2(workarea, dx + length, dy, &direction, 1);
    }
  }
  else if (direction == 2)
  {
    while (direction)
    {
      direction = 0;
      dx -= length;
      CheckMV2(workarea, dx - length, dy, &direction, 1);
    }
  }

  CheckMV2(workarea, dx, dy - length, &direction, 2);
  CheckMV2(workarea, dx, dy + length, &direction, 1);

  if (direction == 1)
  {
    while (direction)
    {
      direction = 0;
      dy += length;
      CheckMV2(workarea, dx, dy + length, &direction, 1);
    }
  }
  else if (direction == 2)
  {
    while (direction)
    {
      direction = 0;
      dy -= length;
      CheckMV2(workarea, dx, dy - length, &direction, 1);
    }
  }
}



void PlaneOfBlocks::ExpandingSearch(WorkingArea &workarea, int r, int s, int mvx, int mvy) // diameter = 2*r + 1, step=s
{ // part of true enhaustive search (thin expanding square) around mvx, mvy
  int i, j;
  //	VECTOR mv = workarea.bestMV; // bug: it was pointer assignent, not values, so iterative! - v2.1

    // sides of square without corners
  for (i = -r + s; i < r; i += s) // without corners! - v2.1
  {
    CheckMV(workarea, mvx + i, mvy - r);
    CheckMV(workarea, mvx + i, mvy + r);
  }

  for (j = -r + s; j < r; j += s)
  {
    CheckMV(workarea, mvx - r, mvy + j);
    CheckMV(workarea, mvx + r, mvy + j);
  }

  // then corners - they are more far from cenrer
  CheckMV(workarea, mvx - r, mvy - r);
  CheckMV(workarea, mvx - r, mvy + r);
  CheckMV(workarea, mvx + r, mvy - r);
  CheckMV(workarea, mvx + r, mvy + r);
}



/* (x-1)%6 */
static const int mod6m1[8] = { 5,0,1,2,3,4,5,0 };
/* radius 2 hexagon. repeated entries are to avoid having to compute mod6 every time. */
static const int hex2[8][2] = { {-1,-2}, {-2,0}, {-1,2}, {1,2}, {2,0}, {1,-2}, {-1,-2}, {-2,0} };

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
    CheckMVdir(workarea, bmx - 2, bmy, &dir, 0);
    CheckMVdir(workarea, bmx - 1, bmy + 2, &dir, 1);
    CheckMVdir(workarea, bmx + 1, bmy + 2, &dir, 2);
    CheckMVdir(workarea, bmx + 2, bmy, &dir, 3);
    CheckMVdir(workarea, bmx + 1, bmy - 2, &dir, 4);
    CheckMVdir(workarea, bmx - 1, bmy - 2, &dir, 5);


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

        CheckMVdir(workarea, bmx + hex2[odir + 0][0], bmy + hex2[odir + 0][1], &dir, odir - 1);
        CheckMVdir(workarea, bmx + hex2[odir + 1][0], bmy + hex2[odir + 1][1], &dir, odir);
        CheckMVdir(workarea, bmx + hex2[odir + 2][0], bmy + hex2[odir + 2][1], &dir, odir + 1);
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
  ExpandingSearch(workarea, 1, 1, bmx, bmy);
}



void PlaneOfBlocks::CrossSearch(WorkingArea &workarea, int start, int x_max, int y_max, int mvx, int mvy)
{
  // part of umh  search
  for (int i = start; i < x_max; i += 2)
  {
    CheckMV(workarea, mvx - i, mvy);
    CheckMV(workarea, mvx + i, mvy);
  }

  for (int j = start; j < y_max; j += 2)
  {
    CheckMV(workarea, mvx, mvy + j);
    CheckMV(workarea, mvx, mvy + j);
  }
}


void PlaneOfBlocks::UMHSearch(WorkingArea &workarea, int i_me_range, int omx, int omy) // radius
{
  // Uneven-cross Multi-Hexagon-grid Search (see x264)
  /* hexagon grid */

//	int omx = workarea.bestMV.x;
//	int omy = workarea.bestMV.y;
  // my mod: do not shift the center after Cross
  CrossSearch(workarea, 1, i_me_range, i_me_range, omx, omy);

  int i = 1;
  do
  {
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
      CheckMV(workarea, mx, my);
    }
  } while (++i <= i_me_range / 4);

  //	if( bmy <= mv_y_max )
  // {
  //		goto me_hex2;
  //	}

  Hex2Search(workarea, i_me_range);
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
const uint8_t *	PlaneOfBlocks::GetRefBlock(WorkingArea &workarea, int nVx, int nVy)
{
  //	return pRefFrame->GetPlane(YPLANE)->GetAbsolutePointer((workarea.x[0]<<nLogPel) + nVx, (workarea.y[0]<<nLogPel) + nVy);
  return (nPel == 2) ? pRefFrame->GetPlane(YPLANE)->GetAbsolutePointerPel <1>((workarea.x[0] << 1) + nVx, (workarea.y[0] << 1) + nVy) :
    (nPel == 1) ? pRefFrame->GetPlane(YPLANE)->GetAbsolutePointerPel <0>((workarea.x[0]) + nVx, (workarea.y[0]) + nVy) :
    pRefFrame->GetPlane(YPLANE)->GetAbsolutePointerPel <2>((workarea.x[0] << 2) + nVx, (workarea.y[0] << 2) + nVy);
}

const uint8_t *	PlaneOfBlocks::GetRefBlockU(WorkingArea &workarea, int nVx, int nVy)
{
  //	return pRefFrame->GetPlane(UPLANE)->GetAbsolutePointer((workarea.x[1]<<nLogPel) + (nVx >> 1), (workarea.y[1]<<nLogPel) + (yRatioUV==1 ? nVy : nVy>>1) ); //v.1.2.1
  return (nPel == 2) ? pRefFrame->GetPlane(UPLANE)->GetAbsolutePointerPel <1>((workarea.x[1] << 1) + (xRatioUV == 1 ? nVx : nVx >> 1), (workarea.y[1] << 1) + (yRatioUV == 1 ? nVy : nVy >> 1)) :
    (nPel == 1) ? pRefFrame->GetPlane(UPLANE)->GetAbsolutePointerPel <0>((workarea.x[1]) + (xRatioUV == 1 ? nVx : nVx >> 1), (workarea.y[1]) + (yRatioUV == 1 ? nVy : nVy >> 1)) :
    pRefFrame->GetPlane(UPLANE)->GetAbsolutePointerPel <2>((workarea.x[1] << 2) + (xRatioUV == 1 ? nVx : nVx >> 1), (workarea.y[1] << 2) + (yRatioUV == 1 ? nVy : nVy >> 1));
  // xRatioUV fix after 2.7.0.22c
}

const uint8_t *	PlaneOfBlocks::GetRefBlockV(WorkingArea &workarea, int nVx, int nVy)
{
  //	return pRefFrame->GetPlane(VPLANE)->GetAbsolutePointer((workarea.x[2]<<nLogPel) + (nVx >> 1), (workarea.y[2]<<nLogPel) + (yRatioUV==1 ? nVy : nVy>>1) );
  return (nPel == 2) ? pRefFrame->GetPlane(VPLANE)->GetAbsolutePointerPel <1>((workarea.x[2] << 1) + (xRatioUV == 1 ? nVx : nVx >> 1), (workarea.y[2] << 1) + (yRatioUV == 1 ? nVy : nVy >> 1)) :
    (nPel == 1) ? pRefFrame->GetPlane(VPLANE)->GetAbsolutePointerPel <0>((workarea.x[2]) + (xRatioUV == 1 ? nVx : nVx >> 1), (workarea.y[2]) + (yRatioUV == 1 ? nVy : nVy >> 1)) :
    pRefFrame->GetPlane(VPLANE)->GetAbsolutePointerPel <2>((workarea.x[2] << 2) + (xRatioUV == 1 ? nVx : nVx >> 1), (workarea.y[2] << 2) + (yRatioUV == 1 ? nVy : nVy >> 1));
  // xRatioUV fix after 2.7.0.22c
}

const uint8_t *	PlaneOfBlocks::GetSrcBlock(int nX, int nY)
{
  return pSrcFrame->GetPlane(YPLANE)->GetAbsolutePelPointer(nX, nY);
}

sad_t	PlaneOfBlocks::LumaSADx(WorkingArea &workarea, const unsigned char *pRef0)
{
  sad_t sad;
  sad_t refLuma; // int or float/double?
  switch (dctmode)
  {
  case 1: // dct SAD
    workarea.DCT->DCTBytes2D(pRef0, nRefPitch[0], &workarea.dctRef[0], dctpitch);
    sad = (SAD(&workarea.dctSrc[0], dctpitch, &workarea.dctRef[0], dctpitch) + abs(workarea.dctSrc[0] - workarea.dctRef[0]) * 3)*nBlkSizeX / 2; //correct reduced DC component
    break;
  case 2: //  globally (lumaChange) weighted spatial and DCT
    sad = SAD(workarea.pSrc[0], nSrcPitch[0], pRef0, nRefPitch[0]);
    if (dctweight16 > 0)
    {
      workarea.DCT->DCTBytes2D(pRef0, nRefPitch[0], &workarea.dctRef[0], dctpitch);
      sad_t dctsad = (SAD(&workarea.dctSrc[0], dctpitch, &workarea.dctRef[0], dctpitch) + abs(workarea.dctSrc[0] - workarea.dctRef[0]) * 3)*nBlkSizeX / 2;
      sad = (sad*(16 - dctweight16) + dctsad*dctweight16) / 16;
    }
    break;
  case 3: // per block adaptive switched from spatial to equal mixed SAD (faster)
    refLuma = LUMA(pRef0, nRefPitch[0]);
    sad = SAD(workarea.pSrc[0], nSrcPitch[0], pRef0, nRefPitch[0]);
    if (abs((int)workarea.srcLuma - (int)refLuma) > ((int)workarea.srcLuma + (int)refLuma) >> 5)
    {
      workarea.DCT->DCTBytes2D(pRef0, nRefPitch[0], &workarea.dctRef[0], dctpitch);
      sad_t dctsad = SAD(&workarea.dctSrc[0], dctpitch, &workarea.dctRef[0], dctpitch)*nBlkSizeX / 2;
      sad = sad / 2 + dctsad / 2;
    }
    break;
  case 4: //  per block adaptive switched from spatial to mixed SAD with more weight of DCT (best?)
    refLuma = LUMA(pRef0, nRefPitch[0]);
    sad = SAD(workarea.pSrc[0], nSrcPitch[0], pRef0, nRefPitch[0]);
    if (abs((int)workarea.srcLuma - (int)refLuma) > ((int)workarea.srcLuma + (int)refLuma) >> 5)
    {
      workarea.DCT->DCTBytes2D(pRef0, nRefPitch[0], &workarea.dctRef[0], dctpitch);
      sad_t dctsad = SAD(&workarea.dctSrc[0], dctpitch, &workarea.dctRef[0], dctpitch)*nBlkSizeX / 2;
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
      sad = (sad*(16 - dctweight16) + dctsad*dctweight16) / 16;
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
      sad = (sad*(16 - dctweighthalf) + dctsad*dctweighthalf) / 16;
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

sad_t	PlaneOfBlocks::LumaSAD(WorkingArea &workarea, const unsigned char *pRef0)
{
#ifdef MOTION_DEBUG
  workarea.iter++;
#endif
#ifdef ALLOW_DCT
  // made simple SAD more prominent (~1% faster) while keeping DCT support (TSchniede)
  return !dctmode ? SAD(workarea.pSrc[0], nSrcPitch[0], pRef0, nRefPitch[0]) : LumaSADx(workarea, pRef0);
#else
  return SAD(workarea.pSrc[0], nSrcPitch[0], pRef0, nRefPitch[0]);
#endif
}


/* check if the vector (vx, vy) is better than the best vector found so far without penalty new - renamed in v.2.11*/
void	PlaneOfBlocks::CheckMV0(WorkingArea &workarea, int vx, int vy)
{		//here the chance for default values are high especially for zeroMVfieldShifted (on left/top border)
  if (
#ifdef ONLY_CHECK_NONDEFAULT_MV
  ((vx != 0) || (vy != zeroMVfieldShifted.y)) &&
    ((vx != workarea.predictor.x) || (vy != workarea.predictor.y)) &&
    ((vx != workarea.globalMVPredictor.x) || (vy != workarea.globalMVPredictor.y)) &&
#endif
    workarea.IsVectorOK(vx, vy))
  {
    sad_t saduv = (chroma) ? SADCHROMA(workarea.pSrc[1], nSrcPitch[1], GetRefBlockU(workarea, vx, vy), nRefPitch[1])
      + SADCHROMA(workarea.pSrc[2], nSrcPitch[2], GetRefBlockV(workarea, vx, vy), nRefPitch[2]) : 0;
    sad_t sad = LumaSAD(workarea, GetRefBlock(workarea, vx, vy));
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
  }
}

/* check if the vector (vx, vy) is better than the best vector found so far */
void	PlaneOfBlocks::CheckMV(WorkingArea &workarea, int vx, int vy)
{		//here the chance for default values are high especially for zeroMVfieldShifted (on left/top border)
  if (
#ifdef ONLY_CHECK_NONDEFAULT_MV
  ((vx != 0) || (vy != zeroMVfieldShifted.y)) &&
    ((vx != workarea.predictor.x) || (vy != workarea.predictor.y)) &&
    ((vx != workarea.globalMVPredictor.x) || (vy != workarea.globalMVPredictor.y)) &&
#endif
    workarea.IsVectorOK(vx, vy))
  {
    sad_t saduv =
      !(chroma) ? 0 :
      SADCHROMA(workarea.pSrc[1], nSrcPitch[1], GetRefBlockU(workarea, vx, vy), nRefPitch[1])
      + SADCHROMA(workarea.pSrc[2], nSrcPitch[2], GetRefBlockV(workarea, vx, vy), nRefPitch[2]);
    sad_t sad = LumaSAD(workarea, GetRefBlock(workarea, vx, vy));
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
  }
}

/* check if the vector (vx, vy) is better, and update dir accordingly */
void	PlaneOfBlocks::CheckMV2(WorkingArea &workarea, int vx, int vy, int *dir, int val)
{
  if (
#ifdef ONLY_CHECK_NONDEFAULT_MV
  ((vx != 0) || (vy != zeroMVfieldShifted.y)) &&
    ((vx != workarea.predictor.x) || (vy != workarea.predictor.y)) &&
    ((vx != workarea.globalMVPredictor.x) || (vy != workarea.globalMVPredictor.y)) &&
#endif
    workarea.IsVectorOK(vx, vy))
  {
    sad_t saduv =
      !(chroma) ? 0 :
      SADCHROMA(workarea.pSrc[1], nSrcPitch[1], GetRefBlockU(workarea, vx, vy), nRefPitch[1])
      + SADCHROMA(workarea.pSrc[2], nSrcPitch[2], GetRefBlockV(workarea, vx, vy), nRefPitch[2]);
    sad_t sad = LumaSAD(workarea, GetRefBlock(workarea, vx, vy));
    sad += saduv;
    sad_t cost = sad + workarea.MotionDistorsion(vx, vy) + ((penaltyNew*(int64_t)sad) >> 8); // v1.5.8
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
  }
}

/* check if the vector (vx, vy) is better, and update dir accordingly, but not workarea.bestMV.x, y */
void	PlaneOfBlocks::CheckMVdir(WorkingArea &workarea, int vx, int vy, int *dir, int val)
{
  if (
#ifdef ONLY_CHECK_NONDEFAULT_MV
  ((vx != 0) || (vy != zeroMVfieldShifted.y)) &&
    ((vx != workarea.predictor.x) || (vy != workarea.predictor.y)) &&
    ((vx != workarea.globalMVPredictor.x) || (vy != workarea.globalMVPredictor.y)) &&
#endif
    workarea.IsVectorOK(vx, vy))
  {
    sad_t saduv = (chroma) ? SADCHROMA(workarea.pSrc[1], nSrcPitch[1], GetRefBlockU(workarea, vx, vy), nRefPitch[1])
      + SADCHROMA(workarea.pSrc[2], nSrcPitch[2], GetRefBlockV(workarea, vx, vy), nRefPitch[2]) : 0;
    sad_t sad = LumaSAD(workarea, GetRefBlock(workarea, vx, vy));
    sad += saduv;
    sad_t cost = sad + workarea.MotionDistorsion(vx, vy) + ((penaltyNew*(int64_t)sad) >> 8); // v1.5.8
//		if (sad > LSAD/4) DebugPrintf("%d %d %d %d %d %d %d", workarea.blkIdx, vx, vy, val, workarea.nMinCost, cost, sad);
//		int cost = sad + sad*workarea.MotionDistorsion(vx, vy)/(nBlkSizeX*nBlkSizeY*4) + ((penaltyNew*sad)>>8); // v1.5.8
    if (cost < workarea.nMinCost)
    {
      workarea.bestMV.sad = sad;
      workarea.nMinCost = cost;
      *dir = val;
    }
  }
}

/* clip a vector to the horizontal boundaries */
int	PlaneOfBlocks::ClipMVx(WorkingArea &workarea, int vx)
{
  //	return imin(workarea.nDxMax - 1, imax(workarea.nDxMin, vx));
  if (vx < workarea.nDxMin) return workarea.nDxMin;
  else if (vx >= workarea.nDxMax) return workarea.nDxMax - 1;
  else return vx;
}

/* clip a vector to the vertical boundaries */
int	PlaneOfBlocks::ClipMVy(WorkingArea &workarea, int vy)
{
  //	return imin(workarea.nDyMax - 1, imax(workarea.nDyMin, vy));
  if (vy < workarea.nDyMin) return workarea.nDyMin;
  else if (vy >= workarea.nDyMax) return workarea.nDyMax - 1;
  else return vy;
}

/* clip a vector to the search boundaries */
VECTOR	PlaneOfBlocks::ClipMV(WorkingArea &workarea, VECTOR v)
{
  VECTOR v2;
  v2.x = ClipMVx(workarea, v.x);
  v2.y = ClipMVy(workarea, v.y);
  v2.sad = v.sad;
  return v2;
}


/* find the median between a, b and c */
int	PlaneOfBlocks::Median(int a, int b, int c)
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
unsigned int	PlaneOfBlocks::SquareDifferenceNorm(const VECTOR& v1, const VECTOR& v2)
{
  return (v1.x - v2.x) * (v1.x - v2.x) + (v1.y - v2.y) * (v1.y - v2.y);
}

/* computes square distance between two vectors */
unsigned int	PlaneOfBlocks::SquareDifferenceNorm(const VECTOR& v1, const int v2x, const int v2y)
{
  return (v1.x - v2x) * (v1.x - v2x) + (v1.y - v2y) * (v1.y - v2y);
}

/* check if an index is inside the block's min and max indexes */
bool	PlaneOfBlocks::IsInFrame(int i)
{
  return ((i >= 0) && (i < nBlkCount));
}



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
      workarea.x[0] = pSrcFrame->GetPlane(YPLANE)->GetHPadding() + (nBlkSizeX - nOverlapX)*(nBlkX - 1);
      if (chroma)
      {
        workarea.x[1] = pSrcFrame->GetPlane(UPLANE)->GetHPadding() + ((nBlkSizeX - nOverlapX) / xRatioUV)*(nBlkX - 1);  // PF after 2.7.0.22c
        workarea.x[2] = pSrcFrame->GetPlane(VPLANE)->GetHPadding() + ((nBlkSizeX - nOverlapX) / xRatioUV)*(nBlkX - 1);
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
      workarea.pSrc[0] = pSrcFrame->GetPlane(YPLANE)->GetAbsolutePelPointer(workarea.x[0], workarea.y[0]);
      //create aligned copy
      BLITLUMA(workarea.pSrc_temp[0], nSrcPitch[0], workarea.pSrc[0], nSrcPitch_plane[0]);
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

      PseudoEPZSearch(workarea);
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
        workarea.sumLumaChange += LUMA(GetRefBlock(workarea, 0, 0), nRefPitch[0]) - LUMA(workarea.pSrc[0], nSrcPitch[0]);
      }

      /* increment indexes & pointers */
      if (iblkx < nBlkX - 1)
      {
        workarea.x[0] += (nBlkSizeX - nOverlapX)*workarea.blkScanDir;
        workarea.x[1] += ((nBlkSizeX - nOverlapX)*workarea.blkScanDir / xRatioUV); // PF after 2.7.0.22c);
        workarea.x[2] += ((nBlkSizeX - nOverlapX)*workarea.blkScanDir / xRatioUV);
      }
    }	// for iblkx

    pBlkData += nBlkX*N_PER_BLOCK;
    if (outfilebuf != NULL) // write vector to outfile
    {
      outfilebuf += nBlkX * 4;// 4 short word per block
    }

    workarea.y[0] += (nBlkSizeY - nOverlapY);
    workarea.y[1] += ((nBlkSizeY - nOverlapY) >> nLogyRatioUV);
    workarea.y[2] += ((nBlkSizeY - nOverlapY) >> nLogyRatioUV);
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
      workarea.x[0] = pSrcFrame->GetPlane(YPLANE)->GetHPadding() + (nBlkSizeX - nOverlapX)*(nBlkX - 1);
      if (chroma)
      {
        workarea.x[1] = pSrcFrame->GetPlane(UPLANE)->GetHPadding() + ((nBlkSizeX - nOverlapX) / xRatioUV)*(nBlkX - 1);
        workarea.x[2] = pSrcFrame->GetPlane(VPLANE)->GetHPadding() + ((nBlkSizeX - nOverlapX) / xRatioUV)*(nBlkX - 1);
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

      if (_smooth == 1) // interpolate
      {
        VECTOR vectorOld1 = _mv_clip_ptr->GetBlock(0, blkxold1 + blkyold1*nBlkXold).GetMV(); // 4 old nearest vectors (may coinside)
        VECTOR vectorOld2 = _mv_clip_ptr->GetBlock(0, blkxold2 + blkyold1*nBlkXold).GetMV();
        VECTOR vectorOld3 = _mv_clip_ptr->GetBlock(0, blkxold1 + blkyold2*nBlkXold).GetMV();
        VECTOR vectorOld4 = _mv_clip_ptr->GetBlock(0, blkxold2 + blkyold2*nBlkXold).GetMV();

        // interpolate
        int vector1_x = vectorOld1.x*nStepXold + deltaX*(vectorOld2.x - vectorOld1.x); // scaled by nStepXold to skip slow division
        int vector1_y = vectorOld1.y*nStepXold + deltaX*(vectorOld2.y - vectorOld1.y);
        bigsad_t vector1_sad = (bigsad_t)vectorOld1.sad*nStepXold + deltaX*((bigsad_t)vectorOld2.sad - vectorOld1.sad);

        int vector2_x = vectorOld3.x*nStepXold + deltaX*(vectorOld4.x - vectorOld3.x);
        int vector2_y = vectorOld3.y*nStepXold + deltaX*(vectorOld4.y - vectorOld3.y);
        bigsad_t vector2_sad = (bigsad_t)vectorOld3.sad*nStepXold + deltaX*((bigsad_t)vectorOld4.sad - vectorOld3.sad);

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
      workarea.predictor.sad = (sad_t)((bigsad_t)vectorOld.sad * (nBlkSizeX*nBlkSizeY) / (nBlkSizeXold*nBlkSizeYold)); // normalized to new block size

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

      sad_t saduv = (chroma) ? SADCHROMA(workarea.pSrc[1], nSrcPitch[1], GetRefBlockU(workarea, workarea.predictor.x, workarea.predictor.y), nRefPitch[1])
        + SADCHROMA(workarea.pSrc[2], nSrcPitch[2], GetRefBlockV(workarea, workarea.predictor.x, workarea.predictor.y), nRefPitch[2]) : 0;
      sad_t sad = LumaSAD(workarea, GetRefBlock(workarea, workarea.predictor.x, workarea.predictor.y));
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
        if (searchType & ONETIME)
        {
          for (int i = nSearchParam; i > 0; i /= 2)
          {
            OneTimeSearch(workarea, i);
          }
        }

        if (searchType & NSTEP)
        {
          NStepSearch(workarea, nSearchParam);
        }

        if (searchType & LOGARITHMIC)
        {
          for (int i = nSearchParam; i > 0; i /= 2)
          {
            DiamondSearch(workarea, i);
          }
        }

        if (searchType & EXHAUSTIVE)
        {
          //       ExhaustiveSearch(nSearchParam);
          int mvx = workarea.bestMV.x;
          int mvy = workarea.bestMV.y;
          for (int i = 1; i <= nSearchParam; i++)// region is same as exhaustive, but ordered by radius (from near to far)
          {
            ExpandingSearch(workarea, i, 1, mvx, mvy);
          }
        }

        if (searchType & HEX2SEARCH)
        {
          Hex2Search(workarea, nSearchParam);
        }

        if (searchType & UMHSEARCH)
        {
          UMHSearch(workarea, nSearchParam, workarea.bestMV.x, workarea.bestMV.y);
        }

        if (searchType & HSEARCH)
        {
          int mvx = workarea.bestMV.x;
          int mvy = workarea.bestMV.y;
          for (int i = 1; i <= nSearchParam; i++)// region is same as exhaustive, but ordered by radius (from near to far)
          {
            CheckMV(workarea, mvx - i, mvy);
            CheckMV(workarea, mvx + i, mvy);
          }
        }

        if (searchType & VSEARCH)
        {
          int mvx = workarea.bestMV.x;
          int mvy = workarea.bestMV.y;
          for (int i = 1; i <= nSearchParam; i++)// region is same as exhaustive, but ordered by radius (from near to far)
          {
            CheckMV(workarea, mvx, mvy - i);
            CheckMV(workarea, mvx, mvy + i);
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
        workarea.sumLumaChange += LUMA(GetRefBlock(workarea, 0, 0), nRefPitch[0]) - LUMA(workarea.pSrc[0], nSrcPitch[0]);
      }

      if (iblkx < nBlkX - 1)
      {
        workarea.x[0] += (nBlkSizeX - nOverlapX)*workarea.blkScanDir;
        workarea.x[1] += ((nBlkSizeX - nOverlapX)*workarea.blkScanDir / xRatioUV);// PF after 2.7.0.22c);
        workarea.x[2] += ((nBlkSizeX - nOverlapX)*workarea.blkScanDir / xRatioUV);
      }
    }	// for workarea.blkx

    pBlkData += nBlkX*N_PER_BLOCK;
    if (outfilebuf != NULL) // write vector to outfile
    {
      outfilebuf += nBlkX * 4;// 4 short word per block
    }

    workarea.y[0] += (nBlkSizeY - nOverlapY);
    workarea.y[1] += ((nBlkSizeY - nOverlapY) >> nLogyRatioUV); // same as /yRatioUV
    workarea.y[2] += ((nBlkSizeY - nOverlapY) >> nLogyRatioUV);
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



PlaneOfBlocks::WorkingArea::WorkingArea(int nBlkSizeX, int nBlkSizeY, int dctpitch, int nLogxRatioUV, int xRatioUV, int nLogyRatioUV, int yRatioUV, int _pixelsize, int _bits_per_pixel)
  : dctSrc(nBlkSizeY*dctpitch)
  , dctRef(nBlkSizeY*dctpitch)
  , DCT(0)
  , pixelsize(_pixelsize)
  , bits_per_pixel(_bits_per_pixel)
{
#if (ALIGN_SOURCEBLOCK > 1)
  int blocksize = nBlkSizeX*nBlkSizeY*pixelsize; // for memory allocation pixelsize needed
  int UVblocksize = blocksize >> (nLogxRatioUV + nLogyRatioUV); // >> nx >> ny
  int sizeAlignedBlock = blocksize + (ALIGN_SOURCEBLOCK - (blocksize%ALIGN_SOURCEBLOCK)) +
    2 * UVblocksize + (ALIGN_SOURCEBLOCK - (UVblocksize%ALIGN_SOURCEBLOCK));
  // int sizeAlignedBlock=blocksize+(ALIGN_SOURCEBLOCK-(blocksize%ALIGN_SOURCEBLOCK))+
  //                         2*((blocksize/2)>>nLogyRatioUV)+(ALIGN_SOURCEBLOCK-(((blocksize/2)/yRatioUV)%ALIGN_SOURCEBLOCK)); // why >>Logy then /y?
  pSrc_temp_base.resize(sizeAlignedBlock);
  pSrc_temp[0] = &pSrc_temp_base[0];
  pSrc_temp[1] = (uint8_t *)((((uintptr_t)pSrc_temp[0]) + blocksize + ALIGN_SOURCEBLOCK - 1)&(~(ALIGN_SOURCEBLOCK - 1)));
  pSrc_temp[2] = (uint8_t *)(((((uintptr_t)pSrc_temp[1]) + UVblocksize + ALIGN_SOURCEBLOCK - 1)&(~(ALIGN_SOURCEBLOCK - 1))));
#endif	// ALIGN_SOURCEBLOCK
  /* VS
#ifdef ALIGN_SOURCEBLOCK
    dctpitch = max(nBlkSizeX, 16) * bytesPerSample;
    dctSrc_base = new uint8_t[nBlkSizeY*dctpitch+ALIGN_PLANES-1];
    dctRef_base = new uint8_t[nBlkSizeY*dctpitch+ALIGN_PLANES-1];
    dctSrc = (uint8_t *)(((((intptr_t)dctSrc_base) + ALIGN_PLANES - 1)&(~(ALIGN_PLANES - 1))));//aligned like this means, that it will have optimum fit in the cache
    dctRef = (uint8_t *)(((((intptr_t)dctRef_base) + ALIGN_PLANES - 1)&(~(ALIGN_PLANES - 1))));

    int blocksize = nBlkSizeX * nBlkSizeY * bytesPerSample;
    int blocksizeUV = blocksize >> (nLogxRatioUV + nLogyRatioUV);
    int sizeAlignedBlock = blocksize + (ALIGN_SOURCEBLOCK - (blocksize % ALIGN_SOURCEBLOCK))
        + 2 * blocksizeUV + (ALIGN_SOURCEBLOCK - (blocksizeUV % ALIGN_SOURCEBLOCK));

    pSrc_temp_base = new uint8_t[sizeAlignedBlock + ALIGN_PLANES - 1];
    pSrc_temp[0] = (uint8_t *)(((intptr_t)pSrc_temp_base + ALIGN_PLANES - 1) & (~(ALIGN_PLANES - 1)));
    pSrc_temp[1] = (uint8_t *)(((intptr_t)pSrc_temp[0] + blocksize + ALIGN_SOURCEBLOCK - 1) & (~(ALIGN_SOURCEBLOCK - 1)));
    pSrc_temp[2] = (uint8_t *)(((intptr_t)pSrc_temp[1] + blocksizeUV + ALIGN_SOURCEBLOCK - 1) & (~(ALIGN_SOURCEBLOCK - 1)));
#else
    dctpitch = max(nBlkSizeX, 16) * bytesPerSample;
    dctSrc = new uint8_t[nBlkSizeY*dctpitch];
    dctRef = new uint8_t[nBlkSizeY*dctpitch];
#endif
*/

}



PlaneOfBlocks::WorkingArea::~WorkingArea()
{
  // Nothing
}




/* check if a vector is inside search boundaries */
bool	PlaneOfBlocks::WorkingArea::IsVectorOK(int vx, int vy) const
{
  return (
    (vx >= nDxMin)
    && (vy >= nDyMin)
    && (vx < nDxMax)
    && (vy < nDyMax)
    );
}

/* computes the cost of a vector (vx, vy) */
int	PlaneOfBlocks::WorkingArea::MotionDistorsion(int vx, int vy) const
{
  int dist = SquareDifferenceNorm(predictor, vx, vy);
  return (nLambda * dist) >> (16 - bits_per_pixel) /*8*/; // PF
}

/* computes the length cost of a vector (vx, vy) */
//inline int LengthPenalty(int vx, int vy)
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
    _x_ratio_uv,
    _y_ratio_uv_log,
    _y_ratio_uv,
    _pixelsize,
    _bits_per_pixel
  ));
}


