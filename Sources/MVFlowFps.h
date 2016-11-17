#ifndef __MV_FLOWFPS__
#define __MV_FLOWFPS__

#include "MVClip.h"
#include "MVFilter.h"
#include "SimpleResize.h"
#include "yuy2planes.h"

class MVFlowFps
:	public GenericVideoFilter
,	public MVFilter
{
private:

   MVClip mvClipB;
   MVClip mvClipF;
  	unsigned int numerator;
	unsigned int denominator;
  	unsigned int numeratorOld;
	unsigned int denominatorOld;
	int maskmode;
   double ml;
   bool isse;
   bool planar;
   bool blend;

   PClip finest; // v2.0

   int nleftLast;
   int nrightLast;

   __int64 fa, fb;

   // fullframe vector mask
   short *VXFullYB; //backward
   short *VXFullUVB;
   short *VYFullYB;
   short *VYFullUVB;
   short *VXFullYF;
   short *VXFullUVF;
   short *VYFullYF;
   short *VYFullUVF;
   short *VXFullYBB; //backward backward
   short *VXFullUVBB;
   short *VYFullYBB;
   short *VYFullUVBB;
   short *VXFullYFF; // forward forward
   short *VXFullUVFF;
   short *VYFullYFF;
   short *VYFullUVFF;

   // Small vector mask
   short *VXSmallYB;
   short *VXSmallUVB;
   short *VYSmallYB;
   short *VYSmallUVB;
   short *VXSmallYF;
   short *VXSmallUVF;
   short *VYSmallYF;
   short *VYSmallUVF;
   short *VXSmallYBB;
   short *VXSmallUVBB;
   short *VYSmallYBB;
   short *VYSmallUVBB;
   short *VXSmallYFF;
   short *VXSmallUVFF;
   short *VYSmallYFF;
   short *VYSmallUVFF;

   BYTE *MaskSmallB;
   BYTE *MaskFullYB;
   BYTE *MaskFullUVB;
   BYTE *MaskSmallF;
   BYTE *MaskFullYF;
   BYTE *MaskFullUVF;

   BYTE *SADMaskSmallB;
   BYTE *SADMaskSmallF;

  int nWidthUV;
   int nHeightUV;
	int	VPitchY, VPitchUV;

	int nBlkXP;
	 int nBlkYP;
	 int nWidthP;
	 int nHeightP;
	 int nWidthPUV;
	 int nHeightPUV;

	 int nHPaddingUV;
	 int nVPaddingUV;

   SimpleResize *upsizer;
   SimpleResize *upsizerUV;

	YUY2Planes * DstPlanes;

public:
	MVFlowFps(PClip _child, PClip _super, PClip _mvbw, PClip _mvfw, unsigned int _num, unsigned int _den, int _maskmode, double _ml,
                bool _blend, sad_t nSCD1, int nSCD2, bool isse, bool _planar, IScriptEnvironment* env);
	~MVFlowFps();
	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);

  int __stdcall SetCacheHints(int cachehints, int frame_range) override {
    return cachehints == CACHE_GET_MTMODE ? MT_MULTI_INSTANCE : 0;
  }

};

#endif
