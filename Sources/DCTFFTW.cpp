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



#include	"conc/CritSec.h"
#include "DCTFFTW.h"

//#define __INTEL_COMPILER_USE_INTRINSIC_PROTOTYPES 1
#include <emmintrin.h>

#include	<algorithm>

#include <cstdio>



conc::Mutex	DCTFFTW::_fftw_mutex;



DCTFFTW::DCTFFTW(int _sizex, int _sizey, HINSTANCE hinstFFTW3, int _dctmode, int _pixelsize) 
{
	fftwf_free_addr = (fftwf_free_proc) GetProcAddress(hinstFFTW3, "fftwf_free"); 
    if(!fftwf_free_addr) fftwf_free_addr = (fftwf_free_proc) GetProcAddress(hinstFFTW3, "fftw_free"); // ffw3 v3.5!!!
	
    fftwf_malloc_addr = (fftwf_malloc_proc)GetProcAddress(hinstFFTW3, "fftwf_malloc"); 
    if(!fftwf_malloc_addr) fftwf_malloc_addr = (fftwf_malloc_proc)GetProcAddress(hinstFFTW3, "fftw_malloc"); 
    
    fftwf_destroy_plan_addr = (fftwf_destroy_plan_proc) GetProcAddress(hinstFFTW3, "fftwf_destroy_plan");
    if(!fftwf_destroy_plan_addr) fftwf_destroy_plan_addr = (fftwf_destroy_plan_proc) GetProcAddress(hinstFFTW3, "fftw_destroy_plan");
    
    fftwf_plan_r2r_2d_addr = (fftwf_plan_r2r_2d_proc) GetProcAddress(hinstFFTW3, "fftwf_plan_r2r_2d");
    if(!fftwf_plan_r2r_2d_addr) fftwf_plan_r2r_2d_addr = (fftwf_plan_r2r_2d_proc) GetProcAddress(hinstFFTW3, "fftw_plan_r2r_2d");
    
    fftwf_execute_r2r_addr = (fftwf_execute_r2r_proc) GetProcAddress(hinstFFTW3, "fftwf_execute_r2r");
    if(!fftwf_execute_r2r_addr) fftwf_execute_r2r_addr = (fftwf_execute_r2r_proc) GetProcAddress(hinstFFTW3, "fftw_execute_r2r");

    // members of the DCTClass
	sizex = _sizex;
	sizey = _sizey;
	dctmode = _dctmode;
    pixelsize = _pixelsize;

	int size2d = sizey*sizex;

	int cursize = 1;
	dctshift = 0;
	while (cursize < size2d) 
	{
		dctshift++;
		cursize = (cursize<<1);
	}

	dctshift0 = dctshift + 2;

	// FFTW plan construction and destruction are not thread-safe.
	// http://www.fftw.org/fftw3_doc/Thread-safety.html#Thread-safety
	conc::CritSec	lock (_fftw_mutex);

	fSrc = (float *)fftwf_malloc_addr(sizeof(float) * size2d );
	fSrcDCT = (float *)fftwf_malloc_addr(sizeof(float) * size2d );

	dctplan = fftwf_plan_r2r_2d_addr(sizey, sizex, fSrc, fSrcDCT, FFTW_REDFT10, FFTW_REDFT10, FFTW_ESTIMATE); // direct fft 
}



DCTFFTW::~DCTFFTW()
{
	conc::CritSec	lock (_fftw_mutex);

	fftwf_destroy_plan_addr(dctplan);
	fftwf_free_addr(fSrc);
	fftwf_free_addr(fSrcDCT);
}

// put source data to real array for FFT
// see also DePanEstimate_fftw::frame_data2d
template<typename pixel_t>
void DCTFFTW::Bytes2Float (const unsigned char * srcp, int src_pitch, float * realdata)
{
    int floatpitch = sizex;
	int i, j;
	for (j = 0; j < sizey; j++)
	{ 
		for (i = 0; i < sizex; i+=1) // typical sizex is 16
		{
            realdata[i] = reinterpret_cast<const pixel_t *>(srcp)[i];
		}
		srcp += src_pitch;
		realdata += floatpitch;
	}
}

//  put source data to real array for FFT
template <typename pixel_t>
void DCTFFTW::Float2Bytes (unsigned char * dstp0, int dst_pitch, float * realdata)
{
    pixel_t *dstp = reinterpret_cast<pixel_t *>(dstp0);
    dst_pitch /= sizeof(pixel_t);

	int floatpitch = sizex;
	int i, j;
	int integ;

  int maxPixelValue = (1 << (pixelsize << 3)) - 1; // 255/65535
  int middlePixelValue = 1 << ((pixelsize << 3) - 1);   // 128/32768 

  // integer conversion
  // was1: 	_asm fld f; _asm fistp integ; // fast conversion
  // integ = (int)f; (x64)
  // was2: integ = (int)(nearbyintf(f)); // thx jackoneill
  // final: integ = (int)f: float to int with truncation
  //        sse2: cvttss2si is generated. No fast trick needed (VC6 era?)

  float f = realdata[0]*0.5f; // to be compatible with integer DCTINT8
  integ = int(f); 
	dstp[0] = std::min(maxPixelValue, std::max(0, (integ>>dctshift0) + middlePixelValue)); // DC

	for (i = 1; i < sizex; i+=1)
	{
		f = realdata[i]*0.707f; // to be compatible with integer DCTINT8
    integ = int(f);
		dstp[i] = std::min(maxPixelValue, std::max(0, (integ>>dctshift) + middlePixelValue));
	}

  dstp += dst_pitch;
	realdata += floatpitch;
	
  for (j = 1; j < sizey; j++)
	{ 
		for (i = 0; i < sizex; i+=1)
		{
			f = realdata[i]*0.707f; // to be compatible with integer DCTINT8
      integ = (int)f;
      dstp[i] = std::min(maxPixelValue, std::max(0, (integ>>dctshift) + middlePixelValue));
		}
		dstp += dst_pitch;
		realdata += floatpitch;
	}
}


void DCTFFTW::DCTBytes2D(const unsigned char *srcp, int src_pitch, unsigned char *dctp, int dct_pitch)
{
    _mm_empty ();
    if(pixelsize==1){
        Bytes2Float<uint8_t>(srcp, src_pitch, fSrc);
        fftwf_execute_r2r_addr(dctplan, fSrc, fSrcDCT);
        Float2Bytes<uint8_t>(dctp, dct_pitch, fSrcDCT);
    }
    else {
        Bytes2Float<uint16_t>(srcp, src_pitch, fSrc);
        fftwf_execute_r2r_addr(dctplan, fSrc, fSrcDCT);
        Float2Bytes<uint16_t>(dctp, dct_pitch, fSrcDCT);
    }
}
