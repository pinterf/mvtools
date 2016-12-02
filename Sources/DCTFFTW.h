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
#include "types.h"

#define	NOGDI
#define	NOMINMAX
#define	WIN32_LEAN_AND_MEAN
#include "windows.h"



class DCTFFTW
:	public DCTClass
{
  typedef void (DCTFFTW::*Float2BytesFunction)(unsigned char * srcp0, int _pitch, float * realdata);
  typedef void (DCTFFTW::*Bytes2FloatFunction)(const unsigned char * srcp8, int src_pitch, float * realdata);

	HINSTANCE hinstFFTW3;
	fftwf_malloc_proc fftwf_malloc_addr;
	fftwf_free_proc fftwf_free_addr;
	fftwf_plan_r2r_2d_proc fftwf_plan_r2r_2d_addr;
	fftwf_destroy_plan_proc fftwf_destroy_plan_addr;
	fftwf_execute_r2r_proc fftwf_execute_r2r_addr;

	float * fSrc;
	fftwf_plan dctplan;
	float * fSrcDCT;

//  members from DCTClass (DCTClass is parent of DCTINT or DCTFFTW)
//	int sizex;
//	int sizey;
//	int dctmode;
//  int pixelsize
//  int bits_per_pixel;
	int dctshift;
	int dctshift0;

    template<typename pixel_t>
    void Bytes2Float_C(const unsigned char * srcp0, int _pitch, float * realdata);
    
    template<typename pixel_t>
    void Float2Bytes_C(unsigned char * srcp0, int _pitch, float * realdata);

    template<typename pixel_t, int nBlkSizeX>
    void Bytes2Float_SSE2(const unsigned char * srcp8, int _pitch, float * realdata);

    template <typename pixel_t, int nBlkSizeX, bool hasSSE4>
    void Float2Bytes_SSE2(unsigned char * dstp0, int dst_pitch, float * realdata);

    DCTFFTW::Float2BytesFunction get_floatToBytesPROC_function(int BlockX, int BlockY, int pixelsize, arch_t arch);
    DCTFFTW::Float2BytesFunction floatToBytesPROC;

    DCTFFTW::Bytes2FloatFunction get_bytesToFloatPROC_function(int BlockX, int BlockY, int pixelsize, arch_t arch);
    DCTFFTW::Bytes2FloatFunction bytesToFloatPROC;

	static conc::Mutex _fftw_mutex;

public:

	DCTFFTW(int _sizex, int _sizey, ::HINSTANCE _hFFTW3, int _dctmode, int _pixelsize, int _bits_per_pixel, int cpu);
	~DCTFFTW();
    // works internally by pixelsize:
	void DCTBytes2D(const unsigned char *srcp0, int _src_pitch, unsigned char *dctp, int _dct_pitch);

};

#endif