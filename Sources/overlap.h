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

#ifndef __OVERLAP__
#define __OVERLAP__

#include <map>
#include <tuple>
#include "CopyCode.h"
#include <cstdio>
#include "def.h"
#include "emmintrin.h"

// top, middle, bottom and left, middle, right windows
#define OW_TL 0
#define OW_TM 1
#define OW_TR 2
#define OW_ML 3
#define OW_MM 4
#define OW_MR 5
#define OW_BL 6
#define OW_BM 7
#define OW_BR 8

class OverlapWindows
{
  int nx; // window sizes
  int ny;
  int ox; // overap sizes
  int oy;
  int size; // full window size= nx*ny

  short * Overlap9Windows;
  float * Overlap9WindowsF;

  float *fWin1UVx;
  float *fWin1UVxfirst;
  float *fWin1UVxlast;
  float *fWin1UVy;
  float *fWin1UVyfirst;
  float *fWin1UVylast;
public :

  OverlapWindows(int _nx, int _ny, int _ox, int _oy);
   ~OverlapWindows();

   MV_FORCEINLINE int Getnx() const { return nx; }
   MV_FORCEINLINE int Getny() const { return ny; }
   MV_FORCEINLINE int GetSize() const { return size; }
   MV_FORCEINLINE short *GetWindow(int i) const { return Overlap9Windows + size*i; }
   MV_FORCEINLINE float* GetWindowF(int i) const { return Overlap9WindowsF + size * i; }
};

typedef void (OverlapsFunction)(uint16_t *pDst, int nDstPitch,
                            const unsigned char *pSrc, int nSrcPitch,
              short *pWin, int nWinPitch);
typedef void (OverlapsLsbFunction)(int *pDst, int nDstPitch, const unsigned char *pSrc, const unsigned char *pSrcLsb, int nSrcPitch, short *pWin, int nWinPitch);

OverlapsFunction* get_overlaps_function(int BlockX, int BlockY, int pixelsize, bool out32, arch_t arch);
OverlapsLsbFunction* get_overlaps_lsb_function(int BlockX, int BlockY, int pixelsize, arch_t arch);

//=============================================================
// short

template <typename pixel_t, int blockWidth, int blockHeight>
// pDst is short* for 8 bit, int * for 16 bit sources
void Overlaps_C(uint16_t *pDst0, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch)
{
  // pWin from 0 to 2048 total shift 11
  // when pixel_t == uint16_t, dst should be int*
  typedef typename std::conditional < sizeof(pixel_t) == 1, short, int>::type target_t;
  target_t *pDst = reinterpret_cast<target_t *>(pDst0);

#if 0
  // debug
  int s[blockWidth][blockHeight];
  int w[blockWidth][blockHeight];
  for (int j = 0; j < blockHeight; j++) {
    for (int i = 0; i < blockWidth; i++)
    {
      s[i][j] = reinterpret_cast<const pixel_t *>(pSrc + j * nSrcPitch)[i];
      w[i][j] = (pWin + j * nWinPitch)[i];
    }
  }
#endif

  // pWin: Cos^2 based weigthing factors with 11 bits precision. (0-2048)
  // SourceBlocks overlap sample for Blocksize=4 Overlaps=2:
  // a1     a1   | a1+b1         a1+b1       | b1      b1    | b1+c1         b1+c1       | c1       c1   
  // a1     a1   | a1+b1         a1+b1       | b1      b1    | b1+c1         b1+c1       | c1      c1   
  // ------------+---------------------------+---------------+---------------------------+--------------
  // a1+a2  a1+a2| a1+a2+b1+b2   a1+a2+b1+b2 | b1+b2   b1+b2 | b1+b2+c1+c2   b1+b2+c1+c2 | c1+c2   c1+c2
  // a1+a2  a1+a2| a1+a2+b1+b2   a1+a2+b1+b2 | b1+b2   b1+b2 | b1+b2+c1+c2   b1+b2+c1+c2 | c1+c2   c1+c2
  // ------------+---------------------------+---------------+---------------------------+--------------
  // a2+a3  a2+a3| a2+a3+b2+b3   a2+a3+b2+b3 | b2+b3   b2+b3 | b2+b3+c2+c3   b2+b3+c2+c3 | c2+c3   c2+c3
  // a2+a3  a2+a3| a2+a3+b2+b3   a2+a3+b2+b3 | b2+b3   b2+b3 | b2+b3+c2+c3   b2+b3+c2+c3 | c2+c3   c2+c3
  // ...
  for (int j=0; j<blockHeight; j++)
  {
      for (int i=0; i<blockWidth; i++)
      {
        int val = reinterpret_cast<const pixel_t *>(pSrc)[i];
        short win = pWin[i];
        if constexpr(sizeof(pixel_t) == 1) {
          // Original, this calculation appears also in Overlap-a.asm:
          // pDst[i] = ( pDst[i] + ((reinterpret_cast<const pixel_t *>(pSrc)[i]*pWin[i] + 256)>> 6)); // see rest shift 5 in Short2Bytes<uint8_t> in overlap.cpp
          // PF 180216 Why is rounding 256 (1<<8) and not 32 (1<<5) ?
          // This const 256 appears in the asm code, is it copy/paste?
          // +256: +0.125 plus on average
          // Original: [[Sum(x + (1<<8)) >> 6]         ] >> 5
          // Modified: [[Sum(x + (1<<5)) >> 6] + (1<<4)] >> 5
          // 16 bit  :  [Sum(x         )     ] + (1<<10)] >> 11
          pDst[i] = pDst[i] + ((val * win + (1 << 5)/*256*/) >> 6);
          // Reason of the partial shift: here we have to escape from the >=16bit range.
          // 8+11 = 19 bits intermediate, too much -> 19>>6 = 13bit is O.K.
          // Finally a shift of 5 bits makes 13 bits to 8 bits in ShortToBytes.
        }
        else {
          // 16 bits data + 11 bits window = 27 bits max (safe)
          // shift 11 in Short2Bytes16
          int a = pDst[i];
          pDst[i] = a + val * win;
          
        }
      }
      pDst += nDstPitch;
    pSrc += nSrcPitch;
    pWin += nWinPitch;
  }
}

/* 8 bit source, 32 bit temporary target */
template <int blockWidth, int blockHeight>
// pDst is short* for 8 bit, int * for 16 bit sources, float * from 32 bit sources
// out32: internally everything is float
void Overlaps_float_new_C(uint16_t* pDst0, int nDstPitch, const unsigned char* pSrc, int nSrcPitch, short* _pWin, int nWinPitch)
{
  // src is tmpBlock, for this new method it is always float
  auto pWin = reinterpret_cast<float*>(_pWin); // overlap windows constants are now float

  // in integer 8-16 bit format originally pWin from 0 to 2048 total shift 11
  // for float internal working formats no scaling occurs in overlaps constants

  float* pDst = reinterpret_cast<float*>(pDst0);

  for (int j = 0; j < blockHeight; j++)
  {
    for (int i = 0; i < blockWidth; i++)
    {
      float val = reinterpret_cast<const float*>(pSrc)[i];
      float win = pWin[i] /** (1.0f / 2048.0f)*/;
      pDst[i] = pDst[i] + val * win;
    }
    pDst += nDstPitch;
    pSrc += nSrcPitch;
    pWin += nWinPitch;
  }
}

template <int blockWidth, int blockHeight>
// pDst is short* for 8 bit, int * for 16 bit sources, float * from 32 bit sources
void Overlaps_float_C(uint16_t *pDst0, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *_pWin, int nWinPitch)
{
  auto pWin = reinterpret_cast<float*>(_pWin);
  // in integer 8-16 bit format originally pWin from 0 to 2048 total shift 11
  // for float internal working formats no scaling occurs in overlaps constants

  // when pixel_t == uint16_t, dst should be int*
  float *pDst = reinterpret_cast<float *>(pDst0);

  for (int j = 0; j<blockHeight; j++)
  {
    for (int i = 0; i<blockWidth; i++)
    {
      float val = reinterpret_cast<const float *>(pSrc)[i];
      float win = pWin[i] * (1.0f / 2048.0f);
      pDst[i] = pDst[i] + val * win;
    }
    pDst += nDstPitch;
    pSrc += nSrcPitch;
    pWin += nWinPitch;
  }
}


template <int blockWidth, int blockHeight>
void OverlapsLsb_C(int *pDst, int nDstPitch, const unsigned char *pSrc, const unsigned char *pSrcLsb, int nSrcPitch, short *pWin, int nWinPitch)
{
  // pWin from 0 to 2048: total shift 11
  for (int j=0; j<blockHeight; j++)
  {
    for (int i=0; i<blockWidth; i++)
    {
      const int		val = (pSrc [i] << 8) + pSrcLsb [i];
      short win = pWin[i];
      // 16 bits data + 11 bits window = 27 bits max (safe)
      // shift 11 in Short2Bytes_lsb
      pDst [i] += val * win;
    }
    pDst += nDstPitch;
    pSrc += nSrcPitch;
    pSrcLsb += nSrcPitch;
    pWin += nWinPitch;
  }
}


#ifdef USE_OVERLAPS_ASM
extern "C" void __cdecl  Overlaps64x64_sse2(uint16_t *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch);
extern "C" void __cdecl  Overlaps64x48_sse2(uint16_t *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch);
extern "C" void __cdecl  Overlaps64x32_sse2(uint16_t *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch);
extern "C" void __cdecl  Overlaps64x16_sse2(uint16_t *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch);
extern "C" void __cdecl  Overlaps48x64_sse2(uint16_t *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch);
extern "C" void __cdecl  Overlaps48x48_sse2(uint16_t *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch);
extern "C" void __cdecl  Overlaps48x24_sse2(uint16_t *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch);
extern "C" void __cdecl  Overlaps48x12_sse2(uint16_t *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch);
extern "C" void __cdecl  Overlaps32x64_sse2(uint16_t *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch);
extern "C" void __cdecl  Overlaps32x32_sse2(uint16_t *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch);
extern "C" void __cdecl  Overlaps32x24_sse2(uint16_t *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch);
extern "C" void __cdecl  Overlaps32x16_sse2(uint16_t *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch);
extern "C" void __cdecl  Overlaps32x8_sse2(uint16_t *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch);
extern "C" void __cdecl  Overlaps24x48_sse2(uint16_t *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch);
extern "C" void __cdecl  Overlaps24x32_sse2(uint16_t *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch);
extern "C" void __cdecl  Overlaps24x24_sse2(uint16_t *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch);
extern "C" void __cdecl  Overlaps24x12_sse2(uint16_t *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch);
extern "C" void __cdecl  Overlaps24x16_sse2(uint16_t *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch);
extern "C" void __cdecl  Overlaps24x6_sse2(uint16_t *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch);
extern "C" void __cdecl  Overlaps16x64_sse2(uint16_t *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch);
extern "C" void __cdecl  Overlaps16x32_sse2(uint16_t *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch);
extern "C" void __cdecl  Overlaps16x16_sse2(uint16_t *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch);
extern "C" void __cdecl  Overlaps16x12_sse2(uint16_t *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch);
extern "C" void __cdecl  Overlaps16x8_sse2(uint16_t *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch);
extern "C" void __cdecl  Overlaps16x4_sse2(uint16_t *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch);
extern "C" void __cdecl  Overlaps16x2_sse2(uint16_t *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch);
extern "C" void __cdecl  Overlaps12x48_sse2(uint16_t *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch);
extern "C" void __cdecl  Overlaps12x24_sse2(uint16_t *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch);
extern "C" void __cdecl  Overlaps12x16_sse2(uint16_t *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch);
extern "C" void __cdecl  Overlaps12x12_sse2(uint16_t *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch);
extern "C" void __cdecl  Overlaps12x6_sse2(uint16_t *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch);
extern "C" void __cdecl  Overlaps12x3_sse2(uint16_t *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch);
extern "C" void __cdecl  Overlaps8x32_sse2(uint16_t *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch);
extern "C" void __cdecl  Overlaps8x16_sse2(uint16_t *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch);
extern "C" void __cdecl  Overlaps8x8_sse2(uint16_t *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch);
extern "C" void __cdecl  Overlaps8x4_sse2(uint16_t *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch);
extern "C" void __cdecl  Overlaps8x2_sse2(uint16_t *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch);
extern "C" void __cdecl  Overlaps8x1_sse2(uint16_t *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch);
extern "C" void __cdecl  Overlaps4x8_sse2(uint16_t *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch);
extern "C" void __cdecl  Overlaps4x4_sse2(uint16_t *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch);
extern "C" void __cdecl  Overlaps4x2_sse2(uint16_t *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch);
extern "C" void __cdecl  Overlaps2x4_sse2(uint16_t *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch);
extern "C" void __cdecl  Overlaps2x2_sse2(uint16_t *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, short *pWin, int nWinPitch);
#endif

void Short2Bytes_sse2(unsigned char *pDst, int nDstPitch, uint16_t *pDstShort, int dstShortPitch, int nWidth, int nHeight);
void Short2Bytes(unsigned char *pDst, int nDstPitch, uint16_t *pDstShort, int dstShortPitch, int nWidth, int nHeight);
void Short2BytesLsb(unsigned char *pDst, unsigned char *pDstLsb, int nDstPitch, int *pDstInt, int dstIntPitch, int nWidth, int nHeight);
void Short2Bytes_Int32toWord16(uint16_t *pDst, int nDstPitch, int *pDstInt, int dstIntPitch, int nWidth, int nHeight, int bits_per_pixel);
void Short2Bytes_Int32toWord16_sse4(uint16_t *pDst, int nDstPitch, int *pDstInt, int dstIntPitch, int nWidth, int nHeight, int bits_per_pixel);
void Short2Bytes_FloatInInt32ArrayToFloat(float *pDst, int nDstPitch, float *pDstInt, int dstIntPitch, int nWidth, int nHeight);

template<typename pixel_t>
void OverlapsBuf_Float2Bytes_sse4(unsigned char* pDst0, int nDstPitch, float* pTmpFloat, int tmpPitch, int nWidth, int nHeight, int bits_per_pixel);
template<typename pixel_t>
void OverlapsBuf_Float2Bytes(unsigned char* pDst0, int nDstPitch, float* pTmpFloat, int tmpPitch, int nWidth, int nHeight, int bits_per_pixel);

template<typename pixel_t>
void LimitChanges_c(unsigned char *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, int nWidth, int nHeight, float nLimit_f);
void LimitChanges_float_c(unsigned char *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, int nWidth, int nHeight, float nLimit_f);

typedef void(LimitFunction_t)(unsigned char *pDst8, int nDstPitch, const unsigned char *pSrc8, int nSrcPitch, const int nWidth, int nHeight, float nLimit_f);

template<typename pixel_t, bool hasSSE41>
void LimitChanges_sse2_new(unsigned char *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, int nWidth, int nHeight, float nLimit_f);

void LimitChanges_src8_target16_c(unsigned char *pDst, int nDstPitch, const unsigned char *pSrc, int nSrcPitch, int nWidth, int nHeight, float nLimit_f);

#endif
