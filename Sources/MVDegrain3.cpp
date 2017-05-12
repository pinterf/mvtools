// Make a motion compensate temporal denoiser
// Copyright(c)2006 A.G.Balakhnin aka Fizick
// See legal notice in Copying.txt for more information

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

#include "ClipFnc.h"
#include "Interpolation.h"
#include "MVDegrain3.h"
#include "MVFrame.h"
#include "MVGroupOfFrames.h"
#include "MVPlane.h"
#include "Padding.h"
#include "profile.h"
#include "SuperParams64Bits.h"
#include "CopyCode.h"
#include "overlap.h"
#include <stdint.h>
#include <commonfunctions.h>
#include "def.h"

//#include	<mmintrin.h>

// PF 160926: MDegrain3 -> MDegrainX: common 1..5 level MDegrain functions
// 170105: MDegrain6

//
#ifdef LEVEL_IS_TEMPLATE
template<int level>
Denoise1to6Function* MVDegrainX<level>::get_denoise123_function(int BlockX, int BlockY, int _pixelsize, bool _lsb_flag, int _level, arch_t arch)
#else
Denoise1to6Function* MVDegrainX::get_denoise123_function(int BlockX, int BlockY, int _pixelsize, bool _lsb_flag, int _level, arch_t arch)
#endif
{
  // 8 bit only (pixelsize==1)
  //---------- DENOISE/DEGRAIN
  // BlkSizeX, BlkSizeY, pixelsize, lsb_flag, level_of_MDegrain, arch_t
  std::map<std::tuple<int, int, int, bool, int, arch_t>, Denoise1to6Function*> func_degrain;
  using std::make_tuple;

// level 1-6, 8bit C, 8bit lsb C, 16 bit C, 16 bit SSE41
#define MAKE_FN_LEVEL(x, y, level) \
func_degrain[make_tuple(x, y, 1, false, level, NO_SIMD)] = Degrain1to6_C<uint8_t, x, y, false, level>; \
func_degrain[make_tuple(x, y, 1, true, level, NO_SIMD)] = Degrain1to6_C<uint8_t, x, y, true, level>; \
func_degrain[make_tuple(x, y, 2, false, level, NO_SIMD)] = Degrain1to6_C<uint16_t, x, y, false, level>; \
func_degrain[make_tuple(x, y, 2, false, level, USE_SSE41)] = Degrain1to6_16_sse41<x, y, level>;
#define MAKE_FN(x, y) \
MAKE_FN_LEVEL(x,y,1) \
MAKE_FN_LEVEL(x,y,2) \
MAKE_FN_LEVEL(x,y,3) \
MAKE_FN_LEVEL(x,y,4) \
MAKE_FN_LEVEL(x,y,5) \
MAKE_FN_LEVEL(x,y,6)
  MAKE_FN(64, 64)
    MAKE_FN(64, 48)
    MAKE_FN(64, 32)
    MAKE_FN(64, 16)
    MAKE_FN(48, 64)
    MAKE_FN(48, 48)
    MAKE_FN(48, 24)
    MAKE_FN(48, 12)
    MAKE_FN(32, 64)
    MAKE_FN(32, 32)
    MAKE_FN(32, 24)
    MAKE_FN(32, 16)
    MAKE_FN(32, 8)
    MAKE_FN(24, 48)
    MAKE_FN(24, 32)
    MAKE_FN(24, 24)
    MAKE_FN(24, 12)
    MAKE_FN(24, 6)
    MAKE_FN(16, 64)
    MAKE_FN(16, 32)
    MAKE_FN(16, 16)
    MAKE_FN(16, 12)
    MAKE_FN(16, 8)
    MAKE_FN(16, 4)
    MAKE_FN(16, 2)
    MAKE_FN(16, 1)
    MAKE_FN(12, 48)
    MAKE_FN(12, 24)
    MAKE_FN(12, 16)
    MAKE_FN(12, 12)
    MAKE_FN(12, 6)
    MAKE_FN(8, 32)
    MAKE_FN(8, 16)
    MAKE_FN(8, 8)
    MAKE_FN(8, 4)
    MAKE_FN(8, 2)
    MAKE_FN(8, 1)
    MAKE_FN(6, 12)
    MAKE_FN(6, 6)
    MAKE_FN(6, 3)
    MAKE_FN(4, 8)
    MAKE_FN(4, 4)
    MAKE_FN(4, 2)
    MAKE_FN(4, 1)
    MAKE_FN(3, 6)
    MAKE_FN(3, 3)
    MAKE_FN(2, 4)
    MAKE_FN(2, 2)
    MAKE_FN(2, 1)
#undef MAKE_FN
#undef MAKE_FN_LEVEL

// 8 bit sse2 degrain function (mmx is replaced with sse2 for x86 width 4)
#define MAKE_FN_LEVEL(x, y, level) \
func_degrain[make_tuple(x, y, 1, false, level, USE_SSE2)] = Degrain1to6_sse2<x, y, false, level>; \
func_degrain[make_tuple(x, y, 1, true, level, USE_SSE2)] = Degrain1to6_sse2<x, y, true, level>;
#define MAKE_FN_LEVEL_MMX(x, y, level) \
func_degrain[make_tuple(x, y, 1, false, level, USE_SSE2)] = Degrain1to6_mmx<x, y, false, level>; \
func_degrain[make_tuple(x, y, 1, true, level, USE_SSE2)] = Degrain1to6_mmx<x, y, true, level>;
#define MAKE_FN(x, y) \
MAKE_FN_LEVEL(x,y,1) \
MAKE_FN_LEVEL(x,y,2) \
MAKE_FN_LEVEL(x,y,3) \
MAKE_FN_LEVEL(x,y,4) \
MAKE_FN_LEVEL(x,y,5) \
MAKE_FN_LEVEL(x,y,6)
#define MAKE_FN_MMX(x, y) \
MAKE_FN_LEVEL_MMX(x,y,1) \
MAKE_FN_LEVEL_MMX(x,y,2) \
MAKE_FN_LEVEL_MMX(x,y,3) \
MAKE_FN_LEVEL_MMX(x,y,4) \
MAKE_FN_LEVEL_MMX(x,y,5) \
MAKE_FN_LEVEL_MMX(x,y,6)
    MAKE_FN(64, 64)
    MAKE_FN(64, 48)
    MAKE_FN(64, 32)
    MAKE_FN(64, 16)
    MAKE_FN(48, 64)
    MAKE_FN(48, 48)
    MAKE_FN(48, 24)
    MAKE_FN(48, 12)
    MAKE_FN(32, 64)
    MAKE_FN(32, 32)
    MAKE_FN(32, 24)
    MAKE_FN(32, 16)
    MAKE_FN(32, 8)
    MAKE_FN(24, 48)
    MAKE_FN(24, 32)
    MAKE_FN(24, 24)
    MAKE_FN(24, 12)
    MAKE_FN(24, 6)
    MAKE_FN(16, 64)
    MAKE_FN(16, 32)
    MAKE_FN(16, 16)
    MAKE_FN(16, 12)
    MAKE_FN(16, 8)
    MAKE_FN(16, 4)
    MAKE_FN(16, 2)
    MAKE_FN(16, 1)
    MAKE_FN(12, 48)
    MAKE_FN(12, 24)
    MAKE_FN(12, 16)
    MAKE_FN(12, 12)
    MAKE_FN(12, 6)
    MAKE_FN(8, 32)
    MAKE_FN(8, 16)
    MAKE_FN(8, 8)
    MAKE_FN(8, 4)
    MAKE_FN(8, 2)
    MAKE_FN(8, 1)
    MAKE_FN(6, 12)
    MAKE_FN(6, 6)
    MAKE_FN(6, 3)
    MAKE_FN(4, 8)
    MAKE_FN(4, 4)
    MAKE_FN(4, 2)
#if 0
#ifndef _M_X64
    MAKE_FN_MMX(4, 8)
    MAKE_FN_MMX(4, 4)
    MAKE_FN_MMX(4, 2)
#else
    MAKE_FN(4, 8)
    MAKE_FN(4, 4)
    MAKE_FN(4, 2)
#endif
#endif
    MAKE_FN(4, 1)
    MAKE_FN(3, 6)
    MAKE_FN(3, 3)
    MAKE_FN(2, 4)
    MAKE_FN(2, 2)
    MAKE_FN(2, 1)
#undef MAKE_FN
#undef MAKE_FN_MMX
#undef MAKE_FN_LEVEL
#undef MAKE_FN_LEVEL_MMX

  Denoise1to6Function* result = nullptr;
  arch_t archlist[] = { USE_AVX2, USE_AVX, USE_SSE41, USE_SSE2, NO_SIMD };
  int index = 0;
  while (result == nullptr) {
    arch_t current_arch_try = archlist[index++];
    if (current_arch_try > arch) continue;
    result = func_degrain[make_tuple(BlockX, BlockY, pixelsize, _lsb_flag, _level, current_arch_try)];
    if (result == nullptr && current_arch_try == NO_SIMD)
      break;
  }
  return result;
}


// If mvfw is null, mvbw is assumed to be a radius-3 multi-vector clip.


#ifdef LEVEL_IS_TEMPLATE
template<int level>
MVDegrainX<level>::MVDegrainX(
#else
MVDegrainX::MVDegrainX(
#endif
  PClip _child, PClip _super, 
  PClip _mvbw, PClip _mvfw, PClip _mvbw2, PClip _mvfw2, PClip _mvbw3, PClip _mvfw3, PClip _mvbw4, PClip _mvfw4, PClip _mvbw5, PClip _mvfw5, PClip _mvbw6, PClip _mvfw6,
  sad_t _thSAD, sad_t _thSADC, int _YUVplanes, sad_t _nLimit, sad_t _nLimitC,
  sad_t _nSCD1, int _nSCD2, bool _isse2, bool _planar, bool _lsb_flag,
  bool _mt_flag, 
#ifndef LEVEL_IS_TEMPLATE
  int _level, 
#endif
  IScriptEnvironment* env_ptr
) : GenericVideoFilter(_child)
, MVFilter(
  // mvfw/mvfw2/mvfw3/mvfw4/mvfw5/mvfw6
  // MDegrain1/2/3/4/5/6
#ifndef LEVEL_IS_TEMPLATE
  (!_mvfw) ? _mvbw : (_level == 1 ? _mvfw : (_level == 2 ? _mvfw2 : (_level == 3 ? _mvfw3 : (_level == 4 ? _mvfw4 : (_level == 5 ? _mvfw5 : _mvfw6))))),
  _level == 1 ? "MDegrain1" : (_level == 2 ? "MDegrain2" : (_level == 3 ? "MDegrain3" : (_level == 4 ? "MDegrain4" : (_level == 5 ? "MDegrain5" : "MDegrain6")))),
  env_ptr,
  (!_mvfw) ? _level * 2 : 1, 
  (!_mvfw) ? _level * 2 - 1 : 0) // 1/3/5
#else
  (!_mvfw) ? _mvbw : (level == 1 ? _mvfw : (level == 2 ? _mvfw2 : (level == 3 ? _mvfw3 : (level == 4 ? _mvfw4 : (level == 5 ? _mvfw5 : _mvfw6))))),
  level == 1 ? "MDegrain1" : (level == 2 ? "MDegrain2" : (level == 3 ? "MDegrain3" : (level == 4 ? "MDegrain4" : (level == 5 ? "MDegrain5" : "MDegrain6")))),
  env_ptr,
  (!_mvfw) ? level * 2 : 1, 
  (!_mvfw) ? level * 2 - 1 : 0) // 1/3/5
#endif
  , super(_super)
  , lsb_flag(_lsb_flag)
  , height_lsb_mul(_lsb_flag ? 2 : 1)
#ifndef LEVEL_IS_TEMPLATE
  , level( _level )
#endif
  , DstPlanes(0)
  , SrcPlanes(0)
{
  //DstShortAlign32 = nullptr;
  DstShort = nullptr;
  //DstIntAlign32 = nullptr;
  DstInt = nullptr;
  const int group_len = level * 2; // 2, 4, 6
                                   // remark: _nSCD1 and 2 are scaled with bits_per_pixel in MVClip
  mvClipF[0] = new MVClip((!_mvfw) ? _mvbw : _mvfw, _nSCD1, _nSCD2, env_ptr, (!_mvfw) ? group_len : 1, (!_mvfw) ? 1 : 0);
  mvClipB[0] = new MVClip((!_mvfw) ? _mvbw : _mvbw, _nSCD1, _nSCD2, env_ptr, (!_mvfw) ? group_len : 1, (!_mvfw) ? 0 : 0);
  if (level >= 2) {
    mvClipF[1] = new MVClip((!_mvfw) ? _mvbw : _mvfw2, _nSCD1, _nSCD2, env_ptr, (!_mvfw) ? group_len : 1, (!_mvfw) ? 3 : 0);
    mvClipB[1] = new MVClip((!_mvfw) ? _mvbw : _mvbw2, _nSCD1, _nSCD2, env_ptr, (!_mvfw) ? group_len : 1, (!_mvfw) ? 2 : 0);
    if (level >= 3) {
      mvClipF[2] = new MVClip((!_mvfw) ? _mvbw : _mvfw3, _nSCD1, _nSCD2, env_ptr, (!_mvfw) ? group_len : 1, (!_mvfw) ? 5 : 0);
      mvClipB[2] = new MVClip((!_mvfw) ? _mvbw : _mvbw3, _nSCD1, _nSCD2, env_ptr, (!_mvfw) ? group_len : 1, (!_mvfw) ? 4 : 0);
      if (level >= 4) {
        mvClipF[3] = new MVClip((!_mvfw) ? _mvbw : _mvfw4, _nSCD1, _nSCD2, env_ptr, (!_mvfw) ? group_len : 1, (!_mvfw) ? 7 : 0);
        mvClipB[3] = new MVClip((!_mvfw) ? _mvbw : _mvbw4, _nSCD1, _nSCD2, env_ptr, (!_mvfw) ? group_len : 1, (!_mvfw) ? 6 : 0);
        if (level >= 5) {
          mvClipF[4] = new MVClip((!_mvfw) ? _mvbw : _mvfw5, _nSCD1, _nSCD2, env_ptr, (!_mvfw) ? group_len : 1, (!_mvfw) ? 9 : 0);
          mvClipB[4] = new MVClip((!_mvfw) ? _mvbw : _mvbw5, _nSCD1, _nSCD2, env_ptr, (!_mvfw) ? group_len : 1, (!_mvfw) ? 8 : 0);
          if (level >= 6) {
            mvClipF[5] = new MVClip((!_mvfw) ? _mvbw : _mvfw6, _nSCD1, _nSCD2, env_ptr, (!_mvfw) ? group_len : 1, (!_mvfw) ? 11 : 0);
            mvClipB[5] = new MVClip((!_mvfw) ? _mvbw : _mvbw6, _nSCD1, _nSCD2, env_ptr, (!_mvfw) ? group_len : 1, (!_mvfw) ? 10 : 0);
          }
        }
      }
    }
  }

    // e.g. 10000*999999 is too much
  thSAD = (uint64_t)_thSAD * mvClipB[0]->GetThSCD1() / _nSCD1; // normalize to block SAD
  thSADC = (uint64_t)_thSADC * mvClipB[0]->GetThSCD1() / _nSCD1; // chroma
  // nSCD1 is already scaled in MVClip constructor, no further scale is needed

  YUVplanes = _YUVplanes;
  nLimit = _nLimit;
  nLimitC = _nLimitC;
  isse_flag = _isse2;
  planar = _planar;

  CheckSimilarity(*mvClipB[0], "mvbw", env_ptr);
  CheckSimilarity(*mvClipF[0], "mvfw", env_ptr);
  if (level >= 2) {
    CheckSimilarity(*mvClipF[1], "mvfw2", env_ptr);
    CheckSimilarity(*mvClipB[1], "mvbw2", env_ptr);
    if (level >= 3) {
      CheckSimilarity(*mvClipF[2], "mvfw3", env_ptr);
      CheckSimilarity(*mvClipB[2], "mvbw3", env_ptr);
      if (level >= 4) {
        CheckSimilarity(*mvClipF[3], "mvfw4", env_ptr);
        CheckSimilarity(*mvClipB[3], "mvbw4", env_ptr);
        if (level >= 5) {
          CheckSimilarity(*mvClipF[4], "mvfw5", env_ptr);
          CheckSimilarity(*mvClipB[4], "mvbw5", env_ptr);
          if (level >= 6) {
            CheckSimilarity(*mvClipF[5], "mvfw6", env_ptr);
            CheckSimilarity(*mvClipB[5], "mvbw6", env_ptr);
          }
        }
      }
    }
  }

  if (mvClipB[0]->GetDeltaFrame() <= 0 || mvClipF[0]->GetDeltaFrame() <= 0)   // 2.5.11.22, 
                                                                              //todo check PF 160926: 2nd must be clipF, not the same clipB
    env_ptr->ThrowError("MDegrain%d: cannot use motion vectors with absolute frame references.", level);

  const ::VideoInfo &	vi_super = _super->GetVideoInfo();

  //pixelsize and bits_per_pixel: in MVFilter, property of motion vectors

  pixelsize_super = vi_super.ComponentSize();
  bits_per_pixel_super = vi_super.BitsPerComponent();
  pixelsize_super_shift = ilog2(pixelsize_super);
  // pixelsize, bits_per_pixel: vector clip data

  // no need for SAD scaling, it is coming from the mv clip analysis. nSCD1 is already scaled in MVClip constructor
  /* must be good from 2.7.13.22
  thSAD = sad_t(thSAD / 255.0 * ((1 << bits_per_pixel) - 1));
  thSADC = sad_t(thSADC / 255.0 * ((1 << bits_per_pixel) - 1));
  */
  // todo: use it, for spare one multiplication in weighting func
  thSADpow2 = thSAD * thSAD;
  thSADCpow2 = thSADC * thSADC;

  // get parameters of prepared super clip - v2.0
  SuperParams64Bits params;
  memcpy(&params, &vi_super.num_audio_samples, 8);
  int nHeightS = params.nHeight;
  int nSuperHPad = params.nHPad;
  int nSuperVPad = params.nVPad;
  int nSuperPel = params.nPel;
  nSuperModeYUV = params.nModeYUV;
  int nSuperLevels = params.nLevels;
  for (int i = 0; i < level; i++) {
    pRefBGOF[i] = new MVGroupOfFrames(nSuperLevels, nWidth, nHeight, nSuperPel, nSuperHPad, nSuperVPad, nSuperModeYUV, isse_flag, xRatioUV, yRatioUV, pixelsize_super, bits_per_pixel_super, _mt_flag);
    pRefFGOF[i] = new MVGroupOfFrames(nSuperLevels, nWidth, nHeight, nSuperPel, nSuperHPad, nSuperVPad, nSuperModeYUV, isse_flag, xRatioUV, yRatioUV, pixelsize_super, bits_per_pixel_super, _mt_flag);
  }
  int nSuperWidth = vi_super.width;
  int nSuperHeight = vi_super.height;

  if (nHeight != nHeightS
    || nHeight != vi.height
    || nWidth != nSuperWidth - nSuperHPad * 2
    || nWidth != vi.width
    || nPel != nSuperPel)
  {
    env_ptr->ThrowError("MDegrainX : wrong source or super frame size");
  }

  if (lsb_flag && (pixelsize != 1 || pixelsize_super != 1))
    env_ptr->ThrowError("MDegrainX : lsb_flag only for 8 bit sources");

  if (bits_per_pixel_super != bits_per_pixel) {
    env_ptr->ThrowError("MDegrainX : clip and super clip have different bit depths");
  }

  if ((pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2 && !planar)
  {
    DstPlanes = new YUY2Planes(nWidth, nHeight * height_lsb_mul);
    SrcPlanes = new YUY2Planes(nWidth, nHeight);
  }
  dstShortPitch = ((nWidth + 15) / 16) * 16;  // short (2 byte) granularity
  dstIntPitch = dstShortPitch; // int (4 byte) granularity, 4*16
  if (nOverlapX > 0 || nOverlapY > 0)
  {
    OverWins = new OverlapWindows(nBlkSizeX, nBlkSizeY, nOverlapX, nOverlapY);
    OverWinsUV = new OverlapWindows(nBlkSizeX >> nLogxRatioUV, nBlkSizeY >> nLogyRatioUV, nOverlapX >> nLogxRatioUV, nOverlapY >> nLogyRatioUV);
    if (lsb_flag || pixelsize_super > 1)
    {
      DstInt = (int *)_aligned_malloc(dstIntPitch * nHeight * sizeof(int), 32); 
    }
    else
    {
      DstShort = (unsigned short *)_aligned_malloc(dstShortPitch * nHeight * sizeof(short), 32); 
    }
  }

  // in overlaps.h
  // OverlapsLsbFunction
  // OverlapsFunction
  // in M(V)DegrainX: DenoiseXFunction
  arch_t arch;
  if ((((env_ptr->GetCPUFlags() & CPUF_AVX2) != 0) & isse_flag))
    arch = USE_AVX2;
  else if ((((env_ptr->GetCPUFlags() & CPUF_AVX) != 0) & isse_flag))
    arch = USE_AVX;
  else if ((((env_ptr->GetCPUFlags() & CPUF_SSE4_1) != 0) & isse_flag))
    arch = USE_SSE41;
  else if ((((env_ptr->GetCPUFlags() & CPUF_SSE2) != 0) & isse_flag))
    arch = USE_SSE2;
  else
    arch = NO_SIMD;

  // C only -> NO_SIMD
  // lsb 16-bit hack: uint8_t only
  OVERSLUMALSB = get_overlaps_lsb_function(nBlkSizeX, nBlkSizeY, sizeof(uint8_t), NO_SIMD);
  OVERSCHROMALSB = get_overlaps_lsb_function(nBlkSizeX >> nLogxRatioUV, nBlkSizeY >> nLogyRatioUV, sizeof(uint8_t), NO_SIMD);

  OVERSLUMA = get_overlaps_function(nBlkSizeX, nBlkSizeY, sizeof(uint8_t), arch);
  OVERSCHROMA = get_overlaps_function(nBlkSizeX >> nLogxRatioUV, nBlkSizeY >> nLogyRatioUV, sizeof(uint8_t), arch);

  OVERSLUMA16 = get_overlaps_function(nBlkSizeX, nBlkSizeY, sizeof(uint16_t), arch);
  OVERSCHROMA16 = get_overlaps_function(nBlkSizeX >> nLogxRatioUV, nBlkSizeY >> nLogyRatioUV, sizeof(uint16_t), arch);

  DEGRAINLUMA = get_denoise123_function(nBlkSizeX, nBlkSizeY, pixelsize_super, lsb_flag, level, arch);
  DEGRAINCHROMA = get_denoise123_function(nBlkSizeX >> nLogxRatioUV, nBlkSizeY >> nLogyRatioUV, pixelsize_super, lsb_flag, level, arch);
  if (!OVERSLUMA)
    env_ptr->ThrowError("MDegrain%d : no valid OVERSLUMA function for %dx%d, pixelsize=%d, lsb_flag=%d, level=%d", level, nBlkSizeX, nBlkSizeY, pixelsize_super, (int)lsb_flag, level);
  if (!OVERSCHROMA)
    env_ptr->ThrowError("MDegrain%d : no valid OVERSCHROMA function for %dx%d, pixelsize=%d, lsb_flag=%d, level=%d", level, nBlkSizeX, nBlkSizeY, pixelsize_super, (int)lsb_flag, level);
  if (!DEGRAINLUMA)
    env_ptr->ThrowError("MDegrain%d : no valid DEGRAINLUMA function for %dx%d, pixelsize=%d, lsb_flag=%d, level=%d", level, nBlkSizeX, nBlkSizeY, pixelsize_super, (int)lsb_flag, level);
  if (!DEGRAINCHROMA)
    env_ptr->ThrowError("MDegrain%d : no valid DEGRAINCHROMA function for %dx%d, pixelsize=%d, lsb_flag=%d, level=%d", level, nBlkSizeX, nBlkSizeY, pixelsize_super, (int)lsb_flag, level);

#ifndef LEVEL_IS_TEMPLATE
  switch (level) {
  case 1: NORMWEIGHTS = norm_weights<1>; break;
  case 2: NORMWEIGHTS = norm_weights<2>; break;
  case 3: NORMWEIGHTS = norm_weights<3>; break;
  case 4: NORMWEIGHTS = norm_weights<4>; break;
  case 5: NORMWEIGHTS = norm_weights<5>; break;
  case 6: NORMWEIGHTS = norm_weights<6>; break;
  }
#endif

  // max blocksize = 32, 170507: 64 (moved to const)
  const int		tmp_size = MAX_BLOCK_SIZE * MAX_BLOCK_SIZE * pixelsize_super;
  tmpBlock = (uint8_t *)_aligned_malloc(tmp_size * height_lsb_mul, 64); // new BYTE[tmp_size * height_lsb_mul]; PF. 16.10.26
  tmpBlockLsb = (lsb_flag) ? (tmpBlock + tmp_size) : 0;

  if (lsb_flag)
  {
    vi.height <<= 1;
  }
}


#ifdef LEVEL_IS_TEMPLATE
template<int level>
MVDegrainX<level>::~MVDegrainX()
#else
MVDegrainX::~MVDegrainX()
#endif
{
  if ((pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2 && !planar)
  {
    delete DstPlanes;
    delete SrcPlanes;
  }
  if (nOverlapX > 0 || nOverlapY > 0)
  {
    delete OverWins;
    delete OverWinsUV;
    _aligned_free(DstShort);
    _aligned_free(DstInt);
  }
  _aligned_free(tmpBlock);
  for (int i = 0; i < level; i++) {
    delete pRefBGOF[i];
    delete pRefFGOF[i];
  }
}




#ifdef LEVEL_IS_TEMPLATE
template<int level>
PVideoFrame __stdcall MVDegrainX<level>::GetFrame(int n, IScriptEnvironment* env)
#else
PVideoFrame __stdcall MVDegrainX::GetFrame(int n, IScriptEnvironment* env)
#endif
{
  int nWidth_B = nBlkX*(nBlkSizeX - nOverlapX) + nOverlapX;
  int nHeight_B = nBlkY*(nBlkSizeY - nOverlapY) + nOverlapY;

  BYTE *pDst[3], *pDstCur[3];
  const BYTE *pSrcCur[3];
  const BYTE *pSrc[3];
  const BYTE *pRefB[MAX_DEGRAIN][3];
  const BYTE *pRefF[MAX_DEGRAIN][3];
  int nDstPitches[3], nSrcPitches[3];
  int nRefBPitches[MAX_DEGRAIN][3], nRefFPitches[MAX_DEGRAIN][3];
  unsigned char *pDstYUY2;
  const unsigned char *pSrcYUY2;
  int nDstPitchYUY2;
  int nSrcPitchYUY2;
  bool isUsableB[MAX_DEGRAIN], isUsableF[MAX_DEGRAIN];

  PVideoFrame mvF[MAX_DEGRAIN];
  PVideoFrame mvB[MAX_DEGRAIN];

  framenumber = n; // debug

  for (int j = level - 1; j >= 0; j--)
  {
    mvF[j] = mvClipF[j]->GetFrame(n, env);
    mvClipF[j]->Update(mvF[j], env);
    isUsableF[j] = mvClipF[j]->IsUsable();
    mvF[j] = 0; // v2.0.9.2 -  it seems, we do not need in vectors clip anymore when we finished copiing them to fakeblockdatas
  }
  for (int j = 0; j < level; j++)
  {
    mvB[j] = mvClipB[j]->GetFrame(n, env);
    mvClipB[j]->Update(mvB[j], env);
    isUsableB[j] = mvClipB[j]->IsUsable();
    mvB[j] = 0;
  }
  int				lsb_offset_y = 0;
  int				lsb_offset_u = 0;
  int				lsb_offset_v = 0;

  //	if ( mvClipB.IsUsable() && mvClipF.IsUsable() && mvClipB2.IsUsable() && mvClipF2.IsUsable() )
  //	{
  PVideoFrame	src = child->GetFrame(n, env);
  PVideoFrame dst = env->NewVideoFrame(vi);
  if ((pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2)
  {
    if (!planar)
    {
      pDstYUY2 = dst->GetWritePtr();
      nDstPitchYUY2 = dst->GetPitch();
      pDst[0] = DstPlanes->GetPtr();
      pDst[1] = DstPlanes->GetPtrU();
      pDst[2] = DstPlanes->GetPtrV();
      nDstPitches[0] = DstPlanes->GetPitch();
      nDstPitches[1] = DstPlanes->GetPitchUV();
      nDstPitches[2] = DstPlanes->GetPitchUV();
      pSrcYUY2 = src->GetReadPtr();
      nSrcPitchYUY2 = src->GetPitch();
      pSrc[0] = SrcPlanes->GetPtr();
      pSrc[1] = SrcPlanes->GetPtrU();
      pSrc[2] = SrcPlanes->GetPtrV();
      nSrcPitches[0] = SrcPlanes->GetPitch();
      nSrcPitches[1] = SrcPlanes->GetPitchUV();
      nSrcPitches[2] = SrcPlanes->GetPitchUV();
      YUY2ToPlanes(pSrcYUY2, nSrcPitchYUY2, nWidth, nHeight,
        pSrc[0], nSrcPitches[0], pSrc[1], pSrc[2], nSrcPitches[1], isse_flag);
    }
    else
    {
      pDst[0] = dst->GetWritePtr();
      pDst[1] = pDst[0] + nWidth;
      pDst[2] = pDst[1] + nWidth / 2;
      nDstPitches[0] = dst->GetPitch();
      nDstPitches[1] = nDstPitches[0];
      nDstPitches[2] = nDstPitches[0];
      pSrc[0] = src->GetReadPtr();
      pSrc[1] = pSrc[0] + nWidth;
      pSrc[2] = pSrc[1] + nWidth / 2;
      nSrcPitches[0] = src->GetPitch();
      nSrcPitches[1] = nSrcPitches[0];
      nSrcPitches[2] = nSrcPitches[0];
    }
  }
  else
  {
    pDst[0] = YWPLAN(dst);
    pDst[1] = UWPLAN(dst);
    pDst[2] = VWPLAN(dst);
    nDstPitches[0] = YPITCH(dst);
    nDstPitches[1] = UPITCH(dst);
    nDstPitches[2] = VPITCH(dst);
    pSrc[0] = YRPLAN(src);
    pSrc[1] = URPLAN(src);
    pSrc[2] = VRPLAN(src);
    nSrcPitches[0] = YPITCH(src);
    nSrcPitches[1] = UPITCH(src);
    nSrcPitches[2] = VPITCH(src);
  }

  lsb_offset_y = nDstPitches[0] * nHeight;
  lsb_offset_u = nDstPitches[1] * (nHeight >> nLogyRatioUV);
  lsb_offset_v = nDstPitches[2] * (nHeight >> nLogyRatioUV);

  if (lsb_flag)
  {
    memset(pDst[0] + lsb_offset_y, 0, lsb_offset_y);
    if (!planar)
    {
      memset(pDst[1] + lsb_offset_u, 0, lsb_offset_u);
      memset(pDst[2] + lsb_offset_v, 0, lsb_offset_v);
    }
  }

  PVideoFrame refB[MAX_DEGRAIN], refF[MAX_DEGRAIN];

  // reorder ror regular frames order in v2.0.9.2
  for (int j = level - 1; j >= 0; j--)
    mvClipF[j]->use_ref_frame(refF[j], isUsableF[j], super, n, env);
  for (int j = 0; j < level; j++)
    mvClipB[j]->use_ref_frame(refB[j], isUsableB[j], super, n, env);

  if ((pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2)
  {
    for (int j = level - 1; j >= 0; j--)
    {
      if (isUsableF[j])
      {
        pRefF[j][0] = refF[j]->GetReadPtr();
        pRefF[j][1] = pRefF[j][0] + refF[j]->GetRowSize() / 2;
        pRefF[j][2] = pRefF[j][1] + refF[j]->GetRowSize() / 4;
        nRefFPitches[j][0] = refF[j]->GetPitch();
        nRefFPitches[j][1] = nRefFPitches[j][0];
        nRefFPitches[j][2] = nRefFPitches[j][0];
      }
    }
    for (int j = 0; j < level; j++)
    {
      if (isUsableB[j])
      {
        pRefB[j][0] = refB[j]->GetReadPtr();
        pRefB[j][1] = pRefB[j][0] + refB[j]->GetRowSize() / 2;
        pRefB[j][2] = pRefB[j][1] + refB[j]->GetRowSize() / 4;
        nRefBPitches[j][0] = refB[j]->GetPitch();
        nRefBPitches[j][1] = nRefBPitches[j][0];
        nRefBPitches[j][2] = nRefBPitches[j][0];
      }
    }
  }
  else
  {
    for (int j = level - 1; j >= 0; j--)
    {
      if (isUsableF[j])
      {
        pRefF[j][0] = YRPLAN(refF[j]);
        pRefF[j][1] = URPLAN(refF[j]);
        pRefF[j][2] = VRPLAN(refF[j]);
        nRefFPitches[j][0] = YPITCH(refF[j]);
        nRefFPitches[j][1] = UPITCH(refF[j]);
        nRefFPitches[j][2] = VPITCH(refF[j]);
      }
    }

    for (int j = 0; j < level; j++)
    {
      if (isUsableB[j])
      {
        pRefB[j][0] = YRPLAN(refB[j]);
        pRefB[j][1] = URPLAN(refB[j]);
        pRefB[j][2] = VRPLAN(refB[j]);
        nRefBPitches[j][0] = YPITCH(refB[j]);
        nRefBPitches[j][1] = UPITCH(refB[j]);
        nRefBPitches[j][2] = VPITCH(refB[j]);
      }
    }
  }

  MVPlane *pPlanesB[3][MAX_DEGRAIN] = { 0 };
  MVPlane *pPlanesF[3][MAX_DEGRAIN] = { 0 };

  for (int j = level - 1; j >= 0; j--)
  {
    if (isUsableF[j])
    {
      pRefFGOF[j]->Update(YUVplanes, (BYTE*)pRefF[j][0], nRefFPitches[j][0], (BYTE*)pRefF[j][1], nRefFPitches[j][1], (BYTE*)pRefF[j][2], nRefFPitches[j][2]);
      if (YUVplanes & YPLANE)
        pPlanesF[0][j] = pRefFGOF[j]->GetFrame(0)->GetPlane(YPLANE);
      if (YUVplanes & UPLANE)
        pPlanesF[1][j] = pRefFGOF[j]->GetFrame(0)->GetPlane(UPLANE);
      if (YUVplanes & VPLANE)
        pPlanesF[2][j] = pRefFGOF[j]->GetFrame(0)->GetPlane(VPLANE);
    }
  }

  for (int j = 0; j < level; j++)
  {
    if (isUsableB[j])
    {
      pRefBGOF[j]->Update(YUVplanes, (BYTE*)pRefB[j][0], nRefBPitches[j][0], (BYTE*)pRefB[j][1], nRefBPitches[j][1], (BYTE*)pRefB[j][2], nRefBPitches[j][2]);// v2.0
      if (YUVplanes & YPLANE)
        pPlanesB[0][j] = pRefBGOF[j]->GetFrame(0)->GetPlane(YPLANE);
      if (YUVplanes & UPLANE)
        pPlanesB[1][j] = pRefBGOF[j]->GetFrame(0)->GetPlane(UPLANE);
      if (YUVplanes & VPLANE)
        pPlanesB[2][j] = pRefBGOF[j]->GetFrame(0)->GetPlane(VPLANE);
    }
  }

  PROFILE_START(MOTION_PROFILE_COMPENSATION);
  pDstCur[0] = pDst[0];
  pDstCur[1] = pDst[1];
  pDstCur[2] = pDst[2];
  pSrcCur[0] = pSrc[0];
  pSrcCur[1] = pSrc[1];
  pSrcCur[2] = pSrc[2];

  // -----------------------------------------------------------------------------
  // -----------------------------------------------------------------------------
  // LUMA plane Y

  if (!(YUVplanes & YPLANE))
  {
    BitBlt(pDstCur[0], nDstPitches[0], pSrcCur[0], nSrcPitches[0], nWidth << pixelsize_super_shift, nHeight, isse_flag);
  }

  else
  {

    if (nOverlapX == 0 && nOverlapY == 0)
    {
      for (int by = 0; by < nBlkY; by++)
      {
        int xx = 0; // logical offset. Mul by 2 for pixelsize_super==2. Don't mul for indexing int* array
        for (int bx = 0; bx < nBlkX; bx++)
        {
          int i = by*nBlkX + bx;
          const BYTE * pB[MAX_DEGRAIN], *pF[MAX_DEGRAIN];
          int npB[MAX_DEGRAIN], npF[MAX_DEGRAIN];
          int WSrc;
          int WRefB[MAX_DEGRAIN], WRefF[MAX_DEGRAIN];

          for (int j = 0; j < level; j++) {
            use_block_y(pB[j], npB[j], WRefB[j], isUsableB[j], *mvClipB[j], i, pPlanesB[0][j], pSrcCur[0], xx << pixelsize_super_shift, nSrcPitches[0]);
            use_block_y(pF[j], npF[j], WRefF[j], isUsableF[j], *mvClipF[j], i, pPlanesF[0][j], pSrcCur[0], xx << pixelsize_super_shift, nSrcPitches[0]);
          }
#ifndef LEVEL_IS_TEMPLATE
          NORMWEIGHTS(WSrc, WRefB, WRefF);
#else
          norm_weights<level>(WSrc, WRefB, WRefF);
#endif

          // luma
          DEGRAINLUMA(pDstCur[0] + (xx << pixelsize_super_shift), pDstCur[0] + lsb_offset_y + (xx << pixelsize_super_shift),
            lsb_flag, nDstPitches[0], pSrcCur[0] + (xx << pixelsize_super_shift), nSrcPitches[0],
            pB, npB, pF, npF,
            WSrc, WRefB, WRefF
          );

          xx += nBlkSizeX; // xx: indexing offset

          if (bx == nBlkX - 1 && nWidth_B < nWidth) // right non-covered region
          {
            // luma
            BitBlt(pDstCur[0] + (nWidth_B << pixelsize_super_shift), nDstPitches[0],
              pSrcCur[0] + (nWidth_B << pixelsize_super_shift), nSrcPitches[0], (nWidth - nWidth_B) << pixelsize_super_shift, nBlkSizeY, isse_flag);
          }
        }	// for bx

        pDstCur[0] += (nBlkSizeY) * (nDstPitches[0]);
        pSrcCur[0] += (nBlkSizeY) * (nSrcPitches[0]);

        if (by == nBlkY - 1 && nHeight_B < nHeight) // bottom uncovered region
        {
          // luma
          BitBlt(pDstCur[0], nDstPitches[0], pSrcCur[0], nSrcPitches[0], nWidth << pixelsize_super_shift, nHeight - nHeight_B, isse_flag);
        }
      }	// for by
    }	// nOverlapX==0 && nOverlapY==0

      // -----------------------------------------------------------------

    else // overlap
    {
      unsigned short *pDstShort = DstShort;
      int *pDstInt = DstInt;
      const int tmpPitch = nBlkSizeX;

      if (lsb_flag || pixelsize_super == 2)
      {
        MemZoneSet(reinterpret_cast<unsigned char*>(pDstInt), 0,
          nWidth_B * sizeof(int), nHeight_B, 0, 0, dstIntPitch * sizeof(int));
      }
      else
      {
        MemZoneSet(reinterpret_cast<unsigned char*>(pDstShort), 0,
          nWidth_B * sizeof(short), nHeight_B, 0, 0, dstShortPitch * sizeof(short));
      }

      for (int by = 0; by < nBlkY; by++)
      {
        int wby = ((by + nBlkY - 3) / (nBlkY - 2)) * 3;
        int xx = 0; // logical offset. Mul by 2 for pixelsize_super==2. Don't mul for indexing int* array
        for (int bx = 0; bx < nBlkX; bx++)
        {
          // select window
          int wbx = (bx + nBlkX - 3) / (nBlkX - 2);
          short *winOver = OverWins->GetWindow(wby + wbx);

          int i = by*nBlkX + bx;

          const BYTE * pB[MAX_DEGRAIN], *pF[MAX_DEGRAIN];
          int npB[MAX_DEGRAIN], npF[MAX_DEGRAIN];
          int WSrc;
          int WRefB[MAX_DEGRAIN], WRefF[MAX_DEGRAIN];

          for (int j = 0; j < level; j++) {
            use_block_y(pB[j], npB[j], WRefB[j], isUsableB[j], *mvClipB[j], i, pPlanesB[0][j], pSrcCur[0], xx << pixelsize_super_shift, nSrcPitches[0]);
            use_block_y(pF[j], npF[j], WRefF[j], isUsableF[j], *mvClipF[j], i, pPlanesF[0][j], pSrcCur[0], xx << pixelsize_super_shift, nSrcPitches[0]);
          }
#ifndef LEVEL_IS_TEMPLATE
          NORMWEIGHTS(WSrc, WRefB, WRefF);
#else
          norm_weights<level>(WSrc, WRefB, WRefF);
#endif
          // luma
          DEGRAINLUMA(tmpBlock, tmpBlockLsb, lsb_flag, tmpPitch << pixelsize_super_shift, pSrcCur[0] + (xx << pixelsize_super_shift), nSrcPitches[0],
            pB, npB, pF, npF,
            WSrc,
            WRefB, WRefF
          );
          if (lsb_flag)
          {
            OVERSLUMALSB(pDstInt + xx, dstIntPitch, tmpBlock, tmpBlockLsb, tmpPitch, winOver, nBlkSizeX);
          }
          else if (pixelsize_super == 1)
          {
            OVERSLUMA(pDstShort + xx, dstShortPitch, tmpBlock, tmpPitch, winOver, nBlkSizeX);
          }
          else if (pixelsize_super == 2) {
            OVERSLUMA16((uint16_t *)(pDstInt + xx), dstIntPitch, tmpBlock, tmpPitch << pixelsize_super_shift, winOver, nBlkSizeX);
          }

          xx += (nBlkSizeX - nOverlapX);
        }	// for bx

        pSrcCur[0] += (nBlkSizeY - nOverlapY) * (nSrcPitches[0]); // byte pointer
        pDstShort += (nBlkSizeY - nOverlapY) * dstShortPitch; // short pointer
        pDstInt += (nBlkSizeY - nOverlapY) * dstIntPitch; // int pointer
      }	// for by
      if (lsb_flag)
      {
        Short2BytesLsb(pDst[0], pDst[0] + lsb_offset_y, nDstPitches[0], DstInt, dstIntPitch, nWidth_B, nHeight_B);
      }
      else if (pixelsize_super == 1)
      { 
        if(isse_flag)
          Short2Bytes_sse2(pDst[0], nDstPitches[0], DstShort, dstShortPitch, nWidth_B, nHeight_B);
        else
          Short2Bytes(pDst[0], nDstPitches[0], DstShort, dstShortPitch, nWidth_B, nHeight_B);
      }
      else if (pixelsize_super == 2)
      {
        Short2Bytes_Int32toWord16((uint16_t *)(pDst[0]), nDstPitches[0], DstInt, dstIntPitch, nWidth_B, nHeight_B, bits_per_pixel_super);
        //Short2Bytes_Int32toWord16_sse4((uint16_t *)(pDst[0]), nDstPitches[0], DstInt, dstIntPitch, nWidth_B, nHeight_B, bits_per_pixel_super);
      }
      else if (pixelsize_super == 4)
      {
        Short2Bytes_FloatInInt32ArrayToFloat((float *)(pDst[0]), nDstPitches[0], DstInt, dstIntPitch, nWidth_B, nHeight_B);
      }
      if (nWidth_B < nWidth)
      {
        BitBlt(pDst[0] + (nWidth_B << pixelsize_super_shift), nDstPitches[0],
          pSrc[0] + (nWidth_B << pixelsize_super_shift), nSrcPitches[0],
          (nWidth - nWidth_B) << pixelsize_super_shift, nHeight_B, isse_flag);
      }
      if (nHeight_B < nHeight) // bottom noncovered region
      {
        BitBlt(pDst[0] + nHeight_B*nDstPitches[0], nDstPitches[0],
          pSrc[0] + nHeight_B*nSrcPitches[0], nSrcPitches[0],
          nWidth << pixelsize_super_shift, nHeight - nHeight_B, isse_flag);
      }
    }	// overlap - end

    if (nLimit < (1 << bits_per_pixel_super) - 1)
    {
      if ((pixelsize_super == 1) && isse_flag)
      {
        LimitChanges_sse2(pDst[0], nDstPitches[0], pSrc[0], nSrcPitches[0], nWidth, nHeight, nLimit);
      }
      else
      {
        if (pixelsize_super == 1)
          LimitChanges_c<uint8_t>(pDst[0], nDstPitches[0], pSrc[0], nSrcPitches[0], nWidth, nHeight, nLimit);
        else
          LimitChanges_c<uint16_t>(pDst[0], nDstPitches[0], pSrc[0], nSrcPitches[0], nWidth, nHeight, nLimit);
      }
    }
  }

  //----------------------------------------------------------------------------
  // -----------------------------------------------------------------------------
  // CHROMA plane U
  process_chroma(
    UPLANE & nSuperModeYUV,
    pDst[1], pDstCur[1], nDstPitches[1], pSrc[1], pSrcCur[1], nSrcPitches[1],
    isUsableB, isUsableF,
    pPlanesB[1], pPlanesF[1],
    lsb_offset_u, nWidth_B, nHeight_B
  );

  //----------------------------------------------------------------------------------
  // -----------------------------------------------------------------------------
  // CHROMA plane V

  process_chroma(
    VPLANE & nSuperModeYUV,
    pDst[2], pDstCur[2], nDstPitches[2], pSrc[2], pSrcCur[2], nSrcPitches[2],
    isUsableB, isUsableF,
    pPlanesB[2], pPlanesF[2],
    lsb_offset_v, nWidth_B, nHeight_B
  );

  //--------------------------------------------------------------------------------

#ifndef _M_X64
  _mm_empty();	// (we may use double-float somewhere) Fizick
#endif

  PROFILE_STOP(MOTION_PROFILE_COMPENSATION);

  if ((pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2 && !planar)
  {
    YUY2FromPlanes(pDstYUY2, nDstPitchYUY2, nWidth, nHeight * height_lsb_mul,
      pDst[0], nDstPitches[0], pDst[1], pDst[2], nDstPitches[1], isse_flag);
  }

  return dst;
}



#ifdef LEVEL_IS_TEMPLATE
template<int level>
void	MVDegrainX<level>::process_chroma(int plane_mask, BYTE *pDst, BYTE *pDstCur, int nDstPitch, const BYTE *pSrc, const BYTE *pSrcCur, int nSrcPitch,
  bool isUsableB[MAX_DEGRAIN], bool isUsableF[MAX_DEGRAIN], MVPlane *pPlanesB[MAX_DEGRAIN], MVPlane *pPlanesF[MAX_DEGRAIN],
  int lsb_offset_uv, int nWidth_B, int nHeight_B)
#else
void	MVDegrainX::process_chroma(int plane_mask, BYTE *pDst, BYTE *pDstCur, int nDstPitch, const BYTE *pSrc, const BYTE *pSrcCur, int nSrcPitch,
  bool isUsableB[MAX_DEGRAIN], bool isUsableF[MAX_DEGRAIN], MVPlane *pPlanesB[MAX_DEGRAIN], MVPlane *pPlanesF[MAX_DEGRAIN],
  int lsb_offset_uv, int nWidth_B, int nHeight_B)
#endif
{
  if (!(YUVplanes & plane_mask))
  {
    BitBlt(pDstCur, nDstPitch, pSrcCur, nSrcPitch, (nWidth >> nLogxRatioUV) << pixelsize_super_shift, nHeight >> nLogyRatioUV, isse_flag);
  }

  else
  {
    if (nOverlapX == 0 && nOverlapY == 0)
    {
      int effective_nSrcPitch = (nBlkSizeY >> nLogyRatioUV) * nSrcPitch; // pitch is byte granularity
      int effective_nDstPitch = (nBlkSizeY >> nLogyRatioUV) * nDstPitch; // pitch is short granularity

      for (int by = 0; by < nBlkY; by++)
      {
        int xx = 0; // index
        for (int bx = 0; bx < nBlkX; bx++)
        {
          int i = by*nBlkX + bx;
          const BYTE * pBV[MAX_DEGRAIN], *pFV[MAX_DEGRAIN];
          int npBV[MAX_DEGRAIN], npFV[MAX_DEGRAIN];
          int WSrc, WRefB[MAX_DEGRAIN], WRefF[MAX_DEGRAIN];

          for (int j = 0; j < level; j++) {
            // xx: byte granularity pointer shift
            use_block_uv(pBV[j], npBV[j], WRefB[j], isUsableB[j], *mvClipB[j], i, pPlanesB[j], pSrcCur, xx << pixelsize_super_shift, nSrcPitch);
            use_block_uv(pFV[j], npFV[j], WRefF[j], isUsableF[j], *mvClipF[j], i, pPlanesF[j], pSrcCur, xx << pixelsize_super_shift, nSrcPitch);
          }
#ifndef LEVEL_IS_TEMPLATE
          NORMWEIGHTS(WSrc, WRefB, WRefF);
#else
          norm_weights<level>(WSrc, WRefB, WRefF);
#endif
          // chroma
          DEGRAINCHROMA(pDstCur + (xx << pixelsize_super_shift), pDstCur + (xx << pixelsize_super_shift) + lsb_offset_uv,
            lsb_flag, nDstPitch, pSrcCur + (xx << pixelsize_super_shift), nSrcPitch,
            pBV, npBV, pFV, npFV,
            WSrc, WRefB, WRefF
          );

          xx += (nBlkSizeX >> nLogxRatioUV); // xx: indexing offset

          if (bx == nBlkX - 1 && nWidth_B < nWidth) // right non-covered region
          {
            // chroma
            BitBlt(pDstCur + ((nWidth_B  >> nLogxRatioUV) << pixelsize_super_shift), nDstPitch,
              pSrcCur + ((nWidth_B  >> nLogxRatioUV) << pixelsize_super_shift), nSrcPitch, ((nWidth - nWidth_B)  >> nLogxRatioUV) << pixelsize_super_shift, nBlkSizeY >> nLogyRatioUV, isse_flag);
          }
        }	// for bx

        pDstCur += effective_nDstPitch;
        pSrcCur += effective_nSrcPitch;

        if (by == nBlkY - 1 && nHeight_B < nHeight) // bottom uncovered region
        {
          // chroma
          BitBlt(pDstCur, nDstPitch, pSrcCur, nSrcPitch, (nWidth >> nLogxRatioUV) << pixelsize_super_shift, (nHeight - nHeight_B) >> nLogyRatioUV, isse_flag);
        }
      }	// for by
    }	// nOverlapX==0 && nOverlapY==0

      // -----------------------------------------------------------------

    else // overlap
    {
      unsigned short *pDstShort = DstShort;
      int *pDstInt = DstInt;
      const int tmpPitch = nBlkSizeX;

      if (lsb_flag || pixelsize_super > 1)
      {
        MemZoneSet(reinterpret_cast<unsigned char*>(pDstInt), 0,
          nWidth_B * sizeof(int) >> nLogxRatioUV, nHeight_B >> nLogyRatioUV, 0, 0, dstIntPitch * sizeof(int));
      }
      else
      {
        MemZoneSet(reinterpret_cast<unsigned char*>(pDstShort), 0,
          nWidth_B * sizeof(short) >> nLogxRatioUV, nHeight_B >> nLogyRatioUV, 0, 0, dstShortPitch * sizeof(short));
      }

      int effective_nSrcPitch = ((nBlkSizeY - nOverlapY) >> nLogyRatioUV) * nSrcPitch; // pitch is byte granularity
      int effective_dstShortPitch = ((nBlkSizeY - nOverlapY) >> nLogyRatioUV) * dstShortPitch; // pitch is short granularity
      int effective_dstIntPitch = ((nBlkSizeY - nOverlapY) >> nLogyRatioUV) * dstIntPitch; // pitch is int granularity

      for (int by = 0; by < nBlkY; by++)
      {
        int wby = ((by + nBlkY - 3) / (nBlkY - 2)) * 3;
        int xx = 0; // logical offset. Mul by 2 for pixelsize_super==2. Don't mul for indexing int* array
        for (int bx = 0; bx < nBlkX; bx++)
        {
          // select window
          int wbx = (bx + nBlkX - 3) / (nBlkX - 2);
          short *winOverUV = OverWinsUV->GetWindow(wby + wbx);

          int i = by*nBlkX + bx;
          const BYTE * pBV[MAX_DEGRAIN], *pFV[MAX_DEGRAIN];
          int npBV[MAX_DEGRAIN], npFV[MAX_DEGRAIN];
          int WSrc, WRefB[MAX_DEGRAIN], WRefF[MAX_DEGRAIN];

          for (int j = 0; j < level; j++) {
            use_block_uv(pBV[j], npBV[j], WRefB[j], isUsableB[j], *mvClipB[j], i, pPlanesB[j], pSrcCur, xx << pixelsize_super_shift, nSrcPitch);
            use_block_uv(pFV[j], npFV[j], WRefF[j], isUsableF[j], *mvClipF[j], i, pPlanesF[j], pSrcCur, xx << pixelsize_super_shift, nSrcPitch);
          }
#ifndef LEVEL_IS_TEMPLATE
          NORMWEIGHTS(WSrc, WRefB, WRefF);
#else
          norm_weights<level>(WSrc, WRefB, WRefF);
#endif
          // chroma
          DEGRAINCHROMA(tmpBlock, tmpBlockLsb, lsb_flag, tmpPitch << pixelsize_super_shift, pSrcCur + (xx << pixelsize_super_shift), nSrcPitch,
            pBV, npBV, pFV, npFV,
            WSrc, WRefB, WRefF
          );
          if (lsb_flag)
          { // no pixelsize here pointers Int or short
            OVERSCHROMALSB(pDstInt + xx, dstIntPitch, tmpBlock, tmpBlockLsb, tmpPitch, winOverUV, nBlkSizeX >> nLogxRatioUV);
          }
          else if (pixelsize_super == 1)
          {
            OVERSCHROMA(pDstShort + xx, dstShortPitch, tmpBlock, tmpPitch, winOverUV, nBlkSizeX >> nLogxRatioUV);
          }
          else if (pixelsize_super == 2)
          {
            OVERSCHROMA16((uint16_t*)(pDstInt + xx), dstIntPitch, tmpBlock, tmpPitch << pixelsize_super_shift, winOverUV, nBlkSizeX >> nLogxRatioUV);
          }

          xx += ((nBlkSizeX - nOverlapX) >> nLogxRatioUV); // no pixelsize here

        }	// for bx

        pSrcCur += effective_nSrcPitch; // pitch is byte granularity
        pDstShort += effective_dstShortPitch; // pitch is short granularity
        pDstInt += effective_dstIntPitch; // pitch is int granularity
      }	// for by

      if (lsb_flag)
      {
        Short2BytesLsb(pDst, pDst + lsb_offset_uv, nDstPitch, DstInt, dstIntPitch, nWidth_B >> nLogxRatioUV, nHeight_B >> nLogyRatioUV);
      }
      else if (pixelsize_super == 1)
      { // pixelsize
        if (isse_flag)
          Short2Bytes_sse2(pDst, nDstPitch, DstShort, dstShortPitch, nWidth_B >> nLogxRatioUV, nHeight_B >> nLogyRatioUV);
        else
          Short2Bytes(pDst, nDstPitch, DstShort, dstShortPitch, nWidth_B >> nLogxRatioUV, nHeight_B >> nLogyRatioUV);
      }
      else if (pixelsize_super == 2)
      { 
        Short2Bytes_Int32toWord16((uint16_t *)(pDst), nDstPitch, DstInt, dstIntPitch, nWidth_B >> nLogxRatioUV, nHeight_B >> nLogyRatioUV, bits_per_pixel_super);
        //Short2Bytes_Int32toWord16_sse4((uint16_t *)(pDst), nDstPitch, DstInt, dstIntPitch, nWidth_B >> nLogxRatioUV, nHeight_B >> nLogyRatioUV, bits_per_pixel_super);
      }
      else if (pixelsize_super == 4)
      {
        Short2Bytes_FloatInInt32ArrayToFloat((float *)(pDst), nDstPitch, DstInt, dstIntPitch, nWidth_B >> nLogxRatioUV, nHeight_B >> nLogyRatioUV);
      }      
      if (nWidth_B < nWidth)
      {
        BitBlt(pDst + ((nWidth_B >> nLogxRatioUV) << pixelsize_super_shift), nDstPitch,
          pSrc + ((nWidth_B >> nLogxRatioUV) << pixelsize_super_shift), nSrcPitch,
          ((nWidth - nWidth_B) >> nLogxRatioUV) << pixelsize_super_shift, nHeight_B >> nLogyRatioUV, isse_flag);
      }
      if (nHeight_B < nHeight) // bottom noncovered region
      {
        BitBlt(pDst + ((nDstPitch*nHeight_B) >> nLogyRatioUV), nDstPitch,
          pSrc + ((nSrcPitch*nHeight_B) >> nLogyRatioUV), nSrcPitch,
          (nWidth >> nLogxRatioUV) << pixelsize_super_shift, (nHeight - nHeight_B) >> nLogyRatioUV, isse_flag);
      }
    }	// overlap - end

    if(pixelsize <= 2) {
      if (nLimitC < (1 << bits_per_pixel_super) - 1)
      {
        if (isse_flag)
        {
          if (pixelsize_super == 1)
            LimitChanges_sse2_new<uint8_t>(pDst, nDstPitch, pSrc, nSrcPitch, nWidth >> nLogxRatioUV, nHeight >> nLogyRatioUV, nLimitC);
          else
            LimitChanges_sse2_new<uint16_t>(pDst, nDstPitch, pSrc, nSrcPitch, nWidth >> nLogxRatioUV, nHeight >> nLogyRatioUV, nLimitC);
        }
        else
        {
          if (pixelsize_super == 1)
            LimitChanges_c<uint8_t>(pDst, nDstPitch, pSrc, nSrcPitch, nWidth >> nLogxRatioUV, nHeight >> nLogyRatioUV, nLimitC);
          else if(pixelsize_super == 2)
            LimitChanges_c<uint16_t>(pDst, nDstPitch, pSrc, nSrcPitch, nWidth >> nLogxRatioUV, nHeight >> nLogyRatioUV, nLimitC);
        }
      }
    }
    else {
      //LimitChanges_float_c(pDst, nDstPitch, pSrc, nSrcPitch, nWidth >> nLogxRatioUV, nHeight >> nLogyRatioUV, nLimitC_f);
    }
  }
}

// todo: put together with use_block_uv,  
// todo: change /xRatioUV and /yRatioUV to bit shifts everywhere
#ifdef LEVEL_IS_TEMPLATE
template<int level>
MV_FORCEINLINE void	MVDegrainX<level>::use_block_y(const BYTE * &p, int &np, int &WRef, bool isUsable, const MVClip &mvclip, int i, const MVPlane *pPlane, const BYTE *pSrcCur, int xx, int nSrcPitch)
#else
MV_FORCEINLINE void	MVDegrainX::use_block_y(const BYTE * &p, int &np, int &WRef, bool isUsable, const MVClip &mvclip, int i, const MVPlane *pPlane, const BYTE *pSrcCur, int xx, int nSrcPitch)
#endif
{
  if (isUsable)
  {
    const FakeBlockData &block = mvclip.GetBlock(0, i);
    int blx = block.GetX() * nPel + block.GetMV().x;
    int bly = block.GetY() * nPel + block.GetMV().y;
    p = pPlane->GetPointer(blx, bly);
    np = pPlane->GetPitch();
    sad_t blockSAD = block.GetSAD(); // SAD of MV Block. Scaled to MVClip's bits_per_pixel;
    WRef = DegrainWeight(thSAD, blockSAD, bits_per_pixel);
  }
  else
  {
    p = pSrcCur + xx; // xx here:byte offset
    np = nSrcPitch;
    WRef = 0;
  }
}

// no difference for 1-2-3
#ifdef LEVEL_IS_TEMPLATE
template<int level>
MV_FORCEINLINE void	MVDegrainX<level>::use_block_uv(const BYTE * &p, int &np, int &WRef, bool isUsable, const MVClip &mvclip, int i, const MVPlane *pPlane, const BYTE *pSrcCur, int xx, int nSrcPitch)
#else
MV_FORCEINLINE void	MVDegrainX::use_block_uv(const BYTE * &p, int &np, int &WRef, bool isUsable, const MVClip &mvclip, int i, const MVPlane *pPlane, const BYTE *pSrcCur, int xx, int nSrcPitch)
#endif
{
  if (isUsable)
  {
    const FakeBlockData &block = mvclip.GetBlock(0, i);
    int blx = block.GetX() * nPel + block.GetMV().x;
    int bly = block.GetY() * nPel + block.GetMV().y;
    p = pPlane->GetPointer(blx >> nLogxRatioUV, bly >> nLogyRatioUV); // pixelsize - aware
    np = pPlane->GetPitch();
    sad_t blockSAD = block.GetSAD();  // SAD of MV Block. Scaled to MVClip's bits_per_pixel;
    WRef = DegrainWeight(thSADC, blockSAD, bits_per_pixel);
  }
  else
  {
    p = pSrcCur + xx; // xx:byte offset
    np = nSrcPitch;
    WRef = 0;
  }
}



template<int level>
MV_FORCEINLINE void	norm_weights(int &WSrc, int(&WRefB)[MAX_DEGRAIN], int(&WRefF)[MAX_DEGRAIN])
{
  WSrc = 256;
  int WSum;
  if (level == 6)
    WSum = WRefB[0] + WRefF[0] + WSrc + WRefB[1] + WRefF[1] + WRefB[2] + WRefF[2] + WRefB[3] + WRefF[3] + WRefB[4] + WRefF[4] + WRefB[5] + WRefF[5] + 1;
  else if (level == 5)
    WSum = WRefB[0] + WRefF[0] + WSrc + WRefB[1] + WRefF[1] + WRefB[2] + WRefF[2] + WRefB[3] + WRefF[3] + WRefB[4] + WRefF[4] + 1;
  else if (level == 4)
    WSum = WRefB[0] + WRefF[0] + WSrc + WRefB[1] + WRefF[1] + WRefB[2] + WRefF[2] + WRefB[3] + WRefF[3] + 1;
  else if (level == 3)
    WSum = WRefB[0] + WRefF[0] + WSrc + WRefB[1] + WRefF[1] + WRefB[2] + WRefF[2] + 1;
  else if (level == 2)
    WSum = WRefB[0] + WRefF[0] + WSrc + WRefB[1] + WRefF[1] + 1;
  else if (level == 1)
    WSum = WRefB[0] + WRefF[0] + WSrc + 1;
  WRefB[0] = WRefB[0] * 256 / WSum; // normalize weights to 256
  WRefF[0] = WRefF[0] * 256 / WSum;
  if (level >= 2) {
    WRefB[1] = WRefB[1] * 256 / WSum; // normalize weights to 256
    WRefF[1] = WRefF[1] * 256 / WSum;
  }
  if (level >= 3) {
    WRefB[2] = WRefB[2] * 256 / WSum; // normalize weights to 256
    WRefF[2] = WRefF[2] * 256 / WSum;
  }
  if (level >= 4) {
    WRefB[3] = WRefB[3] * 256 / WSum; // normalize weights to 256
    WRefF[3] = WRefF[3] * 256 / WSum;
  }
  if (level >= 5) {
    WRefB[4] = WRefB[4] * 256 / WSum; // normalize weights to 256
    WRefF[4] = WRefF[4] * 256 / WSum;
  }
  if (level >= 6) {
    WRefB[5] = WRefB[5] * 256 / WSum; // normalize weights to 256
    WRefF[5] = WRefF[5] * 256 / WSum;
  }
  if (level == 6)
    WSrc = 256 - WRefB[0] - WRefF[0] - WRefB[1] - WRefF[1] - WRefB[2] - WRefF[2] - WRefB[3] - WRefF[3] - WRefB[4] - WRefF[4] - WRefB[5] - WRefF[5];
  else if (level == 5)
    WSrc = 256 - WRefB[0] - WRefF[0] - WRefB[1] - WRefF[1] - WRefB[2] - WRefF[2] - WRefB[3] - WRefF[3] - WRefB[4] - WRefF[4];
  else if (level == 4)
    WSrc = 256 - WRefB[0] - WRefF[0] - WRefB[1] - WRefF[1] - WRefB[2] - WRefF[2] - WRefB[3] - WRefF[3];
  else if (level == 3)
    WSrc = 256 - WRefB[0] - WRefF[0] - WRefB[1] - WRefF[1] - WRefB[2] - WRefF[2];
  else if (level == 2)
    WSrc = 256 - WRefB[0] - WRefF[0] - WRefB[1] - WRefF[1];
  else if (level == 1)
    WSrc = 256 - WRefB[0] - WRefF[0];
}

#ifdef LEVEL_IS_TEMPLATE

template class MVDegrainX<1>;
/*
(PClip _child, PClip _super, PClip _mvbw, PClip _mvfw, PClip _mvbw2, PClip _mvfw2, PClip _mvbw3, PClip _mvfw3, PClip _mvbw4, PClip _mvfw4, PClip _mvbw5, PClip _mvfw5,
sad_t _thSAD, sad_t _thSADC, int _YUVplanes, sad_t _nLimit, sad_t _nLimitC,
sad_t _nSCD1, int _nSCD2, bool _isse2, bool _planar, bool _lsb_flag,
bool _mt_flag, 
#ifndef LEVEL_IS_TEMPLATE
int _level, 
#endif
IScriptEnvironment* env);
*/
template class MVDegrainX<2>;
/*
(PClip _child, PClip _super, PClip _mvbw, PClip _mvfw, PClip _mvbw2, PClip _mvfw2, PClip _mvbw3, PClip _mvfw3, PClip _mvbw4, PClip _mvfw4, PClip _mvbw5, PClip _mvfw5,
sad_t _thSAD, sad_t _thSADC, int _YUVplanes, sad_t _nLimit, sad_t _nLimitC,
sad_t _nSCD1, int _nSCD2, bool _isse2, bool _planar, bool _lsb_flag,
bool _mt_flag, 
#ifndef LEVEL_IS_TEMPLATE
int _level, 
#endif
IScriptEnvironment* env);
#endif
*/
#endif

