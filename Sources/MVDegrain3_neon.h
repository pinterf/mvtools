#ifndef __MV_DEGRAIN3_NEON__
#define __MV_DEGRAIN3_NEON__

#include <stdint.h>

// aarch64 NEON version of Degrain1to6_C (integer formats): same parameters, same results.
// out16_type: 0: native 8 or 16 bit, 2: 8 bit in, native 16 bit out
// blockWidth: template, the height comes from WidthHeightForC
template<int blockWidth, typename pixel_t, int out16_type, int level>
void Degrain1to6_neon(uint8_t *pDst, int WidthHeightForC, int nDstPitch, const uint8_t *pSrc, int nSrcPitch,
  const uint8_t *pRefB[], int BPitch[], const uint8_t *pRefF[], int FPitch[],
  int WSrc,
  int WRefB[], int WRefF[]);

#endif
