

#include "ClipFnc.h"
#include "CopyCode.h"
#include	"def.h"
#include	"MDegrainN.h"
#include	"MVDegrain3.h"
#include "MVFrame.h"
#include "MVPlane.h"
#include "MVFilter.h"
#include "profile.h"
#include "SuperParams64Bits.h"

#include	<emmintrin.h>
#include	<mmintrin.h>

#include	<cassert>
#include	<cmath>
#include <map>
#include <tuple>
#include <stdint.h>
#include "commonfunctions.h"

// out16_type: 
//   0: native 8 or 16
//   1: 8bit in, lsb
//   2: 8bit in, native16 out
template <typename pixel_t, int blockWidth, int blockHeight, int out16_type>
void DegrainN_C(
  BYTE *pDst, BYTE *pDstLsb, int nDstPitch,
  const BYTE *pSrc, int nSrcPitch,
  const BYTE *pRef[], int Pitch[],
  int Wall[], int trad
)
{

  // avoid unnecessary templates. C implementation is here for the sake of correctness and for very small block sizes
  // For all other cases where speed counts, at least SSE2 is used
  // Use only one parameter
  // PF180306 not yet const int blockWidth = (WidthHeightForC >> 16);
  // PF180306 const int blockHeight = (WidthHeightForC & 0xFFFF);

  constexpr bool lsb_flag = (out16_type == 1);
  constexpr bool out16 = (out16_type == 2);

  if constexpr(lsb_flag || out16)
  {
    // 8 bit base only
    for (int h = 0; h < blockHeight; ++h)
    {
      for (int x = 0; x < blockWidth; ++x)
      {
        int				val = pSrc[x] * Wall[0];
        for (int k = 0; k < trad; ++k)
        {
          val += pRef[k * 2][x] * Wall[k * 2 + 1]
            + pRef[k * 2 + 1][x] * Wall[k * 2 + 2];
        }
        if constexpr(lsb_flag) {
          pDst[x] = (uint8_t)(val >> 8);
          pDstLsb[x] = (uint8_t)(val & 255);
        }
        else { // out16
          reinterpret_cast<uint16_t *>(pDst)[x] = (uint16_t)val;
        }
      }

      pDst += nDstPitch;
      if constexpr(lsb_flag)
        pDstLsb += nDstPitch;
      pSrc += nSrcPitch;
      for (int k = 0; k < trad; ++k)
      {
        pRef[k * 2] += Pitch[k * 2];
        pRef[k * 2 + 1] += Pitch[k * 2 + 1];
      }
    }
  }

  else
  {
    typedef typename std::conditional < sizeof(pixel_t) <= 2, int, float>::type target_t;
    constexpr target_t rounder = (sizeof(pixel_t) <= 2) ? 128 : 0;
    constexpr float scaleback = 1.0f / (1 << DEGRAIN_WEIGHT_BITS);

    // Wall: 8 bit. rounding: 128
    for (int h = 0; h < blockHeight; ++h)
    {
      for (int x = 0; x < blockWidth; ++x)
      {
        target_t val = reinterpret_cast<const pixel_t *>(pSrc)[x] * (target_t)Wall[0] + rounder;
        for (int k = 0; k < trad; ++k)
        {
          val += reinterpret_cast<const pixel_t *>(pRef[k * 2])[x] * (target_t)Wall[k * 2 + 1]
            + reinterpret_cast<const pixel_t *>(pRef[k * 2 + 1])[x] * (target_t)Wall[k * 2 + 2];
        }
        if constexpr(sizeof(pixel_t) <= 2)
          reinterpret_cast<pixel_t *>(pDst)[x] = (pixel_t)(val >> 8); // 8-16bit
        else
          reinterpret_cast<pixel_t *>(pDst)[x] = val * scaleback; // 32bit float
      }

      pDst += nDstPitch;
      pSrc += nSrcPitch;
      for (int k = 0; k < trad; ++k)
      {
        pRef[k * 2] += Pitch[k * 2];
        pRef[k * 2 + 1] += Pitch[k * 2 + 1];
      }
    }
  }
}

#if 0
#ifndef _M_X64
template <int blockWidth, int blockHeight>
void DegrainN_mmx(
  BYTE *pDst, BYTE *pDstLsb, bool lsb_flag, int nDstPitch,
  const BYTE *pSrc, int nSrcPitch,
  const BYTE *pRef[], int Pitch[],
  int Wall[], int trad
)
{
  const __m64			z = _mm_setzero_si64();

  if (lsb_flag)
  {
    const __m64			m = _mm_set1_pi16(255);

    for (int h = 0; h < blockHeight; ++h)
    {
      for (int x = 0; x < blockWidth; x += 4)
      {
        __m64				val = _m_pmullw(
          _m_punpcklbw(*(__m64 *) (pSrc + x), z),
          _mm_set1_pi16(Wall[0])
        );
        for (int k = 0; k < trad; ++k)
        {
          const __m64		s1 = _m_pmullw(
            _m_punpcklbw(*(__m64 *) (pRef[k * 2] + x), z),
            _mm_set1_pi16(Wall[k * 2 + 1])
          );
          const __m64		s2 = _m_pmullw(
            _m_punpcklbw(*(__m64 *) (pRef[k * 2 + 1] + x), z),
            _mm_set1_pi16(Wall[k * 2 + 2])
          );
          val = _m_paddw(val, s1);
          val = _m_paddw(val, s2);
        }
        *(int *)(pDst + x) =
          _m_to_int(_m_packuswb(_m_psrlwi(val, 8), z));
        *(int *)(pDstLsb + x) =
          _m_to_int(_m_packuswb(_mm_and_si64(val, m), z));
      }

      pDst += nDstPitch;
      pDstLsb += nDstPitch;
      pSrc += nSrcPitch;
      for (int k = 0; k < trad; ++k)
      {
        pRef[k * 2] += Pitch[k * 2];
        pRef[k * 2 + 1] += Pitch[k * 2 + 1];
      }
    }
  }

  else
  {
    const __m64		o = _mm_set1_pi16(128);

    for (int h = 0; h < blockHeight; ++h)
    {
      for (int x = 0; x < blockWidth; x += 4)
      {
        __m64				val = _m_paddw(_m_pmullw(
          _m_punpcklbw(*(__m64 *) (pSrc + x), z),
          _mm_set1_pi16(Wall[0])
        ), o);
        for (int k = 0; k < trad; ++k)
        {
          const __m64		s1 = _m_pmullw(
            _m_punpcklbw(*(__m64 *) (pRef[k * 2] + x), z),
            _mm_set1_pi16(Wall[k * 2 + 1])
          );
          const __m64		s2 = _m_pmullw(
            _m_punpcklbw(*(__m64 *) (pRef[k * 2 + 1] + x), z),
            _mm_set1_pi16(Wall[k * 2 + 2])
          );
          val = _m_paddw(val, s1);
          val = _m_paddw(val, s2);
        }
        *(int *)(pDst + x) =
          _m_to_int(_m_packuswb(_m_psrlwi(val, 8), z));
      }

      pDst += nDstPitch;
      pSrc += nSrcPitch;
      for (int k = 0; k < trad; ++k)
      {
        pRef[k * 2] += Pitch[k * 2];
        pRef[k * 2 + 1] += Pitch[k * 2 + 1];
      }
    }
  }

  _m_empty();
}
#endif
#endif

// out16_type: 
//   0: native 8 or 16
//   1: 8bit in, lsb
//   2: 8bit in, native16 out
template <int blockWidth, int blockHeight, int out16_type>
void DegrainN_sse2(
  BYTE *pDst, BYTE *pDstLsb, int nDstPitch,
  const BYTE *pSrc, int nSrcPitch,
  const BYTE *pRef[], int Pitch[],
  int Wall[], int trad
)
{
  assert(!(blockWidth % 8) != 0 && blockWidth != 4);
  // only mod8 or w=4 supported

  // avoid unnecessary templates. C implementation is here for the sake of correctness and for very small block sizes
  // For all other cases where speed counts, at least SSE2 is used
  // Use only one parameter
  // PF180306 not yet const int blockWidth = (WidthHeightForC >> 16);
  // PF180306 const int blockHeight = (WidthHeightForC & 0xFFFF);

  constexpr bool lsb_flag = (out16_type == 1);
  constexpr bool out16 = (out16_type == 2);

  const __m128i	z = _mm_setzero_si128();

  if constexpr(lsb_flag || out16)
  {
    // no rounding
    const __m128i	m = _mm_set1_epi16(255);

    for (int h = 0; h < blockHeight; ++h)
    {
      for (int x = 0; x < blockWidth; x += 8)
      {
        __m128i			val = _mm_mullo_epi16(
          _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i *) (pSrc + x)), z),
          _mm_set1_epi16(Wall[0])
        );
        for (int k = 0; k < trad; ++k)
        {
          const __m128i	s1 = _mm_mullo_epi16(
            _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i *) (pRef[k * 2] + x)), z),
            _mm_set1_epi16(Wall[k * 2 + 1])
          );
          const __m128i	s2 = _mm_mullo_epi16(
            _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i *) (pRef[k * 2 + 1] + x)), z),
            _mm_set1_epi16(Wall[k * 2 + 2])
          );
          val = _mm_add_epi16(val, s1);
          val = _mm_add_epi16(val, s2);
        }
        if constexpr(blockWidth >= 8) {
          if constexpr(lsb_flag) {
            _mm_storel_epi64((__m128i*)(pDst + x), _mm_packus_epi16(_mm_srli_epi16(val, 8), z));
            _mm_storel_epi64((__m128i*)(pDstLsb + x), _mm_packus_epi16(_mm_and_si128(val, m), z));
          }
          else {
            _mm_storeu_si128((__m128i*)(pDst + x * 2), val);
          }
        }
        else if constexpr(blockWidth == 4) {
          if constexpr(lsb_flag) {
            *(uint32_t*)(pDst + x) = _mm_cvtsi128_si32(_mm_packus_epi16(_mm_srli_epi16(val, 8), z));
            *(uint32_t*)(pDstLsb + x) = _mm_cvtsi128_si32(_mm_packus_epi16(_mm_and_si128(val, m), z));
          }
          else {
            _mm_storel_epi64((__m128i*)(pDst + x * 2), val);
          }
        }
      }
      pDst += nDstPitch;
      if constexpr(lsb_flag)
        pDstLsb += nDstPitch;
      pSrc += nSrcPitch;
      for (int k = 0; k < trad; ++k)
      {
        pRef[k * 2] += Pitch[k * 2];
        pRef[k * 2 + 1] += Pitch[k * 2 + 1];
      }
    }
  }

  else
  {
    // base 8 bit -> 8 bit
    const __m128i	o = _mm_set1_epi16(128); // rounding

    for (int h = 0; h < blockHeight; ++h)
    {
      for (int x = 0; x < blockWidth; x += 8)
      {
        __m128i			val = _mm_add_epi16(_mm_mullo_epi16(
          _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i *) (pSrc + x)), z),
          _mm_set1_epi16(Wall[0])
        ), o);
        for (int k = 0; k < trad; ++k)
        {
          const __m128i	s1 = _mm_mullo_epi16(
            _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i *) (pRef[k * 2] + x)), z),
            _mm_set1_epi16(Wall[k * 2 + 1])
          );
          const __m128i	s2 = _mm_mullo_epi16(
            _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i *) (pRef[k * 2 + 1] + x)), z),
            _mm_set1_epi16(Wall[k * 2 + 2])
          );
          val = _mm_add_epi16(val, s1);
          val = _mm_add_epi16(val, s2);
        }
        if constexpr(blockWidth >= 8) {
          _mm_storel_epi64(
            (__m128i*)(pDst + x),
            _mm_packus_epi16(_mm_srli_epi16(val, 8), z)
          );
        }
        else {
          *(uint32_t*)(pDst + x) = _mm_cvtsi128_si32(_mm_packus_epi16(_mm_srli_epi16(val, 8), z));
        }
      }

      pDst += nDstPitch;
      pSrc += nSrcPitch;
      for (int k = 0; k < trad; ++k)
      {
        pRef[k * 2] += Pitch[k * 2];
        pRef[k * 2 + 1] += Pitch[k * 2 + 1];
      }
    }
  }
}

MDegrainN::DenoiseNFunction* MDegrainN::get_denoiseN_function(int BlockX, int BlockY, int _bits_per_pixel, bool _lsb_flag, bool _out16_flag, arch_t arch)
{
  //---------- DENOISE/DEGRAIN
  const int DEGRAIN_TYPE_8BIT = 1;
  const int DEGRAIN_TYPE_8BIT_STACKED = 2;
  const int DEGRAIN_TYPE_8BIT_OUT16 = 4;
  const int DEGRAIN_TYPE_10to14BIT = 8;
  const int DEGRAIN_TYPE_16BIT = 16;
  const int DEGRAIN_TYPE_10to16BIT = DEGRAIN_TYPE_10to14BIT + DEGRAIN_TYPE_16BIT;
  const int DEGRAIN_TYPE_32BIT = 32;
  // BlkSizeX, BlkSizeY, degrain_type, arch_t
  std::map<std::tuple<int, int, int, arch_t>, DenoiseNFunction*> func_degrain;
  using std::make_tuple;

  int type_to_search;
  if (_bits_per_pixel == 8) {
    if (_out16_flag)
      type_to_search = DEGRAIN_TYPE_8BIT_OUT16;
    else if (_lsb_flag)
      type_to_search = DEGRAIN_TYPE_8BIT_STACKED;
    else
      type_to_search = DEGRAIN_TYPE_8BIT;
  }
  else if (_bits_per_pixel <= 16)
    type_to_search = DEGRAIN_TYPE_10to16BIT;
  else if (_bits_per_pixel == 32)
    type_to_search = DEGRAIN_TYPE_32BIT;
  else
    return nullptr;


  // 8bit C, 8bit lsb C, 8bit out16 C, 10-16 bit C, float C (same for all, no blocksize templates)
#define MAKE_FN(x, y) \
func_degrain[make_tuple(x, y, DEGRAIN_TYPE_8BIT, NO_SIMD)] = DegrainN_C<uint8_t, x, y, 0>; \
func_degrain[make_tuple(x, y, DEGRAIN_TYPE_8BIT_STACKED, NO_SIMD)] = DegrainN_C<uint8_t, x, y, 1>; \
func_degrain[make_tuple(x, y, DEGRAIN_TYPE_8BIT_OUT16, NO_SIMD)] = DegrainN_C<uint8_t, x, y, 2>; \
func_degrain[make_tuple(x, y, DEGRAIN_TYPE_10to16BIT, NO_SIMD)] = DegrainN_C<uint16_t, x, y, 0>; \
func_degrain[make_tuple(x, y, DEGRAIN_TYPE_32BIT, NO_SIMD)] = DegrainN_C<float, x, y, 0>;
    MAKE_FN(64, 64)
    MAKE_FN(64, 48)
    MAKE_FN(64, 32)
    MAKE_FN(64, 16)
    MAKE_FN(48, 64)
    MAKE_FN(48, 48)
    MAKE_FN(48, 24)
    MAKE_FN(48, 12)
    MAKE_FN(32, 64)
    MAKE_FN(32, 32)
    MAKE_FN(32, 24)
    MAKE_FN(32, 16)
    MAKE_FN(32, 8)
    MAKE_FN(24, 48)
    MAKE_FN(24, 32)
    MAKE_FN(24, 24)
    MAKE_FN(24, 12)
    MAKE_FN(24, 6)
    MAKE_FN(16, 64)
    MAKE_FN(16, 32)
    MAKE_FN(16, 16)
    MAKE_FN(16, 12)
    MAKE_FN(16, 8)
    MAKE_FN(16, 4)
    MAKE_FN(16, 2)
    MAKE_FN(16, 1)
    MAKE_FN(12, 48)
    MAKE_FN(12, 24)
    MAKE_FN(12, 16)
    MAKE_FN(12, 12)
    MAKE_FN(12, 6)
    MAKE_FN(12, 3)
    MAKE_FN(8, 32)
    MAKE_FN(8, 16)
    MAKE_FN(8, 8)
    MAKE_FN(8, 4)
    MAKE_FN(8, 2)
    MAKE_FN(8, 1)
    MAKE_FN(6, 24)
    MAKE_FN(6, 12)
    MAKE_FN(6, 6)
    MAKE_FN(6, 3)
    MAKE_FN(4, 8)
    MAKE_FN(4, 4)
    MAKE_FN(4, 2)
    MAKE_FN(4, 1)
    MAKE_FN(3, 6)
    MAKE_FN(3, 3)
    MAKE_FN(2, 4)
    MAKE_FN(2, 2)
    MAKE_FN(2, 1)
#undef MAKE_FN
#undef MAKE_FN_LEVEL

      // and the SSE2 versions for 8 bit
#define MAKE_FN(x, y) \
func_degrain[make_tuple(x, y, DEGRAIN_TYPE_8BIT, USE_SSE2)] = DegrainN_sse2<x, y, 0>; \
func_degrain[make_tuple(x, y, DEGRAIN_TYPE_8BIT_STACKED, USE_SSE2)] = DegrainN_sse2<x, y, 1>; \
func_degrain[make_tuple(x, y, DEGRAIN_TYPE_8BIT_OUT16, USE_SSE2)] = DegrainN_sse2<x, y, 2>;
  MAKE_FN(64, 64)
    MAKE_FN(64, 48)
    MAKE_FN(64, 32)
    MAKE_FN(64, 16)
    MAKE_FN(48, 64)
    MAKE_FN(48, 48)
    MAKE_FN(48, 24)
    MAKE_FN(48, 12)
    MAKE_FN(32, 64)
    MAKE_FN(32, 32)
    MAKE_FN(32, 24)
    MAKE_FN(32, 16)
    MAKE_FN(32, 8)
    MAKE_FN(24, 48)
    MAKE_FN(24, 32)
    MAKE_FN(24, 24)
    MAKE_FN(24, 12)
    MAKE_FN(24, 6)
    MAKE_FN(16, 64)
    MAKE_FN(16, 32)
    MAKE_FN(16, 16)
    MAKE_FN(16, 12)
    MAKE_FN(16, 8)
    MAKE_FN(16, 4)
    MAKE_FN(16, 2)
    MAKE_FN(16, 1)
    //MAKE_FN(12, 48) // w is mod8 or w==4 supported
    //MAKE_FN(12, 24)
    //MAKE_FN(12, 16)
    //MAKE_FN(12, 12)
    //MAKE_FN(12, 6)
    //MAKE_FN(12, 3) 
    MAKE_FN(8, 32)
    MAKE_FN(8, 16)
    MAKE_FN(8, 8)
    MAKE_FN(8, 4)
    MAKE_FN(8, 2)
    MAKE_FN(8, 1)
    //MAKE_FN(6, 24) // w is mod8 or w==4 supported
    //MAKE_FN(6, 12)
    //MAKE_FN(6, 6)
    //MAKE_FN(6, 3)
    MAKE_FN(4, 8)
    MAKE_FN(4, 4)
    MAKE_FN(4, 2)
    MAKE_FN(4, 1)
    //MAKE_FN(3, 6) // w is mod8 or w==4 supported
    //MAKE_FN(3, 3)
    //MAKE_FN(2, 4) // no 2 byte width, only C
    //MAKE_FN(2, 2) // no 2 byte width, only C
    //MAKE_FN(2, 1) // no 2 byte width, only C
#undef MAKE_FN
#undef MAKE_FN_LEVEL

  DenoiseNFunction* result = nullptr;
  arch_t archlist[] = { USE_AVX2, USE_AVX, USE_SSE41, USE_SSE2, NO_SIMD };
  int index = 0;
  while (result == nullptr) {
    arch_t current_arch_try = archlist[index++];
    if (current_arch_try > arch) continue;
    result = func_degrain[make_tuple(BlockX, BlockY, type_to_search, current_arch_try)];
    if (result == nullptr && current_arch_try == NO_SIMD)
      break;
  }
  return result;
}



MDegrainN::MDegrainN(
  PClip child, PClip super, PClip mvmulti, int trad,
  sad_t thsad, sad_t thsadc, int yuvplanes, float nlimit, float nlimitc,
  sad_t nscd1, int nscd2, bool isse_flag, bool planar_flag, bool lsb_flag,
  sad_t thsad2, sad_t thsadc2, bool mt_flag, bool out16_flag, IScriptEnvironment* env_ptr
)
  : GenericVideoFilter(child)
  , MVFilter(mvmulti, "MDegrainN", env_ptr, 1, 0)
  , _mv_clip_arr()
  , _trad(trad)
  , _yuvplanes(yuvplanes)
  , _nlimit(nlimit)
  , _nlimitc(nlimitc)
  , _super(super)
  , _planar_flag(planar_flag)
  , _lsb_flag(lsb_flag)
  , _mt_flag(mt_flag)
  , _out16_flag(out16_flag)
  , _height_lsb_or_out16_mul((lsb_flag || out16_flag) ? 2 : 1)
  , _nsupermodeyuv(-1)
  , _dst_planes(nullptr)
  , _src_planes(nullptr)
  , _overwins()
  , _overwins_uv()
  , _oversluma_ptr(0)
  , _overschroma_ptr(0)
  , _oversluma16_ptr(0)
  , _overschroma16_ptr(0)
  , _oversluma32_ptr(0)
  , _overschroma32_ptr(0)
  , _oversluma_lsb_ptr(0)
  , _overschroma_lsb_ptr(0)
  , _degrainluma_ptr(0)
  , _degrainchroma_ptr(0)
  , _dst_short()
  , _dst_short_pitch()
  , _dst_int()
  , _dst_int_pitch()
  //,	_usable_flag_arr ()
  //,	_planes_ptr ()
  //,	_dst_ptr_arr ()
  //,	_src_ptr_arr ()
  //,	_dst_pitch_arr ()
  //,	_src_pitch_arr ()
  //,	_lsb_offset_arr ()
  , _covered_width(0)
  , _covered_height(0)
  , _boundary_cnt_arr()
{
  has_at_least_v8 = true;
  try { env_ptr->CheckVersion(8); }
  catch (const AvisynthError&) { has_at_least_v8 = false; }

  if (trad > MAX_TEMP_RAD)
  {
    env_ptr->ThrowError(
      "MDegrainN: temporal radius too large (max %d)",
      MAX_TEMP_RAD
    );
  }
  else if (trad < 1)
  {
    env_ptr->ThrowError("MDegrainN: temporal radius must be at least 1.");
  }

  _mv_clip_arr.resize(_trad * 2);
  for (int k = 0; k < _trad * 2; ++k)
  {
    _mv_clip_arr[k]._clip_sptr = SharedPtr <MVClip>(
      new MVClip(mvmulti, nscd1, nscd2, env_ptr, _trad * 2, k)
      );

    static const char *name_0[2] = { "mvbw", "mvfw" };
    char txt_0[127 + 1];
    sprintf(txt_0, "%s%d", name_0[k & 1], 1 + k / 2);
    CheckSimilarity(*(_mv_clip_arr[k]._clip_sptr), txt_0, env_ptr);
  }

  const sad_t mv_thscd1 = _mv_clip_arr[0]._clip_sptr->GetThSCD1();
  thsad = (uint64_t)thsad   * mv_thscd1 / nscd1;	// normalize to block SAD
  thsadc = (uint64_t)thsadc  * mv_thscd1 / nscd1;	// chroma
  thsad2 = (uint64_t)thsad2  * mv_thscd1 / nscd1;
  thsadc2 = (uint64_t)thsadc2 * mv_thscd1 / nscd1;

  const ::VideoInfo &vi_super = _super->GetVideoInfo();

  if (!vi.IsSameColorspace(_super->GetVideoInfo()))
    env_ptr->ThrowError("MDegrainN: source and super clip video format is different!");

  // v2.7.39- make subsampling independent from motion vector's origin:
  // because xRatioUV and yRatioUV: in MVFilter, property of motion vectors
  xRatioUV_super = 1;
  yRatioUV_super = 1;
  if (!vi.IsY() && !vi.IsRGB()) {
    xRatioUV_super = vi.IsYUY2() ? 2 : (1 << vi.GetPlaneWidthSubsampling(PLANAR_U));
    yRatioUV_super = vi.IsYUY2() ? 1 : (1 << vi.GetPlaneHeightSubsampling(PLANAR_U));
  }
  nLogxRatioUV_super = ilog2(xRatioUV_super);
  nLogyRatioUV_super = ilog2(yRatioUV_super);

  pixelsize_super = vi_super.ComponentSize(); // of MVFilter
  bits_per_pixel_super = vi_super.BitsPerComponent();

  _cpuFlags = isse_flag ? env_ptr->GetCPUFlags() : 0;

// get parameters of prepared super clip - v2.0
  SuperParams64Bits params;
  memcpy(&params, &vi_super.num_audio_samples, 8);
  const int nHeightS = params.nHeight;
  const int nSuperHPad = params.nHPad;
  const int nSuperVPad = params.nVPad;
  const int nSuperPel = params.nPel;
  const int nSuperLevels = params.nLevels;
  _nsupermodeyuv = params.nModeYUV;

  // no need for SAD scaling, it is coming from the mv clip analysis. nSCD1 is already scaled in MVClip constructor
  /* must be good from 2.7.13.22
  thsad = sad_t(thsad / 255.0 * ((1 << bits_per_pixel) - 1));
  thsadc = sad_t(thsadc / 255.0 * ((1 << bits_per_pixel) - 1));
  thsad2 = sad_t(thsad2 / 255.0 * ((1 << bits_per_pixel) - 1));
  thsadc2 = sad_t(thsadc2 / 255.0 * ((1 << bits_per_pixel) - 1));
  */

  for (int k = 0; k < _trad * 2; ++k)
  {
    MvClipInfo &c_info = _mv_clip_arr[k];

    c_info._gof_sptr = SharedPtr <MVGroupOfFrames>(new MVGroupOfFrames(
      nSuperLevels,
      nWidth,
      nHeight,
      nSuperPel,
      nSuperHPad,
      nSuperVPad,
      _nsupermodeyuv,
      _cpuFlags,
      xRatioUV_super,
      yRatioUV_super,
      pixelsize_super,
      bits_per_pixel_super,
      mt_flag
    ));

    // Computes the SAD thresholds for this source frame, a cosine-shaped
    // smooth transition between thsad(c) and thsad(c)2.
    const int		d = k / 2 + 1;
    c_info._thsad = ClipFnc::interpolate_thsad(thsad, thsad2, d, _trad);
    c_info._thsadc = ClipFnc::interpolate_thsad(thsadc, thsadc2, d, _trad);
    c_info._thsad_sq = double(c_info._thsad) * double(c_info._thsad);
    c_info._thsadc_sq = double(c_info._thsadc) * double(c_info._thsadc);
  }

  const int nSuperWidth = vi_super.width;
  pixelsize_super_shift = ilog2(pixelsize_super);

  if (nHeight != nHeightS
    || nHeight != vi.height
    || nWidth != nSuperWidth - nSuperHPad * 2
    || nWidth != vi.width
    || nPel != nSuperPel)
  {
    env_ptr->ThrowError("MDegrainN : wrong source or super frame size");
  }

  if(lsb_flag && (pixelsize != 1 || pixelsize_super != 1))
    env_ptr->ThrowError("MDegrainN : lsb_flag only for 8 bit sources");

  if (out16_flag) {
    if (pixelsize != 1 || pixelsize_super != 1)
      env_ptr->ThrowError("MDegrainN : out16 flag only for 8 bit sources");
    if (!vi.IsY8() && !vi.IsYV12() && !vi.IsYV16() && !vi.IsYV24())
      env_ptr->ThrowError("MDegrainN : only YV8, YV12, YV16 or YV24 allowed for out16");
  }

  if (lsb_flag && out16_flag)
    env_ptr->ThrowError("MDegrainN : cannot specify both lsb and out16 flag");

  // output can be different bit depth from input
  pixelsize_output = pixelsize_super;
  bits_per_pixel_output = bits_per_pixel_super;
  pixelsize_output_shift = pixelsize_super_shift;
  if (out16_flag) {
    pixelsize_output = sizeof(uint16_t);
    bits_per_pixel_output = 16;
    pixelsize_output_shift = ilog2(pixelsize_output);
  }

  if ((pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2 && !_planar_flag)
  {
    _dst_planes = std::unique_ptr <YUY2Planes>(
      new YUY2Planes(nWidth, nHeight * _height_lsb_or_out16_mul)
      );
    _src_planes = std::unique_ptr <YUY2Planes>(
      new YUY2Planes(nWidth, nHeight)
      );
  }
  _dst_short_pitch = ((nWidth + 15) / 16) * 16;
  _dst_int_pitch = _dst_short_pitch;
  if (nOverlapX > 0 || nOverlapY > 0)
  {
    _overwins = std::unique_ptr <OverlapWindows>(
      new OverlapWindows(nBlkSizeX, nBlkSizeY, nOverlapX, nOverlapY)
      );
    _overwins_uv = std::unique_ptr <OverlapWindows>(new OverlapWindows(
      nBlkSizeX >> nLogxRatioUV_super, nBlkSizeY >> nLogyRatioUV_super,
      nOverlapX >> nLogxRatioUV_super, nOverlapY >> nLogyRatioUV_super
    ));
    if (_lsb_flag || pixelsize_output > 1)
    {
      _dst_int.resize(_dst_int_pitch * nHeight);
    }
    else
    {
      _dst_short.resize(_dst_short_pitch * nHeight);
    }
  }
  if (nOverlapY > 0)
  {
    _boundary_cnt_arr.resize(nBlkY);
  }

    // in overlaps.h
    // OverlapsLsbFunction
    // OverlapsFunction
    // in M(V)DegrainX: DenoiseXFunction
  arch_t arch;
  if ((_cpuFlags & CPUF_AVX2) != 0)
    arch = USE_AVX2;
  else if ((_cpuFlags & CPUF_AVX) != 0)
    arch = USE_AVX;
  else if ((_cpuFlags & CPUF_SSE4_1) != 0)
    arch = USE_SSE41;
  else if ((_cpuFlags & CPUF_SSE2) != 0)
    arch = USE_SSE2;
  else
    arch = NO_SIMD;

// C only -> NO_SIMD
  _oversluma_lsb_ptr = get_overlaps_lsb_function(nBlkSizeX, nBlkSizeY, sizeof(uint8_t), NO_SIMD);
  _overschroma_lsb_ptr = get_overlaps_lsb_function(nBlkSizeX / xRatioUV_super, nBlkSizeY / yRatioUV_super, sizeof(uint8_t), NO_SIMD);

  _oversluma_ptr = get_overlaps_function(nBlkSizeX, nBlkSizeY, sizeof(uint8_t), arch);
  _overschroma_ptr = get_overlaps_function(nBlkSizeX / xRatioUV_super, nBlkSizeY / yRatioUV_super, sizeof(uint8_t), arch);

  _oversluma16_ptr = get_overlaps_function(nBlkSizeX, nBlkSizeY, sizeof(uint16_t), arch);
  _overschroma16_ptr = get_overlaps_function(nBlkSizeX >> nLogxRatioUV_super, nBlkSizeY >> nLogyRatioUV_super, sizeof(uint16_t), arch);

  _oversluma32_ptr = get_overlaps_function(nBlkSizeX, nBlkSizeY, sizeof(float), arch);
  _overschroma32_ptr = get_overlaps_function(nBlkSizeX >> nLogxRatioUV_super, nBlkSizeY >> nLogyRatioUV_super, sizeof(float), arch);

  _degrainluma_ptr = get_denoiseN_function(nBlkSizeX, nBlkSizeY, bits_per_pixel_super, lsb_flag, out16_flag, arch);
  _degrainchroma_ptr = get_denoiseN_function(nBlkSizeX / xRatioUV_super, nBlkSizeY / yRatioUV_super, bits_per_pixel_super, lsb_flag, out16_flag, arch);

  if (!_oversluma_lsb_ptr)
    env_ptr->ThrowError("MDegrainN : no valid _oversluma_lsb_ptr function for %dx%d, pixelsize=%d, lsb_flag=%d", nBlkSizeX, nBlkSizeY, pixelsize_super, (int)lsb_flag);
  if (!_overschroma_lsb_ptr)
    env_ptr->ThrowError("MDegrainN : no valid _overschroma_lsb_ptr function for %dx%d, pixelsize=%d, lsb_flag=%d", nBlkSizeX, nBlkSizeY, pixelsize_super, (int)lsb_flag);
  if (!_oversluma_ptr)
    env_ptr->ThrowError("MDegrainN : no valid _oversluma_ptr function for %dx%d, pixelsize=%d, lsb_flag=%d", nBlkSizeX, nBlkSizeY, pixelsize_super, (int)lsb_flag);
  if (!_overschroma_ptr)
    env_ptr->ThrowError("MDegrainN : no valid _overschroma_ptr function for %dx%d, pixelsize=%d, lsb_flag=%d", nBlkSizeX, nBlkSizeY, pixelsize_super, (int)lsb_flag);
  if (!_degrainluma_ptr)
    env_ptr->ThrowError("MDegrainN : no valid _degrainluma_ptr function for %dx%d, pixelsize=%d, lsb_flag=%d", nBlkSizeX, nBlkSizeY, pixelsize_super, (int)lsb_flag);
  if (!_degrainchroma_ptr)
    env_ptr->ThrowError("MDegrainN : no valid _degrainchroma_ptr function for %dx%d, pixelsize=%d, lsb_flag=%d", nBlkSizeX, nBlkSizeY, pixelsize_super, (int)lsb_flag);

  if ((_cpuFlags & CPUF_SSE2) != 0)
  {
    if(out16_flag)
      LimitFunction = LimitChanges_src8_target16_c; // todo SSE2
    else if (pixelsize_super == 1)
      LimitFunction = LimitChanges_sse2_new<uint8_t, 0>;
    else if (pixelsize_super == 2) { // pixelsize_super == 2
      if ((_cpuFlags & CPUF_SSE4_1) != 0)
        LimitFunction = LimitChanges_sse2_new<uint16_t, 1>;
      else
        LimitFunction = LimitChanges_sse2_new<uint16_t, 0>;
    }
    else {
      LimitFunction = LimitChanges_float_c; // no SSE2
    }
  }
  else
  {
    if (out16_flag)
      LimitFunction = LimitChanges_src8_target16_c; // todo SSE2
    else if (pixelsize_super == 1)
      LimitFunction = LimitChanges_c<uint8_t>;
    else if (pixelsize_super == 2)
      LimitFunction = LimitChanges_c<uint16_t>;
    else
      LimitFunction = LimitChanges_float_c;
  }

  //---------- end of functions

  // 16 bit output hack
  if (_lsb_flag)
  {
    vi.height <<= 1;
  }

  if (out16_flag) {
    if (vi.IsY8())
      vi.pixel_type = VideoInfo::CS_Y16;
    else if (vi.IsYV12())
      vi.pixel_type = VideoInfo::CS_YUV420P16;
    else if (vi.IsYV16())
      vi.pixel_type = VideoInfo::CS_YUV422P16;
    else if (vi.IsYV24())
      vi.pixel_type = VideoInfo::CS_YUV444P16;
  }
}



MDegrainN::~MDegrainN()
{
  // Nothing
}

static void plane_copy_8_to_16_c(uint8_t *dstp, int dstpitch, const uint8_t *srcp, int srcpitch, int width, int height)
{
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      reinterpret_cast<uint16_t *>(dstp)[x] = srcp[x] << 8;
    }
    dstp += dstpitch;
    srcp += srcpitch;
  }
}


::PVideoFrame __stdcall MDegrainN::GetFrame(int n, ::IScriptEnvironment* env_ptr)
{
  _covered_width = nBlkX * (nBlkSizeX - nOverlapX) + nOverlapX;
  _covered_height = nBlkY * (nBlkSizeY - nOverlapY) + nOverlapY;

  const BYTE * pRef[MAX_TEMP_RAD * 2][3];
  int nRefPitches[MAX_TEMP_RAD * 2][3];
  unsigned char *pDstYUY2;
  const unsigned char *pSrcYUY2;
  int nDstPitchYUY2;
  int nSrcPitchYUY2;

  for (int k2 = 0; k2 < _trad * 2; ++k2)
  {
    // reorder ror regular frames order in v2.0.9.2
    const int k = reorder_ref(k2);

    // v2.0.9.2 - it seems we do not need in vectors clip anymore when we
    // finished copying them to fakeblockdatas
    MVClip &mv_clip = *(_mv_clip_arr[k]._clip_sptr);
    ::PVideoFrame mv = mv_clip.GetFrame(n, env_ptr);
    mv_clip.Update(mv, env_ptr);
    _usable_flag_arr[k] = mv_clip.IsUsable();
  }

  PVideoFrame src = child->GetFrame(n, env_ptr);
  PVideoFrame dst = has_at_least_v8 ? env_ptr->NewVideoFrameP(vi, &src) : env_ptr->NewVideoFrame(vi); // frame property support

  if ((pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2)
  {
    if (!_planar_flag)
    {
      pDstYUY2 = dst->GetWritePtr();
      nDstPitchYUY2 = dst->GetPitch();
      _dst_ptr_arr[0] = _dst_planes->GetPtr();
      _dst_ptr_arr[1] = _dst_planes->GetPtrU();
      _dst_ptr_arr[2] = _dst_planes->GetPtrV();
      _dst_pitch_arr[0] = _dst_planes->GetPitch();
      _dst_pitch_arr[1] = _dst_planes->GetPitchUV();
      _dst_pitch_arr[2] = _dst_planes->GetPitchUV();

      pSrcYUY2 = src->GetReadPtr();
      nSrcPitchYUY2 = src->GetPitch();
      _src_ptr_arr[0] = _src_planes->GetPtr();
      _src_ptr_arr[1] = _src_planes->GetPtrU();
      _src_ptr_arr[2] = _src_planes->GetPtrV();
      _src_pitch_arr[0] = _src_planes->GetPitch();
      _src_pitch_arr[1] = _src_planes->GetPitchUV();
      _src_pitch_arr[2] = _src_planes->GetPitchUV();

      YUY2ToPlanes(
        pSrcYUY2, nSrcPitchYUY2, nWidth, nHeight,
        _src_ptr_arr[0], _src_pitch_arr[0],
        _src_ptr_arr[1], _src_ptr_arr[2], _src_pitch_arr[1],
        _cpuFlags
      );
    }
    else
    {
      _dst_ptr_arr[0] = dst->GetWritePtr();
      _dst_ptr_arr[1] = _dst_ptr_arr[0] + nWidth;
      _dst_ptr_arr[2] = _dst_ptr_arr[1] + nWidth / 2; //yuy2 xratio
      _dst_pitch_arr[0] = dst->GetPitch();
      _dst_pitch_arr[1] = _dst_pitch_arr[0];
      _dst_pitch_arr[2] = _dst_pitch_arr[0];
      _src_ptr_arr[0] = src->GetReadPtr();
      _src_ptr_arr[1] = _src_ptr_arr[0] + nWidth;
      _src_ptr_arr[2] = _src_ptr_arr[1] + nWidth / 2;
      _src_pitch_arr[0] = src->GetPitch();
      _src_pitch_arr[1] = _src_pitch_arr[0];
      _src_pitch_arr[2] = _src_pitch_arr[0];
    }
  }
  else
  {
    _dst_ptr_arr[0] = YWPLAN(dst);
    _dst_ptr_arr[1] = UWPLAN(dst);
    _dst_ptr_arr[2] = VWPLAN(dst);
    _dst_pitch_arr[0] = YPITCH(dst);
    _dst_pitch_arr[1] = UPITCH(dst);
    _dst_pitch_arr[2] = VPITCH(dst);
    _src_ptr_arr[0] = YRPLAN(src);
    _src_ptr_arr[1] = URPLAN(src);
    _src_ptr_arr[2] = VRPLAN(src);
    _src_pitch_arr[0] = YPITCH(src);
    _src_pitch_arr[1] = UPITCH(src);
    _src_pitch_arr[2] = VPITCH(src);
  }

  _lsb_offset_arr[0] = _dst_pitch_arr[0] * nHeight;
  _lsb_offset_arr[1] = _dst_pitch_arr[1] * (nHeight >> nLogyRatioUV_super);
  _lsb_offset_arr[2] = _dst_pitch_arr[2] * (nHeight >> nLogyRatioUV_super);

  if (_lsb_flag)
  {
    memset(_dst_ptr_arr[0] + _lsb_offset_arr[0], 0, _lsb_offset_arr[0]);
    if (!_planar_flag)
    {
      memset(_dst_ptr_arr[1] + _lsb_offset_arr[1], 0, _lsb_offset_arr[1]);
      memset(_dst_ptr_arr[2] + _lsb_offset_arr[2], 0, _lsb_offset_arr[2]);
    }
  }

  ::PVideoFrame ref[MAX_TEMP_RAD * 2];

  for (int k2 = 0; k2 < _trad * 2; ++k2)
  {
    // reorder ror regular frames order in v2.0.9.2
    const int k = reorder_ref(k2);
    MVClip &mv_clip = *(_mv_clip_arr[k]._clip_sptr);
    mv_clip.use_ref_frame(ref[k], _usable_flag_arr[k], _super, n, env_ptr);
  }

  if ((pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2)
  {
    for (int k2 = 0; k2 < _trad * 2; ++k2)
    {
      const int k = reorder_ref(k2);
      if (_usable_flag_arr[k])
      {
        pRef[k][0] = ref[k]->GetReadPtr();
        pRef[k][1] = pRef[k][0] + ref[k]->GetRowSize() / 2;
        pRef[k][2] = pRef[k][1] + ref[k]->GetRowSize() / 4;
        nRefPitches[k][0] = ref[k]->GetPitch();
        nRefPitches[k][1] = nRefPitches[k][0];
        nRefPitches[k][2] = nRefPitches[k][0];
      }
    }
  }
  else
  {
    for (int k2 = 0; k2 < _trad * 2; ++k2)
    {
      const int k = reorder_ref(k2);
      if (_usable_flag_arr[k])
      {
        pRef[k][0] = YRPLAN(ref[k]);
        pRef[k][1] = URPLAN(ref[k]);
        pRef[k][2] = VRPLAN(ref[k]);
        nRefPitches[k][0] = YPITCH(ref[k]);
        nRefPitches[k][1] = UPITCH(ref[k]);
        nRefPitches[k][2] = VPITCH(ref[k]);
      }
    }
  }

  memset(_planes_ptr, 0, _trad * 2 * sizeof(_planes_ptr[0]));

  for (int k2 = 0; k2 < _trad * 2; ++k2)
  {
    const int k = reorder_ref(k2);
    MVGroupOfFrames &gof = *(_mv_clip_arr[k]._gof_sptr);
    gof.Update(
      _yuvplanes,
      const_cast <BYTE *> (pRef[k][0]), nRefPitches[k][0],
      const_cast <BYTE *> (pRef[k][1]), nRefPitches[k][1],
      const_cast <BYTE *> (pRef[k][2]), nRefPitches[k][2]
    );
    if (_yuvplanes & YPLANE)
    {
      _planes_ptr[k][0] = gof.GetFrame(0)->GetPlane(YPLANE);
    }
    if (_yuvplanes & UPLANE)
    {
      _planes_ptr[k][1] = gof.GetFrame(0)->GetPlane(UPLANE);
    }
    if (_yuvplanes & VPLANE)
    {
      _planes_ptr[k][2] = gof.GetFrame(0)->GetPlane(VPLANE);
    }
  }

  PROFILE_START(MOTION_PROFILE_COMPENSATION);

  //-------------------------------------------------------------------------
  // LUMA plane Y

  if ((_yuvplanes & YPLANE) == 0)
  {
    if (_out16_flag) {
      // copy 8 bit source to 16bit target
      plane_copy_8_to_16_c(_dst_ptr_arr[0], _dst_pitch_arr[0],
        _src_ptr_arr[0], _src_pitch_arr[0],
        nWidth, nHeight);
    }
    else {
      BitBlt(
        _dst_ptr_arr[0], _dst_pitch_arr[0],
        _src_ptr_arr[0], _src_pitch_arr[0],
        nWidth << pixelsize_super_shift, nHeight
      );
    }
  }
  else
  {
    Slicer slicer(_mt_flag);

    if (nOverlapX == 0 && nOverlapY == 0)
    {
      slicer.start(
        nBlkY,
        *this,
        &MDegrainN::process_luma_normal_slice
      );
      slicer.wait();
    }

    // Overlap
    else
    {
      uint16_t *pDstShort = (_dst_short.empty()) ? 0 : &_dst_short[0];
      int *pDstInt = (_dst_int.empty()) ? 0 : &_dst_int[0];

      if (_lsb_flag || pixelsize_output>1)
      {
        MemZoneSet(
          reinterpret_cast <unsigned char *> (pDstInt), 0,
          _covered_width * sizeof(int), _covered_height, 0, 0, _dst_int_pitch * sizeof(int)
        );
      }
      else
      {
        MemZoneSet(
          reinterpret_cast <unsigned char *> (pDstShort), 0,
          _covered_width * sizeof(short), _covered_height, 0, 0, _dst_short_pitch * sizeof(short)
        );
      }

      if (nOverlapY > 0)
      {
        memset(
          &_boundary_cnt_arr[0],
          0,
          _boundary_cnt_arr.size() * sizeof(_boundary_cnt_arr[0])
        );
      }

      slicer.start(
        nBlkY,
        *this,
        &MDegrainN::process_luma_overlap_slice,
        2
      );
      slicer.wait();

      if (_lsb_flag)
      {
        Short2BytesLsb(
          _dst_ptr_arr[0],
          _dst_ptr_arr[0] + _lsb_offset_arr[0],
          _dst_pitch_arr[0],
          &_dst_int[0], _dst_int_pitch,
          _covered_width, _covered_height
        );
      }
      else if (_out16_flag)
      {
        Short2Bytes_Int32toWord16(
          (uint16_t *)_dst_ptr_arr[0], _dst_pitch_arr[0],
          &_dst_int[0], _dst_int_pitch,
          _covered_width, _covered_height,
          bits_per_pixel_output
        );
      }
      else if(pixelsize_super == 1)
      {
        Short2Bytes(
          _dst_ptr_arr[0], _dst_pitch_arr[0],
          &_dst_short[0], _dst_short_pitch,
          _covered_width, _covered_height
        );
      }
      else if (pixelsize_super == 2)
      {
        Short2Bytes_Int32toWord16(
          (uint16_t *)_dst_ptr_arr[0], _dst_pitch_arr[0],
          &_dst_int[0], _dst_int_pitch,
          _covered_width, _covered_height,
          bits_per_pixel_super
        );
      }
      else if (pixelsize_super == 4)
      {
        Short2Bytes_FloatInInt32ArrayToFloat(
          (float *)_dst_ptr_arr[0], _dst_pitch_arr[0],
          (float *)&_dst_int[0], _dst_int_pitch,
          _covered_width, _covered_height
        );
      }
      if (_covered_width < nWidth)
      {
        if (_out16_flag) {
          // copy 8 bit source to 16bit target
          plane_copy_8_to_16_c(_dst_ptr_arr[0] + (_covered_width << pixelsize_output_shift), _dst_pitch_arr[0],
            _src_ptr_arr[0] + _covered_width, _src_pitch_arr[0],
            nWidth - _covered_width, _covered_height
          );
        }
        else {
          BitBlt(
            _dst_ptr_arr[0] + (_covered_width << pixelsize_super_shift), _dst_pitch_arr[0],
            _src_ptr_arr[0] + (_covered_width << pixelsize_super_shift), _src_pitch_arr[0],
            (nWidth - _covered_width) << pixelsize_super_shift, _covered_height
          );
        }
      }
      if (_covered_height < nHeight) // bottom noncovered region
      {
        if (_out16_flag) {
          // copy 8 bit source to 16bit target
          plane_copy_8_to_16_c(_dst_ptr_arr[0] + _covered_height * _dst_pitch_arr[0], _dst_pitch_arr[0],
            _src_ptr_arr[0] + _covered_height * _src_pitch_arr[0], _src_pitch_arr[0],
            nWidth, nHeight - _covered_height
          );
        }
        else {
          BitBlt(
            _dst_ptr_arr[0] + _covered_height * _dst_pitch_arr[0], _dst_pitch_arr[0],
            _src_ptr_arr[0] + _covered_height * _src_pitch_arr[0], _src_pitch_arr[0],
            nWidth << pixelsize_super_shift, nHeight - _covered_height
          );
        }
      }
    }	// overlap - end

    if (_nlimit < 255)
    {
      // limit is 0-255 relative, for any bit depth
      float realLimit;
      if (pixelsize_output <= 2)
        realLimit = _nlimit * (1 << (bits_per_pixel_output - 8));
      else
        realLimit = _nlimit / 255.0f;
      LimitFunction(_dst_ptr_arr[0], _dst_pitch_arr[0],
        _src_ptr_arr[0], _src_pitch_arr[0],
        nWidth, nHeight,
        realLimit
      );
    }
  }

  //-------------------------------------------------------------------------
  // CHROMA planes

  process_chroma <1>(UPLANE & _nsupermodeyuv);
  process_chroma <2>(VPLANE & _nsupermodeyuv);

  //-------------------------------------------------------------------------

#ifndef _M_X64 
  _mm_empty(); // (we may use double-float somewhere) Fizick
#endif

  PROFILE_STOP(MOTION_PROFILE_COMPENSATION);

  if ((pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2 && !_planar_flag)
  {
    YUY2FromPlanes(
      pDstYUY2, nDstPitchYUY2, nWidth, nHeight * _height_lsb_or_out16_mul,
      _dst_ptr_arr[0], _dst_pitch_arr[0],
      _dst_ptr_arr[1], _dst_ptr_arr[2], _dst_pitch_arr[1], _cpuFlags);
  }

  return (dst);
}



// Fn...F1 B1...Bn
int MDegrainN::reorder_ref(int index) const
{
  assert(index >= 0);
  assert(index < _trad * 2);

  const int k = (index < _trad)
    ? (_trad - index) * 2 - 1
    : (index - _trad) * 2;

  return (k);
}



template <int P>
void	MDegrainN::process_chroma(int plane_mask)
{
  if ((_yuvplanes & plane_mask) == 0)
  {
    if (_out16_flag) {
      // copy 8 bit source to 16bit target
      plane_copy_8_to_16_c(_dst_ptr_arr[P], _dst_pitch_arr[P],
        _src_ptr_arr[P], _src_pitch_arr[P],
        nWidth >> nLogxRatioUV_super, nHeight >> nLogyRatioUV_super
      );
    }
    else {
      BitBlt(
        _dst_ptr_arr[P], _dst_pitch_arr[P],
        _src_ptr_arr[P], _src_pitch_arr[P],
        (nWidth >> nLogxRatioUV_super) << pixelsize_super_shift, nHeight >> nLogyRatioUV_super
      );
    }
  }

  else
  {
    Slicer slicer(_mt_flag);

    if (nOverlapX == 0 && nOverlapY == 0)
    {
      slicer.start(
        nBlkY,
        *this,
        &MDegrainN::process_chroma_normal_slice <P>
      );
      slicer.wait();
    }

    // Overlap
    else
    {
      uint16_t * pDstShort = (_dst_short.empty()) ? 0 : &_dst_short[0];
      int * pDstInt = (_dst_int.empty()) ? 0 : &_dst_int[0];

      if (_lsb_flag || pixelsize_output > 1)
      {
        MemZoneSet(
          reinterpret_cast <unsigned char *> (pDstInt), 0,
          (_covered_width * sizeof(int)) >> nLogxRatioUV_super, _covered_height >> nLogyRatioUV_super,
          0, 0, _dst_int_pitch * sizeof(int)
        );
      }
      else
      {  
        MemZoneSet(
          reinterpret_cast <unsigned char *> (pDstShort), 0,
          (_covered_width * sizeof(short)) >> nLogxRatioUV_super, _covered_height >> nLogyRatioUV_super,
          0, 0, _dst_short_pitch * sizeof(short)
        );
      }

      if (nOverlapY > 0)
      {
        memset(
          &_boundary_cnt_arr[0],
          0,
          _boundary_cnt_arr.size() * sizeof(_boundary_cnt_arr[0])
        );
      }

      slicer.start(
        nBlkY,
        *this,
        &MDegrainN::process_chroma_overlap_slice <P>,
        2
      );
      slicer.wait();
      
      if (_lsb_flag)
      {
        Short2BytesLsb(
          _dst_ptr_arr[P],
          _dst_ptr_arr[P] + _lsb_offset_arr[P], // 8 bit only
          _dst_pitch_arr[P],
          &_dst_int[0], _dst_int_pitch,
          _covered_width >> nLogxRatioUV_super, _covered_height >> nLogyRatioUV_super
        );
      }
      else if (_out16_flag)
      {
        Short2Bytes_Int32toWord16(
          (uint16_t *)_dst_ptr_arr[P], _dst_pitch_arr[P],
          &_dst_int[0], _dst_int_pitch,
          _covered_width >> nLogxRatioUV_super, _covered_height >> nLogyRatioUV_super,
          bits_per_pixel_output
        );
      }
      else if (pixelsize_super == 1)
      {
        Short2Bytes(
          _dst_ptr_arr[P], _dst_pitch_arr[P],
          &_dst_short[0], _dst_short_pitch,
          _covered_width >> nLogxRatioUV_super, _covered_height >> nLogyRatioUV_super
        );
      }
      else if (pixelsize_super == 2)
      {
        Short2Bytes_Int32toWord16(
          (uint16_t *)_dst_ptr_arr[P], _dst_pitch_arr[P],
          &_dst_int[0], _dst_int_pitch,
          _covered_width >> nLogxRatioUV_super, _covered_height >> nLogyRatioUV_super,
          bits_per_pixel_super
        );
      }
      else if (pixelsize_super == 4)
      {
        Short2Bytes_FloatInInt32ArrayToFloat(
          (float *)_dst_ptr_arr[P], _dst_pitch_arr[P],
          (float *)&_dst_int[0], _dst_int_pitch,
          _covered_width >> nLogxRatioUV_super, _covered_height >> nLogyRatioUV_super
        );
      }

      if (_covered_width < nWidth)
      {
        if (_out16_flag) {
          // copy 8 bit source to 16bit target
          plane_copy_8_to_16_c(_dst_ptr_arr[P] + ((_covered_width >> nLogxRatioUV_super) << pixelsize_output_shift), _dst_pitch_arr[P],
            _src_ptr_arr[P] + (_covered_width >> nLogxRatioUV_super), _src_pitch_arr[P],
            (nWidth - _covered_width) >> nLogxRatioUV_super, _covered_height >> nLogyRatioUV_super
          );
        }
        else {
          BitBlt(
            _dst_ptr_arr[P] + ((_covered_width >> nLogxRatioUV_super) << pixelsize_super_shift), _dst_pitch_arr[P],
            _src_ptr_arr[P] + ((_covered_width >> nLogxRatioUV_super) << pixelsize_super_shift), _src_pitch_arr[P],
            ((nWidth - _covered_width) >> nLogxRatioUV_super) << pixelsize_super_shift, _covered_height >> nLogyRatioUV_super
          );
        }
      }
      if (_covered_height < nHeight) // bottom noncovered region
      {
        if (_out16_flag) {
          // copy 8 bit source to 16bit target
          plane_copy_8_to_16_c(_dst_ptr_arr[P] + ((_dst_pitch_arr[P] * _covered_height) >> nLogyRatioUV_super), _dst_pitch_arr[P],
            _src_ptr_arr[P] + ((_src_pitch_arr[P] * _covered_height) >> nLogyRatioUV_super), _src_pitch_arr[P],
            nWidth >> nLogxRatioUV_super, ((nHeight - _covered_height) >> nLogyRatioUV_super)
          );
        }
        else {
          BitBlt(
            _dst_ptr_arr[P] + ((_dst_pitch_arr[P] * _covered_height) >> nLogyRatioUV_super), _dst_pitch_arr[P],
            _src_ptr_arr[P] + ((_src_pitch_arr[P] * _covered_height) >> nLogyRatioUV_super), _src_pitch_arr[P],
            (nWidth >> nLogxRatioUV_super) << pixelsize_super_shift, ((nHeight - _covered_height) >> nLogyRatioUV_super)
          );
        }
      }
    } // overlap - end

    if (_nlimitc < 255)
    {
      // limit is 0-255 relative, for any bit depth
      float realLimit;
      if (pixelsize_output <= 2)
        realLimit = _nlimitc * (1 << (bits_per_pixel_output - 8));
      else
        realLimit = (float)_nlimitc / 255.0f;
      LimitFunction(_dst_ptr_arr[P], _dst_pitch_arr[P],
        _src_ptr_arr[P], _src_pitch_arr[P],
        nWidth >> nLogxRatioUV_super, nHeight >> nLogyRatioUV_super,
        realLimit
      );
    }
  }
}



void	MDegrainN::process_luma_normal_slice(Slicer::TaskData &td)
{
  assert(&td != 0);

  const int rowsize = nBlkSizeY;
  BYTE *pDstCur = _dst_ptr_arr[0] + td._y_beg * rowsize * _dst_pitch_arr[0]; // P.F. why *rowsize? (*nBlkSizeY)
  const BYTE *pSrcCur = _src_ptr_arr[0] + td._y_beg * rowsize * _src_pitch_arr[0]; // P.F. why *rowsize? (*nBlkSizeY)

  for (int by = td._y_beg; by < td._y_end; ++by)
  {
    int xx = 0; // logical offset. Mul by 2 for pixelsize_super==2. Don't mul for indexing int* array
    for (int bx = 0; bx < nBlkX; ++bx)
    {
      int i = by * nBlkX + bx;
      const BYTE *ref_data_ptr_arr[MAX_TEMP_RAD * 2];
      int pitch_arr[MAX_TEMP_RAD * 2];
      int weight_arr[1 + MAX_TEMP_RAD * 2];

      for (int k = 0; k < _trad * 2; ++k)
      {
        use_block_y(
          ref_data_ptr_arr[k],
          pitch_arr[k],
          weight_arr[k + 1],
          _usable_flag_arr[k],
          _mv_clip_arr[k],
          i,
          _planes_ptr[k][0],
          pSrcCur,
          xx << pixelsize_super_shift,
          _src_pitch_arr[0]
        );
      }

      norm_weights(weight_arr, _trad);

      // luma
      _degrainluma_ptr(
        pDstCur + (xx << pixelsize_output_shift), pDstCur + _lsb_offset_arr[0] + (xx << pixelsize_super_shift), _dst_pitch_arr[0],
        pSrcCur + (xx << pixelsize_super_shift), _src_pitch_arr[0],
        ref_data_ptr_arr, pitch_arr, weight_arr, _trad
      );

      xx += (nBlkSizeX); // xx: indexing offset

      if (bx == nBlkX - 1 && _covered_width < nWidth) // right non-covered region
      {
        if (_out16_flag) {
          // copy 8 bit source to 16bit target
          plane_copy_8_to_16_c(
            pDstCur + (_covered_width << pixelsize_output_shift), _dst_pitch_arr[0],
            pSrcCur + (_covered_width << pixelsize_super_shift), _src_pitch_arr[0],
            nWidth - _covered_width, nBlkSizeY
          );
        }
        else {
          // luma
          BitBlt(
            pDstCur + (_covered_width << pixelsize_super_shift), _dst_pitch_arr[0],
            pSrcCur + (_covered_width << pixelsize_super_shift), _src_pitch_arr[0],
            (nWidth - _covered_width) << pixelsize_super_shift, nBlkSizeY);
        }
      }
    }	// for bx

    pDstCur += rowsize * _dst_pitch_arr[0];
    pSrcCur += rowsize * _src_pitch_arr[0];

    if (by == nBlkY - 1 && _covered_height < nHeight) // bottom uncovered region
    {
      // luma
      if (_out16_flag) {
        // copy 8 bit source to 16bit target
        plane_copy_8_to_16_c(
          pDstCur, _dst_pitch_arr[0],
          pSrcCur, _src_pitch_arr[0],
          nWidth, nHeight - _covered_height
        );
      }
      else {
        BitBlt(
          pDstCur, _dst_pitch_arr[0],
          pSrcCur, _src_pitch_arr[0],
          nWidth << pixelsize_super_shift, nHeight - _covered_height
        );
      }
    }
  }	// for by
}



void	MDegrainN::process_luma_overlap_slice(Slicer::TaskData &td)
{
  assert(&td != 0);

  if (nOverlapY == 0
    || (td._y_beg == 0 && td._y_end == nBlkY))
  {
    process_luma_overlap_slice(td._y_beg, td._y_end);
  }

  else
  {
    assert(td._y_end - td._y_beg >= 2);

    process_luma_overlap_slice(td._y_beg, td._y_end - 1);

    const conc::AioAdd <int>	inc_ftor(+1);

    const int cnt_top = conc::AtomicIntOp::exec_new(
      _boundary_cnt_arr[td._y_beg],
      inc_ftor
    );
    if (td._y_beg > 0 && cnt_top == 2)
    {
      process_luma_overlap_slice(td._y_beg - 1, td._y_beg);
    }

    int cnt_bot = 2;
    if (td._y_end < nBlkY)
    {
      cnt_bot = conc::AtomicIntOp::exec_new(
        _boundary_cnt_arr[td._y_end],
        inc_ftor
      );
    }
    if (cnt_bot == 2)
    {
      process_luma_overlap_slice(td._y_end - 1, td._y_end);
    }
  }
}



void	MDegrainN::process_luma_overlap_slice(int y_beg, int y_end)
{
  TmpBlock       tmp_block;

  const int      rowsize = nBlkSizeY - nOverlapY;
  const BYTE *   pSrcCur = _src_ptr_arr[0] + y_beg * rowsize * _src_pitch_arr[0];

  uint16_t * pDstShort = (_dst_short.empty()) ? 0 : &_dst_short[0] + y_beg * rowsize * _dst_short_pitch;
  int *pDstInt = (_dst_int.empty()) ? 0 : &_dst_int[0] + y_beg * rowsize * _dst_int_pitch;
  const int tmpPitch = nBlkSizeX;
  assert(tmpPitch <= TmpBlock::MAX_SIZE);

  for (int by = y_beg; by < y_end; ++by)
  {
    // indexing overlap windows weighting table: top=0 middle=3 bottom=6
    /*
    0 = Top Left    1 = Top Middle    2 = Top Right
    3 = Middle Left 4 = Middle Middle 5 = Middle Right
    6 = Bottom Left 7 = Bottom Middle 8 = Bottom Right
    */

    int wby = (by == 0) ? 0 * 3 : (by == nBlkY - 1) ? 2 * 3 : 1 * 3; // 0 for very first, 2*3 for very last, 1*3 for all others in the middle
    int xx = 0; // logical offset. Mul by 2 for pixelsize_super==2. Don't mul for indexing int* array
    for (int bx = 0; bx < nBlkX; ++bx)
    {
      // select window
      // indexing overlap windows weighting table: left=+0 middle=+1 rightmost=+2
      int wbx = (bx == 0) ? 0 : (bx == nBlkX - 1) ? 2 : 1; // 0 for very first, 2 for very last, 1 for all others in the middle
      short *winOver = _overwins->GetWindow(wby + wbx);

      int i = by * nBlkX + bx;
      const BYTE *ref_data_ptr_arr[MAX_TEMP_RAD * 2];
      int pitch_arr[MAX_TEMP_RAD * 2];
      int weight_arr[1 + MAX_TEMP_RAD * 2];

      for (int k = 0; k < _trad * 2; ++k)
      {
        use_block_y(
          ref_data_ptr_arr[k],
          pitch_arr[k],
          weight_arr[k + 1],
          _usable_flag_arr[k],
          _mv_clip_arr[k],
          i,
          _planes_ptr[k][0],
          pSrcCur,
          xx << pixelsize_super_shift,
          _src_pitch_arr[0]
        );
      }

      norm_weights(weight_arr, _trad);

      // luma
      _degrainluma_ptr(
        &tmp_block._d[0], tmp_block._lsb_ptr, tmpPitch << pixelsize_output_shift,
        pSrcCur + (xx << pixelsize_super_shift), _src_pitch_arr[0],
        ref_data_ptr_arr, pitch_arr, weight_arr, _trad
      );
      if (_lsb_flag)
      {
        _oversluma_lsb_ptr(
          pDstInt + xx, _dst_int_pitch,
          &tmp_block._d[0], tmp_block._lsb_ptr, tmpPitch,
          winOver, nBlkSizeX
        );
      }
      else if (_out16_flag)
      {
        // cast to match the prototype
        _oversluma16_ptr((uint16_t *)(pDstInt + xx), _dst_int_pitch, &tmp_block._d[0], tmpPitch << pixelsize_output_shift, winOver, nBlkSizeX);
      }
      else if (pixelsize_super == 1)
      {
        _oversluma_ptr(
          pDstShort + xx, _dst_short_pitch,
          &tmp_block._d[0], tmpPitch,
          winOver, nBlkSizeX
        );
      }
      else if (pixelsize_super == 2) {
        _oversluma16_ptr((uint16_t *)(pDstInt + xx), _dst_int_pitch, &tmp_block._d[0], tmpPitch << pixelsize_super_shift, winOver, nBlkSizeX);
      }
      else { // pixelsize_super == 4
        _oversluma32_ptr((uint16_t *)(pDstInt + xx), _dst_int_pitch, &tmp_block._d[0], tmpPitch << pixelsize_super_shift, winOver, nBlkSizeX);
      }

      xx += nBlkSizeX - nOverlapX;
    } // for bx

    pSrcCur += rowsize * _src_pitch_arr[0]; // byte pointer
    pDstShort += rowsize * _dst_short_pitch; // short pointer
    pDstInt += rowsize * _dst_int_pitch; // int pointer
  } // for by
}



template <int P>
void	MDegrainN::process_chroma_normal_slice(Slicer::TaskData &td)
{
  assert(&td != 0);
  const int rowsize = nBlkSizeY >> nLogyRatioUV_super; // bad name. it's height really
  BYTE *pDstCur = _dst_ptr_arr[P] + td._y_beg * rowsize * _dst_pitch_arr[P];
  const BYTE *pSrcCur = _src_ptr_arr[P] + td._y_beg * rowsize * _src_pitch_arr[P];

  int effective_nSrcPitch = (nBlkSizeY >> nLogyRatioUV_super) * _src_pitch_arr[P]; // pitch is byte granularity
  int effective_nDstPitch = (nBlkSizeY >> nLogyRatioUV_super) * _dst_pitch_arr[P]; // pitch is short granularity

  for (int by = td._y_beg; by < td._y_end; ++by)
  {
    int xx = 0; // index
    for (int bx = 0; bx < nBlkX; ++bx)
    {
      int i = by * nBlkX + bx;
      const BYTE *ref_data_ptr_arr[MAX_TEMP_RAD * 2]; // vs: const uint8_t *pointers[radius * 2]; // Moved by the degrain function. 
      int pitch_arr[MAX_TEMP_RAD * 2];
      int weight_arr[1 + MAX_TEMP_RAD * 2]; // 0th is special. vs:int WSrc, WRefs[radius * 2]; 

      for (int k = 0; k < _trad * 2; ++k)
      {
        use_block_uv(
          ref_data_ptr_arr[k],
          pitch_arr[k],
          weight_arr[k + 1],
          _usable_flag_arr[k],
          _mv_clip_arr[k],
          i,
          _planes_ptr[k][P],
          pSrcCur,
          xx << pixelsize_super_shift, // the pointer increment inside knows that xx later here is incremented with nBlkSize and not nBlkSize>>_xRatioUV
              // todo: copy from MDegrainX. Here we shift, and incement with nBlkSize>>_xRatioUV
          _src_pitch_arr[P]
        ); // vs: extra nLogPel, plane, xSubUV, ySubUV, thSAD
      }

      norm_weights(weight_arr, _trad); // normaliseWeights<radius>(WSrc, WRefs);


      // chroma
      _degrainchroma_ptr(
        pDstCur + (xx << pixelsize_output_shift),
        pDstCur + (xx << pixelsize_super_shift) + _lsb_offset_arr[P], _dst_pitch_arr[P],
        pSrcCur + (xx << pixelsize_super_shift), _src_pitch_arr[P],
        ref_data_ptr_arr, pitch_arr, weight_arr, _trad
      );

      //if (nLogxRatioUV != nLogxRatioUV_super) // orphaned if. chroma processing failed between 2.7.1-2.7.20
      //xx += nBlkSizeX; // blksize of Y plane, that's why there is xx >> xRatioUVlog above
      xx += (nBlkSizeX >> nLogxRatioUV_super); // xx: indexing offset

      if (bx == nBlkX - 1 && _covered_width < nWidth) // right non-covered region
      {
        // chroma
        if (_out16_flag) {
          // copy 8 bit source to 16bit target
          plane_copy_8_to_16_c(
            pDstCur + ((_covered_width >> nLogxRatioUV_super) << pixelsize_output_shift), _dst_pitch_arr[P],
            pSrcCur + ((_covered_width >> nLogxRatioUV_super) << pixelsize_super_shift), _src_pitch_arr[P],
            (nWidth - _covered_width) >> nLogxRatioUV_super/* real row_size */, rowsize /* bad name. it's height = nBlkSizeY >> nLogyRatioUV_super*/
          );
        }
        else {
          BitBlt(
            pDstCur + ((_covered_width >> nLogxRatioUV_super) << pixelsize_super_shift), _dst_pitch_arr[P],
            pSrcCur + ((_covered_width >> nLogxRatioUV_super) << pixelsize_super_shift), _src_pitch_arr[P],
            ((nWidth - _covered_width) >> nLogxRatioUV_super) << pixelsize_super_shift /* real row_size */, rowsize /* bad name. it's height = nBlkSizeY >> nLogyRatioUV_super*/
          );
        }
      }
    } // for bx

    pDstCur += effective_nDstPitch;
    pSrcCur += effective_nSrcPitch;

    if (by == nBlkY - 1 && _covered_height < nHeight) // bottom uncovered region
    {
      // chroma
      if (_out16_flag) {
        // copy 8 bit source to 16bit target
        plane_copy_8_to_16_c(
          pDstCur, _dst_pitch_arr[P],
          pSrcCur, _src_pitch_arr[P],
          nWidth >> nLogxRatioUV_super, (nHeight - _covered_height) >> nLogyRatioUV_super /* height */
        );
      }
      else {
        BitBlt(
          pDstCur, _dst_pitch_arr[P],
          pSrcCur, _src_pitch_arr[P],
          (nWidth >> nLogxRatioUV_super) << pixelsize_super_shift, (nHeight - _covered_height) >> nLogyRatioUV_super /* height */
        );
      }
    }
  } // for by
}



template <int P>
void	MDegrainN::process_chroma_overlap_slice(Slicer::TaskData &td)
{
  assert(&td != 0);

  if (nOverlapY == 0
    || (td._y_beg == 0 && td._y_end == nBlkY))
  {
    process_chroma_overlap_slice <P>(td._y_beg, td._y_end);
  }

  else
  {
    assert(td._y_end - td._y_beg >= 2);

    process_chroma_overlap_slice <P>(td._y_beg, td._y_end - 1);

    const conc::AioAdd <int> inc_ftor(+1);

    const int cnt_top = conc::AtomicIntOp::exec_new(
      _boundary_cnt_arr[td._y_beg],
      inc_ftor
    );
    if (td._y_beg > 0 && cnt_top == 2)
    {
      process_chroma_overlap_slice <P>(td._y_beg - 1, td._y_beg);
    }

    int				cnt_bot = 2;
    if (td._y_end < nBlkY)
    {
      cnt_bot = conc::AtomicIntOp::exec_new(
        _boundary_cnt_arr[td._y_end],
        inc_ftor
      );
    }
    if (cnt_bot == 2)
    {
      process_chroma_overlap_slice <P>(td._y_end - 1, td._y_end);
    }
  }
}



template <int P>
void	MDegrainN::process_chroma_overlap_slice(int y_beg, int y_end)
{
  TmpBlock       tmp_block;

  const int rowsize = (nBlkSizeY - nOverlapY) >> nLogyRatioUV_super; // bad name. it's height really
  const BYTE *pSrcCur = _src_ptr_arr[P] + y_beg * rowsize * _src_pitch_arr[P];

  uint16_t *pDstShort = (_dst_short.empty()) ? 0 : &_dst_short[0] + y_beg * rowsize * _dst_short_pitch;
  int *pDstInt = (_dst_int.empty()) ? 0 : &_dst_int[0] + y_beg * rowsize * _dst_int_pitch;
  const int tmpPitch = nBlkSizeX;
  assert(tmpPitch <= TmpBlock::MAX_SIZE);

  int effective_nSrcPitch = ((nBlkSizeY - nOverlapY) >> nLogyRatioUV_super) * _src_pitch_arr[P]; // pitch is byte granularity
  int effective_dstShortPitch = ((nBlkSizeY - nOverlapY) >> nLogyRatioUV_super) * _dst_short_pitch; // pitch is short granularity
  int effective_dstIntPitch = ((nBlkSizeY - nOverlapY) >> nLogyRatioUV_super) * _dst_int_pitch; // pitch is int granularity

  for (int by = y_beg; by < y_end; ++by)
  {
    // indexing overlap windows weighting table: top=0 middle=3 bottom=6
    /*
    0 = Top Left    1 = Top Middle    2 = Top Right
    3 = Middle Left 4 = Middle Middle 5 = Middle Right
    6 = Bottom Left 7 = Bottom Middle 8 = Bottom Right
    */

    int wby = (by == 0) ? 0 * 3 : (by == nBlkY - 1) ? 2 * 3 : 1 * 3; // 0 for very first, 2*3 for very last, 1*3 for all others in the middle
    int xx = 0; // logical offset. Mul by 2 for pixelsize_super==2. Don't mul for indexing int* array
    for (int bx = 0; bx < nBlkX; ++bx)
    {
      // select window
      // indexing overlap windows weighting table: left=+0 middle=+1 rightmost=+2
      int wbx = (bx == 0) ? 0 : (bx == nBlkX - 1) ? 2 : 1; // 0 for very first, 2 for very last, 1 for all others in the middle
      short *winOverUV = _overwins_uv->GetWindow(wby + wbx);

      int i = by * nBlkX + bx;
      const BYTE *ref_data_ptr_arr[MAX_TEMP_RAD * 2];
      int pitch_arr[MAX_TEMP_RAD * 2];
      int weight_arr[1 + MAX_TEMP_RAD * 2]; // 0th is special

      for (int k = 0; k < _trad * 2; ++k)
      {
        use_block_uv(
          ref_data_ptr_arr[k],
          pitch_arr[k],
          weight_arr[k + 1], // from 1st
          _usable_flag_arr[k],
          _mv_clip_arr[k],
          i,
          _planes_ptr[k][P],
          pSrcCur,
          xx << pixelsize_super_shift, //  the pointer increment inside knows that xx later here is incremented with nBlkSize and not nBlkSize>>_xRatioUV
          _src_pitch_arr[P]
        );
      }

      norm_weights(weight_arr, _trad); // 0th + 1..MAX_TEMP_RAD*2

      // chroma
      // here we don't pass pixelsize, because _degrainchroma_ptr points already to the uint16_t version
      // if the clip was 16 bit one
      _degrainchroma_ptr(
        &tmp_block._d[0], tmp_block._lsb_ptr, tmpPitch << pixelsize_output_shift,
        pSrcCur + (xx << pixelsize_super_shift), _src_pitch_arr[P],
        ref_data_ptr_arr, pitch_arr, weight_arr, _trad
      );
      if (_lsb_flag)
      {
        _overschroma_lsb_ptr(
          pDstInt + xx, _dst_int_pitch,
          &tmp_block._d[0], tmp_block._lsb_ptr, tmpPitch,
          winOverUV, nBlkSizeX >> nLogxRatioUV_super
        );
      }
      else if (_out16_flag)
      {
        // cast to match the prototype
        _overschroma16_ptr(
          (uint16_t*)(pDstInt + xx), _dst_int_pitch,
          &tmp_block._d[0], tmpPitch << pixelsize_output_shift,
          winOverUV, nBlkSizeX >> nLogxRatioUV_super);
      }
      else if (pixelsize_super == 1)
      {
        _overschroma_ptr(
          pDstShort + xx, _dst_short_pitch,
          &tmp_block._d[0], tmpPitch,
          winOverUV, nBlkSizeX >> nLogxRatioUV_super);
      } else if (pixelsize_super == 2)
      {
        _overschroma16_ptr(
          (uint16_t*)(pDstInt + xx), _dst_int_pitch, 
          &tmp_block._d[0], tmpPitch << pixelsize_super_shift, 
          winOverUV, nBlkSizeX >> nLogxRatioUV_super);
      }
      else // if (pixelsize_super == 4)
      {
        _overschroma32_ptr(
          (uint16_t*)(pDstInt + xx), _dst_int_pitch,
          &tmp_block._d[0], tmpPitch << pixelsize_super_shift,
          winOverUV, nBlkSizeX >> nLogxRatioUV_super);
      }

      xx += ((nBlkSizeX - nOverlapX) >> nLogxRatioUV_super); // no pixelsize here

    } // for bx

    pSrcCur += effective_nSrcPitch; // pitch is byte granularity
    pDstShort += effective_dstShortPitch; // pitch is short granularity
    pDstInt += effective_dstIntPitch; // pitch is int granularity
  } // for by
}



void	MDegrainN::use_block_y(
  const BYTE * &p, int &np, int &wref, bool usable_flag, const MvClipInfo &c_info,
  int i, const MVPlane *plane_ptr, const BYTE *src_ptr, int xx, int src_pitch
)
{
  if (usable_flag)
  {
    const FakeBlockData &	block = c_info._clip_sptr->GetBlock(0, i);
    const int blx = block.GetX() * nPel + block.GetMV().x;
    const int bly = block.GetY() * nPel + block.GetMV().y;
    p = plane_ptr->GetPointer(blx, bly);
    np = plane_ptr->GetPitch();
    const sad_t block_sad = block.GetSAD(); // SAD of MV Block. Scaled to MVClip's bits_per_pixel;
    wref = DegrainWeight(c_info._thsad, c_info._thsad_sq, block_sad);
  }
  else
  {
    p = src_ptr + xx;
    np = src_pitch;
    wref = 0;
  }
}



void	MDegrainN::use_block_uv(
  const BYTE * &p, int &np, int &wref, bool usable_flag, const MvClipInfo &c_info,
  int i, const MVPlane *plane_ptr, const BYTE *src_ptr, int xx, int src_pitch
)
{
  if (usable_flag)
  {
    const FakeBlockData &block = c_info._clip_sptr->GetBlock(0, i);
    const int blx = block.GetX() * nPel + block.GetMV().x;
    const int bly = block.GetY() * nPel + block.GetMV().y;
    p = plane_ptr->GetPointer(blx >> nLogxRatioUV_super, bly >> nLogyRatioUV_super);
    np = plane_ptr->GetPitch();
    const sad_t block_sad = block.GetSAD(); // SAD of MV Block. Scaled to MVClip's bits_per_pixel;
    wref = DegrainWeight(c_info._thsadc, c_info._thsadc_sq, block_sad);
  }
  else
  {
    // just to have a valid data pointer, will not count, weight is zero
    p = src_ptr + xx; // done: kill  >> nLogxRatioUV_super from here and put it in the caller like in MDegrainX
    np = src_pitch;
    wref = 0;
  }
}



void MDegrainN::norm_weights(int wref_arr[], int trad)
{
  const int nbr_frames = trad * 2 + 1;

  const int one = 1 << DEGRAIN_WEIGHT_BITS; // 8 bit, 256

  wref_arr[0] = one;
  int wsum = 1;
  for (int k = 0; k < nbr_frames; ++k)
  {
    wsum += wref_arr[k];
  }

  // normalize weights to 256
  int wsrc = one;
  for (int k = 1; k < nbr_frames; ++k)
  {
    const int norm = wref_arr[k] * one / wsum;
    wref_arr[k] = norm;
    wsrc -= norm;
  }
  wref_arr[0] = wsrc;
}

