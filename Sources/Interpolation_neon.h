#ifndef __INTERPOLATION_NEON__
#define __INTERPOLATION_NEON__

#include <stdint.h>

// aarch64 NEON parts of the MSuper reduce and refine filters (8-16 bit), same results as the C.

// RB2BilinearFilteredVertical, one row: the 1/8 3/8 3/8 1/8 vertical filter for x in [0, n), n is returned (multiple of 8)
template<typename pixel_t>
int RB2BilinearFilteredVerticalLine_neon(pixel_t *pDst, const pixel_t *pSrc, int nSrcPitch, int nWidth);

// RB2BilinearFilteredHorizontalInplace, one row: x in [1, n), n is returned; the caller does the rest from n
template<typename pixel_t>
int RB2BilinearFilteredHorizontalInplaceLine_neon(pixel_t *pSrc, int nWidth);

// drop-in replacements of HorizontalWiener / VerticalWiener
template<typename pixel_t>
void HorizontalWiener_neon(unsigned char *pDst8, const unsigned char *pSrc8, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template<typename pixel_t>
void VerticalWiener_neon(unsigned char *pDst8, const unsigned char *pSrc8, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);

#endif
