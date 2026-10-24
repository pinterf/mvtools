#ifndef __MDEGRAINN_NEON__
#define __MDEGRAINN_NEON__

#include <stdint.h>

// aarch64 NEON version of DegrainN_C (integer formats): same parameters, same results.
// out16_type: 0: native 8 or 16 bit, 2: 8 bit in, native 16 bit out
template <typename pixel_t, int blockWidth, int blockHeight, int out16_type>
void DegrainN_neon(
  uint8_t* pDst, int nDstPitch,
  const uint8_t* pSrc, int nSrcPitch,
  const uint8_t* pRef[], int Pitch[],
  int Wall[], int trad
);

// float: DegrainN_C<float> with the same FMA contraction as GCC on aarch64
template <int blockWidth, int blockHeight>
void DegrainN_float_neon(
  uint8_t* pDst, int nDstPitch,
  const uint8_t* pSrc, int nSrcPitch,
  const uint8_t* pRef[], int Pitch[],
  int Wall[], int trad
);

#endif
