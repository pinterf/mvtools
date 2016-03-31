#ifndef __MV_INTER__
#define __MV_INTER__

#include "CopyCode.h"
#include "MVClip.h"
#include "MVFilter.h"
#include "SimpleResize.h"
#include "yuy2planes.h"



class MVGroupOfFrames;

/*! \brief Filter that change fps by blocks moving
 */
class MVBlockFps
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
   int mode;
   int thres;
   PClip super;
   bool isse2;
   bool planar;
   bool blend;

   __int64 fa, fb;

   int nSuperModeYUV;

   BYTE *MaskFullYB; // shifted (projected) images planes
   BYTE *MaskFullUVB;
   BYTE *MaskFullYF;
   BYTE *MaskFullUVF;

   BYTE *MaskOccY; // full frame occlusion mask
   BYTE *MaskOccUV;

   BYTE *smallMaskF;// small forward occlusion mask
   BYTE *smallMaskB; // backward
   BYTE *smallMaskO; // both

   BYTE *OnesBlock; // block of ones

   int nWidthP, nHeightP, nPitchY, nPitchUV, nHeightPUV, nWidthPUV, nHeightUV, nWidthUV;
   int nBlkXP, nBlkYP;

   COPYFunction *BLITLUMA;
   COPYFunction *BLITCHROMA;

	YUY2Planes * DstPlanes;

	MVGroupOfFrames *pRefBGOF;
	MVGroupOfFrames *pRefFGOF;

	void MakeSmallMask(BYTE *image, int imagePitch, BYTE *smallmask, int nBlkX, int nBlkY, int nBlkSizeX, int nBlkSizeY, int threshold);
	void InflateMask(BYTE *smallmask, int nBlkX, int nBlkY);
	void MultMasks(BYTE *smallmaskF, BYTE *smallmaskB, BYTE *smallmaskO,  int nBlkX, int nBlkY);
	void ResultBlock(BYTE *pDst, int dst_pitch, const BYTE * pMCB, int MCB_pitch, const BYTE * pMCF, int MCF_pitch,
		const BYTE * pRef, int ref_pitch, const BYTE * pSrc, int src_pitch, BYTE *maskB, int mask_pitch, BYTE *maskF,
		BYTE *pOcc, int nBlkSizeX, int nBlkSizeY, int time256, int mode);

	SimpleResize *upsizer;
   SimpleResize *upsizerUV;

   int nSuperHPad, nSuperVPad;

public:
	MVBlockFps(
		PClip _child, PClip _super, PClip _mvbw, PClip _mvfw,
		unsigned int _num, unsigned int _den, int _mode, int _thres, bool _blend,
		int nSCD1, int nSCD2, bool isse, bool _planar, bool mt_flag,
		IScriptEnvironment* env
	);
	~MVBlockFps();
	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
};

#endif
