#include "SADFunctions_avx2.h"
//#include "overlap.h"
#include <map>
#include <tuple>
#include <immintrin.h>
#include <stdint.h>

inline unsigned int SADABS(int x) {	return ( x < 0 ) ? -x : x; }

template<int nBlkWidth, int nBlkHeight, typename pixel_t>
static unsigned int Sad_AVX2_C(const uint8_t *pSrc, int nSrcPitch,const uint8_t *pRef, int nRefPitch)
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


SADFunction* get_sad_avx2_C_function(int BlockX, int BlockY, int pixelsize, arch_t arch)
{
    // BlkSizeX, BlkSizeY, pixelsize, arch_t
    std::map<std::tuple<int, int, int, arch_t>, SADFunction*> func_sad;
    using std::make_tuple;

    func_sad[make_tuple(32, 32, 1, NO_SIMD)] = Sad_AVX2_C<32, 32,uint8_t>;
    func_sad[make_tuple(32, 16, 1, NO_SIMD)] = Sad_AVX2_C<32, 16,uint8_t>;
    func_sad[make_tuple(32, 8 , 1, NO_SIMD)] = Sad_AVX2_C<32, 8,uint8_t>;
    func_sad[make_tuple(16, 32, 1, NO_SIMD)] = Sad_AVX2_C<16, 32,uint8_t>;
    func_sad[make_tuple(16, 16, 1, NO_SIMD)] = Sad_AVX2_C<16, 16,uint8_t>;
    func_sad[make_tuple(16, 8 , 1, NO_SIMD)] = Sad_AVX2_C<16, 8,uint8_t>;
    func_sad[make_tuple(16, 4 , 1, NO_SIMD)] = Sad_AVX2_C<16, 4,uint8_t>;
    func_sad[make_tuple(16, 2 , 1, NO_SIMD)] = Sad_AVX2_C<16, 2,uint8_t>;
    func_sad[make_tuple(16, 1 , 1, NO_SIMD)] = Sad_AVX2_C<16, 1,uint8_t>;
    func_sad[make_tuple(8 , 16, 1, NO_SIMD)] = Sad_AVX2_C<8 , 16,uint8_t>;
    func_sad[make_tuple(8 , 8 , 1, NO_SIMD)] = Sad_AVX2_C<8 , 8,uint8_t>;
    func_sad[make_tuple(8 , 4 , 1, NO_SIMD)] = Sad_AVX2_C<8 , 4,uint8_t>;
    func_sad[make_tuple(8 , 2 , 1, NO_SIMD)] = Sad_AVX2_C<8 , 2,uint8_t>;
    func_sad[make_tuple(8 , 1 , 1, NO_SIMD)] = Sad_AVX2_C<8 , 1,uint8_t>;
    func_sad[make_tuple(4 , 8 , 1, NO_SIMD)] = Sad_AVX2_C<4 , 8,uint8_t>;
    func_sad[make_tuple(4 , 4 , 1, NO_SIMD)] = Sad_AVX2_C<4 , 4,uint8_t>;
    func_sad[make_tuple(4 , 2 , 1, NO_SIMD)] = Sad_AVX2_C<4 , 2,uint8_t>;
    func_sad[make_tuple(4 , 1 , 1, NO_SIMD)] = Sad_AVX2_C<4 , 1,uint8_t>;
    func_sad[make_tuple(2 , 4 , 1, NO_SIMD)] = Sad_AVX2_C<2 , 4,uint8_t>;
    func_sad[make_tuple(2 , 2 , 1, NO_SIMD)] = Sad_AVX2_C<2 , 2,uint8_t>;
    func_sad[make_tuple(2 , 1 , 1, NO_SIMD)] = Sad_AVX2_C<2 , 1,uint8_t>;

    func_sad[make_tuple(32, 32, 2, NO_SIMD)] = Sad_AVX2_C<32, 32,uint16_t>;
    func_sad[make_tuple(32, 16, 2, NO_SIMD)] = Sad_AVX2_C<32, 16,uint16_t>;
    func_sad[make_tuple(32, 8 , 2, NO_SIMD)] = Sad_AVX2_C<32, 8,uint16_t>;
    func_sad[make_tuple(16, 32, 2, NO_SIMD)] = Sad_AVX2_C<16, 32,uint16_t>;
    func_sad[make_tuple(16, 16, 2, NO_SIMD)] = Sad_AVX2_C<16, 16,uint16_t>;
    func_sad[make_tuple(16, 8 , 2, NO_SIMD)] = Sad_AVX2_C<16, 8,uint16_t>;
    func_sad[make_tuple(16, 4 , 2, NO_SIMD)] = Sad_AVX2_C<16, 4,uint16_t>;
    func_sad[make_tuple(16, 2 , 2, NO_SIMD)] = Sad_AVX2_C<16, 2,uint16_t>;
    func_sad[make_tuple(16, 1 , 2, NO_SIMD)] = Sad_AVX2_C<16, 1,uint16_t>;
    func_sad[make_tuple(8 , 16, 2, NO_SIMD)] = Sad_AVX2_C<8 , 16,uint16_t>;
    func_sad[make_tuple(8 , 8 , 2, NO_SIMD)] = Sad_AVX2_C<8 , 8,uint16_t>;
    func_sad[make_tuple(8 , 4 , 2, NO_SIMD)] = Sad_AVX2_C<8 , 4,uint16_t>;
    func_sad[make_tuple(8 , 2 , 2, NO_SIMD)] = Sad_AVX2_C<8 , 2,uint16_t>;
    func_sad[make_tuple(8 , 1 , 2, NO_SIMD)] = Sad_AVX2_C<8 , 1,uint16_t>;
    func_sad[make_tuple(4 , 8 , 2, NO_SIMD)] = Sad_AVX2_C<4 , 8,uint16_t>;
    func_sad[make_tuple(4 , 4 , 2, NO_SIMD)] = Sad_AVX2_C<4 , 4,uint16_t>;
    func_sad[make_tuple(4 , 2 , 2, NO_SIMD)] = Sad_AVX2_C<4 , 2,uint16_t>;
    func_sad[make_tuple(4 , 1 , 2, NO_SIMD)] = Sad_AVX2_C<4 , 1,uint16_t>;
    func_sad[make_tuple(2 , 4 , 2, NO_SIMD)] = Sad_AVX2_C<2 , 4,uint16_t>;
    func_sad[make_tuple(2 , 2 , 2, NO_SIMD)] = Sad_AVX2_C<2 , 2,uint16_t>;
    func_sad[make_tuple(2 , 1 , 2, NO_SIMD)] = Sad_AVX2_C<2 , 1,uint16_t>;
    
    SADFunction *result = func_sad[make_tuple(BlockX, BlockY, pixelsize, arch)];
    return result;
}


/*
// add 4 floats horizontally.
// sse3 (not ssse3)
float hsum_ps_sse3(__m128 v) {
__m128 shuf = _mm_movehdup_ps(v);        // broadcast elements 3,1 to 2,0
__m128 sums = _mm_add_ps(v, shuf);
shuf        = _mm_movehl_ps(shuf, sums); // high half -> low half
sums        = _mm_add_ss(sums, shuf);
return        _mm_cvtss_f32(sums);

or ssse3
v = _mm_hadd_ps(v, v);
v = _mm_hadd_ps(v, v);
or
const __m128 t = _mm_add_ps(v, _mm_movehl_ps(v, v));
const __m128 sum = _mm_add_ss(t, _mm_shuffle_ps(t, t, 1));
}
*/

// integer 256 with SSE2-like function from AVX2
// above width==16 for uint16_t and width==32 for uint8_t
template<int nBlkWidth, int nBlkHeight, typename pixel_t>
unsigned int Sad16_avx2(const uint8_t *pSrc, int nSrcPitch,const uint8_t *pRef, int nRefPitch)
{
  _mm256_zeroupper(); 
  //__assume_aligned(piMblk, 16);
  //__assume_aligned(piRef, 16);
  if ((sizeof(pixel_t) == 2 && nBlkWidth < 8) || (sizeof(pixel_t) == 1 && nBlkWidth <= 16)) {
    assert("AVX2 not supported for uint16_t with BlockSize<8 or uint8_t with blocksize<16");
    return 0;
  }

  __m256i zero = _mm256_setzero_si256();
  __m256i sum = _mm256_setzero_si256(); // 2x or 4x int is probably enough for 32x32

  bool two_rows = (sizeof(pixel_t) == 2 && nBlkWidth == 8) || (sizeof(pixel_t) == 1 && nBlkWidth == 16);

  for ( int y = 0; y < nBlkHeight; y++ )
  {
    for ( int x = 0; x < nBlkWidth; x+=32 )
    {
      __m256i src1, src2;
      if (two_rows)  {
        // two 16 byte rows at a time
        src1 = _mm256_loadu2_m128i((const __m128i *) (pSrc + nSrcPitch + x), (const __m128i *) (pSrc + x));
        src2 = _mm256_loadu2_m128i((const __m128i *) (pRef + nRefPitch + x), (const __m128i *) (pRef + x));
      }
      else {
        src1 = _mm256_loadu_si256((__m256i *) (pSrc + x));
        src2 = _mm256_loadu_si256((__m256i *) (pRef + x));
      }
      if(sizeof(pixel_t) == 1) {
        sum = _mm256_add_epi32(sum, _mm256_sad_epu8(src1, src2));
        // result in four 32 bit areas at each 64 bytes
      }
      else {
        // we have 16 words
#ifdef METHOD1
        // lower 8 word to 8 int32
        __m256i src1_lo = _mm256_unpacklo_epi16(src1, zero);
        __m256i src2_lo = _mm256_unpacklo_epi16(src2, zero);
        __m256i result_lo = _mm256_abs_epi32(_mm256_sub_epi32(src1_lo, src2_lo));
        sum = _mm256_add_epi32(sum, result_lo);
        // high 8 word to 8 int32
        __m256i src1_hi = _mm256_unpackhi_epi16(src1, zero);
        __m256i src2_hi = _mm256_unpackhi_epi16(src2, zero);
        __m256i result_hi = _mm256_abs_epi32(_mm256_sub_epi32(src1_hi, src2_hi));
        sum = _mm256_add_epi32(sum, result_hi);
#else
        __m256i greater_t = _mm256_subs_epu16(src1, src2); // unsigned sub with saturation
        __m256i smaller_t = _mm256_subs_epu16(src2, src1);
        __m256i absdiff = _mm256_or_si256(greater_t, smaller_t); //abs(s1-s2)  == (satsub(s1,s2) | satsub(s2,s1))
                                                                 // 16 x uint16 absolute differences
        sum = _mm256_add_epi32(sum, _mm256_unpacklo_epi16(absdiff, zero));
        sum = _mm256_add_epi32(sum, _mm256_unpackhi_epi16(absdiff, zero));
        // sum1_32, sum2_32, sum3_32, sum4_32 .. sum8_32
#endif
      }
    }
    if(two_rows) {
      y++;
      pSrc += nSrcPitch*2;
      pRef += nRefPitch*2;
    }
    else {
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
  __m256i a03;
  __m256i a47;
  if(sizeof(pixel_t) == 2) {
    // at 16 bits: we have 8 integers for sum: a0 a1 a2 a3 a4 a5 a6 a7
    a03 = _mm256_unpacklo_epi32(sum, zero); // a0 0 a1 0 a2 0 a3 0
    a47 = _mm256_unpackhi_epi32(sum, zero); // a4 0 a5 0 a6 0 a7 0
    a03 = _mm256_add_epi32(a03, a47); // a0+a4,0, a1+a5, 0, a2+a6, 0, a3+a7, 0
                                      // sum here: sum1 0 sum2 0 sum3 0 sum4 0, as in uint8_t case
  }
  if(sizeof(pixel_t) == 1) {
    a03 = sum;
  }

  // add the low 128 bit to the high 128 bit
  __m128i sum128 = _mm_add_epi32(_mm256_castsi256_si128(a03),_mm256_extractf128_si256(a03, 1));
  // a0+a4+a2+a6, 0, a1+a5+a3+a7, 0,

  __m128i sum_hi = _mm_unpackhi_epi64(sum128, _mm_setzero_si128()); // a1+a5+a3+a7
  sum128 = _mm_add_epi32(sum128, sum_hi); // a0+a4+a2+a6 + a1+a5+a3+a7

  int result = _mm_cvtsi128_si32(sum128);

  _mm256_zeroupper();
  /* Use VZEROUPPER to avoid the penalty of switching from AVX to SSE */

  return result;
}

