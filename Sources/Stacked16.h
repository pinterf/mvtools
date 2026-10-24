// MDegrain1-6, MDegrainN lsb=true: the out16=true result in the stacked 16 bit format

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

#ifndef __STACKED16_H__
#define __STACKED16_H__

#include "avisynth.h"
#include <stdint.h>

// Native 16 bit Y16, YUV420P16, YUV422P16, YUV444P16 to Y8, YV12, YV16, YV24 of double height: in each plane
// the most significant bytes (MSB) are stacked on the top of the least significant bytes (LSB),
// like ConvertToStacked of AviSynth+. The stacked format was the 16 bit output of the 8 bit only AviSynth.
class Stacked16 : public GenericVideoFilter
{
  bool has_at_least_v8;

public:
  Stacked16(PClip _child, IScriptEnvironment* env) : GenericVideoFilter(_child)
  {
    has_at_least_v8 = true;
    try { env->CheckVersion(8); }
    catch (const AvisynthError&) { has_at_least_v8 = false; }

    if (vi.pixel_type == VideoInfo::CS_Y16)
      vi.pixel_type = VideoInfo::CS_Y8;
    else if (vi.pixel_type == VideoInfo::CS_YUV420P16)
      vi.pixel_type = VideoInfo::CS_YV12;
    else if (vi.pixel_type == VideoInfo::CS_YUV422P16)
      vi.pixel_type = VideoInfo::CS_YV16;
    else if (vi.pixel_type == VideoInfo::CS_YUV444P16)
      vi.pixel_type = VideoInfo::CS_YV24;
    else
      env->ThrowError("Stacked16: Y16, YUV420P16, YUV422P16 or YUV444P16 input is needed");
    vi.height *= 2;
  }

  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env) override
  {
    PVideoFrame src = child->GetFrame(n, env);
    PVideoFrame dst = has_at_least_v8 ? env->NewVideoFrameP(vi, &src) : env->NewVideoFrame(vi); // frame property support
    static const int planes[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
    const int nPlanes = vi.IsY8() ? 1 : 3;
    for (int p = 0; p < nPlanes; p++) {
      const int plane = planes[p];
      const uint16_t* s = reinterpret_cast<const uint16_t*>(src->GetReadPtr(plane));
      const int src_pitch = src->GetPitch(plane) / sizeof(uint16_t);
      const int width = src->GetRowSize(plane) / sizeof(uint16_t);
      const int height = src->GetHeight(plane);
      uint8_t* msb = dst->GetWritePtr(plane);
      const int dst_pitch = dst->GetPitch(plane);
      uint8_t* lsb = msb + (ptrdiff_t)dst_pitch * height;
      for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
          msb[x] = (uint8_t)(s[x] >> 8);
          lsb[x] = (uint8_t)(s[x] & 0xFF);
        }
        s += src_pitch;
        msb += dst_pitch;
        lsb += dst_pitch;
      }
    }
    return dst;
  }

  int __stdcall SetCacheHints(int cachehints, int frame_range) override {
    return cachehints == CACHE_GET_MTMODE ? MT_NICE_FILTER : 0;
  }
};

#endif
