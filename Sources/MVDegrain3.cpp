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

//#include	<mmintrin.h>

// PF 160926: MDegrain3 -> MDegrainX: common 1..5 level MDegrain functions

#if 0
MVDegrainX::Denoise3Function* MVDegrainX::get_denoise3_function(int BlockX, int BlockY, int pixelsize, arch_t arch)
{
    // 8 bit only (pixelsize==1)
    //---------- DENOISE/DEGRAIN
    // BlkSizeX, BlkSizeY, pixelsize, arch_t
    std::map<std::tuple<int, int, int, arch_t>, Denoise3Function*> func_degrain;
    using std::make_tuple;

    func_degrain[make_tuple(32, 32, 1, NO_SIMD)] = Degrain3_C<32, 32>;
    func_degrain[make_tuple(32, 16, 1, NO_SIMD)] = Degrain3_C<32, 16>;
    func_degrain[make_tuple(32, 8 , 1, NO_SIMD)] = Degrain3_C<32, 8>;
    func_degrain[make_tuple(16, 32, 1, NO_SIMD)] = Degrain3_C<16, 32>;
    func_degrain[make_tuple(16, 16, 1, NO_SIMD)] = Degrain3_C<16, 16>;
    func_degrain[make_tuple(16, 8 , 1, NO_SIMD)] = Degrain3_C<16, 8>;
    func_degrain[make_tuple(16, 4 , 1, NO_SIMD)] = Degrain3_C<16, 4>;
    func_degrain[make_tuple(16, 2 , 1, NO_SIMD)] = Degrain3_C<16, 2>;
    func_degrain[make_tuple(8 , 16, 1, NO_SIMD)] = Degrain3_C<8 , 16>;
    func_degrain[make_tuple(8 , 8 , 1, NO_SIMD)] = Degrain3_C<8 , 8>;
    func_degrain[make_tuple(8 , 4 , 1, NO_SIMD)] = Degrain3_C<8 , 4>;
    func_degrain[make_tuple(8 , 2 , 1, NO_SIMD)] = Degrain3_C<8 , 2>;
    func_degrain[make_tuple(8 , 1 , 1, NO_SIMD)] = Degrain3_C<8 , 1>;
    func_degrain[make_tuple(4 , 8 , 1, NO_SIMD)] = Degrain3_C<4 , 8>;
    func_degrain[make_tuple(4 , 4 , 1, NO_SIMD)] = Degrain3_C<4 , 4>;
    func_degrain[make_tuple(4 , 2 , 1, NO_SIMD)] = Degrain3_C<4 , 2>;
    func_degrain[make_tuple(2 , 4 , 1, NO_SIMD)] = Degrain3_C<2 , 4>;
    func_degrain[make_tuple(2 , 2 , 1, NO_SIMD)] = Degrain3_C<2 , 2>;

#if 0
#ifndef _M_X64
    func_degrain[make_tuple(32, 32, 1, USE_MMX)] = Degrain3_mmx<32, 32>;
    func_degrain[make_tuple(32, 16, 1, USE_MMX)] = Degrain3_mmx<32, 16>;
    func_degrain[make_tuple(32, 8 , 1, USE_MMX)] = Degrain3_mmx<32, 8>;
    func_degrain[make_tuple(16, 32, 1, USE_MMX)] = Degrain3_mmx<16, 32>;
    func_degrain[make_tuple(16, 16, 1, USE_MMX)] = Degrain3_mmx<16, 16>;
    func_degrain[make_tuple(16, 8 , 1, USE_MMX)] = Degrain3_mmx<16, 8>;
    func_degrain[make_tuple(16, 4 , 1, USE_MMX)] = Degrain3_mmx<16, 4>;
    func_degrain[make_tuple(16, 2 , 1, USE_MMX)] = Degrain3_mmx<16, 2>;
    func_degrain[make_tuple(8 , 16, 1, USE_MMX)] = Degrain3_mmx<8 , 16>;
    func_degrain[make_tuple(8 , 8 , 1, USE_MMX)] = Degrain3_mmx<8 , 8>;
    func_degrain[make_tuple(8 , 4 , 1, USE_MMX)] = Degrain3_mmx<8 , 4>;
    func_degrain[make_tuple(8 , 2 , 1, USE_MMX)] = Degrain3_mmx<8 , 2>;
    func_degrain[make_tuple(8 , 1 , 1, USE_MMX)] = Degrain3_mmx<8 , 1>;
    func_degrain[make_tuple(4 , 8 , 1, USE_MMX)] = Degrain3_mmx<4 , 8>;
    func_degrain[make_tuple(4 , 4 , 1, USE_MMX)] = Degrain3_mmx<4 , 4>;
    func_degrain[make_tuple(4 , 2 , 1, USE_MMX)] = Degrain3_mmx<4 , 2>;
    func_degrain[make_tuple(2 , 4 , 1, USE_MMX)] = Degrain3_mmx<2 , 4>;
    func_degrain[make_tuple(2 , 2 , 1, USE_MMX)] = Degrain3_mmx<2 , 2>;
#endif
#endif
    func_degrain[make_tuple(32, 32, 1, USE_SSE2)] = Degrain3_sse2<32, 32>;
    func_degrain[make_tuple(32, 16, 1, USE_SSE2)] = Degrain3_sse2<32, 16>;
    func_degrain[make_tuple(32, 8 , 1, USE_SSE2)] = Degrain3_sse2<32, 8>;
    func_degrain[make_tuple(16, 32, 1, USE_SSE2)] = Degrain3_sse2<16, 32>;
    func_degrain[make_tuple(16, 16, 1, USE_SSE2)] = Degrain3_sse2<16, 16>;
    func_degrain[make_tuple(16, 8 , 1, USE_SSE2)] = Degrain3_sse2<16, 8>;
    func_degrain[make_tuple(16, 4 , 1, USE_SSE2)] = Degrain3_sse2<16, 4>;
    func_degrain[make_tuple(16, 2 , 1, USE_SSE2)] = Degrain3_sse2<16, 2>;
    func_degrain[make_tuple(8 , 16, 1, USE_SSE2)] = Degrain3_sse2<8 , 16>;
    func_degrain[make_tuple(8 , 8 , 1, USE_SSE2)] = Degrain3_sse2<8 , 8>;
    func_degrain[make_tuple(8 , 4 , 1, USE_SSE2)] = Degrain3_sse2<8 , 4>;
    func_degrain[make_tuple(8 , 2 , 1, USE_SSE2)] = Degrain3_sse2<8 , 2>;
    func_degrain[make_tuple(8 , 1 , 1, USE_SSE2)] = Degrain3_sse2<8 , 1>;
    func_degrain[make_tuple(4 , 8 , 1, USE_SSE2)] = Degrain3_sse2<4 , 8>;
    func_degrain[make_tuple(4 , 4 , 1, USE_SSE2)] = Degrain3_sse2<4 , 4>;
    func_degrain[make_tuple(4 , 2 , 1, USE_SSE2)] = Degrain3_sse2<4 , 2>;
    func_degrain[make_tuple(2 , 4 , 1, USE_SSE2)] = Degrain3_sse2<2 , 4>;
    func_degrain[make_tuple(2 , 2 , 1, USE_SSE2)] = Degrain3_sse2<2 , 2>;

    return func_degrain[make_tuple(BlockX, BlockY, pixelsize, arch)];
}
#endif

MVDegrainX::Denoise1to5Function* MVDegrainX::get_denoise123_function(int BlockX, int BlockY, int pixelsize, bool lsb_flag, int level, arch_t arch)
{
  // 8 bit only (pixelsize==1)
  //---------- DENOISE/DEGRAIN
  // BlkSizeX, BlkSizeY, pixelsize, lsb_flag, level_of_MDegrain, arch_t
  std::map<std::tuple<int, int, int, bool, int, arch_t>, Denoise1to5Function*> func_degrain;
  using std::make_tuple;

  // C, level1, lsb=false
  func_degrain[make_tuple(32, 32, 1, false, 1, NO_SIMD)] = Degrain1to5_C<uint8_t, 32, 32, false, 1>;
  func_degrain[make_tuple(32, 16, 1, false, 1, NO_SIMD)] = Degrain1to5_C<uint8_t, 32, 16, false, 1>;
  func_degrain[make_tuple(32, 8 , 1, false, 1, NO_SIMD)] = Degrain1to5_C<uint8_t, 32, 8 , false, 1>;
  func_degrain[make_tuple(16, 32, 1, false, 1, NO_SIMD)] = Degrain1to5_C<uint8_t, 16, 32, false, 1>;
  func_degrain[make_tuple(16, 16, 1, false, 1, NO_SIMD)] = Degrain1to5_C<uint8_t, 16, 16, false, 1>;
  func_degrain[make_tuple(16, 8 , 1, false, 1, NO_SIMD)] = Degrain1to5_C<uint8_t, 16, 8 , false, 1>;
  func_degrain[make_tuple(16, 4 , 1, false, 1, NO_SIMD)] = Degrain1to5_C<uint8_t, 16, 4 , false, 1>;
  func_degrain[make_tuple(16, 2 , 1, false, 1, NO_SIMD)] = Degrain1to5_C<uint8_t, 16, 2 , false, 1>;
  func_degrain[make_tuple(8 , 16, 1, false, 1, NO_SIMD)] = Degrain1to5_C<uint8_t, 8 , 16, false, 1>;
  func_degrain[make_tuple(8 , 8 , 1, false, 1, NO_SIMD)] = Degrain1to5_C<uint8_t, 8 , 8 , false, 1>;
  func_degrain[make_tuple(8 , 4 , 1, false, 1, NO_SIMD)] = Degrain1to5_C<uint8_t, 8 , 4 , false, 1>;
  func_degrain[make_tuple(8 , 2 , 1, false, 1, NO_SIMD)] = Degrain1to5_C<uint8_t, 8 , 2 , false, 1>;
  func_degrain[make_tuple(8 , 1 , 1, false, 1, NO_SIMD)] = Degrain1to5_C<uint8_t, 8 , 1 , false, 1>;
  func_degrain[make_tuple(4 , 8 , 1, false, 1, NO_SIMD)] = Degrain1to5_C<uint8_t, 4 , 8 , false, 1>;
  func_degrain[make_tuple(4 , 4 , 1, false, 1, NO_SIMD)] = Degrain1to5_C<uint8_t, 4 , 4 , false, 1>;
  func_degrain[make_tuple(4 , 2 , 1, false, 1, NO_SIMD)] = Degrain1to5_C<uint8_t, 4 , 2 , false, 1>;
  func_degrain[make_tuple(2 , 4 , 1, false, 1, NO_SIMD)] = Degrain1to5_C<uint8_t, 2 , 4 , false, 1>;
  func_degrain[make_tuple(2 , 2 , 1, false, 1, NO_SIMD)] = Degrain1to5_C<uint8_t, 2 , 2 , false, 1>;
  // C, level1, lsb=true
  func_degrain[make_tuple(32, 32, 1, true, 1, NO_SIMD)] = Degrain1to5_C<uint8_t, 32, 32, true, 1>;
  func_degrain[make_tuple(32, 16, 1, true, 1, NO_SIMD)] = Degrain1to5_C<uint8_t, 32, 16, true, 1>;
  func_degrain[make_tuple(32, 8 , 1, true, 1, NO_SIMD)] = Degrain1to5_C<uint8_t, 32, 8 , true, 1>;
  func_degrain[make_tuple(16, 32, 1, true, 1, NO_SIMD)] = Degrain1to5_C<uint8_t, 16, 32, true, 1>;
  func_degrain[make_tuple(16, 16, 1, true, 1, NO_SIMD)] = Degrain1to5_C<uint8_t, 16, 16, true, 1>;
  func_degrain[make_tuple(16, 8 , 1, true, 1, NO_SIMD)] = Degrain1to5_C<uint8_t, 16, 8 , true, 1>;
  func_degrain[make_tuple(16, 4 , 1, true, 1, NO_SIMD)] = Degrain1to5_C<uint8_t, 16, 4 , true, 1>;
  func_degrain[make_tuple(16, 2 , 1, true, 1, NO_SIMD)] = Degrain1to5_C<uint8_t, 16, 2 , true, 1>;
  func_degrain[make_tuple(8 , 16, 1, true, 1, NO_SIMD)] = Degrain1to5_C<uint8_t, 8 , 16, true, 1>;
  func_degrain[make_tuple(8 , 8 , 1, true, 1, NO_SIMD)] = Degrain1to5_C<uint8_t, 8 , 8 , true, 1>;
  func_degrain[make_tuple(8 , 4 , 1, true, 1, NO_SIMD)] = Degrain1to5_C<uint8_t, 8 , 4 , true, 1>;
  func_degrain[make_tuple(8 , 2 , 1, true, 1, NO_SIMD)] = Degrain1to5_C<uint8_t, 8 , 2 , true, 1>;
  func_degrain[make_tuple(8 , 1 , 1, true, 1, NO_SIMD)] = Degrain1to5_C<uint8_t, 8 , 1 , true, 1>;
  func_degrain[make_tuple(4 , 8 , 1, true, 1, NO_SIMD)] = Degrain1to5_C<uint8_t, 4 , 8 , true, 1>;
  func_degrain[make_tuple(4 , 4 , 1, true, 1, NO_SIMD)] = Degrain1to5_C<uint8_t, 4 , 4 , true, 1>;
  func_degrain[make_tuple(4 , 2 , 1, true, 1, NO_SIMD)] = Degrain1to5_C<uint8_t, 4 , 2 , true, 1>;
  func_degrain[make_tuple(2 , 4 , 1, true, 1, NO_SIMD)] = Degrain1to5_C<uint8_t, 2 , 4 , true, 1>;
  func_degrain[make_tuple(2 , 2 , 1, true, 1, NO_SIMD)] = Degrain1to5_C<uint8_t, 2 , 2 , true, 1>;
  
  // C, level2, lsb=false
  func_degrain[make_tuple(32, 32, 1, false, 2, NO_SIMD)] = Degrain1to5_C<uint8_t, 32, 32, false, 2>;
  func_degrain[make_tuple(32, 16, 1, false, 2, NO_SIMD)] = Degrain1to5_C<uint8_t, 32, 16, false, 2>;
  func_degrain[make_tuple(32, 8 , 1, false, 2, NO_SIMD)] = Degrain1to5_C<uint8_t, 32, 8 , false, 2>;
  func_degrain[make_tuple(16, 32, 1, false, 2, NO_SIMD)] = Degrain1to5_C<uint8_t, 16, 32, false, 2>;
  func_degrain[make_tuple(16, 16, 1, false, 2, NO_SIMD)] = Degrain1to5_C<uint8_t, 16, 16, false, 2>;
  func_degrain[make_tuple(16, 8 , 1, false, 2, NO_SIMD)] = Degrain1to5_C<uint8_t, 16, 8 , false, 2>;
  func_degrain[make_tuple(16, 4 , 1, false, 2, NO_SIMD)] = Degrain1to5_C<uint8_t, 16, 4 , false, 2>;
  func_degrain[make_tuple(16, 2 , 1, false, 2, NO_SIMD)] = Degrain1to5_C<uint8_t, 16, 2 , false, 2>;
  func_degrain[make_tuple(8 , 16, 1, false, 2, NO_SIMD)] = Degrain1to5_C<uint8_t, 8 , 16, false, 2>;
  func_degrain[make_tuple(8 , 8 , 1, false, 2, NO_SIMD)] = Degrain1to5_C<uint8_t, 8 , 8 , false, 2>;
  func_degrain[make_tuple(8 , 4 , 1, false, 2, NO_SIMD)] = Degrain1to5_C<uint8_t, 8 , 4 , false, 2>;
  func_degrain[make_tuple(8 , 2 , 1, false, 2, NO_SIMD)] = Degrain1to5_C<uint8_t, 8 , 2 , false, 2>;
  func_degrain[make_tuple(8 , 1 , 1, false, 2, NO_SIMD)] = Degrain1to5_C<uint8_t, 8 , 1 , false, 2>;
  func_degrain[make_tuple(4 , 8 , 1, false, 2, NO_SIMD)] = Degrain1to5_C<uint8_t, 4 , 8 , false, 2>;
  func_degrain[make_tuple(4 , 4 , 1, false, 2, NO_SIMD)] = Degrain1to5_C<uint8_t, 4 , 4 , false, 2>;
  func_degrain[make_tuple(4 , 2 , 1, false, 2, NO_SIMD)] = Degrain1to5_C<uint8_t, 4 , 2 , false, 2>;
  func_degrain[make_tuple(2 , 4 , 1, false, 2, NO_SIMD)] = Degrain1to5_C<uint8_t, 2 , 4 , false, 2>;
  func_degrain[make_tuple(2 , 2 , 1, false, 2, NO_SIMD)] = Degrain1to5_C<uint8_t, 2 , 2 , false, 2>;
  // C, level2, lsb=true
  func_degrain[make_tuple(32, 32, 1, true, 2, NO_SIMD)] = Degrain1to5_C<uint8_t, 32, 32, true, 2>;
  func_degrain[make_tuple(32, 16, 1, true, 2, NO_SIMD)] = Degrain1to5_C<uint8_t, 32, 16, true, 2>;
  func_degrain[make_tuple(32, 8 , 1, true, 2, NO_SIMD)] = Degrain1to5_C<uint8_t, 32, 8 , true, 2>;
  func_degrain[make_tuple(16, 32, 1, true, 2, NO_SIMD)] = Degrain1to5_C<uint8_t, 16, 32, true, 2>;
  func_degrain[make_tuple(16, 16, 1, true, 2, NO_SIMD)] = Degrain1to5_C<uint8_t, 16, 16, true, 2>;
  func_degrain[make_tuple(16, 8 , 1, true, 2, NO_SIMD)] = Degrain1to5_C<uint8_t, 16, 8 , true, 2>;
  func_degrain[make_tuple(16, 4 , 1, true, 2, NO_SIMD)] = Degrain1to5_C<uint8_t, 16, 4 , true, 2>;
  func_degrain[make_tuple(16, 2 , 1, true, 2, NO_SIMD)] = Degrain1to5_C<uint8_t, 16, 2 , true, 2>;
  func_degrain[make_tuple(8 , 16, 1, true, 2, NO_SIMD)] = Degrain1to5_C<uint8_t, 8 , 16, true, 2>;
  func_degrain[make_tuple(8 , 8 , 1, true, 2, NO_SIMD)] = Degrain1to5_C<uint8_t, 8 , 8 , true, 2>;
  func_degrain[make_tuple(8 , 4 , 1, true, 2, NO_SIMD)] = Degrain1to5_C<uint8_t, 8 , 4 , true, 2>;
  func_degrain[make_tuple(8 , 2 , 1, true, 2, NO_SIMD)] = Degrain1to5_C<uint8_t, 8 , 2 , true, 2>;
  func_degrain[make_tuple(8 , 1 , 1, true, 2, NO_SIMD)] = Degrain1to5_C<uint8_t, 8 , 1 , true, 2>;
  func_degrain[make_tuple(4 , 8 , 1, true, 2, NO_SIMD)] = Degrain1to5_C<uint8_t, 4 , 8 , true, 2>;
  func_degrain[make_tuple(4 , 4 , 1, true, 2, NO_SIMD)] = Degrain1to5_C<uint8_t, 4 , 4 , true, 2>;
  func_degrain[make_tuple(4 , 2 , 1, true, 2, NO_SIMD)] = Degrain1to5_C<uint8_t, 4 , 2 , true, 2>;
  func_degrain[make_tuple(2 , 4 , 1, true, 2, NO_SIMD)] = Degrain1to5_C<uint8_t, 2 , 4 , true, 2>;
  func_degrain[make_tuple(2 , 2 , 1, true, 2, NO_SIMD)] = Degrain1to5_C<uint8_t, 2 , 2 , true, 2>;

  // C, level3, lsb=false
  func_degrain[make_tuple(32, 32, 1, false, 3, NO_SIMD)] = Degrain1to5_C<uint8_t, 32, 32, false, 3>;
  func_degrain[make_tuple(32, 16, 1, false, 3, NO_SIMD)] = Degrain1to5_C<uint8_t, 32, 16, false, 3>;
  func_degrain[make_tuple(32, 8 , 1, false, 3, NO_SIMD)] = Degrain1to5_C<uint8_t, 32, 8 , false, 3>;
  func_degrain[make_tuple(16, 32, 1, false, 3, NO_SIMD)] = Degrain1to5_C<uint8_t, 16, 32, false, 3>;
  func_degrain[make_tuple(16, 16, 1, false, 3, NO_SIMD)] = Degrain1to5_C<uint8_t, 16, 16, false, 3>;
  func_degrain[make_tuple(16, 8 , 1, false, 3, NO_SIMD)] = Degrain1to5_C<uint8_t, 16, 8 , false, 3>;
  func_degrain[make_tuple(16, 4 , 1, false, 3, NO_SIMD)] = Degrain1to5_C<uint8_t, 16, 4 , false, 3>;
  func_degrain[make_tuple(16, 2 , 1, false, 3, NO_SIMD)] = Degrain1to5_C<uint8_t, 16, 2 , false, 3>;
  func_degrain[make_tuple(8 , 16, 1, false, 3, NO_SIMD)] = Degrain1to5_C<uint8_t, 8 , 16, false, 3>;
  func_degrain[make_tuple(8 , 8 , 1, false, 3, NO_SIMD)] = Degrain1to5_C<uint8_t, 8 , 8 , false, 3>;
  func_degrain[make_tuple(8 , 4 , 1, false, 3, NO_SIMD)] = Degrain1to5_C<uint8_t, 8 , 4 , false, 3>;
  func_degrain[make_tuple(8 , 2 , 1, false, 3, NO_SIMD)] = Degrain1to5_C<uint8_t, 8 , 2 , false, 3>;
  func_degrain[make_tuple(8 , 1 , 1, false, 3, NO_SIMD)] = Degrain1to5_C<uint8_t, 8 , 1 , false, 3>;
  func_degrain[make_tuple(4 , 8 , 1, false, 3, NO_SIMD)] = Degrain1to5_C<uint8_t, 4 , 8 , false, 3>;
  func_degrain[make_tuple(4 , 4 , 1, false, 3, NO_SIMD)] = Degrain1to5_C<uint8_t, 4 , 4 , false, 3>;
  func_degrain[make_tuple(4 , 2 , 1, false, 3, NO_SIMD)] = Degrain1to5_C<uint8_t, 4 , 2 , false, 3>;
  func_degrain[make_tuple(2 , 4 , 1, false, 3, NO_SIMD)] = Degrain1to5_C<uint8_t, 2 , 4 , false, 3>;
  func_degrain[make_tuple(2 , 2 , 1, false, 3, NO_SIMD)] = Degrain1to5_C<uint8_t, 2 , 2 , false, 3>;
  // C, level3, lsb=true
  func_degrain[make_tuple(32, 32, 1, true, 3, NO_SIMD)] = Degrain1to5_C<uint8_t, 32, 32, true, 3>;
  func_degrain[make_tuple(32, 16, 1, true, 3, NO_SIMD)] = Degrain1to5_C<uint8_t, 32, 16, true, 3>;
  func_degrain[make_tuple(32, 8 , 1, true, 3, NO_SIMD)] = Degrain1to5_C<uint8_t, 32, 8 , true, 3>;
  func_degrain[make_tuple(16, 32, 1, true, 3, NO_SIMD)] = Degrain1to5_C<uint8_t, 16, 32, true, 3>;
  func_degrain[make_tuple(16, 16, 1, true, 3, NO_SIMD)] = Degrain1to5_C<uint8_t, 16, 16, true, 3>;
  func_degrain[make_tuple(16, 8 , 1, true, 3, NO_SIMD)] = Degrain1to5_C<uint8_t, 16, 8 , true, 3>;
  func_degrain[make_tuple(16, 4 , 1, true, 3, NO_SIMD)] = Degrain1to5_C<uint8_t, 16, 4 , true, 3>;
  func_degrain[make_tuple(16, 2 , 1, true, 3, NO_SIMD)] = Degrain1to5_C<uint8_t, 16, 2 , true, 3>;
  func_degrain[make_tuple(8 , 16, 1, true, 3, NO_SIMD)] = Degrain1to5_C<uint8_t, 8 , 16, true, 3>;
  func_degrain[make_tuple(8 , 8 , 1, true, 3, NO_SIMD)] = Degrain1to5_C<uint8_t, 8 , 8 , true, 3>;
  func_degrain[make_tuple(8 , 4 , 1, true, 3, NO_SIMD)] = Degrain1to5_C<uint8_t, 8 , 4 , true, 3>;
  func_degrain[make_tuple(8 , 2 , 1, true, 3, NO_SIMD)] = Degrain1to5_C<uint8_t, 8 , 2 , true, 3>;
  func_degrain[make_tuple(8 , 1 , 1, true, 3, NO_SIMD)] = Degrain1to5_C<uint8_t, 8 , 1 , true, 3>;
  func_degrain[make_tuple(4 , 8 , 1, true, 3, NO_SIMD)] = Degrain1to5_C<uint8_t, 4 , 8 , true, 3>;
  func_degrain[make_tuple(4 , 4 , 1, true, 3, NO_SIMD)] = Degrain1to5_C<uint8_t, 4 , 4 , true, 3>;
  func_degrain[make_tuple(4 , 2 , 1, true, 3, NO_SIMD)] = Degrain1to5_C<uint8_t, 4 , 2 , true, 3>;
  func_degrain[make_tuple(2 , 4 , 1, true, 3, NO_SIMD)] = Degrain1to5_C<uint8_t, 2 , 4 , true, 3>;
  func_degrain[make_tuple(2 , 2 , 1, true, 3, NO_SIMD)] = Degrain1to5_C<uint8_t, 2 , 2 , true, 3>;
  // C, level4, lsb=false
  func_degrain[make_tuple(32, 32, 1, false, 4, NO_SIMD)] = Degrain1to5_C<uint8_t, 32, 32, false, 4>;
  func_degrain[make_tuple(32, 16, 1, false, 4, NO_SIMD)] = Degrain1to5_C<uint8_t, 32, 16, false, 4>;
  func_degrain[make_tuple(32, 8 , 1, false, 4, NO_SIMD)] = Degrain1to5_C<uint8_t, 32, 8 , false, 4>;
  func_degrain[make_tuple(16, 32, 1, false, 4, NO_SIMD)] = Degrain1to5_C<uint8_t, 16, 32, false, 4>;
  func_degrain[make_tuple(16, 16, 1, false, 4, NO_SIMD)] = Degrain1to5_C<uint8_t, 16, 16, false, 4>;
  func_degrain[make_tuple(16, 8 , 1, false, 4, NO_SIMD)] = Degrain1to5_C<uint8_t, 16, 8 , false, 4>;
  func_degrain[make_tuple(16, 4 , 1, false, 4, NO_SIMD)] = Degrain1to5_C<uint8_t, 16, 4 , false, 4>;
  func_degrain[make_tuple(16, 2 , 1, false, 4, NO_SIMD)] = Degrain1to5_C<uint8_t, 16, 2 , false, 4>;
  func_degrain[make_tuple(8 , 16, 1, false, 4, NO_SIMD)] = Degrain1to5_C<uint8_t, 8 , 16, false, 4>;
  func_degrain[make_tuple(8 , 8 , 1, false, 4, NO_SIMD)] = Degrain1to5_C<uint8_t, 8 , 8 , false, 4>;
  func_degrain[make_tuple(8 , 4 , 1, false, 4, NO_SIMD)] = Degrain1to5_C<uint8_t, 8 , 4 , false, 4>;
  func_degrain[make_tuple(8 , 2 , 1, false, 4, NO_SIMD)] = Degrain1to5_C<uint8_t, 8 , 2 , false, 4>;
  func_degrain[make_tuple(8 , 1 , 1, false, 4, NO_SIMD)] = Degrain1to5_C<uint8_t, 8 , 1 , false, 4>;
  func_degrain[make_tuple(4 , 8 , 1, false, 4, NO_SIMD)] = Degrain1to5_C<uint8_t, 4 , 8 , false, 4>;
  func_degrain[make_tuple(4 , 4 , 1, false, 4, NO_SIMD)] = Degrain1to5_C<uint8_t, 4 , 4 , false, 4>;
  func_degrain[make_tuple(4 , 2 , 1, false, 4, NO_SIMD)] = Degrain1to5_C<uint8_t, 4 , 2 , false, 4>;
  func_degrain[make_tuple(2 , 4 , 1, false, 4, NO_SIMD)] = Degrain1to5_C<uint8_t, 2 , 4 , false, 4>;
  func_degrain[make_tuple(2 , 2 , 1, false, 4, NO_SIMD)] = Degrain1to5_C<uint8_t, 2 , 2 , false, 4>;
  // C, level4, lsb=true
  func_degrain[make_tuple(32, 32, 1, true, 4, NO_SIMD)] = Degrain1to5_C<uint8_t, 32, 32, true, 4>;
  func_degrain[make_tuple(32, 16, 1, true, 4, NO_SIMD)] = Degrain1to5_C<uint8_t, 32, 16, true, 4>;
  func_degrain[make_tuple(32, 8 , 1, true, 4, NO_SIMD)] = Degrain1to5_C<uint8_t, 32, 8 , true, 4>;
  func_degrain[make_tuple(16, 32, 1, true, 4, NO_SIMD)] = Degrain1to5_C<uint8_t, 16, 32, true, 4>;
  func_degrain[make_tuple(16, 16, 1, true, 4, NO_SIMD)] = Degrain1to5_C<uint8_t, 16, 16, true, 4>;
  func_degrain[make_tuple(16, 8 , 1, true, 4, NO_SIMD)] = Degrain1to5_C<uint8_t, 16, 8 , true, 4>;
  func_degrain[make_tuple(16, 4 , 1, true, 4, NO_SIMD)] = Degrain1to5_C<uint8_t, 16, 4 , true, 4>;
  func_degrain[make_tuple(16, 2 , 1, true, 4, NO_SIMD)] = Degrain1to5_C<uint8_t, 16, 2 , true, 4>;
  func_degrain[make_tuple(8 , 16, 1, true, 4, NO_SIMD)] = Degrain1to5_C<uint8_t, 8 , 16, true, 4>;
  func_degrain[make_tuple(8 , 8 , 1, true, 4, NO_SIMD)] = Degrain1to5_C<uint8_t, 8 , 8 , true, 4>;
  func_degrain[make_tuple(8 , 4 , 1, true, 4, NO_SIMD)] = Degrain1to5_C<uint8_t, 8 , 4 , true, 4>;
  func_degrain[make_tuple(8 , 2 , 1, true, 4, NO_SIMD)] = Degrain1to5_C<uint8_t, 8 , 2 , true, 4>;
  func_degrain[make_tuple(8 , 1 , 1, true, 4, NO_SIMD)] = Degrain1to5_C<uint8_t, 8 , 1 , true, 4>;
  func_degrain[make_tuple(4 , 8 , 1, true, 4, NO_SIMD)] = Degrain1to5_C<uint8_t, 4 , 8 , true, 4>;
  func_degrain[make_tuple(4 , 4 , 1, true, 4, NO_SIMD)] = Degrain1to5_C<uint8_t, 4 , 4 , true, 4>;
  func_degrain[make_tuple(4 , 2 , 1, true, 4, NO_SIMD)] = Degrain1to5_C<uint8_t, 4 , 2 , true, 4>;
  func_degrain[make_tuple(2 , 4 , 1, true, 4, NO_SIMD)] = Degrain1to5_C<uint8_t, 2 , 4 , true, 4>;
  func_degrain[make_tuple(2 , 2 , 1, true, 4, NO_SIMD)] = Degrain1to5_C<uint8_t, 2 , 2 , true, 4>;

  // C, level5, lsb=false
  func_degrain[make_tuple(32, 32, 1, false, 5, NO_SIMD)] = Degrain1to5_C<uint8_t, 32, 32, false, 5>;
  func_degrain[make_tuple(32, 16, 1, false, 5, NO_SIMD)] = Degrain1to5_C<uint8_t, 32, 16, false, 5>;
  func_degrain[make_tuple(32, 8 , 1, false, 5, NO_SIMD)] = Degrain1to5_C<uint8_t, 32, 8 , false, 5>;
  func_degrain[make_tuple(16, 32, 1, false, 5, NO_SIMD)] = Degrain1to5_C<uint8_t, 16, 32, false, 5>;
  func_degrain[make_tuple(16, 16, 1, false, 5, NO_SIMD)] = Degrain1to5_C<uint8_t, 16, 16, false, 5>;
  func_degrain[make_tuple(16, 8 , 1, false, 5, NO_SIMD)] = Degrain1to5_C<uint8_t, 16, 8 , false, 5>;
  func_degrain[make_tuple(16, 4 , 1, false, 5, NO_SIMD)] = Degrain1to5_C<uint8_t, 16, 4 , false, 5>;
  func_degrain[make_tuple(16, 2 , 1, false, 5, NO_SIMD)] = Degrain1to5_C<uint8_t, 16, 2 , false, 5>;
  func_degrain[make_tuple(8 , 16, 1, false, 5, NO_SIMD)] = Degrain1to5_C<uint8_t, 8 , 16, false, 5>;
  func_degrain[make_tuple(8 , 8 , 1, false, 5, NO_SIMD)] = Degrain1to5_C<uint8_t, 8 , 8 , false, 5>;
  func_degrain[make_tuple(8 , 4 , 1, false, 5, NO_SIMD)] = Degrain1to5_C<uint8_t, 8 , 4 , false, 5>;
  func_degrain[make_tuple(8 , 2 , 1, false, 5, NO_SIMD)] = Degrain1to5_C<uint8_t, 8 , 2 , false, 5>;
  func_degrain[make_tuple(8 , 1 , 1, false, 5, NO_SIMD)] = Degrain1to5_C<uint8_t, 8 , 1 , false, 5>;
  func_degrain[make_tuple(4 , 8 , 1, false, 5, NO_SIMD)] = Degrain1to5_C<uint8_t, 4 , 8 , false, 5>;
  func_degrain[make_tuple(4 , 4 , 1, false, 5, NO_SIMD)] = Degrain1to5_C<uint8_t, 4 , 4 , false, 5>;
  func_degrain[make_tuple(4 , 2 , 1, false, 5, NO_SIMD)] = Degrain1to5_C<uint8_t, 4 , 2 , false, 5>;
  func_degrain[make_tuple(2 , 4 , 1, false, 5, NO_SIMD)] = Degrain1to5_C<uint8_t, 2 , 4 , false, 5>;
  func_degrain[make_tuple(2 , 2 , 1, false, 5, NO_SIMD)] = Degrain1to5_C<uint8_t, 2 , 2 , false, 5>;
  // C, level5, lsb=true
  func_degrain[make_tuple(32, 32, 1, true, 5, NO_SIMD)] = Degrain1to5_C<uint8_t, 32, 32, true, 5>;
  func_degrain[make_tuple(32, 16, 1, true, 5, NO_SIMD)] = Degrain1to5_C<uint8_t, 32, 16, true, 5>;
  func_degrain[make_tuple(32, 8 , 1, true, 5, NO_SIMD)] = Degrain1to5_C<uint8_t, 32, 8 , true, 5>;
  func_degrain[make_tuple(16, 32, 1, true, 5, NO_SIMD)] = Degrain1to5_C<uint8_t, 16, 32, true, 5>;
  func_degrain[make_tuple(16, 16, 1, true, 5, NO_SIMD)] = Degrain1to5_C<uint8_t, 16, 16, true, 5>;
  func_degrain[make_tuple(16, 8 , 1, true, 5, NO_SIMD)] = Degrain1to5_C<uint8_t, 16, 8 , true, 5>;
  func_degrain[make_tuple(16, 4 , 1, true, 5, NO_SIMD)] = Degrain1to5_C<uint8_t, 16, 4 , true, 5>;
  func_degrain[make_tuple(16, 2 , 1, true, 5, NO_SIMD)] = Degrain1to5_C<uint8_t, 16, 2 , true, 5>;
  func_degrain[make_tuple(8 , 16, 1, true, 5, NO_SIMD)] = Degrain1to5_C<uint8_t, 8 , 16, true, 5>;
  func_degrain[make_tuple(8 , 8 , 1, true, 5, NO_SIMD)] = Degrain1to5_C<uint8_t, 8 , 8 , true, 5>;
  func_degrain[make_tuple(8 , 4 , 1, true, 5, NO_SIMD)] = Degrain1to5_C<uint8_t, 8 , 4 , true, 5>;
  func_degrain[make_tuple(8 , 2 , 1, true, 5, NO_SIMD)] = Degrain1to5_C<uint8_t, 8 , 2 , true, 5>;
  func_degrain[make_tuple(8 , 1 , 1, true, 5, NO_SIMD)] = Degrain1to5_C<uint8_t, 8 , 1 , true, 5>;
  func_degrain[make_tuple(4 , 8 , 1, true, 5, NO_SIMD)] = Degrain1to5_C<uint8_t, 4 , 8 , true, 5>;
  func_degrain[make_tuple(4 , 4 , 1, true, 5, NO_SIMD)] = Degrain1to5_C<uint8_t, 4 , 4 , true, 5>;
  func_degrain[make_tuple(4 , 2 , 1, true, 5, NO_SIMD)] = Degrain1to5_C<uint8_t, 4 , 2 , true, 5>;
  func_degrain[make_tuple(2 , 4 , 1, true, 5, NO_SIMD)] = Degrain1to5_C<uint8_t, 2 , 4 , true, 5>;
  func_degrain[make_tuple(2 , 2 , 1, true, 5, NO_SIMD)] = Degrain1to5_C<uint8_t, 2 , 2 , true, 5>;

// 16 bit
// C, level1, lsb=false
  func_degrain[make_tuple(32, 32, 2, false, 1, NO_SIMD)] = Degrain1to5_C<uint16_t, 32, 32, false, 1>;
  func_degrain[make_tuple(32, 16, 2, false, 1, NO_SIMD)] = Degrain1to5_C<uint16_t, 32, 16, false, 1>;
  func_degrain[make_tuple(32, 8 , 2, false, 1, NO_SIMD)] = Degrain1to5_C<uint16_t, 32, 8 , false, 1>;
  func_degrain[make_tuple(16, 32, 2, false, 1, NO_SIMD)] = Degrain1to5_C<uint16_t, 16, 32, false, 1>;
  func_degrain[make_tuple(16, 16, 2, false, 1, NO_SIMD)] = Degrain1to5_C<uint16_t, 16, 16, false, 1>;
  func_degrain[make_tuple(16, 8 , 2, false, 1, NO_SIMD)] = Degrain1to5_C<uint16_t, 16, 8 , false, 1>;
  func_degrain[make_tuple(16, 4 , 2, false, 1, NO_SIMD)] = Degrain1to5_C<uint16_t, 16, 4 , false, 1>;
  func_degrain[make_tuple(16, 2 , 2, false, 1, NO_SIMD)] = Degrain1to5_C<uint16_t, 16, 2 , false, 1>;
  func_degrain[make_tuple(8 , 16, 2, false, 1, NO_SIMD)] = Degrain1to5_C<uint16_t, 8 , 16, false, 1>;
  func_degrain[make_tuple(8 , 8 , 2, false, 1, NO_SIMD)] = Degrain1to5_C<uint16_t, 8 , 8 , false, 1>;
  func_degrain[make_tuple(8 , 4 , 2, false, 1, NO_SIMD)] = Degrain1to5_C<uint16_t, 8 , 4 , false, 1>;
  func_degrain[make_tuple(8 , 2 , 2, false, 1, NO_SIMD)] = Degrain1to5_C<uint16_t, 8 , 2 , false, 1>;
  func_degrain[make_tuple(8 , 1 , 2, false, 1, NO_SIMD)] = Degrain1to5_C<uint16_t, 8 , 1 , false, 1>;
  func_degrain[make_tuple(4 , 8 , 2, false, 1, NO_SIMD)] = Degrain1to5_C<uint16_t, 4 , 8 , false, 1>;
  func_degrain[make_tuple(4 , 4 , 2, false, 1, NO_SIMD)] = Degrain1to5_C<uint16_t, 4 , 4 , false, 1>;
  func_degrain[make_tuple(4 , 2 , 2, false, 1, NO_SIMD)] = Degrain1to5_C<uint16_t, 4 , 2 , false, 1>;
  func_degrain[make_tuple(2 , 4 , 2, false, 1, NO_SIMD)] = Degrain1to5_C<uint16_t, 2 , 4 , false, 1>;
  func_degrain[make_tuple(2 , 2 , 2, false, 1, NO_SIMD)] = Degrain1to5_C<uint16_t, 2 , 2 , false, 1>;
#if 0
  // NO lsb + 16 bit 
  // C, level1, lsb=true
  func_degrain[make_tuple(32, 32, 2, true, 1, NO_SIMD)] = Degrain1to5_C<uint16_t, 32, 32, true, 1>;
  func_degrain[make_tuple(32, 16, 2, true, 1, NO_SIMD)] = Degrain1to5_C<uint16_t, 32, 16, true, 1>;
  func_degrain[make_tuple(32, 8 , 2, true, 1, NO_SIMD)] = Degrain1to5_C<uint16_t, 32, 8 , true, 1>;
  func_degrain[make_tuple(16, 32, 2, true, 1, NO_SIMD)] = Degrain1to5_C<uint16_t, 16, 32, true, 1>;
  func_degrain[make_tuple(16, 16, 2, true, 1, NO_SIMD)] = Degrain1to5_C<uint16_t, 16, 16, true, 1>;
  func_degrain[make_tuple(16, 8 , 2, true, 1, NO_SIMD)] = Degrain1to5_C<uint16_t, 16, 8 , true, 1>;
  func_degrain[make_tuple(16, 4 , 2, true, 1, NO_SIMD)] = Degrain1to5_C<uint16_t, 16, 4 , true, 1>;
  func_degrain[make_tuple(16, 2 , 2, true, 1, NO_SIMD)] = Degrain1to5_C<uint16_t, 16, 2 , true, 1>;
  func_degrain[make_tuple(8 , 16, 2, true, 1, NO_SIMD)] = Degrain1to5_C<uint16_t, 8 , 16, true, 1>;
  func_degrain[make_tuple(8 , 8 , 2, true, 1, NO_SIMD)] = Degrain1to5_C<uint16_t, 8 , 8 , true, 1>;
  func_degrain[make_tuple(8 , 4 , 2, true, 1, NO_SIMD)] = Degrain1to5_C<uint16_t, 8 , 4 , true, 1>;
  func_degrain[make_tuple(8 , 2 , 2, true, 1, NO_SIMD)] = Degrain1to5_C<uint16_t, 8 , 2 , true, 1>;
  func_degrain[make_tuple(8 , 1 , 2, true, 1, NO_SIMD)] = Degrain1to5_C<uint16_t, 8 , 1 , true, 1>;
  func_degrain[make_tuple(4 , 8 , 2, true, 1, NO_SIMD)] = Degrain1to5_C<uint16_t, 4 , 8 , true, 1>;
  func_degrain[make_tuple(4 , 4 , 2, true, 1, NO_SIMD)] = Degrain1to5_C<uint16_t, 4 , 4 , true, 1>;
  func_degrain[make_tuple(4 , 2 , 2, true, 1, NO_SIMD)] = Degrain1to5_C<uint16_t, 4 , 2 , true, 1>;
  func_degrain[make_tuple(2 , 4 , 2, true, 1, NO_SIMD)] = Degrain1to5_C<uint16_t, 2 , 4 , true, 1>;
  func_degrain[make_tuple(2 , 2 , 2, true, 1, NO_SIMD)] = Degrain1to5_C<uint16_t, 2 , 2 , true, 1>;
#endif
  // C, level2, lsb=false
  func_degrain[make_tuple(32, 32, 2, false, 2, NO_SIMD)] = Degrain1to5_C<uint16_t, 32, 32, false, 2>;
  func_degrain[make_tuple(32, 16, 2, false, 2, NO_SIMD)] = Degrain1to5_C<uint16_t, 32, 16, false, 2>;
  func_degrain[make_tuple(32, 8 , 2, false, 2, NO_SIMD)] = Degrain1to5_C<uint16_t, 32, 8 , false, 2>;
  func_degrain[make_tuple(16, 32, 2, false, 2, NO_SIMD)] = Degrain1to5_C<uint16_t, 16, 32, false, 2>;
  func_degrain[make_tuple(16, 16, 2, false, 2, NO_SIMD)] = Degrain1to5_C<uint16_t, 16, 16, false, 2>;
  func_degrain[make_tuple(16, 8 , 2, false, 2, NO_SIMD)] = Degrain1to5_C<uint16_t, 16, 8 , false, 2>;
  func_degrain[make_tuple(16, 4 , 2, false, 2, NO_SIMD)] = Degrain1to5_C<uint16_t, 16, 4 , false, 2>;
  func_degrain[make_tuple(16, 2 , 2, false, 2, NO_SIMD)] = Degrain1to5_C<uint16_t, 16, 2 , false, 2>;
  func_degrain[make_tuple(8 , 16, 2, false, 2, NO_SIMD)] = Degrain1to5_C<uint16_t, 8 , 16, false, 2>;
  func_degrain[make_tuple(8 , 8 , 2, false, 2, NO_SIMD)] = Degrain1to5_C<uint16_t, 8 , 8 , false, 2>;
  func_degrain[make_tuple(8 , 4 , 2, false, 2, NO_SIMD)] = Degrain1to5_C<uint16_t, 8 , 4 , false, 2>;
  func_degrain[make_tuple(8 , 2 , 2, false, 2, NO_SIMD)] = Degrain1to5_C<uint16_t, 8 , 2 , false, 2>;
  func_degrain[make_tuple(8 , 1 , 2, false, 2, NO_SIMD)] = Degrain1to5_C<uint16_t, 8 , 1 , false, 2>;
  func_degrain[make_tuple(4 , 8 , 2, false, 2, NO_SIMD)] = Degrain1to5_C<uint16_t, 4 , 8 , false, 2>;
  func_degrain[make_tuple(4 , 4 , 2, false, 2, NO_SIMD)] = Degrain1to5_C<uint16_t, 4 , 4 , false, 2>;
  func_degrain[make_tuple(4 , 2 , 2, false, 2, NO_SIMD)] = Degrain1to5_C<uint16_t, 4 , 2 , false, 2>;
  func_degrain[make_tuple(2 , 4 , 2, false, 2, NO_SIMD)] = Degrain1to5_C<uint16_t, 2 , 4 , false, 2>;
  func_degrain[make_tuple(2 , 2 , 2, false, 2, NO_SIMD)] = Degrain1to5_C<uint16_t, 2 , 2 , false, 2>;
  // C, level2, lsb=true
#if 0
  func_degrain[make_tuple(32, 32, 2, true, 2, NO_SIMD)] = Degrain1to5_C<uint16_t, 32, 32, true, 2>;
  func_degrain[make_tuple(32, 16, 2, true, 2, NO_SIMD)] = Degrain1to5_C<uint16_t, 32, 16, true, 2>;
  func_degrain[make_tuple(32, 8 , 2, true, 2, NO_SIMD)] = Degrain1to5_C<uint16_t, 32, 8 , true, 2>;
  func_degrain[make_tuple(16, 32, 2, true, 2, NO_SIMD)] = Degrain1to5_C<uint16_t, 16, 32, true, 2>;
  func_degrain[make_tuple(16, 16, 2, true, 2, NO_SIMD)] = Degrain1to5_C<uint16_t, 16, 16, true, 2>;
  func_degrain[make_tuple(16, 8 , 2, true, 2, NO_SIMD)] = Degrain1to5_C<uint16_t, 16, 8 , true, 2>;
  func_degrain[make_tuple(16, 4 , 2, true, 2, NO_SIMD)] = Degrain1to5_C<uint16_t, 16, 4 , true, 2>;
  func_degrain[make_tuple(16, 2 , 2, true, 2, NO_SIMD)] = Degrain1to5_C<uint16_t, 16, 2 , true, 2>;
  func_degrain[make_tuple(8 , 16, 2, true, 2, NO_SIMD)] = Degrain1to5_C<uint16_t, 8 , 16, true, 2>;
  func_degrain[make_tuple(8 , 8 , 2, true, 2, NO_SIMD)] = Degrain1to5_C<uint16_t, 8 , 8 , true, 2>;
  func_degrain[make_tuple(8 , 4 , 2, true, 2, NO_SIMD)] = Degrain1to5_C<uint16_t, 8 , 4 , true, 2>;
  func_degrain[make_tuple(8 , 2 , 2, true, 2, NO_SIMD)] = Degrain1to5_C<uint16_t, 8 , 2 , true, 2>;
  func_degrain[make_tuple(8 , 1 , 2, true, 2, NO_SIMD)] = Degrain1to5_C<uint16_t, 8 , 1 , true, 2>;
  func_degrain[make_tuple(4 , 8 , 2, true, 2, NO_SIMD)] = Degrain1to5_C<uint16_t, 4 , 8 , true, 2>;
  func_degrain[make_tuple(4 , 4 , 2, true, 2, NO_SIMD)] = Degrain1to5_C<uint16_t, 4 , 4 , true, 2>;
  func_degrain[make_tuple(4 , 2 , 2, true, 2, NO_SIMD)] = Degrain1to5_C<uint16_t, 4 , 2 , true, 2>;
  func_degrain[make_tuple(2 , 4 , 2, true, 2, NO_SIMD)] = Degrain1to5_C<uint16_t, 2 , 4 , true, 2>;
  func_degrain[make_tuple(2 , 2 , 2, true, 2, NO_SIMD)] = Degrain1to5_C<uint16_t, 2 , 2 , true, 2>;
#endif
  // C, level3, lsb=false
  func_degrain[make_tuple(32, 32, 2, false, 3, NO_SIMD)] = Degrain1to5_C<uint16_t, 32, 32, false, 3>;
  func_degrain[make_tuple(32, 16, 2, false, 3, NO_SIMD)] = Degrain1to5_C<uint16_t, 32, 16, false, 3>;
  func_degrain[make_tuple(32, 8 , 2, false, 3, NO_SIMD)] = Degrain1to5_C<uint16_t, 32, 8 , false, 3>;
  func_degrain[make_tuple(16, 32, 2, false, 3, NO_SIMD)] = Degrain1to5_C<uint16_t, 16, 32, false, 3>;
  func_degrain[make_tuple(16, 16, 2, false, 3, NO_SIMD)] = Degrain1to5_C<uint16_t, 16, 16, false, 3>;
  func_degrain[make_tuple(16, 8 , 2, false, 3, NO_SIMD)] = Degrain1to5_C<uint16_t, 16, 8 , false, 3>;
  func_degrain[make_tuple(16, 4 , 2, false, 3, NO_SIMD)] = Degrain1to5_C<uint16_t, 16, 4 , false, 3>;
  func_degrain[make_tuple(16, 2 , 2, false, 3, NO_SIMD)] = Degrain1to5_C<uint16_t, 16, 2 , false, 3>;
  func_degrain[make_tuple(8 , 16, 2, false, 3, NO_SIMD)] = Degrain1to5_C<uint16_t, 8 , 16, false, 3>;
  func_degrain[make_tuple(8 , 8 , 2, false, 3, NO_SIMD)] = Degrain1to5_C<uint16_t, 8 , 8 , false, 3>;
  func_degrain[make_tuple(8 , 4 , 2, false, 3, NO_SIMD)] = Degrain1to5_C<uint16_t, 8 , 4 , false, 3>;
  func_degrain[make_tuple(8 , 2 , 2, false, 3, NO_SIMD)] = Degrain1to5_C<uint16_t, 8 , 2 , false, 3>;
  func_degrain[make_tuple(8 , 1 , 2, false, 3, NO_SIMD)] = Degrain1to5_C<uint16_t, 8 , 1 , false, 3>;
  func_degrain[make_tuple(4 , 8 , 2, false, 3, NO_SIMD)] = Degrain1to5_C<uint16_t, 4 , 8 , false, 3>;
  func_degrain[make_tuple(4 , 4 , 2, false, 3, NO_SIMD)] = Degrain1to5_C<uint16_t, 4 , 4 , false, 3>;
  func_degrain[make_tuple(4 , 2 , 2, false, 3, NO_SIMD)] = Degrain1to5_C<uint16_t, 4 , 2 , false, 3>;
  func_degrain[make_tuple(2 , 4 , 2, false, 3, NO_SIMD)] = Degrain1to5_C<uint16_t, 2 , 4 , false, 3>;
  func_degrain[make_tuple(2 , 2 , 2, false, 3, NO_SIMD)] = Degrain1to5_C<uint16_t, 2 , 2 , false, 3>;
  // C, level3, lsb=true
#if 0
  func_degrain[make_tuple(32, 32, 2, true, 3, NO_SIMD)] = Degrain1to5_C<uint16_t, 32, 32, true, 3>;
  func_degrain[make_tuple(32, 16, 2, true, 3, NO_SIMD)] = Degrain1to5_C<uint16_t, 32, 16, true, 3>;
  func_degrain[make_tuple(32, 8 , 2, true, 3, NO_SIMD)] = Degrain1to5_C<uint16_t, 32, 8 , true, 3>;
  func_degrain[make_tuple(16, 32, 2, true, 3, NO_SIMD)] = Degrain1to5_C<uint16_t, 16, 32, true, 3>;
  func_degrain[make_tuple(16, 16, 2, true, 3, NO_SIMD)] = Degrain1to5_C<uint16_t, 16, 16, true, 3>;
  func_degrain[make_tuple(16, 8 , 2, true, 3, NO_SIMD)] = Degrain1to5_C<uint16_t, 16, 8 , true, 3>;
  func_degrain[make_tuple(16, 4 , 2, true, 3, NO_SIMD)] = Degrain1to5_C<uint16_t, 16, 4 , true, 3>;
  func_degrain[make_tuple(16, 2 , 2, true, 3, NO_SIMD)] = Degrain1to5_C<uint16_t, 16, 2 , true, 3>;
  func_degrain[make_tuple(8 , 16, 2, true, 3, NO_SIMD)] = Degrain1to5_C<uint16_t, 8 , 16, true, 3>;
  func_degrain[make_tuple(8 , 8 , 2, true, 3, NO_SIMD)] = Degrain1to5_C<uint16_t, 8 , 8 , true, 3>;
  func_degrain[make_tuple(8 , 4 , 2, true, 3, NO_SIMD)] = Degrain1to5_C<uint16_t, 8 , 4 , true, 3>;
  func_degrain[make_tuple(8 , 2 , 2, true, 3, NO_SIMD)] = Degrain1to5_C<uint16_t, 8 , 2 , true, 3>;
  func_degrain[make_tuple(8 , 1 , 2, true, 3, NO_SIMD)] = Degrain1to5_C<uint16_t, 8 , 1 , true, 3>;
  func_degrain[make_tuple(4 , 8 , 2, true, 3, NO_SIMD)] = Degrain1to5_C<uint16_t, 4 , 8 , true, 3>;
  func_degrain[make_tuple(4 , 4 , 2, true, 3, NO_SIMD)] = Degrain1to5_C<uint16_t, 4 , 4 , true, 3>;
  func_degrain[make_tuple(4 , 2 , 2, true, 3, NO_SIMD)] = Degrain1to5_C<uint16_t, 4 , 2 , true, 3>;
  func_degrain[make_tuple(2 , 4 , 2, true, 3, NO_SIMD)] = Degrain1to5_C<uint16_t, 2 , 4 , true, 3>;
  func_degrain[make_tuple(2 , 2 , 2, true, 3, NO_SIMD)] = Degrain1to5_C<uint16_t, 2 , 2 , true, 3>;
#endif
  // C, level4, lsb=false
  func_degrain[make_tuple(32, 32, 2, false, 4, NO_SIMD)] = Degrain1to5_C<uint16_t, 32, 32, false, 4>;
  func_degrain[make_tuple(32, 16, 2, false, 4, NO_SIMD)] = Degrain1to5_C<uint16_t, 32, 16, false, 4>;
  func_degrain[make_tuple(32, 8 , 2, false, 4, NO_SIMD)] = Degrain1to5_C<uint16_t, 32, 8 , false, 4>;
  func_degrain[make_tuple(16, 32, 2, false, 4, NO_SIMD)] = Degrain1to5_C<uint16_t, 16, 32, false, 4>;
  func_degrain[make_tuple(16, 16, 2, false, 4, NO_SIMD)] = Degrain1to5_C<uint16_t, 16, 16, false, 4>;
  func_degrain[make_tuple(16, 8 , 2, false, 4, NO_SIMD)] = Degrain1to5_C<uint16_t, 16, 8 , false, 4>;
  func_degrain[make_tuple(16, 4 , 2, false, 4, NO_SIMD)] = Degrain1to5_C<uint16_t, 16, 4 , false, 4>;
  func_degrain[make_tuple(16, 2 , 2, false, 4, NO_SIMD)] = Degrain1to5_C<uint16_t, 16, 2 , false, 4>;
  func_degrain[make_tuple(8 , 16, 2, false, 4, NO_SIMD)] = Degrain1to5_C<uint16_t, 8 , 16, false, 4>;
  func_degrain[make_tuple(8 , 8 , 2, false, 4, NO_SIMD)] = Degrain1to5_C<uint16_t, 8 , 8 , false, 4>;
  func_degrain[make_tuple(8 , 4 , 2, false, 4, NO_SIMD)] = Degrain1to5_C<uint16_t, 8 , 4 , false, 4>;
  func_degrain[make_tuple(8 , 2 , 2, false, 4, NO_SIMD)] = Degrain1to5_C<uint16_t, 8 , 2 , false, 4>;
  func_degrain[make_tuple(8 , 1 , 2, false, 4, NO_SIMD)] = Degrain1to5_C<uint16_t, 8 , 1 , false, 4>;
  func_degrain[make_tuple(4 , 8 , 2, false, 4, NO_SIMD)] = Degrain1to5_C<uint16_t, 4 , 8 , false, 4>;
  func_degrain[make_tuple(4 , 4 , 2, false, 4, NO_SIMD)] = Degrain1to5_C<uint16_t, 4 , 4 , false, 4>;
  func_degrain[make_tuple(4 , 2 , 2, false, 4, NO_SIMD)] = Degrain1to5_C<uint16_t, 4 , 2 , false, 4>;
  func_degrain[make_tuple(2 , 4 , 2, false, 4, NO_SIMD)] = Degrain1to5_C<uint16_t, 2 , 4 , false, 4>;
  func_degrain[make_tuple(2 , 2 , 2, false, 4, NO_SIMD)] = Degrain1to5_C<uint16_t, 2 , 2 , false, 4>;
  // C, level4, lsb=true
#if 0
  func_degrain[make_tuple(32, 32, 2, true, 4, NO_SIMD)] = Degrain1to5_C<uint16_t, 32, 32, true, 4>;
  func_degrain[make_tuple(32, 16, 2, true, 4, NO_SIMD)] = Degrain1to5_C<uint16_t, 32, 16, true, 4>;
  func_degrain[make_tuple(32, 8 , 2, true, 4, NO_SIMD)] = Degrain1to5_C<uint16_t, 32, 8 , true, 4>;
  func_degrain[make_tuple(16, 32, 2, true, 4, NO_SIMD)] = Degrain1to5_C<uint16_t, 16, 32, true, 4>;
  func_degrain[make_tuple(16, 16, 2, true, 4, NO_SIMD)] = Degrain1to5_C<uint16_t, 16, 16, true, 4>;
  func_degrain[make_tuple(16, 8 , 2, true, 4, NO_SIMD)] = Degrain1to5_C<uint16_t, 16, 8 , true, 4>;
  func_degrain[make_tuple(16, 4 , 2, true, 4, NO_SIMD)] = Degrain1to5_C<uint16_t, 16, 4 , true, 4>;
  func_degrain[make_tuple(16, 2 , 2, true, 4, NO_SIMD)] = Degrain1to5_C<uint16_t, 16, 2 , true, 4>;
  func_degrain[make_tuple(8 , 16, 2, true, 4, NO_SIMD)] = Degrain1to5_C<uint16_t, 8 , 16, true, 4>;
  func_degrain[make_tuple(8 , 8 , 2, true, 4, NO_SIMD)] = Degrain1to5_C<uint16_t, 8 , 8 , true, 4>;
  func_degrain[make_tuple(8 , 4 , 2, true, 4, NO_SIMD)] = Degrain1to5_C<uint16_t, 8 , 4 , true, 4>;
  func_degrain[make_tuple(8 , 2 , 2, true, 4, NO_SIMD)] = Degrain1to5_C<uint16_t, 8 , 2 , true, 4>;
  func_degrain[make_tuple(8 , 1 , 2, true, 4, NO_SIMD)] = Degrain1to5_C<uint16_t, 8 , 1 , true, 4>;
  func_degrain[make_tuple(4 , 8 , 2, true, 4, NO_SIMD)] = Degrain1to5_C<uint16_t, 4 , 8 , true, 4>;
  func_degrain[make_tuple(4 , 4 , 2, true, 4, NO_SIMD)] = Degrain1to5_C<uint16_t, 4 , 4 , true, 4>;
  func_degrain[make_tuple(4 , 2 , 2, true, 4, NO_SIMD)] = Degrain1to5_C<uint16_t, 4 , 2 , true, 4>;
  func_degrain[make_tuple(2 , 4 , 2, true, 4, NO_SIMD)] = Degrain1to5_C<uint16_t, 2 , 4 , true, 4>;
  func_degrain[make_tuple(2 , 2 , 2, true, 4, NO_SIMD)] = Degrain1to5_C<uint16_t, 2 , 2 , true, 4>;
#endif
  // C, level5, lsb=false
  func_degrain[make_tuple(32, 32, 2, false, 5, NO_SIMD)] = Degrain1to5_C<uint16_t, 32, 32, false, 5>;
  func_degrain[make_tuple(32, 16, 2, false, 5, NO_SIMD)] = Degrain1to5_C<uint16_t, 32, 16, false, 5>;
  func_degrain[make_tuple(32, 8 , 2, false, 5, NO_SIMD)] = Degrain1to5_C<uint16_t, 32, 8 , false, 5>;
  func_degrain[make_tuple(16, 32, 2, false, 5, NO_SIMD)] = Degrain1to5_C<uint16_t, 16, 32, false, 5>;
  func_degrain[make_tuple(16, 16, 2, false, 5, NO_SIMD)] = Degrain1to5_C<uint16_t, 16, 16, false, 5>;
  func_degrain[make_tuple(16, 8 , 2, false, 5, NO_SIMD)] = Degrain1to5_C<uint16_t, 16, 8 , false, 5>;
  func_degrain[make_tuple(16, 4 , 2, false, 5, NO_SIMD)] = Degrain1to5_C<uint16_t, 16, 4 , false, 5>;
  func_degrain[make_tuple(16, 2 , 2, false, 5, NO_SIMD)] = Degrain1to5_C<uint16_t, 16, 2 , false, 5>;
  func_degrain[make_tuple(8 , 16, 2, false, 5, NO_SIMD)] = Degrain1to5_C<uint16_t, 8 , 16, false, 5>;
  func_degrain[make_tuple(8 , 8 , 2, false, 5, NO_SIMD)] = Degrain1to5_C<uint16_t, 8 , 8 , false, 5>;
  func_degrain[make_tuple(8 , 4 , 2, false, 5, NO_SIMD)] = Degrain1to5_C<uint16_t, 8 , 4 , false, 5>;
  func_degrain[make_tuple(8 , 2 , 2, false, 5, NO_SIMD)] = Degrain1to5_C<uint16_t, 8 , 2 , false, 5>;
  func_degrain[make_tuple(8 , 1 , 2, false, 5, NO_SIMD)] = Degrain1to5_C<uint16_t, 8 , 1 , false, 5>;
  func_degrain[make_tuple(4 , 8 , 2, false, 5, NO_SIMD)] = Degrain1to5_C<uint16_t, 4 , 8 , false, 5>;
  func_degrain[make_tuple(4 , 4 , 2, false, 5, NO_SIMD)] = Degrain1to5_C<uint16_t, 4 , 4 , false, 5>;
  func_degrain[make_tuple(4 , 2 , 2, false, 5, NO_SIMD)] = Degrain1to5_C<uint16_t, 4 , 2 , false, 5>;
  func_degrain[make_tuple(2 , 4 , 2, false, 5, NO_SIMD)] = Degrain1to5_C<uint16_t, 2 , 4 , false, 5>;
  func_degrain[make_tuple(2 , 2 , 2, false, 5, NO_SIMD)] = Degrain1to5_C<uint16_t, 2 , 2 , false, 5>;
  // C, level5, lsb=true
#if 0
  func_degrain[make_tuple(32, 32, 2, true, 5, NO_SIMD)] = Degrain1to5_C<uint16_t, 32, 32, true, 5>;
  func_degrain[make_tuple(32, 16, 2, true, 5, NO_SIMD)] = Degrain1to5_C<uint16_t, 32, 16, true, 5>;
  func_degrain[make_tuple(32, 8 , 2, true, 5, NO_SIMD)] = Degrain1to5_C<uint16_t, 32, 8 , true, 5>;
  func_degrain[make_tuple(16, 32, 2, true, 5, NO_SIMD)] = Degrain1to5_C<uint16_t, 16, 32, true, 5>;
  func_degrain[make_tuple(16, 16, 2, true, 5, NO_SIMD)] = Degrain1to5_C<uint16_t, 16, 16, true, 5>;
  func_degrain[make_tuple(16, 8 , 2, true, 5, NO_SIMD)] = Degrain1to5_C<uint16_t, 16, 8 , true, 5>;
  func_degrain[make_tuple(16, 4 , 2, true, 5, NO_SIMD)] = Degrain1to5_C<uint16_t, 16, 4 , true, 5>;
  func_degrain[make_tuple(16, 2 , 2, true, 5, NO_SIMD)] = Degrain1to5_C<uint16_t, 16, 2 , true, 5>;
  func_degrain[make_tuple(8 , 16, 2, true, 5, NO_SIMD)] = Degrain1to5_C<uint16_t, 8 , 16, true, 5>;
  func_degrain[make_tuple(8 , 8 , 2, true, 5, NO_SIMD)] = Degrain1to5_C<uint16_t, 8 , 8 , true, 5>;
  func_degrain[make_tuple(8 , 4 , 2, true, 5, NO_SIMD)] = Degrain1to5_C<uint16_t, 8 , 4 , true, 5>;
  func_degrain[make_tuple(8 , 2 , 2, true, 5, NO_SIMD)] = Degrain1to5_C<uint16_t, 8 , 2 , true, 5>;
  func_degrain[make_tuple(8 , 1 , 2, true, 5, NO_SIMD)] = Degrain1to5_C<uint16_t, 8 , 1 , true, 5>;
  func_degrain[make_tuple(4 , 8 , 2, true, 5, NO_SIMD)] = Degrain1to5_C<uint16_t, 4 , 8 , true, 5>;
  func_degrain[make_tuple(4 , 4 , 2, true, 5, NO_SIMD)] = Degrain1to5_C<uint16_t, 4 , 4 , true, 5>;
  func_degrain[make_tuple(4 , 2 , 2, true, 5, NO_SIMD)] = Degrain1to5_C<uint16_t, 4 , 2 , true, 5>;
  func_degrain[make_tuple(2 , 4 , 2, true, 5, NO_SIMD)] = Degrain1to5_C<uint16_t, 2 , 4 , true, 5>;
  func_degrain[make_tuple(2 , 2 , 2, true, 5, NO_SIMD)] = Degrain1to5_C<uint16_t, 2 , 2 , true, 5>;
#endif

// SSE2
// level1, lsb=false
  func_degrain[make_tuple(32, 32, 1, false, 1, USE_SSE2)] = Degrain1to5_sse2<32, 32, false, 1>;
  func_degrain[make_tuple(32, 16, 1, false, 1, USE_SSE2)] = Degrain1to5_sse2<32, 16, false, 1>;
  func_degrain[make_tuple(32, 8 , 1, false, 1, USE_SSE2)] = Degrain1to5_sse2<32, 8 , false, 1>;
  func_degrain[make_tuple(16, 32, 1, false, 1, USE_SSE2)] = Degrain1to5_sse2<16, 32, false, 1>;
  func_degrain[make_tuple(16, 16, 1, false, 1, USE_SSE2)] = Degrain1to5_sse2<16, 16, false, 1>;
  func_degrain[make_tuple(16, 8 , 1, false, 1, USE_SSE2)] = Degrain1to5_sse2<16, 8 , false, 1>;
  func_degrain[make_tuple(16, 4 , 1, false, 1, USE_SSE2)] = Degrain1to5_sse2<16, 4 , false, 1>;
  func_degrain[make_tuple(16, 2 , 1, false, 1, USE_SSE2)] = Degrain1to5_sse2<16, 2 , false, 1>;
  func_degrain[make_tuple(8 , 16, 1, false, 1, USE_SSE2)] = Degrain1to5_sse2<8 , 16, false, 1>;
  func_degrain[make_tuple(8 , 8 , 1, false, 1, USE_SSE2)] = Degrain1to5_sse2<8 , 8 , false, 1>;
  func_degrain[make_tuple(8 , 4 , 1, false, 1, USE_SSE2)] = Degrain1to5_sse2<8 , 4 , false, 1>;
  func_degrain[make_tuple(8 , 2 , 1, false, 1, USE_SSE2)] = Degrain1to5_sse2<8 , 2 , false, 1>;
  func_degrain[make_tuple(8 , 1 , 1, false, 1, USE_SSE2)] = Degrain1to5_sse2<8 , 1 , false, 1>;
  func_degrain[make_tuple(4 , 8 , 1, false, 1, USE_SSE2)] = Degrain1to5_sse2<4 , 8 , false, 1>;
  func_degrain[make_tuple(4 , 4 , 1, false, 1, USE_SSE2)] = Degrain1to5_sse2<4 , 4 , false, 1>;
  func_degrain[make_tuple(4 , 2 , 1, false, 1, USE_SSE2)] = Degrain1to5_sse2<4 , 2 , false, 1>;
  func_degrain[make_tuple(2 , 4 , 1, false, 1, USE_SSE2)] = Degrain1to5_sse2<2 , 4 , false, 1>;
  func_degrain[make_tuple(2 , 2 , 1, false, 1, USE_SSE2)] = Degrain1to5_sse2<2 , 2 , false, 1>;
  // level1 lsb=true
  func_degrain[make_tuple(32, 32, 1, true, 1, USE_SSE2)] = Degrain1to5_sse2<32, 32, true, 1>;
  func_degrain[make_tuple(32, 16, 1, true, 1, USE_SSE2)] = Degrain1to5_sse2<32, 16, true, 1>;
  func_degrain[make_tuple(32, 8 , 1, true, 1, USE_SSE2)] = Degrain1to5_sse2<32, 8 , true, 1>;
  func_degrain[make_tuple(16, 32, 1, true, 1, USE_SSE2)] = Degrain1to5_sse2<16, 32, true, 1>;
  func_degrain[make_tuple(16, 16, 1, true, 1, USE_SSE2)] = Degrain1to5_sse2<16, 16, true, 1>;
  func_degrain[make_tuple(16, 8 , 1, true, 1, USE_SSE2)] = Degrain1to5_sse2<16, 8 , true, 1>;
  func_degrain[make_tuple(16, 4 , 1, true, 1, USE_SSE2)] = Degrain1to5_sse2<16, 4 , true, 1>;
  func_degrain[make_tuple(16, 2 , 1, true, 1, USE_SSE2)] = Degrain1to5_sse2<16, 2 , true, 1>;
  func_degrain[make_tuple(8 , 16, 1, true, 1, USE_SSE2)] = Degrain1to5_sse2<8 , 16, true, 1>;
  func_degrain[make_tuple(8 , 8 , 1, true, 1, USE_SSE2)] = Degrain1to5_sse2<8 , 8 , true, 1>;
  func_degrain[make_tuple(8 , 4 , 1, true, 1, USE_SSE2)] = Degrain1to5_sse2<8 , 4 , true, 1>;
  func_degrain[make_tuple(8 , 2 , 1, true, 1, USE_SSE2)] = Degrain1to5_sse2<8 , 2 , true, 1>;
  func_degrain[make_tuple(8 , 1 , 1, true, 1, USE_SSE2)] = Degrain1to5_sse2<8 , 1 , true, 1>;
  func_degrain[make_tuple(4 , 8 , 1, true, 1, USE_SSE2)] = Degrain1to5_sse2<4 , 8 , true, 1>;
  func_degrain[make_tuple(4 , 4 , 1, true, 1, USE_SSE2)] = Degrain1to5_sse2<4 , 4 , true, 1>;
  func_degrain[make_tuple(4 , 2 , 1, true, 1, USE_SSE2)] = Degrain1to5_sse2<4 , 2 , true, 1>;
  func_degrain[make_tuple(2 , 4 , 1, true, 1, USE_SSE2)] = Degrain1to5_sse2<2 , 4 , true, 1>;
  func_degrain[make_tuple(2 , 2 , 1, true, 1, USE_SSE2)] = Degrain1to5_sse2<2 , 2 , true, 1>;


  // level2, lsb=false
  func_degrain[make_tuple(32, 32, 1, false, 2, USE_SSE2)] = Degrain1to5_sse2<32, 32, false, 2>;
  func_degrain[make_tuple(32, 16, 1, false, 2, USE_SSE2)] = Degrain1to5_sse2<32, 16, false, 2>;
  func_degrain[make_tuple(32, 8 , 1, false, 2, USE_SSE2)] = Degrain1to5_sse2<32, 8 , false, 2>;
  func_degrain[make_tuple(16, 32, 1, false, 2, USE_SSE2)] = Degrain1to5_sse2<16, 32, false, 2>;
  func_degrain[make_tuple(16, 16, 1, false, 2, USE_SSE2)] = Degrain1to5_sse2<16, 16, false, 2>;
  func_degrain[make_tuple(16, 8 , 1, false, 2, USE_SSE2)] = Degrain1to5_sse2<16, 8 , false, 2>;
  func_degrain[make_tuple(16, 4 , 1, false, 2, USE_SSE2)] = Degrain1to5_sse2<16, 4 , false, 2>;
  func_degrain[make_tuple(16, 2 , 1, false, 2, USE_SSE2)] = Degrain1to5_sse2<16, 2 , false, 2>;
  func_degrain[make_tuple(8 , 16, 1, false, 2, USE_SSE2)] = Degrain1to5_sse2<8 , 16, false, 2>;
  func_degrain[make_tuple(8 , 8 , 1, false, 2, USE_SSE2)] = Degrain1to5_sse2<8 , 8 , false, 2>;
  func_degrain[make_tuple(8 , 4 , 1, false, 2, USE_SSE2)] = Degrain1to5_sse2<8 , 4 , false, 2>;
  func_degrain[make_tuple(8 , 2 , 1, false, 2, USE_SSE2)] = Degrain1to5_sse2<8 , 2 , false, 2>;
  func_degrain[make_tuple(8 , 1 , 1, false, 2, USE_SSE2)] = Degrain1to5_sse2<8 , 1 , false, 2>;
  func_degrain[make_tuple(4 , 8 , 1, false, 2, USE_SSE2)] = Degrain1to5_sse2<4 , 8 , false, 2>;
  func_degrain[make_tuple(4 , 4 , 1, false, 2, USE_SSE2)] = Degrain1to5_sse2<4 , 4 , false, 2>;
  func_degrain[make_tuple(4 , 2 , 1, false, 2, USE_SSE2)] = Degrain1to5_sse2<4 , 2 , false, 2>;
  func_degrain[make_tuple(2 , 4 , 1, false, 2, USE_SSE2)] = Degrain1to5_sse2<2 , 4 , false, 2>;
  func_degrain[make_tuple(2 , 2 , 1, false, 2, USE_SSE2)] = Degrain1to5_sse2<2 , 2 , false, 2>;
  // level2 lsb=true
  func_degrain[make_tuple(32, 32, 1, true, 2, USE_SSE2)] = Degrain1to5_sse2<32, 32, true, 2>;
  func_degrain[make_tuple(32, 16, 1, true, 2, USE_SSE2)] = Degrain1to5_sse2<32, 16, true, 2>;
  func_degrain[make_tuple(32, 8 , 1, true, 2, USE_SSE2)] = Degrain1to5_sse2<32, 8 , true, 2>;
  func_degrain[make_tuple(16, 32, 1, true, 2, USE_SSE2)] = Degrain1to5_sse2<16, 32, true, 2>;
  func_degrain[make_tuple(16, 16, 1, true, 2, USE_SSE2)] = Degrain1to5_sse2<16, 16, true, 2>;
  func_degrain[make_tuple(16, 8 , 1, true, 2, USE_SSE2)] = Degrain1to5_sse2<16, 8 , true, 2>;
  func_degrain[make_tuple(16, 4 , 1, true, 2, USE_SSE2)] = Degrain1to5_sse2<16, 4 , true, 2>;
  func_degrain[make_tuple(16, 2 , 1, true, 2, USE_SSE2)] = Degrain1to5_sse2<16, 2 , true, 2>;
  func_degrain[make_tuple(8 , 16, 1, true, 2, USE_SSE2)] = Degrain1to5_sse2<8 , 16, true, 2>;
  func_degrain[make_tuple(8 , 8 , 1, true, 2, USE_SSE2)] = Degrain1to5_sse2<8 , 8 , true, 2>;
  func_degrain[make_tuple(8 , 4 , 1, true, 2, USE_SSE2)] = Degrain1to5_sse2<8 , 4 , true, 2>;
  func_degrain[make_tuple(8 , 2 , 1, true, 2, USE_SSE2)] = Degrain1to5_sse2<8 , 2 , true, 2>;
  func_degrain[make_tuple(8 , 1 , 1, true, 2, USE_SSE2)] = Degrain1to5_sse2<8 , 1 , true, 2>;
  func_degrain[make_tuple(4 , 8 , 1, true, 2, USE_SSE2)] = Degrain1to5_sse2<4 , 8 , true, 2>;
  func_degrain[make_tuple(4 , 4 , 1, true, 2, USE_SSE2)] = Degrain1to5_sse2<4 , 4 , true, 2>;
  func_degrain[make_tuple(4 , 2 , 1, true, 2, USE_SSE2)] = Degrain1to5_sse2<4 , 2 , true, 2>;
  func_degrain[make_tuple(2 , 4 , 1, true, 2, USE_SSE2)] = Degrain1to5_sse2<2 , 4 , true, 2>;
  func_degrain[make_tuple(2 , 2 , 1, true, 2, USE_SSE2)] = Degrain1to5_sse2<2 , 2 , true, 2>;

  // level3, lsb=false
  func_degrain[make_tuple(32, 32, 1, false, 3, USE_SSE2)] = Degrain1to5_sse2<32, 32, false, 3>;
  func_degrain[make_tuple(32, 16, 1, false, 3, USE_SSE2)] = Degrain1to5_sse2<32, 16, false, 3>;
  func_degrain[make_tuple(32, 8 , 1, false, 3, USE_SSE2)] = Degrain1to5_sse2<32, 8 , false, 3>;
  func_degrain[make_tuple(16, 32, 1, false, 3, USE_SSE2)] = Degrain1to5_sse2<16, 32, false, 3>;
  func_degrain[make_tuple(16, 16, 1, false, 3, USE_SSE2)] = Degrain1to5_sse2<16, 16, false, 3>;
  func_degrain[make_tuple(16, 8 , 1, false, 3, USE_SSE2)] = Degrain1to5_sse2<16, 8 , false, 3>;
  func_degrain[make_tuple(16, 4 , 1, false, 3, USE_SSE2)] = Degrain1to5_sse2<16, 4 , false, 3>;
  func_degrain[make_tuple(16, 2 , 1, false, 3, USE_SSE2)] = Degrain1to5_sse2<16, 2 , false, 3>;
  func_degrain[make_tuple(8 , 16, 1, false, 3, USE_SSE2)] = Degrain1to5_sse2<8 , 16, false, 3>;
  func_degrain[make_tuple(8 , 8 , 1, false, 3, USE_SSE2)] = Degrain1to5_sse2<8 , 8 , false, 3>;
  func_degrain[make_tuple(8 , 4 , 1, false, 3, USE_SSE2)] = Degrain1to5_sse2<8 , 4 , false, 3>;
  func_degrain[make_tuple(8 , 2 , 1, false, 3, USE_SSE2)] = Degrain1to5_sse2<8 , 2 , false, 3>;
  func_degrain[make_tuple(8 , 1 , 1, false, 3, USE_SSE2)] = Degrain1to5_sse2<8 , 1 , false, 3>;
  func_degrain[make_tuple(4 , 8 , 1, false, 3, USE_SSE2)] = Degrain1to5_sse2<4 , 8 , false, 3>;
  func_degrain[make_tuple(4 , 4 , 1, false, 3, USE_SSE2)] = Degrain1to5_sse2<4 , 4 , false, 3>;
  func_degrain[make_tuple(4 , 2 , 1, false, 3, USE_SSE2)] = Degrain1to5_sse2<4 , 2 , false, 3>;
  func_degrain[make_tuple(2 , 4 , 1, false, 3, USE_SSE2)] = Degrain1to5_sse2<2 , 4 , false, 3>;
  func_degrain[make_tuple(2 , 2 , 1, false, 3, USE_SSE2)] = Degrain1to5_sse2<2 , 2 , false, 3>;
  // level3, lsb=true
  func_degrain[make_tuple(32, 32, 1, true, 3, USE_SSE2)] = Degrain1to5_sse2<32, 32, true, 3>;
  func_degrain[make_tuple(32, 16, 1, true, 3, USE_SSE2)] = Degrain1to5_sse2<32, 16, true, 3>;
  func_degrain[make_tuple(32, 8 , 1, true, 3, USE_SSE2)] = Degrain1to5_sse2<32, 8 , true, 3>;
  func_degrain[make_tuple(16, 32, 1, true, 3, USE_SSE2)] = Degrain1to5_sse2<16, 32, true, 3>;
  func_degrain[make_tuple(16, 16, 1, true, 3, USE_SSE2)] = Degrain1to5_sse2<16, 16, true, 3>;
  func_degrain[make_tuple(16, 8 , 1, true, 3, USE_SSE2)] = Degrain1to5_sse2<16, 8 , true, 3>;
  func_degrain[make_tuple(16, 4 , 1, true, 3, USE_SSE2)] = Degrain1to5_sse2<16, 4 , true, 3>;
  func_degrain[make_tuple(16, 2 , 1, true, 3, USE_SSE2)] = Degrain1to5_sse2<16, 2 , true, 3>;
  func_degrain[make_tuple(8 , 16, 1, true, 3, USE_SSE2)] = Degrain1to5_sse2<8 , 16, true, 3>;
  func_degrain[make_tuple(8 , 8 , 1, true, 3, USE_SSE2)] = Degrain1to5_sse2<8 , 8 , true, 3>;
  func_degrain[make_tuple(8 , 4 , 1, true, 3, USE_SSE2)] = Degrain1to5_sse2<8 , 4 , true, 3>;
  func_degrain[make_tuple(8 , 2 , 1, true, 3, USE_SSE2)] = Degrain1to5_sse2<8 , 2 , true, 3>;
  func_degrain[make_tuple(8 , 1 , 1, true, 3, USE_SSE2)] = Degrain1to5_sse2<8 , 1 , true, 3>;
  func_degrain[make_tuple(4 , 8 , 1, true, 3, USE_SSE2)] = Degrain1to5_sse2<4 , 8 , true, 3>;
  func_degrain[make_tuple(4 , 4 , 1, true, 3, USE_SSE2)] = Degrain1to5_sse2<4 , 4 , true, 3>;
  func_degrain[make_tuple(4 , 2 , 1, true, 3, USE_SSE2)] = Degrain1to5_sse2<4 , 2 , true, 3>;
  func_degrain[make_tuple(2 , 4 , 1, true, 3, USE_SSE2)] = Degrain1to5_sse2<2 , 4 , true, 3>;
  func_degrain[make_tuple(2 , 2 , 1, true, 3, USE_SSE2)] = Degrain1to5_sse2<2 , 2 , true, 3>;

  // level4, lsb=false
  func_degrain[make_tuple(32, 32, 1, false, 4, USE_SSE2)] = Degrain1to5_sse2<32, 32, false, 4>;
  func_degrain[make_tuple(32, 16, 1, false, 4, USE_SSE2)] = Degrain1to5_sse2<32, 16, false, 4>;
  func_degrain[make_tuple(32, 8 , 1, false, 4, USE_SSE2)] = Degrain1to5_sse2<32, 8 , false, 4>;
  func_degrain[make_tuple(16, 32, 1, false, 4, USE_SSE2)] = Degrain1to5_sse2<16, 32, false, 4>;
  func_degrain[make_tuple(16, 16, 1, false, 4, USE_SSE2)] = Degrain1to5_sse2<16, 16, false, 4>;
  func_degrain[make_tuple(16, 8 , 1, false, 4, USE_SSE2)] = Degrain1to5_sse2<16, 8 , false, 4>;
  func_degrain[make_tuple(16, 4 , 1, false, 4, USE_SSE2)] = Degrain1to5_sse2<16, 4 , false, 4>;
  func_degrain[make_tuple(16, 2 , 1, false, 4, USE_SSE2)] = Degrain1to5_sse2<16, 2 , false, 4>;
  func_degrain[make_tuple(8 , 16, 1, false, 4, USE_SSE2)] = Degrain1to5_sse2<8 , 16, false, 4>;
  func_degrain[make_tuple(8 , 8 , 1, false, 4, USE_SSE2)] = Degrain1to5_sse2<8 , 8 , false, 4>;
  func_degrain[make_tuple(8 , 4 , 1, false, 4, USE_SSE2)] = Degrain1to5_sse2<8 , 4 , false, 4>;
  func_degrain[make_tuple(8 , 2 , 1, false, 4, USE_SSE2)] = Degrain1to5_sse2<8 , 2 , false, 4>;
  func_degrain[make_tuple(8 , 1 , 1, false, 4, USE_SSE2)] = Degrain1to5_sse2<8 , 1 , false, 4>;
  func_degrain[make_tuple(4 , 8 , 1, false, 4, USE_SSE2)] = Degrain1to5_sse2<4 , 8 , false, 4>;
  func_degrain[make_tuple(4 , 4 , 1, false, 4, USE_SSE2)] = Degrain1to5_sse2<4 , 4 , false, 4>;
  func_degrain[make_tuple(4 , 2 , 1, false, 4, USE_SSE2)] = Degrain1to5_sse2<4 , 2 , false, 4>;
  func_degrain[make_tuple(2 , 4 , 1, false, 4, USE_SSE2)] = Degrain1to5_sse2<2 , 4 , false, 4>;
  func_degrain[make_tuple(2 , 2 , 1, false, 4, USE_SSE2)] = Degrain1to5_sse2<2 , 2 , false, 4>;
  // level4, lsb=true
  func_degrain[make_tuple(32, 32, 1, true, 4, USE_SSE2)] = Degrain1to5_sse2<32, 32, true, 4>;
  func_degrain[make_tuple(32, 16, 1, true, 4, USE_SSE2)] = Degrain1to5_sse2<32, 16, true, 4>;
  func_degrain[make_tuple(32, 8 , 1, true, 4, USE_SSE2)] = Degrain1to5_sse2<32, 8 , true, 4>;
  func_degrain[make_tuple(16, 32, 1, true, 4, USE_SSE2)] = Degrain1to5_sse2<16, 32, true, 4>;
  func_degrain[make_tuple(16, 16, 1, true, 4, USE_SSE2)] = Degrain1to5_sse2<16, 16, true, 4>;
  func_degrain[make_tuple(16, 8 , 1, true, 4, USE_SSE2)] = Degrain1to5_sse2<16, 8 , true, 4>;
  func_degrain[make_tuple(16, 4 , 1, true, 4, USE_SSE2)] = Degrain1to5_sse2<16, 4 , true, 4>;
  func_degrain[make_tuple(16, 2 , 1, true, 4, USE_SSE2)] = Degrain1to5_sse2<16, 2 , true, 4>;
  func_degrain[make_tuple(8 , 16, 1, true, 4, USE_SSE2)] = Degrain1to5_sse2<8 , 16, true, 4>;
  func_degrain[make_tuple(8 , 8 , 1, true, 4, USE_SSE2)] = Degrain1to5_sse2<8 , 8 , true, 4>;
  func_degrain[make_tuple(8 , 4 , 1, true, 4, USE_SSE2)] = Degrain1to5_sse2<8 , 4 , true, 4>;
  func_degrain[make_tuple(8 , 2 , 1, true, 4, USE_SSE2)] = Degrain1to5_sse2<8 , 2 , true, 4>;
  func_degrain[make_tuple(8 , 1 , 1, true, 4, USE_SSE2)] = Degrain1to5_sse2<8 , 1 , true, 4>;
  func_degrain[make_tuple(4 , 8 , 1, true, 4, USE_SSE2)] = Degrain1to5_sse2<4 , 8 , true, 4>;
  func_degrain[make_tuple(4 , 4 , 1, true, 4, USE_SSE2)] = Degrain1to5_sse2<4 , 4 , true, 4>;
  func_degrain[make_tuple(4 , 2 , 1, true, 4, USE_SSE2)] = Degrain1to5_sse2<4 , 2 , true, 4>;
  func_degrain[make_tuple(2 , 4 , 1, true, 4, USE_SSE2)] = Degrain1to5_sse2<2 , 4 , true, 4>;
  func_degrain[make_tuple(2 , 2 , 1, true, 4, USE_SSE2)] = Degrain1to5_sse2<2 , 2 , true, 4>;

  // level5, lsb=false
  func_degrain[make_tuple(32, 32, 1, false, 5, USE_SSE2)] = Degrain1to5_sse2<32, 32, false, 5>;
  func_degrain[make_tuple(32, 16, 1, false, 5, USE_SSE2)] = Degrain1to5_sse2<32, 16, false, 5>;
  func_degrain[make_tuple(32, 8 , 1, false, 5, USE_SSE2)] = Degrain1to5_sse2<32, 8 , false, 5>;
  func_degrain[make_tuple(16, 32, 1, false, 5, USE_SSE2)] = Degrain1to5_sse2<16, 32, false, 5>;
  func_degrain[make_tuple(16, 16, 1, false, 5, USE_SSE2)] = Degrain1to5_sse2<16, 16, false, 5>;
  func_degrain[make_tuple(16, 8 , 1, false, 5, USE_SSE2)] = Degrain1to5_sse2<16, 8 , false, 5>;
  func_degrain[make_tuple(16, 4 , 1, false, 5, USE_SSE2)] = Degrain1to5_sse2<16, 4 , false, 5>;
  func_degrain[make_tuple(16, 2 , 1, false, 5, USE_SSE2)] = Degrain1to5_sse2<16, 2 , false, 5>;
  func_degrain[make_tuple(8 , 16, 1, false, 5, USE_SSE2)] = Degrain1to5_sse2<8 , 16, false, 5>;
  func_degrain[make_tuple(8 , 8 , 1, false, 5, USE_SSE2)] = Degrain1to5_sse2<8 , 8 , false, 5>;
  func_degrain[make_tuple(8 , 4 , 1, false, 5, USE_SSE2)] = Degrain1to5_sse2<8 , 4 , false, 5>;
  func_degrain[make_tuple(8 , 2 , 1, false, 5, USE_SSE2)] = Degrain1to5_sse2<8 , 2 , false, 5>;
  func_degrain[make_tuple(8 , 1 , 1, false, 5, USE_SSE2)] = Degrain1to5_sse2<8 , 1 , false, 5>;
  func_degrain[make_tuple(4 , 8 , 1, false, 5, USE_SSE2)] = Degrain1to5_sse2<4 , 8 , false, 5>;
  func_degrain[make_tuple(4 , 4 , 1, false, 5, USE_SSE2)] = Degrain1to5_sse2<4 , 4 , false, 5>;
  func_degrain[make_tuple(4 , 2 , 1, false, 5, USE_SSE2)] = Degrain1to5_sse2<4 , 2 , false, 5>;
  func_degrain[make_tuple(2 , 4 , 1, false, 5, USE_SSE2)] = Degrain1to5_sse2<2 , 4 , false, 5>;
  func_degrain[make_tuple(2 , 2 , 1, false, 5, USE_SSE2)] = Degrain1to5_sse2<2 , 2 , false, 5>;
  // level5, lsb=true
  func_degrain[make_tuple(32, 32, 1, true, 5, USE_SSE2)] = Degrain1to5_sse2<32, 32, true, 5>;
  func_degrain[make_tuple(32, 16, 1, true, 5, USE_SSE2)] = Degrain1to5_sse2<32, 16, true, 5>;
  func_degrain[make_tuple(32, 8 , 1, true, 5, USE_SSE2)] = Degrain1to5_sse2<32, 8 , true, 5>;
  func_degrain[make_tuple(16, 32, 1, true, 5, USE_SSE2)] = Degrain1to5_sse2<16, 32, true, 5>;
  func_degrain[make_tuple(16, 16, 1, true, 5, USE_SSE2)] = Degrain1to5_sse2<16, 16, true, 5>;
  func_degrain[make_tuple(16, 8 , 1, true, 5, USE_SSE2)] = Degrain1to5_sse2<16, 8 , true, 5>;
  func_degrain[make_tuple(16, 4 , 1, true, 5, USE_SSE2)] = Degrain1to5_sse2<16, 4 , true, 5>;
  func_degrain[make_tuple(16, 2 , 1, true, 5, USE_SSE2)] = Degrain1to5_sse2<16, 2 , true, 5>;
  func_degrain[make_tuple(8 , 16, 1, true, 5, USE_SSE2)] = Degrain1to5_sse2<8 , 16, true, 5>;
  func_degrain[make_tuple(8 , 8 , 1, true, 5, USE_SSE2)] = Degrain1to5_sse2<8 , 8 , true, 5>;
  func_degrain[make_tuple(8 , 4 , 1, true, 5, USE_SSE2)] = Degrain1to5_sse2<8 , 4 , true, 5>;
  func_degrain[make_tuple(8 , 2 , 1, true, 5, USE_SSE2)] = Degrain1to5_sse2<8 , 2 , true, 5>;
  func_degrain[make_tuple(8 , 1 , 1, true, 5, USE_SSE2)] = Degrain1to5_sse2<8 , 1 , true, 5>;
  func_degrain[make_tuple(4 , 8 , 1, true, 5, USE_SSE2)] = Degrain1to5_sse2<4 , 8 , true, 5>;
  func_degrain[make_tuple(4 , 4 , 1, true, 5, USE_SSE2)] = Degrain1to5_sse2<4 , 4 , true, 5>;
  func_degrain[make_tuple(4 , 2 , 1, true, 5, USE_SSE2)] = Degrain1to5_sse2<4 , 2 , true, 5>;
  func_degrain[make_tuple(2 , 4 , 1, true, 5, USE_SSE2)] = Degrain1to5_sse2<2 , 4 , true, 5>;
  func_degrain[make_tuple(2 , 2 , 1, true, 5, USE_SSE2)] = Degrain1to5_sse2<2 , 2 , true, 5>;

  Denoise1to5Function *result = func_degrain[make_tuple(BlockX, BlockY, pixelsize, lsb_flag, level, arch)];
  if(!result) // fallback to C
    result = func_degrain[make_tuple(BlockX, BlockY, pixelsize, lsb_flag, level, NO_SIMD)];
  return result;
}


// If mvfw is null, mvbw is assumed to be a radius-3 multi-vector clip.
MVDegrainX::MVDegrainX (
	PClip _child, PClip _super, PClip mvbw, PClip mvfw, PClip mvbw2,  PClip mvfw2, PClip mvbw3,  PClip mvfw3, PClip mvbw4,  PClip mvfw4, PClip mvbw5,  PClip mvfw5,
	int _thSAD, int _thSADC, int _YUVplanes, int _nLimit, int _nLimitC,
	int _nSCD1, int _nSCD2, bool _isse2, bool _planar, bool _lsb_flag,
	bool mt_flag, int _level, IScriptEnvironment* env
)
:	GenericVideoFilter (_child)
//,	MVFilter ((! mvfw) ? mvbw : mvfw,  "MDegrain1",    env, (! mvfw) ? 2 : 1, (! mvfw) ? 1 : 0)
//,	MVFilter ((! mvfw) ? mvbw : mvfw2, "MDegrain2",    env, (! mvfw) ? 4 : 1, (! mvfw) ? 3 : 0)
,	MVFilter (
  (! mvfw) ? mvbw : (_level==1 ? mvfw : (_level == 2 ? mvfw2 : (_level == 3 ? mvfw3 : (_level == 4 ? mvfw4 : mvfw5)))),  // mvfw/mvfw2/mvfw3/mvfw4/mvfw5
  _level == 1 ? "MDegrain1" : (_level==2 ? "MDegrain2" : (_level==3 ? "MDegrain3" : (_level==4 ? "MDegrain4" : "MDegrain5"))),   // MDegrain1/2/3/4/5
  env, 
  (! mvfw) ? _level*2 : 1, (! mvfw) ? _level*2-1 : 0) // 1/3/5
/*
,	mvClipF  ((! mvfw) ? mvbw : mvfw,  _nSCD1, _nSCD2, env, (! mvfw) ? 2 : 1, (! mvfw) ? 1 : 0)
,	mvClipB  ((! mvfw) ? mvbw : mvbw,  _nSCD1, _nSCD2, env, (! mvfw) ? 2 : 1, (! mvfw) ? 0 : 0)

,	mvClipF2 ((! mvfw) ? mvbw : mvfw2, _nSCD1, _nSCD2, env, (! mvfw) ? 4 : 1, (! mvfw) ? 3 : 0)
,	mvClipF  ((! mvfw) ? mvbw : mvfw,  _nSCD1, _nSCD2, env, (! mvfw) ? 4 : 1, (! mvfw) ? 1 : 0)
,	mvClipB  ((! mvfw) ? mvbw : mvbw,  _nSCD1, _nSCD2, env, (! mvfw) ? 4 : 1, (! mvfw) ? 0 : 0)
,	mvClipB2 ((! mvfw) ? mvbw : mvbw2, _nSCD1, _nSCD2, env, (! mvfw) ? 4 : 1, (! mvfw) ? 2 : 0)
*/

,	super (_super)
,	lsb_flag (_lsb_flag)
,	height_lsb_mul ((_lsb_flag) ? 2 : 1)
,	DstShort (0)
,	DstInt (0)
, level(_level)
{
  const int group_len = level * 2; // 2, 4, 6
  mvClipF[0] = new MVClip((!mvfw) ? mvbw : mvfw, _nSCD1, _nSCD2, env, (!mvfw) ? group_len : 1, (!mvfw) ? 1 : 0);
  mvClipB[0] = new MVClip((!mvfw) ? mvbw : mvbw, _nSCD1, _nSCD2, env, (!mvfw) ? group_len : 1, (!mvfw) ? 0 : 0);
  if(level>=2) {
    mvClipF[1] = new MVClip((!mvfw) ? mvbw : mvfw2, _nSCD1, _nSCD2, env, (!mvfw) ? group_len : 1, (!mvfw) ? 3 : 0);
    mvClipB[1] = new MVClip((!mvfw) ? mvbw : mvbw2, _nSCD1, _nSCD2, env, (!mvfw) ? group_len : 1, (!mvfw) ? 2 : 0);
    if(level>=3) {
      mvClipF[2] = new MVClip((!mvfw) ? mvbw : mvfw3, _nSCD1, _nSCD2, env, (!mvfw) ? group_len : 1, (!mvfw) ? 5 : 0);
      mvClipB[2] = new MVClip((!mvfw) ? mvbw : mvbw3, _nSCD1, _nSCD2, env, (!mvfw) ? group_len : 1, (!mvfw) ? 4 : 0);
      if(level>=4) {
        mvClipF[3] = new MVClip((!mvfw) ? mvbw : mvfw4, _nSCD1, _nSCD2, env, (!mvfw) ? group_len : 1, (!mvfw) ? 7 : 0);
        mvClipB[3] = new MVClip((!mvfw) ? mvbw : mvbw4, _nSCD1, _nSCD2, env, (!mvfw) ? group_len : 1, (!mvfw) ? 6 : 0);
        if(level>=5) {
          mvClipF[4] = new MVClip((!mvfw) ? mvbw : mvfw5, _nSCD1, _nSCD2, env, (!mvfw) ? group_len : 1, (!mvfw) ? 9 : 0);
          mvClipB[4] = new MVClip((!mvfw) ? mvbw : mvbw5, _nSCD1, _nSCD2, env, (!mvfw) ? group_len : 1, (!mvfw) ? 8 : 0);
        }
      }
    }
  }


  thSAD = _thSAD * mvClipB[0]->GetThSCD1() / _nSCD1; // normalize to block SAD
  thSADC = _thSADC * mvClipB[0]->GetThSCD1() / _nSCD1; // chroma
  // later can be modified for 16 bit

	YUVplanes = _YUVplanes;
	nLimit = _nLimit;
	nLimitC = _nLimitC;
	isse2 = _isse2;
	planar = _planar;

  CheckSimilarity(*mvClipB[0], "mvbw",  env);
  CheckSimilarity(*mvClipF[0], "mvfw",  env);
  if(level>=2) {
    CheckSimilarity(*mvClipF[1], "mvfw2", env);
    CheckSimilarity(*mvClipB[1], "mvbw2", env);
    if(level>=3) {
      CheckSimilarity(*mvClipF[2], "mvfw3", env);
      CheckSimilarity(*mvClipB[2], "mvbw3", env);
      if(level>=4) {
        CheckSimilarity(*mvClipF[3], "mvfw4", env);
        CheckSimilarity(*mvClipB[3], "mvbw4", env);
        if(level>=5) {
          CheckSimilarity(*mvClipF[4], "mvfw5", env);
          CheckSimilarity(*mvClipB[4], "mvbw5", env);
        }
      }
    }
  }

  if (mvClipB[0]->GetDeltaFrame() <= 0 || mvClipF[0]->GetDeltaFrame() <= 0)   // 2.5.11.22, 
    //todo check PF 160926: 2nd must be clipF, not the same clipB
    env->ThrowError("MDegrain%d: cannot use motion vectors with absolute frame references.", level);

	const ::VideoInfo &	vi_super = _super->GetVideoInfo ();

  //pixelsize and bits_per_pixel: in MVFilter, property of motion vectors

#ifdef AVS16
  pixelsize_super = vi_super.ComponentSize();
  bits_per_pixel_super = vi_super.BitsPerComponent();
  // pixelsize, bits_per_pixel: vector clip data
#else
  pixelsize_super = 1;
  bits_per_pixel_super = 8;
#endif
  // SAD is coming from the mv clip analysis. Parameter must be scaled accordingly
  //if (pixelsize_super == 2) {
  
    thSAD = int(thSAD / 255.0 * ((1 << bits_per_pixel) - 1));
    thSADC = int(thSADC / 255.0 * ((1 << bits_per_pixel) - 1));
  
  //}
  // todo Check: nLimit and nLimitC default: 255 (always 8 bit range)

	// get parameters of prepared super clip - v2.0
	SuperParams64Bits params;
	memcpy(&params, &vi_super.num_audio_samples, 8);
	int nHeightS = params.nHeight;
	int nSuperHPad = params.nHPad;
	int nSuperVPad = params.nVPad;
	int nSuperPel = params.nPel;
	nSuperModeYUV = params.nModeYUV;
	int nSuperLevels = params.nLevels;
  for(int i=0;i<level;i++) {
    pRefBGOF[i] = new MVGroupOfFrames(nSuperLevels, nWidth, nHeight, nSuperPel, nSuperHPad, nSuperVPad, nSuperModeYUV, isse2, xRatioUV, yRatioUV, pixelsize_super, bits_per_pixel_super, mt_flag);
    pRefFGOF[i] = new MVGroupOfFrames(nSuperLevels, nWidth, nHeight, nSuperPel, nSuperHPad, nSuperVPad, nSuperModeYUV, isse2, xRatioUV, yRatioUV, pixelsize_super, bits_per_pixel_super, mt_flag);
  }
	int nSuperWidth  = vi_super.width;
	int nSuperHeight = vi_super.height;

	if (   nHeight != nHeightS
	    || nHeight != vi.height
	    || nWidth  != nSuperWidth-nSuperHPad*2
	    || nWidth  != vi.width
	    || nPel    != nSuperPel)
	{
		env->ThrowError("MDegrainX : wrong source or super frame size");
	}

   if ( (pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2 && !planar)
   {
		DstPlanes =  new YUY2Planes(nWidth, nHeight * height_lsb_mul);
		SrcPlanes =  new YUY2Planes(nWidth, nHeight);
   }
   dstShortPitch = ((nWidth + 15)/16)*16;  // short (2 byte) granularity
	 dstIntPitch = dstShortPitch; // int (4 byte) granulairty
   if (nOverlapX >0 || nOverlapY>0)
   {
		OverWins = new OverlapWindows(nBlkSizeX, nBlkSizeY, nOverlapX, nOverlapY);
		OverWinsUV = new OverlapWindows(nBlkSizeX/xRatioUV, nBlkSizeY/yRatioUV, nOverlapX/xRatioUV, nOverlapY/yRatioUV);
		if (lsb_flag || pixelsize_super == 2)
		{
			DstInt = new int [dstIntPitch * nHeight];
		}
		else
		{
			DstShort = new unsigned short[dstShortPitch*nHeight];
		}
   }

   // in overlaps.h
   // OverlapsLsbFunction
   // OverlapsFunction
   // in M(V)DegrainX: DenoiseXFunction
   arch_t arch;
   if ((pixelsize_super == 1) && (((env->GetCPUFlags() & CPUF_SSE2) != 0) & isse2))
       arch = USE_SSE2;
   /*else if ((pixelsize == 1) && isse2) // PF no MMX support
       arch = USE_MMX;*/
   else
       arch = NO_SIMD;

   // C only -> NO_SIMD
   // lsb 16-bit hack: uint8_t only
   OVERSLUMALSB   = get_overlaps_lsb_function(nBlkSizeX, nBlkSizeY, sizeof(uint8_t), NO_SIMD);
   OVERSCHROMALSB = get_overlaps_lsb_function(nBlkSizeX/xRatioUV, nBlkSizeY/yRatioUV, sizeof(uint8_t), NO_SIMD);

   OVERSLUMA   = get_overlaps_function(nBlkSizeX, nBlkSizeY, sizeof(uint8_t), arch);
   OVERSCHROMA = get_overlaps_function(nBlkSizeX/xRatioUV, nBlkSizeY/yRatioUV, sizeof(uint8_t), arch);


  // todo: like lsb function int ptr
   OVERSLUMA16   = get_overlaps_function(nBlkSizeX, nBlkSizeY, sizeof(uint16_t), NO_SIMD);
   OVERSCHROMA16   = get_overlaps_function(nBlkSizeX, nBlkSizeY, sizeof(uint16_t), NO_SIMD);

   //DEGRAINLUMA = get_denoise3_function(nBlkSizeX, nBlkSizeY, pixelsize, arch);
   //DEGRAINCHROMA = get_denoise3_function(nBlkSizeX/xRatioUV, nBlkSizeY/yRatioUV, pixelsize, arch);
   DEGRAINLUMA = get_denoise123_function(nBlkSizeX, nBlkSizeY, pixelsize_super, lsb_flag, level, arch);
   DEGRAINCHROMA = get_denoise123_function(nBlkSizeX/xRatioUV, nBlkSizeY/yRatioUV, pixelsize_super, lsb_flag, level, arch);
   if(!OVERSLUMA)
     env->ThrowError("MDegrain%d : no valid OVERSLUMA function for %dx%d, pixelsize=%d, lsb_flag=%d, level=%d", level, nBlkSizeX, nBlkSizeY, pixelsize_super, (int)lsb_flag, level);
   if(!OVERSCHROMA)
     env->ThrowError("MDegrain%d : no valid OVERSCHROMA function for %dx%d, pixelsize=%d, lsb_flag=%d, level=%d", level, nBlkSizeX, nBlkSizeY, pixelsize_super, (int)lsb_flag, level);
   if(!DEGRAINLUMA)
     env->ThrowError("MDegrain%d : no valid DEGRAINLUMA function for %dx%d, pixelsize=%d, lsb_flag=%d, level=%d", level, nBlkSizeX, nBlkSizeY, pixelsize_super, (int)lsb_flag, level);
   if(!DEGRAINCHROMA)
     env->ThrowError("MDegrain%d : no valid DEGRAINCHROMA function for %dx%d, pixelsize=%d, lsb_flag=%d, level=%d", level, nBlkSizeX, nBlkSizeY, pixelsize_super, (int)lsb_flag, level);

	const int		tmp_size = 32 * 32 * pixelsize_super;
	tmpBlock = new BYTE[tmp_size * height_lsb_mul];
	tmpBlockLsb = (lsb_flag) ? (tmpBlock + tmp_size) : 0;

	if (lsb_flag)
	{
		vi.height <<= 1;
	}
}


MVDegrainX::~MVDegrainX()
{
   if ( (pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2 )
   {
		delete DstPlanes;
		delete SrcPlanes;
   }
   if (nOverlapX >0 || nOverlapY>0)
   {
	   delete OverWins;
	   delete OverWinsUV;
	   delete [] DstShort;
	   delete [] DstInt;
   }
   delete [] tmpBlock;
   for (int i = 0; i < level; i++) {
     delete pRefBGOF[i];
     delete pRefFGOF[i];
   }
   //delete pRefB2GOF;
   //delete pRefF2GOF;
   //delete pRefB3GOF;
   //delete pRefF3GOF;
}




PVideoFrame __stdcall MVDegrainX::GetFrame(int n, IScriptEnvironment* env)
{
	int nWidth_B = nBlkX*(nBlkSizeX - nOverlapX) + nOverlapX;
	int nHeight_B = nBlkY*(nBlkSizeY - nOverlapY) + nOverlapY;

	BYTE *pDst[3], *pDstCur[3];
	const BYTE *pSrcCur[3];
	const BYTE *pSrc[3];
	const BYTE *pRefB[MAX_DEGRAIN][3];
	const BYTE *pRefF[MAX_DEGRAIN][3];
	//const BYTE *pRefB2[3];
	//const BYTE *pRefF2[3];
	//const BYTE *pRefB3[3];
	//const BYTE *pRefF3[3];
	int nDstPitches[3], nSrcPitches[3];
	int nRefBPitches[MAX_DEGRAIN][3], nRefFPitches[MAX_DEGRAIN][3];
	//int nRefB2Pitches[3], nRefF2Pitches[3];
	//int nRefB3Pitches[3], nRefF3Pitches[3];
	unsigned char *pDstYUY2;
	const unsigned char *pSrcYUY2;
	int nDstPitchYUY2;
	int nSrcPitchYUY2;
  bool isUsableB[MAX_DEGRAIN], isUsableF[MAX_DEGRAIN]; // , isUsableB2, isUsableF2, isUsableB3, isUsableF3;

  PVideoFrame mvF[MAX_DEGRAIN];
  PVideoFrame mvB[MAX_DEGRAIN];

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
  /*
	PVideoFrame mvF3 = mvClipF3.GetFrame(n, env);
	mvClipF3.Update(mvF3, env);
	isUsableF3 = mvClipF3.IsUsable();
	mvF3 = 0; // v2.0.9.2 -  it seems, we do not need in vectors clip anymore when we finished copiing them to fakeblockdatas
	PVideoFrame mvF2 = mvClipF2.GetFrame(n, env);
	mvClipF2.Update(mvF2, env);
	isUsableF2 = mvClipF2.IsUsable();
	mvF2 =0;
	PVideoFrame mvF = mvClipF.GetFrame(n, env);
	mvClipF.Update(mvF, env);
	isUsableF = mvClipF.IsUsable();
	mvF =0;
  
	PVideoFrame mvB = mvClipB.GetFrame(n, env);
	mvClipB.Update(mvB, env);
	isUsableB = mvClipB.IsUsable();
	mvB =0;
	PVideoFrame mvB2 = mvClipB2.GetFrame(n, env);
	mvClipB2.Update(mvB2, env);
	isUsableB2 = mvClipB2.IsUsable();
	mvB2 =0;
	PVideoFrame mvB3 = mvClipB3.GetFrame(n, env);
	mvClipB3.Update(mvB3, env);
	isUsableB3 = mvClipB3.IsUsable();
	mvB3 =0;
  */
	int				lsb_offset_y = 0;
	int				lsb_offset_u = 0;
	int				lsb_offset_v = 0;

//	if ( mvClipB.IsUsable() && mvClipF.IsUsable() && mvClipB2.IsUsable() && mvClipF2.IsUsable() )
//	{
	PVideoFrame	src	= child->GetFrame(n, env);
	PVideoFrame dst = env->NewVideoFrame(vi);
	if ( (pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2 )
	{
		if (!planar)
		{
			pDstYUY2 = dst->GetWritePtr();
			nDstPitchYUY2 = dst->GetPitch();
			pDst[0] = DstPlanes->GetPtr();
			pDst[1] = DstPlanes->GetPtrU();
			pDst[2] = DstPlanes->GetPtrV();
			nDstPitches[0]  = DstPlanes->GetPitch();
			nDstPitches[1]  = DstPlanes->GetPitchUV();
			nDstPitches[2]  = DstPlanes->GetPitchUV();
			pSrcYUY2 = src->GetReadPtr();
			nSrcPitchYUY2 = src->GetPitch();
			pSrc[0] = SrcPlanes->GetPtr();
			pSrc[1] = SrcPlanes->GetPtrU();
			pSrc[2] = SrcPlanes->GetPtrV();
			nSrcPitches[0]  = SrcPlanes->GetPitch();
			nSrcPitches[1]  = SrcPlanes->GetPitchUV();
			nSrcPitches[2]  = SrcPlanes->GetPitchUV();
			YUY2ToPlanes(pSrcYUY2, nSrcPitchYUY2, nWidth, nHeight,
				pSrc[0], nSrcPitches[0], pSrc[1], pSrc[2], nSrcPitches[1], isse2);
		}
		else
		{
			pDst[0] = dst->GetWritePtr();
			pDst[1] = pDst[0] + nWidth;
			pDst[2] = pDst[1] + nWidth/2;
			nDstPitches[0] = dst->GetPitch();
			nDstPitches[1] = nDstPitches[0];
			nDstPitches[2] = nDstPitches[0];
			pSrc[0] = src->GetReadPtr();
			pSrc[1] = pSrc[0] + nWidth;
			pSrc[2] = pSrc[1] + nWidth/2;
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

	lsb_offset_y = nDstPitches [0] *  nHeight;
	lsb_offset_u = nDstPitches [1] * (nHeight / yRatioUV);
	lsb_offset_v = nDstPitches [2] * (nHeight / yRatioUV);

	if (lsb_flag)
	{
		memset (pDst [0] + lsb_offset_y, 0, lsb_offset_y);
		if (! planar)
		{
			memset (pDst [1] + lsb_offset_u, 0, lsb_offset_u);
			memset (pDst [2] + lsb_offset_v, 0, lsb_offset_v);
		}
	}

	PVideoFrame refB[MAX_DEGRAIN], refF[MAX_DEGRAIN];

  // reorder ror regular frames order in v2.0.9.2
  for (int j = level - 1; j >= 0; j--)
    mvClipF[j]->use_ref_frame (refF[j], isUsableF[j], super, n, env);
  for (int j = 0; j < level; j++)
    mvClipB[j]->use_ref_frame (refB[j], isUsableB[j], super, n, env);
/*
	mvClipF3.use_ref_frame (refF3, isUsableF3, super, n, env);
	mvClipF2.use_ref_frame (refF2, isUsableF2, super, n, env);
	mvClipF. use_ref_frame (refF,  isUsableF,  super, n, env);
	mvClipB. use_ref_frame (refB,  isUsableB,  super, n, env);
	mvClipB2.use_ref_frame (refB2, isUsableB2, super, n, env);
	mvClipB3.use_ref_frame (refB3, isUsableB3, super, n, env);
*/
	if ( (pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2 )
	{
    for (int j = level - 1; j >= 0; j--)
    {
      if (isUsableF[j])
      {
        pRefF[j][0] = refF[j]->GetReadPtr();
        pRefF[j][1] = pRefF[j][0] + refF[j]->GetRowSize()/2;
        pRefF[j][2] = pRefF[j][1] + refF[j]->GetRowSize()/4;
        nRefFPitches[j][0]  = refF[j]->GetPitch();
        nRefFPitches[j][1]  = nRefFPitches[j][0];
        nRefFPitches[j][2]  = nRefFPitches[j][0];
      }
    }
    /*
		if (isUsableF3)
		{
			pRefF3[0] = refF3->GetReadPtr();
			pRefF3[1] = pRefF3[0] + refF3->GetRowSize()/2;
			pRefF3[2] = pRefF3[1] + refF3->GetRowSize()/4;
			nRefF3Pitches[0]  = refF3->GetPitch();
			nRefF3Pitches[1]  = nRefF3Pitches[0];
			nRefF3Pitches[2]  = nRefF3Pitches[0];
		}
		if (isUsableF2)
		{
			pRefF2[0] = refF2->GetReadPtr();
			pRefF2[1] = pRefF2[0] + refF2->GetRowSize()/2;
			pRefF2[2] = pRefF2[1] + refF2->GetRowSize()/4;
			nRefF2Pitches[0]  = refF2->GetPitch();
			nRefF2Pitches[1]  = nRefF2Pitches[0];
			nRefF2Pitches[2]  = nRefF2Pitches[0];
		}
		if (isUsableF)
		{
			pRefF[0] = refF->GetReadPtr();
			pRefF[1] = pRefF[0] + refF->GetRowSize()/2;
			pRefF[2] = pRefF[1] + refF->GetRowSize()/4;
			nRefFPitches[0]  = refF->GetPitch();
			nRefFPitches[1]  = nRefFPitches[0];
			nRefFPitches[2]  = nRefFPitches[0];
		}
    */
    for (int j = 0; j<level;j++)
    {
      if (isUsableB[j])
      {
        pRefB[j][0] = refB[j]->GetReadPtr();
        pRefB[j][1] = pRefB[j][0] + refB[j]->GetRowSize()/2;
        pRefB[j][2] = pRefB[j][1] + refB[j]->GetRowSize()/4;
        nRefBPitches[j][0]  = refB[j]->GetPitch();
        nRefBPitches[j][1]  = nRefBPitches[j][0];
        nRefBPitches[j][2]  = nRefBPitches[j][0];
      }
    }
    /*
		if (isUsableB)
		{
			pRefB[0] = refB->GetReadPtr();
			pRefB[1] = pRefB[0] + refB->GetRowSize()/2;
			pRefB[2] = pRefB[1] + refB->GetRowSize()/4;
			nRefBPitches[0]  = refB->GetPitch();
			nRefBPitches[1]  = nRefBPitches[0];
			nRefBPitches[2]  = nRefBPitches[0];
		}
		if (isUsableB2)
		{
			pRefB2[0] = refB2->GetReadPtr();
			pRefB2[1] = pRefB2[0] + refB2->GetRowSize()/2;
			pRefB2[2] = pRefB2[1] + refB2->GetRowSize()/4;
			nRefB2Pitches[0]  = refB2->GetPitch();
			nRefB2Pitches[1]  = nRefB2Pitches[0];
			nRefB2Pitches[2]  = nRefB2Pitches[0];
		}
		if (isUsableB3)
		{
			pRefB3[0] = refB3->GetReadPtr();
			pRefB3[1] = pRefB3[0] + refB3->GetRowSize()/2;
			pRefB3[2] = pRefB3[1] + refB3->GetRowSize()/4;
			nRefB3Pitches[0]  = refB3->GetPitch();
			nRefB3Pitches[1]  = nRefB3Pitches[0];
			nRefB3Pitches[2]  = nRefB3Pitches[0];
		}
    */
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
    /*
    if (isUsableF3)
		{
			pRefF3[0] = YRPLAN(refF3);
			pRefF3[1] = URPLAN(refF3);
			pRefF3[2] = VRPLAN(refF3);
			nRefF3Pitches[0] = YPITCH(refF3);
			nRefF3Pitches[1] = UPITCH(refF3);
			nRefF3Pitches[2] = VPITCH(refF3);
		}
		if (isUsableF2)
		{
			pRefF2[0] = YRPLAN(refF2);
			pRefF2[1] = URPLAN(refF2);
			pRefF2[2] = VRPLAN(refF2);
			nRefF2Pitches[0] = YPITCH(refF2);
			nRefF2Pitches[1] = UPITCH(refF2);
			nRefF2Pitches[2] = VPITCH(refF2);
		}
		if (isUsableF)
		{
			pRefF[0] = YRPLAN(refF);
			pRefF[1] = URPLAN(refF);
			pRefF[2] = VRPLAN(refF);
			nRefFPitches[0] = YPITCH(refF);
			nRefFPitches[1] = UPITCH(refF);
			nRefFPitches[2] = VPITCH(refF);
		}
    */
    for (int j = 0; j<level;j++)
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
    /*
		if (isUsableB)
		{
			pRefB[0] = YRPLAN(refB);
			pRefB[1] = URPLAN(refB);
			pRefB[2] = VRPLAN(refB);
			nRefBPitches[0] = YPITCH(refB);
			nRefBPitches[1] = UPITCH(refB);
			nRefBPitches[2] = VPITCH(refB);
		}
		if (isUsableB2)
		{
			pRefB2[0] = YRPLAN(refB2);
			pRefB2[1] = URPLAN(refB2);
			pRefB2[2] = VRPLAN(refB2);
			nRefB2Pitches[0] = YPITCH(refB2);
			nRefB2Pitches[1] = UPITCH(refB2);
			nRefB2Pitches[2] = VPITCH(refB2);
		}
		if (isUsableB3)
		{
			pRefB3[0] = YRPLAN(refB3);
			pRefB3[1] = URPLAN(refB3);
			pRefB3[2] = VRPLAN(refB3);
			nRefB3Pitches[0] = YPITCH(refB3);
			nRefB3Pitches[1] = UPITCH(refB3);
			nRefB3Pitches[2] = VPITCH(refB3);
		}
    */
	}

	MVPlane *pPlanesB[3][MAX_DEGRAIN]  = { 0 };
   MVPlane *pPlanesF[3][MAX_DEGRAIN]  = { 0 };
   /*
   MVPlane *pPlanesB2[3] = { 0 };
   MVPlane *pPlanesF2[3] = { 0 };
   MVPlane *pPlanesB3[3] = { 0 };
   MVPlane *pPlanesF3[3] = { 0 };
   */
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
   /*
	if (isUsableF3)
	{
		pRefF3GOF->Update(YUVplanes, (BYTE*)pRefF3[0], nRefF3Pitches[0], (BYTE*)pRefF3[1], nRefF3Pitches[1], (BYTE*)pRefF3[2], nRefF3Pitches[2]);
		if (YUVplanes & YPLANE)
			pPlanesF3[0] = pRefF3GOF->GetFrame(0)->GetPlane(YPLANE);
		if (YUVplanes & UPLANE)
			pPlanesF3[1] = pRefF3GOF->GetFrame(0)->GetPlane(UPLANE);
		if (YUVplanes & VPLANE)
			pPlanesF3[2] = pRefF3GOF->GetFrame(0)->GetPlane(VPLANE);
	}
	if (isUsableF2)
	{
		pRefF2GOF->Update(YUVplanes, (BYTE*)pRefF2[0], nRefF2Pitches[0], (BYTE*)pRefF2[1], nRefF2Pitches[1], (BYTE*)pRefF2[2], nRefF2Pitches[2]);
		if (YUVplanes & YPLANE)
			pPlanesF2[0] = pRefF2GOF->GetFrame(0)->GetPlane(YPLANE);
		if (YUVplanes & UPLANE)
			pPlanesF2[1] = pRefF2GOF->GetFrame(0)->GetPlane(UPLANE);
		if (YUVplanes & VPLANE)
			pPlanesF2[2] = pRefF2GOF->GetFrame(0)->GetPlane(VPLANE);
	}
	if (isUsableF)
	{
		pRefFGOF->Update(YUVplanes, (BYTE*)pRefF[0], nRefFPitches[0], (BYTE*)pRefF[1], nRefFPitches[1], (BYTE*)pRefF[2], nRefFPitches[2]);
		if (YUVplanes & YPLANE)
			pPlanesF[0] = pRefFGOF->GetFrame(0)->GetPlane(YPLANE);
		if (YUVplanes & UPLANE)
			pPlanesF[1] = pRefFGOF->GetFrame(0)->GetPlane(UPLANE);
		if (YUVplanes & VPLANE)
			pPlanesF[2] = pRefFGOF->GetFrame(0)->GetPlane(VPLANE);
	}
  */
   for (int j = 0; j<level;j++)
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
   /*
	if (isUsableB)
	{
		pRefBGOF->Update(YUVplanes, (BYTE*)pRefB[0], nRefBPitches[0], (BYTE*)pRefB[1], nRefBPitches[1], (BYTE*)pRefB[2], nRefBPitches[2]);// v2.0
		if (YUVplanes & YPLANE)
			pPlanesB[0] = pRefBGOF->GetFrame(0)->GetPlane(YPLANE);
		if (YUVplanes & UPLANE)
			pPlanesB[1] = pRefBGOF->GetFrame(0)->GetPlane(UPLANE);
		if (YUVplanes & VPLANE)
			pPlanesB[2] = pRefBGOF->GetFrame(0)->GetPlane(VPLANE);
	}
	if (isUsableB2)
	{
		pRefB2GOF->Update(YUVplanes, (BYTE*)pRefB2[0], nRefB2Pitches[0], (BYTE*)pRefB2[1], nRefB2Pitches[1], (BYTE*)pRefB2[2], nRefB2Pitches[2]);// v2.0
		if (YUVplanes & YPLANE)
			pPlanesB2[0] = pRefB2GOF->GetFrame(0)->GetPlane(YPLANE);
		if (YUVplanes & UPLANE)
			pPlanesB2[1] = pRefB2GOF->GetFrame(0)->GetPlane(UPLANE);
		if (YUVplanes & VPLANE)
			pPlanesB2[2] = pRefB2GOF->GetFrame(0)->GetPlane(VPLANE);
	}
	if (isUsableB3)
	{
		pRefB3GOF->Update(YUVplanes, (BYTE*)pRefB3[0], nRefB3Pitches[0], (BYTE*)pRefB3[1], nRefB3Pitches[1], (BYTE*)pRefB3[2], nRefB3Pitches[2]);// v2.0
		if (YUVplanes & YPLANE)
			pPlanesB3[0] = pRefB3GOF->GetFrame(0)->GetPlane(YPLANE);
		if (YUVplanes & UPLANE)
			pPlanesB3[1] = pRefB3GOF->GetFrame(0)->GetPlane(UPLANE);
		if (YUVplanes & VPLANE)
			pPlanesB3[2] = pRefB3GOF->GetFrame(0)->GetPlane(VPLANE);
	}
  */

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
		BitBlt(pDstCur[0], nDstPitches[0], pSrcCur[0], nSrcPitches[0], nWidth*pixelsize_super, nHeight, isse2);
	}

	else
	{
		if (nOverlapX==0 && nOverlapY==0)
		{
			for (int by=0; by<nBlkY; by++)
			{
				int xx = 0; // logical offset. Mul by 2 for pixelsize_super==2. Don't mul for indexing int* array
				for (int bx=0; bx<nBlkX; bx++)
				{
					int i = by*nBlkX + bx;
          const BYTE * pB[MAX_DEGRAIN], *pF[MAX_DEGRAIN]; // , *pB2, *pF2, *pB3, *pF3;
          int npB[MAX_DEGRAIN], npF[MAX_DEGRAIN]; // , npB2, npF2, npB3, npF3;
          int WSrc;
          int WRefB[MAX_DEGRAIN], WRefF[MAX_DEGRAIN]; // , WRefB2, WRefF2, WRefB3, WRefF3;

          for (int j = 0; j < level; j++) {
            use_block_y (pB[j] , npB[j] , WRefB[j] , isUsableB[j] , *mvClipB[j] , i, pPlanesB[0][j], pSrcCur [0], xx*pixelsize_super, nSrcPitches [0]);
            use_block_y (pF[j] , npF[j] , WRefF[j] , isUsableF[j] , *mvClipF[j] , i, pPlanesF[0][j], pSrcCur [0], xx*pixelsize_super, nSrcPitches [0]);
          }
          /*
					use_block_y (pB , npB , WRefB , isUsableB , mvClipB , i, pPlanesB  [0], pSrcCur [0], xx, nSrcPitches [0]);
					use_block_y (pF , npF , WRefF , isUsableF , mvClipF , i, pPlanesF  [0], pSrcCur [0], xx, nSrcPitches [0]);
					use_block_y (pB2, npB2, WRefB2, isUsableB2, mvClipB2, i, pPlanesB2 [0], pSrcCur [0], xx, nSrcPitches [0]);
					use_block_y (pF2, npF2, WRefF2, isUsableF2, mvClipF2, i, pPlanesF2 [0], pSrcCur [0], xx, nSrcPitches [0]);
					use_block_y (pB3, npB3, WRefB3, isUsableB3, mvClipB3, i, pPlanesB3 [0], pSrcCur [0], xx, nSrcPitches [0]);
					use_block_y (pF3, npF3, WRefF3, isUsableF3, mvClipF3, i, pPlanesF3 [0], pSrcCur [0], xx, nSrcPitches [0]);
          */
          if(level==1)
					  norm_weights<1>(WSrc, WRefB, WRefF);
          else if(level==2)
            norm_weights<2>(WSrc, WRefB, WRefF);
          else if(level==3)
            norm_weights<3>(WSrc, WRefB, WRefF);
          else if(level==4)
            norm_weights<4>(WSrc, WRefB, WRefF);
          else if(level==5)
            norm_weights<5>(WSrc, WRefB, WRefF);

					// luma
					DEGRAINLUMA(pDstCur[0] + xx*pixelsize_super, pDstCur[0] + lsb_offset_y + xx*pixelsize_super,
						lsb_flag, nDstPitches[0], pSrcCur[0]+ xx*pixelsize_super, nSrcPitches[0],
						pB, npB, pF, npF, //pB2, npB2, pF2, npF2, pB3, npB3, pF3, npF3,
						WSrc, WRefB, WRefF //, WRefB2, WRefF2, WRefB3, WRefF3
          );

					xx += nBlkSizeX; // xx: indexing offset

					if (bx == nBlkX-1 && nWidth_B < nWidth) // right non-covered region
					{
						// luma
						BitBlt(pDstCur[0] + nWidth_B*pixelsize_super, nDstPitches[0],
							pSrcCur[0] + nWidth_B*pixelsize_super, nSrcPitches[0], (nWidth-nWidth_B)*pixelsize_super, nBlkSizeY, isse2);
					}
				}	// for bx

				pDstCur[0] += ( nBlkSizeY ) * (nDstPitches[0]);
				pSrcCur[0] += ( nBlkSizeY ) * (nSrcPitches[0]);

				if (by == nBlkY-1 && nHeight_B < nHeight) // bottom uncovered region
				{
					// luma
					BitBlt(pDstCur[0], nDstPitches[0], pSrcCur[0], nSrcPitches[0], nWidth*pixelsize_super, nHeight-nHeight_B, isse2);
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
					nWidth_B*sizeof(int), nHeight_B, 0, 0, dstIntPitch*sizeof(int));
			}
			else
			{
				MemZoneSet(reinterpret_cast<unsigned char*>(pDstShort), 0,
					nWidth_B*sizeof(short), nHeight_B, 0, 0, dstShortPitch*sizeof(short));
			}

			for (int by=0; by<nBlkY; by++)
			{
				int wby = ((by + nBlkY - 3)/(nBlkY - 2))*3;
				int xx = 0; // logical offset. Mul by 2 for pixelsize_super==2. Don't mul for indexing int* array
				for (int bx=0; bx<nBlkX; bx++)
				{
					// select window
					int wbx = (bx + nBlkX - 3)/(nBlkX - 2);
					short *			winOver = OverWins->GetWindow(wby + wbx);

					int i = by*nBlkX + bx;

          const BYTE * pB[MAX_DEGRAIN], *pF[MAX_DEGRAIN]; // , *pB2, *pF2, *pB3, *pF3;
          int npB[MAX_DEGRAIN], npF[MAX_DEGRAIN]; // , npB2, npF2, npB3, npF3;
          int WSrc;
          int WRefB[MAX_DEGRAIN], WRefF[MAX_DEGRAIN]; // , WRefB2, WRefF2, WRefB3, WRefF3;

          for (int j = 0; j < level; j++) {
            use_block_y (pB[j] , npB[j] , WRefB[j] , isUsableB[j] , *mvClipB[j] , i, pPlanesB[0][j], pSrcCur [0], xx*pixelsize_super, nSrcPitches [0]);
            use_block_y (pF[j] , npF[j] , WRefF[j] , isUsableF[j] , *mvClipF[j] , i, pPlanesF[0][j], pSrcCur [0], xx*pixelsize_super, nSrcPitches [0]);
          }
          if(level==1)
            norm_weights<1>(WSrc, WRefB, WRefF);
          else if(level==2)
            norm_weights<2>(WSrc, WRefB, WRefF);
          else if(level==3)
            norm_weights<3>(WSrc, WRefB, WRefF);
          else if(level==4)
            norm_weights<4>(WSrc, WRefB, WRefF);
          else if(level==5)
            norm_weights<5>(WSrc, WRefB, WRefF);
          /*
					use_block_y (pB , npB , WRefB , isUsableB , mvClipB , i, pPlanesB  [0], pSrcCur [0], xx, nSrcPitches [0]);
					use_block_y (pF , npF , WRefF , isUsableF , mvClipF , i, pPlanesF  [0], pSrcCur [0], xx, nSrcPitches [0]);
					use_block_y (pB2, npB2, WRefB2, isUsableB2, mvClipB2, i, pPlanesB2 [0], pSrcCur [0], xx, nSrcPitches [0]);
					use_block_y (pF2, npF2, WRefF2, isUsableF2, mvClipF2, i, pPlanesF2 [0], pSrcCur [0], xx, nSrcPitches [0]);
					use_block_y (pB3, npB3, WRefB3, isUsableB3, mvClipB3, i, pPlanesB3 [0], pSrcCur [0], xx, nSrcPitches [0]);
					use_block_y (pF3, npF3, WRefF3, isUsableF3, mvClipF3, i, pPlanesF3 [0], pSrcCur [0], xx, nSrcPitches [0]);
					norm_weights (WSrc, WRefB, WRefF, WRefB2, WRefF2, WRefB3, WRefF3);
*/
					// luma
					DEGRAINLUMA(tmpBlock, tmpBlockLsb, lsb_flag, tmpPitch*pixelsize_super, pSrcCur[0]+ xx*pixelsize_super, nSrcPitches[0],
						pB, npB, pF, npF, // pB2, npB2, pF2, npF2, pB3, npB3, pF3, npF3,
						WSrc, 
            WRefB, WRefF
            //, WRefB2, WRefF2, WRefB3, WRefF3
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
            OVERSLUMA16((uint16_t *)(pDstInt + xx), dstIntPitch, tmpBlock, tmpPitch*pixelsize_super, winOver, nBlkSizeX);
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
			else if(pixelsize_super == 1)
			{
				Short2Bytes(pDst[0], nDstPitches[0], DstShort, dstShortPitch, nWidth_B, nHeight_B);
			} 
      else if(pixelsize_super == 2) 
      {
        Short2Bytes16((uint16_t *)(pDst[0]), pDst[0] + lsb_offset_y, nDstPitches[0], DstInt, dstIntPitch, nWidth_B, nHeight_B, bits_per_pixel_super);
      }
      if (nWidth_B < nWidth)
			{
				BitBlt(pDst[0] + nWidth_B*pixelsize_super, nDstPitches[0],
					pSrc[0] + nWidth_B*pixelsize_super, nSrcPitches[0],
					(nWidth-nWidth_B)*pixelsize_super, nHeight_B, isse2);
			}
			if (nHeight_B < nHeight) // bottom noncovered region
			{
				BitBlt(pDst[0] + nHeight_B*nDstPitches[0], nDstPitches[0],
					pSrc[0] + nHeight_B*nSrcPitches[0], nSrcPitches[0],
					nWidth*pixelsize_super, nHeight-nHeight_B, isse2);
			}
		}	// overlap - end

		if (nLimit < (1 << bits_per_pixel_super)-1)
		{
			if ((pixelsize_super==1) && isse2)
			{
				LimitChanges_sse2(pDst[0], nDstPitches[0], pSrc[0], nSrcPitches[0], nWidth, nHeight, nLimit);
			}
			else
			{
                if(pixelsize_super==1)
                    LimitChanges_c<uint8_t>(pDst[0], nDstPitches[0], pSrc[0], nSrcPitches[0], nWidth, nHeight, nLimit);
                else
                    LimitChanges_c<uint16_t>(pDst[0], nDstPitches[0], pSrc[0], nSrcPitches[0], nWidth, nHeight, nLimit);
            }
		}
	}

//----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// CHROMA plane U

	process_chroma (
		UPLANE & nSuperModeYUV,
		pDst [1], pDstCur [1], nDstPitches [1], pSrc [1], pSrcCur [1], nSrcPitches [1],
		isUsableB, isUsableF, // isUsableB2, isUsableF2, isUsableB3, isUsableF3,
		pPlanesB[1], pPlanesF[1], //, pPlanesB2 [1], pPlanesF2 [1], pPlanesB3 [1], pPlanesF3 [1],
		lsb_offset_u, nWidth_B, nHeight_B
	);

//----------------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// CHROMA plane V

	process_chroma (
		VPLANE & nSuperModeYUV,
		pDst [2], pDstCur [2], nDstPitches [2], pSrc [2], pSrcCur [2], nSrcPitches [2],
		isUsableB, isUsableF, //, isUsableB2, isUsableF2, isUsableB3, isUsableF3,
		pPlanesB [2], pPlanesF [2], //, pPlanesB2 [2], pPlanesF2 [2], pPlanesB3 [2], pPlanesF3 [2],
		lsb_offset_v, nWidth_B, nHeight_B
	);

//--------------------------------------------------------------------------------

#ifndef _M_X64
  _mm_empty ();	// (we may use double-float somewhere) Fizick
#endif

	PROFILE_STOP(MOTION_PROFILE_COMPENSATION);

	if ( (pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2 && !planar)
	{
		YUY2FromPlanes(pDstYUY2, nDstPitchYUY2, nWidth, nHeight * height_lsb_mul,
		pDst[0], nDstPitches[0], pDst[1], pDst[2], nDstPitches[1], isse2);
	}

	return dst;
}



void	MVDegrainX::process_chroma(int plane_mask, BYTE *pDst, BYTE *pDstCur, int nDstPitch, const BYTE *pSrc, const BYTE *pSrcCur, int nSrcPitch,
  bool isUsableB[MAX_DEGRAIN], bool isUsableF[MAX_DEGRAIN], MVPlane *pPlanesB[MAX_DEGRAIN], MVPlane *pPlanesF[MAX_DEGRAIN],
  int lsb_offset_uv, int nWidth_B, int nHeight_B)
{
  if (!(YUVplanes & plane_mask))
  {
    BitBlt(pDstCur, nDstPitch, pSrcCur, nSrcPitch, nWidth / xRatioUV*pixelsize_super, nHeight / yRatioUV, isse2);
  }

  else
  {
    if (nOverlapX == 0 && nOverlapY == 0)
    {
      for (int by = 0; by < nBlkY; by++)
      {
        int xx = 0; // byte granularity
        for (int bx = 0; bx < nBlkX; bx++)
        {
          int i = by*nBlkX + bx;
          const BYTE * pBV[MAX_DEGRAIN], *pFV[MAX_DEGRAIN]; // , *pB2V, *pF2V, *pB3V, *pF3V;
          int npBV[MAX_DEGRAIN], npFV[MAX_DEGRAIN]; // , npB2V, npF2V, npB3V, npF3V;
          int WSrc, WRefB[MAX_DEGRAIN], WRefF[MAX_DEGRAIN]; // , WRefB2, WRefF2, WRefB3, WRefF3;

          for (int j = 0; j < level; j++) {
            // xx: byte granularity pointer shift
            use_block_uv(pBV[j], npBV[j], WRefB[j], isUsableB[j], *mvClipB[j], i, pPlanesB[j], pSrcCur, xx, nSrcPitch);
            use_block_uv(pFV[j], npFV[j], WRefF[j], isUsableF[j], *mvClipF[j], i, pPlanesF[j], pSrcCur, xx, nSrcPitch);
          }
          if (level == 1)
            norm_weights<1>(WSrc, WRefB, WRefF);
          else if (level == 2)
            norm_weights<2>(WSrc, WRefB, WRefF);
          else if (level == 3)
            norm_weights<3>(WSrc, WRefB, WRefF);
          else if(level==4)
            norm_weights<4>(WSrc, WRefB, WRefF);
          else if(level==5)
            norm_weights<5>(WSrc, WRefB, WRefF);

          // chroma
          DEGRAINCHROMA(pDstCur + (xx / xRatioUV), pDstCur + (xx / xRatioUV) + lsb_offset_uv,
            lsb_flag, nDstPitch, pSrcCur + (xx / xRatioUV), nSrcPitch,
            pBV, npBV, pFV, npFV, //pB2V, npB2V, pF2V, npF2V, pB3V, npB3V, pF3V, npF3V,
            WSrc, WRefB, WRefF //, WRefB2, WRefF2, WRefB3, WRefF3
          );

          xx += nBlkSizeX*pixelsize_super;

          if (bx == nBlkX - 1 && nWidth_B < nWidth) // right non-covered region
          {
            // chroma
            BitBlt(pDstCur + (nWidth_B / xRatioUV)*pixelsize_super, nDstPitch,
              pSrcCur + (nWidth_B / xRatioUV)*pixelsize_super, nSrcPitch, (nWidth - nWidth_B) / xRatioUV*pixelsize_super, (nBlkSizeY) / yRatioUV, isse2);
          }
        }	// for bx

        pDstCur += (nBlkSizeY) / yRatioUV * nDstPitch;
        pSrcCur += (nBlkSizeY) / yRatioUV * nSrcPitch;

        if (by == nBlkY - 1 && nHeight_B < nHeight) // bottom uncovered region
        {
          // chroma
          BitBlt(pDstCur, nDstPitch, pSrcCur, nSrcPitch, nWidth / xRatioUV*pixelsize_super, (nHeight - nHeight_B) / yRatioUV, isse2);
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
          nWidth_B * sizeof(int) / xRatioUV, nHeight_B / yRatioUV, 0, 0, dstIntPitch * sizeof(int));
      }
      else
      {
        MemZoneSet(reinterpret_cast<unsigned char*>(pDstShort), 0,
          nWidth_B * sizeof(short) / xRatioUV, nHeight_B / yRatioUV, 0, 0, dstShortPitch * sizeof(short));
      }

      for (int by = 0; by < nBlkY; by++)
      {
        int wby = ((by + nBlkY - 3) / (nBlkY - 2)) * 3;
        int xx = 0;
        for (int bx = 0; bx < nBlkX; bx++)
        {
          // select window
          int wbx = (bx + nBlkX - 3) / (nBlkX - 2);
          short *			winOverUV = OverWinsUV->GetWindow(wby + wbx);

          int i = by*nBlkX + bx;
          const BYTE * pBV[MAX_DEGRAIN], *pFV[MAX_DEGRAIN]; // , *pB2V, *pF2V, *pB3V, *pF3V;
          int npBV[MAX_DEGRAIN], npFV[MAX_DEGRAIN]; // , npB2V, npF2V, npB3V, npF3V;
          int WSrc, WRefB[MAX_DEGRAIN], WRefF[MAX_DEGRAIN]; // , WRefB2, WRefF2, WRefB3, WRefF3;

          for (int j = 0; j < level; j++) {
            use_block_uv(pBV[j], npBV[j], WRefB[j], isUsableB[j], *mvClipB[j], i, pPlanesB[j], pSrcCur, xx, nSrcPitch);
            use_block_uv(pFV[j], npFV[j], WRefF[j], isUsableF[j], *mvClipF[j], i, pPlanesF[j], pSrcCur, xx, nSrcPitch);
          }
          if (level == 1)
            norm_weights<1>(WSrc, WRefB, WRefF);
          else if (level == 2)
            norm_weights<2>(WSrc, WRefB, WRefF);
          else if (level == 3)
            norm_weights<3>(WSrc, WRefB, WRefF);
          else if(level==4)
            norm_weights<4>(WSrc, WRefB, WRefF);
          else if(level==5)
            norm_weights<5>(WSrc, WRefB, WRefF);

          // chroma
          DEGRAINCHROMA(tmpBlock, tmpBlockLsb, lsb_flag, tmpPitch*pixelsize_super, pSrcCur + (xx / xRatioUV)*pixelsize_super, nSrcPitch,
            pBV, npBV, pFV, npFV, //pB2V, npB2V, pF2V, npF2V, pB3V, npB3V, pF3V, npF3V,
            WSrc, WRefB, WRefF //, WRefB2, WRefF2, WRefB3, WRefF3
          );
          if (lsb_flag)
          { // no pixelsize here pointers Int or short
            OVERSCHROMALSB(pDstInt + (xx / xRatioUV), dstIntPitch, tmpBlock, tmpBlockLsb, tmpPitch, winOverUV, nBlkSizeX / xRatioUV);
          }
          else if (pixelsize_super==1)
          {
            OVERSCHROMA(pDstShort + (xx / xRatioUV), dstShortPitch, tmpBlock, tmpPitch, winOverUV, nBlkSizeX / xRatioUV);
          }
          else if (pixelsize_super==2)
          {
            OVERSCHROMA16((uint16_t*)(pDstInt + (xx / xRatioUV)), dstIntPitch, tmpBlock, tmpPitch*pixelsize_super, winOverUV, nBlkSizeX / xRatioUV);
          }

          xx += (nBlkSizeX - nOverlapX); // no pixelsize here

        }	// for bx

        pSrcCur += (nBlkSizeY - nOverlapY) / yRatioUV * nSrcPitch; // pitch is byte granularity
        pDstShort += (nBlkSizeY - nOverlapY) / yRatioUV * dstShortPitch; // pitch is short granularity
        pDstInt += (nBlkSizeY - nOverlapY) / yRatioUV * dstIntPitch; // pitch is int granularity
      }	// for by

      if (lsb_flag)
      { 
        Short2BytesLsb(pDst, pDst + lsb_offset_uv, nDstPitch, DstInt, dstIntPitch, nWidth_B / xRatioUV, nHeight_B / yRatioUV);
      }
      else if(pixelsize_super==1)
      { // pixelsize
        Short2Bytes(pDst, nDstPitch, DstShort, dstShortPitch, nWidth_B / xRatioUV, nHeight_B / yRatioUV);
      }
      else if(pixelsize_super==2)
      { // pixelsize
        Short2Bytes16((uint16_t *)(pDst), pDst + lsb_offset_uv, nDstPitch, DstInt, dstIntPitch, nWidth_B / xRatioUV, nHeight_B / yRatioUV, bits_per_pixel_super);
      }
      if (nWidth_B < nWidth)
      {
        BitBlt(pDst + (nWidth_B / xRatioUV)*pixelsize_super, nDstPitch,
          pSrc + (nWidth_B / xRatioUV)*pixelsize_super, nSrcPitch,
          (nWidth - nWidth_B) / xRatioUV*pixelsize_super, nHeight_B / yRatioUV, isse2);
      }
      if (nHeight_B < nHeight) // bottom noncovered region
      {
        BitBlt(pDst + nDstPitch*nHeight_B / yRatioUV, nDstPitch,
          pSrc + nSrcPitch*nHeight_B / yRatioUV, nSrcPitch,
          nWidth / xRatioUV*pixelsize_super, (nHeight - nHeight_B) / yRatioUV, isse2);
      }
    }	// overlap - end

    if (nLimitC < (1 << bits_per_pixel_super) - 1)
    {
      if ((pixelsize_super == 1) && isse2)
      {
        LimitChanges_sse2(pDst, nDstPitch, pSrc, nSrcPitch, nWidth / xRatioUV, nHeight / yRatioUV, nLimitC);
      }
      else
      {
        if (pixelsize_super == 1)
          LimitChanges_c<uint8_t>(pDst, nDstPitch, pSrc, nSrcPitch, nWidth / xRatioUV, nHeight / yRatioUV, nLimitC);
        else
          LimitChanges_c<uint16_t>(pDst, nDstPitch, pSrc, nSrcPitch, nWidth / xRatioUV, nHeight / yRatioUV, nLimitC);
      }
    }
  }
}


// no difference for 1-2-3
void	MVDegrainX::use_block_y (const BYTE * &p, int &np, int &WRef, bool isUsable, const MVClip &mvclip, int i, const MVPlane *pPlane, const BYTE *pSrcCur, int xx, int nSrcPitch)
{
	if (isUsable)
	{
		const FakeBlockData &block = mvclip.GetBlock(0, i);
		int blx = block.GetX() * nPel + block.GetMV().x;
		int bly = block.GetY() * nPel + block.GetMV().y;
		p = pPlane->GetPointer(blx, bly);
		np = pPlane->GetPitch();
		int blockSAD = block.GetSAD(); // SAD of MV Block. Scaled to MVClip's bits_per_pixel;
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
void	MVDegrainX::use_block_uv (const BYTE * &p, int &np, int &WRef, bool isUsable, const MVClip &mvclip, int i, const MVPlane *pPlane, const BYTE *pSrcCur, int xx, int nSrcPitch)
{
	if (isUsable)
	{
		const FakeBlockData &block = mvclip.GetBlock(0, i);
		int blx = block.GetX() * nPel + block.GetMV().x;
		int bly = block.GetY() * nPel + block.GetMV().y;
		p = pPlane->GetPointer(blx/xRatioUV, bly/yRatioUV); // pixelsize - aware
		np = pPlane->GetPitch();
		int blockSAD = block.GetSAD();  // SAD of MV Block. Scaled to MVClip's bits_per_pixel;
		WRef = DegrainWeight(thSADC, blockSAD, bits_per_pixel);
	}
	else
	{
		p = pSrcCur + (xx/xRatioUV); // xx:byte offset
		np = nSrcPitch;
		WRef = 0;
	}
}



template<int level>
void	MVDegrainX::norm_weights (int &WSrc, int (&WRefB)[MAX_DEGRAIN], int (&WRefF)[MAX_DEGRAIN])
{
	WSrc = 256;
  int WSum;
  if (level==5)
    WSum = WRefB[0] + WRefF[0] + WSrc + WRefB[1] + WRefF[1] + WRefB[2] + WRefF[2] + WRefB[3] + WRefF[3] + WRefB[4] + WRefF[4] +1;
  if (level==4)
    WSum = WRefB[0] + WRefF[0] + WSrc + WRefB[1] + WRefF[1] + WRefB[2] + WRefF[2] + WRefB[3] + WRefF[3] +1;
  if (level==3)
    WSum = WRefB[0] + WRefF[0] + WSrc + WRefB[1] + WRefF[1] + WRefB[2] + WRefF[2] + 1;
  else if(level==2)
    WSum = WRefB[0] + WRefF[0] + WSrc + WRefB[1] + WRefF[1] + 1;
  else if(level==1)
    WSum = WRefB[0] + WRefF[0] + WSrc + 1;
  WRefB[0]  = WRefB[0]*256/WSum; // normalize weights to 256
	WRefF[0]  = WRefF[0]*256/WSum;
  if(level>=2) {
    WRefB[1]  = WRefB[1]*256/WSum; // normalize weights to 256
    WRefF[1]  = WRefF[1]*256/WSum;
  }
  if(level>=3) {
    WRefB[2]  = WRefB[2]*256/WSum; // normalize weights to 256
    WRefF[2]  = WRefF[2]*256/WSum;
  }
  if(level>=4) {
    WRefB[3]  = WRefB[3]*256/WSum; // normalize weights to 256
    WRefF[3]  = WRefF[3]*256/WSum;
  }
  if(level>=5) {
    WRefB[4]  = WRefB[4]*256/WSum; // normalize weights to 256
    WRefF[4]  = WRefF[4]*256/WSum;
  }
  if (level == 5)
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
