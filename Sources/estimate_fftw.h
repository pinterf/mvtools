/*
    DePanEstimate plugin for Avisynth 2.5 - global motion estimation
    Version 1.10, February 2016
    (DePanEstimate function)
    Copyright(c) 2004-2016, A.G. Balakhnin aka Fizick
    bag@hotmail.ru

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

	uses FFTW library (http://www.fftw.org)
	as Windows binary DLL compiled with gcc under MinGW by Alessio Massaro,
	which support for threads and have AMD K7 (3dNow!) in addition to SSE/SSE2.
	ftp://ftp.fftw.org/pub/fftw/fftw3win32mingw.zip

	calculates global motion in frames (by phase-shift method),

	Parameters of DePanEstimate:
		clip - input clip
		range - number of previous (and also next) frames (fields) near requsted frame to estimate motion
		trust - frames correlation at scene change
		winx  - number of columns of fft window (must be power of 2). (width)
		winy  - number of rows of fft window (must be power of 2).    (height)
		dxmax - limit of x shift
		dymax - limit of y shift
		zoommax - maximum zoom factor ( if =1, zoom is not estimated)
		improve - improve zoom estimation by iteration
		stab -  calculated trust decreasing for large shifts
		pixaspect - pixel aspect
		info  - show motion info on frame
		log - output log file with motion data
		debug - output data for debugview utility
		show - show correlation sufrace
		extlog - output extended log file with motion and trust data


	The DePanEstimate function output is special service clip with coded motion data in frames.

*/
#ifndef __ESTIMATE_FFTW_H__
#define __ESTIMATE_FFTW_H__

#include "windows.h"
#include "avisynth.h"
#include "stdio.h"
//#include "fftw\fftw3.h"
#include "fftwlite.h" // v.1.2

//****************************************************************************
class DePanEstimate_fftw : public GenericVideoFilter {
  // DePan defines the name of your filter class.
  // This name is only used internally, and does not affect the name of your filter or similar.
  // This filter extends GenericVideoFilter, which incorporates basic functionality.
  // All functions present in the filter must also be present here.
  bool has_at_least_v8;

// filter parameters
  int range;  // radius of frame series for motion calculation
  float trust_limit;  // scene change threshold, percent of correlation variation
  int winx;  // number of columns of fft window (must be power of 2). (width)
  int winy;  // number of rows of fft window (must be power of 2).    (height)
  int wleft; // v1.10
  int wtop;
  int dxmax;
  int dymax;
  float zoommax;  // maximum zoom factor ( if =1, zoom is not estimated)
  float stab; // calculated trust decreasing for large shifts
  float pixaspect;
  int info;
  const char *logfilename;
  int debug;       // debug mode, output data for debugview utility
  int show; // show correlation surface
  const char *extlogfilename;

  int pixelsize; // avs+
  int bits_per_pixel;

  // internal parameters
  int fieldbased;
  int TFF;  // top field first

  bool isYUY2;

  FILE *logfile;
  FILE *extlogfile;
  int fftcachecapacity;

  int * fftcachelist; //  cached fft frames numbers
  int * fftcachelist2; //  cached fft frames numbers right
//	float *** fftcache;
  fftwf_complex ** fftcache;
  //	float *** fftcache2;
  fftwf_complex ** fftcache2;
  int * fftcachelistcomp; //  cached fft compensated frames numbers
  int * fftcachelistcomp2; //  cached fft compensated frames numbers right
//	float *** fftcachecomp;
  fftwf_complex ** fftcachecomp;
  //	float *** fftcachecomp2;
  fftwf_complex ** fftcachecomp2;
  //	float ** correl;  // correlation surface
  //	float ** correl2;  // correlation surface for right zoom
  fftwf_complex *  correl;  // correlation surface
  fftwf_complex *  correl2;  // correlation surface for right zoom
//	float * fftwork;
//	int * fftip;


  float * realcorrel;
  float * realcorrel2;
  //	int winxpadded;
  //	int winsize;
  //	int fftsize;
  fftwf_plan plan, planinv;

  // motion tables
  float * motionx;
  float * motiony;
  float * motionzoom;
  float * trust;

  char messagebuf[32]; // buffer for message info
//	int height; // source height


//	BYTE *comp; // temp array for zoom compensated frame luma
//	BYTE *compYUY2; // temp array for zoom compensated frame luma for YUY2
//	int comp_pitch;

//	int * work2width1030;  // work array for interpolation
  char debugbuf[96]; // buffer for debugview utility

  // added in v.1.2 for delayed loading
  HINSTANCE hinstFFTW3;
  fftwf_malloc_proc fftwf_malloc_addr;
  fftwf_free_proc fftwf_free_addr;
  fftwf_plan_dft_r2c_2d_proc fftwf_plan_dft_r2c_2d_addr;
  fftwf_plan_dft_c2r_2d_proc fftwf_plan_dft_c2r_2d_addr;
  fftwf_destroy_plan_proc fftwf_destroy_plan_addr;
  fftwf_execute_dft_r2c_proc fftwf_execute_dft_r2c_addr;
  fftwf_execute_dft_c2r_proc fftwf_execute_dft_c2r_addr;



  template <typename pixel_t>
  void frame_data2d(const BYTE * srcp0, int height, int src_width, int pitch, float * fftdata, int winx, int winy, int winleft, int wintop);

  void mult_conj_data2d(fftwf_complex *fftnext, fftwf_complex *fftsrc, fftwf_complex *mult, int winx, int winy);
  void get_motion_vector(float *realcorrel, int winx, int winy, float trust_limit, int dxmax, int dymax, float stab, int nframe, int fieldbased, int TFF, float pixaspect, float *fdx, float *fdy, float *trust, int debug);
  void clear_unnecessary_cache(int *fftcachelist, int fftcachecapacity, int ndest, int range);
  int get_cache_number(int * fftcachelist, int fftcachecapacity, int ndest);
  int get_free_cache_number(int * fftcachelist, int fftcachecapacity);

  template<typename pixel_t>
  fftwf_complex * get_plane_fft(const BYTE * srcp, int src_height, int src_width, int src_pitch, int nsrc, int *fftcachelist, int fftcachecapacity, fftwf_complex **fftcache, int winx, int winy, int winleft, int wintop, fftwf_plan plan); // v.1.1

  template <typename pixel_t>
  void showcorrelation(float *realcorrel, int winx, int winy, BYTE *dstp0, int dst_pitch, int winleft, int wintop);


public:
  // This defines that these functions are present in your class.
  // These functions must be that same as those actually implemented.
  // Since the functions are "public" they are accessible to other classes.
  // Otherwise they can only be called from functions within the class itself.

  DePanEstimate_fftw(PClip _child, int _range, float _trust, int _winx, int _winy, int _wleft, int _wtop, int _dxmax, int _dymax, float _zoommax, float _stab, float _pixaspect, int _info, const char * _logfilename, int _debug, int _show, const char * _extlogfilename, IScriptEnvironment* env);
  // This is the constructor. It does not return any value, and is always used,
  //  when an instance of the class is created.
  // Since there is no code in this, this is the definition.

  ~DePanEstimate_fftw();
  // The is the destructor definition. This is called when the filter is destroyed.


  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
  // This is the function that AviSynth calls to get a given frame.
  // So when this functions gets called, the filter is supposed to return frame n.
};

#endif
