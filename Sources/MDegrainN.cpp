

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


template <typename pixel_t, int blockWidth, int blockHeight>
void DegrainN_C(
  BYTE *pDst, BYTE *pDstLsb, bool lsb_flag, int nDstPitch,
  const BYTE *pSrc, int nSrcPitch,
  const BYTE *pRef[], int Pitch[],
  int Wall[], int trad
)
{
  if (lsb_flag)
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

        pDst[x] = val >> 8;
        pDstLsb[x] = val & 255;
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
    // Wall: 8 bit. rounding: 128
    for (int h = 0; h < blockHeight; ++h)
    {
      for (int x = 0; x < blockWidth; ++x)
      {
        int val = reinterpret_cast<const pixel_t *>(pSrc)[x] * Wall[0] + 128;
        for (int k = 0; k < trad; ++k)
        {
          val += reinterpret_cast<const pixel_t *>(pRef[k * 2])[x] * Wall[k * 2 + 1]
            + reinterpret_cast<const pixel_t *>(pRef[k * 2 + 1])[x] * Wall[k * 2 + 2];
        }
        reinterpret_cast<pixel_t *>(pDst)[x] = val >> 8;
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

template <int blockWidth, int blockHeight>
void DegrainN_sse2(
  BYTE *pDst, BYTE *pDstLsb, bool lsb_flag, int nDstPitch,
  const BYTE *pSrc, int nSrcPitch,
  const BYTE *pRef[], int Pitch[],
  int Wall[], int trad
)
{
  const __m128i	z = _mm_setzero_si128();

  if (lsb_flag)
  {
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
        if (blockWidth >= 8) {
          _mm_storel_epi64(
            (__m128i*)(pDst + x),
            _mm_packus_epi16(_mm_srli_epi16(val, 8), z)
          );
          _mm_storel_epi64(
            (__m128i*)(pDstLsb + x),
            _mm_packus_epi16(_mm_and_si128(val, m), z)
          );
        }
        else {
            *(uint32_t*)(pDst + x) = _mm_cvtsi128_si32(_mm_packus_epi16(_mm_srli_epi16(val, 8), z));
            *(uint32_t*)(pDstLsb + x) = _mm_cvtsi128_si32(_mm_packus_epi16(_mm_and_si128(val, m), z));
        }
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
    const __m128i	o = _mm_set1_epi16(128);

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
        if (blockWidth >= 8) {
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

MDegrainN::DenoiseNFunction* MDegrainN::get_denoiseN_function(int BlockX, int BlockY, int _pixelsize, arch_t arch)
{
    // 8 bit only (pixelsize==1)
    //---------- DENOISE/DEGRAIN
    // BlkSizeX, BlkSizeY, pixelsize, arch_t
  std::map<std::tuple<int, int, int, arch_t>, DenoiseNFunction*> func_degrain;
  using std::make_tuple;

  // 8 bit
  func_degrain[make_tuple(64, 64, 1, NO_SIMD)] = DegrainN_C<uint8_t, 64, 64>;
  func_degrain[make_tuple(64, 32, 1, NO_SIMD)] = DegrainN_C<uint8_t, 64, 32>;
  func_degrain[make_tuple(64, 16, 1, NO_SIMD)] = DegrainN_C<uint8_t, 64, 16>;
  func_degrain[make_tuple(32, 64, 1, NO_SIMD)] = DegrainN_C<uint8_t, 32, 64>;
  func_degrain[make_tuple(32, 32, 1, NO_SIMD)] = DegrainN_C<uint8_t, 32, 32>;
  func_degrain[make_tuple(32, 16, 1, NO_SIMD)] = DegrainN_C<uint8_t, 32, 16>;
  func_degrain[make_tuple(32, 8, 1, NO_SIMD)] = DegrainN_C<uint8_t, 32, 8>;
  func_degrain[make_tuple(16, 32, 1, NO_SIMD)] = DegrainN_C<uint8_t, 16, 32>;
  func_degrain[make_tuple(16, 16, 1, NO_SIMD)] = DegrainN_C<uint8_t, 16, 16>;
  func_degrain[make_tuple(16, 8, 1, NO_SIMD)] = DegrainN_C<uint8_t, 16, 8>;
  func_degrain[make_tuple(16, 4, 1, NO_SIMD)] = DegrainN_C<uint8_t, 16, 4>;
  func_degrain[make_tuple(16, 2, 1, NO_SIMD)] = DegrainN_C<uint8_t, 16, 2>;
  func_degrain[make_tuple(8, 16, 1, NO_SIMD)] = DegrainN_C<uint8_t, 8, 16>;
  func_degrain[make_tuple(8, 8, 1, NO_SIMD)] = DegrainN_C<uint8_t, 8, 8>;
  func_degrain[make_tuple(8, 4, 1, NO_SIMD)] = DegrainN_C<uint8_t, 8, 4>;
  func_degrain[make_tuple(8, 2, 1, NO_SIMD)] = DegrainN_C<uint8_t, 8, 2>;
  func_degrain[make_tuple(8, 1, 1, NO_SIMD)] = DegrainN_C<uint8_t, 8, 1>;
  func_degrain[make_tuple(4, 8, 1, NO_SIMD)] = DegrainN_C<uint8_t, 4, 8>;
  func_degrain[make_tuple(4, 4, 1, NO_SIMD)] = DegrainN_C<uint8_t, 4, 4>;
  func_degrain[make_tuple(4, 2, 1, NO_SIMD)] = DegrainN_C<uint8_t, 4, 2>;
  func_degrain[make_tuple(2, 4, 1, NO_SIMD)] = DegrainN_C<uint8_t, 2, 4>;
  func_degrain[make_tuple(2, 2, 1, NO_SIMD)] = DegrainN_C<uint8_t, 2, 2>;
  // 10-16 bit
  func_degrain[make_tuple(64, 64, 2, NO_SIMD)] = DegrainN_C<uint16_t, 64, 64>;
  func_degrain[make_tuple(64, 32, 2, NO_SIMD)] = DegrainN_C<uint16_t, 64, 32>;
  func_degrain[make_tuple(64, 16, 2, NO_SIMD)] = DegrainN_C<uint16_t, 64, 16>;
  func_degrain[make_tuple(32, 64, 2, NO_SIMD)] = DegrainN_C<uint16_t, 32, 64>;
  func_degrain[make_tuple(32, 32, 2, NO_SIMD)] = DegrainN_C<uint16_t, 32, 32>;
  func_degrain[make_tuple(32, 16, 2, NO_SIMD)] = DegrainN_C<uint16_t, 32, 16>;
  func_degrain[make_tuple(32, 8, 2, NO_SIMD)] = DegrainN_C<uint16_t, 32, 8>;
  func_degrain[make_tuple(16, 32, 2, NO_SIMD)] = DegrainN_C<uint16_t, 16, 32>;
  func_degrain[make_tuple(16, 16, 2, NO_SIMD)] = DegrainN_C<uint16_t, 16, 16>;
  func_degrain[make_tuple(16, 8, 2, NO_SIMD)] = DegrainN_C<uint16_t, 16, 8>;
  func_degrain[make_tuple(16, 4, 2, NO_SIMD)] = DegrainN_C<uint16_t, 16, 4>;
  func_degrain[make_tuple(16, 2, 2, NO_SIMD)] = DegrainN_C<uint16_t, 16, 2>;
  func_degrain[make_tuple(8, 16, 2, NO_SIMD)] = DegrainN_C<uint16_t, 8, 16>;
  func_degrain[make_tuple(8, 8, 2, NO_SIMD)] = DegrainN_C<uint16_t, 8, 8>;
  func_degrain[make_tuple(8, 4, 2, NO_SIMD)] = DegrainN_C<uint16_t, 8, 4>;
  func_degrain[make_tuple(8, 2, 2, NO_SIMD)] = DegrainN_C<uint16_t, 8, 2>;
  func_degrain[make_tuple(8, 1, 2, NO_SIMD)] = DegrainN_C<uint16_t, 8, 1>;
  func_degrain[make_tuple(4, 8, 2, NO_SIMD)] = DegrainN_C<uint16_t, 4, 8>;
  func_degrain[make_tuple(4, 4, 2, NO_SIMD)] = DegrainN_C<uint16_t, 4, 4>;
  func_degrain[make_tuple(4, 2, 2, NO_SIMD)] = DegrainN_C<uint16_t, 4, 2>;
  func_degrain[make_tuple(2, 4, 2, NO_SIMD)] = DegrainN_C<uint16_t, 2, 4>;
  func_degrain[make_tuple(2, 2, 2, NO_SIMD)] = DegrainN_C<uint16_t, 2, 2>;

  func_degrain[make_tuple(64, 64, 1, USE_SSE2)] = DegrainN_sse2<64, 64>;
  func_degrain[make_tuple(64, 32, 1, USE_SSE2)] = DegrainN_sse2<64, 32>;
  func_degrain[make_tuple(64, 16, 1, USE_SSE2)] = DegrainN_sse2<64, 16>;
  func_degrain[make_tuple(32, 64, 1, USE_SSE2)] = DegrainN_sse2<32, 64>;
  func_degrain[make_tuple(32, 32, 1, USE_SSE2)] = DegrainN_sse2<32, 32>;
  func_degrain[make_tuple(32, 16, 1, USE_SSE2)] = DegrainN_sse2<32, 16>;
  func_degrain[make_tuple(32, 8, 1, USE_SSE2)] = DegrainN_sse2<32, 8>;
  func_degrain[make_tuple(16, 32, 1, USE_SSE2)] = DegrainN_sse2<16, 32>;
  func_degrain[make_tuple(16, 16, 1, USE_SSE2)] = DegrainN_sse2<16, 16>;
  func_degrain[make_tuple(16, 8, 1, USE_SSE2)] = DegrainN_sse2<16, 8>;
  func_degrain[make_tuple(16, 4, 1, USE_SSE2)] = DegrainN_sse2<16, 4>;
  func_degrain[make_tuple(16, 2, 1, USE_SSE2)] = DegrainN_sse2<16, 2>;
  func_degrain[make_tuple(8, 16, 1, USE_SSE2)] = DegrainN_sse2<8, 16>;
  func_degrain[make_tuple(8, 8, 1, USE_SSE2)] = DegrainN_sse2<8, 8>;
  func_degrain[make_tuple(8, 4, 1, USE_SSE2)] = DegrainN_sse2<8, 4>;
  func_degrain[make_tuple(8, 2, 1, USE_SSE2)] = DegrainN_sse2<8, 2>;
  func_degrain[make_tuple(8, 1, 1, USE_SSE2)] = DegrainN_sse2<8, 1>;
  func_degrain[make_tuple(4, 8, 1, USE_SSE2)] = DegrainN_sse2<4, 8>;
  func_degrain[make_tuple(4, 4, 1, USE_SSE2)] = DegrainN_sse2<4, 4>;
  func_degrain[make_tuple(4, 2, 1, USE_SSE2)] = DegrainN_sse2<4, 2>;
  //func_degrain[make_tuple(2, 4, 1, USE_SSE2)] = DegrainN_sse2<2, 4>;  // no 2 byte width, only C
  //func_degrain[make_tuple(2, 2, 1, USE_SSE2)] = DegrainN_sse2<2, 2>;

  DenoiseNFunction* result = nullptr;
  arch_t archlist[] = { USE_AVX2, USE_AVX, USE_SSE41, USE_SSE2, NO_SIMD };
  int index = 0;
  while (result == nullptr) {
    arch_t current_arch_try = archlist[index++];
    if (current_arch_try > arch) continue;
    result = func_degrain[make_tuple(BlockX, BlockY, pixelsize, current_arch_try)];
    if (result == nullptr && current_arch_try == NO_SIMD)
      break;
  }

#if 0
  DenoiseNFunction* result = func_degrain[make_tuple(BlockX, BlockY, _pixelsize, arch)];
  if(!result) // fallback to C
    result = func_degrain[make_tuple(BlockX, BlockY, _pixelsize, NO_SIMD)];
#endif
  return result;
}



MDegrainN::MDegrainN(
  PClip child, PClip super, PClip mvmulti, int trad,
  sad_t thsad, sad_t thsadc, int yuvplanes, sad_t nlimit, sad_t nlimitc,
  sad_t nscd1, int nscd2, bool isse_flag, bool planar_flag, bool lsb_flag,
  sad_t thsad2, sad_t thsadc2, bool mt_flag, IScriptEnvironment* env_ptr
)
  : GenericVideoFilter(child)
  , MVFilter(mvmulti, "MDegrainN", env_ptr, 1, 0)
  , _mv_clip_arr()
  , _trad(trad)
  , _yuvplanes(yuvplanes)
  , _nlimit(nlimit)
  , _nlimitc(nlimitc)
  , _super(super)
  , _isse_flag(isse_flag)
  , _planar_flag(planar_flag)
  , _lsb_flag(lsb_flag)
  , _mt_flag(mt_flag)
  , _height_lsb_mul((lsb_flag) ? 2 : 1)
  , _xratiouv_log((xRatioUV == 2) ? 1 : 0)
  , _yratiouv_log((yRatioUV == 2) ? 1 : 0)
  , _nsupermodeyuv(-1)
  , _dst_planes(nullptr)
  , _src_planes(nullptr)
  , _overwins()
  , _overwins_uv()
  , _oversluma_ptr(0)
  , _overschroma_ptr(0)
  , _oversluma16_ptr(0)
  , _overschroma16_ptr(0)
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

  pixelsize_super = vi_super.ComponentSize(); // of MVFilter
  bits_per_pixel_super = vi_super.BitsPerComponent();

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
      _isse_flag,
      xRatioUV,
      yRatioUV,
      pixelsize_super, // todo check! or pixelsize
      bits_per_pixel_super,
      mt_flag
    ));

    // Computes the SAD thresholds for this source frame, a cosine-shaped
    // smooth transition between thsad(c) and thsad(c)2.
    const int		d = k / 2 + 1;
    c_info._thsad = ClipFnc::interpolate_thsad(thsad, thsad2, d, _trad);
    c_info._thsadc = ClipFnc::interpolate_thsad(thsadc, thsadc2, d, _trad);
  }

  const int nSuperWidth = vi_super.width;
  const int nSuperHeight = vi_super.height;

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

  if (bits_per_pixel_super != bits_per_pixel) {
    env_ptr->ThrowError("MDegrainN : clip and super clip have different bit depths");
  }

  if ((pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2 && !_planar_flag)
  {
    _dst_planes = std::unique_ptr <YUY2Planes>(
      new YUY2Planes(nWidth, nHeight * _height_lsb_mul)
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
      nBlkSizeX >> _xratiouv_log, nBlkSizeY >> _yratiouv_log, // PF 2->xratiouv
      nOverlapX >> _xratiouv_log, nOverlapY >> _yratiouv_log
    ));
    if (_lsb_flag || pixelsize_super > 1) // PF or pixelsize?
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
  int avs_cpu_flag = env_ptr->GetCPUFlags();
  if ((((avs_cpu_flag & CPUF_AVX2) != 0) & isse_flag))
    arch = USE_AVX2;
  else if ((((avs_cpu_flag & CPUF_AVX) != 0) & isse_flag))
    arch = USE_AVX;
  else if ((((avs_cpu_flag & CPUF_SSE4_1) != 0) & isse_flag))
    arch = USE_SSE41;
  else if ((((avs_cpu_flag & CPUF_SSE2) != 0) & isse_flag))
    arch = USE_SSE2;
  else
    arch = NO_SIMD;

// C only -> NO_SIMD
  _oversluma_lsb_ptr = get_overlaps_lsb_function(nBlkSizeX, nBlkSizeY, sizeof(uint8_t), NO_SIMD);
  _overschroma_lsb_ptr = get_overlaps_lsb_function(nBlkSizeX / xRatioUV, nBlkSizeY / yRatioUV, sizeof(uint8_t), NO_SIMD);

  _oversluma_ptr = get_overlaps_function(nBlkSizeX, nBlkSizeY, pixelsize_super, arch);
  _overschroma_ptr = get_overlaps_function(nBlkSizeX / xRatioUV, nBlkSizeY / yRatioUV, pixelsize_super, arch);

  _oversluma16_ptr = get_overlaps_function(nBlkSizeX, nBlkSizeY, sizeof(uint16_t), arch);
  _overschroma16_ptr = get_overlaps_function(nBlkSizeX >> nLogxRatioUV, nBlkSizeY >> nLogyRatioUV, sizeof(uint16_t), arch);

  _degrainluma_ptr = get_denoiseN_function(nBlkSizeX, nBlkSizeY, pixelsize_super, arch);
  _degrainchroma_ptr = get_denoiseN_function(nBlkSizeX / xRatioUV, nBlkSizeY / yRatioUV, pixelsize_super, arch);

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

  //---------- end of functions

  // 16 bit output hack
  if (_lsb_flag)
  {
    vi.height <<= 1;
  }
}



MDegrainN::~MDegrainN()
{
  // Nothing
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

  ::PVideoFrame src = child->GetFrame(n, env_ptr);
  ::PVideoFrame dst = env_ptr->NewVideoFrame(vi);
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
        _isse_flag
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
  _lsb_offset_arr[1] = _dst_pitch_arr[1] * (nHeight >> _yratiouv_log);
  _lsb_offset_arr[2] = _dst_pitch_arr[2] * (nHeight >> _yratiouv_log);

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
    BitBlt(
      _dst_ptr_arr[0], _dst_pitch_arr[0],
      _src_ptr_arr[0], _src_pitch_arr[0],
      nWidth*pixelsize_super, nHeight, _isse_flag
    );
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
      unsigned short *pDstShort = (_dst_short.empty()) ? 0 : &_dst_short[0];
      int *pDstInt = (_dst_int.empty()) ? 0 : &_dst_int[0];

      if (_lsb_flag || pixelsize_super>1)
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
          &_dst_int[0], _dst_int_pitch,
          _covered_width, _covered_height
        );
      }
      if (_covered_width < nWidth)
      {
        BitBlt(
          _dst_ptr_arr[0] + _covered_width*pixelsize_super, _dst_pitch_arr[0],
          _src_ptr_arr[0] + _covered_width*pixelsize_super, _src_pitch_arr[0],
          (nWidth - _covered_width)*pixelsize_super, _covered_height, _isse_flag
        );
      }
      if (_covered_height < nHeight) // bottom noncovered region
      {
        BitBlt(
          _dst_ptr_arr[0] + _covered_height * _dst_pitch_arr[0], _dst_pitch_arr[0],
          _src_ptr_arr[0] + _covered_height * _src_pitch_arr[0], _src_pitch_arr[0],
          nWidth*pixelsize_super, nHeight - _covered_height, _isse_flag
        );
      }
    }	// overlap - end

    if (pixelsize_super <= 2)
    {
      if (_nlimit < (1 << bits_per_pixel_super) - 1)
      {
        if (_isse_flag)
        {
          if (pixelsize_super == 1)
            LimitChanges_sse2_new<uint8_t>(
              _dst_ptr_arr[0], _dst_pitch_arr[0],
              _src_ptr_arr[0], _src_pitch_arr[0],
              nWidth, nHeight,
              _nlimit
              );
          else // pixelsize_super == 2
            LimitChanges_sse2_new<uint16_t>(
              _dst_ptr_arr[0], _dst_pitch_arr[0],
              _src_ptr_arr[0], _src_pitch_arr[0],
              nWidth, nHeight,
              _nlimit
              );
        }
        else
        {
          if (pixelsize_super == 1)
            LimitChanges_c<uint8_t>(
              _dst_ptr_arr[0], _dst_pitch_arr[0],
              _src_ptr_arr[0], _src_pitch_arr[0],
              nWidth, nHeight, _nlimit
              );
          else
            LimitChanges_c<uint16_t>(
              _dst_ptr_arr[0], _dst_pitch_arr[0],
              _src_ptr_arr[0], _src_pitch_arr[0],
              nWidth, nHeight, _nlimit
              );
        }
      }
    }
    else {
      /*
      LimitChanges_float_c(
        _dst_ptr_arr[0], _dst_pitch_arr[0],
        _src_ptr_arr[0], _src_pitch_arr[0],
        nWidth, nHeight,
        _nlimit_f
      );
      */
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
      pDstYUY2, nDstPitchYUY2, nWidth, nHeight * _height_lsb_mul,
      _dst_ptr_arr[0], _dst_pitch_arr[0],
      _dst_ptr_arr[1], _dst_ptr_arr[2], _dst_pitch_arr[1], _isse_flag);
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
    BitBlt(
      _dst_ptr_arr[P], _dst_pitch_arr[P],
      _src_ptr_arr[P], _src_pitch_arr[P],
      pixelsize_super * (nWidth >> _xratiouv_log), nHeight >> _yratiouv_log, _isse_flag
    );
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
      unsigned short * pDstShort = (_dst_short.empty()) ? 0 : &_dst_short[0];
      int * pDstInt = (_dst_int.empty()) ? 0 : &_dst_int[0];

      if (_lsb_flag || pixelsize_super > 1)
      {
        MemZoneSet(
          reinterpret_cast <unsigned char *> (pDstInt), 0,
          (_covered_width * sizeof(int)) >> _xratiouv_log, _covered_height >> _yratiouv_log,
          0, 0, _dst_int_pitch * sizeof(int)
        );
      }
      else
      {  
        MemZoneSet(
          reinterpret_cast <unsigned char *> (pDstShort), 0,
          (_covered_width * sizeof(short)) >> _xratiouv_log, _covered_height >> _yratiouv_log,
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
          _covered_width >> _xratiouv_log, _covered_height >> _yratiouv_log
        );
      }
      else if (pixelsize_super == 1)
      {
        Short2Bytes(
          _dst_ptr_arr[P], _dst_pitch_arr[P],
          &_dst_short[0], _dst_short_pitch,
          _covered_width >> _xratiouv_log, _covered_height >> _yratiouv_log
        );
      }
      else if (pixelsize_super == 2)
      {
        Short2Bytes_Int32toWord16(
          (uint16_t *)_dst_ptr_arr[P], _dst_pitch_arr[P],
          &_dst_int[0], _dst_int_pitch,
          _covered_width >> _xratiouv_log, _covered_height >> _yratiouv_log,
          bits_per_pixel_super
        );
      }
      else if (pixelsize_super == 4)
      {
        Short2Bytes_FloatInInt32ArrayToFloat(
          (float *)_dst_ptr_arr[P], _dst_pitch_arr[P],
          &_dst_int[0], _dst_int_pitch,
          _covered_width >> _xratiouv_log, _covered_height >> _yratiouv_log
        );
      }

      if (_covered_width < nWidth)
      {
        BitBlt(
          _dst_ptr_arr[P] + (_covered_width >> _xratiouv_log) * pixelsize_super, _dst_pitch_arr[P],
          _src_ptr_arr[P] + (_covered_width >> _xratiouv_log) * pixelsize_super, _src_pitch_arr[P],
          ((nWidth - _covered_width) >> _xratiouv_log) * pixelsize_super, _covered_height >> _yratiouv_log,
          _isse_flag
        );
      }
      if (_covered_height < nHeight) // bottom noncovered region
      {
        BitBlt(
          _dst_ptr_arr[P] + ((_dst_pitch_arr[P] * _covered_height) >> _yratiouv_log), _dst_pitch_arr[P],
          _src_ptr_arr[P] + ((_src_pitch_arr[P] * _covered_height) >> _yratiouv_log), _src_pitch_arr[P],
          (nWidth >> _xratiouv_log) * pixelsize_super, ((nHeight - _covered_height) >> _yratiouv_log),
          _isse_flag
        );
      }
    } // overlap - end

    if (pixelsize_super <= 2) {
      if (_nlimitc < (1 << bits_per_pixel_super) - 1)
      {
        if (_isse_flag)
        {
          if (pixelsize_super == 1)
            LimitChanges_sse2_new<uint8_t>(
              _dst_ptr_arr[P], _dst_pitch_arr[P],
              _src_ptr_arr[P], _src_pitch_arr[P],
              nWidth >> _xratiouv_log, nHeight >> _yratiouv_log,
              _nlimitc
            );
          else // pixelsize_super == 2
            LimitChanges_sse2_new<uint16_t>(
              _dst_ptr_arr[P], _dst_pitch_arr[P],
              _src_ptr_arr[P], _src_pitch_arr[P],
              nWidth >> _xratiouv_log, nHeight >> _yratiouv_log,
              _nlimitc
              );
        }
        else
        {
          if (pixelsize_super == 1)
            LimitChanges_c<uint8_t>(
              _dst_ptr_arr[P], _dst_pitch_arr[P],
              _src_ptr_arr[P], _src_pitch_arr[P],
              nWidth >> _xratiouv_log, nHeight >> _yratiouv_log,
              _nlimitc
              );
          else
            LimitChanges_c<uint16_t>(
              _dst_ptr_arr[P], _dst_pitch_arr[P],
              _src_ptr_arr[P], _src_pitch_arr[P],
              nWidth >> _xratiouv_log, nHeight >> _yratiouv_log,
              _nlimitc
              );
        }
      }
    }
    else {
      /*
      LimitChanges_float_c(
        _dst_ptr_arr[P], _dst_pitch_arr[P],
        _src_ptr_arr[P], _src_pitch_arr[P],
        nWidth >> _xratiouv_log, nHeight >> _yratiouv_log,
        _nlimitc_f
        );
        */
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
          xx * pixelsize_super,
          _src_pitch_arr[0]
        );
      }

      norm_weights(weight_arr, _trad);

      // luma
      _degrainluma_ptr(
        pDstCur + xx*pixelsize_super, pDstCur + _lsb_offset_arr[0] + xx*pixelsize_super, _lsb_flag, _dst_pitch_arr[0],
        pSrcCur + xx*pixelsize_super, _src_pitch_arr[0],
        ref_data_ptr_arr, pitch_arr, weight_arr, _trad
      );

      xx += (nBlkSizeX); // xx: indexing offset

      if (bx == nBlkX - 1 && _covered_width < nWidth) // right non-covered region
      {
        // luma
        BitBlt(
          pDstCur + _covered_width * pixelsize_super, _dst_pitch_arr[0],
          pSrcCur + _covered_width * pixelsize_super, _src_pitch_arr[0],
          (nWidth - _covered_width) * pixelsize_super, nBlkSizeY, _isse_flag);
      }
    }	// for bx

    pDstCur += rowsize * _dst_pitch_arr[0];
    pSrcCur += rowsize * _src_pitch_arr[0];

    if (by == nBlkY - 1 && _covered_height < nHeight) // bottom uncovered region
    {
      // luma
      BitBlt(
        pDstCur, _dst_pitch_arr[0],
        pSrcCur, _src_pitch_arr[0],
        nWidth*pixelsize_super, nHeight - _covered_height, _isse_flag
      );
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

  unsigned short * pDstShort = (_dst_short.empty()) ? 0 : &_dst_short[0] + y_beg * rowsize * _dst_short_pitch;
  int *pDstInt = (_dst_int.empty()) ? 0 : &_dst_int[0] + y_beg * rowsize * _dst_int_pitch;
  const int tmpPitch = nBlkSizeX;
  assert(tmpPitch <= TmpBlock::MAX_SIZE);

  for (int by = y_beg; by < y_end; ++by)
  {
    int wby = ((by + nBlkY - 3) / (nBlkY - 2)) * 3;
    int xx = 0; // logical offset. Mul by 2 for pixelsize_super==2. Don't mul for indexing int* array
    for (int bx = 0; bx < nBlkX; ++bx)
    {
      // select window
      int wbx = (bx + nBlkX - 3) / (nBlkX - 2);
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
          xx*pixelsize_super,
          _src_pitch_arr[0]
        );
      }

      norm_weights(weight_arr, _trad);

      // luma
      _degrainluma_ptr(
        &tmp_block._d[0], tmp_block._lsb_ptr, _lsb_flag, tmpPitch*pixelsize_super,
        pSrcCur + xx*pixelsize_super, _src_pitch_arr[0],
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
      else if (pixelsize_super == 1)
      {
        _oversluma_ptr(
          pDstShort + xx, _dst_short_pitch,
          &tmp_block._d[0], tmpPitch,
          winOver, nBlkSizeX
        );
      }
      else if (pixelsize_super == 2) {
        _oversluma16_ptr((uint16_t *)(pDstInt + xx), _dst_int_pitch, &tmp_block._d[0], tmpPitch*pixelsize_super, winOver, nBlkSizeX);
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
  const int rowsize = nBlkSizeY >> _yratiouv_log; // bad name. it's height really
  BYTE *pDstCur = _dst_ptr_arr[P] + td._y_beg * rowsize * _dst_pitch_arr[P];
  const BYTE *pSrcCur = _src_ptr_arr[P] + td._y_beg * rowsize * _src_pitch_arr[P];

  int effective_nSrcPitch = (nBlkSizeY >> _yratiouv_log) * _src_pitch_arr[P]; // pitch is byte granularity
  int effective_nDstPitch = (nBlkSizeY >> _yratiouv_log) * _dst_pitch_arr[P]; // pitch is short granularity

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
          xx*pixelsize_super, // the pointer increment inside knows that xx later here is incremented with nBlkSize and not nBlkSize>>_xRatioUV
              // todo: copy from MDegrainX. Here we shift, and incement with nBlkSize>>_xRatioUV
          _src_pitch_arr[P]
        ); // vs: extra nLogPel, plane, xSubUV, ySubUV, thSAD
      }

      norm_weights(weight_arr, _trad); // normaliseWeights<radius>(WSrc, WRefs);


      // chroma
      _degrainchroma_ptr(
        pDstCur + xx * pixelsize_super,
        pDstCur + xx * pixelsize_super + _lsb_offset_arr[P], _lsb_flag, _dst_pitch_arr[P],
        pSrcCur + xx * pixelsize_super, _src_pitch_arr[P],
        ref_data_ptr_arr, pitch_arr, weight_arr, _trad
      );

      if (nLogxRatioUV != _xratiouv_log)
      //xx += nBlkSizeX; // blksize of Y plane, that's why there is xx >> xRatioUVlog above
      xx += (nBlkSizeX >> nLogxRatioUV); // xx: indexing offset

      if (bx == nBlkX - 1 && _covered_width < nWidth) // right non-covered region
      {
        // chroma
        BitBlt(
          pDstCur + (_covered_width >> _xratiouv_log) * pixelsize_super, _dst_pitch_arr[P],
          pSrcCur + (_covered_width >> _xratiouv_log) * pixelsize_super, _src_pitch_arr[P],
          ((nWidth - _covered_width) >> _xratiouv_log) * pixelsize_super /* real row_size */, rowsize /* bad name. it's height = nBlkSizeY >> _yratiouv_log*/,
          _isse_flag
        );
      }
    } // for bx

    pDstCur += effective_nDstPitch;
    pSrcCur += effective_nSrcPitch;

    if (by == nBlkY - 1 && _covered_height < nHeight) // bottom uncovered region
    {
      // chroma
      BitBlt(
        pDstCur, _dst_pitch_arr[P],
        pSrcCur, _src_pitch_arr[P],
        (nWidth >> _xratiouv_log)*pixelsize_super, (nHeight - _covered_height) >> _yratiouv_log /* height */,
        _isse_flag
      );
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

  const int rowsize = (nBlkSizeY - nOverlapY) >> _yratiouv_log; // bad name. it's height really
  const BYTE *pSrcCur = _src_ptr_arr[P] + y_beg * rowsize * _src_pitch_arr[P];

  unsigned short *pDstShort = (_dst_short.empty()) ? 0 : &_dst_short[0] + y_beg * rowsize * _dst_short_pitch;
  int *pDstInt = (_dst_int.empty()) ? 0 : &_dst_int[0] + y_beg * rowsize * _dst_int_pitch;
  const int tmpPitch = nBlkSizeX;
  assert(tmpPitch <= TmpBlock::MAX_SIZE);

  int effective_nSrcPitch = ((nBlkSizeY - nOverlapY) >> _yratiouv_log) * _src_pitch_arr[P]; // pitch is byte granularity
  int effective_dstShortPitch = ((nBlkSizeY - nOverlapY) >> _yratiouv_log) * _dst_short_pitch; // pitch is short granularity
  int effective_dstIntPitch = ((nBlkSizeY - nOverlapY) >> _yratiouv_log) * _dst_int_pitch; // pitch is int granularity

  for (int by = y_beg; by < y_end; ++by)
  {
    int wby = ((by + nBlkY - 3) / (nBlkY - 2)) * 3;
    int xx = 0; // logical offset. Mul by 2 for pixelsize_super==2. Don't mul for indexing int* array
    for (int bx = 0; bx < nBlkX; ++bx)
    {
      // select window
      int wbx = (bx + nBlkX - 3) / (nBlkX - 2);
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
          xx * pixelsize_super, //  the pointer increment inside knows that xx later here is incremented with nBlkSize and not nBlkSize>>_xRatioUV
          _src_pitch_arr[P]
        );
      }

      norm_weights(weight_arr, _trad); // 0th + 1..MAX_TEMP_RAD*2

      // chroma
      // here we don't pass pixelsize, because _degrainchroma_ptr points already to the uint16_t version
      // if the clip was 16 bit one
      _degrainchroma_ptr(
        &tmp_block._d[0], tmp_block._lsb_ptr, _lsb_flag, tmpPitch * pixelsize_super,
        pSrcCur + xx * pixelsize_super, _src_pitch_arr[P],
        ref_data_ptr_arr, pitch_arr, weight_arr, _trad
      );
      if (_lsb_flag)
      {
        _overschroma_lsb_ptr(
          pDstInt + xx, _dst_int_pitch,
          &tmp_block._d[0], tmp_block._lsb_ptr, tmpPitch,
          winOverUV, nBlkSizeX >> _xratiouv_log
        );
      }
      else if(pixelsize_super == 1)
      {
        _overschroma_ptr(
          pDstShort + xx, _dst_short_pitch,
          &tmp_block._d[0], tmpPitch,
          winOverUV, nBlkSizeX >> _xratiouv_log);
      } else if (pixelsize_super == 2)
      {
        _overschroma16_ptr(
          (uint16_t*)(pDstInt + xx), _dst_int_pitch, 
          &tmp_block._d[0], tmpPitch*pixelsize_super, 
          winOverUV, nBlkSizeX >> _xratiouv_log);
      }

      xx += ((nBlkSizeX - nOverlapX) >> nLogxRatioUV); // no pixelsize here

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
    wref = DegrainWeight(c_info._thsad, block_sad);
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
    p = plane_ptr->GetPointer(blx >> _xratiouv_log, bly >> _yratiouv_log);
    np = plane_ptr->GetPitch();
    const sad_t block_sad = block.GetSAD(); // SAD of MV Block. Scaled to MVClip's bits_per_pixel;
    wref = DegrainWeight(c_info._thsadc, block_sad);
  }
  else
  {
    p = src_ptr + xx; // done: kill  >> _xratiouv_log from here and put it in the caller like in MDegrainX
    np = src_pitch;
    wref = 0;
  }
}



void	MDegrainN::norm_weights(int wref_arr[], int trad)
{
  const int nbr_frames = trad * 2 + 1;

  wref_arr[0] = 256;
  int wsum = 1;
  for (int k = 0; k < nbr_frames; ++k)
  {
    wsum += wref_arr[k];
  }

  // normalize weights to 256
  int wsrc = 256;
  for (int k = 1; k < nbr_frames; ++k)
  {
    const int norm = wref_arr[k] * 256 / wsum;
    wref_arr[k] = norm;
    wsrc -= norm;
  }
  wref_arr[0] = wsrc;
}

