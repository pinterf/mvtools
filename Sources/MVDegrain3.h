#ifndef __MV_DEGRAIN3__
#define __MV_DEGRAIN3__

#include "CopyCode.h"
#include "MVClip.h"
#include "MVFilter.h"
#include "overlap.h"
#include "yuy2planes.h"
#include <stdint.h>
#include "def.h"

class MVGroupOfFrames;
class MVPlane;
class MVFilter;

#define MAX_DEGRAIN 6

//#define LEVEL_IS_TEMPLATE
/*
using Denoise1to5Function = void (*)(
BYTE *pDst, BYTE *pDstLsb, bool lsb_flag, int nDstPitch, const BYTE *pSrc, int nSrcPitch,
const BYTE *pRefB[MAX_DEGRAIN], int BPitch[MAX_DEGRAIN], const BYTE *pRefF[MAX_DEGRAIN], int FPitch[MAX_DEGRAIN],
int WSrc,
int WRefB[MAX_DEGRAIN], int WRefF[MAX_DEGRAIN]
);
*/
typedef void (Denoise1to6Function)(
  BYTE *pDst, BYTE *pDstLsb, bool lsb_flag, int nDstPitch, const BYTE *pSrc, int nSrcPitch,
  const BYTE *pRefB[MAX_DEGRAIN], int BPitch[MAX_DEGRAIN], const BYTE *pRefF[MAX_DEGRAIN], int FPitch[MAX_DEGRAIN],
  int WSrc,
  int WRefB[MAX_DEGRAIN], int WRefF[MAX_DEGRAIN]
  );

/*! \brief Filter that denoise the picture
*/

#ifdef LEVEL_IS_TEMPLATE
template<int level> // PF level as template
#endif
class MVDegrainX
  : public GenericVideoFilter
  , public MVFilter
{
private:
  typedef void (norm_weights_Function_t)(int &WSrc, int(&WRefB)[MAX_DEGRAIN], int(&WRefF)[MAX_DEGRAIN]);


  MVClip *mvClipB[MAX_DEGRAIN];
  MVClip *mvClipF[MAX_DEGRAIN];
  sad_t thSAD;
  sad_t thSADpow2;
  sad_t thSADC;
  sad_t thSADCpow2;
  int YUVplanes;
  sad_t nLimit;
  sad_t nLimitC;
  PClip super;
  bool isse_flag;
  bool planar;
  bool lsb_flag;
  int height_lsb_mul;
  //int pixelsize; // in MVFilter
  //int bits_per_pixel; // in MVFilter
  int pixelsize_super; // PF not param, from create
  int bits_per_pixel_super; // PF not param, from create
  int pixelsize_super_shift;

  int nSuperModeYUV;

  YUY2Planes * DstPlanes;
  YUY2Planes * SrcPlanes;

  OverlapWindows *OverWins;
  OverlapWindows *OverWinsUV;

  OverlapsFunction *OVERSLUMA;
  OverlapsFunction *OVERSCHROMA;
  OverlapsFunction *OVERSLUMA16;
  OverlapsFunction *OVERSCHROMA16;
  OverlapsLsbFunction *OVERSLUMALSB;
  OverlapsLsbFunction *OVERSCHROMALSB;
  Denoise1to6Function *DEGRAINLUMA;
  Denoise1to6Function *DEGRAINCHROMA;

#ifndef LEVEL_IS_TEMPLATE
  norm_weights_Function_t *NORMWEIGHTS;
#endif

  MVGroupOfFrames *pRefBGOF[MAX_DEGRAIN], *pRefFGOF[MAX_DEGRAIN];
  //MVGroupOfFrames *pRefB2GOF, *pRefF2GOF;
  //MVGroupOfFrames *pRefB3GOF, *pRefF3GOF;

  unsigned char *tmpBlock;
  unsigned char *tmpBlockLsb;	// Not allocated, it's just a reference to a part of the tmpBlock area (or 0 if no LSB)
  unsigned short * DstShort;
  int dstShortPitch;
  int * DstInt;
  int dstIntPitch;

#ifndef LEVEL_IS_TEMPLATE
  const int level;
#endif
  int framenumber;

public:
  MVDegrainX(PClip _child, PClip _super, 
    PClip _mvbw, PClip _mvfw, PClip _mvbw2, PClip _mvfw2, PClip _mvbw3, PClip _mvfw3, PClip _mvbw4, PClip _mvfw4, PClip _mvbw5, PClip _mvfw5, PClip _mvbw6, PClip _mvfw6,
    sad_t _thSAD, sad_t _thSADC, int _YUVplanes, sad_t _nLimit, sad_t _nLimitC,
    sad_t _nSCD1, int _nSCD2, bool _isse2, bool _planar, bool _lsb_flag,
    bool _mt_flag, 
#ifndef LEVEL_IS_TEMPLATE
    int _level, 
#endif
    IScriptEnvironment* env_ptr);
  ~MVDegrainX();
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);

  int __stdcall SetCacheHints(int cachehints, int frame_range) override {
    return cachehints == CACHE_GET_MTMODE ? MT_MULTI_INSTANCE : 0;
  }

private:
  inline void process_chroma(int plane_mask, BYTE *pDst, BYTE *pDstCur, int nDstPitch, const BYTE *pSrc, const BYTE *pSrcCur, int nSrcPitch,
    bool isUsableB[MAX_DEGRAIN], bool isUsableF[MAX_DEGRAIN], MVPlane *pPlanesB[MAX_DEGRAIN], MVPlane *pPlanesF[MAX_DEGRAIN],
    int lsb_offset_uv, int nWidth_B, int nHeight_B);
  // MV_FORCEINLINE void	process_chroma(int plane_mask, BYTE *pDst, BYTE *pDstCur, int nDstPitch, const BYTE *pSrc, const BYTE *pSrcCur, int nSrcPitch, bool isUsableB, bool isUsableF, bool isUsableB2, bool isUsableF2, bool isUsableB3, bool isUsableF3, MVPlane *pPlanesB, MVPlane *pPlanesF, MVPlane *pPlanesB2, MVPlane *pPlanesF2, MVPlane *pPlanesB3, MVPlane *pPlanesF3, int lsb_offset_uv, int nWidth_B, int nHeight_B);
  MV_FORCEINLINE void	use_block_y(const BYTE * &p, int &np, int &WRef, bool isUsable, const MVClip &mvclip, int i, const MVPlane *pPlane, const BYTE *pSrcCur, int xx, int nSrcPitch);
  MV_FORCEINLINE void	use_block_uv(const BYTE * &p, int &np, int &WRef, bool isUsable, const MVClip &mvclip, int i, const MVPlane *pPlane, const BYTE *pSrcCur, int xx, int nSrcPitch);
  // static MV_FORCEINLINE void	norm_weights(int &WSrc, int &WRefB, int &WRefF, int &WRefB2, int &WRefF2, int &WRefB3, int &WRefF3);
  Denoise1to6Function *get_denoise123_function(int BlockX, int BlockY, int _pixelsize, bool _lsb_flag, int _level, arch_t _arch);
};

template<typename pixel_t, int blockWidth, int blockHeight, bool lsb_flag, int level >
void Degrain1to6_C(uint8_t *pDst, BYTE *pDstLsb, bool _lsb_flag_not_used_template_rulez, int nDstPitch, const uint8_t *pSrc, int nSrcPitch,
  const uint8_t *pRefB[MAX_DEGRAIN], int BPitch[MAX_DEGRAIN], const uint8_t *pRefF[MAX_DEGRAIN], int FPitch[MAX_DEGRAIN],
  int WSrc,
  int WRefB[MAX_DEGRAIN], int WRefF[MAX_DEGRAIN])
{
  const bool no_need_round = lsb_flag || (sizeof(pixel_t) > 1);
  for (int h = 0; h < blockHeight; h++)
  {
    for (int x = 0; x < blockWidth; x++)
    {
      if (level == 1) {
        const int		val = reinterpret_cast<const pixel_t *>(pSrc)[x] * WSrc +
          reinterpret_cast<const pixel_t *>(pRefF[0])[x] * WRefF[0] + reinterpret_cast<const pixel_t *>(pRefB[0])[x] * WRefB[0];
        reinterpret_cast<pixel_t *>(pDst)[x] = (val + (no_need_round ? 0 : 128)) >> 8;
        if (lsb_flag)
          pDstLsb[x] = val & 255;
      }
      else if (level == 2) {
        const int		val = reinterpret_cast<const pixel_t *>(pSrc)[x] * WSrc +
          reinterpret_cast<const pixel_t *>(pRefF[0])[x] * WRefF[0] + reinterpret_cast<const pixel_t *>(pRefB[0])[x] * WRefB[0] +
          reinterpret_cast<const pixel_t *>(pRefF[1])[x] * WRefF[1] + reinterpret_cast<const pixel_t *>(pRefB[1])[x] * WRefB[1];
        reinterpret_cast<pixel_t *>(pDst)[x] = (val + (no_need_round ? 0 : 128)) >> 8;
        if (lsb_flag)
          pDstLsb[x] = val & 255;
      }
      else if (level == 3) {
        const int		val = reinterpret_cast<const pixel_t *>(pSrc)[x] * WSrc +
          reinterpret_cast<const pixel_t *>(pRefF[0])[x] * WRefF[0] + reinterpret_cast<const pixel_t *>(pRefB[0])[x] * WRefB[0] +
          reinterpret_cast<const pixel_t *>(pRefF[1])[x] * WRefF[1] + reinterpret_cast<const pixel_t *>(pRefB[1])[x] * WRefB[1] +
          reinterpret_cast<const pixel_t *>(pRefF[2])[x] * WRefF[2] + reinterpret_cast<const pixel_t *>(pRefB[2])[x] * WRefB[2];
        reinterpret_cast<pixel_t *>(pDst)[x] = (val + (no_need_round ? 0 : 128)) >> 8;
        if (lsb_flag)
          pDstLsb[x] = val & 255;
      }
      else if (level == 4) {
        const int		val = reinterpret_cast<const pixel_t *>(pSrc)[x] * WSrc +
          reinterpret_cast<const pixel_t *>(pRefF[0])[x] * WRefF[0] + reinterpret_cast<const pixel_t *>(pRefB[0])[x] * WRefB[0] +
          reinterpret_cast<const pixel_t *>(pRefF[1])[x] * WRefF[1] + reinterpret_cast<const pixel_t *>(pRefB[1])[x] * WRefB[1] +
          reinterpret_cast<const pixel_t *>(pRefF[2])[x] * WRefF[2] + reinterpret_cast<const pixel_t *>(pRefB[2])[x] * WRefB[2] +
          reinterpret_cast<const pixel_t *>(pRefF[3])[x] * WRefF[3] + reinterpret_cast<const pixel_t *>(pRefB[3])[x] * WRefB[3];
        reinterpret_cast<pixel_t *>(pDst)[x] = (val + (no_need_round ? 0 : 128)) >> 8;
        if (lsb_flag)
          pDstLsb[x] = val & 255;
      }
      else if (level == 5) {
        const int		val = reinterpret_cast<const pixel_t *>(pSrc)[x] * WSrc +
          reinterpret_cast<const pixel_t *>(pRefF[0])[x] * WRefF[0] + reinterpret_cast<const pixel_t *>(pRefB[0])[x] * WRefB[0] +
          reinterpret_cast<const pixel_t *>(pRefF[1])[x] * WRefF[1] + reinterpret_cast<const pixel_t *>(pRefB[1])[x] * WRefB[1] +
          reinterpret_cast<const pixel_t *>(pRefF[2])[x] * WRefF[2] + reinterpret_cast<const pixel_t *>(pRefB[2])[x] * WRefB[2] +
          reinterpret_cast<const pixel_t *>(pRefF[3])[x] * WRefF[3] + reinterpret_cast<const pixel_t *>(pRefB[3])[x] * WRefB[3] +
          reinterpret_cast<const pixel_t *>(pRefF[4])[x] * WRefF[4] + reinterpret_cast<const pixel_t *>(pRefB[4])[x] * WRefB[4];
        reinterpret_cast<pixel_t *>(pDst)[x] = (val + (no_need_round ? 0 : 128)) >> 8;
        if (lsb_flag)
          pDstLsb[x] = val & 255;
      }
      else if (level == 6) {
        const int		val = reinterpret_cast<const pixel_t *>(pSrc)[x] * WSrc +
          reinterpret_cast<const pixel_t *>(pRefF[0])[x] * WRefF[0] + reinterpret_cast<const pixel_t *>(pRefB[0])[x] * WRefB[0] +
          reinterpret_cast<const pixel_t *>(pRefF[1])[x] * WRefF[1] + reinterpret_cast<const pixel_t *>(pRefB[1])[x] * WRefB[1] +
          reinterpret_cast<const pixel_t *>(pRefF[2])[x] * WRefF[2] + reinterpret_cast<const pixel_t *>(pRefB[2])[x] * WRefB[2] +
          reinterpret_cast<const pixel_t *>(pRefF[3])[x] * WRefF[3] + reinterpret_cast<const pixel_t *>(pRefB[3])[x] * WRefB[3] +
          reinterpret_cast<const pixel_t *>(pRefF[4])[x] * WRefF[4] + reinterpret_cast<const pixel_t *>(pRefB[4])[x] * WRefB[4] +
          reinterpret_cast<const pixel_t *>(pRefF[5])[x] * WRefF[5] + reinterpret_cast<const pixel_t *>(pRefB[5])[x] * WRefB[5];
        reinterpret_cast<pixel_t *>(pDst)[x] = (val + (no_need_round ? 0 : 128)) >> 8;
        if (lsb_flag)
          pDstLsb[x] = val & 255;
      }
    }
    pDst += nDstPitch;
    if (lsb_flag)
      pDstLsb += nDstPitch;
    pSrc += nSrcPitch;
    pRefB[0] += BPitch[0];
    pRefF[0] += FPitch[0];
    if (level >= 2) {
      pRefB[1] += BPitch[1];
      pRefF[1] += FPitch[1];
      if (level >= 3) {
        pRefB[2] += BPitch[2];
        pRefF[2] += FPitch[2];
        if (level >= 4) {
          pRefB[3] += BPitch[3];
          pRefF[3] += FPitch[3];
          if (level >= 5) {
            pRefB[4] += BPitch[4];
            pRefF[4] += FPitch[4];
            if (level >= 6) {
              pRefB[5] += BPitch[5];
              pRefF[5] += FPitch[5];
            }
          }
        }
      }
    }
  }
}


#ifndef _M_X64
template<int blockWidth, int blockHeight, bool lsb_flag, int level >
void Degrain1to6_mmx(BYTE *pDst, BYTE *pDstLsb, bool _lsb_flag_not_used_template_rulez, int nDstPitch, const BYTE *pSrc, int nSrcPitch,
  const BYTE *pRefB[MAX_DEGRAIN], int BPitch[MAX_DEGRAIN], const BYTE *pRefF[MAX_DEGRAIN], int FPitch[MAX_DEGRAIN],
  int WSrc,
  int WRefB[MAX_DEGRAIN], int WRefF[MAX_DEGRAIN])
{
  __m64 z = _mm_setzero_si64();
  __m64 ws = _mm_set1_pi16(WSrc);
  __m64 wb1, wf1;
  __m64 wb2, wf2;
  __m64 wb3, wf3;
  __m64 wb4, wf4;
  __m64 wb5, wf5;
  __m64 wb6, wf6;
  wb1 = _mm_set1_pi16(WRefB[0]);
  wf1 = _mm_set1_pi16(WRefF[0]);
  if (level >= 2) {
    wb2 = _mm_set1_pi16(WRefB[1]);
    wf2 = _mm_set1_pi16(WRefF[1]);
    if (level >= 3) {
      wb3 = _mm_set1_pi16(WRefB[2]);
      wf3 = _mm_set1_pi16(WRefF[2]);
      if (level >= 4) {
        wb4 = _mm_set1_pi16(WRefB[3]);
        wf4 = _mm_set1_pi16(WRefF[3]);
        if (level >= 5) {
          wb5 = _mm_set1_pi16(WRefB[4]);
          wf5 = _mm_set1_pi16(WRefF[4]);
          if (level >= 6) {
            wb6 = _mm_set1_pi16(WRefB[5]);
            wf6 = _mm_set1_pi16(WRefF[5]);
          }
        }
      }
    }
  }

  if (lsb_flag)
  {
    __m64 m = _mm_set1_pi16(255);
    for (int h = 0; h < blockHeight; h++)
    {
      for (int x = 0; x < blockWidth; x += 4)
      {
        __m64	val;
        if (level == 6) {
          val =
            _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pSrc + x), z), ws),
              _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefB[0] + x), z), wb1),
                _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefF[0] + x), z), wf1),
                  _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefB[1] + x), z), wb2),
                    _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefF[1] + x), z), wf2),
                      _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefB[2] + x), z), wb3),
                        _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefF[2] + x), z), wf3),
                          _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefB[3] + x), z), wb4),
                            _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefF[3] + x), z), wf4),
                              _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefB[4] + x), z), wb5),
                                _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefF[4] + x), z), wf5),
                                  _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefB[5] + x), z), wb6),
                                     _m_pmullw(_m_punpcklbw(*(__m64*)(pRefF[5] + x), z), wf6)))))))))))));
        }
        if (level == 5) {
          val =
            _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pSrc + x), z), ws),
              _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefB[0] + x), z), wb1),
                _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefF[0] + x), z), wf1),
                  _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefB[1] + x), z), wb2),
                    _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefF[1] + x), z), wf2),
                      _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefB[2] + x), z), wb3),
                        _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefF[2] + x), z), wf3),
                          _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefB[3] + x), z), wb4),
                            _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefF[3] + x), z), wf4),
                              _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefB[4] + x), z), wb5),
                                _m_pmullw(_m_punpcklbw(*(__m64*)(pRefF[4] + x), z), wf5)))))))))));
        }
        if (level == 4) {
          val =
            _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pSrc + x), z), ws),
              _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefB[0] + x), z), wb1),
                _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefF[0] + x), z), wf1),
                  _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefB[1] + x), z), wb2),
                    _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefF[1] + x), z), wf2),
                      _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefB[2] + x), z), wb3),
                        _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefF[2] + x), z), wf3),
                          _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefB[3] + x), z), wb4),
                            _m_pmullw(_m_punpcklbw(*(__m64*)(pRefF[3] + x), z), wf4)))))))));
        }
        if (level == 3) {
          val =
            _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pSrc + x), z), ws),
              _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefB[0] + x), z), wb1),
                _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefF[0] + x), z), wf1),
                  _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefB[1] + x), z), wb2),
                    _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefF[1] + x), z), wf2),
                      _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefB[2] + x), z), wb3),
                        _m_pmullw(_m_punpcklbw(*(__m64*)(pRefF[2] + x), z), wf3)))))));
        }
        else if (level == 2) {
          val =
            _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pSrc + x), z), ws),
              _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefB[0] + x), z), wb1),
                _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefF[0] + x), z), wf1),
                  _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefB[1] + x), z), wb2),
                    _m_pmullw(_m_punpcklbw(*(__m64*)(pRefF[1] + x), z), wf2)))));
        }
        else if (level == 1) {
          val =
            _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pSrc + x), z), ws),
              _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefB[0] + x), z), wb1),
                _m_pmullw(_m_punpcklbw(*(__m64*)(pRefF[0] + x), z), wf1)));
        }

        *(uint32_t *)(pDst + x) = _m_to_int(_m_packuswb(_m_psrlwi(val, 8), z));
        *(uint32_t *)(pDstLsb + x) = _m_to_int(_m_packuswb(_m_psrlwi(val, 8), z));
      }
      pDst += nDstPitch;
      pDstLsb += nDstPitch;
      pSrc += nSrcPitch;
      pRefB[0] += BPitch[0];
      pRefF[0] += FPitch[0];
      if (level >= 2) {
        pRefB[1] += BPitch[1];
        pRefF[1] += FPitch[1];
        if (level >= 3) {
          pRefB[2] += BPitch[2];
          pRefF[2] += FPitch[2];
        }
        if (level >= 4) {
          pRefB[3] += BPitch[3];
          pRefF[3] += FPitch[3];
        }
        if (level >= 5) {
          pRefB[4] += BPitch[4];
          pRefF[4] += FPitch[4];
          if (level >= 6) {
            pRefB[5] += BPitch[5];
            pRefF[5] += FPitch[5];
          }
        }
      }

    }
  }

  else
  {
    __m64 o = _mm_set1_pi16(128);
    for (int h = 0; h < blockHeight; h++)
    {
      for (int x = 0; x < blockWidth; x += 4)
      {
        __m64 res;
        if (level == 6) {
          res = _m_packuswb(_m_psrlwi(
            _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pSrc + x), z), ws),
              _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefB[0] + x), z), wb1),
                _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefF[0] + x), z), wf1),
                  _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefB[1] + x), z), wb2),
                    _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefF[1] + x), z), wf2),
                      _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefB[2] + x), z), wb3),
                        _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefF[2] + x), z), wf3),
                          _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefB[3] + x), z), wb4),
                            _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefF[3] + x), z), wf4),
                              _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefB[4] + x), z), wb5),
                                _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefF[4] + x), z), wf5),
                                  _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefB[5] + x), z), wb6),
                                    _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefF[5] + x), z), wf6),
                                      o))))))))))))), 8), z);
        }
        if (level == 5) {
          res = _m_packuswb(_m_psrlwi(
            _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pSrc + x), z), ws),
              _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefB[0] + x), z), wb1),
                _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefF[0] + x), z), wf1),
                  _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefB[1] + x), z), wb2),
                    _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefF[1] + x), z), wf2),
                      _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefB[2] + x), z), wb3),
                        _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefF[2] + x), z), wf3),
                          _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefB[3] + x), z), wb4),
                            _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefF[3] + x), z), wf4),
                              _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefB[4] + x), z), wb5),
                                _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefF[4] + x), z), wf5),
                                  o))))))))))), 8), z);
        }
        else if (level == 4) {
          res = _m_packuswb(_m_psrlwi(
            _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pSrc + x), z), ws),
              _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefB[0] + x), z), wb1),
                _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefF[0] + x), z), wf1),
                  _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefB[1] + x), z), wb2),
                    _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefF[1] + x), z), wf2),
                      _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefB[2] + x), z), wb3),
                        _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefF[2] + x), z), wf3),
                          _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefB[3] + x), z), wb4),
                            _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefF[3] + x), z), wf4),
                              o))))))))), 8), z);
        }
        else if (level == 3) {
          res = _m_packuswb(_m_psrlwi(
            _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pSrc + x), z), ws),
              _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefB[0] + x), z), wb1),
                _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefF[0] + x), z), wf1),
                  _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefB[1] + x), z), wb2),
                    _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefF[1] + x), z), wf2),
                      _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefB[2] + x), z), wb3),
                        _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefF[2] + x), z), wf3),
                          o))))))), 8), z);
          //			 pDst[x] = (pRefF[x]*WRefF + pSrc[x]*WSrc + pRefB[x]*WRefB + pRefF2[x]*WRefF2 + pRefB2[x]*WRefB2 + pRefF3[x]*WRefF3 + pRefB3[x]*WRefB3 + 128)>>8;
        }
        else if (level == 2) {
          res = _m_packuswb(_m_psrlwi(
            _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pSrc + x), z), ws),
              _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefB[0] + x), z), wb1),
                _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefF[0] + x), z), wf1),
                  _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefB[1] + x), z), wb2),
                    _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefF[1] + x), z), wf2),
                      o))))), 8), z);
          //			 pDst[x] = (pRefF[x]*WRefF + pSrc[x]*WSrc + pRefB[x]*WRefB + pRefF2[x]*WRefF2 + pRefB2[x]*WRefB2 + 128)>>8;
        }
        else if (level == 1) {
          res = _m_packuswb(_m_psrlwi(
            _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pSrc + x), z), ws),
              _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefB[0] + x), z), wb1),
                _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefF[0] + x), z), wf1),
                  o))), 8), z);
          //				 pDst[x] = (pRefF[x]*WRefF + pSrc[x]*WSrc + pRefB[x]*WRefB + 128)>>8;// weighted (by SAD) average
        }
        *(uint32_t *)(pDst + x) = _m_to_int(res);
      }
      pDst += nDstPitch;
      pSrc += nSrcPitch;

      pRefB[0] += BPitch[0];
      pRefF[0] += FPitch[0];
      if (level >= 2) {
        pRefB[1] += BPitch[1];
        pRefF[1] += FPitch[1];
        if (level >= 3) {
          pRefB[2] += BPitch[2];
          pRefF[2] += FPitch[2];
          if (level >= 4) {
            pRefB[3] += BPitch[3];
            pRefF[3] += FPitch[3];
            if (level >= 5) {
              pRefB[4] += BPitch[4];
              pRefF[4] += FPitch[4];
              if (level >= 6) {
                pRefB[5] += BPitch[5];
                pRefF[5] += FPitch[5];
              }
            }
          }
        }
      }
    }
  }
  _m_empty();
}
#endif


template<int blockWidth, int blockHeight, bool lsb_flag, int level >
void Degrain1to6_sse2(BYTE *pDst, BYTE *pDstLsb, bool _lsb_flag_not_used_template_rulez, int nDstPitch, const BYTE *pSrc, int nSrcPitch,
  const BYTE *pRefB[MAX_DEGRAIN], int BPitch[MAX_DEGRAIN], const BYTE *pRefF[MAX_DEGRAIN], int FPitch[MAX_DEGRAIN],
  int WSrc,
  int WRefB[MAX_DEGRAIN], int WRefF[MAX_DEGRAIN])
{
  __m128i z = _mm_setzero_si128();
  __m128i ws = _mm_set1_epi16(WSrc);
  __m128i wb1, wf1;
  __m128i wb2, wf2;
  __m128i wb3, wf3;
  __m128i wb4, wf4;
  __m128i wb5, wf5;
  __m128i wb6, wf6;
  wb1 = _mm_set1_epi16(WRefB[0]);
  wf1 = _mm_set1_epi16(WRefF[0]);
  if (level >= 2) {
    wb2 = _mm_set1_epi16(WRefB[1]);
    wf2 = _mm_set1_epi16(WRefF[1]);
    if (level >= 3) {
      wb3 = _mm_set1_epi16(WRefB[2]);
      wf3 = _mm_set1_epi16(WRefF[2]);
      if (level >= 4) {
        wb4 = _mm_set1_epi16(WRefB[3]);
        wf4 = _mm_set1_epi16(WRefF[3]);
        if (level >= 5) {
          wb5 = _mm_set1_epi16(WRefB[4]);
          wf5 = _mm_set1_epi16(WRefF[4]);
          if (level >= 6) {
            wb6 = _mm_set1_epi16(WRefB[5]);
            wf6 = _mm_set1_epi16(WRefF[5]);
          }
        }
      }
    }
  }

  if (lsb_flag)
  {
    __m128i m = _mm_set1_epi16(255);
    for (int h = 0; h < blockHeight; h++)
    {
      for (int x = 0; x < blockWidth; x += 8)
      {
        __m128i	val;
        if (level == 6) {
          val =
            _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pSrc + x)), z), ws),
              _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[0] + x)), z), wb1),
                _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[0] + x)), z), wf1),
                  _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[1] + x)), z), wb2),
                    _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[1] + x)), z), wf2),
                      _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[2] + x)), z), wb3),
                        _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[2] + x)), z), wf3),
                          _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[3] + x)), z), wb4),
                            _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[3] + x)), z), wf4),
                              _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[4] + x)), z), wb5),
                                _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[4] + x)), z), wf5),
                                  _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[5] + x)), z), wb6),
                                     _mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[5] + x)), z), wf6)))))))))))));
        }
        if (level == 5) {
          val =
            _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pSrc + x)), z), ws),
              _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[0] + x)), z), wb1),
                _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[0] + x)), z), wf1),
                  _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[1] + x)), z), wb2),
                    _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[1] + x)), z), wf2),
                      _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[2] + x)), z), wb3),
                        _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[2] + x)), z), wf3),
                          _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[3] + x)), z), wb4),
                            _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[3] + x)), z), wf4),
                              _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[4] + x)), z), wb5),
                                _mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[4] + x)), z), wf5)))))))))));
        }
        if (level == 4) {
          val =
            _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pSrc + x)), z), ws),
              _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[0] + x)), z), wb1),
                _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[0] + x)), z), wf1),
                  _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[1] + x)), z), wb2),
                    _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[1] + x)), z), wf2),
                      _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[2] + x)), z), wb3),
                        _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[2] + x)), z), wf3),
                          _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[3] + x)), z), wb4),
                            _mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[3] + x)), z), wf4)))))))));
        }
        if (level == 3) {
          val =
            _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pSrc + x)), z), ws),
              _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[0] + x)), z), wb1),
                _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[0] + x)), z), wf1),
                  _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[1] + x)), z), wb2),
                    _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[1] + x)), z), wf2),
                      _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[2] + x)), z), wb3),
                        _mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[2] + x)), z), wf3)))))));
        }
        else if (level == 2) {
          val =
            _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pSrc + x)), z), ws),
              _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[0] + x)), z), wb1),
                _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[0] + x)), z), wf1),
                  _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[1] + x)), z), wb2),
                    _mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[1] + x)), z), wf2)))));
        }
        else if (level == 1) {
          val =
            _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pSrc + x)), z), ws),
              _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[0] + x)), z), wb1),
                _mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[0] + x)), z), wf1)));
        }

        if (blockWidth >= 8) {
          _mm_storel_epi64((__m128i*)(pDst + x), _mm_packus_epi16(_mm_srli_epi16(val, 8), z));
          _mm_storel_epi64((__m128i*)(pDstLsb + x), _mm_packus_epi16(_mm_and_si128(val, m), z));
        } 
        else {
          *(uint32_t *)(pDst + x) = _mm_cvtsi128_si32(_mm_packus_epi16(_mm_srli_epi16(val, 8), z));
          *(uint32_t *)(pDstLsb + x) = _mm_cvtsi128_si32(_mm_packus_epi16(_mm_srli_epi16(val, 8), z));
        }
      }
      pDst += nDstPitch;
      pDstLsb += nDstPitch;
      pSrc += nSrcPitch;
      /*
      for (int i = 0; i < level; i++) {
      pRefB[i] += BPitch[i];
      }
      for (int i = 0; i < level; i++) {
      pRefF[i] += FPitch[i];
      }
      */
      pRefB[0] += BPitch[0];
      pRefF[0] += FPitch[0];
      if (level >= 2) {
        pRefB[1] += BPitch[1];
        pRefF[1] += FPitch[1];
        if (level >= 3) {
          pRefB[2] += BPitch[2];
          pRefF[2] += FPitch[2];
        }
        if (level >= 4) {
          pRefB[3] += BPitch[3];
          pRefF[3] += FPitch[3];
        }
        if (level >= 5) {
          pRefB[4] += BPitch[4];
          pRefF[4] += FPitch[4];
        }
        if (level >= 6) {
          pRefB[5] += BPitch[5];
          pRefF[5] += FPitch[5];
        }
      }

    }
  }

  else
  {
    __m128i o = _mm_set1_epi16(128);
    for (int h = 0; h < blockHeight; h++)
    {
      for (int x = 0; x < blockWidth; x += 8)
      {
        __m128i res;
        if (level == 6) {
          res = _mm_packus_epi16(_mm_srli_epi16(
            _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pSrc + x)), z), ws),
              _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[0] + x)), z), wb1),
                _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[0] + x)), z), wf1),
                  _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[1] + x)), z), wb2),
                    _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[1] + x)), z), wf2),
                      _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[2] + x)), z), wb3),
                        _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[2] + x)), z), wf3),
                          _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[3] + x)), z), wb4),
                            _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[3] + x)), z), wf4),
                              _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[4] + x)), z), wb5),
                                _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[4] + x)), z), wf5),
                                  _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[5] + x)), z), wb6),
                                    _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[5] + x)), z), wf6),
                                      o))))))))))))), 8), z);
        }
        if (level == 5) {
          res = _mm_packus_epi16(_mm_srli_epi16(
            _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pSrc + x)), z), ws),
              _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[0] + x)), z), wb1),
                _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[0] + x)), z), wf1),
                  _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[1] + x)), z), wb2),
                    _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[1] + x)), z), wf2),
                      _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[2] + x)), z), wb3),
                        _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[2] + x)), z), wf3),
                          _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[3] + x)), z), wb4),
                            _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[3] + x)), z), wf4),
                              _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[4] + x)), z), wb5),
                                _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[4] + x)), z), wf5),
                                  o))))))))))), 8), z);
        }
        else if (level == 4) {
          res = _mm_packus_epi16(_mm_srli_epi16(
            _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pSrc + x)), z), ws),
              _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[0] + x)), z), wb1),
                _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[0] + x)), z), wf1),
                  _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[1] + x)), z), wb2),
                    _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[1] + x)), z), wf2),
                      _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[2] + x)), z), wb3),
                        _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[2] + x)), z), wf3),
                          _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[3] + x)), z), wb4),
                            _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[3] + x)), z), wf4),
                              o))))))))), 8), z);
        }
        else if (level == 3) {
          res = _mm_packus_epi16(_mm_srli_epi16(
            _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pSrc + x)), z), ws),
              _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[0] + x)), z), wb1),
                _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[0] + x)), z), wf1),
                  _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[1] + x)), z), wb2),
                    _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[1] + x)), z), wf2),
                      _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[2] + x)), z), wb3),
                        _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[2] + x)), z), wf3),
                          o))))))), 8), z);
          //			 pDst[x] = (pRefF[x]*WRefF + pSrc[x]*WSrc + pRefB[x]*WRefB + pRefF2[x]*WRefF2 + pRefB2[x]*WRefB2 + pRefF3[x]*WRefF3 + pRefB3[x]*WRefB3 + 128)>>8;
        }
        else if (level == 2) {
          res = _mm_packus_epi16(_mm_srli_epi16(
            _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pSrc + x)), z), ws),
              _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[0] + x)), z), wb1),
                _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[0] + x)), z), wf1),
                  _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[1] + x)), z), wb2),
                    _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[1] + x)), z), wf2),
                      o))))), 8), z);
          //			 pDst[x] = (pRefF[x]*WRefF + pSrc[x]*WSrc + pRefB[x]*WRefB + pRefF2[x]*WRefF2 + pRefB2[x]*WRefB2 + 128)>>8;
        }
        else if (level == 1) {
          res = _mm_packus_epi16(_mm_srli_epi16(
            _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pSrc + x)), z), ws),
              _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[0] + x)), z), wb1),
                _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[0] + x)), z), wf1),
                  o))), 8), z);
          //				 pDst[x] = (pRefF[x]*WRefF + pSrc[x]*WSrc + pRefB[x]*WRefB + 128)>>8;// weighted (by SAD) average
        }
        if(blockWidth >= 8)
          _mm_storel_epi64((__m128i*)(pDst + x), res);
        else
          *(uint32_t *)(pDst + x) = _mm_cvtsi128_si32(res);
      }
      pDst += nDstPitch;
      pSrc += nSrcPitch;
      /*
      for (int i = 0; i < level; i++) {
      pRefB[i] += BPitch[i];
      pRefF[i] += FPitch[i];
      }
      */
      pRefB[0] += BPitch[0];
      pRefF[0] += FPitch[0];
      if (level >= 2) {
        pRefB[1] += BPitch[1];
        pRefF[1] += FPitch[1];
        if (level >= 3) {
          pRefB[2] += BPitch[2];
          pRefF[2] += FPitch[2];
          if (level >= 4) {
            pRefB[3] += BPitch[3];
            pRefF[3] += FPitch[3];
            if (level >= 5) {
              pRefB[4] += BPitch[4];
              pRefF[4] += FPitch[4];
              if (level >= 6) {
                pRefB[5] += BPitch[5];
                pRefF[5] += FPitch[5];
              }
            }
          }
        }
      }
    }
  }
}

// Not really related to overlap, but common to MDegrainX functions
// PF 160928: this is bottleneck. Could be optimized with precalc thSAD*thSAD
MV_FORCEINLINE int DegrainWeight(int thSAD, int blockSAD, int bits_per_pixels)
{
  // Returning directly prevents a divide by 0 if thSAD == blockSAD == 0.
  if (thSAD <= blockSAD)
  {
    return 0;
  }
  if(bits_per_pixels <= 8) {
    const int thSAD2    = thSAD    * thSAD;
    const int blockSAD2 = blockSAD * blockSAD;
    const int num = thSAD2 - blockSAD2;
    const int den = thSAD2 + blockSAD2;
    // res = num*256/den
    const int      res = int((num < (1<<23))
      ? (num<<8) /  den      // small numerator
      :  num     / (den>>8)); // very large numerator, prevent overflow
    return (res);
  } else {
    // int overflows with 8+ bits_per_pixel scaled power of 2
    /* float is faster
    const int64_t sq_thSAD = int64_t(thSAD) * thSAD;
    const int64_t sq_blockSAD = int64_t(blockSAD) * blockSAD;
    return (int)((256*(sq_thSAD - sq_blockSAD)) / (sq_thSAD + sq_blockSAD));
    */
    const float sq_thSAD = float(thSAD) * float(thSAD); // std::powf(float(thSAD), 2.0f); 
                                                        // smart compiler makes x*x, VS2015 calls __libm_sse2_pow_precise, way too slow
    const float sq_blockSAD = float(blockSAD) * float(blockSAD); // std::powf(float(blockSAD), 2.0f);
    return (int)(256.0f*(sq_thSAD - sq_blockSAD) / (sq_thSAD + sq_blockSAD));
  }
}

template<int level>
MV_FORCEINLINE void norm_weights(int &WSrc, int(&WRefB)[MAX_DEGRAIN], int(&RefF)[MAX_DEGRAIN]);


#ifdef LEVEL_IS_TEMPLATE
/* for test only, 1 and 2 templated
template class MVDegrainX<1>
(PClip _child, PClip _super, PClip _mvbw, PClip _mvfw, PClip _mvbw2, PClip _mvfw2, PClip _mvbw3, PClip _mvfw3, PClip _mvbw4, PClip _mvfw4, PClip _mvbw5, PClip _mvfw5,
sad_t _thSAD, sad_t _thSADC, int _YUVplanes, sad_t _nLimit, sad_t _nLimitC,
sad_t _nSCD1, int _nSCD2, bool _isse2, bool _planar, bool _lsb_flag,
bool _mt_flag, 
#ifndef LEVEL_IS_TEMPLATE
int _level, 
#endif
IScriptEnvironment* env);

template class MVDegrainX<2>
(PClip _child, PClip _super, PClip _mvbw, PClip _mvfw, PClip _mvbw2, PClip _mvfw2, PClip _mvbw3, PClip _mvfw3, PClip _mvbw4, PClip _mvfw4, PClip _mvbw5, PClip _mvfw5,
sad_t _thSAD, sad_t _thSADC, int _YUVplanes, sad_t _nLimit, sad_t _nLimitC,
sad_t _nSCD1, int _nSCD2, bool _isse2, bool _planar, bool _lsb_flag,
bool _mt_flag, 
#ifndef LEVEL_IS_TEMPLATE
int _level, 
#endif
IScriptEnvironment* env);
#endif

*/
#endif

#endif

