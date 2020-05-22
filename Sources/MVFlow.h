#ifndef __MV_FLOW__
#define __MV_FLOW__

#include "MVClip.h"
#include "MVFilter.h"
#include "SimpleResize.h"
#include "Time256ProviderCst.h"
#include "Time256ProviderPlane.h"
#include "yuy2planes.h"



class MVFlow
  : public GenericVideoFilter
  , public MVFilter
{

private:
  bool has_at_least_v8;

  enum { VECT_AMP = 256 };

  MVClip mvClip;
  int time256;
  int mode;
  bool fields;
//	PClip pelclip;
  //bool isse;
  int cpuFlags;
  bool planar;

  PClip finest; // v2.0
// 	PClip	timeclip; P.F. commented out (2.6.0.5?), could not resolve with 2.5.11.22 intended changes

  short *VXFullY; // fullframe vector mask
  short *VXFullUV;
  short *VYFullY;
  short *VYFullUV;

  short *VXSmallY; // Small vector mask
  short *VXSmallUV;
  short *VYSmallY;
  short *VYSmallUV;

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

  /* 2.5.11.22
    typedef	short	VectLut [VECT_AMP];
    VectLut	*LUTV; // [time256] [v] lookup table for v
   */
  SimpleResize *upsizer;
  SimpleResize *upsizerUV;

/*	template <class T256P> does not fit to 2.5.11.22 logic
  void Fetch(BYTE * pdst, int dst_pitch, const BYTE *pref, int ref_pitch,  short *VXFull, int VXPitch,  short *VYFull, int VYPitch, int width, int height, T256P &t256_provider);
  template <class T256P, int NPELL2>
  void Fetch_NPel(BYTE * pdst, int dst_pitch, const BYTE *pref, int ref_pitch,  short *VXFull, int VXPitch,  short *VYFull, int VYPitch, int width, int height, T256P &t256_provider);
*/
  template<typename pixel_t>
  void Fetch(BYTE * pdst8, int dst_pitch, const BYTE *pref8, int ref_pitch, short *VXFull, int VXPitch, short *VYFull, int VYPitch, int width, int height, int time256);
  template <typename pixel_t, int NPELL2>
  void Fetch_NPel(BYTE * pdst8, int dst_pitch, const BYTE *pref8, int ref_pitch, short *VXFull, int VXPitch, short *VYFull, int VYPitch, int width, int height, int time256);
/*
  template <class T256P> does not fit to 2.5.11.22 logic
  void Shift(BYTE * pdst, int dst_pitch, const BYTE *pref, int ref_pitch,  short *VXFull, int VXPitch,  short *VYFull, int VYPitch, int width, int height, T256P &t256_provider);
  template <class T256P, int NPELL2>
  void Shift_NPel(BYTE * pdst, int dst_pitch, const BYTE *pref, int ref_pitch,  short *VXFull, int VXPitch,  short *VYFull, int VYPitch, int width, int height, T256P &t256_provider);
*/
  template<typename pixel_t>
  void Shift(BYTE * pdst8, int dst_pitch, const BYTE *pref8, int ref_pitch, short *VXFull, int VXPitch, short *VYFull, int VYPitch, int width, int height, int time256);
  template <typename pixel_t, int NPELL2>
  void Shift_NPel(BYTE * pdst8, int dst_pitch, const BYTE *pref8, int ref_pitch, short *VXFull, int VXPitch, short *VYFull, int VYPitch, int width, int height, int time256);
  // 2.5.11.22	void Create_LUTV(int time256, VectLut plut);

  YUY2Planes * DstPlanes;

  bool is444;
  bool isGrey;
  bool isRGB; // avs+ planar
  //bool needDistinctChroma; buffer usage not yet optimized like in MFlowFPS

  int pixelsize_super;
  int bits_per_pixel_super;
  int pixelsize_super_shift;
  int planecount;
  int xRatioUVs[3];
  int yRatioUVs[3];
  int nLogxRatioUVs[3];
  int nLogyRatioUVs[3];

public:
  MVFlow(PClip _child, PClip _super, PClip _vectors, int _time256, int _mode, bool _fields,
    sad_t nSCD1, int nSCD2, bool isse, bool _planar, PClip _timeclip, IScriptEnvironment* env);
  ~MVFlow();
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env) override;

  int __stdcall SetCacheHints(int cachehints, int frame_range) override {
    return cachehints == CACHE_GET_MTMODE ? MT_MULTI_INSTANCE : 0;
  }

};

#endif
