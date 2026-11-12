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

#ifndef __MV_SuperParams__
#define __MV_SuperParams__

#include "avisynth.h"
#include "SuperParams64Bits.h"
#include <cstring>
#include <map>
#include <mutex>
#include <tuple>
#include <vector>

// MSuper parameters, as the filters getting a super clip see them.
//
// The super clip carries them in its VideoInfo (the only data available in a constructor):
// - the legacy 8 bytes (SuperParams64Bits) in vi.num_audio_samples, no room for more fields;
// - the full set: kept in a process-lifetime registry, the super clip carries the registry index in
//   vi.sample_type (marker in the high 16 bits), vi.nchannels is 0.
// The super clip has no audio (audio_samples_per_second == 0), the core ignores the other audio fields.
// An index, unlike a pointer to the MSuper instance, cannot dangle: a filter copying the VideoInfo without
// holding the super clip (e.g. BlankClip(super)) leaves a valid index. Entries are deduplicated, so the
// registry stays small even when MSuper is created per frame (ScriptClip).
// nchannels == 0: a super given as a vector clip fails in MVClip with "invalid vector stream".
// Readers copy the values in their constructor.

struct SuperParams
{
  int nWidth;   // frame width (super clip width without the padding)
  int nHeight;  // frame height
  int nHPad;
  int nVPad;
  int nPel;
  int nModeYUV;
  int nLevels;
  int param;    // SUPER_PARAM_ flags (SuperParams64Bits.h)

  auto as_tuple() const { return std::tie(nWidth, nHeight, nHPad, nVPad, nPel, nModeYUV, nLevels, param); }
  bool operator<(const SuperParams& other) const { return as_tuple() < other.as_tuple(); }
};

constexpr int SUPER_PARAMS_MARKER = 0x53500000; // 'SP' in the high 16 bits of vi.sample_type
constexpr int SUPER_PARAMS_MARKER_MASK = (int)0xFFFF0000;
constexpr int SUPER_PARAMS_MAX_INDEX = 0xFFFF;

class SuperParamsRegistry
{
  std::mutex mtx;
  std::map<SuperParams, int> index_of;
  std::vector<SuperParams> items;

  SuperParamsRegistry() = default;

public:
  static SuperParamsRegistry& instance()
  {
    static SuperParamsRegistry registry;
    return registry;
  }

  // returns -1 when full
  int add(const SuperParams& sp)
  {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = index_of.find(sp);
    if (it != index_of.end())
      return it->second;
    if ((int)items.size() > SUPER_PARAMS_MAX_INDEX)
      return -1;
    const int index = (int)items.size();
    items.push_back(sp);
    index_of.emplace(sp, index);
    return index;
  }

  bool get(int index, SuperParams& sp)
  {
    std::lock_guard<std::mutex> lock(mtx);
    if (index < 0 || index >= (int)items.size())
      return false;
    sp = items[index];
    return true;
  }
};

// MSuper: store the parameters in the super clip's VideoInfo
inline void SetSuperParams(VideoInfo& vi, const SuperParams& sp, IScriptEnvironment* env)
{
  SuperParams64Bits legacy;
  legacy.nHeight = (uint16_t)sp.nHeight;
  legacy.nHPad = (unsigned char)sp.nHPad;
  legacy.nVPad = (unsigned char)sp.nVPad;
  legacy.nPel = (unsigned char)sp.nPel;
  legacy.nModeYUV = (unsigned char)sp.nModeYUV;
  legacy.nLevels = (unsigned char)sp.nLevels;
  legacy.param = (unsigned char)sp.param;
  memcpy(&vi.num_audio_samples, &legacy, 8);

  const int index = SuperParamsRegistry::instance().add(sp);
  if (index < 0)
    env->ThrowError("MSuper: too many different super clip parameter sets");
  vi.sample_type = SUPER_PARAMS_MARKER | index;
  vi.nchannels = 0;
  vi.audio_samples_per_second = 0; // no audio
}

// Filters getting a super clip. Without a registry index (not an MSuper clip of this build) the legacy
// 8 bytes are decoded, the width is derived from the clip width; the callers validate the values.
inline SuperParams GetSuperParams(const VideoInfo& vi)
{
  SuperParams sp;
  if (vi.nchannels == 0 && (vi.sample_type & SUPER_PARAMS_MARKER_MASK) == SUPER_PARAMS_MARKER
    && SuperParamsRegistry::instance().get(vi.sample_type & SUPER_PARAMS_MAX_INDEX, sp))
    return sp;

  SuperParams64Bits legacy;
  memcpy(&legacy, &vi.num_audio_samples, 8);
  sp.nHeight = legacy.nHeight;
  sp.nHPad = legacy.nHPad;
  sp.nVPad = legacy.nVPad;
  sp.nPel = legacy.nPel;
  sp.nModeYUV = legacy.nModeYUV;
  sp.nLevels = legacy.nLevels;
  sp.param = legacy.param;
  sp.nWidth = vi.width - 2 * sp.nHPad;
  return sp;
}

#endif	// __MV_SuperParams__
