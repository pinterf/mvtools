// Author: Manao
// Copyright(c)2006 A.G.Balakhnin aka Fizick - bicubic, wiener
// See legal notice in Copying.txt for more information
//
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

/******************************************************************************
*                                                                             *
*  MVGroupOfFrames : manage a hierachal frame structure                       *
*                                                                             *
******************************************************************************/



#include	"MVGroupOfFrames.h"
#include	"MVFrame.h"
#include	"MVSuper.h"



MVGroupOfFrames::MVGroupOfFrames(int _nLevelCount, int _nWidth, int _nHeight, int _nPel, int _nHPad, int _nVPad, int nMode, int cpuFlags, 
  int _xRatioUV, int _yRatioUV, int _pixelsize, int _bits_per_pixel, bool mt_flag)
:	nLevelCount (_nLevelCount)
,	pFrames (new MVFrame* [_nLevelCount])
,	nWidth (_nWidth)
,	nHeight (_nHeight)
,	nPel (_nPel)
,	nHPad (_nHPad)
,	nVPad (_nVPad)
,	xRatioUV (_xRatioUV)
,	yRatioUV (_yRatioUV)
, pixelsize(_pixelsize)
, bits_per_pixel(_bits_per_pixel)
{

   pFrames[0] = new MVFrame(nWidth, nHeight, nPel, nHPad, nVPad, nMode, cpuFlags, xRatioUV, yRatioUV, pixelsize, bits_per_pixel, mt_flag);
   for ( int i = 1; i < nLevelCount; i++ )
   {
      int nWidthi = PlaneWidthLuma(nWidth, i, xRatioUV /*PF instead of 2*/, nHPad);//(nWidthi / 2) - ((nWidthi / 2) % 2); //  even for YV12, YUY2
      int nHeighti = PlaneHeightLuma(nHeight, i, yRatioUV, nVPad);//(nHeighti / 2) - ((nHeighti / 2) % yRatioUV); // even for YV12
      pFrames[i] = new MVFrame(nWidthi, nHeighti, 1, nHPad, nVPad, nMode, cpuFlags, xRatioUV, yRatioUV, pixelsize, bits_per_pixel, mt_flag);
   }
}

void MVGroupOfFrames::Update(int nMode, uint8_t * pSrcY, int pitchY, uint8_t * pSrcU, int pitchU, uint8_t *pSrcV, int pitchV) // v2.0
{
	for ( int i = 0; i < nLevelCount; i++ )
	{
        // offsets are pixelsize-aware because pitch is in bytes
		unsigned int offY = PlaneSuperOffset(false, nHeight, i, nPel, nVPad, pitchY, yRatioUV); // no need here xRatioUV and pixelsize
		unsigned int offU = PlaneSuperOffset(true, nHeight/yRatioUV, i, nPel, nVPad/yRatioUV, pitchU, yRatioUV);
		unsigned int offV = PlaneSuperOffset(true, nHeight/yRatioUV, i, nPel, nVPad/yRatioUV, pitchV, yRatioUV);
		pFrames[i]->Update (nMode, pSrcY+offY, pitchY, pSrcU+offU, pitchU, pSrcV+offV, pitchV);
	}
}

MVGroupOfFrames::~MVGroupOfFrames()
{
   for ( int i = 0; i < nLevelCount; i++ )
	{
      delete pFrames[i];
	}

   delete[] pFrames;
}

MVFrame *MVGroupOfFrames::GetFrame(int nLevel)
{
   if (( nLevel < 0 ) || ( nLevel >= nLevelCount )) return 0;
   return pFrames[nLevel];
}

void MVGroupOfFrames::SetPlane(const uint8_t *pNewSrc, int nNewPitch, MVPlaneSet nMode)
{
   pFrames[0]->ChangePlane(pNewSrc, nNewPitch, nMode);
}



void	MVGroupOfFrames::set_interp (MVPlaneSet nMode, int rfilter, int sharp)
{
   pFrames[0]->set_interp (nMode, rfilter, sharp);
}



void MVGroupOfFrames::Refine(MVPlaneSet nMode)
{
   pFrames[0]->Refine(nMode);
}



void MVGroupOfFrames::Pad(MVPlaneSet nMode)
{
   pFrames[0]->Pad(nMode);
}



void MVGroupOfFrames::Reduce(MVPlaneSet _nMode)
{
   for (int i = 0; i < nLevelCount - 1; i++ )
   {
      pFrames[i]->ReduceTo(pFrames[i+1], _nMode);
      //pFrames[i+1]->Pad(YUVPLANES);
      // LDS: why padding all the planes when ReduceTo only applies to _nMode?
      pFrames[i+1]->Pad(_nMode);
      // PF: good question, now that we support Y-only, let's modified it 20181113
   }
}



void MVGroupOfFrames::ResetState()
{
   for ( int i = 0; i < nLevelCount; i++ )
	{
      pFrames[i]->ResetState();
	}
}

