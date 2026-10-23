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

// drop-in replacements of the bilinear / bicubic refine filters and Average2
template<typename pixel_t>
void VerticalBilin_neon(unsigned char *pDst8, const unsigned char *pSrc8, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template<typename pixel_t>
void HorizontalBilin_neon(unsigned char *pDst8, const unsigned char *pSrc8, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template<typename pixel_t>
void DiagonalBilin_neon(unsigned char *pDst8, const unsigned char *pSrc8, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template<typename pixel_t>
void HorizontalBicubic_neon(unsigned char *pDst8, const unsigned char *pSrc8, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template<typename pixel_t>
void VerticalBicubic_neon(unsigned char *pDst8, const unsigned char *pSrc8, int nDstPitch, int nSrcPitch, int nWidth, int nHeight, int bits_per_pixel);
template<typename pixel_t>
void Average2_neon(unsigned char *pDst8, const unsigned char *pSrc1_8, const unsigned char *pSrc2_8, int nPitch, int nWidth, int nHeight);

#endif
