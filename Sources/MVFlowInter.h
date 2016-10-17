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
   short *VXFullYB; //backward
   short *VXFullUVB;
   short *VYFullYB;
   short *VYFullUVB;
   short *VXFullYF;  // forward
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

	typedef	short	VectLut [VECT_AMP];
   VectLut	*LUTVB; // [time256] [v] lookup table for v
	VectLut	*LUTVF;


   SimpleResize *upsizer;
   SimpleResize *upsizerUV;

	YUY2Planes * DstPlanes;


public:
	MVFlowInter(PClip _child, PClip _finest, PClip _mvbw, PClip _mvfw, int _time256, double _ml,
                bool _blend, sad_t nSCD1, int nSCD2, bool isse, bool _planar, PClip _timeclip, IScriptEnvironment* env);
	~MVFlowInter();
	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
};

#endif
