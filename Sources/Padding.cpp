// Author: Manao
// Copyright(c)2006 A.G.Balakhnin aka Fizick - YUY2

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
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

#include "Padding.h"
#include <algorithm>
#include <stdint.h>


template<typename pixel_t>
void PadCorner(pixel_t *p, pixel_t v, int hPad, int vPad, int refPitch)
{
  // refPitch here is pixel_t aware
  for (int i = 0; i < vPad; i++)
  {
    if constexpr(sizeof(pixel_t) == 1)
      memset(p, v, hPad); // faster than loop
    else {
      std::fill_n(p, hPad, v);
    }
    p += refPitch;
  }
}

Padding::Padding(PClip _child, int hPad, int vPad, bool _planar, IScriptEnvironment* env) :
  GenericVideoFilter(_child)
{
  has_at_least_v8 = true;
  try { env->CheckVersion(8); }
  catch (const AvisynthError&) { has_at_least_v8 = false; }

  if (!vi.IsYUV() && !vi.IsYUY2() && !vi.IsPlanarRGB() && !vi.IsPlanarRGBA())
    env->ThrowError("Padding : clip must be in the YUV or YUY2 or planar RGB/RGBA color format");

  horizontalPadding = hPad;
  verticalPadding = vPad;
  cpuFlags = env->GetCPUFlags();
  width = vi.width;
  height = vi.height;
  planar = _planar;

  if ((vi.pixel_type & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2 && !planar)
  {
    SrcPlanes = new YUY2Planes(vi.width, vi.height);
  }
  vi.width += horizontalPadding * 2;
  vi.height += verticalPadding * 2;
  if ((vi.pixel_type & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2 && !planar)
  {
    DstPlanes = new YUY2Planes(vi.width, vi.height);
  }
}

Padding::~Padding()
{
  if ((vi.pixel_type & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2 && !planar)
  {
    delete SrcPlanes;
    delete DstPlanes;
  }
}

PVideoFrame __stdcall Padding::GetFrame(int n, IScriptEnvironment *env)
{
  PVideoFrame src = child->GetFrame(n, env);
  PVideoFrame dst = has_at_least_v8 ? env->NewVideoFrameP(vi, &src) : env->NewVideoFrame(vi); // frame property support

  const unsigned char *pSrc[3];
  unsigned char *pDst[3];
  int nDstPitches[3];
  int nSrcPitches[3];
  unsigned char *pDstYUY2;
  int nDstPitchYUY2;

  int xRatioUVs[3] = { 1, 1, 1 };
  int yRatioUVs[3] = { 1, 1, 1 };
  int planecount;

  if (!vi.IsY() && !vi.IsRGB()) {
    xRatioUVs[1] = xRatioUVs[2] = vi.IsYUY2() ? 2 : (1 << vi.GetPlaneWidthSubsampling(PLANAR_U));
    yRatioUVs[1] = yRatioUVs[2] = vi.IsYUY2() ? 1 : (1 << vi.GetPlaneHeightSubsampling(PLANAR_U));
  }
  int pixelsize = vi.ComponentSize();

  if ((vi.pixel_type & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2)
  {
    if (!planar)
    {
      const unsigned char *pSrcYUY2 = src->GetReadPtr();
      int nSrcPitchYUY2 = src->GetPitch();
      pSrc[0] = SrcPlanes->GetPtr();
      pSrc[1] = SrcPlanes->GetPtrU();
      pSrc[2] = SrcPlanes->GetPtrV();
      nSrcPitches[0] = SrcPlanes->GetPitch();
      nSrcPitches[1] = SrcPlanes->GetPitchUV();
      nSrcPitches[2] = SrcPlanes->GetPitchUV();
      YUY2ToPlanes(pSrcYUY2, nSrcPitchYUY2, width, height,
        pSrc[0], nSrcPitches[0], pSrc[1], pSrc[2], nSrcPitches[1], cpuFlags);
      pDst[0] = DstPlanes->GetPtr();
      pDst[1] = DstPlanes->GetPtrU();
      pDst[2] = DstPlanes->GetPtrV();
      nDstPitches[0] = DstPlanes->GetPitch();
      nDstPitches[1] = DstPlanes->GetPitchUV();
      nDstPitches[2] = DstPlanes->GetPitchUV();
    }
    else // planar YUY2
    {
      pSrc[0] = src->GetReadPtr();
      nSrcPitches[0] = src->GetPitch();
      nSrcPitches[1] = src->GetPitch();
      nSrcPitches[2] = src->GetPitch();
      pSrc[1] = pSrc[0] + src->GetRowSize() / 2;
      pSrc[2] = pSrc[1] + src->GetRowSize() / 4;
      pDst[0] = dst->GetWritePtr();
      nDstPitches[0] = dst->GetPitch();
      nDstPitches[1] = dst->GetPitch();
      nDstPitches[2] = dst->GetPitch();
      pDst[1] = pDst[0] + dst->GetRowSize() / 2;
      pDst[2] = pDst[1] + dst->GetRowSize() / 4;
    }
    planecount = 3;
  }
  else
  {
    int planes_y[4] = { PLANAR_Y, PLANAR_U, PLANAR_V, PLANAR_A };
    int planes_r[4] = { PLANAR_G, PLANAR_B, PLANAR_R, PLANAR_A };
    int *planes = (vi.IsYUV() || vi.IsYUVA()) ? planes_y : planes_r;
    planecount = std::min(vi.NumComponents(), 3);
    for (int p = 0; p < planecount; ++p) {
      const int plane = planes[p];
      pDst[p] = dst->GetWritePtr(plane);
      nDstPitches[p] = dst->GetPitch(plane);
      pSrc[p] = src->GetReadPtr(plane);
      nSrcPitches[p] = src->GetPitch(plane);
    }
  }


  for (int p = 0; p < planecount; ++p) {
    env->BitBlt(pDst[p] + horizontalPadding / xRatioUVs[p] * pixelsize + verticalPadding / yRatioUVs[p] * nDstPitches[p],
      nDstPitches[p], pSrc[p], nSrcPitches[p], width / xRatioUVs[p] * pixelsize, height / yRatioUVs[p]);
    if (pixelsize == 1)
      PadReferenceFrame<uint8_t>(pDst[p], nDstPitches[p], horizontalPadding / xRatioUVs[p], verticalPadding / yRatioUVs[p], width / xRatioUVs[p], height / yRatioUVs[p]);
    else if (pixelsize == 2)
      PadReferenceFrame<uint16_t>(pDst[p], nDstPitches[p], horizontalPadding / xRatioUVs[p], verticalPadding / yRatioUVs[p], width / xRatioUVs[p], height / yRatioUVs[p]);
    else
      PadReferenceFrame<float>(pDst[p], nDstPitches[p], horizontalPadding / xRatioUVs[p], verticalPadding / yRatioUVs[p], width / xRatioUVs[p], height / yRatioUVs[p]);
  }

  if ((vi.pixel_type & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2 && !planar)
  {
    pDstYUY2 = dst->GetWritePtr();
    nDstPitchYUY2 = dst->GetPitch();
    YUY2FromPlanes(pDstYUY2, nDstPitchYUY2, vi.width, vi.height,
      pDst[0], nDstPitches[0], pDst[1], pDst[2], nDstPitches[1], cpuFlags);
  }
  return dst;
}

template<typename pixel_t>
void Padding::PadReferenceFrame(unsigned char *refFrame8, int refPitch, int hPad, int vPad, int width, int height)
{
  pixel_t *refFrame = reinterpret_cast<pixel_t *>(refFrame8);
  refPitch /= sizeof(pixel_t);

  pixel_t *pfoff = refFrame + vPad * refPitch + hPad;

  // Up-Left
  PadCorner<pixel_t>(refFrame, pfoff[0], hPad, vPad, refPitch); // PF refPitch is already pixelsize aware!
  // Up-Right
  PadCorner<pixel_t>(refFrame + hPad + width, pfoff[width - 1], hPad, vPad, refPitch);
  // Down-Left
  PadCorner<pixel_t>(refFrame + (vPad + height) * refPitch,
    pfoff[(height - 1) * refPitch], hPad, vPad, refPitch);
  // Down-Right
  PadCorner<pixel_t>(refFrame + hPad + width + (vPad + height) * refPitch,
    pfoff[(height - 1) * refPitch + width - 1], hPad, vPad, refPitch);

  // Top and bottom
  // LDS: would multiple memcpy be faster? The inner loop is very short
  // and the offset calculations are not trivial.
  for (int i = 0; i < width; i++)
  {
    pixel_t value_t = pfoff[i];
    pixel_t value_b = pfoff[i + (height - 1) * refPitch];
    pixel_t *p_t = refFrame + hPad + i;
    pixel_t *p_b = p_t + (height + vPad) * refPitch;
    for (int j = 0; j < vPad; j++)
    {
      p_t[0] = value_t;
      p_b[0] = value_b;
      p_t += refPitch;
      p_b += refPitch;
    }
  }

  // Left and right
  for (int i = 0; i < height; i++)
  {
    pixel_t value_l = pfoff[i * refPitch];
    pixel_t value_r = pfoff[i * refPitch + width - 1];
    pixel_t *p_l = refFrame + (vPad + i) * refPitch;
    pixel_t *p_r = p_l + width + hPad;
    for (int j = 0; j < hPad; j++)
    {
      p_l[j] = value_l;
      p_r[j] = value_r;
    }
  }
}

template void Padding::PadReferenceFrame<uint8_t>(unsigned char* refFrame8, int refPitch, int hPad, int vPad, int width, int height);
template void Padding::PadReferenceFrame<uint16_t>(unsigned char* refFrame8, int refPitch, int hPad, int vPad, int width, int height);
template void Padding::PadReferenceFrame<float>(unsigned char* refFrame8, int refPitch, int hPad, int vPad, int width, int height);

