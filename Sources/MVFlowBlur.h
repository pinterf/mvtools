#ifndef __MV_FLOWBLUR__
#define __MV_FLOWBLUR__

#include "MVClip.h"
#include "MVFilter.h"
#include "SimpleResize.h"
#include "yuy2planes.h"

class MVFlowBlur
:	public GenericVideoFilter
,	public MVFilter
{
private:

   MVClip mvClipB;
   MVClip mvClipF;
   int blur256; // blur time interval
   int prec; // blur precision (pixels)
   PClip finest;
   //bool isse;
   int cpuFlags;
   bool planar;

//    bool usePelClipHere;

  // fullframe vector mask
   short *VXFullYB; //backward
   short *VXFullUVB;
   short *VYFullYB;
   short *VYFullUVB;
   short *VXFullYF;
   short *VXFullUVF;
   short *VYFullYF;
   short *VYFullUVF;

   // Small vector mask
   short *VXSmallYB;
   short *VXSmallUVB;
   short *VYSmallYB;
   short *VYSmallUVB;
   short *VXSmallYF;
   short *VXSmallUVF;
   short *VYSmallYF;
   short *VYSmallUVF;

   BYTE *MaskSmallB;
   BYTE *MaskFullYB;
   BYTE *MaskFullUVB;
   BYTE *MaskSmallF;
   BYTE *MaskFullYF;
   BYTE *MaskFullUVF;

   int nWidthUV;
   int nHeightUV;
	int	VPitchY, VPitchUV;

	 int nHPaddingUV;
	 int nVPaddingUV;

   SimpleResize *upsizer;
   SimpleResize *upsizerUV;
   template<typename pixel_t, int nLOGPEL>
   void FlowBlur(BYTE * pdst, int dst_pitch, const BYTE *prefB, int ref_pitch,
     short *VXFullB, short *VXFullF, short *VYFullB, short *VYFullF,
     int VPitch, int width, int height, int time256, int nb);

	YUY2Planes * DstPlanes;

public:
	MVFlowBlur(PClip _child, PClip _finest, PClip _mvbw, PClip _mvfw, int _blur256, int _prec,
                int nSCD1, int nSCD2, bool isse, bool _planar, IScriptEnvironment* env);
	~MVFlowBlur();
	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);

  int __stdcall SetCacheHints(int cachehints, int frame_range) override {
    return cachehints == CACHE_GET_MTMODE ? MT_MULTI_INSTANCE : 0;
  }

};

#endif
