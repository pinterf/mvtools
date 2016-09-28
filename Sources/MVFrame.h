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


#ifndef __MV_MVFrame__
#define __MV_MVFrame__



#include	"types.h"
#include	"MVPlaneSet.h"

#include	<cstdio>
#include <stdint.h>



class MVPlane;

class MVFrame
{

   MVPlane *pYPlane;
   MVPlane *pUPlane;
   MVPlane *pVPlane;

   int nMode;
   bool isse;
   int xRatioUV;
   int yRatioUV;
   int pixelsize;
   int bits_per_pixel;

public:
   MVFrame(int nWidth, int nHeight, int nPel, int nHPad, int nVPad, int _nMode, bool _isse, int _xRatioUV, int _yRatioUV, int _pixelsize, int _bits_per_pixel, bool mt_flag);
   ~MVFrame();

   void Update(int _nMode, uint8_t * pSrcY, int pitchY, uint8_t * pSrcU, int pitchU, uint8_t *pSrcV, int pitchV);
   void ChangePlane(const uint8_t *pNewSrc, int nNewPitch, MVPlaneSet _nMode);
	void set_interp (MVPlaneSet _nMode, int rfilter, int sharp);
   void Refine(MVPlaneSet _nMode);
   void Pad(MVPlaneSet _nMode);
   void ReduceTo(MVFrame *pFrame, MVPlaneSet _nMode);
   void ResetState();
   void WriteFrame(FILE *pFile);

   inline MVPlane *GetPlane(MVPlaneSet _nMode)
   {
      // no reason to test for nMode because returning NULL isn't expected in other parts
      // assert(nMode & _nMode & (YPLANE | UPLANE | VPLANE));

      if ( _nMode & YPLANE ) // ( nMode & _nMode & YPLANE )
         return pYPlane;

      if ( _nMode & UPLANE ) // ( nMode & _nMode & UPLANE )
         return pUPlane;

      if ( _nMode & VPLANE ) // ( nMode & _nMode & VPLANE )
         return pVPlane;

      return 0;
   }

   inline int GetMode() { return nMode; }

};



#endif	// __MV_MVFrame__
