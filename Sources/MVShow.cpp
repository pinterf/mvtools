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
#include <cstdio>
#include "emmintrin.h"
//#include <intrin.h>


MVShow::MVShow(PClip _super, PClip vectors, int _scale, int _plane, int _tol, bool _showsad, int _number,
               sad_t nSCD1, int nSCD2, bool _isse, bool _planar, IScriptEnvironment* env) :
GenericVideoFilter(_super),
mvClip(vectors, nSCD1, nSCD2, env, 1, 0),
MVFilter(vectors, "MShow", env, 1, 0)
{
	isse = _isse;
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
	planar =_planar;
	number = _number;

	// get parameters of prepared super clip - v2.0
	SuperParams64Bits params;
	memcpy(&params, &vi.num_audio_samples, 8);
	int nHeightS = params.nHeight;
	nSuperHPad = params.nHPad;
	nSuperVPad = params.nVPad;
	int nSuperPel = params.nPel;
	int nSuperModeYUV = params.nModeYUV;
	int nSuperLevels = params.nLevels;
	int nSuperWidth = vi.width; // really super
	int nSuperHeight = vi.height;

	if (nHeight != nHeightS || nWidth != nSuperWidth-nSuperHPad*2)
		env->ThrowError("MShow : wrong super frame clip");

    vi.height = nHeight + nSuperVPad*2; // one level only (may be redesigned to show all levels with vectors at once)

	if ( (pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2 && !planar)
   {
		DstPlanes =  new YUY2Planes(vi.width, vi.height);
   }
}

MVShow::~MVShow()
{
   if ( (pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2  && !planar)
   {
	delete DstPlanes;
   }
}

PVideoFrame __stdcall MVShow::GetFrame(int n, IScriptEnvironment* env)
{
	PVideoFrame	src = child->GetFrame(n, env);
	PVideoFrame	dst = env->NewVideoFrame(vi);
   BYTE *pDst[3];
	   const BYTE *pSrc[3];
    int nDstPitches[3], nSrcPitches[3];
	unsigned char *pDstYUY2;
	int nDstPitchYUY2;

	PVideoFrame mvn = mvClip.GetFrame(n, env);
   mvClip.Update(mvn, env);

   		if ( (pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2 )
		{
          if (!planar)
          {
			pDst[0] = DstPlanes->GetPtr();
			pDst[1] = DstPlanes->GetPtrU();
			pDst[2] = DstPlanes->GetPtrV();
			nDstPitches[0]  = DstPlanes->GetPitch();
			nDstPitches[1]  = DstPlanes->GetPitchUV();
			nDstPitches[2]  = DstPlanes->GetPitchUV();
          }
          else
          {
            // planar data packed to interleaved format (same as interleved2planar by kassandro) - v2.0.0.5
                pDst[0] = dst->GetWritePtr();
                pDst[1] = pDst[0] + dst->GetRowSize()/2;
                pDst[2] = pDst[1] + dst->GetRowSize()/4;
                nDstPitches[0] = dst->GetPitch();
                nDstPitches[1] = nDstPitches[0];
                nDstPitches[2] = nDstPitches[0];
          }
            pSrc[0] = src->GetReadPtr();
            pSrc[1] = pSrc[0] + src->GetRowSize()/2;
            pSrc[2] = pSrc[1] + src->GetRowSize()/4;
            nSrcPitches[0] = src->GetPitch();
            nSrcPitches[1] = nSrcPitches[0];
            nSrcPitches[2] = nSrcPitches[0];
		}
		else
		{
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
		}

	// Copy the frame into the created frame
   env->BitBlt(pDst[0], nDstPitches[0], pSrc[0], nSrcPitches[0], vi.width*pixelsize, vi.height);
   if (!vi.IsY()) {
      env->BitBlt(pDst[1], nDstPitches[1], pSrc[1], nSrcPitches[1], vi.width / xRatioUV * pixelsize, vi.height / yRatioUV);
      env->BitBlt(pDst[2], nDstPitches[2], pSrc[2], nSrcPitches[2], vi.width / xRatioUV * pixelsize, vi.height / yRatioUV);
    }
  //   if ( !nPlane )

   if ( mvClip.IsUsable() )
      DrawMVs(pDst[0] + nDstPitches[0]*nSuperVPad + nHPadding*pixelsize, nDstPitches[0], pSrc[0] + nSrcPitches[0]*nSuperVPad + nHPadding*pixelsize, nSrcPitches[0]);


		if ( (pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2 && !planar)
		{
			pDstYUY2 = dst->GetWritePtr();
			nDstPitchYUY2 = dst->GetPitch();
			YUY2FromPlanes(pDstYUY2, nDstPitchYUY2, vi.width, vi.height,
								  pDst[0], nDstPitches[0], pDst[1], pDst[2], nDstPitches[1], isse);
		}

	if ( showSad ) {
		char buf[80];
#ifndef _M_X64
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
		sprintf_s(buf, "%d %d", int(mean / nBlkCount)*8/nBlkSizeX*8/nBlkSizeY, nsc*256/nBlkCount);
		if ( (pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2 && !planar)
			DrawStringYUY2(dst, 0, 0, buf);
		else
			DrawString(dst, vi, 0, 0, buf);
	}

	if ( number>0 && number <= mvClip.GetBlkCount()) {
		char buf[80];
		FakeBlockData block = mvClip.GetBlock(0, number);
         int x = block.GetX();
         int y = block.GetY();
		sprintf_s(buf, "n=%d x=%d y=%d vx=%d vy=%d sad=%d",number, x, y, block.GetMV().x, block.GetMV().y, block.GetSAD());
		if ( (pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2 && !planar)
			DrawStringYUY2(dst, 0, 0, buf);
		else
			DrawString(dst, vi, 0, 0, buf);
    if (pixelsize == 1) {
      BYTE *pDstWork = pDst[0] + x + y*nDstPitches[0] + nDstPitches[0] * nSuperVPad + nHPadding;
      for (int h = 0; h < nBlkSizeX; h++)
      {
        for (int w = 0; w < nBlkSizeY; w++)
          pDstWork[w] = 255; // todo pixelsize aware
        pDstWork += nDstPitches[0];
      }
    }
    else { // pixelsize == 2
      uint16_t *pDstWork = (uint16_t *)(pDst[0] + x * pixelsize + y*nDstPitches[0] + nDstPitches[0] * nSuperVPad + nHPadding * pixelsize);
      int pixel_max = (1 << bits_per_pixel) - 1;
      for (int h = 0; h < nBlkSizeX; h++)
      {
        for (int w = 0; w < nBlkSizeY; w++)
          pDstWork[w] = pixel_max; // todo pixelsize aware
        pDstWork += nDstPitches[0] / pixelsize;
      }
    }


	}
	return dst;
}

template<typename pixel_t>
inline void MVShow::DrawPixel(unsigned char *pDst, int nDstPitch, int x, int y, int w, int h, int luma)
{
//	if (( x >= 0 ) && ( x < w ) && ( y >= 0 ) && ( y < h )) // disababled in v.2 - it is no more needed with super clip
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
	for ( int i = 0; i < plane.GetBlockCount(); i++ )
		if ( plane[i].GetSAD() < nTolerance )
      // /2: center
			DrawMV(pDst + plane.GetBlockSizeX() / 2 * effectiveScale * pixelsize + plane.GetBlockSizeY()/ 2 * effectiveScale*nDstPitch, // changed in v1.8, now address is the center of first block
			    nDstPitch, nScale * effectiveScale, plane[i].GetX() * effectiveScale,
				plane[i].GetY() * effectiveScale, (plane.GetBlockSizeX() - plane.GetOverlapX())* effectiveScale,
				(plane.GetBlockSizeY() - plane.GetOverlapY())* effectiveScale,
                plane.GetWidth() * effectiveScale, plane.GetHeight() * effectiveScale,
                plane[i].GetMV(), plane.GetPel());
}
