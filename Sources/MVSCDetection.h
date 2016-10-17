#ifndef __MV_SCDETECTION__
#define __MV_SCDETECTION__


#include "MVClip.h"
#include "MVFilter.h"

#define	NOGDI
#define	NOMINMAX
#define	WIN32_LEAN_AND_MEAN
#include "Windows.h"
#include "avisynth.h"



class MVSCDetection
:	public GenericVideoFilter
,	public MVFilter
{
private:

	MVClip mvClip;
   int sceneChangeValue;
   float sceneChangeValue_f;
   int pixelsize;
   int bits_per_pixel;

public:
	MVSCDetection(::PClip _child, ::PClip vectors, float nSceneChangeValue, sad_t nSCD1, int nSCD2, bool isse, IScriptEnvironment* env);
	~MVSCDetection();
	::PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
};

#endif
