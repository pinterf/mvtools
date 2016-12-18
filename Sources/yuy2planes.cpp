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

YUY2Planes::YUY2Planes(int _nWidth, int _nHeight)
{
  nWidth = _nWidth;
  nHeight = _nHeight;
  srcPitch = (nWidth + 15) & (~15);
  srcPitchUV = (nWidth / 2 + 15) & (~15); //v 1.2.1
  pSrc = (unsigned char*)_aligned_malloc(srcPitch*nHeight, 128);   //v 1.2.1
  pSrcU = (unsigned char*)_aligned_malloc(srcPitchUV*nHeight, 128);
  pSrcV = (unsigned char*)_aligned_malloc(srcPitchUV*nHeight, 128);
}

YUY2Planes::~YUY2Planes()
{
  _aligned_free(pSrc);
  _aligned_free(pSrcU);
  _aligned_free(pSrcV);
}

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
      dstp_y[x * 2] = srcp[x * 4 + 0];
      dstp_y[x * 2 + 1] = srcp[x * 4 + 2];
      dstp_u[x] = srcp[x * 4 + 1];
      dstp_v[x] = srcp[x * 4 + 3];
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

  for (size_t y = 0; y < height; y++) {
    for (size_t x = 0; x < width; x += 8) {

      __m128i yy = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp_y + x * 2));
      __m128i u = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(srcp_u + x));
      __m128i v = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(srcp_v + x));

      __m128i uv = _mm_unpacklo_epi8(u, v);
      __m128i yuv_lo = _mm_unpacklo_epi8(yy, uv);
      __m128i yuv_hi = _mm_unpackhi_epi8(yy, uv);

      _mm_stream_si128(reinterpret_cast<__m128i*>(dstp + x * 4), yuv_lo);
      _mm_stream_si128(reinterpret_cast<__m128i*>(dstp + x * 4 + 16), yuv_hi);
    }

    srcp_y += src_pitch_y;
    srcp_u += src_pitch_uv;
    srcp_v += src_pitch_uv;
    dstp += dst_pitch;
  }
}

void convert_yv16_to_yuy2_c(const BYTE *srcp_y, const BYTE *srcp_u, const BYTE *srcp_v, BYTE *dstp, size_t src_pitch_y, size_t src_pitch_uv, size_t dst_pitch, size_t width, size_t height) {
  for (size_t y = 0; y < height; y++) {
    for (size_t x = 0; x < width / 2; x++) {
      dstp[x * 4 + 0] = srcp_y[x * 2];
      dstp[x * 4 + 1] = srcp_u[x];
      dstp[x * 4 + 2] = srcp_y[x * 2 + 1];
      dstp[x * 4 + 3] = srcp_v[x];
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
  const unsigned char * dstU, const unsigned char * dstV, int dstPitchUV, bool sse2)
{

  if (sse2 && IsPtrAligned(pSrcYUY2, 16)) {
    convert_yuy2_to_yv16_sse2(pSrcYUY2, (unsigned char *)dstY, (unsigned char *)dstU, (unsigned char *)dstV, nSrcPitchYUY2, dstPitchY, dstPitchUV, nWidth, nHeight);
  }
  else
  {
    convert_yuy2_to_yv16_c(pSrcYUY2, (unsigned char *)dstY, (unsigned char *)dstU, (unsigned char *)dstV, nSrcPitchYUY2, dstPitchY, dstPitchUV, nWidth, nHeight);
  }
}

// YV16 -> YUY2
void YUY2FromPlanes(unsigned char *pDstYUY2, int nDstPitchYUY2, int nWidth, int nHeight,
  unsigned char * srcY, int srcPitch,
  unsigned char * srcU, unsigned char * srcV, int srcPitchUV, bool sse2)
{
  if (sse2 && IsPtrAligned(srcY, 16)) {
    //U and V don't have to be aligned since we user movq to read from those
    convert_yv16_to_yuy2_sse2(srcY, srcU, srcV, pDstYUY2, srcPitch, srcPitchUV, nDstPitchYUY2, nWidth, nHeight);
  }
  else
  {
    convert_yv16_to_yuy2_c(srcY, srcU, srcV, pDstYUY2, srcPitch, srcPitchUV, nDstPitchYUY2, nWidth, nHeight);
  }
}

