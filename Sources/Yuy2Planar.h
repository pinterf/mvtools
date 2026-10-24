// YUY2 sources: converted at the filter boundary, the filters work on YV16

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

#ifndef __YUY2PLANAR_H__
#define __YUY2PLANAR_H__

#include "avisynth.h"
#include <cstring>
#include <stdint.h>

// planar=false: interleaved YUY2 <-> YV16 by the ConvertToYV16 and ConvertToYUY2 of the host (lossless repacking).
// planar=true: the "planar YUY2" of the AviSynth 2.5 era (Interleaved2Planar of RemoveGrain): a YUY2 frame
// holding the planes side by side in each row: width bytes Y, width / 2 bytes U, width / 2 bytes V.
class Yuy2SideBySide : public GenericVideoFilter
{
  const bool to_yv16; // true: side by side YUY2 to YV16, false: YV16 to side by side YUY2
  bool has_at_least_v8;

public:
  Yuy2SideBySide(PClip _child, bool _to_yv16, IScriptEnvironment* env) : GenericVideoFilter(_child), to_yv16(_to_yv16)
  {
    has_at_least_v8 = true;
    try { env->CheckVersion(8); }
    catch (const AvisynthError&) { has_at_least_v8 = false; }
    vi.pixel_type = to_yv16 ? VideoInfo::CS_YV16 : VideoInfo::CS_YUY2;
  }

  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env) override
  {
    PVideoFrame src = child->GetFrame(n, env);
    PVideoFrame dst = has_at_least_v8 ? env->NewVideoFrameP(vi, &src) : env->NewVideoFrame(vi); // frame property support
    static const int planes[3] = { PLANAR_Y, PLANAR_U, PLANAR_V };
    int offset = 0; // of the plane in the side by side row
    for (int p = 0; p < 3; p++) {
      const int rowsize = p == 0 ? vi.width : vi.width / 2;
      if (to_yv16)
        env->BitBlt(dst->GetWritePtr(planes[p]), dst->GetPitch(planes[p]), src->GetReadPtr() + offset, src->GetPitch(), rowsize, vi.height);
      else
        env->BitBlt(dst->GetWritePtr() + offset, dst->GetPitch(), src->GetReadPtr(planes[p]), src->GetPitch(planes[p]), rowsize, vi.height);
      offset += rowsize;
    }
    return dst;
  }

  int __stdcall SetCacheHints(int cachehints, int frame_range) override {
    return cachehints == CACHE_GET_MTMODE ? MT_NICE_FILTER : 0;
  }
};

// a YUY2 clip to YV16; other formats (and no clip) are returned as they are
static inline PClip yuy2_to_yv16(const PClip& clip, bool planar, IScriptEnvironment* env)
{
  if (!clip || !clip->GetVideoInfo().IsYUY2())
    return clip;
  if (planar)
    return new Yuy2SideBySide(clip, true, env);
  return env->Invoke("ConvertToYV16", AVSValue(clip)).AsClip();
}

// back to YUY2, as yuy2_to_yv16 got it
static inline PClip yv16_to_yuy2(const PClip& clip, bool planar, IScriptEnvironment* env)
{
  if (planar)
    return new Yuy2SideBySide(clip, false, env);
  return env->Invoke("ConvertToYUY2", AVSValue(clip)).AsClip();
}

#endif
