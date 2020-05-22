#ifndef __MV_DEGRAIN3__
#define __MV_DEGRAIN3__

#include "CopyCode.h"
#include "MVClip.h"
#include "MVFilter.h"
#include "overlap.h"
#include "yuy2planes.h"
#include <stdint.h>
#include "def.h"
#include "intrin.h"

class MVGroupOfFrames;
class MVPlane;
class MVFilter;

#define MAX_DEGRAIN 6
constexpr int DEGRAIN_WEIGHT_BITS = 8;

typedef void (Denoise1to6Function)(
  BYTE *pDst, BYTE *pDstLsb, int WidthHeightForC, int nDstPitch, const BYTE *pSrc, int nSrcPitch,
  const BYTE *pRefB[MAX_DEGRAIN], int BPitch[MAX_DEGRAIN], const BYTE *pRefF[MAX_DEGRAIN], int FPitch[MAX_DEGRAIN],
  int WSrc,
  int WRefB[MAX_DEGRAIN], int WRefF[MAX_DEGRAIN]
  );

/*! \brief Filter that denoise the picture
*/
class MVDegrainX
  : public GenericVideoFilter
  , public MVFilter
{
private:
  typedef void (norm_weights_Function_t)(int &WSrc, int(&WRefB)[MAX_DEGRAIN], int(&WRefF)[MAX_DEGRAIN]);


  MVClip *mvClipB[MAX_DEGRAIN];
  MVClip *mvClipF[MAX_DEGRAIN];
  sad_t thSAD;
  double thSAD_sq;
  sad_t thSADC;
  double thSADC_sq;
  int YUVplanes;
  float nLimit;
  float nLimitC;
  PClip super;
  //bool isse_flag;
  int cpuFlags;
  bool planar;
  bool lsb_flag;
  bool out16_flag; // native 16 bit out instead of fake lsb
  int height_lsb_or_out16_mul;
  //int pixelsize, bits_per_pixel; // in MVFilter
  //int xRatioUV, yRatioUV; // in MVFilter
  int pixelsize_super; // PF not param, from create
  int bits_per_pixel_super; // PF not param, from create
  int pixelsize_super_shift;
  int xRatioUV_super, nLogxRatioUV_super; // 2.7.39-
  int yRatioUV_super, nLogyRatioUV_super;
  
  // 2.7.26
  int pixelsize_output;
  int bits_per_pixel_output;
  int pixelsize_output_shift;

  int nSuperModeYUV;

  YUY2Planes * DstPlanes;
  YUY2Planes * SrcPlanes;

  OverlapWindows *OverWins;
  OverlapWindows *OverWinsUV;

  OverlapsFunction *OVERSLUMA;
  OverlapsFunction *OVERSCHROMA;
  OverlapsFunction *OVERSLUMA16;
  OverlapsFunction *OVERSCHROMA16;
  OverlapsFunction *OVERSLUMA32;
  OverlapsFunction *OVERSCHROMA32;
  OverlapsLsbFunction *OVERSLUMALSB;
  OverlapsLsbFunction *OVERSCHROMALSB;
  Denoise1to6Function *DEGRAINLUMA;
  Denoise1to6Function *DEGRAINCHROMA;

  norm_weights_Function_t *NORMWEIGHTS;

  LimitFunction_t *LimitFunction;

  MVGroupOfFrames *pRefBGOF[MAX_DEGRAIN], *pRefFGOF[MAX_DEGRAIN];

  unsigned char *tmpBlock;
  unsigned char *tmpBlockLsb;	// Not allocated, it's just a reference to a part of the tmpBlock area (or 0 if no LSB)
  uint16_t * DstShort;
  int dstShortPitch;
  int * DstInt;
  int dstIntPitch;

  const int level;
  int framenumber;

public:
  MVDegrainX(PClip _child, PClip _super, 
    PClip _mvbw, PClip _mvfw, PClip _mvbw2, PClip _mvfw2, PClip _mvbw3, PClip _mvfw3, PClip _mvbw4, PClip _mvfw4, PClip _mvbw5, PClip _mvfw5, PClip _mvbw6, PClip _mvfw6,
    sad_t _thSAD, sad_t _thSADC, int _YUVplanes, float _nLimit, float _nLimitC,
    sad_t _nSCD1, int _nSCD2, bool _isse2, bool _planar, bool _lsb_flag,
    bool _mt_flag, bool _out16_flag,
    int _level, 
    IScriptEnvironment* env_ptr);
  ~MVDegrainX();
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env) override;

  int __stdcall SetCacheHints(int cachehints, int frame_range) override {
    return cachehints == CACHE_GET_MTMODE ? MT_MULTI_INSTANCE : 0;
  }

private:
  inline void process_chroma(int plane_mask, BYTE *pDst, BYTE *pDstCur, int nDstPitch, const BYTE *pSrc, const BYTE *pSrcCur, int nSrcPitch,
    bool isUsableB[MAX_DEGRAIN], bool isUsableF[MAX_DEGRAIN], MVPlane *pPlanesB[MAX_DEGRAIN], MVPlane *pPlanesF[MAX_DEGRAIN],
    int lsb_offset_uv, int nWidth_B, int nHeight_B);
  // MV_FORCEINLINE void process_chroma(int plane_mask, BYTE *pDst, BYTE *pDstCur, int nDstPitch, const BYTE *pSrc, const BYTE *pSrcCur, int nSrcPitch, bool isUsableB, bool isUsableF, bool isUsableB2, bool isUsableF2, bool isUsableB3, bool isUsableF3, MVPlane *pPlanesB, MVPlane *pPlanesF, MVPlane *pPlanesB2, MVPlane *pPlanesF2, MVPlane *pPlanesB3, MVPlane *pPlanesF3, int lsb_offset_uv, int nWidth_B, int nHeight_B);
  MV_FORCEINLINE void use_block_y(const BYTE * &p, int &np, int &WRef, bool isUsable, const MVClip &mvclip, int i, const MVPlane *pPlane, const BYTE *pSrcCur, int xx, int nSrcPitch);
  MV_FORCEINLINE void use_block_uv(const BYTE * &p, int &np, int &WRef, bool isUsable, const MVClip &mvclip, int i, const MVPlane *pPlane, const BYTE *pSrcCur, int xx, int nSrcPitch);
  Denoise1to6Function *get_denoise123_function(int BlockX, int BlockY, int _bits_per_pixel, bool _lsb_flag, bool _out16_flag, int _level, arch_t _arch);
};

#pragma warning( push )
#pragma warning( disable : 4101)
  // out16_type: 
  //   0: native 8 or 16
  //   1: 8bit in, lsb (stacked)
  //   2: 8bit in, native16 out
template<typename pixel_t, int out16_type, int level >
void Degrain1to6_C(uint8_t *pDst, BYTE *pDstLsb, int WidthHeightForC, int nDstPitch, const uint8_t *pSrc, int nSrcPitch,
  const uint8_t *pRefB[MAX_DEGRAIN], int BPitch[MAX_DEGRAIN], const uint8_t *pRefF[MAX_DEGRAIN], int FPitch[MAX_DEGRAIN],
  int WSrc,
  int WRefB[MAX_DEGRAIN], int WRefF[MAX_DEGRAIN])
{
  // avoid unnecessary templates. C implementation is here for the sake of correctness and for very small block sizes
  // For all other cases where speed counts, at least SSE2 is used
  // Use only one parameter
  const int blockWidth = (WidthHeightForC >> 16);
  const int blockHeight = (WidthHeightForC & 0xFFFF);

  constexpr bool lsb_flag = (out16_type == 1);
  constexpr bool out16 = (out16_type == 2);

  constexpr int SHIFTBACK = (lsb_flag || out16) ? (DEGRAIN_WEIGHT_BITS - 8) : DEGRAIN_WEIGHT_BITS;
  constexpr int rounder = (1 << SHIFTBACK) / 2; // zero when 8 bit in 16 bit out
  // note: DEGRAIN_WEIGHT_BITS is fixed 8 bits, so no rounding occurs on 8 bit in 16 bit out

  for (int h = 0; h < blockHeight; h++)
  {
    for (int x = 0; x < blockWidth; x++)
    {
      int val = reinterpret_cast<const pixel_t*>(pSrc)[x] * WSrc;

      if constexpr (level >= 1)
        val += reinterpret_cast<const pixel_t*>(pRefF[0])[x] * WRefF[0] + reinterpret_cast<const pixel_t*>(pRefB[0])[x] * WRefB[0];
      if constexpr (level >= 2)
        val += reinterpret_cast<const pixel_t*>(pRefF[1])[x] * WRefF[1] + reinterpret_cast<const pixel_t*>(pRefB[1])[x] * WRefB[1];
      if constexpr (level >= 3)
        val += reinterpret_cast<const pixel_t*>(pRefF[2])[x] * WRefF[2] + reinterpret_cast<const pixel_t*>(pRefB[2])[x] * WRefB[2];
      if constexpr (level >= 4)
        val += reinterpret_cast<const pixel_t*>(pRefF[3])[x] * WRefF[3] + reinterpret_cast<const pixel_t*>(pRefB[3])[x] * WRefB[3];
      if constexpr (level >= 5)
        val += reinterpret_cast<const pixel_t*>(pRefF[4])[x] * WRefF[4] + reinterpret_cast<const pixel_t*>(pRefB[4])[x] * WRefB[4];
      if constexpr (level >= 6)
        val += reinterpret_cast<const pixel_t*>(pRefF[5])[x] * WRefF[5] + reinterpret_cast<const pixel_t*>(pRefB[5])[x] * WRefB[5];

      val = (val + rounder) >> SHIFTBACK;
      if constexpr (lsb_flag) {
        reinterpret_cast<uint8_t*>(pDst)[x] = val >> 8;
        pDstLsb[x] = val & 255;
      }
      else if constexpr (out16) {
        reinterpret_cast<uint16_t*>(pDst)[x] = val;
      }
      else {
        reinterpret_cast<pixel_t*>(pDst)[x] = val;
      }
    }
    pDst += nDstPitch;
    if constexpr(lsb_flag)
      pDstLsb += nDstPitch;
    pSrc += nSrcPitch;
    pRefB[0] += BPitch[0];
    pRefF[0] += FPitch[0];
    if constexpr(level >= 2) {
      pRefB[1] += BPitch[1];
      pRefF[1] += FPitch[1];
      if constexpr(level >= 3) {
        pRefB[2] += BPitch[2];
        pRefF[2] += FPitch[2];
        if constexpr(level >= 4) {
          pRefB[3] += BPitch[3];
          pRefF[3] += FPitch[3];
          if constexpr(level >= 5) {
            pRefB[4] += BPitch[4];
            pRefF[4] += FPitch[4];
            if constexpr(level >= 6) {
              pRefB[5] += BPitch[5];
              pRefF[5] += FPitch[5];
            }
          }
        }
      }
    }
  }
}
#pragma warning( pop ) 

#pragma warning( push )
#pragma warning( disable : 4101)
template<int level >
void Degrain1to6_float_C(uint8_t *pDst, BYTE *pDstLsb, int WidthHeightForC, int nDstPitch, const uint8_t *pSrc, int nSrcPitch,
  const uint8_t *pRefB[MAX_DEGRAIN], int BPitch[MAX_DEGRAIN], const uint8_t *pRefF[MAX_DEGRAIN], int FPitch[MAX_DEGRAIN],
  int WSrc,
  int WRefB[MAX_DEGRAIN], int WRefF[MAX_DEGRAIN])
{
  // avoid unnecessary templates. C implementation is here for the sake of correctness and for very small block sizes
  // For all other cases where speed counts, at least SSE2 is used
  // Use only one parameter
  const int blockWidth = (WidthHeightForC >> 16);
  const int blockHeight = (WidthHeightForC & 0xFFFF);

  const float scaleback = 1.0f / (1 << DEGRAIN_WEIGHT_BITS);

  for (int h = 0; h < blockHeight; h++)
  {
    for (int x = 0; x < blockWidth; x++)
    {
      // note: Weights are still integer numbers of DEGRAIN_WEIGHT_BITS resolution
      float val = reinterpret_cast<const float*>(pSrc)[x] * WSrc;
      if constexpr (level >= 1)
        val += reinterpret_cast<const float*>(pRefF[0])[x] * WRefF[0] + reinterpret_cast<const float*>(pRefB[0])[x] * WRefB[0];
      if constexpr (level >= 2)
        val += reinterpret_cast<const float*>(pRefF[1])[x] * WRefF[1] + reinterpret_cast<const float*>(pRefB[1])[x] * WRefB[1];
      if constexpr (level >= 3)
        val += reinterpret_cast<const float*>(pRefF[2])[x] * WRefF[2] + reinterpret_cast<const float*>(pRefB[2])[x] * WRefB[2];
      if constexpr (level >= 4)
        val += reinterpret_cast<const float*>(pRefF[3])[x] * WRefF[3] + reinterpret_cast<const float*>(pRefB[3])[x] * WRefB[3];
      if constexpr (level >= 5)
        val += reinterpret_cast<const float*>(pRefF[4])[x] * WRefF[4] + reinterpret_cast<const float*>(pRefB[4])[x] * WRefB[4];
      if constexpr (level >= 6)
        val += reinterpret_cast<const float*>(pRefF[5])[x] * WRefF[5] + reinterpret_cast<const float*>(pRefB[5])[x] * WRefB[5];

      reinterpret_cast<float*>(pDst)[x] = val * scaleback;
    }
    pDst += nDstPitch;
    pSrc += nSrcPitch;
    pRefB[0] += BPitch[0];
    pRefF[0] += FPitch[0];
    if constexpr(level >= 2) {
      pRefB[1] += BPitch[1];
      pRefF[1] += FPitch[1];
      if constexpr(level >= 3) {
        pRefB[2] += BPitch[2];
        pRefF[2] += FPitch[2];
        if constexpr(level >= 4) {
          pRefB[3] += BPitch[3];
          pRefF[3] += FPitch[3];
          if constexpr(level >= 5) {
            pRefB[4] += BPitch[4];
            pRefF[4] += FPitch[4];
            if constexpr(level >= 6) {
              pRefB[5] += BPitch[5];
              pRefF[5] += FPitch[5];
            }
          }
        }
      }
    }
  }
}
#pragma warning( pop ) 


#pragma warning( push )
#pragma warning( disable : 4101)
// out16_type: 
//   0: native 8 or 16
//   1: 8bit in, lsb
//   2: 8bit in, native16 out
template<int blockWidth, int blockHeight, int out16_type, int level>
void Degrain1to6_sse2(BYTE* pDst, BYTE* pDstLsb, int WidthHeightForC, int nDstPitch, const BYTE* pSrc, int nSrcPitch,
  const BYTE* pRefB[MAX_DEGRAIN], int BPitch[MAX_DEGRAIN], const BYTE* pRefF[MAX_DEGRAIN], int FPitch[MAX_DEGRAIN],
  int WSrc,
  int WRefB[MAX_DEGRAIN], int WRefF[MAX_DEGRAIN])
{
  // avoid unnecessary templates for larger heights
  const int blockHeightParam = (WidthHeightForC & 0xFFFF);
  const int realBlockHeight = blockHeight == 0 ? blockHeightParam : blockHeight;

  constexpr bool lsb_flag = (out16_type == 1);
  constexpr bool out16 = (out16_type == 2);

  __m128i zero = _mm_setzero_si128();
  __m128i ws = _mm_set1_epi16(WSrc);
  __m128i wb1, wf1;
  __m128i wb2, wf2;
  __m128i wb3, wf3;
  __m128i wb4, wf4;
  __m128i wb5, wf5;
  __m128i wb6, wf6;
  wb1 = _mm_set1_epi16(WRefB[0]);
  wf1 = _mm_set1_epi16(WRefF[0]);
  if constexpr (level >= 2) {
    wb2 = _mm_set1_epi16(WRefB[1]);
    wf2 = _mm_set1_epi16(WRefF[1]);
    if constexpr (level >= 3) {
      wb3 = _mm_set1_epi16(WRefB[2]);
      wf3 = _mm_set1_epi16(WRefF[2]);
      if constexpr (level >= 4) {
        wb4 = _mm_set1_epi16(WRefB[3]);
        wf4 = _mm_set1_epi16(WRefF[3]);
        if constexpr (level >= 5) {
          wb5 = _mm_set1_epi16(WRefB[4]);
          wf5 = _mm_set1_epi16(WRefF[4]);
          if constexpr (level >= 6) {
            wb6 = _mm_set1_epi16(WRefB[5]);
            wf6 = _mm_set1_epi16(WRefF[5]);
          }
        }
      }
    }
  }

  if constexpr (lsb_flag || out16)
  {
    // 8 bit in 16 bits out
    __m128i lsb_mask = _mm_set1_epi16(255);
    for (int h = 0; h < realBlockHeight; h++)
    {
      for (int x = 0; x < blockWidth; x += 8)
      {
        __m128i val;
        if constexpr (level == 6) {
          val =
            _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pSrc + x)), zero), ws),
              _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[0] + x)), zero), wb1),
                _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[0] + x)), zero), wf1),
                  _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[1] + x)), zero), wb2),
                    _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[1] + x)), zero), wf2),
                      _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[2] + x)), zero), wb3),
                        _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[2] + x)), zero), wf3),
                          _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[3] + x)), zero), wb4),
                            _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[3] + x)), zero), wf4),
                              _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[4] + x)), zero), wb5),
                                _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[4] + x)), zero), wf5),
                                  _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[5] + x)), zero), wb6),
                                    _mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[5] + x)), zero), wf6)))))))))))));
        }
        if constexpr (level == 5) {
          val =
            _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pSrc + x)), zero), ws),
              _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[0] + x)), zero), wb1),
                _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[0] + x)), zero), wf1),
                  _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[1] + x)), zero), wb2),
                    _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[1] + x)), zero), wf2),
                      _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[2] + x)), zero), wb3),
                        _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[2] + x)), zero), wf3),
                          _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[3] + x)), zero), wb4),
                            _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[3] + x)), zero), wf4),
                              _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[4] + x)), zero), wb5),
                                _mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[4] + x)), zero), wf5)))))))))));
        }
        if constexpr (level == 4) {
          val =
            _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pSrc + x)), zero), ws),
              _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[0] + x)), zero), wb1),
                _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[0] + x)), zero), wf1),
                  _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[1] + x)), zero), wb2),
                    _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[1] + x)), zero), wf2),
                      _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[2] + x)), zero), wb3),
                        _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[2] + x)), zero), wf3),
                          _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[3] + x)), zero), wb4),
                            _mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[3] + x)), zero), wf4)))))))));
        }
        if constexpr (level == 3) {
          val =
            _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pSrc + x)), zero), ws),
              _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[0] + x)), zero), wb1),
                _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[0] + x)), zero), wf1),
                  _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[1] + x)), zero), wb2),
                    _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[1] + x)), zero), wf2),
                      _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[2] + x)), zero), wb3),
                        _mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[2] + x)), zero), wf3)))))));
        }
        else if constexpr (level == 2) {
          val =
            _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pSrc + x)), zero), ws),
              _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[0] + x)), zero), wb1),
                _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[0] + x)), zero), wf1),
                  _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[1] + x)), zero), wb2),
                    _mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[1] + x)), zero), wf2)))));
        }
        else if constexpr (level == 1) {
          val =
            _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pSrc + x)), zero), ws),
              _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[0] + x)), zero), wb1),
                _mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[0] + x)), zero), wf1)));
        }

        if constexpr (lsb_flag) {
          if constexpr (blockWidth == 12) {
            // 8 from the first, 4 from the second cycle
            if (x == 0) { // 1st 8 bytes
              _mm_storel_epi64((__m128i*)(pDst + x), _mm_packus_epi16(_mm_srli_epi16(val, 8), zero));
              _mm_storel_epi64((__m128i*)(pDstLsb + x), _mm_packus_epi16(_mm_and_si128(val, lsb_mask), zero));
            }
            else { // 2nd 4 bytes
              *(uint32_t*)(pDst + x) = _mm_cvtsi128_si32(_mm_packus_epi16(_mm_srli_epi16(val, 8), zero));
              *(uint32_t*)(pDstLsb + x) = _mm_cvtsi128_si32(_mm_packus_epi16(_mm_and_si128(val, lsb_mask), zero));
            }
          }
          else if constexpr (blockWidth >= 8) {
            // 8, 16, 24, ....
            _mm_storel_epi64((__m128i*)(pDst + x), _mm_packus_epi16(_mm_srli_epi16(val, 8), zero));
            _mm_storel_epi64((__m128i*)(pDstLsb + x), _mm_packus_epi16(_mm_and_si128(val, lsb_mask), zero));
          }
          else if constexpr (blockWidth == 6) {
            // x is always 0
            // 4+2 bytes
            __m128i upper = _mm_packus_epi16(_mm_srli_epi16(val, 8), zero);
            __m128i lower = _mm_packus_epi16(_mm_and_si128(val, lsb_mask), zero);
            *(uint32_t*)(pDst + x) = _mm_cvtsi128_si32(upper);
            *(uint32_t*)(pDstLsb + x) = _mm_cvtsi128_si32(lower);
            *(uint16_t*)(pDst + x + 4) = (uint16_t)_mm_cvtsi128_si32(_mm_srli_si128(upper, 4));
            *(uint16_t*)(pDstLsb + x + 4) = (uint16_t)_mm_cvtsi128_si32(_mm_srli_si128(lower, 4));
          }
          else if constexpr (blockWidth == 4) {
            // x is always 0
            *(uint32_t*)(pDst + x) = _mm_cvtsi128_si32(_mm_packus_epi16(_mm_srli_epi16(val, 8), zero));
            *(uint32_t*)(pDstLsb + x) = _mm_cvtsi128_si32(_mm_packus_epi16(_mm_and_si128(val, lsb_mask), zero));
          }
          else if constexpr (blockWidth == 3) {
            // x is always 0
            // 2 + 1 bytes
            uint32_t reslo = _mm_cvtsi128_si32(_mm_packus_epi16(_mm_srli_epi16(val, 8), zero));
            uint32_t reshi = _mm_cvtsi128_si32(_mm_packus_epi16(_mm_and_si128(val, lsb_mask), zero));
            *(uint16_t*)(pDst + x) = (uint16_t)reslo;
            *(uint16_t*)(pDstLsb + x) = (uint16_t)reshi;
            *(uint8_t*)(pDst + x + 2) = (uint8_t)(reslo >> 16);
            *(uint8_t*)(pDstLsb + x + 2) = (uint8_t)(reshi >> 16);
          }
          else if constexpr (blockWidth == 2) {
            // x is always 0
            // 2 bytes
            uint32_t reslo = _mm_cvtsi128_si32(_mm_packus_epi16(_mm_srli_epi16(val, 8), zero));
            uint32_t reshi = _mm_cvtsi128_si32(_mm_packus_epi16(_mm_and_si128(val, lsb_mask), zero));
            *(uint16_t*)(pDst + x) = (uint16_t)reslo;
            *(uint16_t*)(pDstLsb + x) = (uint16_t)reshi;
          }
        }
        else if constexpr (out16) {
          // keep 16 bit result: no split into msb/lsb

          if constexpr (blockWidth == 12) {
            // 8 from the first, 4 from the second cycle
            if (x == 0) { // 1st 8 pixels 16 bytes
              _mm_store_si128((__m128i*)(pDst + x * 2), val);
            }
            else { // 2nd: 4 pixels 8 bytes bytes
              _mm_storel_epi64((__m128i*)(pDst + x * 2), val);
              //*(uint32_t *)(pDst + x) = _mm_cvtsi128_si32(_mm_packus_epi16(_mm_srli_epi16(val, 8), z));
              //*(uint32_t *)(pDstLsb + x) = _mm_cvtsi128_si32(_mm_packus_epi16(_mm_and_si128(val, m), z));
            }
          }
          else if constexpr (blockWidth >= 8) {
            // 8, 16, 24, ....
            _mm_store_si128((__m128i*)(pDst + x * 2), val);
          }
          else if constexpr (blockWidth == 6) {
            // x is always 0
            // 4+2 pixels: 8 + 4 bytes
            _mm_storel_epi64((__m128i*)(pDst + x * 2), val);
            *(uint32_t*)(pDst + x * 2 + 8) = (uint32_t)_mm_cvtsi128_si32(_mm_srli_si128(val, 8));
          }
          else if constexpr (blockWidth == 4) {
            // x is always 0, 4 pixels, 8 bytes
            _mm_storel_epi64((__m128i*)(pDst + x * 2), val);
          }
          else if constexpr (blockWidth == 3) {
            // x is always 0
            // 2+1 pixels: 4 + 2 bytes
            uint32_t reslo = _mm_cvtsi128_si32(val);
            uint16_t reshi = (uint16_t)_mm_cvtsi128_si32(_mm_srli_si128(val, 4));
            *(uint32_t*)(pDst + x * 2) = reslo;
            *(uint16_t*)(pDst + x * 2 + 4) = reshi;
          }
          else if constexpr (blockWidth == 2) {
            // x is always 0
            // 2 pixels: 4 bytes
            uint32_t reslo = _mm_cvtsi128_si32(val);
            *(uint32_t*)(pDst + x * 2) = reslo;
          }
        }
      }
      pDst += nDstPitch;
      if constexpr (lsb_flag)
        pDstLsb += nDstPitch;
      pSrc += nSrcPitch;

      pRefB[0] += BPitch[0];
      pRefF[0] += FPitch[0];
      if constexpr (level >= 2) {
        pRefB[1] += BPitch[1];
        pRefF[1] += FPitch[1];
        if constexpr (level >= 3) {
          pRefB[2] += BPitch[2];
          pRefF[2] += FPitch[2];
        }
        if constexpr (level >= 4) {
          pRefB[3] += BPitch[3];
          pRefF[3] += FPitch[3];
        }
        if constexpr (level >= 5) {
          pRefB[4] += BPitch[4];
          pRefF[4] += FPitch[4];
        }
        if constexpr (level >= 6) {
          pRefB[5] += BPitch[5];
          pRefF[5] += FPitch[5];
        }
      }
    }
  }
  else
  {
    // 8 bit in, 8 bit out
    __m128i rounder = _mm_set1_epi16(128);
    for (int h = 0; h < realBlockHeight; h++)
    {
      for (int x = 0; x < blockWidth; x += 8)
      {
        __m128i res;
        if constexpr (level == 6) {
          res = _mm_packus_epi16(_mm_srli_epi16(
            _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pSrc + x)), zero), ws),
              _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[0] + x)), zero), wb1),
                _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[0] + x)), zero), wf1),
                  _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[1] + x)), zero), wb2),
                    _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[1] + x)), zero), wf2),
                      _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[2] + x)), zero), wb3),
                        _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[2] + x)), zero), wf3),
                          _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[3] + x)), zero), wb4),
                            _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[3] + x)), zero), wf4),
                              _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[4] + x)), zero), wb5),
                                _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[4] + x)), zero), wf5),
                                  _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[5] + x)), zero), wb6),
                                    _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[5] + x)), zero), wf6),
                                      rounder))))))))))))), 8), zero);
        }
        if constexpr (level == 5) {
          res = _mm_packus_epi16(_mm_srli_epi16(
            _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pSrc + x)), zero), ws),
              _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[0] + x)), zero), wb1),
                _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[0] + x)), zero), wf1),
                  _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[1] + x)), zero), wb2),
                    _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[1] + x)), zero), wf2),
                      _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[2] + x)), zero), wb3),
                        _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[2] + x)), zero), wf3),
                          _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[3] + x)), zero), wb4),
                            _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[3] + x)), zero), wf4),
                              _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[4] + x)), zero), wb5),
                                _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[4] + x)), zero), wf5),
                                  rounder))))))))))), 8), zero);
        }
        else if constexpr (level == 4) {
          res = _mm_packus_epi16(_mm_srli_epi16(
            _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pSrc + x)), zero), ws),
              _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[0] + x)), zero), wb1),
                _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[0] + x)), zero), wf1),
                  _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[1] + x)), zero), wb2),
                    _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[1] + x)), zero), wf2),
                      _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[2] + x)), zero), wb3),
                        _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[2] + x)), zero), wf3),
                          _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[3] + x)), zero), wb4),
                            _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[3] + x)), zero), wf4),
                              rounder))))))))), 8), zero);
        }
        else if constexpr (level == 3) {
          res = _mm_packus_epi16(_mm_srli_epi16(
            _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pSrc + x)), zero), ws),
              _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[0] + x)), zero), wb1),
                _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[0] + x)), zero), wf1),
                  _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[1] + x)), zero), wb2),
                    _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[1] + x)), zero), wf2),
                      _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[2] + x)), zero), wb3),
                        _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[2] + x)), zero), wf3),
                          rounder))))))), 8), zero);
          // pDst[x] = (pRefF[x]*WRefF + pSrc[x]*WSrc + pRefB[x]*WRefB + pRefF2[x]*WRefF2 + pRefB2[x]*WRefB2 + pRefF3[x]*WRefF3 + pRefB3[x]*WRefB3 + 128)>>8;
        }
        else if constexpr (level == 2) {
          res = _mm_packus_epi16(_mm_srli_epi16(
            _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pSrc + x)), zero), ws),
              _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[0] + x)), zero), wb1),
                _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[0] + x)), zero), wf1),
                  _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[1] + x)), zero), wb2),
                    _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[1] + x)), zero), wf2),
                      rounder))))), 8), zero);
          // pDst[x] = (pRefF[x]*WRefF + pSrc[x]*WSrc + pRefB[x]*WRefB + pRefF2[x]*WRefF2 + pRefB2[x]*WRefB2 + 128)>>8;
        }
        else if constexpr (level == 1) {
          res = _mm_packus_epi16(_mm_srli_epi16(
            _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pSrc + x)), zero), ws),
              _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB[0] + x)), zero), wb1),
                _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF[0] + x)), zero), wf1),
                  rounder))), 8), zero);
          // pDst[x] = (pRefF[x]*WRefF + pSrc[x]*WSrc + pRefB[x]*WRefB + 128)>>8;// weighted (by SAD) average
        }

        if constexpr (blockWidth == 12) {
          // 8 from the first, 4 from the second cycle
          if (x == 0) { // 1st 8 bytes
            _mm_storel_epi64((__m128i*)(pDst + x), res);
          }
          else { // 2nd 4 bytes
            *(uint32_t*)(pDst + x) = _mm_cvtsi128_si32(res);
          }
        }
        else if constexpr (blockWidth >= 8) {
          // 8, 16, 24, ....
          _mm_storel_epi64((__m128i*)(pDst + x), res);
        }
        else if constexpr (blockWidth == 6) {
          // x is always 0
          // 4+2 bytes
          *(uint32_t*)(pDst + x) = _mm_cvtsi128_si32(res);
          *(uint16_t*)(pDst + x + 4) = (uint16_t)_mm_cvtsi128_si32(_mm_srli_si128(res, 4));
        }
        else if constexpr (blockWidth == 4) {
          // x is always 0
          *(uint32_t*)(pDst + x) = _mm_cvtsi128_si32(res);
        }
        else if constexpr (blockWidth == 3) {
          // x is always 0
          // 2+1 bytes
          uint32_t res32 = _mm_cvtsi128_si32(res);
          *(uint16_t*)(pDst + x) = (uint16_t)res32;
          *(uint8_t*)(pDst + x + 2) = (uint8_t)(res32 >> 16);
        }
        else if constexpr (blockWidth == 2) {
          // x is always 0
          // 2 bytes
          uint32_t res32 = _mm_cvtsi128_si32(res);
          *(uint16_t*)(pDst + x) = (uint16_t)res32;
        }
      }
      pDst += nDstPitch;
      pSrc += nSrcPitch;

      pRefB[0] += BPitch[0];
      pRefF[0] += FPitch[0];
      if constexpr (level >= 2) {
        pRefB[1] += BPitch[1];
        pRefF[1] += FPitch[1];
        if constexpr (level >= 3) {
          pRefB[2] += BPitch[2];
          pRefF[2] += FPitch[2];
          if constexpr (level >= 4) {
            pRefB[3] += BPitch[3];
            pRefF[3] += FPitch[3];
            if constexpr (level >= 5) {
              pRefB[4] += BPitch[4];
              pRefF[4] += FPitch[4];
              if constexpr (level >= 6) {
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
#pragma warning( pop ) 

#pragma warning( push )
#pragma warning( disable : 4101)
// for blockwidth >=2 (4 bytes for blockwidth==2, 8 bytes for blockwidth==4)
// for special height==0 -> internally nHeight comes from variable (for C: both width and height is variable)
template<int blockWidth, int blockHeight, int level, bool lessThan16bits>
void Degrain1to6_16_sse41(BYTE *pDst, BYTE *pDstLsb, int WidthHeightForC, int nDstPitch, const BYTE *pSrc, int nSrcPitch,
  const BYTE *pRefB[MAX_DEGRAIN], int BPitch[MAX_DEGRAIN], const BYTE *pRefF[MAX_DEGRAIN], int FPitch[MAX_DEGRAIN],
  int WSrc,
  int WRefB[MAX_DEGRAIN], int WRefF[MAX_DEGRAIN])
{
  // avoid unnecessary templates for larger heights
  const int blockHeightParam = (WidthHeightForC & 0xFFFF);
  const int realBlockHeight = blockHeight == 0 ? blockHeightParam : blockHeight;

  __m128i z = _mm_setzero_si128();
  __m128i wbf1, wbf2, wbf3, wbf4, wbf5, wbf6;

  // able to do madd for real 16 bit uint16_t data
  const auto signed16_shifter = _mm_set1_epi16(-32768);

  // Interleave Forward and Backward 16 bit weights for madd
  __m128i ws = _mm_set1_epi32((0 << 16) + WSrc);
  wbf1 = _mm_set1_epi32((WRefF[0] << 16) + WRefB[0]);
  if constexpr(level >= 2) {
    wbf2 = _mm_set1_epi32((WRefF[1] << 16) + WRefB[1]);
    if constexpr(level >= 3) {
      wbf3 = _mm_set1_epi32((WRefF[2] << 16) + WRefB[2]);
      if constexpr(level >= 4) {
        wbf4 = _mm_set1_epi32((WRefF[3] << 16) + WRefB[3]);
        if constexpr(level >= 5) {
          wbf5 = _mm_set1_epi32((WRefF[4] << 16) + WRefB[4]);
          if constexpr(level >= 6) {
            wbf6 = _mm_set1_epi32((WRefF[5] << 16) + WRefB[5]);
          }
        }
      }
    }
  }

  __m128i rounder = _mm_set1_epi32(1 << (DEGRAIN_WEIGHT_BITS - 1)); // rounding: 128 (mul by 8 bit wref scale back)
  for (int h = 0; h < realBlockHeight; h++)
  {
    for (int x = 0; x < blockWidth; x += 8 / sizeof(uint16_t))
    {
      __m128i res;
      auto src = _mm_loadl_epi64((__m128i*)(pSrc + x * sizeof(uint16_t)));
      // make signed when unsigned 16 bit mode
      if constexpr(!lessThan16bits)
        src = _mm_add_epi16(src, signed16_shifter);
      src = _mm_unpacklo_epi16(src, z);
      res = _mm_madd_epi16(src, ws); // pSrc[x] * WSrc + 0 * 0
      // pRefF[n][x] * WRefF[n] + pRefB[n][x] * WRefB[n]
      src = _mm_unpacklo_epi16(_mm_loadl_epi64((__m128i*)(pRefB[0] + x * sizeof(uint16_t))), _mm_loadl_epi64((__m128i*)(pRefF[0] + x * sizeof(uint16_t))));
      if constexpr(!lessThan16bits)
        src = _mm_add_epi16(src, signed16_shifter);
      res = _mm_add_epi32(res, _mm_madd_epi16(src, wbf1));
      if constexpr(level >= 2) {
        src = _mm_unpacklo_epi16(_mm_loadl_epi64((__m128i*)(pRefB[1] + x * sizeof(uint16_t))), _mm_loadl_epi64((__m128i*)(pRefF[1] + x * sizeof(uint16_t))));
        if constexpr(!lessThan16bits)
          src = _mm_add_epi16(src, signed16_shifter);
        res = _mm_add_epi32(res, _mm_madd_epi16(src, wbf2));
      }
      if constexpr(level >= 3) {
        src = _mm_unpacklo_epi16(_mm_loadl_epi64((__m128i*)(pRefB[2] + x * sizeof(uint16_t))), _mm_loadl_epi64((__m128i*)(pRefF[2] + x * sizeof(uint16_t))));
        if constexpr(!lessThan16bits)
          src = _mm_add_epi16(src, signed16_shifter);
        res = _mm_add_epi32(res, _mm_madd_epi16(src, wbf3));
      }
      if constexpr(level >= 4) {
        src = _mm_unpacklo_epi16(_mm_loadl_epi64((__m128i*)(pRefB[3] + x * sizeof(uint16_t))), _mm_loadl_epi64((__m128i*)(pRefF[3] + x * sizeof(uint16_t))));
        if constexpr(!lessThan16bits)
          src = _mm_add_epi16(src, signed16_shifter);
        res = _mm_add_epi32(res, _mm_madd_epi16(src, wbf4));
      }
      if constexpr(level >= 5) {
        src = _mm_unpacklo_epi16(_mm_loadl_epi64((__m128i*)(pRefB[4] + x * sizeof(uint16_t))), _mm_loadl_epi64((__m128i*)(pRefF[4] + x * sizeof(uint16_t))));
        if constexpr(!lessThan16bits)
          src = _mm_add_epi16(src, signed16_shifter);
        res = _mm_add_epi32(res, _mm_madd_epi16(src, wbf5));
      }
      if constexpr(level >= 6) {
        src = _mm_unpacklo_epi16(_mm_loadl_epi64((__m128i*)(pRefB[5] + x * sizeof(uint16_t))), _mm_loadl_epi64((__m128i*)(pRefF[5] + x * sizeof(uint16_t))));
        if constexpr(!lessThan16bits)
          src = _mm_add_epi16(src, signed16_shifter);
        res = _mm_add_epi32(res, _mm_madd_epi16(src, wbf6));
      }
      res = _mm_add_epi32(res, rounder); // round
      res = _mm_packs_epi32(_mm_srai_epi32(res, DEGRAIN_WEIGHT_BITS), z);
      // make unsigned when unsigned 16 bit mode
      if constexpr(!lessThan16bits) 
        res = _mm_add_epi16(res, signed16_shifter);

      // store back
      if constexpr(blockWidth == 6) {
        // special, 4+2
        if (x == 0)
          _mm_storel_epi64((__m128i*)(pDst + x * sizeof(uint16_t)), res);
        else
          *(uint32_t *)(pDst + x * sizeof(uint16_t)) = _mm_cvtsi128_si32(res);
      }
      else if constexpr(blockWidth >= 8 / sizeof(uint16_t)) { // block 4 is already 8 bytes
        // 4, 8, 12, ...
        _mm_storel_epi64((__m128i*)(pDst + x * sizeof(uint16_t)), res);
      }
      else if constexpr(blockWidth == 3) { // blockwidth 3 is 6 bytes
        // x == 0 always
        *(uint32_t *)(pDst) = _mm_cvtsi128_si32(res); // 1-4 bytes
        uint32_t res32 = _mm_cvtsi128_si32(_mm_srli_si128(res, 4)); // 5-8 byte
        *(uint16_t *)(pDst + sizeof(uint32_t)) = (uint16_t)res32; // 2 bytes needed
      }
      else { // blockwidth 2 is 4 bytes
        *(uint32_t *)(pDst + x * sizeof(uint16_t)) = _mm_cvtsi128_si32(res);
      }
    }
    pDst += nDstPitch;
    pSrc += nSrcPitch;
    /* usually this results in slower code
    for (int i = 0; i < level; i++) {
    pRefB[i] += BPitch[i];
    pRefF[i] += FPitch[i];
    }
    */
    // todo: pointer additions by xmm simd code: 2 pointers at a time when x64, 4+2 pointers when x32
    pRefB[0] += BPitch[0];
    pRefF[0] += FPitch[0];
    if constexpr(level >= 2) {
      pRefB[1] += BPitch[1];
      pRefF[1] += FPitch[1];
      if constexpr(level >= 3) {
        pRefB[2] += BPitch[2];
        pRefF[2] += FPitch[2];
        if constexpr(level >= 4) {
          pRefB[3] += BPitch[3];
          pRefF[3] += FPitch[3];
          if constexpr(level >= 5) {
            pRefB[4] += BPitch[4];
            pRefF[4] += FPitch[4];
            if constexpr(level >= 6) {
              pRefB[5] += BPitch[5];
              pRefF[5] += FPitch[5];
            }
          }
        }
      }
    }
  }
}
#pragma warning( pop ) 


// Not really related to overlap, but common to MDegrainX functions
// PF 160928: this is bottleneck. Could be optimized with precalc thSAD*thSAD
// PF 180221: experimental conditionless 'double' precision path with precalculated square of thSad
MV_FORCEINLINE int DegrainWeight(int thSAD, double thSAD_sq, int blockSAD)
{
  // Returning directly prevents a divide by 0 if thSAD == blockSAD == 0.
  // keep integer comparison for speed
  if (thSAD <= blockSAD)
    return 0;

  // float is approximately only 24 bit precise, use double
  const double blockSAD_sq = double(blockSAD) * (blockSAD);
  return (int)((double)(1 << DEGRAIN_WEIGHT_BITS)*(thSAD_sq - blockSAD_sq) / (thSAD_sq + blockSAD_sq));
}

template<int level>
MV_FORCEINLINE void norm_weights(int &WSrc, int(&WRefB)[MAX_DEGRAIN], int(&RefF)[MAX_DEGRAIN]);

#endif

