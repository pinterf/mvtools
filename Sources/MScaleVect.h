// MScaleVect Class
// By Vit, 2011
//
// Scale MVTools motion vectors. Can scale the blocks themselves to create vectors for a different frame size (powers of 2 only)

#ifndef M_SCALE_VECTORS_H
#define M_SCALE_VECTORS_H

#ifdef _WIN32
#define NOGDI
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "windows.h"
#endif
#include	"avisynth.h"

#include "MVAnalysisData.h"

class MScaleVect : public GenericVideoFilter
{
public:
  // Scaling modes supported:
  // - IncreaseBlockSize (0): Scale blocks and vectors to suit a larger clip, limited to powers of 2 (2x larger, 4x larger...)
  // - DecreaseBlockSize (1): Scale blocks and vectors to suit a smaller clip, also limited to powers of 2
  // - VectorsOnly       (2): Scale vectors only, clip-size stays same. Scale can be any float. For esoteric uses such as extrapolation
  enum ScaleMode { IncreaseBlockSize, DecreaseBlockSize, VectorsOnly };

private:
  bool has_at_least_v8;

  // Filter data
  MVAnalysisData mVectorsInfo;  // Clip dimensions, block layout etc.
  ScaleMode      mMode;         // See ScaleMode enum above
  double         mScaleX;       // Scaling to use in X. Limited to powers of 2 when ScaleMode = Increase/DecreaseBlockSize
  double         mScaleY;       // Scaling to use in Y. Limited to powers of 2 when ScaleMode = Increase/DecreaseBlockSize
  bool           mAdjustSubpel; // Scaling of vectors can be done simply by adjusting sub-pixel level, no per-frame work required
  bool				mRevert;			// If we have to flip the time reference (backward/forward)
  int            big_pixel_sad; // for bad sad
  bool           changeBitDepth; // If a different target bit depth was specified, 2.7.24
  int            currentBits;
  int            bitDiff;
  int            mNewBits;


public:
  // Constructor
  MScaleVect( PClip Child, double ScaleX, double ScaleY, ScaleMode Mode, bool Flip, bool AdjustSubpel, int Bits, IScriptEnvironment* Env );

  // Filter operation
  PVideoFrame __stdcall GetFrame( int FrameNum, IScriptEnvironment* Env ) override;

  int __stdcall SetCacheHints(int cachehints, int frame_range) override {
    return cachehints == CACHE_GET_MTMODE ? MT_MULTI_INSTANCE : 0;
  }

};


#endif
