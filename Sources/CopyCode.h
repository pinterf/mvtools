#ifndef __COPYCODE_H__
#define __COPYCODE_H__



#include "types.h"

#include <cstring>
#include <stdint.h>

template <typename pixel_t>
void fill_chroma(BYTE* dstp_u, BYTE* dstp_v, int height, int pitch, pixel_t val);

template <typename pixel_t>
void fill_plane(BYTE* dstp, int height, int pitch, pixel_t val);

void BitBlt(unsigned char* dstp, int dst_pitch, const unsigned char* srcp, int src_pitch, int row_size, int height);
//void asm_BitBlt_ISSE(unsigned char* dstp, int dst_pitch, const unsigned char* srcp, int src_pitch, int row_size, int height);
//extern "C" void memcpy_amd(void *dest, const void *src, size_t n);
extern "C" void MemZoneSet(unsigned char *ptr, unsigned char value, int width,
				int height, int offsetX, int offsetY, int pitch);
extern "C" void MemZoneSet16(uint16_t *ptr, uint16_t value, int width,
  int height, int offsetX, int offsetY, int pitch);

typedef void (COPYFunction)(uint8_t *pDst, int nDstPitch,
                            const uint8_t *pSrc, int nSrcPitch);
COPYFunction* get_copy_function(int BlockX, int BlockY, int pixelsize, arch_t arch);

template<int nBlkWidth, int nBlkHeight, typename pixel_t>
void Copy_C(uint8_t *pDst, int nDstPitch, const uint8_t *pSrc, int nSrcPitch);

template<int nBlkSize, typename pixel_t>
void Copy_C(uint8_t *pDst, int nDstPitch, const uint8_t *pSrc, int nSrcPitch);



#ifdef USE_COPYCODE_ASM

#define MK_CFUNC(functionname) extern "C" void __cdecl functionname (uint8_t *pDst, int nDstPitch, const uint8_t *pSrc, int nSrcPitch)
//default functions
MK_CFUNC(Copy32x32_sse2);
MK_CFUNC(Copy16x32_sse2);
MK_CFUNC(Copy32x16_sse2);
MK_CFUNC(Copy32x8_sse2);
MK_CFUNC(Copy16x16_sse2);
MK_CFUNC(Copy16x8_sse2);
MK_CFUNC(Copy16x4_sse2);
MK_CFUNC(Copy16x2_sse2);
MK_CFUNC(Copy16x1_sse2);
MK_CFUNC(Copy8x16_sse2);
MK_CFUNC(Copy8x8_sse2);
MK_CFUNC(Copy8x4_sse2);
MK_CFUNC(Copy8x2_sse2);
MK_CFUNC(Copy8x1_sse2);
MK_CFUNC(Copy4x8_sse2);
MK_CFUNC(Copy4x4_sse2);
MK_CFUNC(Copy4x2_sse2);
MK_CFUNC(Copy2x4_sse2);
MK_CFUNC(Copy2x2_sse2);
#endif
#undef MK_CFUNC

#endif
