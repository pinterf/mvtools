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
Denoise1to6Function* MVDegrainX::get_denoise123_function(int BlockX, int BlockY, int _bits_per_pixel, bool _lsb_flag, bool _out16_flag, int _level, arch_t arch)
#endif
{
  const int _pixelsize = _bits_per_pixel == 8 ? 1 : (_bits_per_pixel <= 16 ? 2 : 4);
  //---------- DENOISE/DEGRAIN
  const int DEGRAIN_TYPE_8BIT = 1;
  const int DEGRAIN_TYPE_8BIT_STACKED = 2;
  const int DEGRAIN_TYPE_8BIT_OUT16 = 4;
  const int DEGRAIN_TYPE_10to14BIT = 8;
  const int DEGRAIN_TYPE_16BIT = 16;
  const int DEGRAIN_TYPE_10to16BIT = DEGRAIN_TYPE_10to14BIT + DEGRAIN_TYPE_16BIT;
  const int DEGRAIN_TYPE_32BIT = 32;
  // BlkSizeX, BlkSizeY, degrain_type, level_of_MDegrain, arch_t
  std::map<std::tuple<int, int, int, int, arch_t>, Denoise1to6Function*> func_degrain;
  using std::make_tuple;

  int type_to_search;
  if (_bits_per_pixel == 8) {
    if (_out16_flag)
      type_to_search = DEGRAIN_TYPE_8BIT_OUT16;
    else if (_lsb_flag)
      type_to_search = DEGRAIN_TYPE_8BIT_STACKED;
    else
      type_to_search = DEGRAIN_TYPE_8BIT;
  }
  else if (_bits_per_pixel <= 14)
    type_to_search = DEGRAIN_TYPE_10to14BIT;
  else if (_bits_per_pixel == 16)
    type_to_search = DEGRAIN_TYPE_16BIT;
  else if (_bits_per_pixel == 32)
    type_to_search = DEGRAIN_TYPE_32BIT;
  else
    return nullptr;


  // level 1-6, 8bit C, 8bit lsb C, 16 bit C (same for all, no blocksize templates)
#define MAKE_FN_LEVEL(x, y, level) \
func_degrain[make_tuple(x, y, DEGRAIN_TYPE_8BIT, level, NO_SIMD)] = Degrain1to6_C<uint8_t, 0, level>; \
func_degrain[make_tuple(x, y, DEGRAIN_TYPE_8BIT_STACKED, level, NO_SIMD)] = Degrain1to6_C<uint8_t, 1, level>; \
func_degrain[make_tuple(x, y, DEGRAIN_TYPE_8BIT_OUT16, level, NO_SIMD)] = Degrain1to6_C<uint8_t, 2, level>; \
func_degrain[make_tuple(x, y, DEGRAIN_TYPE_10to14BIT, level, NO_SIMD)] = Degrain1to6_C<uint16_t, 0, level>; \
func_degrain[make_tuple(x, y, DEGRAIN_TYPE_16BIT, level, NO_SIMD)] = Degrain1to6_C<uint16_t, 0, level>; \
func_degrain[make_tuple(x, y, DEGRAIN_TYPE_32BIT, level, NO_SIMD)] = Degrain1to6_float_C<level>;
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
    MAKE_FN(12, 3)
    MAKE_FN(8, 32)
    MAKE_FN(8, 16)
    MAKE_FN(8, 8)
    MAKE_FN(8, 4)
    MAKE_FN(8, 2)
MAKE_FN(8, 1)
MAKE_FN(6, 24)
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
// and 16 bit SSE4 function
// for no_template_by_y: special height==0 -> internally nHeight comes from variable (for C: both width and height is variable)
//#define OLD_DEGRAIN16BIT
#ifdef OLD_DEGRAIN16BIT
#define MAKE_FN_LEVEL(x, y, level, yy) \
func_degrain[make_tuple(x, y, DEGRAIN_TYPE_8BIT, level, USE_SSE2)] = Degrain1to6_sse2<x, yy, false, level>; \
func_degrain[make_tuple(x, y, DEGRAIN_TYPE_8BIT_STACKED, level, USE_SSE2)] = Degrain1to6_sse2<x, yy, true, level>; \
func_degrain[make_tuple(x, y, DEGRAIN_TYPE_10to14BIT, level, USE_SSE41)] = Degrain1to6_16_sse41_old<x, yy, level>;\
func_degrain[make_tuple(x, y, DEGRAIN_TYPE_16BIT, level, USE_SSE41)] = Degrain1to6_16_sse41_old<x, yy, level>;
#else
#define MAKE_FN_LEVEL(x, y, level, yy) \
func_degrain[make_tuple(x, y, DEGRAIN_TYPE_8BIT, level, USE_SSE2)] = Degrain1to6_sse2<x, yy, 0, level>; \
func_degrain[make_tuple(x, y, DEGRAIN_TYPE_8BIT_STACKED, level, USE_SSE2)] = Degrain1to6_sse2<x, yy, 1, level>; \
func_degrain[make_tuple(x, y, DEGRAIN_TYPE_8BIT_OUT16, level, USE_SSE2)] = Degrain1to6_sse2<x, yy, 2, level>; \
func_degrain[make_tuple(x, y, DEGRAIN_TYPE_10to14BIT, level, USE_SSE41)] = Degrain1to6_16_sse41<x, yy, level, true>; \
func_degrain[make_tuple(x, y, DEGRAIN_TYPE_16BIT, level, USE_SSE41)] = Degrain1to6_16_sse41<x, yy, level, false>;
#endif

#define MAKE_FN(x, y, yy) \
MAKE_FN_LEVEL(x,y,1, yy) \
MAKE_FN_LEVEL(x,y,2, yy) \
MAKE_FN_LEVEL(x,y,3, yy) \
MAKE_FN_LEVEL(x,y,4, yy) \
MAKE_FN_LEVEL(x,y,5, yy) \
MAKE_FN_LEVEL(x,y,6, yy)
MAKE_FN(64, 64, 0)
MAKE_FN(64, 48, 0)
MAKE_FN(64, 32, 0)
MAKE_FN(64, 16, 0)
MAKE_FN(48, 64, 0)
MAKE_FN(48, 48, 0)
MAKE_FN(48, 24, 0)
MAKE_FN(48, 12, 0)
MAKE_FN(32, 64, 0)
MAKE_FN(32, 32, 0)
MAKE_FN(32, 24, 0)
MAKE_FN(32, 16, 0)
MAKE_FN(32, 8, 0)
MAKE_FN(24, 48, 0)
MAKE_FN(24, 32, 0)
MAKE_FN(24, 24, 0)
MAKE_FN(24, 12, 0)
MAKE_FN(24, 6, 0)
MAKE_FN(16, 64, 0)
MAKE_FN(16, 32, 0)
MAKE_FN(16, 16, 0)
MAKE_FN(16, 12, 0)
MAKE_FN(16, 8, 0)
MAKE_FN(16, 4, 4)
MAKE_FN(16, 2, 2)
MAKE_FN(16, 1, 1)
MAKE_FN(12, 48, 0)
MAKE_FN(12, 24, 0)
MAKE_FN(12, 16, 0)
MAKE_FN(12, 12, 0)
MAKE_FN(12, 6, 6)
MAKE_FN(12, 3, 3)
MAKE_FN(8, 32, 0)
MAKE_FN(8, 16, 0)
MAKE_FN(8, 8, 8)
MAKE_FN(8, 4, 4)
MAKE_FN(8, 2, 2)
MAKE_FN(8, 1, 1)
MAKE_FN(6, 24, 0)
MAKE_FN(6, 12, 0)
MAKE_FN(6, 6, 6)
MAKE_FN(6, 3, 3)
MAKE_FN(4, 8, 8)
MAKE_FN(4, 4, 4)
MAKE_FN(4, 2, 2)
MAKE_FN(4, 1, 1)
MAKE_FN(3, 6, 6)
MAKE_FN(3, 3, 3)
MAKE_FN(2, 4, 4)
MAKE_FN(2, 2, 2)
MAKE_FN(2, 1, 1)
#undef MAKE_FN
#undef MAKE_FN_LEVEL

  Denoise1to6Function* result = nullptr;
  arch_t archlist[] = { USE_AVX2, USE_AVX, USE_SSE41, USE_SSE2, NO_SIMD };
  int index = 0;
  while (result == nullptr) {
    arch_t current_arch_try = archlist[index++];
    if (current_arch_try > arch) continue;
    result = func_degrain[make_tuple(BlockX, BlockY, type_to_search, _level, current_arch_try)];
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
  bool _mt_flag, bool _out16_flag,
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
  , out16_flag(_out16_flag)
  , height_lsb_or_out16_mul((_lsb_flag || _out16_flag) ? 2 : 1)
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
  cpuFlags = _isse2 ? env_ptr->GetCPUFlags() : 0;
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

  const ::VideoInfo &vi_super = _super->GetVideoInfo();


  if (!vi.IsSameColorspace(_super->GetVideoInfo()))
    env_ptr->ThrowError("MDegrain%d: source and super clip video format is different!",level);

  // v2.7.39- make subsampling independent from motion vector's origin:
  // because xRatioUV and yRatioUV: in MVFilter, property of motion vectors
  xRatioUV_super = 1;
  yRatioUV_super = 1;
  if (!vi.IsY() && !vi.IsRGB()) {
    xRatioUV_super = vi.IsYUY2() ? 2 : (1 << vi.GetPlaneWidthSubsampling(PLANAR_U));
    yRatioUV_super = vi.IsYUY2() ? 1 : (1 << vi.GetPlaneHeightSubsampling(PLANAR_U));
  }
  nLogxRatioUV_super = ilog2(xRatioUV_super);
  nLogyRatioUV_super = ilog2(yRatioUV_super);

  // Motion vectors original pixel format now can be 
  // somewhat independent that of source/Super clip. Width, height, padding should match however.

  //pixelsize and bits_per_pixel: in MVFilter, property of motion vectors
  pixelsize_super = vi.ComponentSize();
  bits_per_pixel_super = vi.BitsPerComponent();
  pixelsize_super_shift = ilog2(pixelsize_super);

  // pixelsize, bits_per_pixel: vector clip data

  // no need for SAD scaling, it is coming from the mv clip analysis. nSCD1 is already scaled in MVClip constructor
  /* must be good from 2.7.13.22
  thSAD = sad_t(thSAD / 255.0 * ((1 << bits_per_pixel) - 1));
  thSADC = sad_t(thSADC / 255.0 * ((1 << bits_per_pixel) - 1));
  */
  // todo: use it, to spare one multiplication in weighting func.
  // Done, see defined OLD_DEGRAINWEIGHT
  thSAD_sq= (double)thSAD * thSAD;
  thSADC_sq = (double)thSADC * thSADC;

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
    pRefBGOF[i] = new MVGroupOfFrames(nSuperLevels, nWidth, nHeight, nSuperPel, nSuperHPad, nSuperVPad, nSuperModeYUV, cpuFlags, xRatioUV_super, yRatioUV_super, pixelsize_super, bits_per_pixel_super, _mt_flag);
    pRefFGOF[i] = new MVGroupOfFrames(nSuperLevels, nWidth, nHeight, nSuperPel, nSuperHPad, nSuperVPad, nSuperModeYUV, cpuFlags, xRatioUV_super, yRatioUV_super, pixelsize_super, bits_per_pixel_super, _mt_flag);
  }
  int nSuperWidth = vi_super.width;
  int nSuperHeight = vi_super.height;

  if (nHeight != nHeightS
    || nHeight != vi.height
    || nWidth != nSuperWidth - nSuperHPad * 2
    || nWidth != vi.width
    || nPel != nSuperPel) // PF180227 why to check this one?
  {
    env_ptr->ThrowError("MDegrainX : wrong source or super frame size");
  }

  if (lsb_flag && (pixelsize != 1 || pixelsize_super != 1))
    env_ptr->ThrowError("MDegrainX : lsb flag only for 8 bit sources");

  if (out16_flag) {
    if (pixelsize != 1 || pixelsize_super != 1)
      env_ptr->ThrowError("MDegrainX : out16 flag only for 8 bit sources");
    if(!vi.IsY8() && !vi.IsYV12() && !vi.IsYV16() && !vi.IsYV24())
      env_ptr->ThrowError("MDegrainX : only Y8, YV12, YV16 or YV24 allowed for out16");
  }

  if(lsb_flag && out16_flag)
    env_ptr->ThrowError("MDegrainX : cannot specify both lsb and out16 flag");

  // output can be different bit depth from input
  pixelsize_output = pixelsize_super;
  bits_per_pixel_output = bits_per_pixel_super;
  pixelsize_output_shift = pixelsize_super_shift;
  if (out16_flag) {
    pixelsize_output = sizeof(uint16_t);
    bits_per_pixel_output = 16;
    pixelsize_output_shift = ilog2(pixelsize_output);
  }

  if ((pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2 && !planar)
  {
    DstPlanes = new YUY2Planes(nWidth, nHeight * height_lsb_or_out16_mul);
    SrcPlanes = new YUY2Planes(nWidth, nHeight);
  }
  dstShortPitch = ((nWidth + 15) / 16) * 16;  // short (2 byte) granularity
  dstIntPitch = dstShortPitch; // int (4 byte) granularity, 4*16
  if (nOverlapX > 0 || nOverlapY > 0)
  {
    OverWins = new OverlapWindows(nBlkSizeX, nBlkSizeY, nOverlapX, nOverlapY);
    OverWinsUV = new OverlapWindows(nBlkSizeX >> nLogxRatioUV_super, nBlkSizeY >> nLogyRatioUV_super, nOverlapX >> nLogxRatioUV_super, nOverlapY >> nLogyRatioUV_super);
    if (lsb_flag || pixelsize_output > 1)
    {
      DstInt = (int *)_aligned_malloc(dstIntPitch * nHeight * sizeof(int), 32); // also for float sizeof(int)==sizeof(float)
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
  if ((cpuFlags & CPUF_AVX2) != 0)
    arch = USE_AVX2;
  else if ((cpuFlags & CPUF_AVX) != 0)
    arch = USE_AVX;
  else if ((cpuFlags & CPUF_SSE4_1) != 0)
    arch = USE_SSE41;
  else if ((cpuFlags & CPUF_SSE2) != 0)
    arch = USE_SSE2;
  else
    arch = NO_SIMD;

  // C only -> NO_SIMD
  // lsb 16-bit hack: uint8_t only
  OVERSLUMALSB = get_overlaps_lsb_function(nBlkSizeX, nBlkSizeY, sizeof(uint8_t), NO_SIMD);
  OVERSCHROMALSB = get_overlaps_lsb_function(nBlkSizeX >> nLogxRatioUV_super, nBlkSizeY >> nLogyRatioUV_super, sizeof(uint8_t), NO_SIMD);

  OVERSLUMA = get_overlaps_function(nBlkSizeX, nBlkSizeY, sizeof(uint8_t), arch);
  OVERSCHROMA = get_overlaps_function(nBlkSizeX >> nLogxRatioUV_super, nBlkSizeY >> nLogyRatioUV_super, sizeof(uint8_t), arch);

  OVERSLUMA16 = get_overlaps_function(nBlkSizeX, nBlkSizeY, sizeof(uint16_t), arch);
  OVERSCHROMA16 = get_overlaps_function(nBlkSizeX >> nLogxRatioUV_super, nBlkSizeY >> nLogyRatioUV_super, sizeof(uint16_t), arch);

  OVERSLUMA32 = get_overlaps_function(nBlkSizeX, nBlkSizeY, sizeof(float), arch);
  OVERSCHROMA32 = get_overlaps_function(nBlkSizeX >> nLogxRatioUV_super, nBlkSizeY >> nLogyRatioUV_super, sizeof(float), arch);

  DEGRAINLUMA = get_denoise123_function(nBlkSizeX, nBlkSizeY, bits_per_pixel_super, lsb_flag, out16_flag, level, arch);
  DEGRAINCHROMA = get_denoise123_function(nBlkSizeX >> nLogxRatioUV_super, nBlkSizeY >> nLogyRatioUV_super, bits_per_pixel_super, lsb_flag, out16_flag, level, arch);
  if (!OVERSLUMA)
    env_ptr->ThrowError("MDegrain%d : no valid OVERSLUMA function for %dx%d, bitsperpixel=%d, lsb_flag=%d, level=%d", level, nBlkSizeX, nBlkSizeY, bits_per_pixel_super, (int)lsb_flag, level);
  if (!OVERSCHROMA)
    env_ptr->ThrowError("MDegrain%d : no valid OVERSCHROMA function for %dx%d, bitsperpixel=%d, lsb_flag=%d, level=%d", level, nBlkSizeX, nBlkSizeY, bits_per_pixel_super, (int)lsb_flag, level);
  if (!DEGRAINLUMA)
    env_ptr->ThrowError("MDegrain%d : no valid DEGRAINLUMA function for %dx%d, bitsperpixel=%d, lsb_flag=%d, level=%d", level, nBlkSizeX, nBlkSizeY, bits_per_pixel_super, (int)lsb_flag, level);
  if (!DEGRAINCHROMA)
    env_ptr->ThrowError("MDegrain%d : no valid DEGRAINCHROMA function for %dx%d, bitsperpixel=%d, lsb_flag=%d, level=%d", level, nBlkSizeX, nBlkSizeY, bits_per_pixel_super, (int)lsb_flag, level);

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
  tmpBlock = (uint8_t *)_aligned_malloc(tmp_size * height_lsb_or_out16_mul, 64); // new BYTE[tmp_size * height_lsb_or_out16_mul]; PF. 16.10.26
  tmpBlockLsb = (lsb_flag) ? (tmpBlock + tmp_size) : 0;

  if ((cpuFlags & CPUF_SSE2) != 0)
  {
    if(out16_flag)
      LimitFunction = LimitChanges_src8_target16_c; // todo SSE2
    else if (pixelsize_super == 1)
      LimitFunction = LimitChanges_sse2_new<uint8_t, 0>;
    else if (pixelsize_super == 2) {
      if ((cpuFlags & CPUF_SSE4_1) != 0)
        LimitFunction = LimitChanges_sse2_new<uint16_t, 1>;
      else
        LimitFunction = LimitChanges_sse2_new<uint16_t, 0>;
    }
    else
      LimitFunction = LimitChanges_float_c; // no SSE2
  }
  else
  {
    if (out16_flag)
      LimitFunction = LimitChanges_src8_target16_c; // todo SSE2
    else if (pixelsize_super == 1)
      LimitFunction = LimitChanges_c<uint8_t>;
    else if(pixelsize_super == 2)
      LimitFunction = LimitChanges_c<uint16_t>;
    else
      LimitFunction = LimitChanges_float_c;
  }


  if (lsb_flag)
  {
    vi.height <<= 1;
  }

  if (out16_flag) {
    if (vi.IsY8())
      vi.pixel_type = VideoInfo::CS_Y16;
    else if (vi.IsYV12())
      vi.pixel_type = VideoInfo::CS_YUV420P16;
    else if(vi.IsYV16())
      vi.pixel_type = VideoInfo::CS_YUV422P16;
    else if (vi.IsYV24())
      vi.pixel_type = VideoInfo::CS_YUV444P16;
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
    delete mvClipF[i];
    delete mvClipB[i];
  }
}

static void plane_copy_8_to_16_c(uint8_t *dstp, int dstpitch, const uint8_t *srcp, int srcpitch, int width, int height)
{
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      reinterpret_cast<uint16_t *>(dstp)[x] = srcp[x] << 8;
    }
    dstp += dstpitch;
    srcp += srcpitch;
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
        pSrc[0], nSrcPitches[0], pSrc[1], pSrc[2], nSrcPitches[1], cpuFlags);
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
  lsb_offset_u = nDstPitches[1] * (nHeight >> nLogyRatioUV_super);
  lsb_offset_v = nDstPitches[2] * (nHeight >> nLogyRatioUV_super);

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
    if (out16_flag) {
      // copy 8 bit source to 16bit target
      plane_copy_8_to_16_c(pDstCur[0], nDstPitches[0], pSrcCur[0], nSrcPitches[0], nWidth, nHeight);
    }
    else {
      BitBlt(pDstCur[0], nDstPitches[0], pSrcCur[0], nSrcPitches[0], nWidth << pixelsize_super_shift, nHeight);
    }
  }

  else
  {

    int WidthHeightForC = (nBlkSizeX << 16) + nBlkSizeY; // helps avoiding excessive C templates

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
          DEGRAINLUMA(pDstCur[0] + (xx << pixelsize_output_shift), pDstCur[0] + lsb_offset_y + (xx << pixelsize_super_shift),
            WidthHeightForC, nDstPitches[0], pSrcCur[0] + (xx << pixelsize_super_shift), nSrcPitches[0],
            pB, npB, pF, npF,
            WSrc, WRefB, WRefF
          );

          xx += nBlkSizeX; // xx: indexing offset

          if (bx == nBlkX - 1 && nWidth_B < nWidth) // right non-covered region
          {
            if (out16_flag) {
              // copy 8 bit source to 16bit target
              plane_copy_8_to_16_c(pDstCur[0] + (nWidth_B << pixelsize_output_shift), nDstPitches[0], 
                pSrcCur[0] + (nWidth_B << pixelsize_super_shift), nSrcPitches[0], (nWidth - nWidth_B), nBlkSizeY);
            }
            else {
              // luma
              BitBlt(pDstCur[0] + (nWidth_B << pixelsize_super_shift), nDstPitches[0],
                pSrcCur[0] + (nWidth_B << pixelsize_super_shift), nSrcPitches[0], (nWidth - nWidth_B) << pixelsize_super_shift, nBlkSizeY);
            }
          }
        }	// for bx

        pDstCur[0] += (nBlkSizeY) * (nDstPitches[0]);
        pSrcCur[0] += (nBlkSizeY) * (nSrcPitches[0]);

        if (by == nBlkY - 1 && nHeight_B < nHeight) // bottom uncovered region
        {
          if (out16_flag) {
            // copy 8 bit source to 16bit target
            plane_copy_8_to_16_c(pDstCur[0], nDstPitches[0], pSrcCur[0], nSrcPitches[0], nWidth, nHeight - nHeight_B);
          }
          else {
            // luma
            BitBlt(pDstCur[0], nDstPitches[0], pSrcCur[0], nSrcPitches[0], nWidth << pixelsize_super_shift, nHeight - nHeight_B);
          }
        }
      }	// for by
    }	// nOverlapX==0 && nOverlapY==0

      // -----------------------------------------------------------------

    else // overlap
    {
      unsigned short *pDstShort = DstShort;
      int *pDstInt = DstInt;
      const int tmpPitch = nBlkSizeX;

      if (lsb_flag || pixelsize_output >= 2) // also for 32 bit float
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
        // indexing overlap windows weighting table: top=0 middle=3 bottom=6
        /*
        0 = Top Left    1 = Top Middle    2 = Top Right
        3 = Middle Left 4 = Middle Middle 5 = Middle Right
        6 = Bottom Left 7 = Bottom Middle 8 = Bottom Right
        */

        int wby = (by == 0) ? 0 * 3 : (by == nBlkY - 1) ? 2 * 3 : 1 * 3; // 0 for very first, 2*3 for very last, 1*3 for all others in the middle
        int xx = 0; // logical offset. Mul by 2 for pixelsize_super==2. Don't mul for indexing int* array
        for (int bx = 0; bx < nBlkX; ++bx)
        {
          // select window
          // indexing overlap windows weighting table: left=+0 middle=+1 rightmost=+2
          int wbx = (bx == 0) ? 0 : (bx == nBlkX - 1) ? 2 : 1; // 0 for very first, 2 for very last, 1 for all others in the middle
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
          DEGRAINLUMA(tmpBlock, tmpBlockLsb, WidthHeightForC, tmpPitch << pixelsize_output_shift, pSrcCur[0] + (xx << pixelsize_super_shift), nSrcPitches[0],
            pB, npB, pF, npF,
            WSrc,
            WRefB, WRefF
          );
          if (lsb_flag)
          {
            OVERSLUMALSB(pDstInt + xx, dstIntPitch, tmpBlock, tmpBlockLsb, tmpPitch, winOver, nBlkSizeX);
          }
          else if (out16_flag)
          {
            // cast to match the prototype
            OVERSLUMA16((uint16_t *)(pDstInt + xx), dstIntPitch, tmpBlock, tmpPitch << pixelsize_output_shift, winOver, nBlkSizeX);
          }
          else if (pixelsize_super == 1)
          {
            OVERSLUMA(pDstShort + xx, dstShortPitch, tmpBlock, tmpPitch, winOver, nBlkSizeX);
          }
          else if (pixelsize_super == 2) {
            // cast to match the prototype
            OVERSLUMA16((uint16_t *)(pDstInt + xx), dstIntPitch, tmpBlock, tmpPitch << pixelsize_super_shift, winOver, nBlkSizeX);
          }
          else if (pixelsize_super == 4) {
            // cast to match the prototype
            OVERSLUMA32((uint16_t *)((float *)pDstInt + xx), dstIntPitch, tmpBlock, tmpPitch << pixelsize_super_shift, winOver, nBlkSizeX);
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
      else if (out16_flag)
      {
        if ((cpuFlags & CPUF_SSE4_1) != 0)
          Short2Bytes_Int32toWord16_sse4((uint16_t *)(pDst[0]), nDstPitches[0], DstInt, dstIntPitch, nWidth_B, nHeight_B, bits_per_pixel_output);
        else
          Short2Bytes_Int32toWord16((uint16_t *)(pDst[0]), nDstPitches[0], DstInt, dstIntPitch, nWidth_B, nHeight_B, bits_per_pixel_output);
      }
      else if (pixelsize_super == 1)
      { 
        if((cpuFlags & CPUF_SSE2) != 0)
          Short2Bytes_sse2(pDst[0], nDstPitches[0], DstShort, dstShortPitch, nWidth_B, nHeight_B);
        else
          Short2Bytes(pDst[0], nDstPitches[0], DstShort, dstShortPitch, nWidth_B, nHeight_B);
      }
      else if (pixelsize_super == 2)
      {
        if ((cpuFlags & CPUF_SSE4_1) != 0)
          Short2Bytes_Int32toWord16_sse4((uint16_t *)(pDst[0]), nDstPitches[0], DstInt, dstIntPitch, nWidth_B, nHeight_B, bits_per_pixel_super);
        else
          Short2Bytes_Int32toWord16((uint16_t *)(pDst[0]), nDstPitches[0], DstInt, dstIntPitch, nWidth_B, nHeight_B, bits_per_pixel_super);
      }
      else if (pixelsize_super == 4)
      {
        Short2Bytes_FloatInInt32ArrayToFloat((float *)(pDst[0]), nDstPitches[0], (float *)DstInt, dstIntPitch, nWidth_B, nHeight_B);
      }
      if (nWidth_B < nWidth)
      {
        if (out16_flag) {
          // copy 8 bit source to 16bit target
          plane_copy_8_to_16_c(pDst[0] + (nWidth_B << pixelsize_output_shift), nDstPitches[0],
            pSrc[0] + (nWidth_B << pixelsize_super_shift), nSrcPitches[0],
            (nWidth - nWidth_B), nHeight_B);
        }
        else {
          BitBlt(pDst[0] + (nWidth_B << pixelsize_super_shift), nDstPitches[0],
            pSrc[0] + (nWidth_B << pixelsize_super_shift), nSrcPitches[0],
            (nWidth - nWidth_B) << pixelsize_super_shift, nHeight_B);
        }
      }
      if (nHeight_B < nHeight) // bottom noncovered region
      {
        if (out16_flag) {
          // copy 8 bit source to 16bit target
          plane_copy_8_to_16_c(pDst[0] + nHeight_B * nDstPitches[0], nDstPitches[0],
            pSrc[0] + nHeight_B * nSrcPitches[0], nSrcPitches[0],
            nWidth, nHeight - nHeight_B);
        }
        else {
          BitBlt(pDst[0] + nHeight_B * nDstPitches[0], nDstPitches[0],
            pSrc[0] + nHeight_B * nSrcPitches[0], nSrcPitches[0],
            nWidth << pixelsize_super_shift, nHeight - nHeight_B);
        }
      }
    }	// overlap - end

    if (nLimit < 255)
    {
      // limit is 0-255 relative, for any bit depth
      float realLimit;
      if (pixelsize_output <= 2)
        realLimit = float(nLimit << (bits_per_pixel_output - 8));
      else
        realLimit = (float)nLimit / 255.0f;

      LimitFunction(pDst[0], nDstPitches[0], pSrc[0], nSrcPitches[0], nWidth, nHeight, realLimit);
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
    YUY2FromPlanes(pDstYUY2, nDstPitchYUY2, nWidth, nHeight * height_lsb_or_out16_mul,
      pDst[0], nDstPitches[0], pDst[1], pDst[2], nDstPitches[1], cpuFlags);
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
    if (out16_flag) {
      // copy 8 bit source to 16bit target
      plane_copy_8_to_16_c(pDstCur, nDstPitch, pSrcCur, nSrcPitch, (nWidth >> nLogxRatioUV_super), nHeight >> nLogyRatioUV_super);
    }
    else {
      BitBlt(pDstCur, nDstPitch, pSrcCur, nSrcPitch, (nWidth >> nLogxRatioUV_super) << pixelsize_super_shift, nHeight >> nLogyRatioUV_super);
    }
  }

  else
  {
    int WidthHeightForC_UV = ((nBlkSizeX >> nLogxRatioUV_super) << 16) + (nBlkSizeY >> nLogyRatioUV_super); // helps avoiding excessive C templates

    if (nOverlapX == 0 && nOverlapY == 0)
    {
      int effective_nSrcPitch = (nBlkSizeY >> nLogyRatioUV_super) * nSrcPitch; // pitch is byte granularity
      int effective_nDstPitch = (nBlkSizeY >> nLogyRatioUV_super) * nDstPitch; // pitch is short granularity

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
          DEGRAINCHROMA(pDstCur + (xx << pixelsize_output_shift), pDstCur + (xx << pixelsize_super_shift) + lsb_offset_uv,
            WidthHeightForC_UV, nDstPitch, pSrcCur + (xx << pixelsize_super_shift), nSrcPitch,
            pBV, npBV, pFV, npFV,
            WSrc, WRefB, WRefF
          );

          xx += (nBlkSizeX >> nLogxRatioUV_super); // xx: indexing offset

          if (bx == nBlkX - 1 && nWidth_B < nWidth) // right non-covered region
          {
            if (out16_flag) {
              // copy 8 bit source to 16bit target
              plane_copy_8_to_16_c(pDstCur + ((nWidth_B >> nLogxRatioUV_super) << pixelsize_output_shift), nDstPitch,
                pSrcCur + ((nWidth_B >> nLogxRatioUV_super) << pixelsize_super_shift), nSrcPitch, (nWidth - nWidth_B) >> nLogxRatioUV_super, nBlkSizeY >> nLogyRatioUV_super);
            }
            else {
              // chroma
              BitBlt(pDstCur + ((nWidth_B >> nLogxRatioUV_super) << pixelsize_super_shift), nDstPitch,
                pSrcCur + ((nWidth_B >> nLogxRatioUV_super) << pixelsize_super_shift), nSrcPitch, ((nWidth - nWidth_B) >> nLogxRatioUV_super) << pixelsize_super_shift, nBlkSizeY >> nLogyRatioUV_super);
            }
          }
        }	// for bx

        pDstCur += effective_nDstPitch;
        pSrcCur += effective_nSrcPitch;

        if (by == nBlkY - 1 && nHeight_B < nHeight) // bottom uncovered region
        {
          if (out16_flag) {
            // copy 8 bit source to 16bit target
            plane_copy_8_to_16_c(pDstCur, nDstPitch, pSrcCur, nSrcPitch, nWidth >> nLogxRatioUV_super , (nHeight - nHeight_B) >> nLogyRatioUV_super);
          }
          else {
            // chroma
            BitBlt(pDstCur, nDstPitch, pSrcCur, nSrcPitch, (nWidth >> nLogxRatioUV_super) << pixelsize_super_shift, (nHeight - nHeight_B) >> nLogyRatioUV_super);
          }
        }
      }	// for by
    }	// nOverlapX==0 && nOverlapY==0

      // -----------------------------------------------------------------

    else // overlap
    {
      unsigned short *pDstShort = DstShort;
      int *pDstInt = DstInt;
      const int tmpPitch = nBlkSizeX;

      if (lsb_flag || pixelsize_output > 1)
      {
        MemZoneSet(reinterpret_cast<unsigned char*>(pDstInt), 0,
          nWidth_B * sizeof(int) >> nLogxRatioUV_super, nHeight_B >> nLogyRatioUV_super, 0, 0, dstIntPitch * sizeof(int));
      }
      else
      {
        MemZoneSet(reinterpret_cast<unsigned char*>(pDstShort), 0,
          nWidth_B * sizeof(short) >> nLogxRatioUV_super, nHeight_B >> nLogyRatioUV_super, 0, 0, dstShortPitch * sizeof(short));
      }

      int effective_nSrcPitch = ((nBlkSizeY - nOverlapY) >> nLogyRatioUV_super) * nSrcPitch; // pitch is byte granularity
      int effective_dstShortPitch = ((nBlkSizeY - nOverlapY) >> nLogyRatioUV_super) * dstShortPitch; // pitch is short granularity
      int effective_dstIntPitch = ((nBlkSizeY - nOverlapY) >> nLogyRatioUV_super) * dstIntPitch; // pitch is int granularity

      for (int by = 0; by < nBlkY; by++)
      {
        // indexing overlap windows weighting table: top=0 middle=3 bottom=6
        /*
        0 = Top Left    1 = Top Middle    2 = Top Right
        3 = Middle Left 4 = Middle Middle 5 = Middle Right
        6 = Bottom Left 7 = Bottom Middle 8 = Bottom Right
        */

        int wby = (by == 0) ? 0 * 3 : (by == nBlkY - 1) ? 2 * 3 : 1 * 3; // 0 for very first, 2*3 for very last, 1*3 for all others in the middle
        int xx = 0; // logical offset. Mul by 2 for pixelsize_super==2. Don't mul for indexing int* array
        for (int bx = 0; bx < nBlkX; ++bx)
        {
          // select window
          // indexing overlap windows weighting table: left=+0 middle=+1 rightmost=+2
          int wbx = (bx == 0) ? 0 : (bx == nBlkX - 1) ? 2 : 1; // 0 for very first, 2 for very last, 1 for all others in the middle
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
          DEGRAINCHROMA(tmpBlock, tmpBlockLsb, WidthHeightForC_UV, tmpPitch << pixelsize_output_shift, pSrcCur + (xx << pixelsize_super_shift), nSrcPitch,
            pBV, npBV, pFV, npFV,
            WSrc, WRefB, WRefF
          );
          if (lsb_flag)
          { // no pixelsize here pointers Int or short
            OVERSCHROMALSB(pDstInt + xx, dstIntPitch, tmpBlock, tmpBlockLsb, tmpPitch, winOverUV, nBlkSizeX >> nLogxRatioUV_super);
          }
          else if (out16_flag)
          {
            // cast to match the prototype
            OVERSCHROMA16((uint16_t*)(pDstInt + xx), dstIntPitch, tmpBlock, tmpPitch << pixelsize_output_shift, winOverUV, nBlkSizeX >> nLogxRatioUV_super);
          }
          else if (pixelsize_super == 1)
          {
            OVERSCHROMA(pDstShort + xx, dstShortPitch, tmpBlock, tmpPitch, winOverUV, nBlkSizeX >> nLogxRatioUV_super);
          }
          else if (pixelsize_super == 2)
          {
            // cast to match the prototype
            OVERSCHROMA16((uint16_t*)(pDstInt + xx), dstIntPitch, tmpBlock, tmpPitch << pixelsize_super_shift, winOverUV, nBlkSizeX >> nLogxRatioUV_super);
          }
          else //if (pixelsize_super == 4)
          {
            // cast to match the prototype
            OVERSCHROMA32((uint16_t*)((float *)pDstInt + xx), dstIntPitch, tmpBlock, tmpPitch << pixelsize_super_shift, winOverUV, nBlkSizeX >> nLogxRatioUV_super);
          }

          xx += ((nBlkSizeX - nOverlapX) >> nLogxRatioUV_super); // no pixelsize here

        }	// for bx

        pSrcCur += effective_nSrcPitch; // pitch is byte granularity
        pDstShort += effective_dstShortPitch; // pitch is short granularity
        pDstInt += effective_dstIntPitch; // pitch is int granularity
      }	// for by

      if (lsb_flag)
      {
        Short2BytesLsb(pDst, pDst + lsb_offset_uv, nDstPitch, DstInt, dstIntPitch, nWidth_B >> nLogxRatioUV_super, nHeight_B >> nLogyRatioUV_super);
      }
      else if (out16_flag)
      {
        if ((cpuFlags & CPUF_SSE4_1) != 0)
          Short2Bytes_Int32toWord16_sse4((uint16_t *)(pDst), nDstPitch, DstInt, dstIntPitch, nWidth_B >> nLogxRatioUV_super, nHeight_B >> nLogyRatioUV_super, bits_per_pixel_output);
        else
          Short2Bytes_Int32toWord16((uint16_t *)(pDst), nDstPitch, DstInt, dstIntPitch, nWidth_B >> nLogxRatioUV_super, nHeight_B >> nLogyRatioUV_super, bits_per_pixel_output);
      }
      else if (pixelsize_super == 1)
      { // pixelsize
        if ((cpuFlags & CPUF_SSE2) != 0)
          Short2Bytes_sse2(pDst, nDstPitch, DstShort, dstShortPitch, nWidth_B >> nLogxRatioUV_super, nHeight_B >> nLogyRatioUV_super);
        else
          Short2Bytes(pDst, nDstPitch, DstShort, dstShortPitch, nWidth_B >> nLogxRatioUV_super, nHeight_B >> nLogyRatioUV_super);
      }
      else if (pixelsize_super == 2)
      { 
        if ((cpuFlags & CPUF_SSE4_1) != 0)
          Short2Bytes_Int32toWord16_sse4((uint16_t *)(pDst), nDstPitch, DstInt, dstIntPitch, nWidth_B >> nLogxRatioUV_super, nHeight_B >> nLogyRatioUV_super, bits_per_pixel_super);
        else
          Short2Bytes_Int32toWord16((uint16_t *)(pDst), nDstPitch, DstInt, dstIntPitch, nWidth_B >> nLogxRatioUV_super, nHeight_B >> nLogyRatioUV_super, bits_per_pixel_super);
      }
      else if (pixelsize_super == 4)
      {
        Short2Bytes_FloatInInt32ArrayToFloat((float *)(pDst), nDstPitch, (float *)DstInt, dstIntPitch, nWidth_B >> nLogxRatioUV_super, nHeight_B >> nLogyRatioUV_super);
      }      
      if (nWidth_B < nWidth)
      {
        if (out16_flag) {
          // copy 8 bit source to 16bit target
          plane_copy_8_to_16_c(pDst + ((nWidth_B >> nLogxRatioUV_super) << pixelsize_output_shift), nDstPitch,
            pSrc + ((nWidth_B >> nLogxRatioUV_super) << pixelsize_super_shift), nSrcPitch,
            (nWidth - nWidth_B) >> nLogxRatioUV_super, nHeight_B >> nLogyRatioUV_super);
        }
        else {
          BitBlt(pDst + ((nWidth_B >> nLogxRatioUV_super) << pixelsize_super_shift), nDstPitch,
            pSrc + ((nWidth_B >> nLogxRatioUV_super) << pixelsize_super_shift), nSrcPitch,
            ((nWidth - nWidth_B) >> nLogxRatioUV_super) << pixelsize_super_shift, nHeight_B >> nLogyRatioUV_super);
        }
      }
      if (nHeight_B < nHeight) // bottom noncovered region
      {
        if (out16_flag) {
          // copy 8 bit source to 16bit target
          plane_copy_8_to_16_c(pDst + ((nDstPitch*nHeight_B) >> nLogyRatioUV_super), nDstPitch,
            pSrc + ((nSrcPitch*nHeight_B) >> nLogyRatioUV_super), nSrcPitch,
            (nWidth >> nLogxRatioUV_super), (nHeight - nHeight_B) >> nLogyRatioUV_super);
        }
        else {
          BitBlt(pDst + ((nDstPitch*nHeight_B) >> nLogyRatioUV_super), nDstPitch,
            pSrc + ((nSrcPitch*nHeight_B) >> nLogyRatioUV_super), nSrcPitch,
            (nWidth >> nLogxRatioUV_super) << pixelsize_super_shift, (nHeight - nHeight_B) >> nLogyRatioUV_super);
        }
      }
    }	// overlap - end

    if (nLimitC < 255)
    {
      // limit is 0-255 relative, for any bit depth
      float realLimitc;
      if (pixelsize_output <= 2)
        realLimitc = float(nLimitC << (bits_per_pixel_output - 8));
      else
        realLimitc = (float)nLimitC / 255.0f;
      LimitFunction(pDst, nDstPitch, pSrc, nSrcPitch, nWidth >> nLogxRatioUV_super, nHeight >> nLogyRatioUV_super, realLimitc);
    }
  }
}

// todo: put together with use_block_uv,  
// todo: change /xRatioUV_super and /yRatioUV_super to bit shifts everywhere
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
    WRef = DegrainWeight(thSAD, thSAD_sq, blockSAD);
  }
  else
  {
    // just to point on a valid area. It'll be read in code but its weight will be zero, anyway.
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
    p = pPlane->GetPointer(blx >> nLogxRatioUV_super, bly >> nLogyRatioUV_super); // pixelsize - aware
    np = pPlane->GetPitch();
    sad_t blockSAD = block.GetSAD();  // SAD of MV Block. Scaled to MVClip's bits_per_pixel;
    WRef = DegrainWeight(thSADC, thSADC_sq, blockSAD);
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
  constexpr int DEGRAIN_WMAX = 1 << DEGRAIN_WEIGHT_BITS;
  WSrc = DEGRAIN_WMAX;
  int WSum;
  if constexpr(level == 6)
    WSum = WRefB[0] + WRefF[0] + WSrc + WRefB[1] + WRefF[1] + WRefB[2] + WRefF[2] + WRefB[3] + WRefF[3] + WRefB[4] + WRefF[4] + WRefB[5] + WRefF[5] + 1;
  else if constexpr(level == 5)
    WSum = WRefB[0] + WRefF[0] + WSrc + WRefB[1] + WRefF[1] + WRefB[2] + WRefF[2] + WRefB[3] + WRefF[3] + WRefB[4] + WRefF[4] + 1;
  else if constexpr(level == 4)
    WSum = WRefB[0] + WRefF[0] + WSrc + WRefB[1] + WRefF[1] + WRefB[2] + WRefF[2] + WRefB[3] + WRefF[3] + 1;
  else if constexpr(level == 3)
    WSum = WRefB[0] + WRefF[0] + WSrc + WRefB[1] + WRefF[1] + WRefB[2] + WRefF[2] + 1;
  else if constexpr(level == 2)
    WSum = WRefB[0] + WRefF[0] + WSrc + WRefB[1] + WRefF[1] + 1;
  else if constexpr(level == 1)
    WSum = WRefB[0] + WRefF[0] + WSrc + 1;
  WRefB[0] = WRefB[0] * DEGRAIN_WMAX / WSum; // normalize weights to 256
  WRefF[0] = WRefF[0] * DEGRAIN_WMAX / WSum;
  if constexpr(level >= 2) {
    WRefB[1] = WRefB[1] * DEGRAIN_WMAX / WSum; // normalize weights to 256
    WRefF[1] = WRefF[1] * DEGRAIN_WMAX / WSum;
  }
  if constexpr(level >= 3) {
    WRefB[2] = WRefB[2] * DEGRAIN_WMAX / WSum; // normalize weights to 256
    WRefF[2] = WRefF[2] * DEGRAIN_WMAX / WSum;
  }
  if constexpr(level >= 4) {
    WRefB[3] = WRefB[3] * DEGRAIN_WMAX / WSum; // normalize weights to 256
    WRefF[3] = WRefF[3] * DEGRAIN_WMAX / WSum;
  }
  if constexpr(level >= 5) {
    WRefB[4] = WRefB[4] * DEGRAIN_WMAX / WSum; // normalize weights to 256
    WRefF[4] = WRefF[4] * DEGRAIN_WMAX / WSum;
  }
  if constexpr(level >= 6) {
    WRefB[5] = WRefB[5] * 256 / WSum; // normalize weights to 256
    WRefF[5] = WRefF[5] * 256 / WSum;
  }
  // make sure sum is DEGRAIN_WMAX
  if constexpr(level == 6)
    WSrc = DEGRAIN_WMAX - WRefB[0] - WRefF[0] - WRefB[1] - WRefF[1] - WRefB[2] - WRefF[2] - WRefB[3] - WRefF[3] - WRefB[4] - WRefF[4] - WRefB[5] - WRefF[5];
  else if constexpr(level == 5)
    WSrc = DEGRAIN_WMAX - WRefB[0] - WRefF[0] - WRefB[1] - WRefF[1] - WRefB[2] - WRefF[2] - WRefB[3] - WRefF[3] - WRefB[4] - WRefF[4];
  else if constexpr(level == 4)
    WSrc = DEGRAIN_WMAX - WRefB[0] - WRefF[0] - WRefB[1] - WRefF[1] - WRefB[2] - WRefF[2] - WRefB[3] - WRefF[3];
  else if constexpr(level == 3)
    WSrc = DEGRAIN_WMAX - WRefB[0] - WRefF[0] - WRefB[1] - WRefF[1] - WRefB[2] - WRefF[2];
  else if constexpr(level == 2)
    WSrc = DEGRAIN_WMAX - WRefB[0] - WRefF[0] - WRefB[1] - WRefF[1];
  else if constexpr(level == 1)
    WSrc = DEGRAIN_WMAX - WRefB[0] - WRefF[0];
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

