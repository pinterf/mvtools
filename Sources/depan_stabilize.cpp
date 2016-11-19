/*
  DePan plugin for Avisynth 2.6 interface - global motion estimation and compensation of camera pan
  Version 1.9, November 5, 2006.
  Version 1.10.0, April 29, 2007
  Version 1.10.1
  Version 1.13, February 18, 2016.
  Version 1.13.1, April 6, 2016
  Version 2.13.1, November 19, 2016 by pinterf
  (DePanStabilize function)
  Copyright(c)2004-2016, A.G. Balakhnin aka Fizick
  bag@hotmail.ru

  10-16 bit depth support for Avisynth+ by pinterf

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

    DePan plugin at first stage calculates global motion in frames (by phase-shift method),
  and at second stage shift frame images for global motion compensation

    DePanStabilize function make some motion stabilization (deshake)
  by inertial filtering of global motion.

    Inertial motion filtering method is partially based on article:
  Camera Stabilization Based on 2.5D Motion Estimation and Inertial Motion Filtering.
  by Zhigang Zhu, Guangyou Xu, Yudong Yang, Jesse S. Jin,
  1998 IEEE International Conference on Intelligent Vehicles.
  (File: zhu98camera.pdf)

  Parameters of DePanStabilize:
    clip - input clip (the same as input clip in DePanEstimate)
    data - special service clip with coded motion data, produced by DePanEstimate
    cutoff - vibration frequency cutoff, Hertz
    damping - relative damping coefficient
    addzoom - add adaptive zoom
    prev - previous frame lag to fill border
    next - next frame lag to fill border
    mirror - use mirror to fill border
    dxmax - limit of horizontal correction, in pixels
    dymax - limit of vertical correction, in pixels
    zoommax - limit of zoom correction ( smooth only adaptive zoom)
    rotmax - limit of rotation correction, in degrees
      (if any above max parameter is positive, the correction will be soft limited,
      if negative, the correction will be null and frame set as new base )
    subpixel - pixel interpolation accuracy
    pixaspect - pixel aspect
    fitlast - fit some last frames range to original position
    tzoom - adaptive zoom rise time, sec
    initzoom - initial zoom
    info - show motion info on frame
    inputlog - name of input log file in Deshaker format (default - none, not read)
    method - stablilization method number (0-inertial, 1-average)

*/

#include <avisynth.h>
#include "commonfunctions.h"

#include "info.h"
#include "depanio.h"
#include "depan.h"

#include <chrono>
#include <cassert>

//****************************************************************************
class DePanStabilize : public GenericVideoFilter {
  // DePanStabilize defines the name of your filter class.
  // This name is only used internally, and does not affect the name of your filter or similar.
  // This filter extends GenericVideoFilter, which incorporates basic functionality.
  // All functions present in the filter must also be present here.

  PClip DePanData;      // motion data clip
  float cutoff; // vibration frequency cutoff
  float damping; // relative damping coeff
  float initzoom; // initial zoom - added in v.1.7
  bool addzoom; // add adaptive zoom
  int fillprev;
  int fillnext;
  int mirror;
  int blur; // mirror blur len - v.1.3
  float dxmax;  // deshake limit of dx
  float dymax;  // deshake limit of dy
  float zoommax;
  float rotmax; // degrees
  int subpixel;     // subpixel accuracy by interpolation
  float pixaspect; // pixel aspect
  int fitlast; // range of last frame to fit to original position - added in v.1.2
  float tzoom; // adaptive zoom rise time, sec - added in v. 1.5
  int info;   // show info on frame
  const char *inputlog;  // filename of input log file in Deshaker format
  const char *vdx; // global parameter dx name
  const char *vdy; // global parameter dy name
  const char *vzoom; // global parameter zoom name
  const char *vrot; // global parameter rot name
  int method; // stabllization method
  const char *debuglog;  // filename of debug log P.F.

  // internal parameters
//	int matchfields;
//	int fieldbased;
  int nfields;
  int TFF;
  float fps; // frame per second
  float mass; // mass
  float pdamp;  // damping parameter
  float kstiff; // stiffness
  float freqnative;  // native frequency
//	float s1,s2,c0,c1,c2,cnl; // smoothing filter coefficients
  int nbase; // base frame for stabilization
  int radius; // stabilization radius

// motion tables
  float * motionx;
  float * motiony;
  float * motionzoom;
  float * motionrot;

  //int * work2width4356;  // work array for interpolation

  transform * trcumul; // cumulative transforms array
  transform * trsmoothed; // smoothed cumulative transforms array

  transform nonlinfactor; // it is not transform, but approximate inverse limit values for components

  float * azoom; // adaptive zooms
  float * azoomsmoothed; // adaptive zooms smoothed

  // planes from YUY2 - v1.6
  struct {
    BYTE * srcplaneY;
    BYTE * srcplaneU;
    BYTE * srcplaneV;
    BYTE * dstplaneY;
    BYTE * dstplaneU;
    BYTE * dstplaneV;
    int planeYpitch;
    int planeUVpitch;
    int planeYwidth, planeUVwidth;
  } YUY2data;

  float vdx_val, vdy_val, vzoom_val, vrot_val; // AviSynth permanent variables (SetVar)

  float * wint; // average window
  int wintsize;
  float * winrz; // rize zoom window
  float * winfz; // fall zoom window
  int winrzsize;
  int winfzsize;

  float xcenter;  // center of frame
  float ycenter;

  int pixelsize;
  int bits_per_pixel;
  int width, height;

  FILE *debuglogfile; // P.F. 16.03.11
  char debugbuf[1024];
  std::chrono::time_point<std::chrono::high_resolution_clock> t_start, t_end; // std::chrono::time_point<std::chrono::system_clock> t_start, t_end;

  void LogToFile(char *buf);
  void Inertial(int _nbase, int _ndest, transform * trdif);
  void Average(int nbase, int ndest, int nmax, transform * trdif);
  void InertialLimit(float *dxdif, float *dydif, float *zoomdif, float *rotdif, int ndest, int *nbase);
  float Averagefraction(float dxdif, float dydif, float zoomdif, float rotdif);
public:
  // This defines that these functions are present in your class.
  // These functions must be that same as those actually implemented.
  // Since the functions are "public" they are accessible to other classes.
  // Otherwise they can only be called from functions within the class itself.

  DePanStabilize(PClip _child, PClip _DePanData, float _cutoff, float _damping,
    float _initzoom, bool _addzoom, int _fillprev, int _fillnext, int _mirror, int _blur, float _dxmax,
    float _dymax, float _zoommax, float _rotmax, int _subpixel, float _pixaspect,
    int _fitlast, float _tzoom, int _info, const char * _inputlog,
    const char * _vdx, const char * _vdy, const char * _vzoom, const char * _vrot, int _method, const char * _debuglog, IScriptEnvironment* env);
  // This is the constructor. It does not return any value, and is always used,
  //  when an instance of the class is created.
  // Since there is no code in this, this is the definition.

  ~DePanStabilize();
  // The is the destructor definition. This is called when the filter is destroyed.


  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
  // This is the function that AviSynth calls to get a given frame.
  // So when this functions gets called, the filter is supposed to return frame n.

};

//***************************
//  The following is the implementation
//  of the defined functions.
// ***************************
//****************************************************************************

//Here is the actual constructor code used
DePanStabilize::DePanStabilize(PClip _child, PClip _DePanData, float _cutoff,
  float _damping, float _initzoom, bool _addzoom, int _fillprev, int _fillnext,
  int _mirror, int _blur, float _dxmax, float _dymax, float _zoommax,
  float _rotmax, int _subpixel, float _pixaspect,
  int _fitlast, float _tzoom, int _info, const char * _inputlog,
  const char * _vdx, const char * _vdy, const char * _vzoom, const char * _vrot, int _method, const char * _debuglog, IScriptEnvironment* env) :

  GenericVideoFilter(_child), DePanData(_DePanData), cutoff(_cutoff), damping(_damping),
  initzoom(_initzoom), addzoom(_addzoom), fillprev(_fillprev), fillnext(_fillnext), mirror(_mirror), blur(_blur),
  dxmax(_dxmax), dymax(_dymax), zoommax(_zoommax), rotmax(_rotmax), subpixel(_subpixel),
  pixaspect(_pixaspect), fitlast(_fitlast), tzoom(_tzoom), info(_info), inputlog(_inputlog),
  vdx(_vdx), vdy(_vdy), vzoom(_vzoom), vrot(_vrot), method(_method), debuglog(_debuglog) {
  // This is the implementation of the constructor.
  // The child clip (source clip) is inherited by the GenericVideoFilter,
  //  where the following variables gets defined:
  //   PClip child;   // Contains the source clip.
  //   VideoInfo vi;  // Contains videoinfo on the source clip.

  //char debugbuf[100];
  int error;
  int loginterlaced;
  //	int nfields;
  float lambda;

  t_start = std::chrono::high_resolution_clock::now(); // t_start starts in the constructor. Used in logging

  if (lstrlen(debuglog) > 0) { // v.1.2.3
    debuglogfile = fopen(debuglog, "wt");
    if (debuglogfile == NULL)	env->ThrowError("DePanStabilize: Debuglog file can not be created!");
  }
  else
    debuglogfile = NULL;

  _RPT0(0, "Start \n");

  //	matchfields =1;

  //	zoommax = max(zoommax, initzoom); // v1.7 to prevent user error
  zoommax = zoommax > 0 ? max(zoommax, initzoom) : -max(-zoommax, initzoom); // v1.8.2

  int fieldbased = (vi.IsFieldBased()) ? 1 : 0;
  TFF = (vi.IsTFF()) ? 1 : 0;
  // correction for fieldbased
  if (fieldbased != 0) 	nfields = 2;
  else nfields = 1;

  pixelsize = vi.ComponentSize();
  bits_per_pixel = vi.BitsPerComponent();

  if (!vi.IsYUY2() && !vi.IsYUV())
    env->ThrowError("DePanStabilize: input must be YUV planar!");

  if (method < -1 || method > 2)
    env->ThrowError("DePanStabilize: wrong method");//v1.13

  if ((DePanData->GetVideoInfo().num_frames) != vi.num_frames)
    env->ThrowError("DePanStabilize: The length of input clip must be same as motion data clip !");

  motionx = new float[vi.num_frames]; // was: (float *)malloc(vi.num_frames*sizeof(float));
  motiony = new float[vi.num_frames];
  motionrot = new float[vi.num_frames];
  motionzoom = new float[vi.num_frames];;

  //work2width4356 = (int *)malloc((2 * vi.width + 4356) * sizeof(int)); // work

  trcumul = new transform[vi.num_frames]; // (transform *)malloc(vi.num_frames * sizeof(transform));
  trsmoothed = new transform[vi.num_frames]; // (transform *)malloc(vi.num_frames * sizeof(transform));

  azoom = new float[vi.num_frames]; // (float *) malloc(vi.num_frames * sizeof(float));
  azoomsmoothed = new float[vi.num_frames]; //(float *)malloc(vi.num_frames * sizeof(float));

  //	child->SetCacheHints(CACHE_RANGE,3);
  //	DePanData->SetCacheHints(CACHE_RANGE,3);

  if (lstrlen(inputlog) > 0) { // motion data will be readed from deshaker.log file once at start
//		if (inputlog != "") { // motion data will be readed from deshaker.log file once at start
    error = read_deshakerlog(inputlog, vi.num_frames, motionx, motiony, motionrot, motionzoom, &loginterlaced);
    if (error == -1)	env->ThrowError("DePan: Input log file not found!");
    if (error == -2)	env->ThrowError("DePan: Error input log file format!");
    if (error == -3)	env->ThrowError("DePan: Too many frames in input log file!");
    //		if(vi.IsFieldBased  && loginterlaced==0)	env->ThrowError("DePan: Input log must be in interlaced for fieldbased!");
  }
  else { // motion data will be requesred from DepanEstimate
    if ((DePanData->GetVideoInfo().num_frames) != child->GetVideoInfo().num_frames)
      env->ThrowError("DePan: The length of input clip must be same as motion data clip  !");

    for (int i = 0; i < (vi.num_frames); i++) {
      motionx[i] = MOTIONUNKNOWN;  // init as unknown for all frames
//			motionrot[i] = 0;  // zero rotation for all frames (it is not estimated in current version)
    }

#ifdef _DEBUG
    _RPT1(0, "Num of frames= %d \n", DePanData->GetVideoInfo().num_frames);
    sprintf(debugbuf, "Num of frames= %d \n", DePanData->GetVideoInfo().num_frames);
    LogToFile(debugbuf);
#endif
    //	if (debuglogfile != NULL) { fprintf(debuglogfile, "Num of frames= %d \n", DePanData->GetVideoInfo().num_frames); } // PF debug
  }

  // prepare coefficients for inertial motion smoothing filter

    // termination (max) frequency for vibration eliminating in Hertz
    // damping ratio
//		damping = 0.9; // unfixed in v.1.1.4 (was accidentally fixed =0.9 in all previous versions  :-)
    // elastic stiffness of spring
  kstiff = 1.0;  // value is not important - (not included in result)
  //  relative frequency lambda at half height of response
  lambda = sqrtf(1 + 6 * damping*damping + sqrtf((1 + 6 * damping*damping)*(1 + 6 * damping*damping) + 3));
  // native oscillation frequency
  freqnative = cutoff / lambda;
  // mass of camera
  mass = kstiff / ((6.28f*freqnative)*(6.28f*freqnative));
  // damping parameter
//		pdamp = 2*damping*sqrtf(mass*kstiff);
//		pdamp = 2*damping*sqrtf(kstiff*kstiff/( (6.28f*freqnative)*(6.28f*freqnative) ));
  pdamp = 2 * damping*kstiff / (6.28f*freqnative);
  // frames per secomd
  fps = float(vi.fps_numerator) / vi.fps_denominator;

  // old smoothing filter coefficients from paper
//		float a1 = (2*mass + pdamp*period)/(mass + pdamp*period + kstiff*period*period);
//		float a2 = -mass/(mass + pdamp*period + kstiff*period*period);
//		float b1 = (pdamp*period + kstiff*period*period)/(mass + pdamp*period + kstiff*period*period);
//		float b2 = -pdamp*period/(mass + pdamp*period + kstiff*period*period);

/*		s1 = (2*mass*fps*fps - kstiff)/(mass*fps*fps + pdamp*fps/2);
    s2 = (-mass*fps*fps + pdamp*fps/2)/(mass*fps*fps + pdamp*fps/2);
    c0 = pdamp*fps/2/(mass*fps*fps + pdamp*fps/2);
    c1 = kstiff/(mass*fps*fps + pdamp*fps/2);
    c2 = -pdamp*fps/2/(mass*fps*fps + pdamp*fps/2);
    cnl = -kstiff/(mass*fps*fps + pdamp*fps/2); // nonlinear
*/
// approximate factor values for nonlinear members as half of max
  if (dxmax != 0) { // chanded in v1.7
    nonlinfactor.dxc = 5 / fabsf(dxmax);// chanded in v1.7
  }
  else {
    nonlinfactor.dxc = 0;
  }
  if (fabsf(zoommax) != 1) {// chanded in v1.7
    nonlinfactor.dxx = 5 / (fabsf(zoommax) - 1);// chanded in v1.7
    nonlinfactor.dyy = 5 / (fabsf(zoommax) - 1);// chanded in v1.7
  }
  else {
    nonlinfactor.dxx = 0;
    nonlinfactor.dyy = 0;
  }
  if (dymax != 0) { // chanded in v1.7
    nonlinfactor.dyc = 5 / fabsf(dymax);// chanded in v1.7
  }
  else {
    nonlinfactor.dyc = 0;
  }
  if (rotmax != 0) {// chanded in v1.7
    nonlinfactor.dxy = 5 / fabsf(rotmax);// chanded in v1.7
    nonlinfactor.dyx = 5 / fabsf(rotmax);// chanded in v1.7
  }
  else {
    nonlinfactor.dxy = 0;
    nonlinfactor.dyx = 0;
  }

  nbase = 0;

  // get smoothed cumulative transform members
//		sprintf(debugbuf,"DePanStabilize: freqnative=%f mass=%f damp=%f\n", freqnative, mass, damp);
//		OutputDebugString(debugbuf);

  if (vi.IsYUY2()) // v1.6
  { // create planes from YUY2
    YUY2data.planeYwidth = vi.width;
    YUY2data.planeUVwidth = vi.width / 2;
    YUY2data.planeYpitch = AlignNumber(vi.width, 16);
    YUY2data.planeUVpitch = AlignNumber(vi.width / 2, 16);
    YUY2data.srcplaneY = (BYTE*)malloc(YUY2data.planeYpitch*vi.height);
    YUY2data.srcplaneU = (BYTE*)malloc(YUY2data.planeUVpitch*vi.height);
    YUY2data.srcplaneV = (BYTE*)malloc(YUY2data.planeUVpitch*vi.height);
    YUY2data.dstplaneY = (BYTE*)malloc(YUY2data.planeYpitch*vi.height);
    YUY2data.dstplaneU = (BYTE*)malloc(YUY2data.planeUVpitch*vi.height);
    YUY2data.dstplaneV = (BYTE*)malloc(YUY2data.planeUVpitch*vi.height);
  }


  initzoom = 1 / initzoom; // make consistent with internal definition - v1.7

  wintsize = int(fps / (4 * cutoff));
  radius = wintsize;
  wint = new float[wintsize + 1]; // (float *)malloc((wintsize + 1) * sizeof(float));

  float PI = 3.14159265258f;
  for (int i = 0; i < wintsize; i++)
    wint[i] = cosf(i*0.5f*PI / wintsize);
  wint[wintsize] = 0;

  winrz = new float[wintsize + 1]; // (float *)malloc((wintsize + 1) * sizeof(float));
  winfz = new float[wintsize + 1]; // (float *)malloc((wintsize + 1) * sizeof(float));
  winrzsize = min(wintsize, int(fps*tzoom / 4));
  //	winfzsize = min(wintsize,int(fps*tzoom*1.5/4));
  winfzsize = min(wintsize, int(fps*tzoom / 4));
  for (int i = 0; i < winrzsize; i++)
    winrz[i] = cosf(i*0.5f*PI / winrzsize);
  for (int i = winrzsize; i <= wintsize; i++)
    winrz[i] = 0;
  for (int i = 0; i < winfzsize; i++)
    winfz[i] = cosf(i*0.5f*PI / winfzsize);
  for (int i = winfzsize; i <= wintsize; i++)
    winfz[i] = 0;

  xcenter = vi.width / 2.0f;  // center of frame
  ycenter = vi.height / 2.0f;

}

//****************************************************************************
// This is where any actual destructor code used goes
DePanStabilize::~DePanStabilize() {
  // This is where you can deallocate any memory you might have used.
  delete[] motionx; // free(motionx);
  delete[] motiony;
  delete[] motionzoom;
  delete[] motionrot;
  //free(work2width4356);

  delete[] trcumul;
  delete[] trsmoothed;
  delete[] azoom; // bug fixed with memory leakage in v.0.9.1
  delete[] azoomsmoothed; // bug fixed with memory leakage in v.0.9.1

  if (vi.IsYUY2()) // v1.6
  {
    free(YUY2data.srcplaneY);
    free(YUY2data.srcplaneU);
    free(YUY2data.srcplaneV);
    free(YUY2data.dstplaneY);
    free(YUY2data.dstplaneU);
    free(YUY2data.dstplaneV);
  }

  delete[] wint;
  delete[] winrz;
  delete[] winfz;
  if (debuglogfile != NULL)
    fclose(debuglogfile);

}


//

void DePanStabilize::LogToFile(char *buf)
{
  if (debuglogfile != NULL)
  {
    using namespace std::chrono;
    t_end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed_seconds = t_end - t_start;
    fprintf(debuglogfile, "%f --> %s", elapsed_seconds.count(), buf);
  }
}

void DePanStabilize::Inertial(int nbase, int ndest, transform * ptrdif)
{
  int n;
  transform trnull, trinv, trcur, trtemp;
  float zoom = 1; // make null transform
  motion2transform(0, 0, 0, zoom, 1, 0, 0, 1, 1.0, &trnull);

  sumtransform(trnull, trnull, &trsmoothed[nbase]); // set null as smoothed for base - v1.12
  sumtransform(trnull, trnull, &trsmoothed[nbase + 1]); // set null as smoothed for base+1 - v1.12

  float cdamp = 12.56f*damping / fps;
  float cquad = 39.44f / (fps*fps);

  float firstpass, secondpass;

  // recurrent calculation of smoothed cumulative transforms from base+2 to ndest frames
#ifdef DEBUG
  _RPT2(0, "  >> Inertial: %d->%d recurrent calculation of smoothed cumulative transforms from base+2 to ndest frames\n", nbase + 2, ndest);
  sprintf(debugbuf, "  >> Inertial: %d->%d recurrent calculation of smoothed cumulative transforms from base+2 to ndest frames\n", nbase + 2, ndest);
  LogToFile(debugbuf);
#endif

  for (n = nbase + 2; n <= ndest; n++) {
#ifdef DEBUG
    _RPT1(0, "  >>> n=%d\n", n);
    sprintf(debugbuf, "  >>> n=%d\n", n);
    LogToFile(debugbuf);
#endif
    float sm1;
    float sm2;
    float cu1;
    float cu2;
    float cu0;
    float nonlin;

    //----------------------------------------
    // let's simplify our life. Same for dyc predictor
    sm1 = trsmoothed[n - 1].dxc;
    sm2 = trsmoothed[n - 2].dxc;
    cu1 = trcumul[n - 1].dxc;
    cu2 = trcumul[n - 2].dxc;
    cu0 = trcumul[n].dxc;
    nonlin = nonlinfactor.dxc;
#ifdef _DEBUG
    _RPT3(0, "  >>>> trcumul[n-2].dxc = %f, [n-1].dxc = %f, [n].dxc = %f\n", cu2, cu1, cu0);
    sprintf(debugbuf, "  >>>> trcumul[n-2].dxc = %f, [n-1].dxc = %f, [n].dxc = %f\n", cu2, cu1, cu0);
    LogToFile(debugbuf);
#endif
    //----------------------------------------
    // dxc predictor:
    firstpass = 2 * sm1 - sm2 - \
      cdamp*freqnative*(sm1 - sm2 - cu1 + cu2)* \
      (1 + 0.5f*nonlin / freqnative*fabsf(sm1 - sm2 - cu1 + cu2)) - \
      cquad*freqnative*freqnative*(sm1 - cu1)* \
      (1 + nonlin*fabsf(sm1 - cu1));    // predictor
    // corrector, one iteration must be enough:
    secondpass = 2 * sm1 - sm2 - \
      cdamp*freqnative*0.5f*(firstpass - sm2 - cu0 + cu2)* \
      (1 + 0.5f*nonlin / freqnative*0.5f*fabsf(firstpass - sm2 - cu0 + cu2)) - \
      cquad*freqnative*freqnative*(sm1 - cu1)* \
      (1 + nonlin*fabsf(sm1 - cu1));
#ifdef _DEBUG
    _RPT2(0, "  >>>> trsmoothed[n].dxc %f, %f\n", firstpass, secondpass);
    sprintf(debugbuf, "  >>>> trsmoothed[n].dxc %f, %f\n", firstpass, secondpass);
    LogToFile(debugbuf);
#endif
    trsmoothed[n].dxc = secondpass;
    //----------------------------------------

    // very light (2 frames interval) stabilization of zoom
    trsmoothed[n].dxx = 0.5f*(trcumul[n].dxx + trsmoothed[n - 1].dxx);

    // dxy predictor:
    // double cutoff frequency for rotation
    trsmoothed[n].dxy = 2 * trsmoothed[n - 1].dxy - trsmoothed[n - 2].dxy - \
      cdamp * 2 * freqnative*(trsmoothed[n - 1].dxy - trsmoothed[n - 2].dxy - trcumul[n - 1].dxy + trcumul[n - 2].dxy)* \
      (1 + 0.5f*nonlinfactor.dxy / freqnative*fabsf(trsmoothed[n - 1].dxy - trsmoothed[n - 2].dxy - trcumul[n - 1].dxy + trcumul[n - 2].dxy)) - \
      cquad * 4 * freqnative*freqnative*(trsmoothed[n - 1].dxy - trcumul[n - 1].dxy)* \
      (1 + nonlinfactor.dxy*fabsf(trsmoothed[n - 1].dxy - trcumul[n - 1].dxy));    // predictor
    // corrector, one iteration must be enough:
    trsmoothed[n].dxy = 2 * trsmoothed[n - 1].dxy - trsmoothed[n - 2].dxy - \
      cdamp * 2 * freqnative*0.5f*(trsmoothed[n].dxy - trsmoothed[n - 2].dxy - trcumul[n].dxy + trcumul[n - 2].dxy)* \
      (1 + 0.5f*nonlinfactor.dxy / freqnative*0.5f*fabsf(trsmoothed[n].dxy - trsmoothed[n - 2].dxy - trcumul[n].dxy + trcumul[n - 2].dxy)) - \
      cquad * 4 * freqnative*freqnative*(trsmoothed[n - 1].dxy - trcumul[n - 1].dxy)* \
      (1 + nonlinfactor.dxy*fabsf(trsmoothed[n - 1].dxy - trcumul[n - 1].dxy));    // corrector, one iteration must be enough

    // dyx predictor:
    trsmoothed[n].dyx = -trsmoothed[n].dxy*(pixaspect / nfields)*(pixaspect / nfields); // must be consistent
    //----------------------------------------
    sm1 = trsmoothed[n - 1].dyc;
    sm2 = trsmoothed[n - 2].dyc;
    cu1 = trcumul[n - 1].dyc;
    cu2 = trcumul[n - 2].dyc;
    cu0 = trcumul[n].dyc;
    nonlin = nonlinfactor.dyc;

#ifdef _DEBUG
    _RPT3(0, "  >>>> trcumul[n-2].dyc = %f, [n-1].dyc = %f, [n].dyc = %f\n", cu2, cu1, cu0);
    sprintf(debugbuf, "  >>>> trcumul[n-2].dyc = %f, [n-1].dyc = %f, [n].dyc = %f\n", cu2, cu1, cu0);
    LogToFile(debugbuf);
#endif
    //----------------------------------------
    // dyc predictor:
    firstpass = 2 * sm1 - sm2 - \
      cdamp*freqnative*(sm1 - sm2 - cu1 + cu2)* \
      (1 + 0.5f*nonlin / freqnative*fabsf(sm1 - sm2 - cu1 + cu2)) - \
      cquad*freqnative*freqnative*(sm1 - cu1)* \
      (1 + nonlin*fabsf(sm1 - cu1));    // predictor
                                                  // corrector, one iteration must be enough:
    secondpass = 2 * sm1 - sm2 - \
      cdamp*freqnative*0.5f*(firstpass - sm2 - cu0 + cu2)* \
      (1 + 0.5f*nonlin / freqnative*0.5f*fabsf(firstpass - sm2 - cu0 + cu2)) - \
      cquad*freqnative*freqnative*(sm1 - cu1)* \
      (1 + nonlin*fabsf(sm1 - cu1));
#ifdef _DEBUG
    _RPT2(0, "  >>>> trsmoothed[n].dyc %f, %f\n", firstpass, secondpass);
    sprintf(debugbuf, );
    LogToFile(debugbuf);
#endif
    trsmoothed[n].dyc = secondpass;
    /*
    trsmoothed[n].dyc = 2*trsmoothed[n-1].dyc - trsmoothed[n-2].dyc - \
      cdamp*freqnative*( trsmoothed[n-1].dyc - trsmoothed[n-2].dyc - trcumul[n-1].dyc + trcumul[n-2].dyc )* \
      ( 1 + 0.5f*nonlinfactor.dyc/freqnative*fabsf(trsmoothed[n-1].dyc - trsmoothed[n-2].dyc - trcumul[n-1].dyc + trcumul[n-2].dyc) ) - \
      cquad*freqnative*freqnative*(trsmoothed[n-1].dyc - trcumul[n-1].dyc)* \
      ( 1 + nonlinfactor.dyc*fabsf(trsmoothed[n-1].dyc - trcumul[n-1].dyc) );    // predictor
    // corrector, one iteration must be enough:
    firstpass = trsmoothed[n].dyc;
    secondpass = 2*trsmoothed[n-1].dyc - trsmoothed[n-2].dyc - \
      cdamp*freqnative*0.5f*( trsmoothed[n].dyc - trsmoothed[n-2].dyc - trcumul[n].dyc + trcumul[n-2].dyc )* \
      ( 1 + 0.5f*nonlinfactor.dyc/freqnative*0.5f*fabsf(trsmoothed[n].dyc - trsmoothed[n-2].dyc - trcumul[n].dyc + trcumul[n-2].dyc) ) - \
      cquad*freqnative*freqnative*(trsmoothed[n-1].dyc - trcumul[n-1].dyc)* \
      ( 1 + nonlinfactor.dyc*fabsf(trsmoothed[n-1].dyc - trcumul[n-1].dyc) );    // corrector, one iteration must be enough
    sprintf(debugbuf, "  >>>> trsmoothed[n].dyc %f, %f\n", trsmoothed[n].dyc, secondpass);
    LogToFile(debugbuf);
    */
    /* when nonlinfactor is small (dymax=8 -> 5/8, because nonlinfactor.dyc = 5/fabsf(dymax);// chanded in v1.7
       then it becomes chaotic, non-damping
      11.290916 --> >> > n = 149
      11.290919 --> >> >> trcumul[n - 2].dxc = 148.651062, [n - 1].dxc = 143.604858, [n].dxc = 139.620255
      11.290921 --> >> >> trsmoothed[n].dxc 158.509842, 158.786652
      11.290935 --> >> >> trsmoothed[n].dyc 75.265640, 5.554328
      11.290937 --> >> > n = 150
      11.290939 --> >> >> trcumul[n - 2].dxc = 143.604858, [n - 1].dxc = 139.620255, [n].dxc = 138.575485
      11.290942 --> >> >> trsmoothed[n].dxc 154.598495, 154.971573
      11.290944 --> >> >> trsmoothed[n].dyc 241.518326, -1973.398193
    */
    /* Chaotic sample:
    0.553963 --> >> Inertial: 2->5 recurrent calculation of smoothed cumulative transforms from base + 2 to ndest frames
      0.553965 --> >> > n = 2
      0.553967 --> >> >> trcumul[n - 2].dxc = 0.000000, [n - 1].dxc = -2.886788, [n].dxc = -15.039845
      0.553970 --> >> >> trsmoothed[n].dxc - 0.778461, -2.661203
      0.553972 --> >> >> trsmoothed[n].dyc - 21.915480, 7.578507
      0.553974 --> >> > n = 3
      0.553976 --> >> >> trcumul[n - 2].dxc = -2.886788, [n - 1].dxc = -15.039845, [n].dxc = -23.005192
      0.553979 --> >> >> trsmoothed[n].dxc - 9.658495, -7.243145
      0.553981 --> >> >> trsmoothed[n].dyc 56.719391, -191.539413
      0.553983 --> >> > n = 4
      0.553985 --> >> >> trcumul[n - 2].dxc = -15.039845, [n - 1].dxc = -23.005192, [n].dxc = -28.292694
      0.553987 --> >> >> trsmoothed[n].dxc - 13.109569, -12.501877
      0.553990 --> >> >> trsmoothed[n].dyc - 1044.933960, 51973.785156
      0.553991 --> >> > n = 5
      0.553994 --> >> >> trcumul[n - 2].dxc = -23.005192, [n - 1].dxc = -28.292694, [n].dxc = -35.200096
      0.553996 --> >> >> trsmoothed[n].dxc - 18.137730, -18.261559
      0.553999 --> >> >> trsmoothed[n].dyc 243883.328125, -2916832768.000000

      */
      //----------------------------------------

      // dyy
    trsmoothed[n].dyy = trsmoothed[n].dxx; //must be equal to dxx
  }


  if (addzoom) { // calculate and add adaptive zoom factor to fill borders (for all frames from base to ndest)

    azoom[nbase] = initzoom;
    azoom[nbase + 1] = initzoom;
    azoomsmoothed[nbase] = initzoom;
    azoomsmoothed[nbase + 1] = initzoom;
    for (n = nbase + 2; n <= ndest; n++) {
      // get inverse transform
      inversetransform(trcumul[n], &trinv);
      // calculate difference between smoothed and original non-smoothed cumulative transform
      sumtransform(trinv, trsmoothed[n], &trcur);
      // find adaptive zoom factor
//				transform2motion (trcur, 1, xcenter, ycenter, pixaspect/nfields, &dxdif, &dydif, &rotdif, &zoomdif);
      azoom[n] = initzoom;
      float azoomtest = 1 + (trcur.dxc + trcur.dxy*ycenter) / xcenter; // xleft
      if (azoomtest < azoom[n]) azoom[n] = azoomtest;
      azoomtest = 1 - (trcur.dxc + trcur.dxx*vi.width + trcur.dxy*ycenter - width) / xcenter; //xright
      if (azoomtest < azoom[n]) azoom[n] = azoomtest;
      azoomtest = 1 + (trcur.dyc + trcur.dyx*xcenter) / ycenter; // ytop
      if (azoomtest < azoom[n]) azoom[n] = azoomtest;
      azoomtest = 1 - (trcur.dyc + trcur.dyx*xcenter + trcur.dyy*height - height) / ycenter; //ybottom
      if (azoomtest < azoom[n]) azoom[n] = azoomtest;

      // limit zoom to max - added in v.1.4.0
//				if (fabsf(azoom[n]-1) > fabsf(zoommax)-1)
//					azoom[n] = 	2 - fabsf(zoommax) ;


        // smooth adaptive zoom
        // zoom time factor
      float zf = 1 / (cutoff*tzoom);
      // predictor
      azoomsmoothed[n] = 2 * azoomsmoothed[n - 1] - azoomsmoothed[n - 2] -
        zf*cdamp*freqnative*(azoomsmoothed[n - 1] - azoomsmoothed[n - 2] - azoom[n - 1] + azoom[n - 2])
        //					*( 1 + 0.5f*nonlinfactor.dxx/freqnative*fabsf(azoomsmoothed[n-1] - azoomsmoothed[n-2] - azoom[n-1] + azoom[n-2])) // disabled in v.1.4.0 for more smooth
        - zf*zf*cquad*freqnative*freqnative*(azoomsmoothed[n - 1] - azoom[n - 1])
        //					*( 1 + nonlinfactor.dxx*fabsf(azoomsmoothed[n-1] - azoom[n-1]) )
        ;
      // corrector, one iteration must be enough:
      azoomsmoothed[n] = 2 * azoomsmoothed[n - 1] - azoomsmoothed[n - 2] -
        zf*cdamp*freqnative*0.5f*(azoomsmoothed[n] - azoomsmoothed[n - 2] - azoom[n] + azoom[n - 2])
        //					*( 1 + 0.5f*nonlinfactor.dxx/freqnative*0.5f*fabsf(azoomsmoothed[n] - azoomsmoothed[n-2] - azoom[n] + azoom[n-2]) )
        - zf*zf*cquad*freqnative*freqnative*(azoomsmoothed[n - 1] - azoom[n - 1])
        //					*( 1 + nonlinfactor.dxx*fabsf(azoomsmoothed[n-1] - azoom[n-1]) )
        ;
      zf = zf*0.7f; // slower zoom decreasing
      if (azoomsmoothed[n] > azoomsmoothed[n - 1]) // added in v.1.4.0 for slower zoom decreasing
      {
        // predictor
        azoomsmoothed[n] = 2 * azoomsmoothed[n - 1] - azoomsmoothed[n - 2] -
          zf*cdamp*freqnative*(azoomsmoothed[n - 1] - azoomsmoothed[n - 2] - azoom[n - 1] + azoom[n - 2])
          //					*( 1 + 0.5f*nonlinfactor.dxx/freqnative*fabsf(azoomsmoothed[n-1] - azoomsmoothed[n-2] - azoom[n-1] + azoom[n-2]))
          - zf*zf*cquad*freqnative*freqnative*(azoomsmoothed[n - 1] - azoom[n - 1])
          //					*( 1 + nonlinfactor.dxx*fabsf(azoomsmoothed[n-1] - azoom[n-1]) )
          ;
        // corrector, one iteration must be enough:
        azoomsmoothed[n] = 2 * azoomsmoothed[n - 1] - azoomsmoothed[n - 2] -
          zf*cdamp*freqnative*0.5f*(azoomsmoothed[n] - azoomsmoothed[n - 2] - azoom[n] + azoom[n - 2])
          //					*( 1 + 0.5f*nonlinfactor.dxx/freqnative*0.5f*fabsf(azoomsmoothed[n] - azoomsmoothed[n-2] - azoom[n] + azoom[n-2]) )
          - zf*zf*cquad*freqnative*freqnative*(azoomsmoothed[n - 1] - azoom[n - 1])
          //					*( 1 + nonlinfactor.dxx*fabsf(azoomsmoothed[n-1] - azoom[n-1]) )
          ;
      }
      //			azoomsmoothed[n] = azoomcumul[n]; // debug - no azoom smoothing
      if (azoomsmoothed[n] > 1)
        azoomsmoothed[n] = 1;  // not decrease image size
      // make zoom transform
      motion2transform(0, 0, 0, azoomsmoothed[n], pixaspect / nfields, xcenter, ycenter, 1, 1.0, &trtemp); // added in v.1.5.0
      // get non-adaptive image zoom from transform
//				transform2motion (trsmoothed[n], 1, xcenter, ycenter, pixaspect/nfields, &dxdif, &dydif, &rotdif, &zoomdif); // disabled in v.1.5.0
        // modify transform with adaptive zoom added
//				motion2transform (dxdif, dydif, rotdif, zoomdif*azoomsmoothed[n], pixaspect/nfields,  xcenter,  ycenter, 1, 1.0, &trsmoothed[n]); // replaced in v.1.5.0 by:
      sumtransform(trsmoothed[n], trtemp, &trsmoothed[n]); // added v.1.5.0

    }
  }
  else
  {
    motion2transform(0, 0, 0, initzoom, pixaspect / nfields, xcenter, ycenter, 1, 1.0, &trtemp); // added in v.1.7
    sumtransform(trsmoothed[ndest], trtemp, &trsmoothed[ndest]); // added v.1.7
  }

  // calculate difference between smoothed and original non-smoothed cumulative tranform
  // it will be used as stabilization values

  inversetransform(trcumul[ndest], &trinv);
  sumtransform(trinv, trsmoothed[ndest], ptrdif);
}



void DePanStabilize::Average(int nbase, int ndest, int nmax, transform * ptrdif)
{
  int n;
  transform trinv, trcur, trtemp;

  float norm = 0;
  trsmoothed[ndest].dxc = 0;
  trsmoothed[ndest].dyc = 0;
  trsmoothed[ndest].dxy = 0;
  for (n = nbase; n < ndest; n++)
  {
    trsmoothed[ndest].dxc += trcumul[n].dxc*wint[ndest - n];
    trsmoothed[ndest].dyc += trcumul[n].dyc*wint[ndest - n];
    trsmoothed[ndest].dxy += trcumul[n].dxy*wint[ndest - n];
    norm += wint[ndest - n];
  }
  for (n = ndest; n <= nmax; n++)
  {
    trsmoothed[ndest].dxc += trcumul[n].dxc*wint[n - ndest];
    trsmoothed[ndest].dyc += trcumul[n].dyc*wint[n - ndest];
    trsmoothed[ndest].dxy += trcumul[n].dxy*wint[n - ndest];
    norm += wint[n - ndest];
  }
  trsmoothed[ndest].dxc /= norm;
  trsmoothed[ndest].dyc /= norm;
  trsmoothed[ndest].dxy /= norm;
  trsmoothed[ndest].dyx = -trsmoothed[ndest].dxy*(pixaspect / nfields)*(pixaspect / nfields); // must be consistent
  norm = 0;
  trsmoothed[ndest].dxx = 0;
  for (n = max(nbase, ndest - 1); n < ndest; n++) { // very short interval
    trsmoothed[ndest].dxx += trcumul[n].dxx*wint[ndest - n];
    norm += wint[ndest - n];
  }
  for (n = ndest; n <= min(nmax, ndest + 1); n++) {
    trsmoothed[ndest].dxx += trcumul[n].dxx*wint[n - ndest];
    norm += wint[n - ndest];
  }
  trsmoothed[ndest].dxx /= norm;
  trsmoothed[ndest].dyy = trsmoothed[ndest].dxx;

  //			motion2transform (0, 0, 0, initzoom, pixaspect/nfields, xcenter, ycenter, 1, 1.0, &trtemp); // added in v.1.7
  //			sumtransform (trsmoothed[ndest],trtemp,  &trsmoothed[ndest]); // added v.1.7

  if (addzoom) { // calculate and add adaptive zoom factor to fill borders (for all frames from base to ndest)

    int nbasez = max(nbase, ndest - winfzsize);
    int nmaxz = min(nmax, ndest + winrzsize);
    // symmetrical
//               nmaxz = ndest + min(nmaxz-ndest, ndest-nbasez);
//               nbasez = ndest - min(nmaxz-ndest, ndest-nbasez);

    azoom[nbasez] = initzoom;
    for (n = nbasez + 1; n <= nmaxz; n++) {
      // get inverse transform
      inversetransform(trcumul[n], &trinv);
      // calculate difference between smoothed and original non-smoothed cumulative transform
//					sumtransform(trinv, trsmoothed[n], &trcur);
      sumtransform(trinv, trcumul[n], &trcur);
      // find adaptive zoom factor
      azoom[n] = initzoom;
      float azoomtest = 1 + (trcur.dxc + trcur.dxy*ycenter) / xcenter; // xleft
      if (azoomtest < azoom[n]) azoom[n] = azoomtest;
      azoomtest = 1 - (trcur.dxc + trcur.dxx*vi.width + trcur.dxy*ycenter - vi.width) / xcenter; //xright
      if (azoomtest < azoom[n]) azoom[n] = azoomtest;
      azoomtest = 1 + (trcur.dyc + trcur.dyx*xcenter) / ycenter; // ytop
      if (azoomtest < azoom[n]) azoom[n] = azoomtest;
      azoomtest = 1 - (trcur.dyc + trcur.dyx*xcenter + trcur.dyy*height - height) / ycenter; //ybottom
      if (azoomtest < azoom[n]) azoom[n] = azoomtest;
      //					azoom[n] = initzoom;
    }

    // smooth adaptive zoom
    // zoom time factor
//					zf = 1/(cutoff*tzoom);

    norm = 0;
    azoomsmoothed[ndest] = 0.0;
    for (n = nbasez; n < ndest; n++)
    {
      azoomsmoothed[ndest] += azoom[n] * winfz[ndest - n]; // fall
      norm += winfz[ndest - n];
    }
    for (n = ndest; n <= nmaxz; n++)
    {
      azoomsmoothed[ndest] += azoom[n] * winrz[n - ndest]; // rize
      norm += winrz[n - ndest];
    }
    azoomsmoothed[ndest] /= norm;
    //		sprintf(debugbuf,"DePanStabilize: nbase=%d ndest=%d nmax=%d z=%f zs=%f norm=%f\n", nbase, ndest, nmax, azoom[ndest], azoomsmoothed[ndest], norm );
    //		OutputDebugString(debugbuf);

    //					zf = zf*0.7f; // slower zoom decreasing
    //					if (azoomsmoothed[n] > azoomsmoothed[n-1]) // added in v.1.4.0 for slower zoom decreasing
    //					{
    //					}

              //azoomsmoothed[ndest] = azoom[ndest]; // debug - no azoom smoothing

    if (azoomsmoothed[ndest] > 1)
      azoomsmoothed[ndest] = 1;  // not decrease image size
    // make zoom transform
    motion2transform(0, 0, 0, azoomsmoothed[ndest], pixaspect / nfields, xcenter, ycenter, 1, 1.0, &trtemp);
    sumtransform(trsmoothed[ndest], trtemp, &trsmoothed[ndest]);

    //			}
  }
  else // no addzoom
  {
    motion2transform(0, 0, 0, initzoom, pixaspect / nfields, xcenter, ycenter, 1, 1.0, &trtemp); // added in v.1.7
    sumtransform(trsmoothed[ndest], trtemp, &trsmoothed[ndest]); // added v.1.7
  }
  // calculate difference between smoothed and original non-smoothed cumulative tranform
  // it will be used as stabilization values

  inversetransform(trcumul[ndest], &trinv);
  sumtransform(trinv, trsmoothed[ndest], ptrdif);
}



void DePanStabilize::InertialLimit(float *dxdif, float *dydif, float *zoomdif, float *rotdif, int ndest, int *nbase)
{
    // limit max motion corrections
  if (!(_finite(*dxdif))) // check added in v.1.1.3
  {// infinite or NAN
    *dxdif = 0;
    *dydif = 0;
    *zoomdif = initzoom;
    *rotdif = 0;
    *nbase = ndest;
  }
  else if (fabsf(*dxdif) > fabsf(dxmax))
  {
    if (dxmax >= 0)
    {
      *dxdif = *dxdif >= 0 ? sqrt(*dxdif*dxmax) : -sqrt(-*dxdif*dxmax); // soft limit v.1.8.2
      if (fabsf(*dxdif) > fabsf(dxmax)) // if still bigger, second pass P.F.
        *dxdif = *dxdif >= 0 ? sqrt(*dxdif*dxmax) : -sqrt(-*dxdif*dxmax); // soft limit v.1.8.2
      if (fabsf(*dxdif) > fabsf(dxmax*1.5f))
        *dxdif = *dxdif >= 0 ? dxmax : dxmax; // absolut limit: dxmax*1.5 P.F.
    }
    else
    {
      *dxdif = 0;
      *dydif = 0;
      *zoomdif = initzoom;
      *rotdif = 0;
      *nbase = ndest;
    }
  }

  if (!(_finite(*dydif)))
  {// infinite or NAN
    *dxdif = 0;
    *dydif = 0;
    *zoomdif = initzoom;
    *rotdif = 0;
    *nbase = ndest;
  }
  else if (fabsf(*dydif) > fabsf(dymax))
  {
    if (dymax >= 0)
    {
      *dydif = *dydif >= 0 ? sqrt(*dydif*dymax) : -sqrt(-*dydif*dymax); // soft limit v.1.8.2
      if (fabsf(*dydif) > fabsf(dymax)) // if still bigger, second pass P.F.
        *dydif = *dydif >= 0 ? sqrt(*dydif*dymax) : -sqrt(-*dydif*dymax); // soft limit v.1.8.2
      if (fabsf(*dydif) > fabsf(dymax*1.5f))
        *dydif = *dydif >= 0 ? dymax : dymax; // absolut limit: dxmax*1.5 P.F.
    }
    else
    {
      *dxdif = 0;
      *dydif = 0;
      *zoomdif = initzoom;
      *rotdif = 0;
      *nbase = ndest;
    }
  }

  if (!(_finite(*zoomdif)))
  {// infinite or NAN
    *dxdif = 0;
    *dydif = 0;
    *zoomdif = initzoom;
    *rotdif = 0;
    *nbase = ndest;
  }
  else if (fabsf(*zoomdif - 1) > fabsf(zoommax) - 1)
  {
    if (zoommax >= 0)
    {
      *zoomdif = *zoomdif >= 1 ? 1 + sqrt(fabsf(*zoomdif - 1)*fabsf(zoommax - 1)) : 1 - sqrt(fabsf(*zoomdif - 1)*fabsf(zoommax - 1)); // soft limit v.1.8.2
    }
    else
    {
      *dxdif = 0;
      *dydif = 0;
      *zoomdif = initzoom;
      *rotdif = 0;
      *nbase = ndest;
    }
  }

  if (!(_finite(*rotdif)))
  {// infinite or NAN
    *dxdif = 0;
    *dydif = 0;
    *zoomdif = initzoom;
    *rotdif = 0;
    *nbase = ndest;
  }
  else if (fabsf(*rotdif) > fabsf(rotmax))
  {
    if (rotmax >= 0)
    {
      *rotdif = *rotdif >= 0 ? sqrt(*rotdif*rotmax) : -sqrt(-*rotdif*rotmax); // soft limit v.1.8.2
    }
    else
    {
      *dxdif = 0;
      *dydif = 0;
      *zoomdif = initzoom;
      *rotdif = 0;
      *nbase = ndest;
    }
  }
}


float DePanStabilize::Averagefraction(float dxdif, float dydif, float zoomdif, float rotdif)
{
  float fractionx = fabsf(dxdif) / fabsf(dxmax);
  float fractiony = fabsf(dydif) / fabsf(dymax);
  float fractionz = fabsf(zoomdif - 1) / fabsf((fabsf(zoommax) - 1));
  float fractionr = fabsf(rotdif) / fabsf(rotmax);

  float fraction = max(fractionx, fractiony);
  fraction = max(fraction, fractionz);
  fraction = max(fraction, fractionr);
  return fraction;

}

// ****************************************************************************
//
PVideoFrame __stdcall DePanStabilize::GetFrame(int ndest, IScriptEnvironment* env) {
  // This is the implementation of the GetFrame function.
  // See the header definition for further info.
  PVideoFrame src, dst, dataframe;
  BYTE *dstp, *dstpU, *dstpV;
  const BYTE *srcp;
  //const BYTE  *srcpU, *srcpV;
  int dst_pitch, dst_pitchUV;
  int src_width, src_height, src_pitch;
  //int src_pitchUV, src_widthUV, src_heightUV;
  const BYTE *datap;
  float dxdif, dydif, zoomdif, rotdif;
  int border;
  //borderUV;
  int n, error;
  // char messagebuf[32];
  //	char debugbuf[100];
  //	int nfields;
  int nbasenew;
  int nprev, nnext;
  int notfilled;
  //	float azoomtest;

  //	float zf;

  transform trcur, trnull, trinv, trdif, trtemp, trY, trUV;

  float zoom = 1; // make null transform
  motion2transform(0, 0, 0, zoom, 1, 0, 0, 1, 1.0, &trnull);

  //int isYUY2 = vi.IsYUY2();//v1.6
  int xmsg;

#ifdef _DEBUG
  _RPT1(0, "DePanStabilize::GetFrame starts: ndest=%d \n", ndest);
#endif

// correction for fieldbased
//	if (fieldbased != 0) 	nfields = 2;
//	else nfields = 1;

// ---------------------------------------------------------------------------
  // Get motion info from the DePanData clip frame

  if (method == 0) // inertial
  {
    float framesback = 10 * fps / cutoff;
    if (framesback > fps * 5) framesback = fps * 5; // PF limit to 5 seconds
    nbasenew = int(ndest - framesback); // range of almost full stablization - increased in v 1.4.0
    // fps   cutoff   backframes  seconds
    // 16      0.5     320          20      seems too much, maybe we could limit it?
    // 16      1.0     160          10 
    // 16      2.0      80           5 
    // 50      0.5    1000          20      seems too much, maybe we could limit it? 
    // 50      2.0     250           5
    // 50     10.0      50           1
  }
  else if (method == 1)
    nbasenew = ndest - radius;
  else nbasenew = 0; // 1.13

  if (nbasenew < 0) nbasenew = 0;
  if (nbasenew > nbase || method == 1) nbase = nbasenew; // increase base to limit range
  if (nbase > ndest) nbase = nbasenew; // correction after backward scan

  int nmax;
  if (method == 1)
    nmax = min(ndest + radius, vi.num_frames - 1); // max n to take into account
  else
    nmax = ndest;

#ifdef _DEBUG
  _RPT2(0, "DePanStabilize::GetFrame get motion info START: about frames in interval from begin source to dest in reverse order. nbase=%d->ndest=%d \n", nbase, ndest);
  sprintf(debugbuf, "DePanStabilize::GetFrame get motion info START: about frames in interval from begin source to dest in reverse order. nbase=%d->ndest=%d \n", nbase, ndest);
  LogToFile(debugbuf);
#endif
  //if (debuglogfile != NULL) { fprintf(debuglogfile, "DePanStabilize::GetFrame get motion info about frames in interval from begin source to dest in reverse order. nbase=%d->ndest=%d \n",nbase,ndest); }
  // get motion info about frames in interval from begin source to dest in reverse order

  float max_valid_x_motion = min(dxmax * 3, xcenter / 40); // PF
  float max_valid_y_motion = min(dymax * 3, ycenter / 40); // PF

  for (n = nbase; n <= ndest; n++) {

    if (motionx[n] == MOTIONUNKNOWN) { // motion data is unknown for needed frame
      // note: if inputlogfile has been read, all motion data is always known
#ifdef _DEBUG
      _RPT1(0, "  DePanStabilize::GetFrame Request frame n from the DePanData clip. n=%d \n", n);
      sprintf(debugbuf, "  DePanStabilize::GetFrame Request frame n from the DePanData clip. n=%d \n", n);
      LogToFile(debugbuf);
#endif
      //if (debuglogfile != NULL) { fprintf(debuglogfile, "DePanStabilize::GetFrame Request frame n from the DePanData clip. n=%d \n", n); } // PF debug

      // Request frame n from the DePanData clip.
      // It must contains coded motion data in framebuffer
      dataframe = DePanData->GetFrame(n, env);
      // get pointer to data frame
      datap = dataframe->GetReadPtr();
      // get motiondata from DePanData clip framebuffer
      error = read_depan_data(datap, motionx, motiony, motionzoom, motionrot, n);
      // check if correct
      if (error != 0) env->ThrowError("DePanStabilize: data clip is NOT good DePanEstimate clip !");
      // log first. arrays are vi.num_frames long
#ifdef _DEBUG
      _RPT5(0, "  DePanStabilize::GetFrame DePanData read. n=%d motionx=%f motiony=%f motionzoom=%f motionrot=%f \n", n, motionx[n], motiony[n], motionzoom[n], motionrot[n]);
      sprintf(debugbuf, "  DePanStabilize::GetFrame DePanData read. n=%d motionx=%f motiony=%f motionzoom=%f motionrot=%f \n", n, motionx[n], motiony[n], motionzoom[n], motionrot[n]);
      LogToFile(debugbuf);
#endif

      float mx = motionx[n];
      float my = motiony[n];
      /* PF experiment
      if (fabs(mx) > max_valid_x_motion)
        motionx[n] = mx < 0 ? -max_valid_x_motion : max_valid_x_motion; // MOTIONBAD; 16.04.22 avoid jumping the frame back to 0
      if (fabs(my) > max_valid_y_motion)
        motiony[n] = my < 0 ? -max_valid_y_motion : max_valid_y_motion; // 16.04.22
      */
      if (fabs(mx) > max_valid_x_motion)
        motionx[n] = MOTIONBAD;
      if (fabs(my) > max_valid_y_motion)
        motionx[n] = MOTIONBAD;
    }

    //		if (motionx[n] == MOTIONBAD ) break; // if strictly =0,  than no good
  }

  for (n = ndest; n >= nbase; n--) {
    /* PF experiment
    float mx = motionx[n];
    float my = motiony[n];
    if (fabs(mx) > max_valid_x_motion)
      motionx[n] = MOTIONBAD;
    if (fabs(my) > max_valid_y_motion)
      motionx[n] = MOTIONBAD;
      */
    if (motionx[n] == MOTIONBAD) break; // if strictly =0,  than no good
  }

  // limit frame search range
  if (n > nbase) {
    nbase = n;  // set base frame to new scene start if found
#ifdef _DEBUG
    _RPT1(0, "DePanStabilize::GetFrame New nbase. n=%d \n", nbase);
    sprintf(debugbuf, "DePanStabilize::GetFrame New nbase. n=%d \n", nbase);
    LogToFile(debugbuf);
#endif
  }

  //		sprintf(debugbuf,"DePanStabilize: nbase=%d ndest=%d nmax=%d\n", nbase, ndest, nmax);
  //		OutputDebugString(debugbuf);
#ifdef _DEBUG
  _RPT3(0, "DePanStabilize: nbase=%d ndest=%d nmax=%d\n", nbase, ndest, nmax);
  sprintf(debugbuf, "DePanStabilize: nbase=%d ndest=%d nmax=%d\n", nbase, ndest, nmax);
  LogToFile(debugbuf);

  _RPT2(0, "DePanStabilize::GetFrame get motion Part#1 info START: ndest+1=%d -> nmax=%d \n", ndest + 1, nmax);
  sprintf(debugbuf, "DePanStabilize::GetFrame get motion Part#1 info START: ndest+1=%d -> nmax=%d \n", ndest + 1, nmax);
  LogToFile(debugbuf);
#endif
  for (n = ndest + 1; n <= nmax; n++) {

    if (motionx[n] == MOTIONUNKNOWN) { // motion data is unknown for needed frame
      // note: if inputlogfile has been read, all motion data is always known

      // Request frame n from the DePanData clip.
      // It must contains coded motion data in framebuffer
      dataframe = DePanData->GetFrame(n, env);
      // get pointer to data frame
      datap = dataframe->GetReadPtr();
      // get motiondata from DePanData clip framebuffer
      error = read_depan_data(datap, motionx, motiony, motionzoom, motionrot, n);
      // check if correct
      if (error != 0) env->ThrowError("DePanStabilize: data clip is NOT good DePanEstimate clip !");
#ifdef _DEBUG
      _RPT5(0, "  DePanStabilize::GetFrame DePanData read. n=%d motionx=%f motiony=%f motionzoom=%f motionrot=%f \n", n, *motionx, *motiony, *motionzoom, *motionrot);
      sprintf(debugbuf, "  DePanStabilize::GetFrame DePanData read. n=%d motionx=%f motiony=%f motionzoom=%f motionrot=%f \n", n, *motionx, *motiony, *motionzoom, *motionrot);
      LogToFile(debugbuf);
#endif
    }
    /* PF experiment when motion is way too much, treat it as invalid
    float mx = motionx[n];
    float my = motiony[n];
    if (fabs(mx) > max_valid_x_motion)
      motionx[n] = MOTIONBAD;
    if (fabs(my) > max_valid_y_motion)
      motionx[n] = MOTIONBAD;
      */
    if (motionx[n] == MOTIONBAD) break; // if strictly =0,  than no good
  }

  //		sprintf(debugbuf,"DePanStabilize: nbase=%d ndest=%d nmax=%d n=%d\n", nbase, ndest, nmax, n);
  //		OutputDebugString(debugbuf);
    // limit frame search range
  if (n < nmax) {
    nmax = max(n - 1, ndest);  // set max frame to new scene start-1 if found
  }

  if (method == 1)
  {	// symmetrical
    nmax = ndest + min(nmax - ndest, ndest - nbase);
    nbase = ndest - min(nmax - ndest, ndest - nbase);
  }

  //		sprintf(debugbuf,"DePanStabilize: nbase=%d ndest=%d nmax=%d\n", nbase, ndest, nmax);
  //		OutputDebugString(debugbuf);

  //if (debuglogfile != NULL) { fprintf(debuglogfile, "DePanStabilize. Begin motion2transforms: nbase=%d ndest=%d nmax=%d\n", nbase, ndest, nmax); }

  motion2transform(0, 0, 0, initzoom, pixaspect / nfields, xcenter, ycenter, 1, 1.0, &trtemp); // added in v.1.7
  if (nbase == ndest && method != 1) { // we are at new scene start,
//		trdif null transform
    sumtransform(trtemp, trnull, &trdif); //v1.7
  }
  //      first next frame too - v.1.12
  else { // prepare stabilization data by estimation and smoothing of cumulative motion

    // cumulative transform (position) for all sequence from base

    // ------ debug!
#ifdef _DEBUG
    _RPT1(0, "-- START: cumulative transform (position) for all sequence from nbase=%d \n", nbase);
    if (nbase == 4 && ndest == 6) // !! Specific clip debug!
    {
      _RPT2(0, "DePanStabilize. will be problem nbase=%d ndest=%d\n", nbase, ndest);
      sprintf(debugbuf, "DePanStabilize. will be problem \n");
      LogToFile(debugbuf);
      // if (debuglogfile != NULL) { fprintf(debuglogfile, "DePanStabilize. will be problem \n"); }
    }
#endif

    // base as null
    sumtransform(trnull, trnull, &trcumul[nbase]);// v.1.8.1

    // get cumulative transforms from base to ndest
    for (n = nbase + 1; n <= nmax; n++) {
      float mx = motionx[n];
      float my = motiony[n];
      motion2transform(motionx[n], motiony[n], motionrot[n], motionzoom[n], pixaspect / nfields, xcenter, ycenter, 1, 1.0, &trcur);
      sumtransform(trcumul[n - 1], trcur, &trcumul[n]);
#ifdef _DEBUG
      _RPT5(0, "  cumul[%d].dxc,dyc=%f,%f  MotionX,Y=%f,%f\n", n, trcumul[n].dxc, trcumul[n].dyc, mx, my);
      sprintf(debugbuf, "  cumul[%d].dxc,dyc=%f,%f  MotionX,Y=%f,%f\n", n, trcumul[n].dxc, trcumul[n].dyc, mx, my);
      LogToFile(debugbuf);
#endif
    }
#ifdef _DEBUG
    _RPT0(0, "-- END: cumulative transform (position) for all sequence\n");
#endif

    if (method == 0)// (inertial)
    {
#ifdef _DEBUG
      _RPT0(0, "DePanStabilize. Inertial\n");
#endif
      //  if (debuglogfile != NULL) { fprintf(debuglogfile, "DePanStabilize. Inertial\n"); }
      DePanStabilize::Inertial(nbase, ndest, &trdif);
#ifdef _DEBUG
      // chaotic result, debug point
      if (trdif.dxc < -10000. || trdif.dxc > 10000.)
      {
        _RPT1(0, "DePanStabilize. Inertial: dxc=%f\n", trdif.dxc);
      }
#endif
      // summary motion from summary transform
      transform2motion(trdif, 1, xcenter, ycenter, pixaspect / nfields, &dxdif, &dydif, &rotdif, &zoomdif);
      // fit last - decrease motion correction near end of clip - added in v.1.2.0

      if (vi.num_frames < fitlast + ndest + 1 && method == 0)
      {
        float endFactor = ((float)(vi.num_frames - ndest - 1)) / fitlast; // decrease factor
        dxdif *= endFactor;
        dydif *= endFactor;
        rotdif *= endFactor;
        zoomdif = initzoom + (zoomdif - initzoom)*endFactor;
      }

      _RPT0(0, "DePanStabilize. InertialLimit\n");

      DePanStabilize::InertialLimit(&dxdif, &dydif, &zoomdif, &rotdif, ndest, &nbase);
    }
    else if (method == 1) // windowed average
    {
      _RPT0(0, "DePanStabilize. method=1, Windowed average\n");
      //if (debuglogfile != NULL) { fprintf(debuglogfile, "DePanStabilize. method=1, Windowed average\n"); }
      int radius1 = radius;
      for (int iter = 0; iter < 1; iter++)
      {
        DePanStabilize::Average(nbase, ndest, nmax, &trdif);
        // summary motion from summary transform
        transform2motion(trdif, 1, xcenter, ycenter, pixaspect / nfields, &dxdif, &dydif, &rotdif, &zoomdif);
        float fraction = DePanStabilize::Averagefraction(dxdif, dydif, zoomdif, rotdif);
      }
      radius = radius1;
    }
    else if (method == 2) // full stabilization - v1.13
    {
      inversetransform(trcumul[nmax], &trdif);
      transform2motion(trdif, 1, xcenter, ycenter, pixaspect / nfields, &dxdif, &dydif, &rotdif, &zoomdif);
    }
    else // (method==-1) // tracking - v1.13
    {
      transform2motion(trcumul[nmax], 1, xcenter, ycenter, pixaspect / nfields, &dxdif, &dydif, &rotdif, &zoomdif);
    }


    //		char debugbuf[96]; // buffer for debugview utility
    //		int debug=1;
    //		if (debug != 0) { // debug mode
          // output data for debugview utility
    //			sprintf(debugbuf,"DePanStabilize: frame %d dx=%7.2f dy=%7.2f zoom=%7.5f\n", ndest, dxdif, dydif, zoomdif);
    //			OutputDebugString(debugbuf);
    //		}


        // summary motion from summary transform after max correction
    motion2transform(dxdif, dydif, rotdif, zoomdif, pixaspect / nfields, xcenter, ycenter, 1, 1.0, &trdif);
  }

  // final motion dif for info
  transform2motion(trdif, 1, xcenter, ycenter, pixaspect / nfields, &dxdif, &dydif, &rotdif, &zoomdif);

  _RPT0(0, "DePanStabilize. Construct a new frame based on the information of the current frame\n");

// ---------------------------------------------------------------------------
//output motion to AviSynth (float) variables - added in v.1.8 as requested by AI
  if (vdx != "")
  {
    vdx_val = dxdif; // store to static place
    env->SetVar(vdx, vdx_val); // by reference!
  }
  if (vdy != "")
  {
    vdy_val = dydif;
    env->SetVar(vdy, vdy_val);
  }
  if (vzoom != "")
  {
    vzoom_val = zoomdif;
  }
  env->SetVar(vzoom, vzoom_val);
  if (vrot != "")
  {
    vrot_val = rotdif;
    env->SetVar(vrot, vrot_val);
  }
  // ---------------------------------------------------------------------------

  VideoInfo vi_dst;
  dst = env->NewVideoFrame(vi);
  vi_dst = vi; // same

  // YV12/YV16/YV24/YUV420P16/YUV422P16/YUV444P16/Y8/Y16
  dstp = dst->GetWritePtr();
  dst_pitch = dst->GetPitch();
  // additional info on the U and V planes
  dstpU = dst->GetWritePtr(PLANAR_U);
  dstpV = dst->GetWritePtr(PLANAR_V);
  dst_pitchUV = dst->GetPitch(PLANAR_U);	// The pitch,height and width information

  // --------------------------------------------------------------------

  // Ready to make motion stabilization,
  // use some previous frame to fill borders
  notfilled = 1;  // init as not filled (borders by neighbor frames)
  float dxt1, dyt1, rott1, zoomt1;
  float dabsmin;

  // three passes: fill from prev, fill from next, process current
  for (int fillprev0next1current2 = 0; fillprev0next1current2 < 3; fillprev0next1current2++)
  {
    if (fillprev <= 0 && 0 == fillprev0next1current2) continue;
    if (fillnext <= 0 && 1 == fillprev0next1current2) continue;

    int frame_to_copy;

    if (0 == fillprev0next1current2)
    {
      // fillprev
      _RPT1(0, "DePanStabilize. FillPrev >0: %d\n", fillprev);
      //if (debuglogfile != NULL) { fprintf(debuglogfile, "DePanStabilize. FillPrev >0: %d\n",fillprev); }
      nprev = ndest - fillprev; // get prev frame number
      if (nprev < nbase) nprev = nbase; //  prev distance not exceed base
      int nprevbest = nprev;
      dabsmin = 10000;
      trY = trdif; // luma transform

      for (n = ndest - 1; n >= nprev; n--) {  // summary inverse transform
        motion2transform(motionx[n + 1], motiony[n + 1], motionrot[n + 1], motionzoom[n + 1], pixaspect / nfields, xcenter, ycenter, 1, 1.0, &trcur);
        trtemp = trY;
        nprevbest = n;
        sumtransform(trtemp, trcur, &trY);
        transform2motion(trY, 1, xcenter, ycenter, pixaspect / nfields, &dxt1, &dyt1, &rott1, &zoomt1);
        if ((fabs(dxt1) + fabs(dyt1) + ndest - n) < dabsmin) { // most centered and nearest
          dabsmin = fabs(dxt1) + fabs(dyt1) + ndest - n;
          nprevbest = n;
        }
      }
      frame_to_copy = nprevbest;
      notfilled = 0;  // mark as FILLED
    }
    else if (1 == fillprev0next1current2) {
      // use next frame to fill borders
      // fillnext
      nnext = ndest + fillnext;
      if (nnext >= vi.num_frames) nnext = vi.num_frames - 1;
      int nnextbest = nnext;
      dabsmin = 1000;
      trY = trdif; // luma transform for current frame

                   // get motion info about frames in interval from begin source to dest in reverse order
      for (n = ndest + 1; n <= nnext; n++) {

        if (motionx[n] == MOTIONUNKNOWN) {
          // motion data is unknown for needed frame
          // note: if inputlogfile has been read, all motion data is always known

          // Request frame n from the DePanData clip.
          // It must contains coded motion data in framebuffer
          dataframe = DePanData->GetFrame(n, env);
          // get pointer to data frame
          datap = dataframe->GetReadPtr();
          // get motiondata from DePanData clip framebuffer
          error = read_depan_data(datap, motionx, motiony, motionzoom, motionrot, n);
          // check if correct
          if (error != 0) env->ThrowError("DePan: data clip is NOT good DePanEstimate clip !");
        }

        if (motionx[n] != MOTIONBAD) { //if good
          motion2transform(motionx[n], motiony[n], motionrot[n], motionzoom[n], pixaspect / nfields, xcenter, ycenter, 1, 1.0, &trcur);
          inversetransform(trcur, &trinv);
          trtemp = trY;
          sumtransform(trinv, trtemp, &trY);
          transform2motion(trY, 1, xcenter, ycenter, pixaspect / nfields, &dxt1, &dyt1, &rott1, &zoomt1);
          if ((fabs(dxt1) + fabs(dyt1) + n - ndest) < dabsmin) { // most centered and nearest
            dabsmin = fabs(dxt1) + fabs(dyt1) + n - ndest;
            nnextbest = n;
          }
        }
        else { // bad
          nnextbest = n - 1;  // limit fill frame to last good
          break;
        }
      }

      //		motion2transform (motionx[nnext], motiony[nnext], motionrot[nnext], motionzoom[nnext], pixaspect/nfields,  xcenter,  ycenter, 1, 1.0, &trcur);
      frame_to_copy = nnextbest;
      notfilled = 0;  // mark as FILLED
    }
    else { // if (2 == fillprev0next1current2) 
      // get original current source frame
      // --------------------------------------------------------------------
      _RPT1(0, "DePanStabilize. get original current source frame. ndest= %d\n", ndest);
      //if (debuglogfile != NULL) { fprintf(debuglogfile, "DePanStabilize. get original current source frame. ndest= %d\n", ndest); }
      trY = trdif; // restore transform
      if (method == -1) // tracking - v1.13
        frame_to_copy = nbase;
      else // stabilization
        frame_to_copy = ndest;
    }

    // -------------
    // copy "frame_to_copy" frame

    src = child->GetFrame(frame_to_copy, env);

    _RPT5(0, "DePanStabilize phase#%d. subpixel=%d frame=%d dx=%f dy=%f\n", fillprev0next1current2, subpixel, nbase, dxdif, dydif);

    int planes[] = { PLANAR_Y, PLANAR_U, PLANAR_V };
    int plane_count = vi.IsY() ? 1 : 3;

    for (int p = 0; p < plane_count; p++)
    {
      const int plane = planes[p];

      int blur_current;
      int width_sub = vi.GetPlaneWidthSubsampling(plane);
      int height_sub = vi.GetPlaneHeightSubsampling(plane);

      if (notfilled == 0)
        border = -1;  // negative - borders is filled by prev, not by black!
      else
        border = (plane == PLANAR_Y) ? 0 : (1 << (bits_per_pixel - 1));

      blur_current = (plane == PLANAR_Y) ? blur : blur >> width_sub; // e.g. isYV12 ? blur / 2 : blur

      src_pitch = src->GetPitch(plane);
      src_width = src->GetRowSize(plane);
      src_height = src->GetHeight(plane);
      srcp = src->GetReadPtr(plane);

      BYTE *dstp_current;
      int dst_pitch_current;

      switch (plane)
      {
      case PLANAR_Y: dstp_current = dstp; dst_pitch_current = dst_pitch; break;
      case PLANAR_U: dstp_current = dstpU; dst_pitch_current = dst_pitchUV; break;
      case PLANAR_V: dstp_current = dstpV; dst_pitch_current = dst_pitchUV; break;
      }

      // memo: 
      // move: xc/yc 
      // rot : xy/yx 
      // zoom: xx,yy

      transform *tr_current;
      if (plane == PLANAR_Y)
        tr_current = &trY;
      else {
        // for UV plane
        float rot_ratio = float(1 << width_sub) / (1 << height_sub); //  YV12:2/2=>1 YV24:1/1=>1 YV16:2/1=>2
        trUV.dxc = trY.dxc / (1 << width_sub); // e.g. / 2 if YV12;
        trUV.dxx = trY.dxx; // zoom
        trUV.dxy = trY.dxy / rot_ratio; // rotation.

        trUV.dyc = trY.dyc / (1 << height_sub); // e.g. / 2 if YV12
        trUV.dyy = trY.dyy; // zoom
        trUV.dyx = trY.dyx * rot_ratio; // rotation

        tr_current = &trUV;
      }
#ifdef _DEBUG
      t_start = std::chrono::high_resolution_clock::now();
#endif
        // move src frame plane by vector to partially motion compensated position
        // fillprev/next: always "nearest"
      if (subpixel == 0 || fillprev0next1current2 != 2) {
        //subpixel=0, nearest pixel accuracy
        if (pixelsize == 1)
          compensate_plane_nearest2<uint8_t>(dstp_current, dst_pitch_current, srcp, src_pitch, src_width, src_height, *tr_current, mirror*notfilled, border, blur_current, bits_per_pixel);
        else
          compensate_plane_nearest2<uint16_t>(dstp_current, dst_pitch_current, srcp, src_pitch, src_width, src_height, *tr_current, mirror*notfilled, border, blur_current, bits_per_pixel);
      }
      else if (subpixel == 1) {  // bilinear interpolation
        if (pixelsize == 1)
          compensate_plane_bilinear2<uint8_t>(dstp_current, dst_pitch_current, srcp, src_pitch, src_width, src_height, *tr_current, mirror*notfilled, border, blur_current, bits_per_pixel);
        else
          compensate_plane_bilinear2<uint16_t>(dstp_current, dst_pitch_current, srcp, src_pitch, src_width, src_height, *tr_current, mirror*notfilled, border, blur_current, bits_per_pixel);
      }
      else if (subpixel == 2) { // bicubic interpolation
        if (pixelsize == 1)
          compensate_plane_bicubic2<uint8_t>(dstp_current, dst_pitch_current, srcp, src_pitch, src_width, src_height, *tr_current, mirror*notfilled, border, blur_current, bits_per_pixel);
        else
          compensate_plane_bicubic2<uint16_t>(dstp_current, dst_pitch_current, srcp, src_pitch, src_width, src_height, *tr_current, mirror*notfilled, border, blur_current, bits_per_pixel);
      }
#ifdef _DEBUG
      t_end = std::chrono::high_resolution_clock::now();
      std::chrono::duration<double> elapsed_seconds = t_end - t_start;
      _RPT2(0, "Compensate plane %d %f\r\n", p, elapsed_seconds);
#endif
    } // for planes
  } // prev/next/current frame phase

  if (debuglogfile != NULL) {
    fflush(debuglogfile);
  }

  if (info) { // show text info on frame image
    xmsg = (nfields != 1) ? (ndest % 2) * 15 : 0; // x-position of odd fields message
    char messagebuf[32];
    sprintf_s(messagebuf, " DePanStabilize");
    DrawString(dst, vi, xmsg, 1, messagebuf);
    sprintf_s(messagebuf, " frame=%7d", ndest);
    DrawString(dst, vi, xmsg, 2, messagebuf);
    if (nbase == ndest) sprintf_s(messagebuf, " BASE!=%7d", nbase); // CAPITAL letters
    else sprintf_s(messagebuf, " base =%7d", nbase);
    DrawString(dst, vi, xmsg, 3, messagebuf);
    sprintf_s(messagebuf, " dx  =%7.2f", dxdif);
    DrawString(dst, vi, xmsg, 4, messagebuf);
    sprintf_s(messagebuf, " dy  =%7.2f", dydif);
    DrawString(dst, vi, xmsg, 5, messagebuf);
    sprintf_s(messagebuf, " zoom=%7.5f", zoomdif);
    DrawString(dst, vi, xmsg, 6, messagebuf);
    sprintf_s(messagebuf, " rot= %7.3f", rotdif);
    DrawString(dst, vi, xmsg, 7, messagebuf);
  }

  return dst;
}

//****************************************************************************
// This is the function that created the filter, when the filter has been called.
// This can be used for simple parameter checking, so it is possible to create different filters,
// based on the arguments recieved.

AVSValue __cdecl Create_DePanStabilize(AVSValue args, void* user_data, IScriptEnvironment* env) {
  return new DePanStabilize(args[0].AsClip(), // the 0th parameter is the source clip
    args[1].AsClip(),	// parameter  - motion data clip.
    (float)args[2].AsFloat(1.0f),	// parameter  - cutoff
    (float)args[3].AsFloat(0.9f),	// parameter  - damping  - default changed to 0.9 in v.1.1.4 (was really fixed =0.9 in all prev versions)
    (float)args[4].AsFloat(1.0f),	// parameter  - initial zoom
    args[5].AsBool(false),	// add adaptive zoom
    args[6].AsInt(0),		// fill border by prev
    args[7].AsInt(0),		// fill border by next
    args[8].AsInt(0),		// fill border by mirror
    args[9].AsInt(0),		// blur mirror len
    (float)args[10].AsFloat(60.0f),	// parameter  - dxmax
    (float)args[11].AsFloat(30.0f),	// parameter  - dymax
    (float)args[12].AsFloat(1.05f),	// parameter  - zoommax
    (float)args[13].AsFloat(1.0f),	// parameter  - rotmax
    args[14].AsInt(2),	// parameter  - subpixel
    (float)args[15].AsFloat(1.0f),	// parameter  - pixaspect
    args[16].AsInt(0), // fitlast
    (float)args[17].AsFloat(3.0f),	// parameter  - zoom rise time
    args[18].AsBool(false),	// parameter  - info
    args[19].AsString(""),  // inputlog filename
    args[20].AsString(""),  // dx global param
    args[21].AsString(""),  // dy global param
    args[22].AsString(""),  // zoom global param
    args[23].AsString(""),  // rot global param
    args[24].AsInt(0),	// parameter  - method
    args[25].AsString(""), // debuglog file name - PF
    env);
    // Calls the constructor with the arguments provided.
}


