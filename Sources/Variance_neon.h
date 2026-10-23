#ifndef __VARIANCE_NEON__
#define __VARIANCE_NEON__

// aarch64 NEON block luma sum, same result as Luma_C; pixel_t: uint8_t or uint16_t
template<int nBlkWidth, int nBlkHeight, typename pixel_t>
unsigned int Luma_neon(const unsigned char *pSrc, int nSrcPitch);

#endif
