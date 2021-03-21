#ifndef __MV_SCDETECTION__
#define __MV_SCDETECTION__


#include "MVClip.h"
#include "MVFilter.h"
#include "avisynth.h"



class MVSCDetection
:	public GenericVideoFilter
,	public MVFilter
{
private:
  bool has_at_least_v8;

	MVClip mvClip;
   int sceneChangeValue;
   float sceneChangeValue_f;

public:
	MVSCDetection(::PClip _child, ::PClip vectors, float nSceneChangeValue, sad_t nSCD1, int nSCD2, bool isse, IScriptEnvironment* env);
	~MVSCDetection();
	::PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env) override;

  int __stdcall SetCacheHints(int cachehints, int frame_range) override {
    return cachehints == CACHE_GET_MTMODE ? MT_MULTI_INSTANCE : 0;
  }

};

#endif
