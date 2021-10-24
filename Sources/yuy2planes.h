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

#ifndef __YUY2PLANES_H__
#define __YUY2PLANES_H__

#include <malloc.h>
#include "def.h"

class YUY2Planes
{
  unsigned char *pSrc;
  unsigned char *pSrcU;
  unsigned char *pSrcV;
  int nWidth;
  int nHeight;
  int srcPitch;
  int srcPitchUV;

   
public :

  YUY2Planes(int _nWidth, int _nHeight);
   ~YUY2Planes();

   MV_FORCEINLINE int GetPitch() const { return srcPitch; }
   MV_FORCEINLINE int GetPitchUV() const { return srcPitchUV; }
   MV_FORCEINLINE unsigned char *GetPtr() const { return pSrc; }
   MV_FORCEINLINE unsigned char *GetPtrU() const { return pSrcU; }
   MV_FORCEINLINE unsigned char *GetPtrV() const { return pSrcV; }

#if 0
static	void convYUV422to422(const unsigned char *src, unsigned char *py, unsigned char *pu, unsigned char *pv,
                                        int pitch1, int pitch2y, int pitch2uv, int width, int height);
static	void conv422toYUV422(const unsigned char *py, const unsigned char *pu, const unsigned char *pv,
                           unsigned char *dst, int pitch1Y, int pitch1UV, int pitch2, int width, int height);
#endif
};

void YUY2ToPlanes(const unsigned char *pSrcYUY2, int nSrcPitchYUY2, int nWidth, int nHeight,
                const unsigned char * pSrc1, int _srcPitch,
                const unsigned char * pSrcU1, const unsigned char * pSrcV1, int _srcPitchUV, int cpuFlags);

void YUY2FromPlanes(unsigned char *pSrcYUY2, int nSrcPitchYUY2, int nWidth, int nHeight,
                unsigned char * pSrc1, int _srcPitch,
                unsigned char * pSrcU1, unsigned char * pSrcV1, int _srcPitchUV, int cpuFlags);


#endif
