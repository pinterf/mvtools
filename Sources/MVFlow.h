#ifndef __MV_FLOW__
#define __MV_FLOW__

#include "MVClip.h"
#include "MVFilter.h"
#include "SimpleResize.h"
#include "Time256ProviderCst.h"
#include "Time256ProviderPlane.h"
#include "yuy2planes.h"



class MVFlow
:	public GenericVideoFilter
,	public MVFilter
{

private:

	enum { VECT_AMP = 256	};

   MVClip mvClip;
   int time256;
   int mode;
   bool fields;
//	PClip pelclip;
   bool isse;
   bool planar;

   PClip finest; // v2.0
	PClip	timeclip;

   BYTE *VXFullY; // fullframe vector mask
   BYTE *VXFullUV;
   BYTE *VYFullY;
   BYTE *VYFullUV;

   BYTE *VXSmallY; // Small vector mask
   BYTE *VXSmallUV;
   BYTE *VYSmallY;
   BYTE *VYSmallUV;

	int nBlkXP;
	int nBlkYP;
	int nWidthP;
	int nHeightP;
	int nWidthPUV;
	int nHeightPUV;

	int VPitchY;
	int VPitchUV;

	int nWidthUV;
	int nHeightUV;

	int nHPaddingUV;
	int nVPaddingUV;

	typedef	short	VectLut [VECT_AMP];
   VectLut	*LUTV; // [time256] [v] lookup table for v

   SimpleResize *upsizer;
   SimpleResize *upsizerUV;

	template <class T256P>
	void Fetch(BYTE * pdst, int dst_pitch, const BYTE *pref, int ref_pitch,  BYTE *VXFull, int VXPitch,  BYTE *VYFull, int VYPitch, int width, int height, T256P &t256_provider);
	template <class T256P, int NPELL2>
	void Fetch_NPel(BYTE * pdst, int dst_pitch, const BYTE *pref, int ref_pitch,  BYTE *VXFull, int VXPitch,  BYTE *VYFull, int VYPitch, int width, int height, T256P &t256_provider);

	template <class T256P>
	void Shift(BYTE * pdst, int dst_pitch, const BYTE *pref, int ref_pitch,  BYTE *VXFull, int VXPitch,  BYTE *VYFull, int VYPitch, int width, int height, T256P &t256_provider);
	template <class T256P, int NPELL2>
	void Shift_NPel(BYTE * pdst, int dst_pitch, const BYTE *pref, int ref_pitch,  BYTE *VXFull, int VXPitch,  BYTE *VYFull, int VYPitch, int width, int height, T256P &t256_provider);

	void Create_LUTV(int time256, VectLut plut);

	YUY2Planes * DstPlanes;

public:
	MVFlow(PClip _child, PClip _super, PClip _vectors, int _time256, int _mode, bool _fields,
                int nSCD1, int nSCD2, bool isse, bool _planar, PClip _timeclip, IScriptEnvironment* env);
	~MVFlow();
	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
};

#endif
