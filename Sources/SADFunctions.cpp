#include "SADFunctions.h"
#include "overlap.h"
#include <map>
#include <tuple>
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

SADFunction* get_sad_function(int BlockX, int BlockY, int pixelsize, arch_t arch)
{
    // BlkSizeX, BlkSizeY, pixelsize, arch_t
    std::map<std::tuple<int, int, int, arch_t>, SADFunction*> func_sad;
    using std::make_tuple;

    func_sad[make_tuple(32, 32, 1, NO_SIMD)] = Sad_C<32, 32,uint8_t>;
    func_sad[make_tuple(32, 16, 1, NO_SIMD)] = Sad_C<32, 16,uint8_t>;
    func_sad[make_tuple(32, 8 , 1, NO_SIMD)] = Sad_C<32, 8,uint8_t>;
    func_sad[make_tuple(16, 32, 1, NO_SIMD)] = Sad_C<16, 32,uint8_t>;
    func_sad[make_tuple(16, 16, 1, NO_SIMD)] = Sad_C<16, 16,uint8_t>;
    func_sad[make_tuple(16, 8 , 1, NO_SIMD)] = Sad_C<16, 8,uint8_t>;
    func_sad[make_tuple(16, 4 , 1, NO_SIMD)] = Sad_C<16, 4,uint8_t>;
    func_sad[make_tuple(16, 2 , 1, NO_SIMD)] = Sad_C<16, 2,uint8_t>;
    func_sad[make_tuple(16, 1 , 1, NO_SIMD)] = Sad_C<16, 1,uint8_t>;
    func_sad[make_tuple(8 , 16, 1, NO_SIMD)] = Sad_C<8 , 16,uint8_t>;
    func_sad[make_tuple(8 , 8 , 1, NO_SIMD)] = Sad_C<8 , 8,uint8_t>;
    func_sad[make_tuple(8 , 4 , 1, NO_SIMD)] = Sad_C<8 , 4,uint8_t>;
    func_sad[make_tuple(8 , 2 , 1, NO_SIMD)] = Sad_C<8 , 2,uint8_t>;
    func_sad[make_tuple(8 , 1 , 1, NO_SIMD)] = Sad_C<8 , 1,uint8_t>;
    func_sad[make_tuple(4 , 8 , 1, NO_SIMD)] = Sad_C<4 , 8,uint8_t>;
    func_sad[make_tuple(4 , 4 , 1, NO_SIMD)] = Sad_C<4 , 4,uint8_t>;
    func_sad[make_tuple(4 , 2 , 1, NO_SIMD)] = Sad_C<4 , 2,uint8_t>;
    func_sad[make_tuple(4 , 1 , 1, NO_SIMD)] = Sad_C<4 , 1,uint8_t>;
    func_sad[make_tuple(2 , 4 , 1, NO_SIMD)] = Sad_C<2 , 4,uint8_t>;
    func_sad[make_tuple(2 , 2 , 1, NO_SIMD)] = Sad_C<2 , 2,uint8_t>;
    func_sad[make_tuple(2 , 1 , 1, NO_SIMD)] = Sad_C<2 , 1,uint8_t>;

    func_sad[make_tuple(32, 32, 2, NO_SIMD)] = Sad_C<32, 32,uint16_t>;
    func_sad[make_tuple(32, 16, 2, NO_SIMD)] = Sad_C<32, 16,uint16_t>;
    func_sad[make_tuple(32, 8 , 2, NO_SIMD)] = Sad_C<32, 8,uint16_t>;
    func_sad[make_tuple(16, 32, 2, NO_SIMD)] = Sad_C<16, 32,uint16_t>;
    func_sad[make_tuple(16, 16, 2, NO_SIMD)] = Sad_C<16, 16,uint16_t>;
    func_sad[make_tuple(16, 8 , 2, NO_SIMD)] = Sad_C<16, 8,uint16_t>;
    func_sad[make_tuple(16, 4 , 2, NO_SIMD)] = Sad_C<16, 4,uint16_t>;
    func_sad[make_tuple(16, 2 , 2, NO_SIMD)] = Sad_C<16, 2,uint16_t>;
    func_sad[make_tuple(16, 1 , 2, NO_SIMD)] = Sad_C<16, 1,uint16_t>;
    func_sad[make_tuple(8 , 16, 2, NO_SIMD)] = Sad_C<8 , 16,uint16_t>;
    func_sad[make_tuple(8 , 8 , 2, NO_SIMD)] = Sad_C<8 , 8,uint16_t>;
    func_sad[make_tuple(8 , 4 , 2, NO_SIMD)] = Sad_C<8 , 4,uint16_t>;
    func_sad[make_tuple(8 , 2 , 2, NO_SIMD)] = Sad_C<8 , 2,uint16_t>;
    func_sad[make_tuple(8 , 1 , 2, NO_SIMD)] = Sad_C<8 , 1,uint16_t>;
    func_sad[make_tuple(4 , 8 , 2, NO_SIMD)] = Sad_C<4 , 8,uint16_t>;
    func_sad[make_tuple(4 , 4 , 2, NO_SIMD)] = Sad_C<4 , 4,uint16_t>;
    func_sad[make_tuple(4 , 2 , 2, NO_SIMD)] = Sad_C<4 , 2,uint16_t>;
    func_sad[make_tuple(4 , 1 , 2, NO_SIMD)] = Sad_C<4 , 1,uint16_t>;
    func_sad[make_tuple(2 , 4 , 2, NO_SIMD)] = Sad_C<2 , 4,uint16_t>;
    func_sad[make_tuple(2 , 2 , 2, NO_SIMD)] = Sad_C<2 , 2,uint16_t>;
    func_sad[make_tuple(2 , 1 , 2, NO_SIMD)] = Sad_C<2 , 1,uint16_t>;
    
    // PF SAD 16 SIMD intrinsic functions
    // only for >=8 bytes widths
    func_sad[make_tuple(32, 32, 2, USE_SSE2)] = Sad16_sse2<32, 32,uint16_t>;
    func_sad[make_tuple(32, 16, 2, USE_SSE2)] = Sad16_sse2<32, 16,uint16_t>;
    func_sad[make_tuple(32, 8 , 2, USE_SSE2)] = Sad16_sse2<32, 8,uint16_t>;
    func_sad[make_tuple(16, 32, 2, USE_SSE2)] = Sad16_sse2<16, 32,uint16_t>;
    func_sad[make_tuple(16, 16, 2, USE_SSE2)] = Sad16_sse2<16, 16,uint16_t>;
    func_sad[make_tuple(16, 8 , 2, USE_SSE2)] = Sad16_sse2<16, 8,uint16_t>;
    func_sad[make_tuple(16, 4 , 2, USE_SSE2)] = Sad16_sse2<16, 4,uint16_t>;
    func_sad[make_tuple(16, 2 , 2, USE_SSE2)] = Sad16_sse2<16, 2,uint16_t>;
    func_sad[make_tuple(16, 1 , 2, USE_SSE2)] = Sad16_sse2<16, 1,uint16_t>;
    func_sad[make_tuple(8 , 16, 2, USE_SSE2)] = Sad16_sse2<8 , 16,uint16_t>;
    func_sad[make_tuple(8 , 8 , 2, USE_SSE2)] = Sad16_sse2<8 , 8,uint16_t>;
    func_sad[make_tuple(8 , 4 , 2, USE_SSE2)] = Sad16_sse2<8 , 4,uint16_t>;
    func_sad[make_tuple(8 , 2 , 2, USE_SSE2)] = Sad16_sse2<8 , 2,uint16_t>;
    func_sad[make_tuple(8 , 1 , 2, USE_SSE2)] = Sad16_sse2<8 , 1,uint16_t>;
    func_sad[make_tuple(4 , 8 , 2, USE_SSE2)] = Sad16_sse2<4 , 8,uint16_t>;
    func_sad[make_tuple(4 , 4 , 2, USE_SSE2)] = Sad16_sse2<4 , 4,uint16_t>;
    func_sad[make_tuple(4 , 2 , 2, USE_SSE2)] = Sad16_sse2<4 , 2,uint16_t>;

    // PF uint8_t sse2 versions. test. 
    // a bit slower than the existing external asm. At least for MSVC
    // >=8 bytes
#ifdef SAD_8BIT_INSTINSICS
    func_sad[make_tuple(32, 32, 1, USE_SSE2)] = Sad16_sse2<32, 32,uint8_t>;
    func_sad[make_tuple(32, 16, 1, USE_SSE2)] = Sad16_sse2<32, 16,uint8_t>;
    func_sad[make_tuple(32, 8 , 1, USE_SSE2)] = Sad16_sse2<32, 8,uint8_t>;
    func_sad[make_tuple(16, 32, 1, USE_SSE2)] = Sad16_sse2<16, 32,uint8_t>;
    func_sad[make_tuple(16, 16, 1, USE_SSE2)] = Sad16_sse2<16, 16,uint8_t>;
    func_sad[make_tuple(16, 8 , 1, USE_SSE2)] = Sad16_sse2<16, 8,uint8_t>;
    func_sad[make_tuple(16, 4 , 1, USE_SSE2)] = Sad16_sse2<16, 4,uint8_t>;
    func_sad[make_tuple(16, 2 , 1, USE_SSE2)] = Sad16_sse2<16, 2,uint8_t>;
    func_sad[make_tuple(16, 1 , 1, USE_SSE2)] = Sad16_sse2<16, 1,uint8_t>;
    func_sad[make_tuple(8 , 16, 1, USE_SSE2)] = Sad16_sse2<8 , 16,uint8_t>;
    func_sad[make_tuple(8 , 8 , 1, USE_SSE2)] = Sad16_sse2<8 , 8,uint8_t>;
    func_sad[make_tuple(8 , 4 , 1, USE_SSE2)] = Sad16_sse2<8 , 4,uint8_t>;
    func_sad[make_tuple(8 , 2 , 1, USE_SSE2)] = Sad16_sse2<8 , 2,uint8_t>;
    func_sad[make_tuple(8 , 1 , 1, USE_SSE2)] = Sad16_sse2<8 , 1,uint8_t>;
#else
    func_sad[make_tuple(32, 32, 1, USE_SSE2)] = Sad32x32_iSSE;
    func_sad[make_tuple(32, 16, 1, USE_SSE2)] = Sad32x16_iSSE;
    func_sad[make_tuple(32, 8 , 1, USE_SSE2)] = Sad32x8_iSSE;
    func_sad[make_tuple(16, 32, 1, USE_SSE2)] = Sad16x32_iSSE;
    func_sad[make_tuple(16, 16, 1, USE_SSE2)] = Sad16x16_iSSE;
    func_sad[make_tuple(16, 8 , 1, USE_SSE2)] = Sad16x8_iSSE;
    func_sad[make_tuple(16, 4 , 1, USE_SSE2)] = Sad16x4_iSSE;
    func_sad[make_tuple(16, 2 , 1, USE_SSE2)] = Sad16x2_iSSE;
    func_sad[make_tuple(8 , 16, 1, USE_SSE2)] = Sad8x16_iSSE;
    func_sad[make_tuple(8 , 8 , 1, USE_SSE2)] = Sad8x8_iSSE;
    func_sad[make_tuple(8 , 4 , 1, USE_SSE2)] = Sad8x4_iSSE;
    func_sad[make_tuple(8 , 2 , 1, USE_SSE2)] = Sad8x2_iSSE;
    func_sad[make_tuple(8 , 1 , 1, USE_SSE2)] = Sad8x1_iSSE;
#endif
    func_sad[make_tuple(4 , 8 , 1, USE_SSE2)] = Sad4x8_iSSE;
    func_sad[make_tuple(4 , 4 , 1, USE_SSE2)] = Sad4x4_iSSE;
    func_sad[make_tuple(4 , 2 , 1, USE_SSE2)] = Sad4x2_iSSE;
    func_sad[make_tuple(2 , 4 , 1, USE_SSE2)] = Sad2x4_iSSE;
    func_sad[make_tuple(2 , 2 , 1, USE_SSE2)] = Sad2x2_iSSE;

    SADFunction *result = func_sad[make_tuple(BlockX, BlockY, pixelsize, arch)];
    if (result == nullptr)
        result = func_sad[make_tuple(BlockX, BlockY, pixelsize, NO_SIMD)]; // fallback to C
    return result;
}

SADFunction* get_satd_function(int BlockX, int BlockY, int pixelsize, arch_t arch)
{
    // 8 bit only (pixelsize==1)
    // BlkSizeX, BlkSizeY, pixelsize, arch_t
    std::map<std::tuple<int, int, int, arch_t>, SADFunction*> func_satd;
    using std::make_tuple;

    //x264_pixel_satd_##blksizex##x##blksizey##_sse2

    func_satd[make_tuple(32, 32, 1, USE_SSE2)] = x264_pixel_satd_32x32_sse2;
    func_satd[make_tuple(32, 16, 1, USE_SSE2)] = x264_pixel_satd_32x16_sse2;
    //func_satd[make_tuple(32, 8 , 1, USE_SSE2)] = x264_pixel_satd_32x8_sse2;
    //func_satd[make_tuple(16, 32, 1, USE_SSE2)] = x264_pixel_satd_16x32_sse2;
    func_satd[make_tuple(16, 16, 1, USE_SSE2)] = x264_pixel_satd_16x16_sse2;
    func_satd[make_tuple(16, 8 , 1, USE_SSE2)] = x264_pixel_satd_16x8_sse2;
    //func_satd[make_tuple(16, 4 , 1, USE_SSE2)] = x264_pixel_satd_16x4_sse2;
    //func_satd[make_tuple(16, 2 , 1, USE_SSE2)] = x264_pixel_satd_16x2_sse2;
    func_satd[make_tuple(8 , 16, 1, USE_SSE2)] = x264_pixel_satd_8x16_sse2;
    func_satd[make_tuple(8 , 8 , 1, USE_SSE2)] = x264_pixel_satd_8x8_sse2;
    func_satd[make_tuple(8 , 4 , 1, USE_SSE2)] = x264_pixel_satd_8x4_sse2;
    //func_satd[make_tuple(8 , 2 , 1, USE_SSE2)] = x264_pixel_satd_8x2_sse2;
    //func_satd[make_tuple(8 , 1 , 1, USE_SSE2)] = x264_pixel_satd_8x1_sse2;
    //func_satd[make_tuple(4 , 8 , 1, USE_SSE2)] = x264_pixel_satd_4x8_sse2;
    //func_satd[make_tuple(4 , 4 , 1, USE_SSE2)] = x264_pixel_satd_4x4_sse2;
    //func_satd[make_tuple(4 , 2 , 1, USE_SSE2)] = x264_pixel_satd_4x2_sse2;
    //func_satd[make_tuple(2 , 4 , 1, USE_SSE2)] = x264_pixel_satd_2x4_sse2;
    //func_satd[make_tuple(2 , 2 , 1, USE_SSE2)] = x264_pixel_satd_2x2_sse2;

    SADFunction *result = func_satd[make_tuple(BlockX, BlockY, pixelsize, arch)];
    if (result == nullptr)
        result = func_satd[make_tuple(BlockX, BlockY, pixelsize, NO_SIMD)]; // fallback to C (not available yet)
    return result;
}


// SATD functions for blocks over 16x16 are not defined in pixel-a.asm,
// so as a poor man's substitute, we use a sum of smaller SATD functions.
// 8 bit only
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
