// MVTools
// 2004 Manao
// Copyright(c)2006-2009 A.G.Balakhnin aka Fizick - true motion, overlap, YUY2, pelclip, divide, super
// General classe for motion based filters

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

#include "def.h"
#include	"ClipFnc.h"
#include "commonfunctions.h"
#include "cpu.h"
#include "DCTFFTW.h"
#include "DCTINT.h"
#include "MVAnalyse.h"
#include "MVGroupOfFrames.h"
#include "MVSuper.h"
#include "profile.h"
#include "SuperParams64Bits.h"

#include <cmath>
#include <cstdio>
#include <algorithm>

MVAnalyse::MVAnalyse(
  PClip _child, int _blksizex, int _blksizey, int lv, int st, int stp,
  int _pelSearch, bool isb, int lambda, bool chroma, int df, sad_t _lsad,
  int _plevel, bool _global, int _pnew, int _pzero, int _pglobal,
  int _overlapx, int _overlapy, const char* _outfilename, int _dctmode,
  int _divide, int _sadx264, sad_t _badSAD, int _badrange, bool _isse,
  bool _meander, bool temporal_flag, bool _tryMany, bool multi_flag,
  bool mt_flag, int _chromaSADScale, IScriptEnvironment* env
)
  : ::GenericVideoFilter(_child)
  , _srd_arr(1)
  , _vectorfields_aptr()
  , _multi_flag(multi_flag)
  , _temporal_flag(temporal_flag)
  , _mt_flag(mt_flag)
  , _dct_factory_ptr()
  , _dct_pool()
  , _delta_max(0)

{
  has_at_least_v8 = true;
  try { env->CheckVersion(8); }
  catch (const AvisynthError&) { has_at_least_v8 = false; }

  static int id = 0; _instance_id = id++;
  _RPT1(0, "MvAnalyse.Create id=%d\n", _instance_id);

  if (multi_flag && df < 1)
  {
    env->ThrowError(
      "MAnalyse: cannot use a fixed frame reference "
      "(delta < 1) in multi mode."
    );
  }

  _RPT1(0, "MAnalyze created, isb=%d\n", isb ? 1 : 0);

  pixelsize = vi.ComponentSize();
  bits_per_pixel = vi.BitsPerComponent();

  MVAnalysisData &	analysisData = _srd_arr[0]._analysis_data;
  MVAnalysisData &	analysisDataDivided = _srd_arr[0]._analysis_data_divided;

  if (pixelsize == 4)
  {
    env->ThrowError("MAnalyse: Clip with float pixel type is not supported");
  }

  if (!vi.IsYUV() && !vi.IsYUY2()) // YUY2 is also YUV but let's see what is supported
  {
    env->ThrowError("MAnalyse: Clip must be YUV or YUY2");
  }

  if (vi.IsY())
    chroma = false; // silent fallback

  // get parameters of super clip - v2.0
  SuperParams64Bits	params;
  memcpy(&params, &child->GetVideoInfo().num_audio_samples, 8);
  const int		nHeight = params.nHeight;
  const int		nSuperHPad = params.nHPad;
  const int		nSuperVPad = params.nVPad;
  const int		nSuperPel = params.nPel;
  const int		nSuperModeYUV = params.nModeYUV;
  const int		nSuperLevels = params.nLevels;

  if (nHeight <= 0
    || nSuperHPad < 0
    || nSuperHPad >= vi.width / 2 // PF: intentional /2
    || nSuperVPad < 0
    || nSuperPel < 1
    || nSuperPel     >  4
    || nSuperModeYUV <  0
    || nSuperModeYUV >  YUVPLANES
    || nSuperLevels < 1)
  {
    env->ThrowError("MAnalyse: wrong super clip (pseudoaudio) parameters");
  }

  analysisData.nWidth = vi.width - nSuperHPad * 2;
  analysisData.nHeight = nHeight;
  analysisData.pixelType = vi.pixel_type;
  if (!vi.IsY()) {
    analysisData.yRatioUV = vi.IsYUY2() ? 1 : (1 << vi.GetPlaneHeightSubsampling(PLANAR_U));
    analysisData.xRatioUV = vi.IsYUY2() ? 2 : (1 << vi.GetPlaneWidthSubsampling(PLANAR_U));
  }
  else {
    analysisData.yRatioUV = 1; // n/a
    analysisData.xRatioUV = 1; // n/a
  }
  analysisData.pixelsize = pixelsize;
  analysisData.bits_per_pixel = bits_per_pixel;

  if (_chromaSADScale < -2 || _chromaSADScale>2)
    env->ThrowError(
      "MAnalyze: scaleCSAD must be -2..2"
    );

  analysisData.chromaSADScale = _chromaSADScale;

  pSrcGOF = new MVGroupOfFrames(
    nSuperLevels, analysisData.nWidth, analysisData.nHeight,
    nSuperPel, nSuperHPad, nSuperVPad, nSuperModeYUV,
    _isse, analysisData.xRatioUV, analysisData.yRatioUV, pixelsize, bits_per_pixel, mt_flag
  );
  pRefGOF = new MVGroupOfFrames(
    nSuperLevels, analysisData.nWidth, analysisData.nHeight,
    nSuperPel, nSuperHPad, nSuperVPad, nSuperModeYUV,
    _isse, analysisData.xRatioUV, analysisData.yRatioUV, pixelsize, bits_per_pixel, mt_flag
  );

  analysisData.nBlkSizeX = _blksizex;
  analysisData.nBlkSizeY = _blksizey;
  // same blocksize check in MAnalyze and MRecalculate
  // some blocksizes may not work in 4:2:0 (chroma subsampling division), but o.k. in 4:4:4
  const std::vector< std::pair< int, int > > allowed_blksizes =
  {
    { 64, 64 },{ 64,48 },{ 64,32 },{ 64,16 },
    { 48,64 },{ 48,48 },{ 48,24 },{ 48,12 },
    { 32,64 },{ 32,32 },{ 32,24 },{ 32,16 },{ 32,8 },
    { 24,48 },{ 24,32 },{ 24,24 },{ 24,12 },{ 24,6 },
    { 16,64 },{ 16,32 },{ 16,16 },{ 16,12 },{ 16,8 },{ 16,4 },{ 16,2 },
    { 12,48 },{ 12,24 },{ 12,16 },{ 12,12 },{ 12,6 },{ 12,3 },
    { 8,32 },{ 8,16 },{ 8,8 },{ 8,4 },{ 8,2 },{ 8,1 },
    { 6,24 },{ 6,12 },{ 6,6 },{ 6,3 },
    { 4,8 },{ 4,4 },{ 4,2 },
    { 3,6 },{ 3,3 },
    { 2,4 },{ 2,2 }
  };
  bool found = false;
  for (int i = 0; i < (int)allowed_blksizes.size(); i++) {
    if (analysisData.nBlkSizeX == allowed_blksizes[i].first && analysisData.nBlkSizeY == allowed_blksizes[i].second) {
      found = true;
      break;
    }
  }
  if (!found) {
    env->ThrowError(
      "MAnalyse: Invalid block size: %d x %d", analysisData.nBlkSizeX, analysisData.nBlkSizeY);
  }

  analysisData.nPel = nSuperPel;
  if (analysisData.nPel != 1
    && analysisData.nPel != 2
    && analysisData.nPel != 4)
  {
    env->ThrowError("MAnalyse: pel has to be 1 or 2 or 4");
  }

  analysisData.nDeltaFrame = df;
//	if (analysisData.nDeltaFrame < 1)
//	{
//		analysisData.nDeltaFrame = 1;
//	}

  if (_overlapx < 0 || _overlapx > _blksizex/2
    || _overlapy < 0 || _overlapy > _blksizey/2)
  {
    env->ThrowError("MAnalyse: overlap must be less or equal than half block size");
  }

  if (_overlapx % analysisData.xRatioUV || _overlapy % analysisData.yRatioUV)
  {
    env->ThrowError("MAnalyse: wrong overlap for the colorspace subsampling");
  }

  if (_divide != 0 && (_blksizex < 8 || _blksizey < 8)) // || instead of && 2.5.11.22 green garbage issue
  {
    env->ThrowError(
      "MAnalyse: Block sizes must be 8 or more for divide mode"
    );
  }

  if (_divide != 0
    && ((_overlapx % (2 * analysisData.xRatioUV)) || (_overlapy % (2 * analysisData.yRatioUV))) // PF subsampling-aware
    )
  {
    env->ThrowError("MAnalyse: wrong overlap for the colorspace subsampling for divide mode");
  }

  divideExtra = _divide;

  // include itself, but usually equal to 256 :-)
  headerSize = std::max(int(4 + sizeof(analysisData)), 256);

  analysisData.nOverlapX = _overlapx;
  analysisData.nOverlapY = _overlapy;

  int		nBlkX = (analysisData.nWidth - analysisData.nOverlapX)
    / (analysisData.nBlkSizeX - analysisData.nOverlapX);
  int		nBlkY = (analysisData.nHeight - analysisData.nOverlapY)
    / (analysisData.nBlkSizeY - analysisData.nOverlapY);

  // 2.7.36: fallback to no overlap when either nBlk count is less that 2
  if (nBlkX < 2 || nBlkY < 2) {
    analysisData.nOverlapX = 0;
    analysisData.nOverlapY = 0;
    nBlkX = analysisData.nWidth / analysisData.nBlkSizeX;
    nBlkY = analysisData.nHeight / analysisData.nBlkSizeY;
  }

  analysisData.nBlkX = nBlkX;
  analysisData.nBlkY = nBlkY;

  const int		nWidth_B =
    (analysisData.nBlkSizeX - analysisData.nOverlapX) * nBlkX
    + analysisData.nOverlapX; // covered by blocks
  const int		nHeight_B =
    (analysisData.nBlkSizeY - analysisData.nOverlapY) * nBlkY
    + analysisData.nOverlapY;

  // calculate valid levels
  int				nLevelsMax = 0;
  // at last one block
  while (((nWidth_B >> nLevelsMax) - analysisData.nOverlapX)
    / (analysisData.nBlkSizeX - analysisData.nOverlapX) > 0
    && ((nHeight_B >> nLevelsMax) - analysisData.nOverlapY)
    / (analysisData.nBlkSizeY - analysisData.nOverlapY) > 0)
  {
    ++nLevelsMax;
  }

  analysisData.nLvCount = (lv > 0) ? lv : nLevelsMax + lv;
  if (analysisData.nLvCount > nSuperLevels)
  {
    env->ThrowError(
      "MAnalyse: it is not enough levels  in super clip (%d), "
      "while MAnalyse asks %d", nSuperLevels, analysisData.nLvCount
    );
  }
  if (analysisData.nLvCount < 1
    || analysisData.nLvCount > nLevelsMax)
  {
    env->ThrowError(
      "MAnalyse: non-valid number of levels (%d)", analysisData.nLvCount
    );
  }

  analysisData.isBackward = isb;

  nLambda = lambda;

  // lambda is finally scaled in PlaneOfBlocks::WorkingArea::MotionDistorsion(int vx, int vy) const
  // as return (nLambda * dist) >> (16 - bits_per_pixel)
  // To have it 8x8 normalized, we would use 
  //   nLambda = lambda * ((_blksizex * _blksizey) / (8 * 8)) << (bits_per_pixel-8);  // normalize to 8x8 block size
  // and use 
  //   (nLambda * dist) >> 8  in PlaneOfBlocks::WorkingArea::MotionDistorsion
  // and change default lambda generation in truemotion=true preset
  // But doing this would kill compatibility, there are scripts which are using lambda properly scaled by the block size.

  lsad = _lsad   * (_blksizex * _blksizey) / 64 * (1 << (bits_per_pixel - 8));
  pnew = _pnew;
  plevel = _plevel;
  global = _global;
  pglobal = _pglobal;
  pzero = _pzero;
  badSAD = _badSAD * (_blksizex * _blksizey) / 64 * (1 << (bits_per_pixel - 8));
  badrange = _badrange;
  meander = _meander;
  tryMany = _tryMany;

  if (_dctmode != 0)
  {
    _dct_factory_ptr = std::unique_ptr <DCTFactory>(
      new DCTFactory(_dctmode, _isse, _blksizex, _blksizey, pixelsize, bits_per_pixel, *env)
      );
    _dct_pool.set_factory(*_dct_factory_ptr);
  }

  switch (st)
  {
  case 0:
    searchType = ONETIME;
    nSearchParam = (stp < 1) ? 1 : stp;
    break;
  case 1:
    searchType = NSTEP;
    nSearchParam = (stp < 0) ? 0 : stp;
    break;
  case 3:
    searchType = EXHAUSTIVE;
    nSearchParam = (stp < 1) ? 1 : stp;
    break;
  case 4:
    searchType = HEX2SEARCH;
    nSearchParam = (stp < 1) ? 1 : stp;
    break;
  case 5:
    searchType = UMHSEARCH;
    nSearchParam = (stp < 1) ? 1 : stp; // really min is 4
    break;
  case 6:
    searchType = HSEARCH;
    nSearchParam = (stp < 1) ? 1 : stp;
    break;
  case 7:
    searchType = VSEARCH;
    nSearchParam = (stp < 1) ? 1 : stp;
    break;
  case 2:
  default:
    searchType = LOGARITHMIC;
    nSearchParam = (stp < 1) ? 1 : stp;
  }

  // not below value of 0 at finest level
  nPelSearch = (_pelSearch <= 0) ? analysisData.nPel : _pelSearch;


  analysisData.nFlags = 0;
  analysisData.nFlags |= (_isse) ? MOTION_USE_ISSE : 0;
  analysisData.nFlags |= (analysisData.isBackward) ? MOTION_IS_BACKWARD : 0;
  analysisData.nFlags |= (chroma) ? MOTION_USE_CHROMA_MOTION : 0;

  analysisData.nFlags |= conv_cpuf_flags_to_cpu(env->GetCPUFlags());
  // cpu flags has different layout that Avisynth's CPUF_xxx layout.
  // Never mix CPU_xxxx and CPUF_xxxx constants!

  nModeYUV = (chroma) ? YUVPLANES : YPLANE;
  if ((nModeYUV & nSuperModeYUV) != nModeYUV)
  {
    env->ThrowError(
      "MAnalyse: super clip does not contain needed color data"
    );
  }

  _vectorfields_aptr = std::unique_ptr <GroupOfPlanes>(new GroupOfPlanes(
    analysisData.nBlkSizeX,
    analysisData.nBlkSizeY,
    analysisData.nLvCount,
    analysisData.nPel,
    analysisData.nFlags,
    analysisData.nOverlapX,
    analysisData.nOverlapY,
    analysisData.nBlkX,
    analysisData.nBlkY,
    analysisData.xRatioUV, // PF
    analysisData.yRatioUV,
    divideExtra,
    analysisData.pixelsize, // PF
    analysisData.bits_per_pixel,
    (_dct_factory_ptr.get() != 0) ? &_dct_pool : 0,
    _mt_flag,
    analysisData.chromaSADScale,
    env
  ));

  analysisData.nMagicKey = MVAnalysisData::MOTION_MAGIC_KEY;
  analysisData.nHPadding = nSuperHPad; // v2.0
  analysisData.nVPadding = nSuperVPad;

  // MVAnalysisData and outfile format version: last update v1.8.1
  analysisData.nVersion = MVAnalysisData::VERSION;
//	DebugPrintf(" MVAnalyseData size= %d",sizeof(analysisData));

  outfilename = _outfilename;
  if (lstrlen(outfilename) > 0)
  {
    outfile = fopen(outfilename, "wb");
    if (outfile == NULL)
    {
      env->ThrowError("MAnalyse: out file can not be created!");
    }
    else
    {
      fwrite(&analysisData, sizeof(analysisData), 1, outfile);
      // short vx, short vy, int SAD = 4 words = 8 bytes per block
      outfilebuf = new short[nBlkX * nBlkY * 4];
    }
  }
  else
  {
    outfile = NULL;
    outfilebuf = NULL;
  }

  // Defines the format of the output vector clip
  // count of 32 bit integers: 2_size_validity+(foreachblock(1_validity+blockCount*3))
  const int		width_bytes = headerSize + _vectorfields_aptr->GetArraySize() * 4;
  ClipFnc::format_vector_clip(
    vi, true, nBlkX, "rgb32", width_bytes, "MAnalyse", env
  );

  if (divideExtra)	//v1.8.1
  {
    memcpy(&analysisDataDivided, &analysisData, sizeof(analysisData));
    analysisDataDivided.nBlkX = analysisData.nBlkX * 2;
    analysisDataDivided.nBlkY = analysisData.nBlkY * 2;
    analysisDataDivided.nBlkSizeX = analysisData.nBlkSizeX / 2;
    analysisDataDivided.nBlkSizeY = analysisData.nBlkSizeY / 2;
    analysisDataDivided.nOverlapX = analysisData.nOverlapX / 2;
    analysisDataDivided.nOverlapY = analysisData.nOverlapY / 2;
    analysisDataDivided.nLvCount = analysisData.nLvCount + 1;
  }

  if (_temporal_flag)
  {
    _srd_arr[0]._vec_prev.resize(_vectorfields_aptr->GetArraySize()); // array for prev vectors
  }
  _srd_arr[0]._vec_prev_frame = -2;

  // From this point, analysisData and analysisDataDivided references will
  // become invalid, because of the _srd_arr.resize(). Don't use them any more.

  if (_multi_flag)
  {
    _delta_max = df;

    _srd_arr.resize(_delta_max * 2);
    for (int delta_index = 0; delta_index < _delta_max; ++delta_index)
    {
      for (int dir_index = 0; dir_index < 2; ++dir_index)
      {
        const int		index = delta_index * 2 + dir_index;
        SrcRefData &	srd = _srd_arr[index];
        srd = _srd_arr[0];

        srd._analysis_data.nDeltaFrame = delta_index + 1;
        srd._analysis_data.isBackward = (dir_index == 0);
        if (srd._analysis_data.isBackward)
        {
          srd._analysis_data.nFlags |= MOTION_IS_BACKWARD;
        }
        else
        {
          srd._analysis_data.nFlags &= ~MOTION_IS_BACKWARD;
        }

        srd._analysis_data_divided.nDeltaFrame = srd._analysis_data.nDeltaFrame;
        srd._analysis_data_divided.isBackward = srd._analysis_data.isBackward;
        srd._analysis_data_divided.nFlags = srd._analysis_data.nFlags;
      }
    }

    vi.num_frames *= _delta_max * 2;
    vi.MulDivFPS(_delta_max * 2, 1);
  }

  // we'll transmit to the processing filters a handle
  // on the analyzing filter itself ( it's own pointer ), in order
  // to activate the right parameters.
  if (divideExtra)	//v1.8.1
  {
#if !defined(MV_64BIT)
    vi.nchannels = reinterpret_cast <uintptr_t> (&_srd_arr[0]._analysis_data_divided);
#else
    uintptr_t p = reinterpret_cast <uintptr_t> (&_srd_arr[0]._analysis_data_divided);
    vi.nchannels = 0x80000000L | (int)(p >> 32);
    vi.sample_type = (int)(p & 0xffffffffUL);
#endif
  }
  else
  {
#if !defined(MV_64BIT)
    vi.nchannels = reinterpret_cast <uintptr_t> (&_srd_arr[0]._analysis_data);
#else
    uintptr_t p = reinterpret_cast <uintptr_t> (&_srd_arr[0]._analysis_data);
    vi.nchannels = 0x80000000L | (int)(p >> 32);
    vi.sample_type = (int)(p & 0xffffffffUL);
#endif
  }
}



MVAnalyse::~MVAnalyse()
{
  if (outfile != NULL)
  {
    fclose(outfile);
    outfile = 0;
    delete[] outfilebuf;
    outfilebuf = 0;
  }

  delete pSrcGOF;
  pSrcGOF = 0;
  delete pRefGOF;
  pRefGOF = 0;
  _RPT1(0, "MAnalyze destroyed %d\n",_instance_id);

}



PVideoFrame __stdcall MVAnalyse::GetFrame(int n, IScriptEnvironment* env)
{
  _RPT2(0, "MAnalyze GetFrame, frame=%d id=%d\n", n, _instance_id);
  const int		ndiv = (_multi_flag) ? _delta_max * 2 : 1;
  const int		nsrc = n / ndiv;
  const int		srd_index = n % ndiv;

  SrcRefData &	srd = _srd_arr[srd_index];

  const int		nbr_src_frames = child->GetVideoInfo().num_frames;
  int				minframe;
  int				maxframe;
  int				nref;
  if (srd._analysis_data.nDeltaFrame > 0)
  {
    const int		offset =
      (srd._analysis_data.isBackward)
      ? srd._analysis_data.nDeltaFrame
      : -srd._analysis_data.nDeltaFrame;
    minframe = std::max(-offset, 0);
    maxframe = nbr_src_frames + std::min(-offset, 0);
    nref = nsrc + offset;
  }
  else // special static mode
  {
    nref = -srd._analysis_data.nDeltaFrame;	// positive fixed frame number
    minframe = 0;
    maxframe = nbr_src_frames;
  }

  PVideoFrame			dst = env->NewVideoFrame(vi); // frameprop inheritance later (if there is source)
  unsigned char *	pDst = dst->GetWritePtr();

  // 0 headersize (max(4+sizeof(analysisData),256)
  // 4: analysysData
  // 256: data 
  // 256: 2_size_validity+(foreachblock(1_validity+blockCount*3))

  // write analysis parameters as a header to frame
  memcpy(pDst, &headerSize, sizeof(int));
  if (divideExtra)
  {
    memcpy(
      pDst + sizeof(int),
      &srd._analysis_data_divided,
      sizeof(srd._analysis_data_divided)
    );
  }
  else
  {
    memcpy(
      pDst + sizeof(int),
      &srd._analysis_data,
      sizeof(srd._analysis_data)
    );
  }
  pDst += headerSize;

  if (nsrc < minframe || nsrc >= maxframe)
  {
    // fill all vectors with invalid data
    _vectorfields_aptr->WriteDefaultToArray(reinterpret_cast <int *> (pDst));
  }

  else
  {
//		DebugPrintf ("MVAnalyse: Get src frame %d",nsrc);
    _RPT3(0, "MAnalyze GetFrame, frame_nsrc=%d nref=%d id=%d\n", nsrc, nref, _instance_id);

    PVideoFrame	src = child->GetFrame(nsrc, env); // v2.0
    // if(has_at_least_v8) env->copyFrameProps(src, dst); // frame property support
    // The result clip is a special MV clip. It does not need to inherit the frame props of source

    load_src_frame(*pSrcGOF, src, srd._analysis_data);

//		DebugPrintf ("MVAnalyse: Get ref frame %d", nref);
//		DebugPrintf ("MVAnalyse frame %i backward=%i", nsrc, srd._analysis_data.isBackward);
    ::PVideoFrame	ref = child->GetFrame(nref, env); // v2.0
    load_src_frame(*pRefGOF, ref, srd._analysis_data);

    const int		fieldShift = ClipFnc::compute_fieldshift(
      child,
      vi.IsFieldBased(),
      srd._analysis_data.nPel,
      nsrc,
      nref
    );

    if (outfile != NULL)
    {
      fwrite(&n, sizeof(int), 1, outfile);	// write frame number
    }

    // temporal predictor dst if prev frame was really prev
    int *			pVecPrevOrNull = 0;
    if (_temporal_flag && srd._vec_prev_frame == nsrc - 1)
    {
      pVecPrevOrNull = &srd._vec_prev[0];
    }

    _vectorfields_aptr->SearchMVs(
      pSrcGOF, pRefGOF,
      searchType, nSearchParam, nPelSearch, nLambda, lsad, pnew, plevel,
      global, srd._analysis_data.nFlags, reinterpret_cast<int*>(pDst),
      outfilebuf, fieldShift, pzero, pglobal, badSAD, badrange,
      meander, pVecPrevOrNull, tryMany
    );

    if (divideExtra)
    {
      // make extra level with divided sublocks with median (not estimated)
      // motion
      _vectorfields_aptr->ExtraDivide(
        reinterpret_cast <int *> (pDst),
        srd._analysis_data.nFlags
      );
    }

//		PROFILE_CUMULATE ();
    if (outfile != NULL)
    {
      fwrite(
        outfilebuf,
        sizeof(short) * 4 * srd._analysis_data.nBlkX
        * srd._analysis_data.nBlkY,
        1,
        outfile
      );
    }
  }

  if (_temporal_flag)
  {
    // store previous vectors for use as predictor in next frame
    memcpy(
      &srd._vec_prev[0],
      reinterpret_cast <int *> (pDst),
      _vectorfields_aptr->GetArraySize()
    );
    srd._vec_prev_frame = nsrc;
  }

  _RPT3(0, "MAnalyze GetFrame END, frame_nsrc=%d nref=%d id=%d\n", nsrc, nref, _instance_id);
  return dst;
}



void	MVAnalyse::load_src_frame(MVGroupOfFrames &gof, ::PVideoFrame &src, const MVAnalysisData &ana_data)
{
  PROFILE_START(MOTION_PROFILE_YUY2CONVERT);
  const unsigned char *	pSrcY;
  const unsigned char *	pSrcU;
  const unsigned char *	pSrcV;
  int				nSrcPitchY;
  int				nSrcPitchUV;
  if ((ana_data.pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2)
  {
    // planar data packed to interleaved format (same as interleved2planar
    // by kassandro) - v2.0.0.5
    pSrcY = src->GetReadPtr();
    pSrcU = pSrcY + src->GetRowSize() / 2;
    pSrcV = pSrcU + src->GetRowSize() / 4;
    nSrcPitchY = src->GetPitch();
    nSrcPitchUV = nSrcPitchY;
  }
  else
  {
    pSrcY = src->GetReadPtr(PLANAR_Y);
    pSrcU = src->GetReadPtr(PLANAR_U);
    pSrcV = src->GetReadPtr(PLANAR_V);
    nSrcPitchY = src->GetPitch(PLANAR_Y);
    nSrcPitchUV = src->GetPitch(PLANAR_U);
  }
  PROFILE_STOP(MOTION_PROFILE_YUY2CONVERT);

  gof.Update(
    nModeYUV,
    (BYTE*)pSrcY, nSrcPitchY,
    (BYTE*)pSrcU, nSrcPitchUV,
    (BYTE*)pSrcV, nSrcPitchUV
  ); // v2.0
}

