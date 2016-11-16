#include "SADFunctions_avx.h"
//#include "overlap.h"
#include <map>
#include <tuple>

#include <emmintrin.h>

inline unsigned int SADABS(int x) {	return ( x < 0 ) ? -x : x; }

// simple AVX still does not support 256 bit integer SIMD versions of SSE2 functions
// Still the compiler can generate more optimized code
template<int nBlkWidth, int nBlkHeight, typename pixel_t>
static unsigned int Sad_AVX_C(const uint8_t *pSrc, int nSrcPitch,const uint8_t *pRef,
  int nRefPitch)
{
  _mm256_zeroupper();
  unsigned int sum = 0; // int is probably enough for 32x32
  for ( int y = 0; y < nBlkHeight; y++ )
  {
    for ( int x = 0; x < nBlkWidth; x++ )
      sum += SADABS(reinterpret_cast<const pixel_t *>(pSrc)[x] - reinterpret_cast<const pixel_t *>(pRef)[x]);
    pSrc += nSrcPitch;
    pRef += nRefPitch;
  }
  _mm256_zeroupper();
  return sum;
}



SADFunction* get_sad_avx_C_function(int BlockX, int BlockY, int pixelsize, arch_t arch)
{
    // BlkSizeX, BlkSizeY, pixelsize, arch_t
    std::map<std::tuple<int, int, int, arch_t>, SADFunction*> func_sad;
    using std::make_tuple;

    func_sad[make_tuple(32, 32, 1, NO_SIMD)] = Sad_AVX_C<32, 32,uint8_t>;
    func_sad[make_tuple(32, 16, 1, NO_SIMD)] = Sad_AVX_C<32, 16,uint8_t>;
    func_sad[make_tuple(32, 8 , 1, NO_SIMD)] = Sad_AVX_C<32, 8,uint8_t>;
    func_sad[make_tuple(16, 32, 1, NO_SIMD)] = Sad_AVX_C<16, 32,uint8_t>;
    func_sad[make_tuple(16, 16, 1, NO_SIMD)] = Sad_AVX_C<16, 16,uint8_t>;
    func_sad[make_tuple(16, 8 , 1, NO_SIMD)] = Sad_AVX_C<16, 8,uint8_t>;
    func_sad[make_tuple(16, 4 , 1, NO_SIMD)] = Sad_AVX_C<16, 4,uint8_t>;
    func_sad[make_tuple(16, 2 , 1, NO_SIMD)] = Sad_AVX_C<16, 2,uint8_t>;
    func_sad[make_tuple(16, 1 , 1, NO_SIMD)] = Sad_AVX_C<16, 1,uint8_t>;
    func_sad[make_tuple(8 , 16, 1, NO_SIMD)] = Sad_AVX_C<8 , 16,uint8_t>;
    func_sad[make_tuple(8 , 8 , 1, NO_SIMD)] = Sad_AVX_C<8 , 8,uint8_t>;
    func_sad[make_tuple(8 , 4 , 1, NO_SIMD)] = Sad_AVX_C<8 , 4,uint8_t>;
    func_sad[make_tuple(8 , 2 , 1, NO_SIMD)] = Sad_AVX_C<8 , 2,uint8_t>;
    func_sad[make_tuple(8 , 1 , 1, NO_SIMD)] = Sad_AVX_C<8 , 1,uint8_t>;
    func_sad[make_tuple(4 , 8 , 1, NO_SIMD)] = Sad_AVX_C<4 , 8,uint8_t>;
    func_sad[make_tuple(4 , 4 , 1, NO_SIMD)] = Sad_AVX_C<4 , 4,uint8_t>;
    func_sad[make_tuple(4 , 2 , 1, NO_SIMD)] = Sad_AVX_C<4 , 2,uint8_t>;
    func_sad[make_tuple(4 , 1 , 1, NO_SIMD)] = Sad_AVX_C<4 , 1,uint8_t>;
    func_sad[make_tuple(2 , 4 , 1, NO_SIMD)] = Sad_AVX_C<2 , 4,uint8_t>;
    func_sad[make_tuple(2 , 2 , 1, NO_SIMD)] = Sad_AVX_C<2 , 2,uint8_t>;
    func_sad[make_tuple(2 , 1 , 1, NO_SIMD)] = Sad_AVX_C<2 , 1,uint8_t>;

    func_sad[make_tuple(32, 32, 2, NO_SIMD)] = Sad_AVX_C<32, 32,uint16_t>;
    func_sad[make_tuple(32, 16, 2, NO_SIMD)] = Sad_AVX_C<32, 16,uint16_t>;
    func_sad[make_tuple(32, 8 , 2, NO_SIMD)] = Sad_AVX_C<32, 8,uint16_t>;
    func_sad[make_tuple(16, 32, 2, NO_SIMD)] = Sad_AVX_C<16, 32,uint16_t>;
    func_sad[make_tuple(16, 16, 2, NO_SIMD)] = Sad_AVX_C<16, 16,uint16_t>;
    func_sad[make_tuple(16, 8 , 2, NO_SIMD)] = Sad_AVX_C<16, 8,uint16_t>;
    func_sad[make_tuple(16, 4 , 2, NO_SIMD)] = Sad_AVX_C<16, 4,uint16_t>;
    func_sad[make_tuple(16, 2 , 2, NO_SIMD)] = Sad_AVX_C<16, 2,uint16_t>;
    func_sad[make_tuple(16, 1 , 2, NO_SIMD)] = Sad_AVX_C<16, 1,uint16_t>;
    func_sad[make_tuple(8 , 16, 2, NO_SIMD)] = Sad_AVX_C<8 , 16,uint16_t>;
    func_sad[make_tuple(8 , 8 , 2, NO_SIMD)] = Sad_AVX_C<8 , 8,uint16_t>;
    func_sad[make_tuple(8 , 4 , 2, NO_SIMD)] = Sad_AVX_C<8 , 4,uint16_t>;
    func_sad[make_tuple(8 , 2 , 2, NO_SIMD)] = Sad_AVX_C<8 , 2,uint16_t>;
    func_sad[make_tuple(8 , 1 , 2, NO_SIMD)] = Sad_AVX_C<8 , 1,uint16_t>;
    func_sad[make_tuple(4 , 8 , 2, NO_SIMD)] = Sad_AVX_C<4 , 8,uint16_t>;
    func_sad[make_tuple(4 , 4 , 2, NO_SIMD)] = Sad_AVX_C<4 , 4,uint16_t>;
    func_sad[make_tuple(4 , 2 , 2, NO_SIMD)] = Sad_AVX_C<4 , 2,uint16_t>;
    func_sad[make_tuple(4 , 1 , 2, NO_SIMD)] = Sad_AVX_C<4 , 1,uint16_t>;
    func_sad[make_tuple(2 , 4 , 2, NO_SIMD)] = Sad_AVX_C<2 , 4,uint16_t>;
    func_sad[make_tuple(2 , 2 , 2, NO_SIMD)] = Sad_AVX_C<2 , 2,uint16_t>;
    func_sad[make_tuple(2 , 1 , 2, NO_SIMD)] = Sad_AVX_C<2 , 1,uint16_t>;

    SADFunction *result = func_sad[make_tuple(BlockX, BlockY, pixelsize, arch)];
    return result;
}

// above width==4 for uint16_t and width==8 for uint8_t
template<int nBlkWidth, int nBlkHeight, typename pixel_t>
unsigned int Sad16_sse2_avx(const uint8_t *pSrc, int nSrcPitch,const uint8_t *pRef, int nRefPitch)
{
  /*
  if (is_ptr_aligned(pSrc, 16) && is_ptr_aligned(pref, 16)) {
  // aligned code. though newer procs don't care
  }
  */
  //__assume_aligned(piMblk, 16);
  //__assume_aligned(piRef, 16);
  // check. int result2 = Sad_C<nBlkWidth, nBlkHeight, pixel_t>(pSrc, nSrcPitch, pRef, nRefPitch);
  _mm256_zeroupper(); // diff from main sse2

  __m128i zero = _mm_setzero_si128();
  __m128i sum = _mm_setzero_si128(); // 2x or 4x int is probably enough for 32x32
  const bool two_rows = (sizeof(pixel_t) == 2 && nBlkWidth <= 4) || (sizeof(pixel_t) == 1 && nBlkWidth <= 8);
  const bool one_cycle = (sizeof(pixel_t) * nBlkWidth) == 16;

  for ( int y = 0; y < nBlkHeight; y+= (two_rows ? 2 : 1))
  {
    if (two_rows) { // no x cycle
      __m128i src1, src2;
      // (8 bytes or 4 words) * 2 rows
      src1 = _mm_or_si128(_mm_loadl_epi64((__m128i *) (pSrc)), _mm_slli_si128(_mm_loadl_epi64((__m128i *) (pSrc + nSrcPitch)), 8));
      src2 = _mm_or_si128(_mm_loadl_epi64((__m128i *) (pRef)), _mm_slli_si128(_mm_loadl_epi64((__m128i *) (pRef + nRefPitch)), 8));
      if (sizeof(pixel_t) == 1) {
        // this is uint_16 specific, but will test on uint8_t against external .asm SAD functions)
        sum = _mm_add_epi32(sum, _mm_sad_epu8(src1, src2)); // yihhaaa, existing SIMD   sum1_32, 0, sum2_32, 0
                                                            // result in two 32 bit areas at the upper and lower 64 bytes
      }
      else {
        __m128i greater_t = _mm_subs_epu16(src1, src2); // unsigned sub with saturation
        __m128i smaller_t = _mm_subs_epu16(src2, src1);
        __m128i absdiff = _mm_or_si128(greater_t, smaller_t); //abs(s1-s2)  == (satsub(s1,s2) | satsub(s2,s1))
                                                              // 8 x uint16 absolute differences
        sum = _mm_add_epi32(sum, _mm_unpacklo_epi16(absdiff, zero));
        sum = _mm_add_epi32(sum, _mm_unpackhi_epi16(absdiff, zero));
        // sum1_32, sum2_32, sum3_32, sum4_32
      }
    }
    else if (one_cycle)
    {
      __m128i src1, src2;
      src1 = _mm_loadu_si128((__m128i *) (pSrc)); // no x
      src2 = _mm_loadu_si128((__m128i *) (pRef));
      if (sizeof(pixel_t) == 1) {
        // this is uint_16 specific, but will test on uint8_t against external .asm SAD functions)
        sum = _mm_add_epi32(sum, _mm_sad_epu8(src1, src2)); // yihhaaa, existing SIMD   sum1_32, 0, sum2_32, 0
                                                            // result in two 32 bit areas at the upper and lower 64 bytes
      }
      else {
        __m128i greater_t = _mm_subs_epu16(src1, src2); // unsigned sub with saturation
        __m128i smaller_t = _mm_subs_epu16(src2, src1);
        __m128i absdiff = _mm_or_si128(greater_t, smaller_t); //abs(s1-s2)  == (satsub(s1,s2) | satsub(s2,s1))
                                                              // 8 x uint16 absolute differences
        sum = _mm_add_epi32(sum, _mm_unpacklo_epi16(absdiff, zero));
        sum = _mm_add_epi32(sum, _mm_unpackhi_epi16(absdiff, zero));
        // sum1_32, sum2_32, sum3_32, sum4_32
      }
    }
    else {
      for (int x = 0; x < nBlkWidth; x += 16)
      {
        __m128i src1, src2;
        src1 = _mm_loadu_si128((__m128i *) (pSrc + x));
        src2 = _mm_loadu_si128((__m128i *) (pRef + x));
        if (sizeof(pixel_t) == 1) {
          // this is uint_16 specific, but will test on uint8_t against external .asm SAD functions)
          sum = _mm_add_epi32(sum, _mm_sad_epu8(src1, src2)); // yihhaaa, existing SIMD   sum1_32, 0, sum2_32, 0
                                                              // result in two 32 bit areas at the upper and lower 64 bytes
        }
        else {
          __m128i greater_t = _mm_subs_epu16(src1, src2); // unsigned sub with saturation
          __m128i smaller_t = _mm_subs_epu16(src2, src1);
          __m128i absdiff = _mm_or_si128(greater_t, smaller_t); //abs(s1-s2)  == (satsub(s1,s2) | satsub(s2,s1))
                                                                // 8 x uint16 absolute differences
          sum = _mm_add_epi32(sum, _mm_unpacklo_epi16(absdiff, zero));
          sum = _mm_add_epi32(sum, _mm_unpackhi_epi16(absdiff, zero));
          // sum1_32, sum2_32, sum3_32, sum4_32
        }
        // sum += SADABS(reinterpret_cast<const pixel_t *>(pSrc)[x] - reinterpret_cast<const pixel_t *>(pRef)[x]);
      }
    }
    if (two_rows) {
      pSrc += nSrcPitch*2;
      pRef += nRefPitch*2;
    } else {
      pSrc += nSrcPitch;
      pRef += nRefPitch;
    }
  }
  /*
  [Low64, Hi64]
  _mm_unpacklo_epi64(_mm_setzero_si128(), x)  [0, x0]
  _mm_unpackhi_epi64(_mm_setzero_si128(), x)  [0, x1]
  _mm_move_epi64(x)                           [x0, 0]
  _mm_unpackhi_epi64(x, _mm_setzero_si128())  [x1, 0]
  */
  if(sizeof(pixel_t) == 2) {
    // at 16 bits: we have 4 integers for sum: a0 a1 a2 a3
    __m128i a0_a1 = _mm_unpacklo_epi32(sum, zero); // a0 0 a1 0
    __m128i a2_a3 = _mm_unpackhi_epi32(sum, zero); // a2 0 a3 0
    sum = _mm_add_epi32( a0_a1, a2_a3 ); // a0+a2, 0, a1+a3, 0

                                         /* SSSE3:
                                         sum = _mm_hadd_epi32(sum, zero);  // A1+A2, B1+B2, 0+0, 0+0
                                         sum = _mm_hadd_epi32(sum, zero);  // A1+A2+B1+B2, 0+0+0+0, 0+0+0+0, 0+0+0+0
                                         */
  }
  // sum here: two 32 bit partial result: sum1 0 sum2 0
  __m128i sum_hi = _mm_unpackhi_epi64(sum, zero); // a1 + a3. 2 dwords right 
  sum = _mm_add_epi32(sum, sum_hi);  // a0 + a2 + a1 + a3

  unsigned int result = _mm_cvtsi128_si32(sum);

  // check  
  /*if (result != result2)
  result = result2;*/
  _mm256_zeroupper(); // diff from main sse2

  return result;
}
