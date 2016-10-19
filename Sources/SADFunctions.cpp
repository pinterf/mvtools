#include "SADFunctions.h"
#include "SADFunctions_avx.h"
#include "SADFunctions_avx2.h"
#include "overlap.h"
#include <map>
#include <tuple>
#include <stdint.h>
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
// SATD C from x264 project common/pixel.c
#define BIT_DEPTH 16 // any >8

#if BIT_DEPTH > 8
typedef uint32_t sum_t;
typedef uint64_t sum2_t;
#else
typedef uint16_t sum_t;
typedef uint32_t sum2_t;
#endif
#define BITS_PER_SUM (8 * sizeof(sum_t))

#define HADAMARD4(d0, d1, d2, d3, s0, s1, s2, s3) {\
    sum2_t t0 = s0 + s1;\
    sum2_t t1 = s0 - s1;\
    sum2_t t2 = s2 + s3;\
    sum2_t t3 = s2 - s3;\
    d0 = t0 + t2;\
    d2 = t0 - t2;\
    d1 = t1 + t3;\
    d3 = t1 - t3;\
}

// in: a pseudo-simd number of the form x+(y<<16)
// return: abs(x)+(abs(y)<<16)
// or <<32 for 16 bit pixels
static __forceinline sum2_t abs2( sum2_t a )
{
  sum2_t s = ((a>>(BITS_PER_SUM-1))&(((sum2_t)1<<BITS_PER_SUM)+1))*((sum_t)-1);
  return (a+s)^s;
}

/****************************************************************************
* pixel_satd_WxH: sum of 4x4 Hadamard transformed differences
****************************************************************************/
// uint8_t * instead of uint16_t * and
// int       instead of intptr_t  for keeping parameter list compatible with 8 bit functions
static unsigned int x264_pixel_satd_uint16_4x4_c(const uint8_t *pix1, /*intptr_t*/ int i_pix1, const uint8_t *pix2, /*intptr_t*/ int i_pix2 )
{
  sum2_t tmp[4][2];
  sum2_t a0, a1, a2, a3, b0, b1;
  sum2_t sum = 0;
  for( int i = 0; i < 4; i++, pix1 += i_pix1, pix2 += i_pix2 )
  {
    a0 = reinterpret_cast<const uint16_t *>(pix1)[0] - reinterpret_cast<const uint16_t *>(pix2)[0];
    a1 = reinterpret_cast<const uint16_t *>(pix1)[1] - reinterpret_cast<const uint16_t *>(pix2)[1];
    b0 = (a0+a1) + ((a0-a1)<<BITS_PER_SUM);
    a2 = reinterpret_cast<const uint16_t *>(pix1)[2] - reinterpret_cast<const uint16_t *>(pix2)[2];
    a3 = reinterpret_cast<const uint16_t *>(pix1)[3] - reinterpret_cast<const uint16_t *>(pix2)[3];
    b1 = (a2+a3) + ((a2-a3)<<BITS_PER_SUM);
    tmp[i][0] = b0 + b1;
    tmp[i][1] = b0 - b1;
  }
  for( int i = 0; i < 2; i++ )
  {
    HADAMARD4( a0, a1, a2, a3, tmp[0][i], tmp[1][i], tmp[2][i], tmp[3][i] );
    a0 = abs2(a0) + abs2(a1) + abs2(a2) + abs2(a3);
    sum += ((sum_t)a0) + (a0>>BITS_PER_SUM);
  }
  return (unsigned int)(sum >> 1);
}

static unsigned int x264_pixel_satd_uint16_8x4_c(const uint8_t *pix1, /*intptr_t*/int i_pix1, const uint8_t *pix2, /*intptr_t*/ int i_pix2 )
{
  sum2_t tmp[4][4];
  sum2_t a0, a1, a2, a3;
  sum2_t sum = 0;
  for( int i = 0; i < 4; i++, pix1 += i_pix1, pix2 += i_pix2 )
  {
    a0 = (reinterpret_cast<const uint16_t *>(pix1)[0] - reinterpret_cast<const uint16_t *>(pix2)[0]) + ((sum2_t)(reinterpret_cast<const uint16_t *>(pix1)[4] - reinterpret_cast<const uint16_t *>(pix2)[4]) << BITS_PER_SUM);
    a1 = (reinterpret_cast<const uint16_t *>(pix1)[1] - reinterpret_cast<const uint16_t *>(pix2)[1]) + ((sum2_t)(reinterpret_cast<const uint16_t *>(pix1)[5] - reinterpret_cast<const uint16_t *>(pix2)[5]) << BITS_PER_SUM);
    a2 = (reinterpret_cast<const uint16_t *>(pix1)[2] - reinterpret_cast<const uint16_t *>(pix2)[2]) + ((sum2_t)(reinterpret_cast<const uint16_t *>(pix1)[6] - reinterpret_cast<const uint16_t *>(pix2)[6]) << BITS_PER_SUM);
    a3 = (reinterpret_cast<const uint16_t *>(pix1)[3] - reinterpret_cast<const uint16_t *>(pix2)[3]) + ((sum2_t)(reinterpret_cast<const uint16_t *>(pix1)[7] - reinterpret_cast<const uint16_t *>(pix2)[7]) << BITS_PER_SUM);
    HADAMARD4( tmp[i][0], tmp[i][1], tmp[i][2], tmp[i][3], a0,a1,a2,a3 );
  }
  for( int i = 0; i < 4; i++ )
  {
    HADAMARD4( a0, a1, a2, a3, tmp[0][i], tmp[1][i], tmp[2][i], tmp[3][i] );
    sum += abs2(a0) + abs2(a1) + abs2(a2) + abs2(a3);
  }
  return (((sum_t)sum) + (sum>>BITS_PER_SUM)) >> 1; // (low_32 + high_32)/2 (8 bit: (low_16 + high_16)/2)
}

#define PIXEL_SATD_UINT16_C(w,h,sub)\
static unsigned int x264_pixel_satd_uint16_##w##x##h##_c(const uint8_t *pix1, int i_pix1, const uint8_t *pix2, int i_pix2 )\
{\
    int sum = sub( pix1, i_pix1, pix2, i_pix2 )\
            + sub( pix1+4*i_pix1, i_pix1, pix2+4*i_pix2, i_pix2 );\
    if( w==16 )\
        sum+= sub( pix1+sizeof(uint16_t)*8, i_pix1, pix2+sizeof(uint16_t)*8, i_pix2 )\
            + sub( pix1+sizeof(uint16_t)*8+4*i_pix1, i_pix1, pix2+sizeof(uint16_t)*8+4*i_pix2, i_pix2 );\
    if( h==16 )\
        sum+= sub( pix1+8*i_pix1, i_pix1, pix2+8*i_pix2, i_pix2 )\
            + sub( pix1+12*i_pix1, i_pix1, pix2+12*i_pix2, i_pix2 );\
    if( w==16 && h==16 )\
        sum+= sub( pix1+sizeof(uint16_t)*8+8*i_pix1, i_pix1, pix2+sizeof(uint16_t)*8+8*i_pix2, i_pix2 )\
            + sub( pix1+sizeof(uint16_t)*8+12*i_pix1, i_pix1, pix2+sizeof(uint16_t)*8+12*i_pix2, i_pix2 );\
    return sum;\
}

PIXEL_SATD_UINT16_C(16,16,x264_pixel_satd_uint16_8x4_c )
PIXEL_SATD_UINT16_C(16,8,x264_pixel_satd_uint16_8x4_c )
PIXEL_SATD_UINT16_C(8,16,x264_pixel_satd_uint16_8x4_c )
PIXEL_SATD_UINT16_C(8,8,x264_pixel_satd_uint16_8x4_c )
PIXEL_SATD_UINT16_C(4,16,x264_pixel_satd_uint16_4x4_c )
PIXEL_SATD_UINT16_C(4,8,x264_pixel_satd_uint16_4x4_c )

#undef PIXEL_SATD_UINT16_C

// SATD functions for blocks over 16x16 are not defined in pixel-a.asm,
// so as a poor man's substitute, we use a sum of smaller SATD functions.
// 8 bit only
#define SATD_REC_FUNC_UINT16(blsizex, blsizey, sblx, sbly, type) extern "C" unsigned int __cdecl	x264_pixel_satd_uint16_##blsizex##x##blsizey##_##type (const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch) \
{	\
	const int sum = \
		  x264_pixel_satd_uint16_##sblx##x##sbly##_##type (pSrc,                                      nSrcPitch, pRef,                                      nRefPitch) \
		+ x264_pixel_satd_uint16_##sblx##x##sbly##_##type (pSrc+sblx*sizeof(uint16_t),                nSrcPitch, pRef+sblx*sizeof(uint16_t),                nRefPitch) \
		+ x264_pixel_satd_uint16_##sblx##x##sbly##_##type (pSrc                      +sbly*nSrcPitch, nSrcPitch, pRef                      +sbly*nRefPitch, nRefPitch) \
		+ x264_pixel_satd_uint16_##sblx##x##sbly##_##type (pSrc+sblx*sizeof(uint16_t)+sbly*nSrcPitch, nSrcPitch, pRef+sblx*sizeof(uint16_t)+sbly*nRefPitch, nRefPitch); \
	return (sum); \
}

SATD_REC_FUNC_UINT16 (32, 32, 16, 16, c)
SATD_REC_FUNC_UINT16 (32, 16, 16,  8, c)

#undef SATD_REC_FUNC_UINT16

//------------------

SADFunction* get_sad_function(int BlockX, int BlockY, int pixelsize, arch_t arch)
{
    SADFunction *result;
    using std::make_tuple;

    // BlkSizeX, BlkSizeY, pixelsize, arch_t
    std::map<std::tuple<int, int, int, arch_t>, SADFunction*> func_sad;

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

    //---------------- AVX2
    // PF SAD 16 SIMD intrinsic functions
    // only for >=16 bytes widths (2x16 byte still OK)
    // templates in SADFunctions_avx2
    func_sad[make_tuple(32, 32, 2, USE_AVX2)] = Sad16_avx2<32, 32,uint16_t>;
    func_sad[make_tuple(32, 16, 2, USE_AVX2)] = Sad16_avx2<32, 16,uint16_t>;
    func_sad[make_tuple(32, 8 , 2, USE_AVX2)] = Sad16_avx2<32, 8,uint16_t>;
    func_sad[make_tuple(16, 32, 2, USE_AVX2)] = Sad16_avx2<16, 32,uint16_t>;
    func_sad[make_tuple(16, 16, 2, USE_AVX2)] = Sad16_avx2<16, 16,uint16_t>;
    func_sad[make_tuple(16, 8 , 2, USE_AVX2)] = Sad16_avx2<16, 8,uint16_t>;
    func_sad[make_tuple(16, 4 , 2, USE_AVX2)] = Sad16_avx2<16, 4,uint16_t>;
    func_sad[make_tuple(16, 2 , 2, USE_AVX2)] = Sad16_avx2<16, 2,uint16_t>;
    func_sad[make_tuple(16, 1 , 2, USE_AVX2)] = Sad16_avx2<16, 1,uint16_t>;
    func_sad[make_tuple(8 , 16, 2, USE_AVX2)] = Sad16_avx2<8 , 16,uint16_t>;
    func_sad[make_tuple(8 , 8 , 2, USE_AVX2)] = Sad16_avx2<8 , 8,uint16_t>;
    func_sad[make_tuple(8 , 4 , 2, USE_AVX2)] = Sad16_avx2<8 , 4,uint16_t>;
    func_sad[make_tuple(8 , 2 , 2, USE_AVX2)] = Sad16_avx2<8 , 2,uint16_t>;
    func_sad[make_tuple(8 , 1 , 2, USE_AVX2)] = Sad16_avx2<8 , 1,uint16_t>;
    // >=16 bytes
#ifdef SAD_AVX2_8BIT_INSTINSICS
    func_sad[make_tuple(32, 32, 1, USE_SSE2)] = Sad16_avx2<32, 32,uint8_t>;
    func_sad[make_tuple(32, 16, 1, USE_SSE2)] = Sad16_avx2<32, 16,uint8_t>;
    func_sad[make_tuple(32, 8 , 1, USE_SSE2)] = Sad16_avx2<32, 8,uint8_t>;
    func_sad[make_tuple(16, 32, 1, USE_SSE2)] = Sad16_avx2<16, 32,uint8_t>;
    func_sad[make_tuple(16, 16, 1, USE_SSE2)] = Sad16_avx2<16, 16,uint8_t>;
    func_sad[make_tuple(16, 8 , 1, USE_SSE2)] = Sad16_avx2<16, 8,uint8_t>;
    func_sad[make_tuple(16, 4 , 1, USE_SSE2)] = Sad16_avx2<16, 4,uint8_t>;
    func_sad[make_tuple(16, 2 , 1, USE_SSE2)] = Sad16_avx2<16, 2,uint8_t>;
    func_sad[make_tuple(16, 1 , 1, USE_SSE2)] = Sad16_avx2<16, 1,uint8_t>;
#endif

    result = func_sad[make_tuple(BlockX, BlockY, pixelsize, arch)];

    arch_t arch_orig = arch;

    // no AVX2 -> try AVX
    if (result == nullptr && (arch==USE_AVX2 || arch_orig==USE_AVX)) {
      arch = USE_AVX;
      result = func_sad[make_tuple(BlockX, BlockY, pixelsize, USE_AVX)];
    }
    // no AVX -> try SSE2
    if (result == nullptr && (arch==USE_AVX || arch_orig==USE_SSE2)) {
      arch = USE_SSE2;
      result = func_sad[make_tuple(BlockX, BlockY, pixelsize, USE_SSE2)];
    }
    // no SSE2 -> try C
    if (result == nullptr && (arch==USE_SSE2 || arch_orig==NO_SIMD)) {
      arch = NO_SIMD;
      // priority: C version compiled to avx2, avx
      if(arch_orig==USE_AVX2)
        result = get_sad_avx2_C_function(BlockX, BlockY, pixelsize, NO_SIMD);
      else if(arch_orig==USE_AVX)
        result = get_sad_avx_C_function(BlockX, BlockY, pixelsize, NO_SIMD);

      if(result == nullptr)
        result = func_sad[make_tuple(BlockX, BlockY, pixelsize, NO_SIMD)]; // fallback to C
    }
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
    func_satd[make_tuple(16, 16, 1, USE_SSE2)] = x264_pixel_satd_16x16_sse2;
    func_satd[make_tuple(16, 8 , 1, USE_SSE2)] = x264_pixel_satd_16x8_sse2;
    func_satd[make_tuple(8 , 16, 1, USE_SSE2)] = x264_pixel_satd_8x16_sse2;
    func_satd[make_tuple(8 , 8 , 1, USE_SSE2)] = x264_pixel_satd_8x8_sse2;
    func_satd[make_tuple(8 , 4 , 1, USE_SSE2)] = x264_pixel_satd_8x4_sse2;

    func_satd[make_tuple(32, 32, 2, NO_SIMD)] = x264_pixel_satd_uint16_32x32_c;
    func_satd[make_tuple(32, 16, 2, NO_SIMD)] = x264_pixel_satd_uint16_32x16_c;
    func_satd[make_tuple(16, 16, 2, NO_SIMD)] = x264_pixel_satd_uint16_16x16_c;
    func_satd[make_tuple(16, 8 , 2, NO_SIMD)] = x264_pixel_satd_uint16_16x8_c;
    func_satd[make_tuple(8 , 16, 2, NO_SIMD)] = x264_pixel_satd_uint16_8x16_c;
    func_satd[make_tuple(8 , 8 , 2, NO_SIMD)] = x264_pixel_satd_uint16_8x8_c;
    func_satd[make_tuple(8 , 4 , 2, NO_SIMD)] = x264_pixel_satd_uint16_8x4_c;

    SADFunction *result = func_satd[make_tuple(BlockX, BlockY, pixelsize, arch)];

    arch_t arch_orig = arch;

    // no AVX2 -> try AVX
    if (result == nullptr && (arch==USE_AVX2 || arch_orig==USE_AVX)) {
      arch = USE_AVX;
      result = func_satd[make_tuple(BlockX, BlockY, pixelsize, USE_AVX)];
    }
    // no AVX -> try SSE2
    if (result == nullptr && (arch==USE_AVX || arch_orig==USE_SSE2)) {
      arch = USE_SSE2;
      result = func_satd[make_tuple(BlockX, BlockY, pixelsize, USE_SSE2)];
    }
    // no SSE2 -> try C
    if (result == nullptr && (arch==USE_SSE2 || arch_orig==NO_SIMD)) {
      arch = NO_SIMD;
      // priority: C version compiled to avx2, avx
      /*
      if(arch_orig==USE_AVX2)
        result = get_satd_avx2_C_function(BlockX, BlockY, pixelsize, NO_SIMD);
      else if(arch_orig==USE_AVX)
        result = get_satd_avx_C_function(BlockX, BlockY, pixelsize, NO_SIMD);
      */
      if(result == nullptr)
        result = func_satd[make_tuple(BlockX, BlockY, pixelsize, NO_SIMD)]; // fallback to C
    }
    // fallback to C is not available yet
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

//SATD_REC_FUNC (32, 32, 16, 16, mmx2)
//SATD_REC_FUNC (32, 16, 16,  8, mmx2)
SATD_REC_FUNC (32, 32, 16, 16, sse2)
SATD_REC_FUNC (32, 16, 16,  8, sse2)
SATD_REC_FUNC (32, 32, 16, 16, ssse3)
SATD_REC_FUNC (32, 16, 16,  8, ssse3)

#undef SATD_REC_FUNC

