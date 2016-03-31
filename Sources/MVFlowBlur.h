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
   bool isse;
   bool planar;

//    bool usePelClipHere;

  // fullframe vector mask
   BYTE *VXFullYB; //backward
   BYTE *VXFullUVB;
   BYTE *VYFullYB;
   BYTE *VYFullUVB;
   BYTE *VXFullYF;
   BYTE *VXFullUVF;
   BYTE *VYFullYF;
   BYTE *VYFullUVF;

   // Small vector mask
   BYTE *VXSmallYB;
   BYTE *VXSmallUVB;
   BYTE *VYSmallYB;
   BYTE *VYSmallUVB;
   BYTE *VXSmallYF;
   BYTE *VXSmallUVF;
   BYTE *VYSmallYF;
   BYTE *VYSmallUVF;

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
	void FlowBlur(BYTE * pdst, int dst_pitch, const BYTE *prefB, int ref_pitch,
			   BYTE *VXFullB, BYTE *VXFullF, BYTE *VYFullB, BYTE *VYFullF,
			   int VPitch, int width, int height, int time256, int nb);

	YUY2Planes * DstPlanes;

public:
	MVFlowBlur(PClip _child, PClip _finest, PClip _mvbw, PClip _mvfw, int _blur256, int _prec,
                int nSCD1, int nSCD2, bool isse, bool _planar, IScriptEnvironment* env);
	~MVFlowBlur();
	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
};

#endif
