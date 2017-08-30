// DCT calculation with fftw (real)
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

#ifndef __MV_DCTINT__
#define __MV_DCTINT__



#include "DCTClass.h"



class DCTINT
  : public DCTClass
{

  short * pWorkArea; // 64 + 64 words for I/O block and internal buffer

//	int sizex;
//	int sizey;
//	int dctmode;
  int dctshift;
  int dctshift0;


public:


//	float * fRef;
//	float * fRefDCT;

  DCTINT(int _sizex, int _sizey, int _dctmode);
  ~DCTINT();
  void DCTBytes2D(const unsigned char *srcp0, int _src_pitch, unsigned char *dctp, int _dct_pitch);

};

extern "C" void fdct_mmx(short * const block);
extern "C" void fdct_sse2(short * const block);

/*
extern "C" {

typedef void (fdctFunc)(short * const block);
typedef fdctFunc* fdctFuncPtr;

extern "C" fdctFuncPtr fdct;

fdctFunc fdct_mmx;
fdctFunc fdct_altivec;
fdctFunc fdct_sse2;
  }
*/
#endif