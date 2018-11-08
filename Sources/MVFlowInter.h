#ifndef __MV_FLOWINTER__
#define __MV_FLOWINTER__

#include "MVClip.h"
#include "MVFilter.h"
#include "SimpleResize.h"
#include "yuy2planes.h"

class MVFlowInter
  : public GenericVideoFilter
  , public MVFilter
{
private:

  enum { VECT_AMP = 256 };

  MVClip mvClipB;
  MVClip mvClipF;
  int time256;
  double ml;
  PClip finest;
  int cpuFlags;
  bool planar;
  bool blend;

  // fullframe vector mask, common for all planes
  short *VXFull_B; //backward
  short *VYFull_B;
  short *VXFull_F; // forward
  short *VYFull_F;
  short *VXFull_BB; //backward backward
  short *VYFull_BB;
  short *VXFull_FF; // forward forward
  short *VYFull_FF;

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
  BYTE *MaskFull_B; // common for all planes
  BYTE *MaskSmallF;
  BYTE *MaskFull_F; // common for all planes

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
  MVFlowInter(PClip _child, PClip _finest, PClip _mvbw, PClip _mvfw, int _time256, double _ml,
    bool _blend, sad_t nSCD1, int nSCD2, bool isse, bool _planar, IScriptEnvironment* env);
  ~MVFlowInter();
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);

  int __stdcall SetCacheHints(int cachehints, int frame_range) override {
    return cachehints == CACHE_GET_MTMODE ? MT_MULTI_INSTANCE : 0;
  }

};

#endif
