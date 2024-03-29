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
*  MVFrame : a MVFrame is a threesome of MVPlane, some undefined, some        *
*  defined, according to the nMode value                                      *
*                                                                             *
******************************************************************************/



#include	"MVFrame.h"
#include	"MVPlane.h"



MVFrame::MVFrame(int nWidth, int nHeight, int nPel, int nHPad, int nVPad, int _nMode, int _cpuFlags, int _xRatioUV, int _yRatioUV, int _pixelsize, int _bits_per_pixel, bool mt_flag)
{
   nMode = _nMode;
   cpuFlags = _cpuFlags;
   xRatioUV = _xRatioUV;
   yRatioUV = _yRatioUV;
   pixelsize = _pixelsize;
   bits_per_pixel = _bits_per_pixel;

   if ( nMode & YPLANE )
      pYPlane = new MVPlane(nWidth, nHeight, nPel, nHPad, nVPad, pixelsize, bits_per_pixel, cpuFlags, mt_flag);
   else
      pYPlane = 0;

   if ( nMode & UPLANE )
      pUPlane = new MVPlane(nWidth / xRatioUV, nHeight / yRatioUV, nPel, nHPad / xRatioUV, nVPad / yRatioUV, pixelsize, bits_per_pixel, cpuFlags, mt_flag);
   else
      pUPlane = 0;

   if ( nMode & VPLANE )
      pVPlane = new MVPlane(nWidth / xRatioUV, nHeight / yRatioUV, nPel, nHPad / xRatioUV, nVPad / yRatioUV, pixelsize, bits_per_pixel, cpuFlags, mt_flag);
   else
      pVPlane = 0;
}



void MVFrame::Update(int _nMode, uint8_t * pSrcY, int pitchY, uint8_t * pSrcU, int pitchU, uint8_t *pSrcV, int pitchV)
{
//   nMode = _nMode;

   if ( _nMode & nMode & YPLANE  ) //v2.0.8
      pYPlane->Update(pSrcY, pitchY);

   if ( _nMode & nMode & UPLANE  )
      pUPlane->Update(pSrcU, pitchU);

   if ( _nMode & nMode & VPLANE  )
      pVPlane->Update(pSrcV, pitchV);
}



MVFrame::~MVFrame()
{
   if ( nMode & YPLANE )
      delete pYPlane;

   if ( nMode & UPLANE )
      delete pUPlane;

   if ( nMode & VPLANE )
      delete pVPlane;
}



void MVFrame::ChangePlane(const uint8_t *pNewPlane, int nNewPitch, MVPlaneSet _nMode)
{
   if ( _nMode & nMode & YPLANE )
      pYPlane->ChangePlane(pNewPlane, nNewPitch);

   if ( _nMode & nMode & UPLANE )
      pUPlane->ChangePlane(pNewPlane, nNewPitch);

   if ( _nMode & nMode & VPLANE )
      pVPlane->ChangePlane(pNewPlane, nNewPitch);
}



void	MVFrame::set_interp (MVPlaneSet _nMode, int rfilter, int sharp)
{
   if (nMode & YPLANE & _nMode)
  {
      pYPlane->set_interp (rfilter, sharp);
  }
   if (nMode & UPLANE & _nMode)
  {
      pUPlane->set_interp (rfilter, sharp);
  }
   if (nMode & VPLANE & _nMode)
  {
      pVPlane->set_interp (rfilter, sharp);
  }
}



void MVFrame::Refine(MVPlaneSet _nMode)
{
   if (nMode & YPLANE & _nMode)
  {
      pYPlane->refine_start();
  }
   if (nMode & UPLANE & _nMode)
  {
      pUPlane->refine_start();
  }
   if (nMode & VPLANE & _nMode)
  {
      pVPlane->refine_start();
  }

   if (nMode & YPLANE & _nMode)
  {
     pYPlane->refine_wait();
   }
   if (nMode & UPLANE & _nMode)
  {
     /*
     sprintf(debugbuf, "Refine: waitfor U WHeWeH=%d %d %d %d",
       this->pUPlane->GetWidth(),
       this->pUPlane->GetHeight(),
       this->pUPlane->GetExtendedWidth(),
       this->pUPlane->GetExtendedHeight()
     );
     OutputDebugString(debugbuf);
     */
     pUPlane->refine_wait();
     /*
     sprintf(debugbuf, "Refine: waitfor U OK");
     OutputDebugString(debugbuf);
     */
   }
   if (nMode & VPLANE & _nMode)
  {
     /*sprintf(debugbuf, "Refine: waitfor V WHeWeH=%d %d %d %d",
       this->pVPlane->GetWidth(),
       this->pVPlane->GetHeight(),
       this->pVPlane->GetExtendedWidth(),
       this->pVPlane->GetExtendedHeight()
     );
     OutputDebugString(debugbuf);
     */
     pVPlane->refine_wait();
     /*
      sprintf(debugbuf, "Refine: waitfor V OK");
      OutputDebugString(debugbuf);
      */
   }
}



void MVFrame::Pad(MVPlaneSet _nMode)
{
   if (nMode & YPLANE & _nMode)
      pYPlane->Pad();

   if (nMode & UPLANE & _nMode)
      pUPlane->Pad();

   if (nMode & VPLANE & _nMode)
      pVPlane->Pad();
}



void MVFrame::ResetState()
{
   if ( nMode & YPLANE )
      pYPlane->ResetState();

   if ( nMode & UPLANE )
      pUPlane->ResetState();

   if ( nMode & VPLANE )
      pVPlane->ResetState();
}



void MVFrame::WriteFrame(FILE *pFile)
{
   if ( nMode & YPLANE )
      pYPlane->WritePlane(pFile);

   if ( nMode & UPLANE )
      pUPlane->WritePlane(pFile);

   if ( nMode & VPLANE )
      pVPlane->WritePlane(pFile);
}



void MVFrame::ReduceTo(MVFrame *pFrame, MVPlaneSet _nMode)
{
  // parameter pFrame target frame to which reducing takes place (smaller dimension)

  if (nMode & YPLANE & _nMode)
  {
    pYPlane->reduce_start (pFrame->GetPlane(YPLANE));
  }
  if (nMode & UPLANE & _nMode)
  {
    pUPlane->reduce_start (pFrame->GetPlane(UPLANE));
  }
  if (nMode & VPLANE & _nMode)
  {
    pVPlane->reduce_start (pFrame->GetPlane(VPLANE));
  }

  if (nMode & YPLANE & _nMode)
  {
    pYPlane->reduce_wait ();
  }
  if (nMode & UPLANE & _nMode)
  {
    pUPlane->reduce_wait ();
  }
  if (nMode & VPLANE & _nMode)
  {
    /* avstp bug chasing week
    const bool be = this->pVPlane->GetWidth() == 45 && this->pVPlane->GetHeight() == 72;
    if (be) {
      sprintf(debugbuf, "Reduce: waitfor V WHeWeH=%dx%d %d %d to %dx%d",
        this->pVPlane->GetWidth(),
        this->pVPlane->GetHeight(),
        this->pVPlane->GetExtendedWidth(),
        this->pVPlane->GetExtendedHeight(),
        pFrame->GetPlane(VPLANE)->GetWidth(),
        pFrame->GetPlane(VPLANE)->GetHeight()
        );
      OutputDebugString(debugbuf);
    }
    */
    pVPlane->reduce_wait ();
    /*
    if (be) {
      sprintf(debugbuf, "Reduce: waitfor V OK");
      OutputDebugString(debugbuf);
    }
    */
  }
}

