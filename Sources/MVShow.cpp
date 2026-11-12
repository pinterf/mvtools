// Show the motion vectors of a clip
// Author: Manao
// Copyright(c)2006 A.G.Balakhnin aka Fizick - YUY2, overlap

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

#include "info.h"
#include "ClipFnc.h"
#include "MVShow.h"
#include	"SuperParams64Bits.h"

#include <stdlib.h>
#include <algorithm>
#include <cstdio>
#include "types.h"
#ifdef INTEL_INTRINSICS
#include "emmintrin.h"
#endif
//#include <intrin.h>


MVShow::MVShow(PClip _super, PClip vectors, int _scale, int _plane, int _tol, bool _showsad, int _number,
               sad_t nSCD1, int nSCD2, bool _isse, IScriptEnvironment* env) :
GenericVideoFilter(_super),
MVFilter(vectors, "MShow", env, 1, 0),
mvClip(vectors, nSCD1, nSCD2, env, 1, 0)
{
  has_at_least_v8 = true;
  try { env->CheckVersion(8); }
  catch (const AvisynthError&) { has_at_least_v8 = false; }

  cpuFlags = _isse ? env->GetCPUFlags() : 0;
  nScale = _scale;
  if ( nScale < 1 )
      nScale = 1;

  nPlane = _plane;
  if ( nPlane < 0 )
      nPlane = 0;
   if ( nPlane >= mvClip.GetLevelCount() - 1 )
      nPlane = mvClip.GetLevelCount() - 1;

  nTolerance = _tol;
   if ( nTolerance < 0 )
      nTolerance = 0;

  showSad = _showsad;
  number = _number;

  // get parameters of prepared super clip - v2.0
  SuperParams64Bits params;
  memcpy(&params, &vi.num_audio_samples, 8);
  int nHeightS = params.nHeight;
  nSuperHPad = params.nHPad;
  nSuperVPad = params.nVPad;
  //int nSuperPel = params.nPel;
  //int nSuperModeYUV = params.nModeYUV;
  //int nSuperLevels = params.nLevels;
  int nSuperWidth = vi.width; // really super
  //int nSuperHeight = vi.height;

  nTolerance = nTolerance << (vi.BitsPerComponent() - 8); // scale sad tolerance

  if (nHeight != nHeightS || nWidth != nSuperWidth-nSuperHPad*2)
    env->ThrowError("MShow : wrong super frame clip");

  vi.height = nHeight + nSuperVPad*2; // one level only (may be redesigned to show all levels with vectors at once)
  // multiple of the vertical chroma subsampling, like the super clip
  vi.height = (vi.height + yRatioUV - 1) / yRatioUV * yRatioUV;

}

MVShow::~MVShow()
{
}

PVideoFrame __stdcall MVShow::GetFrame(int n, IScriptEnvironment* env)
{
  PVideoFrame	src = child->GetFrame(n, env);
  PVideoFrame	dst = has_at_least_v8 ? env->NewVideoFrameP(vi, &src) : env->NewVideoFrame(vi); // frame property support

   BYTE *pDst[3];
     const BYTE *pSrc[3];
    int nDstPitches[3], nSrcPitches[3];

  PVideoFrame mvn = mvClip.GetFrame(n, env);
   mvClip.Update(mvn, env);

       pSrc[0] = YRPLAN(src);
       pSrc[1] = URPLAN(src);
       pSrc[2] = VRPLAN(src);
       nSrcPitches[0] = YPITCH(src);
       nSrcPitches[1] = UPITCH(src);
       nSrcPitches[2] = VPITCH(src);

     pDst[0] = YWPLAN(dst);
       pDst[1] = UWPLAN(dst);
       pDst[2] = VWPLAN(dst);
       nDstPitches[0] = YPITCH(dst);
       nDstPitches[1] = UPITCH(dst);
       nDstPitches[2] = VPITCH(dst);

  // Copy the frame into the created frame
   env->BitBlt(pDst[0], nDstPitches[0], pSrc[0], nSrcPitches[0], vi.width*pixelsize, vi.height);
   if (!ClipFnc::is_grey(vi)) {
      env->BitBlt(pDst[1], nDstPitches[1], pSrc[1], nSrcPitches[1], vi.width / xRatioUV * pixelsize, vi.height / yRatioUV);
      env->BitBlt(pDst[2], nDstPitches[2], pSrc[2], nSrcPitches[2], vi.width / xRatioUV * pixelsize, vi.height / yRatioUV);
    }
  //   if ( !nPlane )

   if ( mvClip.IsUsable() )
      DrawMVs(pDst[0], nDstPitches[0], pSrc[0], nSrcPitches[0]);



  if ( showSad ) {
    char buf[80];
#if defined(INTEL_INTRINSICS) && !defined(_M_X64)
    _mm_empty();
#endif
    double mean = 0;
    int nsc = 0;
    sad_t ThSCD1 = mvClip.GetThSCD1();
      for ( int i = 0; i < mvClip.GetBlkCount(); i++ )
    {
         sad_t sad = mvClip.GetBlock(0, i).GetSAD();
//		 DebugPrintf("i= %d, SAD= %d", i, sad);
     mean += sad;
     if (sad > ThSCD1)
            nsc += 1;
    }
    snprintf(buf, sizeof(buf), "%d %d", int(mean / nBlkCount)*8/nBlkSizeX*8/nBlkSizeY, nsc*256/nBlkCount);
    DrawString(dst, vi, 0, 0, buf);
  }

  if (number > 0 && number < mvClip.GetBlkCount()) {
    char buf[80];
    FakeBlockData block = mvClip.GetBlock(0, number);
    int x = block.GetX();
    int y = block.GetY();
    snprintf(buf, sizeof(buf), "n=%d x=%d y=%d vx=%d vy=%d sad=%d", number, x, y, block.GetMV().x, block.GetMV().y, block.GetSAD());
    DrawString(dst, vi, 0, 0, buf);
    // the block, clipped to the frame (the vector clip may come from a clip with other padding)
    const int x0 = std::max(nSuperHPad + x, 0);
    const int y0 = std::max(nSuperVPad + y, 0);
    const int x1 = std::min(nSuperHPad + x + nBlkSizeX, vi.width);
    const int y1 = std::min(nSuperVPad + y + nBlkSizeY, vi.height);
    if (pixelsize == 1) {
      for (int h = y0; h < y1; h++)
        for (int w = x0; w < x1; w++)
          pDst[0][w + h * nDstPitches[0]] = 255;
    }
    else { // pixelsize == 2
      const int pixel_max = (1 << bits_per_pixel) - 1;
      for (int h = y0; h < y1; h++)
        for (int w = x0; w < x1; w++)
          reinterpret_cast<uint16_t*>(pDst[0] + h * nDstPitches[0])[w] = pixel_max;
    }
  }
  ClipFnc::fill_alpha_opaque(dst, vi); // the super clip has no meaningful alpha
  return dst;
}

template<typename pixel_t>
MV_FORCEINLINE void MVShow::DrawPixel(unsigned char *pDst, int nDstPitch, int x, int y, int w, int h, int luma)
{
  // w, h: frame size. The line can leave the frame: scale > 1, or vectors made with larger padding
  // than the padding of the super clip (e.g. MScaleVect)
  if (( x >= 0 ) && ( x < w ) && ( y >= 0 ) && ( y < h ))
    reinterpret_cast<pixel_t *>(pDst)[x + y * nDstPitch / sizeof(pixel_t)] = luma;
}

// Draw the vector, scaled with the right scalar factor.
void MVShow::DrawMV(unsigned char *pDst, int nDstPitch, int scale,
              int x, int y, int sizex, int sizey, int w, int h, VECTOR vector, int pel)
{
  int x0 =  x; // changed in v1.8, now pDdst is the center of first block
  int y0 =  y;

   bool yLonger = false;
  int incrementVal, endVal;
  int shortLen =  scale*vector.y / pel; // added missed scale - Fizick
  int longLen = scale*vector.x / pel;   //
  if ( abs(shortLen) > abs(longLen)) {
    int swap = shortLen;
    shortLen = longLen;
    longLen = swap;
    yLonger = true;
  }
  endVal = longLen;
  if (longLen < 0) {
    incrementVal = -1;
    longLen = -longLen;
  } else incrementVal = 1;
  int decInc;
  if (longLen==0) decInc=0;
  else decInc = (shortLen << 16) / longLen;
  int j=0;
  if (yLonger) {
    if (pixelsize == 1) {
      for (int i = 0; i != endVal; i += incrementVal) {
        int luma = 255 - i *(255 - 160) / endVal;
        DrawPixel<uint8_t>(pDst, nDstPitch, x0 + (j >> 16), y0 + i, w, h, luma);
        j += decInc;
      }
    }
    else { // pixelsize == 2
      const int max_pixel_value = (1 << bits_per_pixel) - 1;
      const int pixel160 = 160 << (bits_per_pixel - 8);
      for (int i = 0; i != endVal; i += incrementVal) {
        int luma = max_pixel_value - i *(max_pixel_value - pixel160) / endVal;
        DrawPixel<uint16_t>(pDst, nDstPitch, x0 + (j >> 16), y0 + i, w, h, luma);
        j += decInc;
      }
    }
  } else {
    if (pixelsize == 1) {
      for (int i = 0; i != endVal; i += incrementVal) {
        int luma = 255 - i *(255 - 160) / endVal;
        DrawPixel<uint8_t>(pDst, nDstPitch, x0 + i, y0 + (j >> 16), w, h, luma);
        j += decInc;
      }
    }
    else { // pixelsize == 2
      const int max_pixel_value = (1 << bits_per_pixel) - 1;
      const int pixel160 = 160 << (bits_per_pixel - 8);
      for (int i = 0; i != endVal; i += incrementVal) {
        int luma = max_pixel_value - i *(max_pixel_value - pixel160) / endVal;
        DrawPixel<uint16_t>(pDst, nDstPitch, x0 + i, y0 + (j >> 16), w, h, luma);
        j += decInc;
      }
    }
  }
}

void MVShow::DrawMVs(unsigned char *pDst, int nDstPitch, const unsigned char *pSrc,
                     int nSrcPitch)
{
  const FakePlaneOfBlocks &plane = mvClip.GetPlane(nPlane);
  int effectiveScale = plane.GetEffectiveScale();
/* there was some memory access problem, disabled in v1.11
  const unsigned char *s = pSrc;

    if ( nPlane )
    {
      for ( int j = 0; j < plane.GetHeight(); j++)
      {
        for ( int i = 0; i < plane.GetWidth(); i++)
        {
          int val = 0;
          for ( int x = i * effectiveScale ; x < effectiveScale * (i + 1); x++ )
            for ( int y = j * effectiveScale; y < effectiveScale * (j + 1); y++ )
              val += pSrc[x + y * nSrcPitch];
          val = (val + effectiveScale * effectiveScale / 2) / (effectiveScale * effectiveScale);
          for ( int x = i * effectiveScale ; x < effectiveScale * (i + 1); x++ )
            for ( int y = j * effectiveScale; y < effectiveScale * (j + 1); y++ )
              pDst[x + y * nDstPitch] = val;
        }
        s += effectiveScale * nSrcPitch;
      }
    }
*/
  for (int i = 0; i < plane.GetBlockCount(); i++) {
    if (plane[i].GetSAD() < nTolerance) {
      // pDst: top left of the frame, the block positions are relative to the padded area
      // /2: center of the block
      DrawMV(pDst, nDstPitch, nScale * effectiveScale,
        nSuperHPad + plane.GetBlockSizeX() / 2 * effectiveScale + plane[i].GetX() * effectiveScale,
        nSuperVPad + plane.GetBlockSizeY() / 2 * effectiveScale + plane[i].GetY() * effectiveScale,
        (plane.GetBlockSizeX() - plane.GetOverlapX()) * effectiveScale,
        (plane.GetBlockSizeY() - plane.GetOverlapY()) * effectiveScale,
        vi.width, vi.height,
        plane[i].GetMV(), plane.GetPel());
    }
  }
}
