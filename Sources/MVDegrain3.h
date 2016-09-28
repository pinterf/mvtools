#ifndef __MV_DEGRAIN3__
#define __MV_DEGRAIN3__

#include "CopyCode.h"
#include "MVClip.h"
#include "MVFilter.h"
#include "overlap.h"
#include "yuy2planes.h"
#include <stdint.h>

class MVGroupOfFrames;
class MVPlane;

#define MAX_DEGRAIN 5
/*! \brief Filter that denoise the picture
 */
class MVDegrainX
  : public GenericVideoFilter
  , public MVFilter
{
private:
  /*
  typedef void (Denoise1Function)(
    BYTE *pDst, BYTE *pDstLsb, bool lsb_flag, int nDstPitch, const BYTE *pSrc, int nSrcPitch,
    const BYTE *pRefB, int BPitch, const BYTE *pRefF, int FPitch,
    int WSrc, int WRefB, int WRefF
    );
  typedef void (Denoise2Function)(
    BYTE *pDst, BYTE *pDstLsb, bool lsb_flag, int nDstPitch, const BYTE *pSrc, int nSrcPitch,
    const BYTE *pRefB, int BPitch, const BYTE *pRefF, int FPitch,
    const BYTE *pRefB2, int B2Pitch, const BYTE *pRefF2, int F2Pitch,
    int WSrc, int WRefB, int WRefF, int WRefB2, int WRefF2
    );
  typedef void (Denoise3Function)(
    BYTE *pDst, BYTE *pDstLsb, bool lsb_flag, int nDstPitch, const BYTE *pSrc, int nSrcPitch,
    const BYTE *pRefB, int BPitch, const BYTE *pRefF, int FPitch,
    const BYTE *pRefB2, int B2Pitch, const BYTE *pRefF2, int F2Pitch,
    const BYTE *pRefB3, int B3Pitch, const BYTE *pRefF3, int F3Pitch,
    int WSrc, int WRefB, int WRefF, int WRefB2, int WRefF2, int WRefB3, int WRefF3
    );
    */
  typedef void (norm_weights_Function_t)(int &WSrc, int(&WRefB)[MAX_DEGRAIN], int(&WRefF)[MAX_DEGRAIN]);


  typedef void (Denoise1to5Function)(
    BYTE *pDst, BYTE *pDstLsb, bool lsb_flag, int nDstPitch, const BYTE *pSrc, int nSrcPitch,
    const BYTE *pRefB[MAX_DEGRAIN], int BPitch[MAX_DEGRAIN], const BYTE *pRefF[MAX_DEGRAIN], int FPitch[MAX_DEGRAIN],
    int WSrc,
    int WRefB[MAX_DEGRAIN], int WRefF[MAX_DEGRAIN]
    );


  MVClip *mvClipB[MAX_DEGRAIN];
  MVClip *mvClipF[MAX_DEGRAIN];
  /*
  MVClip mvClipB2;
  MVClip mvClipF2;
  MVClip mvClipB3;
  MVClip mvClipF3;
  */
  int thSAD;
  int thSADC;
  int YUVplanes;
  int nLimit;
  int nLimitC;
  PClip super;
  bool isse2;
  bool planar;
  bool lsb_flag;
  int height_lsb_mul;
  //int pixelsize; // in MVFilter
  //int bits_per_pixel; // in MVFilter
  int pixelsize_super; // PF not param, from create
  int bits_per_pixel_super; // PF not param, from create

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
  //Denoise3Function *DEGRAINLUMA;
  //Denoise3Function *DEGRAINCHROMA;
  Denoise1to5Function *DEGRAINLUMA;
  Denoise1to5Function *DEGRAINCHROMA;

  norm_weights_Function_t *NORMWEIGHTS;

  MVGroupOfFrames *pRefBGOF[MAX_DEGRAIN], *pRefFGOF[MAX_DEGRAIN];
  //MVGroupOfFrames *pRefB2GOF, *pRefF2GOF;
  //MVGroupOfFrames *pRefB3GOF, *pRefF3GOF;

  unsigned char *tmpBlock;
  unsigned char *tmpBlockLsb;	// Not allocated, it's just a reference to a part of the tmpBlock area (or 0 if no LSB)
  unsigned short * DstShort;
  int dstShortPitch;
  int * DstInt;
  int dstIntPitch;

  int level;

public:
  MVDegrainX(PClip _child, PClip _super, PClip _mvbw, PClip _mvfw, PClip _mvbw2, PClip _mvfw2, PClip _mvbw3, PClip _mvfw3, PClip _mvbw4, PClip _mvfw4, PClip _mvbw5, PClip _mvfw5,
    int _thSAD, int _thSADC, int _YUVplanes, int _nLimit, int _nLimitC,
    int nSCD1, int nSCD2, bool _isse2, bool _planar, bool _lsb_flag,
    bool mt_flag, int _level, IScriptEnvironment* env);
  ~MVDegrainX();
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);

  int __stdcall SetCacheHints(int cachehints, int frame_range) override {
    return cachehints == CACHE_GET_MTMODE ? MT_MULTI_INSTANCE : 0;
  }

private:
  inline void process_chroma(int plane_mask, BYTE *pDst, BYTE *pDstCur, int nDstPitch, const BYTE *pSrc, const BYTE *pSrcCur, int nSrcPitch,
    bool isUsableB[MAX_DEGRAIN], bool isUsableF[MAX_DEGRAIN], MVPlane *pPlanesB[MAX_DEGRAIN], MVPlane *pPlanesF[MAX_DEGRAIN],
    int lsb_offset_uv, int nWidth_B, int nHeight_B);
  // inline void	process_chroma(int plane_mask, BYTE *pDst, BYTE *pDstCur, int nDstPitch, const BYTE *pSrc, const BYTE *pSrcCur, int nSrcPitch, bool isUsableB, bool isUsableF, bool isUsableB2, bool isUsableF2, bool isUsableB3, bool isUsableF3, MVPlane *pPlanesB, MVPlane *pPlanesF, MVPlane *pPlanesB2, MVPlane *pPlanesF2, MVPlane *pPlanesB3, MVPlane *pPlanesF3, int lsb_offset_uv, int nWidth_B, int nHeight_B);
  inline void	use_block_y(const BYTE * &p, int &np, int &WRef, bool isUsable, const MVClip &mvclip, int i, const MVPlane *pPlane, const BYTE *pSrcCur, int xx, int nSrcPitch);
  inline void	use_block_uv(const BYTE * &p, int &np, int &WRef, bool isUsable, const MVClip &mvclip, int i, const MVPlane *pPlane, const BYTE *pSrcCur, int xx, int nSrcPitch);
  template<int level>
  static inline void norm_weights(int &WSrc, int(&WRefB)[MAX_DEGRAIN], int(&RefF)[MAX_DEGRAIN]);
  // static inline void	norm_weights(int &WSrc, int &WRefB, int &WRefF, int &WRefB2, int &WRefF2, int &WRefB3, int &WRefF3);
    //Denoise1Function* get_denoise1_function(int BlockX, int BlockY, int pixelsize, arch_t arch);
    //Denoise2Function* get_denoise2_function(int BlockX, int BlockY, int pixelsize, arch_t arch);
    //Denoise3Function* get_denoise3_function(int BlockX, int BlockY, int pixelsize, arch_t arch);
  Denoise1to5Function* get_denoise123_function(int BlockX, int BlockY, int pixelsize, bool lsb_flag, int level, arch_t arch);
};

template<typename pixel_t, int blockWidth, int blockHeight, bool lsb_flag, int level >
void Degrain1to5_C(uint8_t *pDst, BYTE *pDstLsb, bool lsb_flag, int nDstPitch, const uint8_t *pSrc, int nSrcPitch,
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
          }
        }
      }
    }
  }
}

#if 0
template<int blockWidth, int blockHeight>
void Degrain3_C(BYTE *pDst, BYTE *pDstLsb, bool lsb_flag, int nDstPitch, const BYTE *pSrc, int nSrcPitch,
  const BYTE *pRefB, int BPitch, const BYTE *pRefF, int FPitch,
  const BYTE *pRefB2, int B2Pitch, const BYTE *pRefF2, int F2Pitch,
  const BYTE *pRefB3, int B3Pitch, const BYTE *pRefF3, int F3Pitch,
  int WSrc, int WRefB, int WRefF, int WRefB2, int WRefF2, int WRefB3, int WRefF3)
{
  if (lsb_flag)
  {
    for (int h = 0; h < blockHeight; h++)
    {
      for (int x = 0; x < blockWidth; x++)
      {
        const int		val = pRefF[x] * WRefF + pSrc[x] * WSrc + pRefB[x] * WRefB + pRefF2[x] * WRefF2 + pRefB2[x] * WRefB2 + pRefF3[x] * WRefF3 + pRefB3[x] * WRefB3;
        pDst[x] = val >> 8;
        pDstLsb[x] = val & 255;
      }
      pDst += nDstPitch;
      pDstLsb += nDstPitch;
      pSrc += nSrcPitch;
      pRefB += BPitch;
      pRefF += FPitch;
      pRefB2 += B2Pitch;
      pRefF2 += F2Pitch;
      pRefB3 += B3Pitch;
      pRefF3 += F3Pitch;
    }
  }

  else
  {
    for (int h = 0; h < blockHeight; h++)
    {
      for (int x = 0; x < blockWidth; x++)
      {
        pDst[x] = (pRefF[x] * WRefF + pSrc[x] * WSrc + pRefB[x] * WRefB + pRefF2[x] * WRefF2 + pRefB2[x] * WRefB2 + pRefF3[x] * WRefF3 + pRefB3[x] * WRefB3 + 128) >> 8;
      }
      pDst += nDstPitch;
      pSrc += nSrcPitch;
      pRefB += BPitch;
      pRefF += FPitch;
      pRefB2 += B2Pitch;
      pRefF2 += F2Pitch;
      pRefB3 += B3Pitch;
      pRefF3 += F3Pitch;
    }
  }
}
#endif

#if 0
// PF: no mmx
#ifndef _M_X64
#include <mmintrin.h>
template<int blockWidth, int blockHeight>
void Degrain3_mmx(BYTE *pDst, BYTE *pDstLsb, bool lsb_flag, int nDstPitch, const BYTE *pSrc, int nSrcPitch,
  const BYTE *pRefB, int BPitch, const BYTE *pRefF, int FPitch,
  const BYTE *pRefB2, int B2Pitch, const BYTE *pRefF2, int F2Pitch,
  const BYTE *pRefB3, int B3Pitch, const BYTE *pRefF3, int F3Pitch,
  int WSrc, int WRefB, int WRefF, int WRefB2, int WRefF2, int WRefB3, int WRefF3)
{
  __m64 z = _mm_setzero_si64();
  __m64 ws = _mm_set1_pi16(WSrc);
  __m64 wb1 = _mm_set1_pi16(WRefB);
  __m64 wf1 = _mm_set1_pi16(WRefF);
  __m64 wb2 = _mm_set1_pi16(WRefB2);
  __m64 wf2 = _mm_set1_pi16(WRefF2);
  __m64 wb3 = _mm_set1_pi16(WRefB3);
  __m64 wf3 = _mm_set1_pi16(WRefF3);

  if (lsb_flag)
  {
    __m64 m = _mm_set1_pi16(255);
    for (int h = 0; h < blockHeight; h++)
    {
      for (int x = 0; x < blockWidth; x += 4)
      {
        const __m64		val =
          _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pSrc + x), z), ws),
            _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefB + x), z), wb1),
              _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefF + x), z), wf1),
                _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefB2 + x), z), wb2),
                  _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefF2 + x), z), wf2),
                    _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefB3 + x), z), wb3),
                      _m_pmullw(_m_punpcklbw(*(__m64*)(pRefF3 + x), z), wf3)))))));
        *(int*)(pDst + x) = _m_to_int(_m_packuswb(_m_psrlwi(val, 8), z));
        *(int*)(pDstLsb + x) = _m_to_int(_m_packuswb(_mm_and_si64(val, m), z));
      }
      pDst += nDstPitch;
      pDstLsb += nDstPitch;
      pSrc += nSrcPitch;
      pRefB += BPitch;
      pRefF += FPitch;
      pRefB2 += B2Pitch;
      pRefF2 += F2Pitch;
      pRefB3 += B3Pitch;
      pRefF3 += F3Pitch;
    }
  }

  else
  {
    __m64 o = _mm_set1_pi16(128);
    for (int h = 0; h < blockHeight; h++)
    {
      for (int x = 0; x < blockWidth; x += 4)
      {
        *(int*)(pDst + x) = _m_to_int(_m_packuswb(_m_psrlwi(
          _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pSrc + x), z), ws),
            _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefB + x), z), wb1),
              _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefF + x), z), wf1),
                _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefB2 + x), z), wb2),
                  _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefF2 + x), z), wf2),
                    _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefB3 + x), z), wb3),
                      _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefF3 + x), z), wf3),
                        o))))))), 8), z));
        //			 pDst[x] = (pRefF[x]*WRefF + pSrc[x]*WSrc + pRefB[x]*WRefB + pRefF2[x]*WRefF2 + pRefB2[x]*WRefB2 + pRefF3[x]*WRefF3 + pRefB3[x]*WRefB3 + 128)>>8;
      }
      pDst += nDstPitch;
      pSrc += nSrcPitch;
      pRefB += BPitch;
      pRefF += FPitch;
      pRefB2 += B2Pitch;
      pRefF2 += F2Pitch;
      pRefB3 += B3Pitch;
      pRefF3 += F3Pitch;
    }
  }

  _m_empty();
}
#endif
#endif

#if 0
#include <emmintrin.h>
template<int blockWidth, int blockHeight>
void Degrain3_sse2(BYTE *pDst, BYTE *pDstLsb, bool lsb_flag, int nDstPitch, const BYTE *pSrc, int nSrcPitch,
  const BYTE *pRefB, int BPitch, const BYTE *pRefF, int FPitch,
  const BYTE *pRefB2, int B2Pitch, const BYTE *pRefF2, int F2Pitch,
  const BYTE *pRefB3, int B3Pitch, const BYTE *pRefF3, int F3Pitch,
  int WSrc, int WRefB, int WRefF, int WRefB2, int WRefF2, int WRefB3, int WRefF3)
{
  __m128i z = _mm_setzero_si128();
  __m128i ws = _mm_set1_epi16(WSrc);
  __m128i wb1 = _mm_set1_epi16(WRefB);
  __m128i wf1 = _mm_set1_epi16(WRefF);
  __m128i wb2 = _mm_set1_epi16(WRefB2);
  __m128i wf2 = _mm_set1_epi16(WRefF2);
  __m128i wb3 = _mm_set1_epi16(WRefB3);
  __m128i wf3 = _mm_set1_epi16(WRefF3);

  if (lsb_flag)
  {
    __m128i m = _mm_set1_epi16(255);
    for (int h = 0; h < blockHeight; h++)
    {
      for (int x = 0; x < blockWidth; x += 8)
      {
        const __m128i	val =
          _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pSrc + x)), z), ws),
            _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB + x)), z), wb1),
              _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF + x)), z), wf1),
                _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB2 + x)), z), wb2),
                  _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF2 + x)), z), wf2),
                    _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB3 + x)), z), wb3),
                      _mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF3 + x)), z), wf3)))))));
        _mm_storel_epi64((__m128i*)(pDst + x), _mm_packus_epi16(_mm_srli_epi16(val, 8), z));
        _mm_storel_epi64((__m128i*)(pDstLsb + x), _mm_packus_epi16(_mm_and_si128(val, m), z));
      }
      pDst += nDstPitch;
      pDstLsb += nDstPitch;
      pSrc += nSrcPitch;
      pRefB += BPitch;
      pRefF += FPitch;
      pRefB2 += B2Pitch;
      pRefF2 += F2Pitch;
      pRefB3 += B3Pitch;
      pRefF3 += F3Pitch;
    }
  }

  else
  {
    __m128i o = _mm_set1_epi16(128);
    for (int h = 0; h < blockHeight; h++)
    {
      for (int x = 0; x < blockWidth; x += 8)
      {
        _mm_storel_epi64((__m128i*)(pDst + x), _mm_packus_epi16(_mm_srli_epi16(
          _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pSrc + x)), z), ws),
            _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB + x)), z), wb1),
              _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF + x)), z), wf1),
                _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB2 + x)), z), wb2),
                  _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF2 + x)), z), wf2),
                    _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB3 + x)), z), wb3),
                      _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF3 + x)), z), wf3),
                        o))))))), 8), z));
        //			 pDst[x] = (pRefF[x]*WRefF + pSrc[x]*WSrc + pRefB[x]*WRefB + pRefF2[x]*WRefF2 + pRefB2[x]*WRefB2 + pRefF3[x]*WRefF3 + pRefB3[x]*WRefB3 + 128)>>8;
      }
      pDst += nDstPitch;
      pSrc += nSrcPitch;
      pRefB += BPitch;
      pRefF += FPitch;
      pRefB2 += B2Pitch;
      pRefF2 += F2Pitch;
      pRefB3 += B3Pitch;
      pRefF3 += F3Pitch;
    }
  }
}
#endif

template<int blockWidth, int blockHeight, bool lsb_flag, int level >
void Degrain1to5_sse2(BYTE *pDst, BYTE *pDstLsb, bool lsb_flag, int nDstPitch, const BYTE *pSrc, int nSrcPitch,
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

        _mm_storel_epi64((__m128i*)(pDst + x), _mm_packus_epi16(_mm_srli_epi16(val, 8), z));
        _mm_storel_epi64((__m128i*)(pDstLsb + x), _mm_packus_epi16(_mm_and_si128(val, m), z));
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
        if (level == 5) {
          _mm_storel_epi64((__m128i*)(pDst + x), _mm_packus_epi16(_mm_srli_epi16(
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
                                  o))))))))))), 8), z));
        }
        else if (level == 4) {
          _mm_storel_epi64((__m128i*)(pDst + x), _mm_packus_epi16(_mm_srli_epi16(
            _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pSrc + x)), z), ws),
              _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[0] + x)), z), wb1),
                _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[0] + x)), z), wf1),
                  _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[1] + x)), z), wb2),
                    _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[1] + x)), z), wf2),
                      _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[2] + x)), z), wb3),
                        _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[2] + x)), z), wf3),
                          _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[3] + x)), z), wb4),
                            _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[3] + x)), z), wf4),
                              o))))))))), 8), z));
        }
        else if (level == 3) {
          _mm_storel_epi64((__m128i*)(pDst + x), _mm_packus_epi16(_mm_srli_epi16(
            _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pSrc + x)), z), ws),
              _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[0] + x)), z), wb1),
                _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[0] + x)), z), wf1),
                  _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[1] + x)), z), wb2),
                    _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[1] + x)), z), wf2),
                      _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[2] + x)), z), wb3),
                        _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[2] + x)), z), wf3),
                          o))))))), 8), z));
          //			 pDst[x] = (pRefF[x]*WRefF + pSrc[x]*WSrc + pRefB[x]*WRefB + pRefF2[x]*WRefF2 + pRefB2[x]*WRefB2 + pRefF3[x]*WRefF3 + pRefB3[x]*WRefB3 + 128)>>8;
        }
        else if (level == 2) {
          _mm_storel_epi64((__m128i*)(pDst + x), _mm_packus_epi16(_mm_srli_epi16(
            _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pSrc + x)), z), ws),
              _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[0] + x)), z), wb1),
                _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[0] + x)), z), wf1),
                  _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[1] + x)), z), wb2),
                    _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[1] + x)), z), wf2),
                      o))))), 8), z));
          //			 pDst[x] = (pRefF[x]*WRefF + pSrc[x]*WSrc + pRefB[x]*WRefB + pRefF2[x]*WRefF2 + pRefB2[x]*WRefB2 + 128)>>8;
        }
        else if (level == 1) {
          _mm_storel_epi64((__m128i*)(pDst + x), _mm_packus_epi16(_mm_srli_epi16(
            _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pSrc + x)), z), ws),
              _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[0] + x)), z), wb1),
                _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[0] + x)), z), wf1),
                  o))), 8), z));
          //				 pDst[x] = (pRefF[x]*WRefF + pSrc[x]*WSrc + pRefB[x]*WRefB + 128)>>8;// weighted (by SAD) average
        }
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
            }
          }
        }
      }
    }
  }
}
#endif
