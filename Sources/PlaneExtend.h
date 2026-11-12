// MVTools v2

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

#ifndef __MV_PlaneExtend__
#define __MV_PlaneExtend__

#include "avisynth.h"
#include <cstdint>
#include <cstring>

// Copies a width x height plane into an ext_width x ext_height plane, filling the area on the right / bottom by
// repeating the last column / row (the edge replication of the MSuper padding).
// Used by the filters processing the blocks beyond the frame edge (MOTION_EDGE_BLOCKS, MSuper edgepad): they work
// on a frame of the block grid size, whose source pixels beyond the frame are the repeated edge pixels.
inline void extend_plane(uint8_t* dstp, int dst_pitch, const uint8_t* srcp, int src_pitch,
  int width, int height, int ext_width, int ext_height, int pixelsize)
{
  for (int y = 0; y < height; y++)
  {
    uint8_t* d = dstp + y * dst_pitch;
    memcpy(d, srcp + y * src_pitch, width * pixelsize);
    const uint8_t* last = d + (width - 1) * pixelsize;
    for (int x = width; x < ext_width; x++)
      memcpy(d + x * pixelsize, last, pixelsize);
  }
  const uint8_t* lastrow = dstp + (height - 1) * dst_pitch;
  for (int y = height; y < ext_height; y++)
    memcpy(dstp + y * dst_pitch, lastrow, ext_width * pixelsize);
}

// The planes Y, U, V (or G, B, R) of src extended to the frame size of vi_ext (see extend_plane).
// planecount: 1 for greyscale, else 3; alpha is not copied.
inline PVideoFrame extend_frame(const PVideoFrame& src, const VideoInfo& vi_ext, int width, int height,
  int planecount, int log_xratio, int log_yratio, IScriptEnvironment* env)
{
  PVideoFrame ext = env->NewVideoFrame(vi_ext);
  const int planes[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
  const int pixelsize = vi_ext.ComponentSize();
  for (int p = 0; p < planecount; ++p)
  {
    const int shx = p == 0 ? 0 : log_xratio;
    const int shy = p == 0 ? 0 : log_yratio;
    extend_plane(ext->GetWritePtr(planes[p]), ext->GetPitch(planes[p]), src->GetReadPtr(planes[p]), src->GetPitch(planes[p]),
      width >> shx, height >> shy, vi_ext.width >> shx, vi_ext.height >> shy, pixelsize);
  }
  return ext;
}

// copies the top-left width x height area of the planes of src into dst (back from a frame of the block grid size)
inline void crop_frame_into(PVideoFrame& dst, const PVideoFrame& src, int width, int height,
  int planecount, int log_xratio, int log_yratio, int pixelsize, IScriptEnvironment* env)
{
  const int planes[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
  for (int p = 0; p < planecount; ++p)
  {
    const int shx = p == 0 ? 0 : log_xratio;
    const int shy = p == 0 ? 0 : log_yratio;
    env->BitBlt(dst->GetWritePtr(planes[p]), dst->GetPitch(planes[p]), src->GetReadPtr(planes[p]), src->GetPitch(planes[p]),
      (width >> shx) * pixelsize, height >> shy);
  }
}

#endif	// __MV_PlaneExtend__
