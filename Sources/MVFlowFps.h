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
//   PClip pelclip;
   bool isse;
   bool planar;
   bool blend;

   PClip finest; // v2.0

/*   PClip super; // v2.0
    int nSuperWidth;
    int nSuperHeight;
    int nSuperHPad;
    int nSuperVPad;
    MVGroupOfFrames *pRefGOFF, *pRefGOFB;
*/
//   bool usePelClipHere;

   int nleftLast;
   int nrightLast;

   __int64 fa, fb;

   // fullframe vector mask
   BYTE *VXFullYB; //backward
   BYTE *VXFullUVB;
   BYTE *VYFullYB;
   BYTE *VYFullUVB;
   BYTE *VXFullYF;
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

	short *LUTVB; // lookup table
	short *LUTVF;


/*	int pel2PitchY, pel2HeightY, pel2PitchUV, pel2HeightUV, pel2OffsetY, pel2OffsetUV;

   BYTE * pel2PlaneYB; // big plane
   BYTE * pel2PlaneUB;
   BYTE * pel2PlaneVB;
   BYTE * pel2PlaneYF; // big plane
   BYTE * pel2PlaneUF;
   BYTE * pel2PlaneVF;
*/
   SimpleResize *upsizer;
   SimpleResize *upsizerUV;
//	void FlowInter(BYTE * pdst, int dst_pitch, const BYTE *prefB, const BYTE *prefF, int ref_pitch,
//			   BYTE *VXFullB, BYTE *VXFullF, BYTE *VYFullB, BYTE *VYFullF, BYTE *MaskB, BYTE *MaskF,
//			   int VPitch, int width, int height, int time256);
//	void FlowInterExtra(BYTE * pdst, int dst_pitch, const BYTE *prefB, const BYTE *prefF, int ref_pitch,
//			   BYTE *VXFullB, BYTE *VXFullF, BYTE *VYFullB, BYTE *VYFullF, BYTE *MaskB, BYTE *MaskF,
//			   int VPitch, int width, int height, int time256, BYTE *VXFullBB, BYTE *VXFullFF, BYTE *VYFullBB, BYTE *VYFullFF);
//	void Create_LUTV(int time256, int *LUTVB, int *LUTVF);

//	YUY2Planes * SrcPlanes;
//	YUY2Planes * RefPlanes;
	YUY2Planes * DstPlanes;

//	YUY2Planes * Ref2xPlanes;
//	YUY2Planes * Src2xPlanes;

//    bool isSuper;
//    MVGroupOfFrames *pRefFGOF, *pRefBGOF;
//    int nSuperModeYUV;

public:
	MVFlowFps(PClip _child, PClip _super, PClip _mvbw, PClip _mvfw, unsigned int _num, unsigned int _den, int _maskmode, double _ml,
                bool _blend, int nSCD1, int nSCD2, bool isse, bool _planar, IScriptEnvironment* env);
	~MVFlowFps();
	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
};

#endif
