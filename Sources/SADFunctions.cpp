#include "SADFunctions.h"
/*
A change in interface can be covered like this at least until release

unsigned int Sad16x16_wrap(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch){
	return Sad16x16_sse2(pSrc,nSrcPitch,pRef,nRefPitch);
}

#define MK_CPPWRAP(blkx, blky) unsigned int  Sad##blkx##x##blky##_wrap(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch){return Sad##blkx##x##blky##_iSSE(pSrc, nSrcPitch, pRef, nRefPitch);}
//MK_CPPWRAP(16,16);
MK_CPPWRAP(16,8);
MK_CPPWRAP(16,2);
MK_CPPWRAP(8,16);
MK_CPPWRAP(8,8);
MK_CPPWRAP(8,4);
MK_CPPWRAP(8,2);
MK_CPPWRAP(8,1);
MK_CPPWRAP(4,8);
MK_CPPWRAP(4,4);
MK_CPPWRAP(4,2);
MK_CPPWRAP(2,4);
MK_CPPWRAP(2,2);
#undef MK_CPPWRAP
*/


// SATD functions for blocks over 16x16 are not defined in pixel-a.asm,
// so as a poor man's substitute, we use a sum of smaller SATD functions.
#define	SATD_REC_FUNC(blsizex, blsizey, sblx, sbly, type)	extern "C" unsigned int __cdecl	x264_pixel_satd_##blsizex##x##blsizey##_##type (const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch)	\
{	\
	const int		sum =	\
		  x264_pixel_satd_##sblx##x##sbly##_##type (pSrc,                     nSrcPitch, pRef,                     nRefPitch)	\
		+ x264_pixel_satd_##sblx##x##sbly##_##type (pSrc+sblx,                nSrcPitch, pRef+sblx,                nRefPitch)	\
		+ x264_pixel_satd_##sblx##x##sbly##_##type (pSrc     +sbly*nSrcPitch, nSrcPitch, pRef     +sbly*nRefPitch, nRefPitch)	\
		+ x264_pixel_satd_##sblx##x##sbly##_##type (pSrc+sblx+sbly*nSrcPitch, nSrcPitch, pRef+sblx+sbly*nRefPitch, nRefPitch);	\
	return (sum);	\
}

SATD_REC_FUNC (32, 32, 16, 16, mmx2)
SATD_REC_FUNC (32, 16, 16,  8, mmx2)
SATD_REC_FUNC (32, 32, 16, 16, sse2)
SATD_REC_FUNC (32, 16, 16,  8, sse2)
SATD_REC_FUNC (32, 32, 16, 16, ssse3)
SATD_REC_FUNC (32, 16, 16,  8, ssse3)

#undef SATD_REC_FUNC
