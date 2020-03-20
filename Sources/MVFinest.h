#ifndef __MV_FINEST__
#define __MV_FINEST__

#include "yuy2planes.h"

#define	NOMINMAX
#define	WIN32_LEAN_AND_MEAN
#define	NOGDI
#include	"Windows.h"
#include	"avisynth.h"



class MVGroupOfFrames;

class MVFinest
:	public GenericVideoFilter {

private:

//   int mode;
//   bool fields;
//   PClip pelclip;
   //bool isse;
   int cpuFlags;

   int pixelsize;
   int bits_per_pixel;

   int nPel;

   PClip super; // v2.0
    int nSuperWidth;
    int nSuperHeight;
    int nSuperHPad;
    int nSuperVPad;
    MVGroupOfFrames *pRefGOF;
//   bool usePelClipHere;


//	YUY2Planes * RefPlanes;
//	YUY2Planes * DstPlanes;


public:
	MVFinest(PClip _super, bool isse, IScriptEnvironment* env);
	~MVFinest();
	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env) override;

  int __stdcall SetCacheHints(int cachehints, int frame_range) override {
    return cachehints == CACHE_GET_MTMODE ? MT_MULTI_INSTANCE : 0;
  }

};

#endif
