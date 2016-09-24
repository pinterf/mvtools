#ifndef __MV_DEGRAIN2__
#define __MV_DEGRAIN2__

#include "CopyCode.h"
#include "MVClip.h"
#include "MVFilter.h"
#include "overlap.h"
#include "yuy2planes.h"





class MVGroupOfFrames;
class MVPlane;

/*! \brief Filter that denoise the picture
 */
class MVDegrain2
:	public GenericVideoFilter
,	public MVFilter
{
private:
    typedef void (Denoise2Function) (
        BYTE *pDst, BYTE *pDstLsb, bool lsb_flag, int nDstPitch, const BYTE *pSrc, int nSrcPitch,
        const BYTE *pRefB, int BPitch, const BYTE *pRefF, int FPitch,
        const BYTE *pRefB2, int B2Pitch, const BYTE *pRefF2, int F2Pitch,
        int WSrc, int WRefB, int WRefF, int WRefB2, int WRefF2
        );

   MVClip mvClipB;
   MVClip mvClipF;
   MVClip mvClipB2;
   MVClip mvClipF2;
   int thSAD;
   int thSADC;
   int YUVplanes;
   int nLimit;
   int nLimitC;
   bool isse2;
   bool planar;
	bool lsb_flag;
	int height_lsb_mul;
    //int pixelsize; // in MVFilter
    int pixelsize_super; // PF not param, from create

   PClip super; // v2.0
   int nSuperModeYUV;
	MVGroupOfFrames *pRefBGOF, *pRefFGOF;
	MVGroupOfFrames *pRefB2GOF, *pRefF2GOF;

	YUY2Planes * DstPlanes;
	YUY2Planes * SrcPlanes;

	OverlapWindows *OverWins;
	OverlapWindows *OverWinsUV;

	OverlapsFunction *OVERSLUMA;
	OverlapsFunction *OVERSCHROMA;
	OverlapsLsbFunction *OVERSLUMALSB;
	OverlapsLsbFunction *OVERSCHROMALSB;
	Denoise2Function *DEGRAINLUMA;
	Denoise2Function *DEGRAINCHROMA;

	unsigned char *tmpBlock;
	unsigned char *tmpBlockLsb;	// Not allocated, it's just a reference to a part of the tmpBlock area (or 0 if no LSB)
	unsigned short * DstShort;
	int dstShortPitch;
	int * DstInt;
	int dstIntPitch;

public:
	MVDegrain2(
		PClip _child, PClip _super, PClip _mvbw2, PClip _mvbw, PClip _mvfw, PClip _mvfw2,
		int _thSAD, int _thSADC, int _YUVplanes, int _nLimit, int _nLimitC,
		int nSCD1, int nSCD2, bool _isse2, bool _planar, bool _lsb_flag,
		bool mt_flag, IScriptEnvironment* env
	);
	~MVDegrain2();
	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);

private:
	inline void	process_chroma (int plane_mask, BYTE *pDst, BYTE *pDstCur, int nDstPitch, const BYTE *pSrc, const BYTE *pSrcCur, int nSrcPitch, bool isUsableB, bool isUsableF, bool isUsableB2, bool isUsableF2, MVPlane *pPlanesB, MVPlane *pPlanesF, MVPlane *pPlanesB2, MVPlane *pPlanesF2, int lsb_offset_uv, int nWidth_B, int nHeight_B);
	inline void	use_block_y (const BYTE * &p, int &np, int &WRef, bool isUsable, const MVClip &mvclip, int i, const MVPlane *pPlane, const BYTE *pSrcCur, int xx, int nSrcPitch);
	inline void	use_block_uv (const BYTE * &p, int &np, int &WRef, bool isUsable, const MVClip &mvclip, int i, const MVPlane *pPlane, const BYTE *pSrcCur, int xx, int nSrcPitch);
	static inline void	norm_weights (int &WSrc, int &WRefB, int &WRefF, int &WRefB2, int &WRefF2);
    Denoise2Function* get_denoise2_function(int BlockX, int BlockY, int pixelsize, arch_t arch);
};

template<int blockWidth, int blockHeight>
void Degrain2_C(BYTE *pDst, BYTE *pDstLsb, bool lsb_flag, int nDstPitch, const BYTE *pSrc, int nSrcPitch,
						const BYTE *pRefB, int BPitch, const BYTE *pRefF, int FPitch,
						const BYTE *pRefB2, int B2Pitch, const BYTE *pRefF2, int F2Pitch,
						int WSrc, int WRefB, int WRefF, int WRefB2, int WRefF2)
{
	if (lsb_flag)
	{
		for (int h=0; h<blockHeight; h++)
		{
			for (int x=0; x<blockWidth; x++)
			{
				const int		val = pRefF[x]*WRefF + pSrc[x]*WSrc + pRefB[x]*WRefB + pRefF2[x]*WRefF2 + pRefB2[x]*WRefB2;
				pDst[x]    = val >> 8;
				pDstLsb[x] = val & 255;
			}
			pDst += nDstPitch;
			pDstLsb += nDstPitch;
			pSrc += nSrcPitch;
			pRefB += BPitch;
			pRefF += FPitch;
			pRefB2 += B2Pitch;
			pRefF2 += F2Pitch;
		}
	}

	else
	{
		for (int h=0; h<blockHeight; h++)
		{
			for (int x=0; x<blockWidth; x++)
			{
				pDst[x] = (pRefF[x]*WRefF + pSrc[x]*WSrc + pRefB[x]*WRefB + pRefF2[x]*WRefF2 + pRefB2[x]*WRefB2 + 128)>>8;
			}
			pDst += nDstPitch;
			pSrc += nSrcPitch;
			pRefB += BPitch;
			pRefF += FPitch;
			pRefB2 += B2Pitch;
			pRefF2 += F2Pitch;
		}
	}
}

#ifndef _M_X64

#include <mmintrin.h>
template<int blockWidth, int blockHeight>
void Degrain2_mmx(BYTE *pDst, BYTE *pDstLsb, bool lsb_flag, int nDstPitch, const BYTE *pSrc, int nSrcPitch,
						const BYTE *pRefB, int BPitch, const BYTE *pRefF, int FPitch,
						const BYTE *pRefB2, int B2Pitch, const BYTE *pRefF2, int F2Pitch,
						int WSrc, int WRefB, int WRefF, int WRefB2, int WRefF2)
{
	__m64 z = _mm_setzero_si64();
	__m64 ws = _mm_set1_pi16(WSrc);
	__m64 wb1 = _mm_set1_pi16(WRefB);
	__m64 wf1 = _mm_set1_pi16(WRefF);
	__m64 wb2 = _mm_set1_pi16(WRefB2);
	__m64 wf2 = _mm_set1_pi16(WRefF2);

	if (lsb_flag)
	{
		__m64 m = _mm_set1_pi16(255);
		for (int h=0; h<blockHeight; h++)
		{
			for (int x=0; x<blockWidth; x+=4)
			{
				const __m64		val =
					_m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pSrc   + x), z), ws),
					_m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefB  + x), z), wb1),
					_m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefF  + x), z), wf1),
					_m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefB2 + x), z), wb2),
					         _m_pmullw(_m_punpcklbw(*(__m64*)(pRefF2 + x), z), wf2)))));
				*(int*)(pDst    + x) = _m_to_int(_m_packuswb(_m_psrlwi    (val, 8), z));
				*(int*)(pDstLsb + x) = _m_to_int(_m_packuswb(_mm_and_si64 (val, m), z));
			}
			pDst += nDstPitch;
			pDstLsb += nDstPitch;
			pSrc += nSrcPitch;
			pRefB += BPitch;
			pRefF += FPitch;
			pRefB2 += B2Pitch;
			pRefF2 += F2Pitch;
		}
	}

	else
	{
		__m64 o = _mm_set1_pi16(128);
		for (int h=0; h<blockHeight; h++)
		{
			for (int x=0; x<blockWidth; x+=4)
			{
				 *(int*)(pDst + x) = _m_to_int(_m_packuswb(_m_psrlwi(
					 _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pSrc   + x), z), ws),
					 _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefB  + x), z), wb1),
					 _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefF  + x), z), wf1),
					 _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefB2 + x), z), wb2),
					 _m_paddw(_m_pmullw(_m_punpcklbw(*(__m64*)(pRefF2 + x), z), wf2),
					 o))))), 8), z));
//			 pDst[x] = (pRefF[x]*WRefF + pSrc[x]*WSrc + pRefB[x]*WRefB + pRefF2[x]*WRefF2 + pRefB2[x]*WRefB2 + 128)>>8;
			}
			pDst += nDstPitch;
			pSrc += nSrcPitch;
			pRefB += BPitch;
			pRefF += FPitch;
			pRefB2 += B2Pitch;
			pRefF2 += F2Pitch;
		}
	}

	_m_empty();
}
#endif

#include <emmintrin.h>
template<int blockWidth, int blockHeight>
void Degrain2_sse2(BYTE *pDst, BYTE *pDstLsb, bool lsb_flag, int nDstPitch, const BYTE *pSrc, int nSrcPitch,
						const BYTE *pRefB, int BPitch, const BYTE *pRefF, int FPitch,
						const BYTE *pRefB2, int B2Pitch, const BYTE *pRefF2, int F2Pitch,
						int WSrc, int WRefB, int WRefF, int WRefB2, int WRefF2)
{
	__m128i z = _mm_setzero_si128();
	__m128i ws = _mm_set1_epi16(WSrc);
	__m128i wb1 = _mm_set1_epi16(WRefB);
	__m128i wf1 = _mm_set1_epi16(WRefF);
	__m128i wb2 = _mm_set1_epi16(WRefB2);
	__m128i wf2 = _mm_set1_epi16(WRefF2);

	if (lsb_flag)
	{
		__m128i m = _mm_set1_epi16(255);
		for (int h=0; h<blockHeight; h++)
		{
			for (int x=0; x<blockWidth; x+=8)
			{
				const __m128i	val =
					_mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pSrc   + x)), z), ws),
					_mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB  + x)), z), wb1),
					_mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF  + x)), z), wf1),
					_mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB2 + x)), z), wb2),
					              _mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF2 + x)), z), wf2)))));
				_mm_storel_epi64((__m128i*)(pDst    + x), _mm_packus_epi16(_mm_srli_epi16(val, 8), z));
				_mm_storel_epi64((__m128i*)(pDstLsb + x), _mm_packus_epi16(_mm_and_si128 (val, m), z));
			}
			pDst += nDstPitch;
			pDstLsb += nDstPitch;
			pSrc += nSrcPitch;
			pRefB += BPitch;
			pRefF += FPitch;
			pRefB2 += B2Pitch;
			pRefF2 += F2Pitch;
		}
	}

	else
	{
		__m128i o = _mm_set1_epi16(128);
		for (int h=0; h<blockHeight; h++)
		{
			for (int x=0; x<blockWidth; x+=8)
			{
				 _mm_storel_epi64((__m128i*)(pDst + x), _mm_packus_epi16(_mm_srli_epi16(
					 _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pSrc   + x)), z), ws),
					 _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB  + x)), z), wb1),
					 _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF  + x)), z), wf1),
					 _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefB2 + x)), z), wb2),
					 _mm_add_epi16(_mm_mullo_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64((__m128i*)(pRefF2 + x)), z), wf2),
					 o))))), 8), z));
//			 pDst[x] = (pRefF[x]*WRefF + pSrc[x]*WSrc + pRefB[x]*WRefB + pRefF2[x]*WRefF2 + pRefB2[x]*WRefB2 + 128)>>8;
			}
			pDst += nDstPitch;
			pSrc += nSrcPitch;
			pRefB += BPitch;
			pRefF += FPitch;
			pRefB2 += B2Pitch;
			pRefF2 += F2Pitch;
		}
	}
}

#endif
