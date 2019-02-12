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


#ifndef __MV_MVGroupOfFrames__
#define __MV_MVGroupOfFrames__



#include	"types.h"
#include	"MVPlaneSet.h"
#include <stdint.h>



class MVFrame;

class MVGroupOfFrames
{
   int nLevelCount;
   MVFrame **pFrames;

   int nWidth;
   int nHeight;
   int nPel;
   int nHPad;
   int nVPad;
   int xRatioUV; // PF 160729
   int yRatioUV;
   int pixelsize; // PF 160729
   int bits_per_pixel; // PF 160927

public :
    // xRatioUV PF 160729
   MVGroupOfFrames(int _nLevelCount, int nWidth, int nHeight, int nPel, int nHPad, int nVPad, int nMode, int cpuFlags, int xRatioUV, int yRatioUV, int _pixelsize, int _bits_per_pixel, bool mt_flag);
   ~MVGroupOfFrames();
   void Update(int nModeYUV, uint8_t * pSrcY, int pitchY, uint8_t * pSrcU, int pitchU, uint8_t *pSrcV, int pitchV);

   MVFrame *GetFrame(int nLevel);
   void SetPlane(const uint8_t *pNewSrc, int nNewPitch, MVPlaneSet nMode);
   void set_interp (MVPlaneSet nMode, int rfilter, int sharp);
   void Refine(MVPlaneSet nMode);
   void Pad(MVPlaneSet nMode);
   void Reduce(MVPlaneSet nMode);
   void ResetState();
};



#endif // __MV_MVGroupOfFrames__