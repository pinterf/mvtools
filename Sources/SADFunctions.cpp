#include "SADFunctions.h"
#include "SADFunctions_avx.h"
#include "SADFunctions_avx2.h"
#include "overlap.h"
#include <map>
#include <tuple>
#include <stdint.h>
#include "def.h"
#include <immintrin.h>

MV_FORCEINLINE unsigned int SADABS(int x) {	return ( x < 0 ) ? -x : x; }
//inline unsigned int SADABS(int x) {	return ( x < -16 ) ? 16 : ( x < 0 ) ? -x : ( x > 16) ? 16 : x; }

template<int nBlkWidth, int nBlkHeight, typename pixel_t>
static unsigned int Sad_C(const uint8_t *pSrc, int nSrcPitch,const uint8_t *pRef,
  int nRefPitch)
{
  unsigned int sum = 0; // int is probably enough for 32x32
  for ( int y = 0; y < nBlkHeight; y++ )
  {
    for ( int x = 0; x < nBlkWidth; x++ )
      sum += SADABS(reinterpret_cast<const pixel_t *>(pSrc)[x] - reinterpret_cast<const pixel_t *>(pRef)[x]);
    pSrc += nSrcPitch;
    pRef += nRefPitch;
  }
  return sum;
}

// -- Start of SATD16 intrinsics
// https://github.com/xiph/daala/blob/master/src/x86/sse2mcenc.c
MV_FORCEINLINE void butterfly_2x2_16x8(__m128i &t0, __m128i &t1, __m128i &t2, __m128i &t3) {
  // a = t0 + t1, c = (t0 + t1) - (t1 + t1) = t0 - t1
  // b = t2 + t3, d = (t2 + t3) - (t3 + t3) = t2 - t3
  __m128i a = _mm_add_epi16(t0, t1);
  __m128i c = _mm_add_epi16(t1, t1);
  c = _mm_sub_epi16(a, c);
  __m128i b = _mm_add_epi16(t2, t3);
  __m128i d = _mm_add_epi16(t3, t3);
  d = _mm_sub_epi16(b, d);
  t0 = a;
  t1 = b;
  t2 = c;
  t3 = d;
}

MV_FORCEINLINE void butterfly_2x2_32x4(__m128i &t0, __m128i &t1, __m128i &t2, __m128i &t3) {
  // a = t0 + t1, c = (t0 + t1) - (t1 + t1) = t0 - t1
  // b = t2 + t3, d = (t2 + t3) - (t3 + t3) = t2 - t3
  __m128i a = _mm_add_epi32(t0, t1);
  __m128i c = _mm_add_epi32(t1, t1);
  c = _mm_sub_epi32(a, c);
  __m128i b = _mm_add_epi32(t2, t3);
  __m128i d = _mm_add_epi32(t3, t3);
  d = _mm_sub_epi32(b, d);
  t0 = a;
  t1 = b;
  t2 = c;
  t3 = d;
}

// Transpose 4 vectors with 4x32-bit values
MV_FORCEINLINE void transpose32x4(__m128i &t0, __m128i &t1, __m128i &t2, __m128i &t3) {
  __m128i a = _mm_unpacklo_epi32(t0, t1);
  __m128i b = _mm_unpacklo_epi32(t2, t3);
  __m128i c = _mm_unpackhi_epi32(t0, t1);
  __m128i d = _mm_unpackhi_epi32(t2, t3);
  t0 = _mm_unpacklo_epi64(a, b);
  t1 = _mm_unpackhi_epi64(a, b);
  t2 = _mm_unpacklo_epi64(c, d);
  t3 = _mm_unpackhi_epi64(c, d);
}

// SSE2 replacement for _mm_abs_epi32 (SSSE3)
MV_FORCEINLINE static __m128i _MM_ABS_EPI32(__m128i a) {
  __m128i mask = _mm_cmpgt_epi32(_mm_setzero_si128(), a);
  return _mm_sub_epi32(_mm_xor_si128(a, mask), mask);
}

MV_FORCEINLINE __m128i load_4x16_make_4x32_subtract(const unsigned char *src_p, const unsigned char *ref_p) {
  __m128i src = _mm_loadl_epi64((__m128i *)src_p);
  __m128i ref = _mm_loadl_epi64((__m128i *)ref_p);
  // no need for zero reg, subtract eliminates
  src = _mm_unpacklo_epi16(src, ref); // r3 s3 r2 s2 r1 s1 r0 s0
  ref = _mm_unpacklo_epi16(ref, ref); // r3 r3 r2 r2 r1 r1 r0 r0
  return _mm_sub_epi32(src, ref);     // s3-r2 s2-r2 s1-r1 s0-r0
}

// returns 4* of real value, have to divide it back
// for compatibility with x264 versions, only >>1 needed
template<bool hasSSSE3>
uint32_t compute_satd16_4x4_sse2(const unsigned char *src, int systride, const unsigned char *ref, int rystride) {
  uint32_t satd;
  // 16 bit overflows -> load 8 byte 4*uint16_t -> 4x32-bit
  __m128i a = load_4x16_make_4x32_subtract(src + 0*systride, ref + 0*rystride); // s3-r2 s2-r2 s1-r1 s0-r0
  __m128i b = load_4x16_make_4x32_subtract(src + 1*systride, ref + 1*rystride); 
  __m128i c = load_4x16_make_4x32_subtract(src + 2*systride, ref + 2*rystride); 
  __m128i d = load_4x16_make_4x32_subtract(src + 3*systride, ref + 3*rystride);
  // Vertical 1D transform
  butterfly_2x2_32x4(a, b, c, d);
  butterfly_2x2_32x4(a, b, c, d);
  transpose32x4(a, b, c, d);
  // Horizontal 1D transform
  butterfly_2x2_32x4(a, b, c, d);
  butterfly_2x2_32x4(a, b, c, d);
  // Find the absolute values
  if (!hasSSSE3) {
    a = _MM_ABS_EPI32(a);
    b = _MM_ABS_EPI32(b);
    c = _MM_ABS_EPI32(c);
    d = _MM_ABS_EPI32(d);
  }
  else {
// SSSE3:
    a = _mm_abs_epi32(a);
    b = _mm_abs_epi32(b);
    c = _mm_abs_epi32(c);
    d = _mm_abs_epi32(d);
  }
  // Take the sum of all the absolute values
  a = _mm_add_epi32(a, b);
  c = _mm_add_epi32(c, d);
  __m128i sums = _mm_add_epi32(a, c);
  // Sum the elements of the vector A+B+C+D
  // todo: faster?
  sums = _mm_add_epi32(sums, _mm_shuffle_epi32(sums, _MM_SHUFFLE(0, 1, 2, 3)));
  sums = _mm_add_epi32(sums, _mm_shuffle_epi32(sums, _MM_SHUFFLE(2, 3, 0, 1)));
  satd = _mm_cvtsi128_si32(sums);
  // Shift and round.*/
  //satd = (satd + 2) >> (2); // /4 round => 2
  // for compatibility with x264 versions, only >>1 needed, it is done outside
  // after summing up all
  // todo check: what is the max blocksize it would overflow?
  return satd;
}

#if 0
/*Partially transpose 8 vectors with 8 16-bit values.
Instead of transposing all the way, the vectors are split in half.
For every vector t_n where n is even, the finished vector can be formed by
the lower halves of t_n then t_(n+1).
For every even vector t_n, there is a vector t_(n+1) and that finished vector
can be formed by the upper halves of t_n then t_(n+1).*/
MV_FORCEINLINE void partial_transpose16x8(__m128i &t0, __m128i &t1,
  __m128i &t2, __m128i &t3, __m128i &t4, __m128i &t5,
  __m128i &t6, __m128i &t7) {
  /*00112233*/
  __m128i a0 = _mm_unpacklo_epi16(t0, t1);
  __m128i b0 = _mm_unpacklo_epi16(t2, t3);
  __m128i c0 = _mm_unpacklo_epi16(t4, t5);
  __m128i d0 = _mm_unpacklo_epi16(t6, t7);
  /*44556677*/
  __m128i e0 = _mm_unpackhi_epi16(t0, t1);
  __m128i f0 = _mm_unpackhi_epi16(t2, t3);
  __m128i g0 = _mm_unpackhi_epi16(t4, t5);
  __m128i h0 = _mm_unpackhi_epi16(t6, t7);
  /*00001111*/
  t0 = _mm_unpacklo_epi32(a0, b0);
  t1 = _mm_unpacklo_epi32(c0, d0);
  /*22223333*/
  t2 = _mm_unpackhi_epi32(a0, b0);
  t3 = _mm_unpackhi_epi32(c0, d0);
  /*44445555*/
  t4 = _mm_unpacklo_epi32(e0, f0);
  t5 = _mm_unpacklo_epi32(g0, h0);
  /*66667777*/
  t6 = _mm_unpackhi_epi32(e0, f0);
  t7 = _mm_unpackhi_epi32(g0, h0);
}

// (Does not)Correspond to _mm_cvtepi16_epi32 (sse4.1)
MV_FORCEINLINE void cvti16x8_i32x8(__m128i in, __m128i &out0, __m128i &out1) {
  //_mm_cmplt_epi16 returns a vector with all bits set to the sign bit or most
  // of significant bit
  __m128i extend = _mm_cmplt_epi16(in, _mm_setzero_si128());
  out0 = _mm_unpacklo_epi16(in, extend);
  out1 = _mm_unpackhi_epi16(in, extend);
}

// for less than 16 bits? where no 32 bit expansion needed?
MV_FORCEINLINE __m128i load_8x16_subtract_i16_8x16(const unsigned char *src_p, const unsigned char *ref_p) {
  __m128i src = _mm_load_si128((__m128i *)src_p);  // aligned in mvtools (really?)
  __m128i ref = _mm_loadu_si128((__m128i *)ref_p); // unaligned in mvtools
  return _mm_sub_epi16(src, ref);
}

MV_FORCEINLINE int32_t compute_satd16_8x8_part(const unsigned char *src,
  int systride, const unsigned char *ref, int rystride) {
  const int ln = 3;
  int32_t satd;
  __m128i sums;
  __m128i a = load_8x16_subtract_i16_8x16(src + 0*systride, ref + 0*rystride);
  __m128i b = load_8x16_subtract_i16_8x16(src + 1*systride, ref + 1*rystride);
  __m128i c = load_8x16_subtract_i16_8x16(src + 2*systride, ref + 2*rystride);
  __m128i d = load_8x16_subtract_i16_8x16(src + 3*systride, ref + 3*rystride);
  __m128i e = load_8x16_subtract_i16_8x16(src + 4*systride, ref + 4*rystride);
  __m128i f = load_8x16_subtract_i16_8x16(src + 5*systride, ref + 5*rystride);
  __m128i g = load_8x16_subtract_i16_8x16(src + 6*systride, ref + 6*rystride);
  __m128i h = load_8x16_subtract_i16_8x16(src + 7*systride, ref + 7*rystride);
  /*Vertical 1D transform.*/
  butterfly_2x2_16x8(a, b, c, d);
  butterfly_2x2_16x8(e, f, g, h);
  butterfly_2x2_16x8(a, b, e, f);
  butterfly_2x2_16x8(c, d, g, h);
  butterfly_2x2_16x8(a, b, e, f);
  butterfly_2x2_16x8(c, d, g, h);
  /*Don't transpose all the way because the lower and upper halves of each
  vector can become the size of a whole vector when converted from 16-bit to
  32-bit.*/
  partial_transpose16x8(a, c, b, d, e, g, f, h);
  /*Convert the 1st half of the 16-bit integers to 32-bit integers.
  Only converting one half at a time saves registers.*/
  __m128i ac_buf = c;
  __m128i bd_buf = d;
  __m128i eg_buf = g;
  __m128i fh_buf = h;
  cvti16x8_i32x8(a, a, c); // in out out
  cvti16x8_i32x8(b, b, d);
  cvti16x8_i32x8(e, e, g);
  cvti16x8_i32x8(f, f, h);
  /*Horizontal 1D transform (1st half).*/
  butterfly_2x2_32x4(a, b, c, d);
  butterfly_2x2_32x4(e, f, g, h);
  butterfly_2x2_32x4(a, b, e, f);
  butterfly_2x2_32x4(c, d, g, h);
  butterfly_2x2_32x4(a, b, e, f);
  butterfly_2x2_32x4(c, d, g, h);
  /*Find the absolute values (1st half).*/
  /* SSE2
  a = _MM_ABS_EPI32(a);
  b = _MM_ABS_EPI32(b);
  c = _MM_ABS_EPI32(c);
  d = _MM_ABS_EPI32(d);
  e = _MM_ABS_EPI32(e);
  f = _MM_ABS_EPI32(f);
  g = _MM_ABS_EPI32(g);
  h = _MM_ABS_EPI32(h);
  */
  // SSSE3
  a = _mm_abs_epi32(a);
  b = _mm_abs_epi32(b);
  c = _mm_abs_epi32(c);
  d = _mm_abs_epi32(d);
  e = _mm_abs_epi32(e);
  f = _mm_abs_epi32(f);
  g = _mm_abs_epi32(g);
  h = _mm_abs_epi32(h);
  /*Take the sum of all the absolute values (1st half).
  This will free up registers for the 2nd half.*/
  a = _mm_add_epi32(a, b);
  c = _mm_add_epi32(c, d);
  e = _mm_add_epi32(e, f);
  g = _mm_add_epi32(g, h);
  a = _mm_add_epi32(a, c);
  e = _mm_add_epi32(e, g);
  a = _mm_add_epi32(a, e);
  sums = a;
  /*Convert the 2nd half of the 16-bit integers to 32-bit integers.*/
  cvti16x8_i32x8(ac_buf, a, c); // in, out, out
  cvti16x8_i32x8(bd_buf, b, d);
  cvti16x8_i32x8(eg_buf, e, g);
  cvti16x8_i32x8(fh_buf, f, h);
  /*Horizontal 1D transform (2nd half).*/
  butterfly_2x2_32x4(a, b, c, d);
  butterfly_2x2_32x4(e, f, g, h);
  butterfly_2x2_32x4(a, b, e, f);
  butterfly_2x2_32x4(c, d, g, h);
  butterfly_2x2_32x4(a, b, e, f);
  butterfly_2x2_32x4(c, d, g, h);
  /*Find the absolute values (2nd half).*/
  /* SSE2
  a = _MM_ABS_EPI32(a);
  b = _MM_ABS_EPI32(b);
  c = _MM_ABS_EPI32(c);
  d = _MM_ABS_EPI32(d);
  e = _MM_ABS_EPI32(e);
  f = _MM_ABS_EPI32(f);
  g = _MM_ABS_EPI32(g);
  h = _MM_ABS_EPI32(h);
  */
  // SSSE3
  a = _mm_abs_epi32(a);
  b = _mm_abs_epi32(b);
  c = _mm_abs_epi32(c);
  d = _mm_abs_epi32(d);
  e = _mm_abs_epi32(e);
  f = _mm_abs_epi32(f);
  g = _mm_abs_epi32(g);
  h = _mm_abs_epi32(h);
  /*Take the sum of all the absolute values (2nd half).*/
  a = _mm_add_epi32(a, b);
  c = _mm_add_epi32(c, d);
  e = _mm_add_epi32(e, f);
  g = _mm_add_epi32(g, h);
  a = _mm_add_epi32(a, c);
  e = _mm_add_epi32(e, g);
  a = _mm_add_epi32(a, e);
  sums = _mm_add_epi32(sums, a);
  /*Sum the elements of the vector.*/
  sums = _mm_add_epi32(sums, _mm_shuffle_epi32(sums, _MM_SHUFFLE(0, 1, 2, 3)));
  sums = _mm_add_epi32(sums, _mm_shuffle_epi32(sums, _MM_SHUFFLE(2, 3, 0, 1)));
  satd = _mm_cvtsi128_si32(sums);
  /*Shift and round.*/
  //satd = (satd + 2) >> 2
  //no shift
  return satd;
}

/*Perform SATD on 8x8 blocks within src and ref then sum the results of each one.*/
MV_FORCEINLINE int32_t compute_sum_8x8_satd16(int ln,
  const unsigned char *src, int systride,
  const unsigned char *ref, int rystride) {
  int n;
  int i;
  int j;
  int xstride;
  int32_t satd;
  n = 1 << ln;
  xstride = sizeof(uint16_t);
  assert(n >= 8);
  satd = 0;
  for (i = 0; i < n; i += 8) {
    for (j = 0; j < n; j += 8) {
      satd += compute_satd16_8x8_part(
        src + i*systride + j*xstride, systride,
        ref + i*rystride + j*xstride, rystride);
    }
  }
  return satd;
}
#endif

template<int w, int h, bool hasSSSE3>
MV_FORCEINLINE int32_t calc_satd16_4x4_blocks(const unsigned char *src, int systride, const unsigned char *ref, int rystride) {
  int32_t satd = 0;
  for (int i = 0; i < h; i += 4) {
    for (int j = 0; j < w; j += 4) {
      satd += compute_satd16_4x4_sse2<hasSSSE3>(
        src + i*systride + j*sizeof(uint16_t), systride,
        ref + i*rystride + j*sizeof(uint16_t), rystride);
    }
  }
  //return (satd + 2) >> 2;
  return satd >> 1; // in order to match x264 C and asm version
}

// declaring templates for 16 bit satd
#define MAKE_FN(w,h) \
template<bool hasSSSE3> \
static uint32_t satd16_##w##x##h##_sse2(const unsigned char *src, int systride, const unsigned char *ref, int rystride) { \
  return calc_satd16_4x4_blocks<w, h, hasSSSE3>(src, systride, ref, rystride); \
} 
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
  MAKE_FN(32, 4)
  MAKE_FN(24, 48)
  MAKE_FN(24, 32)
  MAKE_FN(24, 24)
  MAKE_FN(24, 12)
  MAKE_FN(16, 64)
  MAKE_FN(16, 32)
  MAKE_FN(16, 16)
  MAKE_FN(16, 12)
  MAKE_FN(16, 8)
  MAKE_FN(16, 4)
  MAKE_FN(12, 48)
  MAKE_FN(12, 24)
  MAKE_FN(12, 16)
  MAKE_FN(12, 12)
  MAKE_FN(8, 32)
  MAKE_FN(8, 16)
  MAKE_FN(8, 8)
  MAKE_FN(8, 4)
  MAKE_FN(4, 32)
  MAKE_FN(4, 16)
  MAKE_FN(4, 8)
  MAKE_FN(4, 4)
#undef MAKE_FN


/* less than 16 bits?
static int32_t compute_satd16_16x16_sse2(const unsigned char *src, int systride,
  const unsigned char *ref, int rystride) {
  return compute_sum_8x8_satd16(4, src, systride, ref, rystride);
}
*/

// -- End of SATD16 intrinsics

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
/*
void HADAMARD4_sse2(__m128i &d10, __m128i &d32, __m128i &s10, __m128i &s32) {
  // d0 = s0 + s1 + (s2 + s3)
  // d2 = s0 + s1 - (s2 + s3)
  // d1 = s0 - s1 + (s2 - s3)
  // d3 = s0 - s1 - (s2 - s3)
    sum2_t t0 = s0 + s1;\
    sum2_t t1 = s0 - s1;\
    sum2_t t2 = s2 + s3;\
    sum2_t t3 = s2 - s3;\
    d0 = t0 + t2;\
    d2 = t0 - t2;\
    d1 = t1 + t3;\
    d3 = t1 - t3;\

}
*/
#define SUB(a,b) \
(uint32_t)(((int)(a & 0xFFFFFFFF)) - ((int)(b & 0xFFFFFFFF))) | \
((uint64_t)(((int)(((int64_t)a >> 32) & 0xFFFFFFFF)) - ((int)(((int64_t)b >> 32) & 0xFFFFFFFF))) << 32)
#define ADD(a,b) \
(uint32_t)(((int)(a & 0xFFFFFFFF)) + ((int)(b & 0xFFFFFFFF))) | \
((uint64_t)(((int)(((int64_t)a >> 32) & 0xFFFFFFFF)) + ((int)(((int64_t)b >> 32) & 0xFFFFFFFF))) << 32)

/*
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
todo why this differs from the ADD and SUB version?
*/
#define HADAMARD4(d0, d1, d2, d3, s0, s1, s2, s3) {\
    sum2_t t0 = ADD(s0,s1);\
    sum2_t t1 = SUB(s0,s1);\
    sum2_t t2 = ADD(s2,s3);\
    sum2_t t3 = SUB(s2,s3);\
    d0 = ADD(t0,t2);\
    d2 = SUB(t0,t2);\
    d1 = ADD(t1,t3);\
    d3 = SUB(t1,t3);\
}

// (a+b*256) - (c+d*256) = a-c + 256*(b-d) is it OK always?

// in: a pseudo-simd number of the form x+(y<<16)
// return: abs(x)+(abs(y)<<16)
// or <<32 for 16 bit pixels
// todo check is it OK?
static MV_FORCEINLINE sum2_t abs2( sum2_t a )
{
  sum2_t s = ((a>>(BITS_PER_SUM-1))&(((sum2_t)1<<BITS_PER_SUM)+1))*((sum_t)-1);
  return (a+s)^s;
}

/****************************************************************************
* pixel_satd_WxH: sum of 4x4 Hadamard transformed differences
****************************************************************************/
// uint8_t * instead of uint16_t * and
// int       instead of intptr_t  for keeping parameter list compatible with 8 bit functions
static unsigned int mvtools_satd_uint16_4x4_c(const uint8_t *pix1, /*intptr_t*/ int i_pix1, const uint8_t *pix2, /*intptr_t*/ int i_pix2 )
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

static unsigned int mvtools_satd_uint16_8x4_c(const uint8_t *pix1, /*intptr_t*/int i_pix1, const uint8_t *pix2, /*intptr_t*/ int i_pix2 )
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

/*
#define PIXEL_SATD_UINT16_C(w,h,sub)\
static unsigned int mvtools_satd_uint16_##w##x##h##_c(const uint8_t *pix1, int i_pix1, const uint8_t *pix2, int i_pix2 )\
{\
    unsigned int sum = sub( pix1, i_pix1, pix2, i_pix2 )\
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
*/

// test for the 8 bit x264 satd C version
template<int w, int h>
static unsigned int mvtools_satd_uint16_NxN_by_8x4_c(const uint8_t *pix1, int i_pix1, const uint8_t *pix2, int i_pix2)
{
  unsigned int sum = 0;
  for (int i = 0; i < h; i += 4) {
    for (int j = 0; j < w; j += 8) {
      sum += mvtools_satd_uint16_8x4_c(pix1 + sizeof(uint16_t) * j + i * i_pix1, i_pix1, pix2 + sizeof(uint16_t) * j + i * i_pix2, i_pix2);
    }
  }
  return sum;
}

// test for the 8 bit x264 satd C version
template<int w, int h>
static unsigned int mvtools_satd_uint16_NxN_by_4x4_c(const uint8_t *pix1, int i_pix1, const uint8_t *pix2, int i_pix2)
{
  unsigned int sum = 0;
  for (int i = 0; i < h; i += 4) {
    for (int j = 0; j < w; j += 4) {
      sum += mvtools_satd_uint16_4x4_c(pix1 + sizeof(uint16_t) * j + i * i_pix1, i_pix1, pix2 + sizeof(uint16_t) * j + i * i_pix2, i_pix2);
    }
  }
  return sum;
}

static unsigned int mvtools_satd_uint16_64x64_c(const uint8_t *pix1, int i_pix1, const uint8_t *pix2, int i_pix2) {
  return mvtools_satd_uint16_NxN_by_8x4_c<64, 64>(pix1, i_pix1, pix2, i_pix2);
}
static unsigned int mvtools_satd_uint16_64x48_c(const uint8_t *pix1, int i_pix1, const uint8_t *pix2, int i_pix2) {
  return mvtools_satd_uint16_NxN_by_8x4_c<64, 48>(pix1, i_pix1, pix2, i_pix2);
}
static unsigned int mvtools_satd_uint16_64x32_c(const uint8_t *pix1, int i_pix1, const uint8_t *pix2, int i_pix2) {
  return mvtools_satd_uint16_NxN_by_8x4_c<64, 32>(pix1, i_pix1, pix2, i_pix2);
}
static unsigned int mvtools_satd_uint16_64x16_c(const uint8_t *pix1, int i_pix1, const uint8_t *pix2, int i_pix2) {
  return mvtools_satd_uint16_NxN_by_8x4_c<64, 16>(pix1, i_pix1, pix2, i_pix2);
}
static unsigned int mvtools_satd_uint16_48x64_c(const uint8_t *pix1, int i_pix1, const uint8_t *pix2, int i_pix2) {
  return mvtools_satd_uint16_NxN_by_8x4_c<48, 64>(pix1, i_pix1, pix2, i_pix2);
}
static unsigned int mvtools_satd_uint16_48x48_c(const uint8_t *pix1, int i_pix1, const uint8_t *pix2, int i_pix2) {
  return mvtools_satd_uint16_NxN_by_8x4_c<48, 48>(pix1, i_pix1, pix2, i_pix2);
}
static unsigned int mvtools_satd_uint16_48x24_c(const uint8_t *pix1, int i_pix1, const uint8_t *pix2, int i_pix2) {
  return mvtools_satd_uint16_NxN_by_8x4_c<48, 24>(pix1, i_pix1, pix2, i_pix2);
}
static unsigned int mvtools_satd_uint16_48x12_c(const uint8_t *pix1, int i_pix1, const uint8_t *pix2, int i_pix2) {
  return mvtools_satd_uint16_NxN_by_8x4_c<48, 12>(pix1, i_pix1, pix2, i_pix2);
}
static unsigned int mvtools_satd_uint16_32x64_c(const uint8_t *pix1, int i_pix1, const uint8_t *pix2, int i_pix2) {
  return mvtools_satd_uint16_NxN_by_8x4_c<32, 64>(pix1, i_pix1, pix2, i_pix2);
}
static unsigned int mvtools_satd_uint16_32x32_c(const uint8_t *pix1, int i_pix1, const uint8_t *pix2, int i_pix2) {
  return mvtools_satd_uint16_NxN_by_8x4_c<32, 32>(pix1, i_pix1, pix2, i_pix2); 
}
static unsigned int mvtools_satd_uint16_32x24_c(const uint8_t *pix1, int i_pix1, const uint8_t *pix2, int i_pix2) {
  return mvtools_satd_uint16_NxN_by_8x4_c<32, 24>(pix1, i_pix1, pix2, i_pix2);
}
static unsigned int mvtools_satd_uint16_32x16_c(const uint8_t *pix1, int i_pix1, const uint8_t *pix2, int i_pix2) {
  return mvtools_satd_uint16_NxN_by_8x4_c<32, 16>(pix1, i_pix1, pix2, i_pix2); 
}
static unsigned int mvtools_satd_uint16_32x8_c(const uint8_t *pix1, int i_pix1, const uint8_t *pix2, int i_pix2) {
  return mvtools_satd_uint16_NxN_by_8x4_c<32, 8>(pix1, i_pix1, pix2, i_pix2); 
}
static unsigned int mvtools_satd_uint16_32x4_c(const uint8_t *pix1, int i_pix1, const uint8_t *pix2, int i_pix2) {
  return mvtools_satd_uint16_NxN_by_8x4_c<32, 4>(pix1, i_pix1, pix2, i_pix2);
}
static unsigned int mvtools_satd_uint16_24x48_c(const uint8_t *pix1, int i_pix1, const uint8_t *pix2, int i_pix2) {
  return mvtools_satd_uint16_NxN_by_8x4_c<24, 48>(pix1, i_pix1, pix2, i_pix2);
}
static unsigned int mvtools_satd_uint16_24x32_c(const uint8_t *pix1, int i_pix1, const uint8_t *pix2, int i_pix2) {
  return mvtools_satd_uint16_NxN_by_8x4_c<24, 32>(pix1, i_pix1, pix2, i_pix2);
}
static unsigned int mvtools_satd_uint16_24x24_c(const uint8_t *pix1, int i_pix1, const uint8_t *pix2, int i_pix2) {
  return mvtools_satd_uint16_NxN_by_8x4_c<24, 24>(pix1, i_pix1, pix2, i_pix2);
}
static unsigned int mvtools_satd_uint16_24x12_c(const uint8_t *pix1, int i_pix1, const uint8_t *pix2, int i_pix2) {
  return mvtools_satd_uint16_NxN_by_8x4_c<24, 12>(pix1, i_pix1, pix2, i_pix2);
}
static unsigned int mvtools_satd_uint16_16x64_c(const uint8_t *pix1, int i_pix1, const uint8_t *pix2, int i_pix2) {
  return mvtools_satd_uint16_NxN_by_8x4_c<16, 64>(pix1, i_pix1, pix2, i_pix2);
}
static unsigned int mvtools_satd_uint16_16x32_c(const uint8_t *pix1, int i_pix1, const uint8_t *pix2, int i_pix2) {
  return mvtools_satd_uint16_NxN_by_8x4_c<16, 32>(pix1, i_pix1, pix2, i_pix2); 
}
static unsigned int mvtools_satd_uint16_16x16_c(const uint8_t *pix1, int i_pix1, const uint8_t *pix2, int i_pix2) {
  return mvtools_satd_uint16_NxN_by_8x4_c<16, 16>(pix1, i_pix1, pix2, i_pix2); 
}
static unsigned int mvtools_satd_uint16_16x12_c(const uint8_t *pix1, int i_pix1, const uint8_t *pix2, int i_pix2) {
  return mvtools_satd_uint16_NxN_by_8x4_c<16, 12>(pix1, i_pix1, pix2, i_pix2); 
}
static unsigned int mvtools_satd_uint16_16x8_c(const uint8_t *pix1, int i_pix1, const uint8_t *pix2, int i_pix2) {
  return mvtools_satd_uint16_NxN_by_8x4_c<16, 8>(pix1, i_pix1, pix2, i_pix2);
}
static unsigned int mvtools_satd_uint16_16x4_c(const uint8_t *pix1, int i_pix1, const uint8_t *pix2, int i_pix2) {
  return mvtools_satd_uint16_NxN_by_8x4_c<16, 4>(pix1, i_pix1, pix2, i_pix2); 
}
static unsigned int mvtools_satd_uint16_12x48_c(const uint8_t *pix1, int i_pix1, const uint8_t *pix2, int i_pix2) {
  return mvtools_satd_uint16_NxN_by_4x4_c<12, 48>(pix1, i_pix1, pix2, i_pix2);
}
static unsigned int mvtools_satd_uint16_12x24_c(const uint8_t *pix1, int i_pix1, const uint8_t *pix2, int i_pix2) {
  return mvtools_satd_uint16_NxN_by_4x4_c<12, 24>(pix1, i_pix1, pix2, i_pix2);
}
static unsigned int mvtools_satd_uint16_12x16_c(const uint8_t *pix1, int i_pix1, const uint8_t *pix2, int i_pix2) {
  return mvtools_satd_uint16_NxN_by_4x4_c<12, 16>(pix1, i_pix1, pix2, i_pix2);
}
static unsigned int mvtools_satd_uint16_12x12_c(const uint8_t *pix1, int i_pix1, const uint8_t *pix2, int i_pix2) {
  return mvtools_satd_uint16_NxN_by_4x4_c<12, 12>(pix1, i_pix1, pix2, i_pix2);
}
static unsigned int mvtools_satd_uint16_8x32_c(const uint8_t *pix1, int i_pix1, const uint8_t *pix2, int i_pix2) {
  return mvtools_satd_uint16_NxN_by_8x4_c<8, 32>(pix1, i_pix1, pix2, i_pix2);
}
static unsigned int mvtools_satd_uint16_8x16_c(const uint8_t *pix1, int i_pix1, const uint8_t *pix2, int i_pix2) {
  return mvtools_satd_uint16_NxN_by_8x4_c<8, 16>(pix1, i_pix1, pix2, i_pix2); 
}
static unsigned int mvtools_satd_uint16_8x8_c(const uint8_t *pix1, int i_pix1, const uint8_t *pix2, int i_pix2) {
  return mvtools_satd_uint16_NxN_by_8x4_c<8, 8>(pix1, i_pix1, pix2, i_pix2); 
}
/* basic function
static unsigned int mvtools_satd_uint16_8x4_c(const uint8_t *pix1, int i_pix1, const uint8_t *pix2, int i_pix2) {
  return mvtools_satd_uint16_NxN_by_8x4_c<8, 4>(pix1, i_pix1, pix2, i_pix2); 
}
*/
static unsigned int mvtools_satd_uint16_4x32_c(const uint8_t *pix1, int i_pix1, const uint8_t *pix2, int i_pix2) {
  return mvtools_satd_uint16_NxN_by_4x4_c<4, 32>(pix1, i_pix1, pix2, i_pix2);
}
static unsigned int mvtools_satd_uint16_4x16_c(const uint8_t *pix1, int i_pix1, const uint8_t *pix2, int i_pix2) {
  return mvtools_satd_uint16_NxN_by_4x4_c<4, 16>(pix1, i_pix1, pix2, i_pix2);
}
static unsigned int mvtools_satd_uint16_4x8_c(const uint8_t *pix1, int i_pix1, const uint8_t *pix2, int i_pix2) {
  return mvtools_satd_uint16_NxN_by_4x4_c<4, 8>(pix1, i_pix1, pix2, i_pix2); 
}
/* basic function
static unsigned int mvtools_satd_uint16_4x4_c(const uint8_t *pix1, int i_pix1, const uint8_t *pix2, int i_pix2) {
  return mvtools_satd_uint16_NxN_by_4x4_c<4, 4>(pix1, i_pix1, pix2, i_pix2); 
}
*/

static unsigned int mvtools_satd_uint16_16x16_c_2(const uint8_t *pix1, int i_pix1, const uint8_t *pix2, int i_pix2)
{
  unsigned int satd = mvtools_satd_uint16_NxN_by_4x4_c<16, 16>(pix1, i_pix1, pix2, i_pix2);
  return satd;
}

#undef PIXEL_SATD_UINT16_C

// SATD functions for blocks over 16x16 are not defined in pixel-a.asm,
// so as a poor man's substitute, we use a sum of smaller SATD functions.
// 8 bit only
#define SATD_REC_FUNC_UINT16(blsizex, blsizey, sblx, sbly, type) extern "C" unsigned int __cdecl	mvtools_satd_uint16_##blsizex##x##blsizey##_##type (const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch) \
{	\
	const int sum = \
		  mvtools_satd_uint16_##sblx##x##sbly##_##type (pSrc,                                      nSrcPitch, pRef,                                      nRefPitch) \
		+ mvtools_satd_uint16_##sblx##x##sbly##_##type (pSrc+sblx*sizeof(uint16_t),                nSrcPitch, pRef+sblx*sizeof(uint16_t),                nRefPitch) \
		+ mvtools_satd_uint16_##sblx##x##sbly##_##type (pSrc                      +sbly*nSrcPitch, nSrcPitch, pRef                      +sbly*nRefPitch, nRefPitch) \
		+ mvtools_satd_uint16_##sblx##x##sbly##_##type (pSrc+sblx*sizeof(uint16_t)+sbly*nSrcPitch, nSrcPitch, pRef+sblx*sizeof(uint16_t)+sbly*nRefPitch, nRefPitch); \
	return (sum); \
}


// mvtools_satd_uint16_32x32_c
// mvtools_satd_uint16_32x16_c
//SATD_REC_FUNC_UINT16 (32, 32, 16, 16, c)
//SATD_REC_FUNC_UINT16 (32, 16, 16,  8, c)

unsigned int __cdecl x264_pixel_satd_16x16_sse2_debug(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch)
{
  unsigned int satd = x264_pixel_satd_16x16_sse2(pSrc, nSrcPitch, pRef, nRefPitch);
  return satd;
}
/*
unsigned int __cdecl satd_uint16_16x16_sse2(const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch)
{
//  unsigned int val1 = compute_satd16_16x16_sse2(pSrc, nSrcPitch, pRef, nRefPitch);
  unsigned int val1 = compute_sum_8x8_satd16_32bit_inside(4, pSrc, nSrcPitch, pRef, nRefPitch); // 2^4 == 16
//  unsigned int val2 = mvtools_satd_uint16_16x16_c(pSrc, nSrcPitch, pRef, nRefPitch);
//  unsigned int val3 = mvtools_satd_uint16_16x16_c_2(pSrc, nSrcPitch, pRef, nRefPitch);

  if (val1 != val2)
    val1 = val2;
  return val1;
}
*/


#undef SATD_REC_FUNC_UINT16

//------------------

SADFunction* get_sad_function(int BlockX, int BlockY, int pixelsize, arch_t arch)
{
    using std::make_tuple;

    // BlkSizeX, BlkSizeY, pixelsize, arch_t
    std::map<std::tuple<int, int, int, arch_t>, SADFunction*> func_sad;
#define MAKE_SAD_FN(x, y) func_sad[make_tuple(x, y, 1, NO_SIMD)] = Sad_C<x, y, uint8_t>; \
func_sad[make_tuple(x, y, 2, NO_SIMD)] = Sad_C<x, y, uint16_t>;
// match with CopyCode.cpp and Overlap.cpp, and luma (variance.cpp) list
      MAKE_SAD_FN(64, 64)
      MAKE_SAD_FN(64, 48)
      MAKE_SAD_FN(64, 32)
      MAKE_SAD_FN(64, 16)
      MAKE_SAD_FN(48, 64)
      MAKE_SAD_FN(48, 48)
      MAKE_SAD_FN(48, 24)
      MAKE_SAD_FN(48, 12)
      MAKE_SAD_FN(32, 64)
      MAKE_SAD_FN(32, 32)
      MAKE_SAD_FN(32, 24)
      MAKE_SAD_FN(32, 16)
      MAKE_SAD_FN(32, 8)
      MAKE_SAD_FN(24, 48)
      MAKE_SAD_FN(24, 32)
      MAKE_SAD_FN(24, 24)
      MAKE_SAD_FN(24, 12)
      MAKE_SAD_FN(24, 6)
      MAKE_SAD_FN(16, 64)
      MAKE_SAD_FN(16, 32)
      MAKE_SAD_FN(16, 16)
      MAKE_SAD_FN(16, 12)
      MAKE_SAD_FN(16, 8)
      MAKE_SAD_FN(16, 4)
      MAKE_SAD_FN(16, 2)
      MAKE_SAD_FN(16, 1)
      MAKE_SAD_FN(12, 48)
      MAKE_SAD_FN(12, 24)
      MAKE_SAD_FN(12, 16)
      MAKE_SAD_FN(12, 12)
      MAKE_SAD_FN(12, 6)
      MAKE_SAD_FN(8, 32)
      MAKE_SAD_FN(8, 16)
      MAKE_SAD_FN(8, 8)
      MAKE_SAD_FN(8, 4)
      MAKE_SAD_FN(8, 2)
      MAKE_SAD_FN(8, 1)
      MAKE_SAD_FN(6, 12)
      MAKE_SAD_FN(6, 6)
      MAKE_SAD_FN(6, 3)
      MAKE_SAD_FN(4, 8)
      MAKE_SAD_FN(4, 4)
      MAKE_SAD_FN(4, 2)
      MAKE_SAD_FN(4, 1)
      MAKE_SAD_FN(3, 6)
      MAKE_SAD_FN(3, 3)
      MAKE_SAD_FN(2, 4)
      MAKE_SAD_FN(2, 2)
      MAKE_SAD_FN(2, 1)
#undef MAKE_SAD_FN

// PF SAD 16 SIMD intrinsic functions
// only for >=8 bytes widths
// AVX compiled SSE2 code and regular SSE2
// avx is instantiated in SADFunctions_avx.h
#define MAKE_SAD_FN(x, y) func_sad[make_tuple(x, y, 2, USE_AVX)] = Sad16_sse2_avx<x, y, uint16_t>; \
func_sad[make_tuple(x, y, 2, USE_SSE2)] = Sad16_sse2<x, y, uint16_t>;
      MAKE_SAD_FN(64, 64)
      MAKE_SAD_FN(64, 48)
      MAKE_SAD_FN(64, 32)
      MAKE_SAD_FN(64, 16)
      MAKE_SAD_FN(48, 64)
      MAKE_SAD_FN(48, 48)
      MAKE_SAD_FN(48, 24)
      MAKE_SAD_FN(48, 12)
      MAKE_SAD_FN(32, 64)
      MAKE_SAD_FN(32, 32)
      MAKE_SAD_FN(32, 24)
      MAKE_SAD_FN(32, 16)
      MAKE_SAD_FN(32, 8)
      MAKE_SAD_FN(24, 48)
      MAKE_SAD_FN(24, 32)
      MAKE_SAD_FN(24, 24)
      MAKE_SAD_FN(24, 12)
      MAKE_SAD_FN(24, 6)
      MAKE_SAD_FN(16, 64)
      MAKE_SAD_FN(16, 32)
      MAKE_SAD_FN(16, 16)
      MAKE_SAD_FN(16, 12)
      MAKE_SAD_FN(16, 8)
      MAKE_SAD_FN(16, 4)
      MAKE_SAD_FN(16, 2)
      MAKE_SAD_FN(16, 1)
      MAKE_SAD_FN(12, 48)
      MAKE_SAD_FN(12, 24)
      MAKE_SAD_FN(12, 16)
      MAKE_SAD_FN(12, 12)
      MAKE_SAD_FN(12, 6)
      MAKE_SAD_FN(8, 32)
      MAKE_SAD_FN(8, 16)
      MAKE_SAD_FN(8, 8)
      MAKE_SAD_FN(8, 4)
      MAKE_SAD_FN(8, 2)
      MAKE_SAD_FN(8, 1)
      MAKE_SAD_FN(6, 12)
      MAKE_SAD_FN(6, 6)
      //MAKE_SAD_FN(6, 3)
      MAKE_SAD_FN(8, 1)
      MAKE_SAD_FN(4, 8)
      MAKE_SAD_FN(4, 4)
      MAKE_SAD_FN(4, 2)
      //MAKE_SAD_FN(4, 1)  // 8 bytes with height=1 not supported for SSE2
      //MAKE_SAD_FN(2, 4)  // 2 pixels 4 bytes not supported with SSE2
      //MAKE_SAD_FN(2, 2)
      //MAKE_SAD_FN(2, 1)
#undef MAKE_SAD_FN

    // 8 bit SAD function from x265 asm

    // Block Size: 64*x
    // Supported: 64x64, 64x48, 64x32, 64x16
    // AVX2:
    func_sad[make_tuple(64, 64, 1, USE_AVX2)] = x264_pixel_sad_64x64_avx2;
    func_sad[make_tuple(64, 48, 1, USE_AVX2)] = x264_pixel_sad_64x48_avx2;
    func_sad[make_tuple(64, 32, 1, USE_AVX2)] = x264_pixel_sad_64x32_avx2;
    func_sad[make_tuple(64, 16, 1, USE_AVX2)] = x264_pixel_sad_64x16_avx2;
    // SSE2:
    func_sad[make_tuple(64, 64, 1, USE_SSE2)] = x264_pixel_sad_64x64_sse2;
    func_sad[make_tuple(64, 48, 1, USE_SSE2)] = x264_pixel_sad_64x48_sse2;
    func_sad[make_tuple(64, 32, 1, USE_SSE2)] = x264_pixel_sad_64x32_sse2;
    func_sad[make_tuple(64, 16, 1, USE_SSE2)] = x264_pixel_sad_64x16_sse2;

    // Block Size: 48*x
    // Supported: 48x64, 48x48, 48x24, 48x12
    // AVX2
    func_sad[make_tuple(48, 64, 1, USE_AVX2)] = x264_pixel_sad_48x64_avx2;
    func_sad[make_tuple(48, 48, 1, USE_AVX2)] = x264_pixel_sad_48x48_avx2;
    func_sad[make_tuple(48, 24, 1, USE_AVX2)] = x264_pixel_sad_48x24_avx2;
    func_sad[make_tuple(48, 12, 1, USE_AVX2)] = x264_pixel_sad_48x12_avx2;
    // SSE2
    func_sad[make_tuple(48, 64, 1, USE_SSE2)] = x264_pixel_sad_48x64_sse2;
    func_sad[make_tuple(48, 48, 1, USE_SSE2)] = x264_pixel_sad_48x48_sse2;
    func_sad[make_tuple(48, 24, 1, USE_SSE2)] = x264_pixel_sad_48x24_sse2;
    func_sad[make_tuple(48, 12, 1, USE_SSE2)] = x264_pixel_sad_48x12_sse2;

    // Block Size: 32*x
    // Supported: 32x64, 32x32, 32x24, 32x16, 32x8
    // AVX2
    func_sad[make_tuple(32, 64, 1, USE_AVX2)] = x264_pixel_sad_32x64_avx2;
    func_sad[make_tuple(32, 32, 1, USE_AVX2)] = x264_pixel_sad_32x32_avx2;
    func_sad[make_tuple(32, 24, 1, USE_AVX2)] = x264_pixel_sad_32x24_avx2;
    func_sad[make_tuple(32, 16, 1, USE_AVX2)] = x264_pixel_sad_32x16_avx2;
    func_sad[make_tuple(32, 8, 1, USE_AVX2)] = x264_pixel_sad_32x8_avx2;
    // SSE3
    func_sad[make_tuple(32, 64, 1, USE_SSE41)] = x264_pixel_sad_32x64_sse3;
    func_sad[make_tuple(32, 32, 1, USE_SSE41)] = x264_pixel_sad_32x32_sse3;
    func_sad[make_tuple(32, 24, 1, USE_SSE41)] = x264_pixel_sad_32x24_sse3;
    func_sad[make_tuple(32, 16, 1, USE_SSE41)] = x264_pixel_sad_32x16_sse3;
    func_sad[make_tuple(32, 8, 1, USE_SSE41)] = x264_pixel_sad_32x8_sse3;
    // SSE2
    func_sad[make_tuple(32, 64, 1, USE_SSE2)] = x264_pixel_sad_32x64_sse2;
    func_sad[make_tuple(32, 32, 1, USE_SSE2)] = x264_pixel_sad_32x32_sse2;
    func_sad[make_tuple(32, 24, 1, USE_SSE2)] = x264_pixel_sad_32x24_sse2;
    func_sad[make_tuple(32, 16, 1, USE_SSE2)] = x264_pixel_sad_32x16_sse2;
    func_sad[make_tuple(32, 8 , 1, USE_SSE2)] = x264_pixel_sad_32x8_sse2;

    // Block Size: 24*x
    // Supported: 24x48, 24x32, 24x12, 24x6
    func_sad[make_tuple(24, 48, 1, USE_SSE2)] = x264_pixel_sad_24x48_sse2;
    func_sad[make_tuple(24, 32, 1, USE_SSE2)] = x264_pixel_sad_24x32_sse2;
    func_sad[make_tuple(24, 24, 1, USE_SSE2)] = x264_pixel_sad_24x24_sse2;
    func_sad[make_tuple(24, 12, 1, USE_SSE2)] = x264_pixel_sad_24x12_sse2;
    func_sad[make_tuple(24,  6, 1, USE_SSE2)] = x264_pixel_sad_24x6_sse2;

    // Supported: 16x64, 16x32, 16x16, 16x12, 16x8, 16x4
    // SSE3
    func_sad[make_tuple(16, 64, 1, USE_SSE41)] = x264_pixel_sad_16x64_sse3;
    func_sad[make_tuple(16, 32, 1, USE_SSE41)] = x264_pixel_sad_16x32_sse3;
    func_sad[make_tuple(16, 16, 1, USE_SSE41)] = x264_pixel_sad_16x16_sse3;
    func_sad[make_tuple(16, 12, 1, USE_SSE41)] = x264_pixel_sad_16x12_sse3;
    func_sad[make_tuple(16, 8, 1, USE_SSE41)] = x264_pixel_sad_16x8_sse3;
    func_sad[make_tuple(16, 4, 1, USE_SSE41)] = x264_pixel_sad_16x4_sse3;
    // SSE2
    func_sad[make_tuple(16, 64, 1, USE_SSE2)] = x264_pixel_sad_16x64_sse2;
    func_sad[make_tuple(16, 32, 1, USE_SSE2)] = x264_pixel_sad_16x32_sse2;
    func_sad[make_tuple(16, 16, 1, USE_SSE2)] = x264_pixel_sad_16x16_sse2;
    func_sad[make_tuple(16, 12, 1, USE_SSE2)] = x264_pixel_sad_16x12_sse2;
    func_sad[make_tuple(16, 8 , 1, USE_SSE2)] = x264_pixel_sad_16x8_sse2;
    func_sad[make_tuple(16, 4, 1, USE_SSE2)] = x264_pixel_sad_16x4_sse2;//mmx2;
    func_sad[make_tuple(16, 2, 1, USE_SSE2)] = Sad16x2_iSSE;

    // Block Size: 12*x
    // Supported: 12x48, 12x24, 12x16, 12x6 (12x3 is C only)
    func_sad[make_tuple(12, 48, 1, USE_SSE2)] = x264_pixel_sad_12x48_sse2;
    func_sad[make_tuple(12, 24, 1, USE_SSE2)] = x264_pixel_sad_12x24_sse2;
    func_sad[make_tuple(12, 16, 1, USE_SSE2)] = x264_pixel_sad_12x16_sse2;
    func_sad[make_tuple(12, 12, 1, USE_SSE2)] = x264_pixel_sad_12x12_sse2;
    func_sad[make_tuple(12,  6, 1, USE_SSE2)] = x264_pixel_sad_12x6_sse2;

    // Block Size: 8*x
    // Supported: 8x32, 8x16, 8x8, 8x4, (8x2, 8x1)
    func_sad[make_tuple(8, 32, 1, USE_SSE2)] = x264_pixel_sad_8x32_sse2;
    func_sad[make_tuple(8, 16, 1, USE_SSE2)] = x264_pixel_sad_8x16_sse2; // or mmx2 check which is faster
    func_sad[make_tuple(8 , 8,  1, USE_SSE2)] = x264_pixel_sad_8x8_mmx2;
    func_sad[make_tuple(8 , 4 , 1, USE_SSE2)] = x264_pixel_sad_8x4_mmx2;
    
    func_sad[make_tuple(8 , 2 , 1, USE_SSE2)] = Sad8x2_iSSE;
    func_sad[make_tuple(8 , 1 , 1, USE_SSE2)] = Sad8x1_iSSE;

    func_sad[make_tuple(6, 12, 1, USE_SSE2)] = x264_pixel_sad_6x12_sse2;
    func_sad[make_tuple(6, 6, 1, USE_SSE2)] = x264_pixel_sad_6x6_sse2;

    // Block Size: 4*x
    // Supported: 4x8, 4x4, 4x2
    func_sad[make_tuple(4 , 8 , 1, USE_SSE2)] = x264_pixel_sad_4x8_mmx2;
    func_sad[make_tuple(4 , 4 , 1, USE_SSE2)] = x264_pixel_sad_4x4_mmx2;
    func_sad[make_tuple(4 , 2 , 1, USE_SSE2)] = Sad4x2_iSSE;
    // Block Size: 2*x
    // Supported: 2x4, 2x2
    func_sad[make_tuple(2 , 4 , 1, USE_SSE2)] = Sad2x4_iSSE;
    func_sad[make_tuple(2 , 2 , 1, USE_SSE2)] = Sad2x2_iSSE;


    //---------------- AVX2
    // PF SAD 16 SIMD intrinsic functions
    // only for >=16 bytes widths (2x16 byte still OK width 24 is not available)
    // templates in SADFunctions_avx2
#define MAKE_SAD_FN(x, y) func_sad[make_tuple(x, y, 2, USE_AVX2)] = Sad16_avx2<x, y,uint16_t>;
    MAKE_SAD_FN(64, 64)
      MAKE_SAD_FN(64, 48)
      MAKE_SAD_FN(64, 32)
      MAKE_SAD_FN(64, 16)
      MAKE_SAD_FN(48, 64)
      MAKE_SAD_FN(48, 48)
      MAKE_SAD_FN(48, 24)
      MAKE_SAD_FN(48, 12)
      MAKE_SAD_FN(32, 64)
      MAKE_SAD_FN(32, 32)
      MAKE_SAD_FN(32, 24)
      MAKE_SAD_FN(32, 16)
      MAKE_SAD_FN(32, 8)
      // MAKE_SAD_FN(24, 32) // 24*2 is not mod 32 bytes
      MAKE_SAD_FN(16, 64)
      MAKE_SAD_FN(16, 32)
      MAKE_SAD_FN(16, 16)
      MAKE_SAD_FN(16, 12)
      MAKE_SAD_FN(16, 8)
      MAKE_SAD_FN(16, 4)
      MAKE_SAD_FN(16, 2)
      MAKE_SAD_FN(16, 1) // 32 bytes with height=1 is OK for AVX2
      //MAKE_SAD_FN(12, 16) 12*2 not mod 32 bytes
      MAKE_SAD_FN(8, 32)
      MAKE_SAD_FN(8, 16)
      MAKE_SAD_FN(8, 8)
      MAKE_SAD_FN(8, 4)
      MAKE_SAD_FN(8, 2)
      // MAKE_SAD_FN(8, 1) // 16 bytes with height=1 not supported for AVX2
      //MAKE_SAD_FN(4, 8)
      //MAKE_SAD_FN(4, 4)
      //MAKE_SAD_FN(4, 2)
      //MAKE_SAD_FN(4, 1)  // 8 bytes with height=1 not supported for SSE2
      //MAKE_SAD_FN(2, 4)  // 2 pixels 4 bytes not supported with SSE2
      //MAKE_SAD_FN(2, 2)
      //MAKE_SAD_FN(2, 1)
#undef MAKE_SAD_FN


    SADFunction *result = nullptr;
    arch_t archlist[] = { USE_AVX2, USE_AVX, USE_SSE41, USE_SSE2, NO_SIMD };
    int index = 0;
    while (result == nullptr) {
      arch_t current_arch_try = archlist[index++];
      if (current_arch_try > arch) continue;
      if (result == nullptr && current_arch_try == NO_SIMD) {
        if(arch==USE_AVX2)
          result = get_sad_avx2_C_function(BlockX, BlockY, pixelsize, NO_SIMD);
        else if(arch==USE_AVX)
          result = get_sad_avx_C_function(BlockX, BlockY, pixelsize, NO_SIMD);
        else
          result = func_sad[make_tuple(BlockX, BlockY, pixelsize, NO_SIMD)];
      }
      else {
        result = func_sad[make_tuple(BlockX, BlockY, pixelsize, current_arch_try)];
      }
      if (result == nullptr && current_arch_try == NO_SIMD) {
        break;
      }
    }

    return result;
}

// SATD functions for blocks over 16x16 are not defined in pixel-a.asm,
// so as a poor man's substitute, we use a sum of smaller SATD functions.
// 8 bit only
#define	SATD_REC_FUNC(blsizex, blsizey, sblx, sbly, type)	extern "C" unsigned int __cdecl	x264_pixel_satd_##blsizex##x##blsizey##_##type (const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch)	\
{ \
  if(blsizex / sblx == 2 && blsizey / sbly == 2) \
    return \
      x264_pixel_satd_##sblx##x##sbly##_##type (pSrc,                     nSrcPitch, pRef,                     nRefPitch) \
    + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc+sblx,                nSrcPitch, pRef+sblx,                nRefPitch) \
    + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc     +sbly*nSrcPitch, nSrcPitch, pRef     +sbly*nRefPitch, nRefPitch) \
    + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc+sblx+sbly*nSrcPitch, nSrcPitch, pRef+sblx+sbly*nRefPitch, nRefPitch);\
  else if(blsizex / sblx == 3 && blsizey / sbly == 3) \
    return \
    + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc+0*sblx     +0*sbly*nSrcPitch, nSrcPitch, pRef+0*sblx     +0*sbly*nRefPitch, nRefPitch) \
    + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc+1*sblx     +0*sbly*nSrcPitch, nSrcPitch, pRef+1*sblx     +0*sbly*nRefPitch, nRefPitch) \
    + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc+2*sblx     +0*sbly*nSrcPitch, nSrcPitch, pRef+2*sblx     +0*sbly*nRefPitch, nRefPitch) \
    + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc+0*sblx     +1*sbly*nSrcPitch, nSrcPitch, pRef+0*sblx     +1*sbly*nRefPitch, nRefPitch) \
    + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc+1*sblx     +1*sbly*nSrcPitch, nSrcPitch, pRef+1*sblx     +1*sbly*nRefPitch, nRefPitch) \
    + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc+2*sblx     +1*sbly*nSrcPitch, nSrcPitch, pRef+2*sblx     +1*sbly*nRefPitch, nRefPitch) \
    + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc+0*sblx     +2*sbly*nSrcPitch, nSrcPitch, pRef+0*sblx     +2*sbly*nRefPitch, nRefPitch) \
    + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc+1*sblx     +2*sbly*nSrcPitch, nSrcPitch, pRef+1*sblx     +2*sbly*nRefPitch, nRefPitch) \
    + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc+2*sblx     +2*sbly*nSrcPitch, nSrcPitch, pRef+2*sblx     +2*sbly*nRefPitch, nRefPitch); \
  else if(blsizex / sblx == 4 && blsizey / sbly == 4) \
    return \
    + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc+0*sblx     +0*sbly*nSrcPitch, nSrcPitch, pRef+0*sblx     +0*sbly*nRefPitch, nRefPitch) \
    + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc+1*sblx     +0*sbly*nSrcPitch, nSrcPitch, pRef+1*sblx     +0*sbly*nRefPitch, nRefPitch) \
    + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc+2*sblx     +0*sbly*nSrcPitch, nSrcPitch, pRef+2*sblx     +0*sbly*nRefPitch, nRefPitch) \
    + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc+3*sblx     +0*sbly*nSrcPitch, nSrcPitch, pRef+3*sblx     +0*sbly*nRefPitch, nRefPitch) \
    + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc+0*sblx     +1*sbly*nSrcPitch, nSrcPitch, pRef+0*sblx     +1*sbly*nRefPitch, nRefPitch) \
    + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc+1*sblx     +1*sbly*nSrcPitch, nSrcPitch, pRef+1*sblx     +1*sbly*nRefPitch, nRefPitch) \
    + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc+2*sblx     +1*sbly*nSrcPitch, nSrcPitch, pRef+2*sblx     +1*sbly*nRefPitch, nRefPitch) \
    + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc+3*sblx     +1*sbly*nSrcPitch, nSrcPitch, pRef+3*sblx     +1*sbly*nRefPitch, nRefPitch) \
    + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc+0*sblx     +2*sbly*nSrcPitch, nSrcPitch, pRef+0*sblx     +2*sbly*nRefPitch, nRefPitch) \
    + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc+1*sblx     +2*sbly*nSrcPitch, nSrcPitch, pRef+1*sblx     +2*sbly*nRefPitch, nRefPitch) \
    + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc+2*sblx     +2*sbly*nSrcPitch, nSrcPitch, pRef+2*sblx     +2*sbly*nRefPitch, nRefPitch) \
    + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc+3*sblx     +2*sbly*nSrcPitch, nSrcPitch, pRef+3*sblx     +2*sbly*nRefPitch, nRefPitch) \
    + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc+0*sblx     +3*sbly*nSrcPitch, nSrcPitch, pRef+0*sblx     +3*sbly*nRefPitch, nRefPitch) \
    + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc+1*sblx     +3*sbly*nSrcPitch, nSrcPitch, pRef+1*sblx     +3*sbly*nRefPitch, nRefPitch) \
    + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc+2*sblx     +3*sbly*nSrcPitch, nSrcPitch, pRef+2*sblx     +3*sbly*nRefPitch, nRefPitch) \
    + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc+3*sblx     +3*sbly*nSrcPitch, nSrcPitch, pRef+3*sblx     +3*sbly*nRefPitch, nRefPitch); \
  else if(blsizex / sblx == 2 && blsizey / sbly == 4) \
    return \
    + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc+0*sblx     +0*sbly*nSrcPitch, nSrcPitch, pRef+0*sblx     +0*sbly*nRefPitch, nRefPitch) \
    + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc+1*sblx     +0*sbly*nSrcPitch, nSrcPitch, pRef+1*sblx     +0*sbly*nRefPitch, nRefPitch) \
    + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc+0*sblx     +1*sbly*nSrcPitch, nSrcPitch, pRef+0*sblx     +1*sbly*nRefPitch, nRefPitch) \
    + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc+1*sblx     +1*sbly*nSrcPitch, nSrcPitch, pRef+1*sblx     +1*sbly*nRefPitch, nRefPitch) \
    + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc+0*sblx     +2*sbly*nSrcPitch, nSrcPitch, pRef+0*sblx     +2*sbly*nRefPitch, nRefPitch) \
    + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc+1*sblx     +2*sbly*nSrcPitch, nSrcPitch, pRef+1*sblx     +2*sbly*nRefPitch, nRefPitch) \
    + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc+0*sblx     +3*sbly*nSrcPitch, nSrcPitch, pRef+0*sblx     +3*sbly*nRefPitch, nRefPitch) \
    + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc+1*sblx     +3*sbly*nSrcPitch, nSrcPitch, pRef+1*sblx     +3*sbly*nRefPitch, nRefPitch); \
  else if(blsizex / sblx == 4 && blsizey / sbly == 2) \
    return \
    + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc+0*sblx     +0*sbly*nSrcPitch, nSrcPitch, pRef+0*sblx     +0*sbly*nRefPitch, nRefPitch) \
    + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc+1*sblx     +0*sbly*nSrcPitch, nSrcPitch, pRef+1*sblx     +0*sbly*nRefPitch, nRefPitch) \
    + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc+2*sblx     +0*sbly*nSrcPitch, nSrcPitch, pRef+2*sblx     +0*sbly*nRefPitch, nRefPitch) \
    + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc+3*sblx     +0*sbly*nSrcPitch, nSrcPitch, pRef+3*sblx     +0*sbly*nRefPitch, nRefPitch) \
    + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc+0*sblx     +1*sbly*nSrcPitch, nSrcPitch, pRef+0*sblx     +1*sbly*nRefPitch, nRefPitch) \
    + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc+1*sblx     +1*sbly*nSrcPitch, nSrcPitch, pRef+1*sblx     +1*sbly*nRefPitch, nRefPitch) \
    + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc+2*sblx     +1*sbly*nSrcPitch, nSrcPitch, pRef+2*sblx     +1*sbly*nRefPitch, nRefPitch) \
    + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc+3*sblx     +1*sbly*nSrcPitch, nSrcPitch, pRef+3*sblx     +1*sbly*nRefPitch, nRefPitch);\
  assert(0); \
  return 0; \
}

// compound SATD when final blocksize is built from vertical base block arrangement
// blsizex, blsizey: final blocksize
// sblx, sbly: building stone
#define	SATD_REC_FUNC_VERT_ONLY(blsizex, blsizey, sblx, sbly, type)	extern "C" unsigned int __cdecl	x264_pixel_satd_##blsizex##x##blsizey##_##type (const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch)	\
{ \
  if (blsizex != sblx) \
    assert(0); \
  else if(blsizey / sbly == 2) \
    return \
      x264_pixel_satd_##sblx##x##sbly##_##type (pSrc,                     nSrcPitch, pRef,                     nRefPitch) \
    + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc     +sbly*nSrcPitch, nSrcPitch, pRef     +sbly*nRefPitch, nRefPitch);\
  else if(blsizey / sbly == 3) \
    return \
      x264_pixel_satd_##sblx##x##sbly##_##type (pSrc,                       nSrcPitch, pRef,                       nRefPitch) \
    + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc       +sbly*nSrcPitch, nSrcPitch, pRef       +sbly*nRefPitch, nRefPitch) \
    + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc     +2*sbly*nSrcPitch, nSrcPitch, pRef     +2*sbly*nRefPitch, nRefPitch); \
  else if(blsizey / sbly == 4) \
    return \
      x264_pixel_satd_##sblx##x##sbly##_##type (pSrc,                       nSrcPitch, pRef,                       nRefPitch) \
    + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc       +sbly*nSrcPitch, nSrcPitch, pRef       +sbly*nRefPitch, nRefPitch) \
    + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc     +2*sbly*nSrcPitch, nSrcPitch, pRef     +2*sbly*nRefPitch, nRefPitch) \
    + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc     +3*sbly*nSrcPitch, nSrcPitch, pRef     +3*sbly*nRefPitch, nRefPitch);\
  else if(blsizey / sbly == 8) \
    return \
      x264_pixel_satd_##sblx##x##sbly##_##type (pSrc,                       nSrcPitch, pRef,                       nRefPitch) \
    + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc       +sbly*nSrcPitch, nSrcPitch, pRef       +sbly*nRefPitch, nRefPitch) \
    + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc     +2*sbly*nSrcPitch, nSrcPitch, pRef     +2*sbly*nRefPitch, nRefPitch) \
    + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc     +3*sbly*nSrcPitch, nSrcPitch, pRef     +3*sbly*nRefPitch, nRefPitch) \
    + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc     +4*sbly*nSrcPitch, nSrcPitch, pRef     +4*sbly*nRefPitch, nRefPitch) \
    + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc     +5*sbly*nSrcPitch, nSrcPitch, pRef     +5*sbly*nRefPitch, nRefPitch) \
    + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc     +6*sbly*nSrcPitch, nSrcPitch, pRef     +6*sbly*nRefPitch, nRefPitch)\
    + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc     +7*sbly*nSrcPitch, nSrcPitch, pRef     +7*sbly*nRefPitch, nRefPitch);\
  assert(0); \
  return 0; \
}

// compound SATD when final blocksize is build from horizontal base block arrangement
// blsizex, blsizey: final blocksize
// sblx, sbly: building stone
#define	SATD_REC_FUNC_HORIZ_ONLY(blsizex, blsizey, sblx, sbly, type)	extern "C" unsigned int __cdecl	x264_pixel_satd_##blsizex##x##blsizey##_##type (const uint8_t *pSrc, int nSrcPitch, const uint8_t *pRef, int nRefPitch)	\
{ \
  if (blsizex != sblx) \
    assert(0); \
  else if(blsizex / sblx == 2) \
    return \
    x264_pixel_satd_##sblx##x##sbly##_##type (pSrc,                     nSrcPitch, pRef,                     nRefPitch)\
  + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc+sblx,                nSrcPitch, pRef+sblx,                nRefPitch);\
  else if(blsizex / sblx == 4) \
    return \
    x264_pixel_satd_##sblx##x##sbly##_##type (pSrc,                       nSrcPitch, pRef,                       nRefPitch)\
  + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc+sblx,                  nSrcPitch, pRef+sblx,                  nRefPitch)\
  + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc+2*sblx,                nSrcPitch, pRef+2*sblx,                nRefPitch)\
  + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc+3*sblx,                nSrcPitch, pRef+3*sblx,                nRefPitch);\
  else if(blsizex / sblx == 8) \
    return \
    x264_pixel_satd_##sblx##x##sbly##_##type (pSrc,                       nSrcPitch, pRef,                       nRefPitch)\
  + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc+sblx,                  nSrcPitch, pRef+sblx,                  nRefPitch)\
  + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc+2*sblx,                nSrcPitch, pRef+2*sblx,                nRefPitch)\
  + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc+3*sblx,                nSrcPitch, pRef+3*sblx,                nRefPitch)\
  + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc+4*sblx,                nSrcPitch, pRef+4*sblx,                nRefPitch)\
  + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc+5*sblx,                nSrcPitch, pRef+5*sblx,                nRefPitch)\
  + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc+6*sblx,                nSrcPitch, pRef+6*sblx,                nRefPitch)\
  + x264_pixel_satd_##sblx##x##sbly##_##type (pSrc+7*sblx,                nSrcPitch, pRef+7*sblx,                nRefPitch);\
  assert(0); \
  return 0; \
}

// some SATD functions do not exist in native form

// make 12x12 from 3x3 4x4
SATD_REC_FUNC(12, 12, 4, 4, mmx2)
// make 12x24 from 2 12x12
SATD_REC_FUNC_VERT_ONLY(12, 24, 12, 12, mmx2)
// make 12x48 from 3x 12x16
SATD_REC_FUNC_VERT_ONLY(12, 48, 12, 16, sse2)
SATD_REC_FUNC_VERT_ONLY(12, 48, 12, 16, sse4)
SATD_REC_FUNC_VERT_ONLY(12, 48, 12, 16, avx)
// make 32x4 from 2x 16x4
SATD_REC_FUNC_HORIZ_ONLY(32, 4, 16, 4, sse2)
SATD_REC_FUNC_HORIZ_ONLY(32, 4, 16, 4, sse4)
SATD_REC_FUNC_HORIZ_ONLY(32, 4, 16, 4, avx)
// Some satd block sizes do no exist in x86/x64
#ifdef _M_X64
// make 4x32 from 2x 4x16
SATD_REC_FUNC_VERT_ONLY(4, 32, 4, 16, sse2)
#endif

#undef SATD_REC_FUNC



SADFunction* get_satd_function(int BlockX, int BlockY, int pixelsize, arch_t arch)
{
    // 8 bit only (pixelsize==1)
    // BlkSizeX, BlkSizeY, pixelsize, arch_t
    std::map<std::tuple<int, int, int, arch_t>, SADFunction*> func_satd;
    using std::make_tuple;

    //made C callable function prototypes in SadFunction.h
    //some not implemented asm macros are in separate functions (e.g. 32x32 = 16x16 + 16x16 + 16x16 + 16x16) 
    //x264_pixel_satd_##blksizex##x##blksizey##_sse2/sse4/ssse3/avx/avx2   in pixel-a.asm
    
#ifdef _M_X64
    func_satd[make_tuple(64, 64, 1, USE_AVX2)] = x264_pixel_satd_64x64_avx2;
    func_satd[make_tuple(64, 48, 1, USE_AVX2)] = x264_pixel_satd_64x48_avx2;
    func_satd[make_tuple(64, 32, 1, USE_AVX2)] = x264_pixel_satd_64x32_avx2;
    func_satd[make_tuple(64, 16, 1, USE_AVX2)] = x264_pixel_satd_64x16_avx2;
    func_satd[make_tuple(48, 64, 1, USE_AVX2)] = x264_pixel_satd_48x64_avx2;
    func_satd[make_tuple(48, 48, 1, USE_AVX2)] = x264_pixel_satd_48x48_avx2;
    func_satd[make_tuple(48, 24, 1, USE_AVX2)] = x264_pixel_satd_48x24_avx2;
    func_satd[make_tuple(48, 12, 1, USE_AVX2)] = x264_pixel_satd_48x12_avx2;
    func_satd[make_tuple(32, 64, 1, USE_AVX2)] = x264_pixel_satd_32x64_avx2;
    func_satd[make_tuple(32, 32, 1, USE_AVX2)] = x264_pixel_satd_32x32_avx2;
    func_satd[make_tuple(32, 16, 1, USE_AVX2)] = x264_pixel_satd_32x16_avx2;
    func_satd[make_tuple(32, 8, 1, USE_AVX2)] = x264_pixel_satd_32x8_avx2;

    func_satd[make_tuple(16, 64, 1, USE_AVX2)] = x264_pixel_satd_16x64_avx2;
    func_satd[make_tuple(16, 32, 1, USE_AVX2)] = x264_pixel_satd_16x32_avx2;
#endif
    func_satd[make_tuple(16, 16, 1, USE_AVX2)] = x264_pixel_satd_16x16_avx2;
    func_satd[make_tuple(16, 8 , 1, USE_AVX2)] = x264_pixel_satd_16x8_avx2;
    func_satd[make_tuple(8 , 16, 1, USE_AVX2)] = x264_pixel_satd_8x16_avx2;
    func_satd[make_tuple(8 , 8 , 1, USE_AVX2)] = x264_pixel_satd_8x8_avx2;

    func_satd[make_tuple(64, 64, 1, USE_AVX)] = x264_pixel_satd_64x64_avx;
    func_satd[make_tuple(64, 48, 1, USE_AVX)] = x264_pixel_satd_64x48_avx;
    func_satd[make_tuple(64, 32, 1, USE_AVX)] = x264_pixel_satd_64x32_avx;
    func_satd[make_tuple(64, 16, 1, USE_AVX)] = x264_pixel_satd_64x16_avx;
    func_satd[make_tuple(48, 64, 1, USE_AVX)] = x264_pixel_satd_48x64_avx;
    func_satd[make_tuple(48, 48, 1, USE_AVX)] = x264_pixel_satd_48x48_avx;
    func_satd[make_tuple(48, 24, 1, USE_AVX)] = x264_pixel_satd_48x24_avx;
    func_satd[make_tuple(48, 12, 1, USE_AVX)] = x264_pixel_satd_48x12_avx;
    func_satd[make_tuple(32, 64, 1, USE_AVX)] = x264_pixel_satd_32x64_avx;
    func_satd[make_tuple(32, 32, 1, USE_AVX)] = x264_pixel_satd_32x32_avx;
    func_satd[make_tuple(32, 24, 1, USE_AVX)] = x264_pixel_satd_32x24_avx;
    func_satd[make_tuple(32, 16, 1, USE_AVX)] = x264_pixel_satd_32x16_avx;
    func_satd[make_tuple(32,  8, 1, USE_AVX)] = x264_pixel_satd_32x8_avx;
    func_satd[make_tuple(32,  4, 1, USE_AVX)] = x264_pixel_satd_32x4_avx;
    func_satd[make_tuple(24, 48, 1, USE_AVX)] = x264_pixel_satd_24x48_avx;
    func_satd[make_tuple(24, 32, 1, USE_AVX)] = x264_pixel_satd_24x32_avx;
    func_satd[make_tuple(24, 24, 1, USE_AVX)] = x264_pixel_satd_24x24_avx;
    func_satd[make_tuple(24, 12, 1, USE_AVX)] = x264_pixel_satd_24x12_avx;
    func_satd[make_tuple(16, 64, 1, USE_AVX)] = x264_pixel_satd_16x64_avx;
    func_satd[make_tuple(16, 32, 1, USE_AVX)] = x264_pixel_satd_16x32_avx;
    func_satd[make_tuple(16, 16, 1, USE_AVX)] = x264_pixel_satd_16x16_avx;
    func_satd[make_tuple(16, 12, 1, USE_AVX)] = x264_pixel_satd_16x12_avx;
    func_satd[make_tuple(16, 8 , 1, USE_AVX)] = x264_pixel_satd_16x8_avx;
    func_satd[make_tuple(16, 4 , 1, USE_AVX)] = x264_pixel_satd_16x4_avx;
    // no 12x48, 12x24, 12x12. 12x48 generated from 12x16_avx
    func_satd[make_tuple(12, 48, 1, USE_AVX)] = x264_pixel_satd_12x48_avx;
    func_satd[make_tuple(12, 16, 1, USE_AVX)] = x264_pixel_satd_12x16_avx;
    func_satd[make_tuple(8 , 32, 1, USE_AVX)] = x264_pixel_satd_8x32_avx;
    func_satd[make_tuple(8 , 16, 1, USE_AVX)] = x264_pixel_satd_8x16_avx;
    func_satd[make_tuple(8 , 8 , 1, USE_AVX)] = x264_pixel_satd_8x8_avx;
    func_satd[make_tuple(8 , 4 , 1, USE_AVX)] = x264_pixel_satd_8x4_avx;

    func_satd[make_tuple(64, 64, 1, USE_SSE41)] = x264_pixel_satd_64x64_sse4;
    func_satd[make_tuple(64, 48, 1, USE_SSE41)] = x264_pixel_satd_64x48_sse4;
    func_satd[make_tuple(64, 32, 1, USE_SSE41)] = x264_pixel_satd_64x32_sse4;
    func_satd[make_tuple(64, 16, 1, USE_SSE41)] = x264_pixel_satd_64x16_sse4;
    func_satd[make_tuple(48, 64, 1, USE_SSE41)] = x264_pixel_satd_48x64_sse4;
    func_satd[make_tuple(48, 48, 1, USE_SSE41)] = x264_pixel_satd_48x48_sse4;
    func_satd[make_tuple(48, 24, 1, USE_SSE41)] = x264_pixel_satd_48x24_sse4;
    func_satd[make_tuple(48, 12, 1, USE_SSE41)] = x264_pixel_satd_48x12_sse4;
    func_satd[make_tuple(32, 64, 1, USE_SSE41)] = x264_pixel_satd_32x64_sse4;
    func_satd[make_tuple(32, 32, 1, USE_SSE41)] = x264_pixel_satd_32x32_sse4;
    func_satd[make_tuple(32, 24, 1, USE_SSE41)] = x264_pixel_satd_32x24_sse4;
    func_satd[make_tuple(32, 16, 1, USE_SSE41)] = x264_pixel_satd_32x16_sse4;
    func_satd[make_tuple(32,  8, 1, USE_SSE41)] = x264_pixel_satd_32x8_sse4;
    func_satd[make_tuple(32,  4, 1, USE_SSE41)] = x264_pixel_satd_32x4_sse4;
    func_satd[make_tuple(24, 48, 1, USE_SSE41)] = x264_pixel_satd_24x48_sse4;
    func_satd[make_tuple(24, 32, 1, USE_SSE41)] = x264_pixel_satd_24x32_sse4;
    func_satd[make_tuple(24, 24, 1, USE_SSE41)] = x264_pixel_satd_24x24_sse4;
    func_satd[make_tuple(24, 12, 1, USE_SSE41)] = x264_pixel_satd_24x12_sse4;
    func_satd[make_tuple(16, 64, 1, USE_SSE41)] = x264_pixel_satd_16x64_sse4;
    func_satd[make_tuple(16, 32, 1, USE_SSE41)] = x264_pixel_satd_16x32_sse4;
    func_satd[make_tuple(16, 16, 1, USE_SSE41)] = x264_pixel_satd_16x16_sse4;
    func_satd[make_tuple(16, 12, 1, USE_SSE41)] = x264_pixel_satd_16x12_sse4;
    func_satd[make_tuple(16, 8 , 1, USE_SSE41)] = x264_pixel_satd_16x8_sse4;
    func_satd[make_tuple(16, 4 , 1, USE_SSE41)] = x264_pixel_satd_16x4_sse4;
    // no 12x48, 12x24, 12x12. 12x48 generated from 12x16_sse4
    func_satd[make_tuple(12, 48, 1, USE_SSE41)] = x264_pixel_satd_12x48_sse4;
    func_satd[make_tuple(12, 16, 1, USE_SSE41)] = x264_pixel_satd_12x16_sse4;
    func_satd[make_tuple(8 , 32, 1, USE_SSE41)] = x264_pixel_satd_8x32_sse4;
    func_satd[make_tuple(8 , 16, 1, USE_SSE41)] = x264_pixel_satd_8x16_sse4;
    func_satd[make_tuple(8 , 8 , 1, USE_SSE41)] = x264_pixel_satd_8x8_sse4;
    func_satd[make_tuple(8 , 4 , 1, USE_SSE41)] = x264_pixel_satd_8x4_sse4;
    func_satd[make_tuple(4,  8 , 1, USE_SSE41)] = x264_pixel_satd_4x8_sse4;
    func_satd[make_tuple(4,  4 , 1, USE_SSE41)] = x264_pixel_satd_4x4_sse4;

    func_satd[make_tuple(64, 64, 1, USE_SSE2)] = x264_pixel_satd_64x64_sse2;
    func_satd[make_tuple(64, 48, 1, USE_SSE2)] = x264_pixel_satd_64x48_sse2;
    func_satd[make_tuple(64, 32, 1, USE_SSE2)] = x264_pixel_satd_64x32_sse2;
    func_satd[make_tuple(64, 16, 1, USE_SSE2)] = x264_pixel_satd_64x16_sse2;
    func_satd[make_tuple(48, 64, 1, USE_SSE2)] = x264_pixel_satd_48x64_sse2;
    func_satd[make_tuple(48, 48, 1, USE_SSE2)] = x264_pixel_satd_48x48_sse2;
    func_satd[make_tuple(48, 24, 1, USE_SSE2)] = x264_pixel_satd_48x24_sse2;
    func_satd[make_tuple(48, 12, 1, USE_SSE2)] = x264_pixel_satd_48x12_sse2;
    func_satd[make_tuple(32, 64, 1, USE_SSE2)] = x264_pixel_satd_32x64_sse2;
    func_satd[make_tuple(32, 32, 1, USE_SSE2)] = x264_pixel_satd_32x32_sse2;
    func_satd[make_tuple(32, 16, 1, USE_SSE2)] = x264_pixel_satd_32x16_sse2;
    func_satd[make_tuple(32, 24, 1, USE_SSE2)] = x264_pixel_satd_32x24_sse2;
    func_satd[make_tuple(32,  8, 1, USE_SSE2)] = x264_pixel_satd_32x8_sse2;
    func_satd[make_tuple(32,  4, 1, USE_SSE2)] = x264_pixel_satd_32x4_sse2;
    func_satd[make_tuple(24, 48, 1, USE_SSE2)] = x264_pixel_satd_24x48_sse2;
    func_satd[make_tuple(24, 32, 1, USE_SSE2)] = x264_pixel_satd_24x32_sse2;
    func_satd[make_tuple(24, 24, 1, USE_SSE2)] = x264_pixel_satd_24x24_sse2;
    func_satd[make_tuple(24, 12, 1, USE_SSE2)] = x264_pixel_satd_24x12_sse2;
    func_satd[make_tuple(16, 64, 1, USE_SSE2)] = x264_pixel_satd_16x64_sse2;
    func_satd[make_tuple(16, 32, 1, USE_SSE2)] = x264_pixel_satd_16x32_sse2;
    func_satd[make_tuple(16, 16, 1, USE_SSE2)] = x264_pixel_satd_16x16_sse2;
    func_satd[make_tuple(16, 12, 1, USE_SSE2)] = x264_pixel_satd_16x12_sse2;
    func_satd[make_tuple(16, 8 , 1, USE_SSE2)] = x264_pixel_satd_16x8_sse2;
    func_satd[make_tuple(16, 4 , 1, USE_SSE2)] = x264_pixel_satd_16x4_sse2;
    // no 12x48, 12x24, 12x12
    // 12x48 generated from 12x16 sse2, 
    // 12x24, 12x12 generated from 4x4 mmx2, 
    func_satd[make_tuple(12, 48, 1, USE_SSE2)] = x264_pixel_satd_12x48_sse2;
    func_satd[make_tuple(12, 24, 1, USE_SSE2)] = x264_pixel_satd_12x24_mmx2;
    func_satd[make_tuple(12, 16, 1, USE_SSE2)] = x264_pixel_satd_12x16_sse2;
    func_satd[make_tuple(12, 12, 1, USE_SSE2)] = x264_pixel_satd_12x12_mmx2;
    func_satd[make_tuple(8 , 32, 1, USE_SSE2)] = x264_pixel_satd_8x32_sse2;
    func_satd[make_tuple(8 , 16, 1, USE_SSE2)] = x264_pixel_satd_8x16_sse2;
    func_satd[make_tuple(8 , 8 , 1, USE_SSE2)] = x264_pixel_satd_8x8_sse2;
    func_satd[make_tuple(8 , 4 , 1, USE_SSE2)] = x264_pixel_satd_8x4_sse2;
    func_satd[make_tuple(4 , 32, 1, USE_SSE2)] = x264_pixel_satd_4x32_sse2;
    func_satd[make_tuple(4 , 16, 1, USE_SSE2)] = x264_pixel_satd_4x16_sse2;
    func_satd[make_tuple(4 , 8 , 1, USE_SSE2)] = x264_pixel_satd_4x8_sse2;
    func_satd[make_tuple(4 , 4 , 1, USE_SSE2)] = x264_pixel_satd_4x4_mmx2;

    // 16 bit C, SSE4/SSE2 SADT functions
    // e.g.. satd16_64x64_sse2<false>;
#define MAKE_FN(w,h) \
    func_satd[make_tuple(w, h, 2, NO_SIMD)] = mvtools_satd_uint16_##w##x##h##_c; \
    func_satd[make_tuple(w, h, 2, USE_SSE41)] = satd16_##w##x##h##_sse2<true>; \
    func_satd[make_tuple(w, h, 2, USE_SSE2)] = satd16_##w##x##h##_sse2<false>;
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
      MAKE_FN(32, 4)
      MAKE_FN(24, 48)
      MAKE_FN(24, 32)
      MAKE_FN(24, 24)
      MAKE_FN(24, 12)
      MAKE_FN(16, 64)
      MAKE_FN(16, 32)
      MAKE_FN(16, 16)
      MAKE_FN(16, 12)
      MAKE_FN(16, 8)
      MAKE_FN(16, 4)
      MAKE_FN(12, 48)
      MAKE_FN(12, 24)
      MAKE_FN(12, 16)
      MAKE_FN(12, 12)
      MAKE_FN(8, 32)
      MAKE_FN(8, 16)
      MAKE_FN(8, 8)
      MAKE_FN(8, 4)
      MAKE_FN(4, 32)
      MAKE_FN(4, 16)
      MAKE_FN(4, 8)
      MAKE_FN(4, 4)
#undef MAKE_FN

    SADFunction *result = nullptr;
    arch_t archlist[] = { USE_AVX2, USE_AVX, USE_SSE41, USE_SSE2, NO_SIMD };
    int index = 0;
    while (result == nullptr) {
      arch_t current_arch_try = archlist[index++];
      if (current_arch_try > arch) continue;
      if (result == nullptr && current_arch_try == NO_SIMD) {
        result = func_satd[make_tuple(BlockX, BlockY, pixelsize, NO_SIMD)];
      }
      else {
        result = func_satd[make_tuple(BlockX, BlockY, pixelsize, current_arch_try)];
      }
      if (result == nullptr && current_arch_try == NO_SIMD) {
        break;
      }
    }
    return result;
}


