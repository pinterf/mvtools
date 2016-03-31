#ifndef __MV_FLOWINTER__
#define __MV_FLOWINTER__

#include "MVClip.h"
#include "MVFilter.h"
#include "SimpleResize.h"
#include "yuy2planes.h"

class MVFlowInter
:	public GenericVideoFilter
,	public MVFilter
{
private:

	enum { VECT_AMP = 256	};

   MVClip mvClipB;
   MVClip mvClipF;
   int time256;
   double ml;
   PClip finest;
	PClip	timeclip;
   bool isse;
   bool planar;
   bool blend;

   // fullframe vector mask
   BYTE *VXFullYB; //backward
   BYTE *VXFullUVB;
   BYTE *VYFullYB;
   BYTE *VYFullUVB;
   BYTE *VXFullYF;  // forward
   BYTE *VXFullUVF;
   BYTE *VYFullYF;
   BYTE *VYFullUVF;
   BYTE *VXFullYBB; //backward backward
   BYTE *VXFullUVBB;
   BYTE *VYFullYBB;
   BYTE *VYFullUVBB;
   BYTE *VXFullYFF; // forward forward
   BYTE *VXFullUVFF;
   BYTE *VYFullYFF;
   BYTE *VYFullUVFF;

   // Small vector mask
   BYTE *VXSmallYB;
   BYTE *VXSmallUVB;
   BYTE *VYSmallYB;
   BYTE *VYSmallUVB;
   BYTE *VXSmallYF;
   BYTE *VXSmallUVF;
   BYTE *VYSmallYF;
   BYTE *VYSmallUVF;
   BYTE *VXSmallYBB;
   BYTE *VXSmallUVBB;
   BYTE *VYSmallYBB;
   BYTE *VYSmallUVBB;
   BYTE *VXSmallYFF;
   BYTE *VXSmallUVFF;
   BYTE *VYSmallYFF;
   BYTE *VYSmallUVFF;

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

	typedef	short	VectLut [VECT_AMP];
   VectLut	*LUTVB; // [time256] [v] lookup table for v
	VectLut	*LUTVF;


   SimpleResize *upsizer;
   SimpleResize *upsizerUV;

	YUY2Planes * DstPlanes;


public:
	MVFlowInter(PClip _child, PClip _finest, PClip _mvbw, PClip _mvfw, int _time256, double _ml,
                bool _blend, int nSCD1, int nSCD2, bool isse, bool _planar, PClip _timeclip, IScriptEnvironment* env);
	~MVFlowInter();
	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
};

#endif
