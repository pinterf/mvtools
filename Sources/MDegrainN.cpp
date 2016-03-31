

#include "ClipFnc.h"
#include "CopyCode.h"
#include	"def.h"
#include	"MDegrainN.h"
#include "MVFrame.h"
#include "MVPlane.h"
#include "profile.h"
#include "SuperParams64Bits.h"

#include	<emmintrin.h>
#include	<mmintrin.h>

#include	<cassert>
#include	<cmath>



template <int blockWidth, int blockHeight>
void DegrainN_C (
	BYTE *pDst, BYTE *pDstLsb, bool lsb_flag, int nDstPitch,
	const BYTE *pSrc, int nSrcPitch,
	const BYTE *pRef [], int Pitch [],
	int Wall [], int trad
)
{
	if (lsb_flag)
	{
		for (int h = 0; h < blockHeight; ++h)
		{
			for (int x = 0; x < blockWidth; ++x)
			{
				int				val = pSrc [x] * Wall [0];
				for (int k = 0; k < trad; ++k)
				{
					val +=   pRef [k*2    ] [x] * Wall [k*2 + 1]
					       + pRef [k*2 + 1] [x] * Wall [k*2 + 2];
				}
				
				pDst [x]    = val >> 8;
				pDstLsb [x] = val & 255;
			}

			pDst    += nDstPitch;
			pDstLsb += nDstPitch;
			pSrc    += nSrcPitch;
			for (int k = 0; k < trad; ++k)
			{
				pRef [k*2    ] += Pitch [k*2    ];
				pRef [k*2 + 1] += Pitch [k*2 + 1];
			}
		}
	}

	else
	{
		for (int h = 0; h < blockHeight; ++h)
		{
			for (int x = 0; x < blockWidth; ++x)
			{
				int				val = pSrc [x] * Wall [0] + 128;
				for (int k = 0; k < trad; ++k)
				{
					val +=   pRef [k*2    ] [x] * Wall [k*2 + 1]
					       + pRef [k*2 + 1] [x] * Wall [k*2 + 2];
				}
				pDst[x] = val >> 8;
			}

			pDst += nDstPitch;
			pSrc += nSrcPitch;
			for (int k = 0; k < trad; ++k)
			{
				pRef [k*2    ] += Pitch [k*2    ];
				pRef [k*2 + 1] += Pitch [k*2 + 1];
			}
		}
	}
}



template <int blockWidth, int blockHeight>
void DegrainN_mmx (
	BYTE *pDst, BYTE *pDstLsb, bool lsb_flag, int nDstPitch,
	const BYTE *pSrc, int nSrcPitch,
	const BYTE *pRef [], int Pitch [],
	int Wall [], int trad
)
{
	const __m64			z = _mm_setzero_si64();

	if (lsb_flag)
	{
		const __m64			m = _mm_set1_pi16 (255);

		for (int h = 0; h < blockHeight; ++h)
		{
			for (int x = 0; x < blockWidth; x += 4)
			{
				__m64				val = _m_pmullw (
					_m_punpcklbw (*(__m64 *) (pSrc + x), z),
					_mm_set1_pi16 (Wall [0])
				);
				for (int k = 0; k < trad; ++k)
				{
					const __m64		s1 = _m_pmullw (
						_m_punpcklbw (*(__m64 *) (pRef [k * 2    ] + x), z),
						_mm_set1_pi16 (Wall [k * 2 + 1])
					);
					const __m64		s2 = _m_pmullw (
						_m_punpcklbw (*(__m64 *) (pRef [k * 2 + 1] + x), z),
						_mm_set1_pi16 (Wall [k * 2 + 2])
					);
					val = _m_paddw (val, s1);
					val = _m_paddw (val, s2);
				}
				*(int *)(pDst    + x) =
					_m_to_int (_m_packuswb (_m_psrlwi    (val, 8), z));
				*(int *)(pDstLsb + x) =
					_m_to_int (_m_packuswb (_mm_and_si64 (val, m), z));
			}

			pDst    += nDstPitch;
			pDstLsb += nDstPitch;
			pSrc    += nSrcPitch;
			for (int k = 0; k < trad; ++k)
			{
				pRef [k*2    ] += Pitch [k*2    ];
				pRef [k*2 + 1] += Pitch [k*2 + 1];
			}
		}
	}

	else
	{
		const __m64		o = _mm_set1_pi16 (128);

		for (int h = 0; h < blockHeight; ++h)
		{
			for (int x = 0; x < blockWidth; x += 4)
			{
				__m64				val = _m_paddw (_m_pmullw (
					_m_punpcklbw (*(__m64 *) (pSrc + x), z),
					_mm_set1_pi16 (Wall [0])
				), o);
				for (int k = 0; k < trad; ++k)
				{
					const __m64		s1 = _m_pmullw (
						_m_punpcklbw (*(__m64 *) (pRef [k * 2    ] + x), z),
						_mm_set1_pi16 (Wall [k * 2 + 1])
					);
					const __m64		s2 = _m_pmullw (
						_m_punpcklbw (*(__m64 *) (pRef [k * 2 + 1] + x), z),
						_mm_set1_pi16 (Wall [k * 2 + 2])
					);
					val = _m_paddw (val, s1);
					val = _m_paddw (val, s2);
				}
				*(int *)(pDst + x) =
					_m_to_int (_m_packuswb (_m_psrlwi (val, 8), z));
			}

			pDst += nDstPitch;
			pSrc += nSrcPitch;
			for (int k = 0; k < trad; ++k)
			{
				pRef [k*2    ] += Pitch [k*2    ];
				pRef [k*2 + 1] += Pitch [k*2 + 1];
			}
		}
	}

	_m_empty ();
}



template <int blockWidth, int blockHeight>
void DegrainN_sse2 (
	BYTE *pDst, BYTE *pDstLsb, bool lsb_flag, int nDstPitch,
	const BYTE *pSrc, int nSrcPitch,
	const BYTE *pRef [], int Pitch [],
	int Wall [], int trad
)
{
	const __m128i	z = _mm_setzero_si128 ();

	if (lsb_flag)
	{
		const __m128i	m = _mm_set1_epi16 (255);

		for (int h = 0; h < blockHeight; ++h)
		{
			for (int x = 0; x < blockWidth; x += 8)
			{
				__m128i			val = _mm_mullo_epi16 (
					_mm_unpacklo_epi8 (_mm_loadl_epi64 ((__m128i *) (pSrc + x)), z),
					_mm_set1_epi16 (Wall [0])
				);
				for (int k = 0; k < trad; ++k)
				{
					const __m128i	s1 = _mm_mullo_epi16 (
						_mm_unpacklo_epi8 (_mm_loadl_epi64 ((__m128i *) (pRef [k * 2    ] + x)), z),
						_mm_set1_epi16 (Wall [k * 2 + 1])
					);
					const __m128i	s2 = _mm_mullo_epi16 (
						_mm_unpacklo_epi8 (_mm_loadl_epi64 ((__m128i *) (pRef [k * 2 + 1] + x)), z),
						_mm_set1_epi16 (Wall [k * 2 + 2])
					);
					val = _mm_add_epi16 (val, s1);
					val = _mm_add_epi16 (val, s2);
				}
				_mm_storel_epi64 (
					(__m128i*)(pDst    + x),
					_mm_packus_epi16 (_mm_srli_epi16 (val, 8), z)
				);
				_mm_storel_epi64 (
					(__m128i*)(pDstLsb + x),
					_mm_packus_epi16 (_mm_and_si128  (val, m), z)
				);
			}
			pDst    += nDstPitch;
			pDstLsb += nDstPitch;
			pSrc    += nSrcPitch;
			for (int k = 0; k < trad; ++k)
			{
				pRef [k*2    ] += Pitch [k*2    ];
				pRef [k*2 + 1] += Pitch [k*2 + 1];
			}
		}
	}

	else
	{
		const __m128i	o = _mm_set1_epi16 (128);

		for (int h = 0; h < blockHeight; ++h)
		{
			for (int x = 0; x < blockWidth; x += 8)
			{
				__m128i			val = _mm_add_epi16 (_mm_mullo_epi16 (
					_mm_unpacklo_epi8 (_mm_loadl_epi64 ((__m128i *) (pSrc + x)), z),
					_mm_set1_epi16 (Wall [0])
				), o);
				for (int k = 0; k < trad; ++k)
				{
					const __m128i	s1 = _mm_mullo_epi16 (
						_mm_unpacklo_epi8 (_mm_loadl_epi64 ((__m128i *) (pRef [k * 2    ] + x)), z),
						_mm_set1_epi16 (Wall [k * 2 + 1])
					);
					const __m128i	s2 = _mm_mullo_epi16 (
						_mm_unpacklo_epi8 (_mm_loadl_epi64 ((__m128i *) (pRef [k * 2 + 1] + x)), z),
						_mm_set1_epi16 (Wall [k * 2 + 2])
					);
					val = _mm_add_epi16 (val, s1);
					val = _mm_add_epi16 (val, s2);
				}
				_mm_storel_epi64 (
					(__m128i*)(pDst + x),
					_mm_packus_epi16 (_mm_srli_epi16 (val, 8), z)
				);
			}

			pDst += nDstPitch;
			pSrc += nSrcPitch;
			for (int k = 0; k < trad; ++k)
			{
				pRef [k*2    ] += Pitch [k*2    ];
				pRef [k*2 + 1] += Pitch [k*2 + 1];
			}
		}
	}
}



MDegrainN::MDegrainN (
	::PClip child, ::PClip super, ::PClip mvmulti, int trad,
	int thsad, int thsadc, int yuvplanes, int nlimit, int nlimitc,
	int nscd1, int nscd2, bool isse_flag, bool planar_flag, bool lsb_flag,
	int thsad2, int thsadc2, bool mt_flag, ::IScriptEnvironment* env_ptr
)
:	GenericVideoFilter (child)
,	MVFilter (mvmulti, "MDegrainN", env_ptr, 1, 0)
,	_mv_clip_arr ()
,	_trad (trad)
,	_yuvplanes (yuvplanes)
,	_nlimit (nlimit)
,	_nlimitc (nlimitc)
,	_super (super)
,	_isse_flag (isse_flag)
,	_planar_flag (planar_flag)
,	_lsb_flag (lsb_flag)
,	_mt_flag (mt_flag)
,	_height_lsb_mul ((lsb_flag) ? 2 : 1)
,	_yratiouv_log ((yRatioUV == 2) ? 1 : 0)
,	_nsupermodeyuv (-1)
,	_dst_planes (0)
,	_src_planes (0)
,	_overwins ()
,	_overwins_uv ()
,	_oversluma_ptr (0)
,	_overschroma_ptr (0)
,	_oversluma_lsb_ptr (0)
,	_overschroma_lsb_ptr (0)
,	_degrainluma_ptr (0)
,	_degrainchroma_ptr (0)
,	_dst_short ()
,	_dst_short_pitch ()
,	_dst_int ()
,	_dst_int_pitch ()
//,	_usable_flag_arr ()
//,	_planes_ptr ()
//,	_dst_ptr_arr ()
//,	_src_ptr_arr ()
//,	_dst_pitch_arr ()
//,	_src_pitch_arr ()
//,	_lsb_offset_arr ()
,	_covered_width (0)
,	_covered_height (0)
,	_boundary_cnt_arr ()
{
	if (trad > MAX_TEMP_RAD)
	{
		env_ptr->ThrowError (
			"MDegrainN: temporal radius too large (max %d)",
			MAX_TEMP_RAD
		);
	}
	else if (trad < 1)
	{
		env_ptr->ThrowError ("MDegrainN: temporal radius must be at least 1.");
	}

	_mv_clip_arr.resize (_trad * 2);
	for (int k = 0; k < _trad * 2; ++k)
	{
		_mv_clip_arr [k]._clip_sptr = SharedPtr <MVClip> (
			new MVClip (mvmulti, nscd1, nscd2, env_ptr, _trad * 2, k)
		);

		static const char *	name_0 [2] = { "mvbw", "mvfw" };
		char				txt_0 [127+1];
		sprintf (txt_0, "%s%d", name_0 [k & 1], 1 + k / 2);
		CheckSimilarity (*(_mv_clip_arr [k]._clip_sptr), txt_0, env_ptr);
	}

	const int		mv_thscd1 = _mv_clip_arr [0]._clip_sptr->GetThSCD1 ();
	thsad   = thsad   * mv_thscd1 / nscd1;	// normalize to block SAD
	thsadc  = thsadc  * mv_thscd1 / nscd1;	// chroma
	thsad2  = thsad2  * mv_thscd1 / nscd1;
	thsadc2 = thsadc2 * mv_thscd1 / nscd1;

	const ::VideoInfo &	vi_super = _super->GetVideoInfo ();

	// get parameters of prepared super clip - v2.0
	SuperParams64Bits params;
	memcpy (&params, &vi_super.num_audio_samples, 8);
	const int		nHeightS     = params.nHeight;
	const int		nSuperHPad   = params.nHPad;
	const int		nSuperVPad   = params.nVPad;
	const int		nSuperPel    = params.nPel;
	const int		nSuperLevels = params.nLevels;
	_nsupermodeyuv              = params.nModeYUV;

	for (int k = 0; k < _trad * 2; ++k)
	{
		MvClipInfo &		c_info = _mv_clip_arr [k];

		c_info._gof_sptr = SharedPtr <MVGroupOfFrames> (new MVGroupOfFrames (
			nSuperLevels,
			nWidth,
			nHeight,
			nSuperPel,
			nSuperHPad,
			nSuperVPad,
			_nsupermodeyuv,
			_isse_flag,
			yRatioUV,
			mt_flag
		));

		// Computes the SAD thresholds for this source frame, a cosine-shaped
		// smooth transition between thsad(c) and thsad(c)2.
		const int		d = k / 2 + 1;
		c_info._thsad  = ClipFnc::interpolate_thsad (thsad,  thsad2,  d, _trad);
		c_info._thsadc = ClipFnc::interpolate_thsad (thsadc, thsadc2, d, _trad);
	}

	const int		nSuperWidth  = vi_super.width;
	const int		nSuperHeight = vi_super.height;

	if (   nHeight != nHeightS
	    || nHeight != vi.height
	    || nWidth  != nSuperWidth - nSuperHPad * 2
	    || nWidth  != vi.width
	    || nPel    != nSuperPel)
	{
		env_ptr->ThrowError ("MDegrainN : wrong source or super frame size");
	}

   if ((pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2 && ! _planar_flag)
   {
		_dst_planes = std::auto_ptr <YUY2Planes> (
			new YUY2Planes (nWidth, nHeight * _height_lsb_mul)
		);
		_src_planes = std::auto_ptr <YUY2Planes> (
			new YUY2Planes (nWidth, nHeight)
		);
   }
   _dst_short_pitch = ((nWidth + 15) / 16) * 16;
	_dst_int_pitch   = _dst_short_pitch;
   if (nOverlapX > 0 || nOverlapY > 0)
   {
		_overwins    = std::auto_ptr <OverlapWindows> (
			new OverlapWindows (nBlkSizeX, nBlkSizeY, nOverlapX, nOverlapY)
		);
		_overwins_uv = std::auto_ptr <OverlapWindows> (new OverlapWindows (
			nBlkSizeX / 2, nBlkSizeY >> _yratiouv_log,
			nOverlapX / 2, nOverlapY >> _yratiouv_log
		));
		if (_lsb_flag)
		{
			_dst_int.resize (_dst_int_pitch * nHeight);
		}
		else
		{
			_dst_short.resize (_dst_short_pitch * nHeight);
		}
   }
	if (nOverlapY > 0)
	{
		_boundary_cnt_arr.resize (nBlkY);
	}

	switch (nBlkSizeX)
	{
	case 32:
		if (nBlkSizeY==16) {					_oversluma_lsb_ptr   = OverlapsLsb_C<32,16>; 
			if (yRatioUV==2) {				_overschroma_lsb_ptr = OverlapsLsb_C<16,8>;  }
			else {								_overschroma_lsb_ptr = OverlapsLsb_C<16,16>; }
		} else if (nBlkSizeY==32) {		_oversluma_lsb_ptr   = OverlapsLsb_C<32,32>;
			if (yRatioUV==2) {				_overschroma_lsb_ptr = OverlapsLsb_C<16,16>; }
			else {								_overschroma_lsb_ptr = OverlapsLsb_C<16,32>; }
		} break;
	case 16:
		if (nBlkSizeY==16) {					_oversluma_lsb_ptr   = OverlapsLsb_C<16,16>; 
			if (yRatioUV==2) {				_overschroma_lsb_ptr = OverlapsLsb_C<8,8>;   }
			else {								_overschroma_lsb_ptr = OverlapsLsb_C<8,16>;  }
		} else if (nBlkSizeY==8) {			_oversluma_lsb_ptr   = OverlapsLsb_C<16,8>;  
			if (yRatioUV==2) {				_overschroma_lsb_ptr = OverlapsLsb_C<8,4>;   }
			else {								_overschroma_lsb_ptr = OverlapsLsb_C<8,8>;   }
		} else if (nBlkSizeY==2) {			_oversluma_lsb_ptr   = OverlapsLsb_C<16,2>;  
			if (yRatioUV==2) {				_overschroma_lsb_ptr = OverlapsLsb_C<8,1>;   }
			else {								_overschroma_lsb_ptr = OverlapsLsb_C<8,2>;   }
		}
		break;
	case 4:										_oversluma_lsb_ptr   = OverlapsLsb_C<4,4>;   
		if (yRatioUV==2) {					_overschroma_lsb_ptr = OverlapsLsb_C<2,2>;   }
		else {									_overschroma_lsb_ptr = OverlapsLsb_C<2,4>;   }
		break;
	case 8:
	default:
		if (nBlkSizeY==8) {					_oversluma_lsb_ptr   = OverlapsLsb_C<8,8>;   
			if (yRatioUV==2) {				_overschroma_lsb_ptr = OverlapsLsb_C<4,4>;   }
			else {								_overschroma_lsb_ptr = OverlapsLsb_C<4,8>;   }
		} else if (nBlkSizeY==4) {			_oversluma_lsb_ptr   = OverlapsLsb_C<8,4>;   
			if (yRatioUV==2) {				_overschroma_lsb_ptr = OverlapsLsb_C<4,2>;   }
			else {								_overschroma_lsb_ptr = OverlapsLsb_C<4,4>;   }
		}
   }

	if (((env_ptr->GetCPUFlags () & CPUF_SSE2) != 0) & _isse_flag)
	{
		switch (nBlkSizeX)
		{
		case 32:
			if (nBlkSizeY==16) {				_oversluma_ptr   = Overlaps32x16_sse2; _degrainluma_ptr   = DegrainN_sse2<32,16>;
				if (yRatioUV==2) {			_overschroma_ptr = Overlaps16x8_sse2;  _degrainchroma_ptr = DegrainN_sse2<16,8>;	}
				else {							_overschroma_ptr = Overlaps16x16_sse2; _degrainchroma_ptr = DegrainN_sse2<16,16>;	}
			} else if (nBlkSizeY==32) {	_oversluma_ptr   = Overlaps32x32_sse2; _degrainluma_ptr   = DegrainN_sse2<32,32>;
				if (yRatioUV==2) {			_overschroma_ptr = Overlaps16x16_sse2; _degrainchroma_ptr = DegrainN_sse2<16,16>;	}
				else {							_overschroma_ptr = Overlaps16x32_sse2; _degrainchroma_ptr = DegrainN_sse2<16,32>;	}
			} break;
		case 16:
			if (nBlkSizeY==16) {          _oversluma_ptr   = Overlaps16x16_sse2; _degrainluma_ptr   = DegrainN_sse2<16,16>;
				if (yRatioUV==2) {			_overschroma_ptr = Overlaps8x8_sse2;   _degrainchroma_ptr = DegrainN_sse2<8,8>;	}
				else {							_overschroma_ptr = Overlaps8x16_sse2;  _degrainchroma_ptr = DegrainN_sse2<8,16>;	}
			} else if (nBlkSizeY==8) {		_oversluma_ptr   = Overlaps16x8_sse2;  _degrainluma_ptr   = DegrainN_sse2<16,8>;
				if (yRatioUV==2) {			_overschroma_ptr = Overlaps8x4_sse2;   _degrainchroma_ptr = DegrainN_sse2<8,4>;	}
				else {							_overschroma_ptr = Overlaps8x8_sse2;   _degrainchroma_ptr = DegrainN_sse2<8,8>;	}
			} else if (nBlkSizeY==2) {		_oversluma_ptr   = Overlaps16x2_sse2;  _degrainluma_ptr   = DegrainN_sse2<16,2>;
				if (yRatioUV==2) {			_overschroma_ptr = Overlaps8x1_sse2;   _degrainchroma_ptr = DegrainN_sse2<8,1>;	}
				else {							_overschroma_ptr = Overlaps8x2_sse2;   _degrainchroma_ptr = DegrainN_sse2<8,2>;	}
			}
			break;
		case 4:									_oversluma_ptr   = Overlaps4x4_sse2;   _degrainluma_ptr   = DegrainN_mmx<4,4>;
			if (yRatioUV==2) {				_overschroma_ptr = Overlaps_C<2,2>;	  _degrainchroma_ptr = DegrainN_C<2,2>;		}
			else {								_overschroma_ptr = Overlaps_C<2,4>;   _degrainchroma_ptr = DegrainN_C<2,4>;		}
			break;
		case 8:
		default:
			if (nBlkSizeY==8) {				_oversluma_ptr   = Overlaps8x8_sse2;   _degrainluma_ptr   = DegrainN_sse2<8,8>;
				if (yRatioUV==2) {			_overschroma_ptr = Overlaps4x4_sse2;   _degrainchroma_ptr = DegrainN_mmx<4,4>;		}
				else {							_overschroma_ptr = Overlaps4x8_sse2;   _degrainchroma_ptr = DegrainN_mmx<4,8>;		}
			} else if (nBlkSizeY==4) {		_oversluma_ptr   = Overlaps8x4_sse2;   _degrainluma_ptr   = DegrainN_sse2<8,4>;
				if (yRatioUV==2) {			_overschroma_ptr = Overlaps4x2_sse2;	  _degrainchroma_ptr = DegrainN_mmx<4,2>;		}
				else {							_overschroma_ptr = Overlaps4x4_sse2;   _degrainchroma_ptr = DegrainN_mmx<4,4>;		}
			}
		}
	}
	else if (_isse_flag)
	{
		switch (nBlkSizeX)
		{
		case 32:
			if (nBlkSizeY==16) {				_oversluma_ptr   = Overlaps32x16_sse2; _degrainluma_ptr   = DegrainN_mmx<32,16>;
				if (yRatioUV==2) {			_overschroma_ptr = Overlaps16x8_sse2;  _degrainchroma_ptr = DegrainN_mmx<16,8>;	}
				else {							_overschroma_ptr = Overlaps16x16_sse2; _degrainchroma_ptr = DegrainN_mmx<16,16>;	}
			} else if (nBlkSizeY==32) {	_oversluma_ptr   = Overlaps32x32_sse2; _degrainluma_ptr   = DegrainN_mmx<32,32>;
				if (yRatioUV==2) {			_overschroma_ptr = Overlaps16x16_sse2; _degrainchroma_ptr = DegrainN_mmx<16,16>;	}
				else {							_overschroma_ptr = Overlaps16x32_sse2; _degrainchroma_ptr = DegrainN_mmx<16,32>;	}
			} break;
		case 16:
			if (nBlkSizeY==16) {				_oversluma_ptr   = Overlaps16x16_sse2; _degrainluma_ptr   = DegrainN_mmx<16,16>;
				if (yRatioUV==2) {			_overschroma_ptr = Overlaps8x8_sse2;   _degrainchroma_ptr = DegrainN_mmx<8,8>;		}
				else {							_overschroma_ptr = Overlaps8x16_sse2;  _degrainchroma_ptr = DegrainN_mmx<8,16>;	}
			} else if (nBlkSizeY==8) {		_oversluma_ptr   = Overlaps16x8_sse2;  _degrainluma_ptr   = DegrainN_mmx<16,8>;
				if (yRatioUV==2) {			_overschroma_ptr = Overlaps8x4_sse2;   _degrainchroma_ptr = DegrainN_mmx<8,4>;		}
				else {							_overschroma_ptr = Overlaps8x8_sse2;   _degrainchroma_ptr = DegrainN_mmx<8,8>;		}
			} else if (nBlkSizeY==2) {		_oversluma_ptr   = Overlaps16x2_sse2;  _degrainluma_ptr   = DegrainN_mmx<16,2>;
				if (yRatioUV==2) {			_overschroma_ptr = Overlaps8x1_sse2;   _degrainchroma_ptr = DegrainN_mmx<8,1>;		}
				else {							_overschroma_ptr = Overlaps8x2_sse2;   _degrainchroma_ptr = DegrainN_mmx<8,2>;		}
			}
			break;
		case 4:									_oversluma_ptr   = Overlaps4x4_sse2;   _degrainluma_ptr   = DegrainN_mmx<4,4>;
			if (yRatioUV==2) {				_overschroma_ptr = Overlaps_C<2,2>;   _degrainchroma_ptr = DegrainN_C<2,2>;		}
			else {								_overschroma_ptr = Overlaps_C<2,4>;   _degrainchroma_ptr = DegrainN_C<2,4>;		}
			break;
		case 8:
			default:
			if (nBlkSizeY==8) {				_oversluma_ptr   = Overlaps8x8_sse2;   _degrainluma_ptr   = DegrainN_mmx<8,8>;
				if (yRatioUV==2) {			_overschroma_ptr = Overlaps4x4_sse2;   _degrainchroma_ptr = DegrainN_mmx<4,4>;		}
				else {							_overschroma_ptr = Overlaps4x8_sse2;   _degrainchroma_ptr = DegrainN_mmx<4,8>;		}
			} else if (nBlkSizeY==4) {		_oversluma_ptr   = Overlaps8x4_sse2;	  _degrainluma_ptr   = DegrainN_mmx<8,4>;
				if (yRatioUV==2) {			_overschroma_ptr = Overlaps4x2_sse2;	  _degrainchroma_ptr = DegrainN_mmx<4,2>;		}
				else {							_overschroma_ptr = Overlaps4x4_sse2;   _degrainchroma_ptr = DegrainN_mmx<4,4>;		}
			}
		}
	}
	else
	{
		switch (nBlkSizeX)
		{
		case 32:
			if (nBlkSizeY==16) {				_oversluma_ptr   = Overlaps_C<32,16>; _degrainluma_ptr   = DegrainN_C<32,16>;
				if (yRatioUV==2) {			_overschroma_ptr = Overlaps_C<16,8>;  _degrainchroma_ptr = DegrainN_C<16,8>;		}
				else {							_overschroma_ptr = Overlaps_C<16,16>; _degrainchroma_ptr = DegrainN_C<16,16>;		}
			} else if (nBlkSizeY==32) {	_oversluma_ptr   = Overlaps_C<32,32>; _degrainluma_ptr   = DegrainN_C<32,32>;
				if (yRatioUV==2) {			_overschroma_ptr = Overlaps_C<16,16>; _degrainchroma_ptr = DegrainN_C<16,16>;		}
				else {							_overschroma_ptr = Overlaps_C<16,32>; _degrainchroma_ptr = DegrainN_C<16,32>;		}
			} break;
		case 16:
			if (nBlkSizeY==16) {				_oversluma_ptr   = Overlaps_C<16,16>; _degrainluma_ptr   = DegrainN_C<16,16>;
				if (yRatioUV==2) {			_overschroma_ptr = Overlaps_C<8,8>;   _degrainchroma_ptr = DegrainN_C<8,8>;		}
				else {							_overschroma_ptr = Overlaps_C<8,16>;  _degrainchroma_ptr = DegrainN_C<8,16>;		}
			} else if (nBlkSizeY==8) {		_oversluma_ptr   = Overlaps_C<16,8>;  _degrainluma_ptr   = DegrainN_C<16,8>;
				if (yRatioUV==2) {			_overschroma_ptr = Overlaps_C<8,4>;   _degrainchroma_ptr = DegrainN_C<8,4>;		}
				else {							_overschroma_ptr = Overlaps_C<8,8>;   _degrainchroma_ptr = DegrainN_C<8,8>;		}
			} else if (nBlkSizeY==2) {		_oversluma_ptr   = Overlaps_C<16,2>;  _degrainluma_ptr   = DegrainN_C<16,2>;
				if (yRatioUV==2) {			_overschroma_ptr = Overlaps_C<8,1>;   _degrainchroma_ptr = DegrainN_C<8,1>;		}
				else {							_overschroma_ptr = Overlaps_C<8,2>;   _degrainchroma_ptr = DegrainN_C<8,2>;		}
			}
			break;
		case 4:									_oversluma_ptr   = Overlaps_C<4,4>;   _degrainluma_ptr   = DegrainN_C<4,4>;
			if (yRatioUV==2) {				_overschroma_ptr = Overlaps_C<2,2>;   _degrainchroma_ptr = DegrainN_C<2,2>;		}
			else {								_overschroma_ptr = Overlaps_C<2,4>;   _degrainchroma_ptr = DegrainN_C<2,4>;		}
			break;
		case 8:
		default:
			if (nBlkSizeY==8) {				_oversluma_ptr   = Overlaps_C<8,8>;   _degrainluma_ptr   = DegrainN_C<8,8>;
				if (yRatioUV==2) {			_overschroma_ptr = Overlaps_C<4,4>;   _degrainchroma_ptr = DegrainN_C<4,4>;		}
				else {							_overschroma_ptr = Overlaps_C<4,8>;   _degrainchroma_ptr = DegrainN_C<4,8>;		}
			} else if (nBlkSizeY==4) {		_oversluma_ptr   = Overlaps_C<8,4>;   _degrainluma_ptr   = DegrainN_C<8,4>;
				if (yRatioUV==2) {			_overschroma_ptr = Overlaps_C<4,2>;   _degrainchroma_ptr = DegrainN_C<4,2>;		}
				else {							_overschroma_ptr = Overlaps_C<4,4>;   _degrainchroma_ptr = DegrainN_C<4,4>;		}
			}
		}
	}

	if (_lsb_flag)
	{
		vi.height <<= 1;
	}
}



MDegrainN::~MDegrainN ()
{
	// Nothing
}



::PVideoFrame __stdcall	MDegrainN::GetFrame (int n, ::IScriptEnvironment* env_ptr)
{
	_covered_width  = nBlkX * (nBlkSizeX - nOverlapX) + nOverlapX;
	_covered_height = nBlkY * (nBlkSizeY - nOverlapY) + nOverlapY;

	const BYTE *	pRef [MAX_TEMP_RAD * 2] [3];
	int				nRefPitches [MAX_TEMP_RAD * 2] [3];
	unsigned char *	pDstYUY2;
	const unsigned char *	pSrcYUY2;
	int				nDstPitchYUY2;
	int				nSrcPitchYUY2;

	for (int k2 = 0; k2 < _trad * 2; ++k2)
	{
		// reorder ror regular frames order in v2.0.9.2
		const int		k = reorder_ref (k2);

		// v2.0.9.2 - it seems we do not need in vectors clip anymore when we
		// finished copying them to fakeblockdatas
		MVClip &			mv_clip = *(_mv_clip_arr [k]._clip_sptr);
		::PVideoFrame	mv = mv_clip.GetFrame (n, env_ptr);
		mv_clip.Update (mv, env_ptr);
		_usable_flag_arr [k] = mv_clip.IsUsable ();
	}

	::PVideoFrame	src = child->GetFrame (n, env_ptr);
	::PVideoFrame	dst = env_ptr->NewVideoFrame (vi);
	if ((pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2)
	{
		if (! _planar_flag)
		{
			pDstYUY2      = dst->GetWritePtr ();
			nDstPitchYUY2 = dst->GetPitch ();
			_dst_ptr_arr [0]   = _dst_planes->GetPtr ();
			_dst_ptr_arr [1]   = _dst_planes->GetPtrU ();
			_dst_ptr_arr [2]   = _dst_planes->GetPtrV ();
			_dst_pitch_arr [0] = _dst_planes->GetPitch ();
			_dst_pitch_arr [1] = _dst_planes->GetPitchUV ();
			_dst_pitch_arr [2] = _dst_planes->GetPitchUV ();

			pSrcYUY2      = src->GetReadPtr ();
			nSrcPitchYUY2 = src->GetPitch ();
			_src_ptr_arr [0]   = _src_planes->GetPtr ();
			_src_ptr_arr [1]   = _src_planes->GetPtrU ();
			_src_ptr_arr [2]   = _src_planes->GetPtrV ();
			_src_pitch_arr [0] = _src_planes->GetPitch ();
			_src_pitch_arr [1] = _src_planes->GetPitchUV ();
			_src_pitch_arr [2] = _src_planes->GetPitchUV ();

			YUY2ToPlanes (
				pSrcYUY2, nSrcPitchYUY2, nWidth, nHeight,
				_src_ptr_arr [0],                   _src_pitch_arr [0],
				_src_ptr_arr [1], _src_ptr_arr [2], _src_pitch_arr [1],
				_isse_flag
			);
		}
		else
		{
			_dst_ptr_arr [0]   = dst->GetWritePtr ();
			_dst_ptr_arr [1]   = _dst_ptr_arr [0] + nWidth;
			_dst_ptr_arr [2]   = _dst_ptr_arr [1] + nWidth / 2;
			_dst_pitch_arr [0] = dst->GetPitch ();
			_dst_pitch_arr [1] = _dst_pitch_arr [0];
			_dst_pitch_arr [2] = _dst_pitch_arr [0];
			_src_ptr_arr [0]   = src->GetReadPtr ();
			_src_ptr_arr [1]   = _src_ptr_arr [0] + nWidth;
			_src_ptr_arr [2]   = _src_ptr_arr [1] + nWidth / 2;
			_src_pitch_arr [0] = src->GetPitch ();
			_src_pitch_arr [1] = _src_pitch_arr [0];
			_src_pitch_arr [2] = _src_pitch_arr [0];
		}
	}
	else
	{
		 _dst_ptr_arr [0]   = YWPLAN (dst);
		 _dst_ptr_arr [1]   = UWPLAN (dst);
		 _dst_ptr_arr [2]   = VWPLAN (dst);
		 _dst_pitch_arr [0] = YPITCH (dst);
		 _dst_pitch_arr [1] = UPITCH (dst);
		 _dst_pitch_arr [2] = VPITCH (dst);
		 _src_ptr_arr [0]   = YRPLAN (src);
		 _src_ptr_arr [1]   = URPLAN (src);
		 _src_ptr_arr [2]   = VRPLAN (src);
		 _src_pitch_arr [0] = YPITCH (src);
		 _src_pitch_arr [1] = UPITCH (src);
		 _src_pitch_arr [2] = VPITCH (src);
	}

	_lsb_offset_arr [0] = _dst_pitch_arr [0] *  nHeight;
	_lsb_offset_arr [1] = _dst_pitch_arr [1] * (nHeight >> _yratiouv_log);
	_lsb_offset_arr [2] = _dst_pitch_arr [2] * (nHeight >> _yratiouv_log);

	if (_lsb_flag)
	{
		memset (_dst_ptr_arr [0] + _lsb_offset_arr [0], 0, _lsb_offset_arr [0]);
		if (! _planar_flag)
		{
			memset (_dst_ptr_arr [1] + _lsb_offset_arr [1], 0, _lsb_offset_arr [1]);
			memset (_dst_ptr_arr [2] + _lsb_offset_arr [2], 0, _lsb_offset_arr [2]);
		}
	}

	::PVideoFrame	ref [MAX_TEMP_RAD * 2];

	for (int k2 = 0; k2 < _trad * 2; ++k2)
	{
		// reorder ror regular frames order in v2.0.9.2
		const int		k = reorder_ref (k2);
		MVClip &			mv_clip = *(_mv_clip_arr [k]._clip_sptr);
		mv_clip.use_ref_frame (ref [k], _usable_flag_arr [k], _super, n, env_ptr);
	}

	if ((pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2)
	{
		for (int k2 = 0; k2 < _trad * 2; ++k2)
		{
			const int		k = reorder_ref (k2);
			if (_usable_flag_arr [k])
			{
				pRef [k] [0] = ref [k]->GetReadPtr ();
				pRef [k] [1] = pRef [k] [0] + ref [k]->GetRowSize () / 2;
				pRef [k] [2] = pRef [k] [1] + ref [k]->GetRowSize () / 4;
				nRefPitches [k] [0]  = ref [k]->GetPitch();
				nRefPitches [k] [1]  = nRefPitches [k] [0];
				nRefPitches [k] [2]  = nRefPitches [k] [0];
			}
		}
	}
	else
	{
		for (int k2 = 0; k2 < _trad * 2; ++k2)
		{
			const int		k = reorder_ref (k2);
			if (_usable_flag_arr [k])
			{
				pRef [k] [0] = YRPLAN (ref [k]);
				pRef [k] [1] = URPLAN (ref [k]);
				pRef [k] [2] = VRPLAN (ref [k]);
				nRefPitches [k] [0] = YPITCH (ref [k]);
				nRefPitches [k] [1] = UPITCH (ref [k]);
				nRefPitches [k] [2] = VPITCH (ref [k]);
			}
		}
	}

	memset (_planes_ptr, 0, _trad * 2 * sizeof (_planes_ptr [0]));

	for (int k2 = 0; k2 < _trad * 2; ++k2)
	{
		const int		k = reorder_ref (k2);
		MVGroupOfFrames &	gof = *(_mv_clip_arr [k]._gof_sptr);
		gof.Update (
			_yuvplanes,
			const_cast <BYTE *> (pRef [k] [0]), nRefPitches [k] [0],
			const_cast <BYTE *> (pRef [k] [1]), nRefPitches [k] [1],
			const_cast <BYTE *> (pRef [k] [2]), nRefPitches [k] [2]
		);
		if (_yuvplanes & YPLANE)
		{
			_planes_ptr [k] [0] = gof.GetFrame (0)->GetPlane (YPLANE);
		}
		if (_yuvplanes & UPLANE)
		{
			_planes_ptr [k] [1] = gof.GetFrame (0)->GetPlane (UPLANE);
		}
		if (_yuvplanes & VPLANE)
		{
			_planes_ptr [k] [2] = gof.GetFrame (0)->GetPlane (VPLANE);
		}
	}

	PROFILE_START (MOTION_PROFILE_COMPENSATION);

	//-------------------------------------------------------------------------
	// LUMA plane Y

	if ((_yuvplanes & YPLANE) == 0)
	{
		BitBlt (
			_dst_ptr_arr [0], _dst_pitch_arr [0],
			_src_ptr_arr [0], _src_pitch_arr [0],
			nWidth, nHeight, _isse_flag
		);
	}

	else
	{
		Slicer			slicer (_mt_flag);

		if (nOverlapX == 0 && nOverlapY == 0)
		{
			slicer.start (
				nBlkY,
				*this,
				&MDegrainN::process_luma_normal_slice
			);
			slicer.wait ();
		}

		// Overlap
		else
		{
			unsigned short *	pDstShort = (_dst_short.empty ()) ? 0 : &_dst_short [0];
			int *					pDstInt   = (_dst_int.empty ()  ) ? 0 : &_dst_int [0];

			if (_lsb_flag)
			{
				MemZoneSet (
					reinterpret_cast <unsigned char *> (pDstInt), 0,
					_covered_width * 4, _covered_height, 0, 0, _dst_int_pitch * 4
				);
			}
			else
			{
				MemZoneSet (
					reinterpret_cast <unsigned char *> (pDstShort), 0,
					_covered_width * 2, _covered_height, 0, 0, _dst_short_pitch * 2
				);
			}

			if (nOverlapY > 0)
			{
				memset (
					&_boundary_cnt_arr [0],
					0,
					_boundary_cnt_arr.size () * sizeof (_boundary_cnt_arr [0])
				);
			}

			slicer.start (
				nBlkY,
				*this,
				&MDegrainN::process_luma_overlap_slice,
				2
			);
			slicer.wait ();

			if (_lsb_flag)
			{
				Short2BytesLsb (
					_dst_ptr_arr [0],
					_dst_ptr_arr [0] + _lsb_offset_arr [0],
					_dst_pitch_arr [0],
					&_dst_int [0], _dst_int_pitch,
					_covered_width, _covered_height
				);
			}
			else
			{
				Short2Bytes (
					_dst_ptr_arr [0], _dst_pitch_arr [0],
					&_dst_short [0], _dst_short_pitch,
					_covered_width, _covered_height
				);
			}
			if (_covered_width < nWidth)
			{
				BitBlt (
					_dst_ptr_arr [0] + _covered_width, _dst_pitch_arr [0],
					_src_ptr_arr [0] + _covered_width, _src_pitch_arr [0],
					nWidth - _covered_width, _covered_height, _isse_flag
				);
			}
			if (_covered_height < nHeight) // bottom noncovered region
			{
				BitBlt (
					_dst_ptr_arr [0] + _covered_height * _dst_pitch_arr [0], _dst_pitch_arr [0],
					_src_ptr_arr [0] + _covered_height * _src_pitch_arr [0], _src_pitch_arr [0],
					nWidth, nHeight - _covered_height, _isse_flag
				);
			}
		}	// overlap - end

		if (_nlimit < 255)
		{
			if (_isse_flag)
			{
				LimitChanges_sse2 (
					_dst_ptr_arr [0], _dst_pitch_arr [0],
					_src_ptr_arr [0], _src_pitch_arr [0],
					nWidth, nHeight, _nlimit
				);
			}
			else
			{
				LimitChanges_c (
					_dst_ptr_arr [0], _dst_pitch_arr [0],
					_src_ptr_arr [0], _src_pitch_arr [0],
					nWidth, nHeight, _nlimit
				);
			}
		}
	}

	//-------------------------------------------------------------------------
	// CHROMA planes

	process_chroma <1> (UPLANE & _nsupermodeyuv);
	process_chroma <2> (VPLANE & _nsupermodeyuv);

	//-------------------------------------------------------------------------

	_mm_empty (); // (we may use double-float somewhere) Fizick

	PROFILE_STOP (MOTION_PROFILE_COMPENSATION);

	if ((pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2 && ! _planar_flag)
	{
		YUY2FromPlanes (
			pDstYUY2, nDstPitchYUY2, nWidth, nHeight * _height_lsb_mul,
			_dst_ptr_arr [0],                   _dst_pitch_arr [0],
			_dst_ptr_arr [1], _dst_ptr_arr [2], _dst_pitch_arr [1], _isse_flag);
	}

	return (dst);
}



// Fn...F1 B1...Bn
int	MDegrainN::reorder_ref (int index) const
{
	assert (index >= 0);
	assert (index < _trad * 2);

	const int		k =   (index < _trad)
						    ? (_trad - index) * 2 - 1
						    : (index - _trad) * 2;

	return (k);
}



template <int P>
void	MDegrainN::process_chroma (int plane_mask)
{
	if ((_yuvplanes & plane_mask) == 0)
	{
		BitBlt (
			_dst_ptr_arr [P], _dst_pitch_arr [P],
			_src_ptr_arr [P], _src_pitch_arr [P],
			nWidth >> 1, nHeight >> _yratiouv_log, _isse_flag
		);
	}

	else
	{
		Slicer			slicer (_mt_flag);

		if (nOverlapX == 0 && nOverlapY == 0)
		{
			slicer.start (
				nBlkY,
				*this,
				&MDegrainN::process_chroma_normal_slice <P>
			);
			slicer.wait ();
		}

		// Overlap
		else
		{
			unsigned short *	pDstShort = (_dst_short.empty ()) ? 0 : &_dst_short [0];
			int *					pDstInt   = (_dst_int.empty ()  ) ? 0 : &_dst_int [0];

			if (_lsb_flag)
			{
				MemZoneSet (
					reinterpret_cast <unsigned char *> (pDstInt), 0,
					_covered_width * 2, _covered_height >> _yratiouv_log,
					0, 0, _dst_int_pitch * 4
				);
			}
			else
			{
				MemZoneSet (
					reinterpret_cast <unsigned char *> (pDstShort), 0,
					_covered_width, _covered_height >> _yratiouv_log,
					0, 0, _dst_short_pitch * 2
				);
			}

			if (nOverlapY > 0)
			{
				memset (
					&_boundary_cnt_arr [0],
					0,
					_boundary_cnt_arr.size () * sizeof (_boundary_cnt_arr [0])
				);
			}

			slicer.start (
				nBlkY,
				*this,
				&MDegrainN::process_chroma_overlap_slice <P>,
				2
			);
			slicer.wait ();

			if (_lsb_flag)
			{
				Short2BytesLsb (
					_dst_ptr_arr [P],
					_dst_ptr_arr [P] + _lsb_offset_arr [P],
					_dst_pitch_arr [P],
					&_dst_int [0], _dst_int_pitch,
					_covered_width >> 1, _covered_height >> _yratiouv_log
				);
			}
			else
			{
				Short2Bytes (
					_dst_ptr_arr [P], _dst_pitch_arr [P],
					&_dst_short [0], _dst_short_pitch,
					_covered_width >> 1, _covered_height >> _yratiouv_log
				);
			}
			if (_covered_width < nWidth)
			{
				BitBlt (
					_dst_ptr_arr [P] + (_covered_width >> 1), _dst_pitch_arr [P],
					_src_ptr_arr [P] + (_covered_width >> 1), _src_pitch_arr [P],
					(nWidth - _covered_width) >> 1, _covered_height >> _yratiouv_log,
					_isse_flag
				);
			}
			if (_covered_height < nHeight) // bottom noncovered region
			{
				BitBlt (
					_dst_ptr_arr [P] + ((_dst_pitch_arr [P] * _covered_height) >> _yratiouv_log), _dst_pitch_arr [P],
					_src_ptr_arr [P] + ((_src_pitch_arr [P] * _covered_height) >> _yratiouv_log), _src_pitch_arr [P],
					nWidth >> 1, ((nHeight - _covered_height) >> _yratiouv_log),
					_isse_flag
				);
			}
		}	// overlap - end

		if (_nlimitc < 255)
		{
			if (_isse_flag)
			{
				LimitChanges_sse2 (
					_dst_ptr_arr [P], _dst_pitch_arr [P],
					_src_ptr_arr [P], _src_pitch_arr [P],
					nWidth >> 1, nHeight >> _yratiouv_log,
					_nlimitc
				);
			}
			else
			{
				LimitChanges_c (
					_dst_ptr_arr [P], _dst_pitch_arr [P],
					_src_ptr_arr [P], _src_pitch_arr [P],
					nWidth >> 1, nHeight >> _yratiouv_log,
					_nlimitc
				);
			}
		}
	}
}



void	MDegrainN::process_luma_normal_slice (Slicer::TaskData &td)
{
	assert (&td != 0);

	const int		rowsize = nBlkSizeY;
	BYTE *			pDstCur = _dst_ptr_arr [0] + td._y_beg * rowsize * _dst_pitch_arr [0];
	const BYTE *	pSrcCur = _src_ptr_arr [0] + td._y_beg * rowsize * _src_pitch_arr [0];

	for (int by = td._y_beg; by < td._y_end; ++by)
	{
		int				xx = 0;
		for (int bx = 0; bx < nBlkX; ++bx)
		{
			int				i = by * nBlkX + bx;
			const BYTE *	ref_data_ptr_arr [MAX_TEMP_RAD * 2];
			int				pitch_arr [MAX_TEMP_RAD * 2];
			int				weight_arr [1 + MAX_TEMP_RAD * 2];

			for (int k = 0; k < _trad * 2; ++k)
			{
				use_block_y (
					ref_data_ptr_arr [k],
					pitch_arr [k],
					weight_arr [k + 1],
					_usable_flag_arr [k],
					_mv_clip_arr [k],
					i,
					_planes_ptr [k] [0],
					pSrcCur,
					xx,
					_src_pitch_arr [0]
				);
			}

			norm_weights (weight_arr, _trad);

			// luma
			_degrainluma_ptr (
				pDstCur + xx, pDstCur + _lsb_offset_arr [0] + xx, _lsb_flag, _dst_pitch_arr [0],
				pSrcCur + xx, _src_pitch_arr [0],
				ref_data_ptr_arr, pitch_arr, weight_arr, _trad
			);

			xx += (nBlkSizeX);

			if (bx == nBlkX - 1 && _covered_width < nWidth) // right non-covered region
			{
				// luma
				BitBlt (
					pDstCur + _covered_width, _dst_pitch_arr [0],
					pSrcCur + _covered_width, _src_pitch_arr [0],
					nWidth - _covered_width, nBlkSizeY, _isse_flag);
			}
		}	// for bx

		pDstCur += rowsize * _dst_pitch_arr [0];
		pSrcCur += rowsize * _src_pitch_arr [0];

		if (by == nBlkY - 1 && _covered_height < nHeight) // bottom uncovered region
		{
			// luma
			BitBlt (
				pDstCur, _dst_pitch_arr [0],
				pSrcCur, _src_pitch_arr [0],
				nWidth, nHeight - _covered_height, _isse_flag
			);
		}
	}	// for by
}



void	MDegrainN::process_luma_overlap_slice (Slicer::TaskData &td)
{
	assert (&td != 0);

	if (   nOverlapY == 0
	    || (td._y_beg == 0 && td._y_end == nBlkY))
	{
		process_luma_overlap_slice (td._y_beg, td._y_end);
	}

	else
	{
		assert (td._y_end - td._y_beg >= 2);

		process_luma_overlap_slice (td._y_beg, td._y_end - 1);

		const conc::AioAdd <int>	inc_ftor (+1);

		const int		cnt_top = conc::AtomicIntOp::exec_new (
			_boundary_cnt_arr [td._y_beg],
			inc_ftor
		);
		if (td._y_beg > 0 && cnt_top == 2)
		{
			process_luma_overlap_slice (td._y_beg - 1, td._y_beg);
		}

		int				cnt_bot = 2;
		if (td._y_end < nBlkY)
		{
			cnt_bot = conc::AtomicIntOp::exec_new (
				_boundary_cnt_arr [td._y_end],
				inc_ftor
			);
		}
		if (cnt_bot == 2)
		{
			process_luma_overlap_slice (td._y_end - 1, td._y_end);
		}
	}
}



void	MDegrainN::process_luma_overlap_slice (int y_beg, int y_end)
{
	TmpBlock       tmp_block;

	const int      rowsize = nBlkSizeY - nOverlapY;
	const BYTE *   pSrcCur = _src_ptr_arr [0] + y_beg * rowsize * _src_pitch_arr [0];

	unsigned short *	pDstShort = (_dst_short.empty ()) ? 0 : &_dst_short [0] + y_beg * rowsize * _dst_short_pitch;
	int *					pDstInt   = (_dst_int.empty ()  ) ? 0 : &_dst_int [0]   + y_beg * rowsize * _dst_int_pitch;
	const int			tmpPitch  = nBlkSizeX;
	assert (tmpPitch <= TmpBlock::MAX_SIZE);

	for (int by = y_beg; by < y_end; ++by)
	{
		int				wby = ((by + nBlkY - 3) / (nBlkY - 2)) * 3;
		int				xx  = 0;
		for (int bx = 0; bx < nBlkX; ++bx)
		{
			// select window
			int				wbx = (bx + nBlkX - 3) / (nBlkX - 2);
			short *			winOver = _overwins->GetWindow (wby + wbx);

			int				i = by * nBlkX + bx;
			const BYTE *	ref_data_ptr_arr [MAX_TEMP_RAD * 2];
			int				pitch_arr [MAX_TEMP_RAD * 2];
			int				weight_arr [1 + MAX_TEMP_RAD * 2];

			for (int k = 0; k < _trad * 2; ++k)
			{
				use_block_y (
					ref_data_ptr_arr [k],
					pitch_arr [k],
					weight_arr [k + 1],
					_usable_flag_arr [k],
					_mv_clip_arr [k],
					i,
					_planes_ptr [k] [0],
					pSrcCur,
					xx,
					_src_pitch_arr [0]
				);
			}

			norm_weights (weight_arr, _trad);

			// luma
			_degrainluma_ptr (
				&tmp_block._d [0], tmp_block._lsb_ptr, _lsb_flag, tmpPitch,
				pSrcCur + xx, _src_pitch_arr [0],
				ref_data_ptr_arr, pitch_arr, weight_arr, _trad
			);
			if (_lsb_flag)
			{
				_oversluma_lsb_ptr (
					pDstInt + xx, _dst_int_pitch,
					&tmp_block._d [0], tmp_block._lsb_ptr, tmpPitch,
					winOver, nBlkSizeX
				);
			}
			else
			{
				_oversluma_ptr (
					pDstShort + xx, _dst_short_pitch,
					&tmp_block._d [0], tmpPitch,
					winOver, nBlkSizeX
				);
			}

			xx += nBlkSizeX - nOverlapX;
		}	// for bx

		pSrcCur   += rowsize * _src_pitch_arr [0];
		pDstShort += rowsize * _dst_short_pitch;
		pDstInt   += rowsize * _dst_int_pitch;
	}	// for by
}



template <int P>
void	MDegrainN::process_chroma_normal_slice (Slicer::TaskData &td)
{
	assert (&td != 0);

	const int		rowsize = nBlkSizeY >> _yratiouv_log;
	BYTE *			pDstCur = _dst_ptr_arr [P] + td._y_beg * rowsize * _dst_pitch_arr [P];
	const BYTE *	pSrcCur = _src_ptr_arr [P] + td._y_beg * rowsize * _src_pitch_arr [P];

	for (int by = td._y_beg; by < td._y_end; ++by)
	{
		int				xx = 0;
		for (int bx = 0; bx < nBlkX; ++bx)
		{
			int				i = by * nBlkX + bx;
			const BYTE *	ref_data_ptr_arr [MAX_TEMP_RAD * 2];
			int				pitch_arr [MAX_TEMP_RAD * 2];
			int				weight_arr [1 + MAX_TEMP_RAD * 2];

			for (int k = 0; k < _trad * 2; ++k)
			{
				use_block_uv (
					ref_data_ptr_arr [k],
					pitch_arr [k],
					weight_arr [k + 1],
					_usable_flag_arr [k],
					_mv_clip_arr [k],
					i,
					_planes_ptr [k] [P],
					pSrcCur,
					xx,
					_src_pitch_arr [P]
				);
			}

			norm_weights (weight_arr, _trad);

			// chroma
			_degrainchroma_ptr (
				pDstCur + (xx >> 1),
				pDstCur + (xx >> 1) + _lsb_offset_arr [P], _lsb_flag, _dst_pitch_arr [P],
				pSrcCur + (xx >> 1),                                  _src_pitch_arr [P],
				ref_data_ptr_arr, pitch_arr, weight_arr, _trad
			);

			xx += nBlkSizeX;

			if (bx == nBlkX - 1 && _covered_width < nWidth) // right non-covered region
			{
				// chroma
				BitBlt (
					pDstCur + (_covered_width >> 1), _dst_pitch_arr [P],
					pSrcCur + (_covered_width >> 1), _src_pitch_arr [P],
					(nWidth - _covered_width) >> 1, rowsize,
					_isse_flag
				);
			}
		}	// for bx

		pDstCur += rowsize * _dst_pitch_arr [P];
		pSrcCur += rowsize * _src_pitch_arr [P];

		if (by == nBlkY - 1 && _covered_height < nHeight) // bottom uncovered region
		{
			// chroma
			BitBlt (
				pDstCur, _dst_pitch_arr [P],
				pSrcCur, _src_pitch_arr [P],
				nWidth >> 1, (nHeight - _covered_height) >> _yratiouv_log,
				_isse_flag
			);
		}
	}	// for by
}



template <int P>
void	MDegrainN::process_chroma_overlap_slice (Slicer::TaskData &td)
{
	assert (&td != 0);

	if (   nOverlapY == 0
	    || (td._y_beg == 0 && td._y_end == nBlkY))
	{
		process_chroma_overlap_slice <P> (td._y_beg, td._y_end);
	}

	else
	{
		assert (td._y_end - td._y_beg >= 2);

		process_chroma_overlap_slice <P> (td._y_beg, td._y_end - 1);

		const conc::AioAdd <int>	inc_ftor (+1);

		const int		cnt_top = conc::AtomicIntOp::exec_new (
			_boundary_cnt_arr [td._y_beg],
			inc_ftor
		);
		if (td._y_beg > 0 && cnt_top == 2)
		{
			process_chroma_overlap_slice <P> (td._y_beg - 1, td._y_beg);
		}

		int				cnt_bot = 2;
		if (td._y_end < nBlkY)
		{
			cnt_bot = conc::AtomicIntOp::exec_new (
				_boundary_cnt_arr [td._y_end],
				inc_ftor
			);
		}
		if (cnt_bot == 2)
		{
			process_chroma_overlap_slice <P> (td._y_end - 1, td._y_end);
		}
	}
}



template <int P>
void	MDegrainN::process_chroma_overlap_slice (int y_beg, int y_end)
{
	TmpBlock       tmp_block;

	const int		rowsize = (nBlkSizeY - nOverlapY) >> _yratiouv_log;
	const BYTE *	pSrcCur = _src_ptr_arr [P] + y_beg * rowsize * _src_pitch_arr [P];

	unsigned short *	pDstShort = (_dst_short.empty ()) ? 0 : &_dst_short [0] + y_beg * rowsize * _dst_short_pitch;
	int *					pDstInt   = (_dst_int.empty ()  ) ? 0 : &_dst_int [0]   + y_beg * rowsize * _dst_int_pitch;
	const int			tmpPitch  = nBlkSizeX;
	assert (tmpPitch <= TmpBlock::MAX_SIZE);

	for (int by = y_beg; by < y_end; ++by)
	{
		int				wby = ((by + nBlkY - 3) / (nBlkY - 2)) * 3;
		int				xx  = 0;
		for (int bx = 0; bx < nBlkX; ++bx)
		{
			// select window
			int				wbx = (bx + nBlkX - 3) / (nBlkX - 2);
			short *			winOverUV = _overwins_uv->GetWindow (wby + wbx);

			int				i = by * nBlkX + bx;
			const BYTE *	ref_data_ptr_arr [MAX_TEMP_RAD * 2];
			int				pitch_arr [MAX_TEMP_RAD * 2];
			int				weight_arr [1 + MAX_TEMP_RAD * 2];

			for (int k = 0; k < _trad * 2; ++k)
			{
				use_block_uv (
					ref_data_ptr_arr [k],
					pitch_arr [k],
					weight_arr [k + 1],
					_usable_flag_arr [k],
					_mv_clip_arr [k],
					i,
					_planes_ptr [k] [P],
					pSrcCur,
					xx,
					_src_pitch_arr [P]
				);
			}

			norm_weights (weight_arr, _trad);

			// chroma
			_degrainchroma_ptr (
				&tmp_block._d [0], tmp_block._lsb_ptr, _lsb_flag, tmpPitch,
				pSrcCur + (xx >> 1),                              _src_pitch_arr [P],
				ref_data_ptr_arr, pitch_arr, weight_arr, _trad
			);
			if (_lsb_flag)
			{
				_overschroma_lsb_ptr (
					pDstInt + (xx >> 1),                   _dst_int_pitch,
					&tmp_block._d [0], tmp_block._lsb_ptr, tmpPitch,
					winOverUV, nBlkSizeX >> 1
				);
			}
			else
			{
				_overschroma_ptr (
					pDstShort + (xx >> 1), _dst_short_pitch,
					&tmp_block._d [0],     tmpPitch,
					winOverUV, nBlkSizeX >> 1);
			}

			xx += nBlkSizeX - nOverlapX;

		}	// for bx

		pSrcCur   += rowsize * _src_pitch_arr [P];
		pDstShort += rowsize * _dst_short_pitch;
		pDstInt   += rowsize * _dst_int_pitch;
	}	// for by
}



void	MDegrainN::use_block_y (
	const BYTE * &p, int &np, int &wref, bool usable_flag, const MvClipInfo &c_info,
	int i, const MVPlane *plane_ptr, const BYTE *src_ptr, int xx, int src_pitch
)
{
	if (usable_flag)
	{
		const FakeBlockData &	block = c_info._clip_sptr->GetBlock (0, i);
		const int	blx = block.GetX () * nPel + block.GetMV ().x;
		const int	bly = block.GetY () * nPel + block.GetMV ().y;
		p    = plane_ptr->GetPointer (blx, bly);
		np   = plane_ptr->GetPitch ();
		const int	block_sad = block.GetSAD ();
		wref = DegrainWeight (c_info._thsad, block_sad);
	}
	else
	{
		p    = src_ptr + xx;
		np   = src_pitch;
		wref = 0;
	}
}



void	MDegrainN::use_block_uv (
	const BYTE * &p, int &np, int &wref, bool usable_flag, const MvClipInfo &c_info,
	int i, const MVPlane *plane_ptr, const BYTE *src_ptr, int xx, int src_pitch
)
{
	if (usable_flag)
	{
		const FakeBlockData &	block = c_info._clip_sptr->GetBlock (0, i);
		const int	blx = block.GetX () * nPel + block.GetMV ().x;
		const int	bly = block.GetY () * nPel + block.GetMV ().y;
		p    = plane_ptr->GetPointer (blx >> 1, bly >> _yratiouv_log);
		np   = plane_ptr->GetPitch();
		const int	block_sad = block.GetSAD();
		wref = DegrainWeight (c_info._thsadc, block_sad);
	}
	else
	{
		p    = src_ptr + (xx >> 1);
		np   = src_pitch;
		wref = 0;
	}
}



void	MDegrainN::norm_weights (int wref_arr [], int trad)
{
	const int		nbr_frames = trad * 2 + 1;

	wref_arr [0] = 256;
	int				wsum = 1;
	for (int k = 0; k < nbr_frames; ++k)
	{
		wsum += wref_arr [k];
	}

	// normalize weights to 256
	int				wsrc = 256;
	for (int k = 1; k < nbr_frames; ++k)
	{
		const int		norm = wref_arr [k] * 256 / wsum;
		wref_arr [k] = norm;
		wsrc -= norm;
	}
	wref_arr [0] = wsrc;
}

