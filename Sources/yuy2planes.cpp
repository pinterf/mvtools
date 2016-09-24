// YUY2 <-> planar
// Copyright(c)2005 A.G.Balakhnin aka Fizick

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; version 2 of the License.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA, or visit
// http://www.gnu.org/copyleft/gpl.html .

#include "yuy2planes.h"

#include <algorithm> // min
#include "commonfunctions.h"
#include "types.h"

#if !defined(_M_X64)
#define rax	eax
#define rbx	ebx
#define rcx	ecx
#define rdx	edx
#define rsi	esi
#define rdi	edi
#define rbp	ebp
#define rsp	esp
#define movzx_int mov
#define movsx_int mov
#else
#define rax	rax
#define rbx	rbx
#define rcx	rcx
#define rdx	rdx
#define rsi	rsi
#define rdi	rdi
#define rbp	rbp
#define rsp	r15
#define movzx_int movzxd
#define movsx_int movsxd
#endif



YUY2Planes::YUY2Planes(int _nWidth, int _nHeight)
{
	nWidth = _nWidth;
	nHeight = _nHeight;
	srcPitch = (nWidth + 15) & (~15);
	srcPitchUV = (nWidth/2 + 15) & (~15); //v 1.2.1
	pSrc = (unsigned char*) _aligned_malloc(srcPitch*nHeight,128);   //v 1.2.1
	pSrcU = (unsigned char*) _aligned_malloc(srcPitchUV*nHeight,128);
	pSrcV = (unsigned char*) _aligned_malloc(srcPitchUV*nHeight,128);

}

YUY2Planes::~YUY2Planes()
{
	_aligned_free (pSrc);
	_aligned_free (pSrcU);
	_aligned_free (pSrcV);
}

//----------------------------------------------------------------------------------------------
// v1.2.1 asm code borrowed from AviSynth 2.6 CVS
// ConvertPlanar (c) 2005 by Klaus Post
#if 0
void YUY2Planes::convYUV422to422(const unsigned char *src, unsigned char *py, unsigned char *pu, unsigned char *pv,
                                        int pitch1, int pitch2y, int pitch2uv, int width, int height)
{
int widthdiv2 = width>>1;
#if !(defined(_M_X64) && defined(_MSC_VER))
// inline asm ignored for MSVC X64 build
// todo: get code from avs+ project
__int64 Ymask = 0x00FF00FF00FF00FFi64;
__asm
{
#if !defined(_M_X64)
#define pitch1_ pitch1
#define pitch2y_ pitch2y
#define pitch2uv_ pitch2uv
#else
#define pitch1_ r8
#define pitch2y_ r9
#define pitch2uv_ r10
		movsx_int pitch1_,pitch1
		movsx_int pitch2y_,pitch2y
		movsx_int pitch2uv_,pitch2uv
#endif
      //push ebx
mov rdi,[src]
mov rbx,[py]
mov rdx,[pu]
mov rsi,[pv]
mov ecx,widthdiv2
movq mm5,Ymask
yloop:
xor eax,eax
align 16
xloop:
movq mm0,[rdi+rax*4]   ; VYUYVYUY - 1
movq mm1,[rdi+rax*4+8] ; VYUYVYUY - 2
movq mm2,mm0           ; VYUYVYUY - 1
movq mm3,mm1           ; VYUYVYUY - 2
pand mm0,mm5           ; 0Y0Y0Y0Y - 1
psrlw mm2,8        ; 0V0U0V0U - 1
pand mm1,mm5           ; 0Y0Y0Y0Y - 2
psrlw mm3,8            ; 0V0U0V0U - 2
packuswb mm0,mm1       ; YYYYYYYY
packuswb mm2,mm3       ; VUVUVUVU
movntq [rbx+rax*2],mm0   ; store y
movq mm4,mm2           ; VUVUVUVU
pand mm2,mm5           ; 0U0U0U0U
psrlw mm4,8            ; 0V0V0V0V
packuswb mm2,mm2       ; xxxxUUUU
packuswb mm4,mm4       ; xxxxVVVV
movd [rdx+rax],mm2     ; store u
add eax,4
cmp eax,ecx
movd [rsi+rax-4],mm4   ; store v
jl xloop
add rdi,pitch1_
add rbx,pitch2y_
add rdx,pitch2uv_
add rsi,pitch2uv_
dec height
jnz yloop
sfence
emms
      //pop ebx
#undef pitch1_
#undef pitch2y_
#undef pitch2uv_
}
#endif
}
#endif

// YV16 -> YUY2
#if 0
void YUY2Planes::conv422toYUV422(const unsigned char *py, const unsigned char *pu, const unsigned char *pv,
                           unsigned char *dst, int pitch1Y, int pitch1UV, int pitch2, int width, int height)
{
#if !(defined(_M_X64) && defined(_MSC_VER))
  // inline asm ignored for MSVC X64 build
  // todo: get code from avs+ project

int widthdiv2 = width >> 1;
__asm
{
#if !defined(_M_X64)
#define pitch1Y_ pitch1Y
#define pitch2_ pitch2
#define pitch1UV_ pitch1UV
#else
#define pitch1Y_ r8
#define pitch2_ r9
#define pitch1UV_ r10
		movsx_int pitch1Y_,pitch1Y
		movsx_int pitch2_,pitch2
		movsx_int pitch1UV_,pitch1UV
#endif
      //push ebx
mov rbx,[py]
mov rdx,[pu]
mov rsi,[pv]
mov rdi,[dst]
mov ecx,widthdiv2
yloop:
xor eax,eax
align 16
xloop:
movd mm1,[rdx+rax]     ;0000UUUU
movd mm2,[rsi+rax]     ;0000VVVV
movq mm0,[rbx+rax*2]   ;YYYYYYYY
punpcklbw mm1,mm2      ;VUVUVUVU
movq mm3,mm0           ;YYYYYYYY
punpcklbw mm0,mm1      ;VYUYVYUY
add eax,4
punpckhbw mm3,mm1      ;VYUYVYUY
movntq [rdi+rax*4-16],mm0 ;store ; bug fixed in v1.2.2, replaced by faster movntq in 2.5.1
cmp eax,ecx
movntq [rdi+rax*4-8],mm3   ;store  ; bug fixed in v1.2.2
jl xloop
add rbx,pitch1Y_
add rdx,pitch1UV_
add rsi,pitch1UV_
add rdi,pitch2_
dec height
jnz yloop
sfence
emms
      //pop ebx
#undef pitch1Y_
#undef pitch2Y_
#undef pitch2UV_
}
#endif
}
#endif
//----------------------------------------------------------------------------------------------

// borrowed from Avisynth+ project
static void convert_yuy2_to_yv16_sse2(const BYTE *srcp, BYTE *dstp_y, BYTE *dstp_u, BYTE *dstp_v, size_t src_pitch, size_t dst_pitch_y, size_t dst_pitch_uv, size_t width, size_t height)
{
  width /= 2;

  for (size_t y = 0; y < height; ++y) {
    for (size_t x = 0; x < width; x += 8) {
      __m128i p0 = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp + x * 4));      // V3 Y7 U3 Y6 V2 Y5 U2 Y4 V1 Y3 U1 Y2 V0 Y1 U0 Y0
      __m128i p1 = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp + x * 4 + 16)); // V7 Yf U7 Ye V6 Yd U6 Yc V5 Yb U5 Ya V4 Y9 U4 Y8

      __m128i p2 = _mm_unpacklo_epi8(p0, p1); // V5 V1 Yb Y3 U5 U1 Ya Y2 V4 V0 Y9 Y1 U4 U0 Y8 Y0
      __m128i p3 = _mm_unpackhi_epi8(p0, p1); // V7 V3 Yf Y7 U7 U3 Ye Y6 V6 V2 Yd Y5 U6 U2 Yc Y4

      p0 = _mm_unpacklo_epi8(p2, p3); // V6 V4 V2 V0 Yd Y9 Y5 Y1 U6 U4 U2 U0 Yc Y8 Y4 Y0
      p1 = _mm_unpackhi_epi8(p2, p3); // V7 V5 V3 V1 Yf Yb Y7 Y3 U7 U5 U3 U1 Ye Ya Y6 Y2

      p2 = _mm_unpacklo_epi8(p0, p1); // U7 U6 U5 U4 U3 U2 U1 U0 Ye Yc Ya Y8 Y6 Y4 Y2 Y0
      p3 = _mm_unpackhi_epi8(p0, p1); // V7 V6 V5 V4 V3 V2 V1 V0 Yf Yd Yb Y9 Y7 Y5 Y3 Y1

      _mm_storel_epi64(reinterpret_cast<__m128i*>(dstp_u + x), _mm_srli_si128(p2, 8));
      _mm_storel_epi64(reinterpret_cast<__m128i*>(dstp_v + x), _mm_srli_si128(p3, 8));
      _mm_store_si128(reinterpret_cast<__m128i*>(dstp_y + x * 2), _mm_unpacklo_epi8(p2, p3));
    }

    srcp += src_pitch;
    dstp_y += dst_pitch_y;
    dstp_u += dst_pitch_uv;
    dstp_v += dst_pitch_uv;
  }
}

// borrowed from Avisynth+ project
static void convert_yuy2_to_yv16_c(const BYTE *srcp, BYTE *dstp_y, BYTE *dstp_u, BYTE *dstp_v, size_t src_pitch, size_t dst_pitch_y, size_t dst_pitch_uv, size_t width, size_t height)
{
  width /= 2;

  for (size_t y = 0; y < height; ++y) { 
    for (size_t x = 0; x < width; ++x) {
      dstp_y[x * 2]     = srcp[x * 4 + 0];
      dstp_y[x * 2 + 1] = srcp[x * 4 + 2];
      dstp_u[x]         = srcp[x * 4 + 1];
      dstp_v[x]         = srcp[x * 4 + 3];
    }
    srcp += src_pitch;
    dstp_y += dst_pitch_y;
    dstp_u += dst_pitch_uv;
    dstp_v += dst_pitch_uv;
  }
}

void convert_yv16_to_yuy2_sse2(const BYTE *srcp_y, const BYTE *srcp_u, const BYTE *srcp_v, BYTE *dstp, size_t src_pitch_y, size_t src_pitch_uv, size_t dst_pitch, size_t width, size_t height)
{
  width /= 2;

  for (size_t y=0; y<height; y++) { 
    for (size_t x=0; x<width; x+=8) {

      __m128i yy = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp_y + x*2));
      __m128i u = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(srcp_u + x));
      __m128i v = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(srcp_v + x));

      __m128i uv = _mm_unpacklo_epi8(u, v);
      __m128i yuv_lo = _mm_unpacklo_epi8(yy, uv);
      __m128i yuv_hi = _mm_unpackhi_epi8(yy, uv);

      _mm_stream_si128(reinterpret_cast<__m128i*>(dstp + x*4), yuv_lo);
      _mm_stream_si128(reinterpret_cast<__m128i*>(dstp + x*4 + 16), yuv_hi);
    }

    srcp_y += src_pitch_y;
    srcp_u += src_pitch_uv;
    srcp_v += src_pitch_uv;
    dstp += dst_pitch;
  }
}

void convert_yv16_to_yuy2_c(const BYTE *srcp_y, const BYTE *srcp_u, const BYTE *srcp_v, BYTE *dstp, size_t src_pitch_y, size_t src_pitch_uv, size_t dst_pitch, size_t width, size_t height) {
  for (size_t y=0; y < height; y++) {
    for (size_t x=0; x < width / 2; x++) {
      dstp[x*4+0] = srcp_y[x*2];
      dstp[x*4+1] = srcp_u[x];
      dstp[x*4+2] = srcp_y[x*2+1];
      dstp[x*4+3] = srcp_v[x];
    }
    srcp_y += src_pitch_y;
    srcp_u += src_pitch_uv;
    srcp_v += src_pitch_uv;
    dstp += dst_pitch;
  }
}

// YUY2 -> YV16
void YUY2ToPlanes(const unsigned char *pSrcYUY2, int nSrcPitchYUY2, int nWidth, int nHeight,
							  const unsigned char * dstY, int dstPitchY,
							  const unsigned char * dstU, const unsigned char * dstV, int dstPitchUV, bool mmx)
{
#if 1
  /*
	unsigned char * pSrc1 = (unsigned char *) dstY;
	unsigned char * pSrcU1 = (unsigned char *) dstU;
	unsigned char * pSrcV1 = (unsigned char *) dstV;
*/
  nWidth <<= 2; // real target width
  // mmx means SSE2 here. Todo: kill mmx everywhere
  if (mmx && IsPtrAligned(pSrcYUY2, 16)) {
    convert_yuy2_to_yv16_sse2(pSrcYUY2, (unsigned char *) dstY, (unsigned char *) dstU, (unsigned char *) dstV, nSrcPitchYUY2, dstPitchY, dstPitchUV, nWidth, nHeight);
} 
  else
    {
      convert_yuy2_to_yv16_c(pSrcYUY2, (unsigned char *) dstY, (unsigned char *) dstU, (unsigned char *) dstV, nSrcPitchYUY2, dstPitchY, dstPitchUV, nWidth, nHeight);
    }
#else
  // old YUY2->YV16
	unsigned char * pSrc1 = (unsigned char *) dstY;
	unsigned char * pSrcU1 = (unsigned char *) dstU;
	unsigned char * pSrcV1 = (unsigned char *) dstV;

	const int awidth = std::min(nSrcPitchYUY2>>1, (nWidth+7) & -8);
#if !(defined(_M_X64) && defined(_MSC_VER))
  // disable non working x64 msvc asm branch
  mmx = false;
#endif
//mmx=false;
	if (!(awidth&7) && mmx) {  // Use MMX

		YUY2Planes::convYUV422to422(pSrcYUY2, pSrc1, pSrcU1, pSrcV1, nSrcPitchYUY2, dstPitchY, dstPitchUV,  awidth, nHeight);
	}
	else
	{

	for (int h=0; h<nHeight; h++)
	{
		for (int w=0; w<nWidth; w+=2)
		{
			int w2 = w+w;
			pSrc1[w] = pSrcYUY2[w2];
			pSrcU1[w>>1] = pSrcYUY2[w2+1];
			pSrc1[w+1] = pSrcYUY2[w2+2];
			pSrcV1[w>>1] = pSrcYUY2[w2+3];
		}
		pSrc1 += dstPitchY;
		pSrcU1 += dstPitchUV;
		pSrcV1 += dstPitchUV;
		pSrcYUY2 += nSrcPitchYUY2;
	}
	}
#endif
}

// YV16 -> YUY2
void YUY2FromPlanes(unsigned char *pDstYUY2, int nDstPitchYUY2, int nWidth, int nHeight,
							  unsigned char * srcY, int srcPitch,
							  unsigned char * srcU, unsigned char * srcV, int srcPitchUV, bool mmx)
{
#if 1
  // mmx means SSE2 here. Todo: kill mmx everywhere
  if (mmx && IsPtrAligned(srcY, 16)) {
    //U and V don't have to be aligned since we user movq to read from those
    convert_yv16_to_yuy2_sse2(srcY, srcU, srcV, pDstYUY2, srcPitch, srcPitchUV, nDstPitchYUY2, nWidth, nHeight);
  } else
    {
      convert_yv16_to_yuy2_c(srcY, srcU, srcV, pDstYUY2, srcPitch, srcPitchUV, nDstPitchYUY2, nWidth, nHeight);
    }

#else
	const int awidth = std::min(srcPitch, (nWidth+7) & -8);
#if !(defined(_M_X64) && defined(_MSC_VER))
  // disable non working x64 msvc asm branch
  mmx = false;
#endif
  if (!(awidth&7) && mmx) {  // Use MMX
    YUY2Planes::conv422toYUV422(srcY, srcU, srcV, pDstYUY2, srcPitch, srcPitchUV, nDstPitchYUY2, awidth, nHeight);
  }
  else
  {

  for (int h=0; h<nHeight; h++)
	{
		for (int w=0; w<nWidth; w+=2)
		{
			int w2 = w+w;
			pDstYUY2[w2] = srcY[w];
			pDstYUY2[w2+1] = srcU[w>>1];
			pDstYUY2[w2+2] = srcY[w+1];
			pDstYUY2[w2+3] = srcV[w>>1];
		}
		srcY += srcPitch;
		srcU += srcPitchUV;
		srcV += srcPitchUV;
		pDstYUY2 += nDstPitchYUY2;
	}
  }
#endif
}

