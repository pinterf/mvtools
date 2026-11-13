// Make a motion compensate temporal denoiser

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

#ifndef __MV_ANALYSE__
#define __MV_ANALYSE__

#include "conc/ObjPool.h"
#include "DCTFactory.h"
#include "GroupOfPlanes.h"
#include "MVAnalysisData.h"

#include "avisynth.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>



class MVAnalyse
  : public GenericVideoFilter
{
protected:
  bool has_at_least_v8;

  int _instance_id; // debug unique id

  // One instance per Src/Ref combination
  // Multi mode order (bwd/fwd delta): B1, F1, B2, F2, B3, F3...
  // In single mode, only the first element is used.
  // temporal=true: the vectors of frame n-1 are a predictor for frame n. MAnalyse is MT_SERIALIZED then, but the
  // frames still come in the order the threads ask for them (Prefetch, seeking, several MAnalyse instances in an
  // MDegrain), so frame n-1 is often not the last one analysed. The vectors of the last few analysed frames are kept,
  // tagged with their frame number, and the one of frame n-1 is looked up (the fft caches of FFT3DFilter, dfttest
  // work the same way). A frame whose previous frame was not analysed before it still gets no temporal predictor.
  // 8: measured with MDegrain6 (12 MAnalyse instances) and Prefetch 4..16, more slots gave no fewer misses; one slot
  // holds the vectors of one frame (about 0.5 MB at 1080p with blksize=8)
  static constexpr int TEMPORAL_CACHE_SLOTS = 8;
  class PrevVectorCache
  {
    std::vector<std::vector<int>> _data; // vectors of one frame per slot
    std::vector<int> _frame;             // frame number of the slot, -1: empty
    std::vector<uint64_t> _used;         // last use, for replacing the least recently used slot
    uint64_t _clock = 0;
  public:
    // nints: size of the vector data of one frame (GroupOfPlanes::GetArraySize, in ints)
    void init(int nslots, int nints)
    {
      _data.assign(nslots, std::vector<int>(nints));
      _frame.assign(nslots, -1);
      _used.assign(nslots, 0);
    }
    // the vectors of frame n, nullptr if not kept
    int* find(int n)
    {
      for (size_t i = 0; i < _frame.size(); i++)
        if (_frame[i] == n && n >= 0)
        {
          _used[i] = ++_clock;
          return _data[i].data();
        }
      return nullptr;
    }
    // keeps the vectors of frame n (replaces its old copy, else the least recently used slot)
    void store(int n, const int* vectors)
    {
      size_t slot = 0;
      for (size_t i = 0; i < _frame.size(); i++)
      {
        if (_frame[i] == n) { slot = i; break; }
        if (_used[i] < _used[slot]) slot = i;
      }
      memcpy(_data[slot].data(), vectors, _data[slot].size() * sizeof(int));
      _frame[slot] = n;
      _used[slot] = ++_clock;
    }
  };

  class SrcRefData
  {
  public:
    MVAnalysisData _analysis_data;
    MVAnalysisData _analysis_data_divided;

    PrevVectorCache _vec_prev; // temporal=true only
  };

  typedef std::vector<SrcRefData> SrcRefArray;

  SrcRefArray _srd_arr;

  /*! \brief Frames of blocks for which motion vectors will be computed */
  std::unique_ptr<GroupOfPlanes> _vectorfields_aptr; // Temporary data, structure initialised once.

  /*! \brief isse optimisations enabled */
  int cpuFlags;

  /*! \brief motion vecteur cost factor */
  int nLambda;

  /*! \brief search type chosen for refinement in the EPZ */
  SearchType searchType;

  SearchType searchTypeCoarse;

  /*! \brief additionnal parameter for this search */
  int nSearchParam; // usually search radius

  int nPelSearch; // search radius at finest level

  sad_t lsad; // SAD limit for lambda using - added by Fizick
  int pnew; // penalty to cost for new canditate - added by Fizick
  int plen; // penalty factor (similar to lambda) for vector length - added by Fizick
  int plevel; // penalty factors (lambda, plen) level scaling - added by Fizick
  bool global; // use global motion predictor
  int pglobal; // penalty factor for global motion predictor
  int pzero; // penalty factor for zero vector
  const char* outfilename;// vectors output file
  int divideExtra; // divide blocks on sublocks with median motion
  sad_t badSAD; //  SAD threshold to make more wide search for bad vectors
  int badrange;// range (radius) of wide search
  bool meander; //meander (alternate) scan blocks (even row left to right, odd row right to left
  bool tryMany; // try refine around many predictors
  bool tryManyAll; // try refine around many predictors even for finest level (not only for coarser levels)
  const bool _multi_flag;
  const bool _temporal_flag;
  const bool _mt_flag;

  int pixelsize; // PF
  int bits_per_pixel;

  FILE *outfile;
  short * outfilebuf;


  std::unique_ptr<DCTFactory> _dct_factory_ptr; // Not instantiated if not needed
  conc::ObjPool<DCTClass> _dct_pool;

  int headerSize;

  MVGroupOfFrames *pSrcGOF, *pRefGOF; //v2.0. Temporary data, structure initialised once.

  int nModeYUV;

  int _delta_max;

public:

  MVAnalyse(
    PClip _child, int _blksizex, int _blksizey, int lv, int st /* search type */, int stp /* search type param */,
    int _pelSearch, bool isb, int lambda, bool chroma, int df, sad_t _lsad,
    int _plevel, bool _global, int _pnew, int _pzero, int _pglobal,
    int _overlapx, int _overlapy, const char* _outfilename, int _dctmode,
    int _divide, int _sadx264, sad_t _badSAD, int _badrange, bool _isse,
    bool _meander, bool temporal_flag, bool _tryMany, bool multi_flag,
    bool mt_flag, int _chromaSADScale, int st_coarse, bool _tryManyAll, IScriptEnvironment* env);
  ~MVAnalyse();

  ::PVideoFrame __stdcall	GetFrame(int n, ::IScriptEnvironment* env) override;

  int __stdcall SetCacheHints(int cachehints, int frame_range) override {
    return cachehints == CACHE_GET_MTMODE ? (_temporal_flag || lstrlen(outfilename)>0 ? MT_SERIALIZED : MT_MULTI_INSTANCE) : 0;
    // adaptive!
    // temporal = true or using output file is not MT-friendly
  }

private:

  void load_src_frame(MVGroupOfFrames &gof, ::PVideoFrame &src, const MVAnalysisData &ana_data);
};

#endif
