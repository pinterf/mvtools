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

#ifndef __MV_DCTFFTW__
#define __MV_DCTFFTW__

#include	"conc/Mutex.h"
#include "DCTClass.h"
#include "fftwlite.h"

#define	NOGDI
#define	NOMINMAX
#define	WIN32_LEAN_AND_MEAN
#include "windows.h"



class DCTFFTW
:	public DCTClass
{

	HINSTANCE hinstFFTW3;
	fftwf_malloc_proc fftwf_malloc_addr;
	fftwf_free_proc fftwf_free_addr;
	fftwf_plan_r2r_2d_proc fftwf_plan_r2r_2d_addr;
	fftwf_destroy_plan_proc fftwf_destroy_plan_addr;
	fftwf_execute_r2r_proc fftwf_execute_r2r_addr;

	float * fSrc;
	fftwf_plan dctplan;
	float * fSrcDCT;

//	int sizex;
//	int sizey;
//	int dctmode;
	int dctshift;
	int dctshift0;

	void Bytes2Float(const unsigned char * srcp0, int _pitch, float * realdata);
	void Float2Bytes(unsigned char * srcp0, int _pitch, float * realdata);

	static conc::Mutex
						_fftw_mutex;

public:

	DCTFFTW(int _sizex, int _sizey, ::HINSTANCE _hFFTW3, int _dctmode);
	~DCTFFTW();
	void DCTBytes2D(const unsigned char *srcp0, int _src_pitch, unsigned char *dctp, int _dct_pitch);

};

#endif