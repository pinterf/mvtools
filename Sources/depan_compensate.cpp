/*
  DePan plugin for Avisynth 2.6 interface - global motion estimation and compensation of camera pan
  Version 1.9, November 5, 2006.
  Version 1.10.0, April 29, 2007
  Version 1.13.1, April 6, 2016
  Version 2.13.1, November 19, 2016 by pinterf
  Version 2.13.2, April 30, 2020 by pinterf - fix regeression: DepanInterleave allowed colorspace check
  (DePan and DePanInterleave functions)
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

  DePan function - generate clip with motion compensated frames,
  using motion data previously calculated by DePanEstimate

  Parameters of DePan:
    clip - input clip (the same as input clip in DePanEstimate)
    data - special service clip with coded motion data, produced by DePanEstimate
    offset - value of offset in frame units (or field)
    subpixel - pixel interpolation accuracy
    pixaspect - pixel aspect
    matchfields - match vertical position of interlaced fields for better denoising etc
    mirror - use mirror to fill border
    blur - blur mirror max length
    info - show motion info on frame
    inputlog - name of input log file in Deshaker format (default - none, not read)

    DePanInterleave function - generate long interleaved clip with motion compensated
  previous frames, original frames, and motion compensated next frames,
  using motion data previously calculated by DePanEstimate.
  Uses DePan and Interleave(internal) function

  Parameters of DePanInterleave:
    clip - input clip (the same as input clip in DePanEstimate)
    data - special service clip with coded motion data, produced by DePanEstimate
    prev - number of prev compesated clips
    next - number of next compesated clips
    subpixel - pixel interpolation accuracy
    pixaspect - pixel aspect
    matchfields - match vertical position of interlaced fields for better denoising etc
    mirror - use mirror to fill border
    blur - blur mirror max length
    info - show motion info on frame
    inputlog - name of input log file in Deshaker format (default - none, not read)

*/

#include "windows.h"
#include "avisynth.h"
#include "avs/alignment.h"
#include "math.h"
#include "stdio.h"
#include "stdint.h"
#include "info.h"
#include "depanio.h"
#include "depan.h"
#include "yuy2planes.h"
#include <chrono>
#if 0
// moved, common with mvtools yuy2planes
//------------------------------------------------------------------
void YUY2ToPlanes(const BYTE* srcp, int src_height, int src_width, int src_pitch,
  BYTE * srcplaneY, int planeYpitch, BYTE *srcplaneU, int planeUpitch, BYTE *srcplaneV, int planeVpitch)
{
  int h, w;
  for (h = 0; h < src_height; h++)
  {
    for (w = 0; w < src_width; w += 4)
    {
      srcplaneY[w >> 1] = srcp[w];
      srcplaneU[w >> 2] = srcp[w + 1];
      srcplaneY[(w >> 1) + 1] = srcp[w + 2];
      srcplaneV[w >> 2] = srcp[w + 3];
    }
    srcplaneY += planeYpitch;
    srcplaneU += planeUpitch;
    srcplaneV += planeVpitch;
    srcp += src_pitch;
  }
}
//------------------------------------------------------------------
void PlanesToYUY2(BYTE* dstp, int src_height, int src_width, int dst_pitch,
  BYTE * srcplaneY, int planeYpitch, BYTE *srcplaneU, int planeUpitch, BYTE *srcplaneV, int planeVpitch)
{
  int h, w;
  for (h = 0; h < src_height; h++)
  {
    for (w = 0; w < src_width; w += 4)
    {
      dstp[w] = srcplaneY[w >> 1];
      dstp[w + 1] = srcplaneU[w >> 2];
      dstp[w + 2] = srcplaneY[(w >> 1) + 1];
      dstp[w + 3] = srcplaneV[w >> 2];
    }
    srcplaneY += planeYpitch;
    srcplaneU += planeUpitch;
    srcplaneV += planeVpitch;
    dstp += dst_pitch;
  }
}
#endif

//****************************************************************************
//****************************************************************************
//****************************************************************************
//
//                DePan function
//	generate clip with motion compensated frames,
//	using motion data previously calculated by DePanEstimate
//
//****************************************************************************
//****************************************************************************
//****************************************************************************

class DePan : public GenericVideoFilter {
  // DePan defines the name of your filter class. 
  // This name is only used internally, and does not affect the name of your filter or similar.
  // This filter extends GenericVideoFilter, which incorporates basic functionality.
  // All functions present in the filter must also be present here.

  bool has_at_least_v8;

  // filter parameters
  PClip DePanData;  // motion data clip
  float offset;  // offset in frame units
  int subpixel;     // subpixel accuracy by interpolation
  float pixaspect; // pixel aspect
  int matchfields;  // match vertical position of fields for better denoising etc
  int mirror; // mirror empty border
  int blur; // blur mirror length
  int info;         // show motion info on frame
  const char *inputlog;  // filename of input log file in Deshaker format

// internal parameters
  int fieldbased;
  int TFF;
  int intoffset; // integer value of offset = from what neibour frame we will make compensation

// motion tables
  float * motionx;
  float * motiony;
  float * motionrot;
  float * motionzoom;


  struct {
  // planes from YUY2 - v1.6
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
  // moved here from GetFrame
  float xcenter;  // center of frame
  float ycenter;

  int pixelsize; // PF 161118
  int bits_per_pixel;

public:
  // This defines that these functions are present in your class.
  // These functions must be that same as those actually implemented.
  // Since the functions are "public" they are accessible to other classes.
  // Otherwise they can only be called from functions within the class itself.

  DePan(PClip _child, PClip _DePanData, float _offset, int _subpixel, float _pixaspect, int _matchfields,
    int _mirror, int _blur, int _info, const char * _inputlog, IScriptEnvironment* env);
  // This is the constructor. It does not return any value, and is always used, 
  //  when an instance of the class is created.
  // Since there is no code in this, this is the definition.

  ~DePan();
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
DePan::DePan(PClip _child, PClip _DePanData, float _offset, int _subpixel, float _pixaspect, int _matchfields,
  int _mirror, int _blur, int _info, const char * _inputlog, IScriptEnvironment* env) :
  GenericVideoFilter(_child), DePanData(_DePanData), offset(_offset), subpixel(_subpixel), pixaspect(_pixaspect), matchfields(_matchfields),
  mirror(_mirror), blur(_blur), info(_info), inputlog(_inputlog) {
  // This is the implementation of the constructor.
  // The child clip (source clip) is inherited by the GenericVideoFilter,
  //  where the following variables gets defined:
  //   PClip child;   // Contains the source clip.
  //   VideoInfo vi;  // Contains videoinfo on the source clip.

  has_at_least_v8 = true;
  try { env->CheckVersion(8); }
  catch (const AvisynthError&) { has_at_least_v8 = false; }

  int error;
  int loginterlaced;

  pixelsize = vi.ComponentSize();
  bits_per_pixel = vi.BitsPerComponent();

  fieldbased = (vi.IsFieldBased()) ? 1 : 0;
  TFF = (vi.IsTFF()) ? 1 : 0;

  if (!vi.IsYUY2() && !vi.IsYUV() && !vi.IsYUVA())
    env->ThrowError("DePan: input must be YUV planar or YUY2!"); // PF: all planar, inlcuding native 16 bits

  motionx = new float[vi.num_frames];
  motiony = new float[vi.num_frames];
  motionrot = new float[vi.num_frames];
  motionzoom = new float[vi.num_frames];



  // integer of offset, get frame for motion compensation
  if (offset > 0) {
    intoffset = (int)ceil(offset); // large ( = 1 for both 0.5 and 1
  }
  else {
    intoffset = (int)floor(offset); // large abs ( = -1 for both -0.5 and -1)
  }

  int cacherange = 1 + 2 * abs(intoffset); // changed in v1.6

#ifdef AVISYNTH_2_5
  child->SetCacheHints(CACHE_25_RANGE, cacherange); // enabled in v1.6 CACHE_RANGE -> CACHE_25_RANGE
#endif

  if (*inputlog) { // motion data will be readed from deshaker.log file once at start
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
      motionrot[i] = 0;  // zero rotation for all frames (it is not estimated in current version) 
    }
  }
  
  if (vi.IsYUY2()) // v1.6
  { // create planes from YUY2
    YUY2data.planeYwidth=vi.width;
    YUY2data.planeUVwidth=vi.width/2;
    YUY2data.planeYpitch = AlignNumber(vi.width, 16);
    YUY2data.planeUVpitch = AlignNumber(vi.width / 2, 16);
    YUY2data.srcplaneY = (BYTE*) malloc(YUY2data.planeYpitch*vi.height);
    YUY2data.srcplaneU = (BYTE*) malloc(YUY2data.planeUVpitch*vi.height);
    YUY2data.srcplaneV = (BYTE*) malloc(YUY2data.planeUVpitch*vi.height);
    YUY2data.dstplaneY = (BYTE*) malloc(YUY2data.planeYpitch*vi.height);
    YUY2data.dstplaneU = (BYTE*) malloc(YUY2data.planeUVpitch*vi.height);
    YUY2data.dstplaneV = (BYTE*) malloc(YUY2data.planeUVpitch*vi.height);
  }
  

  //  PF moved here
  xcenter = vi.width / 2.0f;  // center of frame
  ycenter = vi.height / 2.0f;

}

//****************************************************************************
// This is where any actual destructor code used goes
DePan::~DePan() {
  // This is where you can deallocate any memory you might have used.

  delete[] motionx;
  delete[] motiony;
  delete[] motionrot;
  delete[] motionzoom;
  //free(work2width4356);
  
  if (vi.IsYUY2()) // v1.6
  {
    free(YUY2data.srcplaneY);
    free(YUY2data.srcplaneU);
    free(YUY2data.srcplaneV);
    free(YUY2data.dstplaneY);
    free(YUY2data.dstplaneU);
    free(YUY2data.dstplaneV);
  }
  
}



//
// ****************************************************************************
//
PVideoFrame __stdcall DePan::GetFrame(int ndest, IScriptEnvironment* env) {
  // This is the implementation of the GetFrame function.
  // See the header definition for further info.
  PVideoFrame src, dst, dataframe;
  BYTE *dstp, *dstpU, *dstpV;
  const BYTE *datap;
  const BYTE * srcp;
  int src_pitch, dst_pitch;
  int dst_pitchUV;
  int motgood;
  int nsrc;
  int border;
  //, borderUV;
  int n;
  int error;
  //	char messagebuf[32];
  int nfields;
  int src_width;
  int src_height;
  float fractoffset;
  int isnodd;
  float dxsum, dysum, rotsum, zoomsum;
  float rotegree = 0;
  float zoom;

  transform tr, trnull, trsum;
  //transform trsumUV; // motion transform structures

  zoom = 1; // null transform
  motion2transform(0, 0, 0, zoom, 1, 0, 0, 1, 1.0, &trnull);

  motion2transform(0, 0, 0, zoom, 1, 0, 0, 1, 1.0, &trsum); // null as initial

  //int isYUY2 = vi.IsYUY2();
  // PF moved to constructor
  //float xcenter = vi.width/2.0f;  // center of frame
  //float ycenter;

  // correction for fieldbased

  if (fieldbased != 0) 	nfields = 2;
  else nfields = 1;

  // Get motion info from the DePanEstimate clip frame


  if (intoffset == 0) { //  NULL transform, return source
    src = child->GetFrame(ndest, env);
    return src;
  }


  // ---------------------------------------------------------------------------
  else if (intoffset > 0) { // FORWARD compensation from some previous frame

    // get prev frame number to make motion compensation
    nsrc = ndest - intoffset;
    fractoffset = (offset + 1 - intoffset);// fracture (mod) of offset, ( 0 < f <= 1)

    if (nsrc < 0) { //  no frames before 0
      src = child->GetFrame(ndest, env); // return current (as dest) source frame
      return src;
    }

    dxsum = 0;
    dysum = 0;
    motgood = 1;
    // get motion info about frames in interval from prev source to dest
    for (n = ndest; n > nsrc; n--) {

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
        if (error != 0) env->ThrowError("DePan: data clip is NOT good DePanEstimate clip !");
      }

      motion2transform(motionx[n], motiony[n], motionrot[n], motionzoom[n], pixaspect / nfields, xcenter, ycenter, 1, fractoffset, &tr);
      sumtransform(trsum, tr, &trsum);
      if (motionx[n] == MOTIONBAD) motgood = 0; // if any strictly =0,  than no good
    }
    if (motgood == 0) {
      sumtransform(trnull, trnull, &trsum); // null transform if any is no good
    }
    if ((fieldbased != 0) && (matchfields != 0)) {
      // reverse 1 line motion correction if matchfields mode
      isnodd = ndest % 2;  // =1 for odd field number n, =0 for even field number n
      if (TFF != 0) {
        trsum.dyc -= 0.5f - isnodd;  // TFF
      }
      else {
        trsum.dyc -= -0.5f + isnodd;  // BFF (or undefined?)
      }
    }


    // summary motion from summary transform
    transform2motion(trsum, 1, xcenter, ycenter, pixaspect / nfields, &dxsum, &dysum, &rotsum, &zoomsum);


  }
  else {
    // ---------------------------------------------------------------------------
        // (intoffset<0), BACKWARD compensation from some next frame
        // get next frame number to make motion compensation
    nsrc = ndest - intoffset;
    fractoffset = (offset - 1 - intoffset);// fracture (mod) of offset, (-1 <= f < 0)

    if (nsrc > vi.num_frames - 1) { //  no frames after last
      src = child->GetFrame(ndest, env); // return current (as dest) source frame
      return src;
    }

    dxsum = 0;
    dysum = 0;
    motgood = 1;
    // get motion info about frames in interval from  dest to source
    for (n = ndest + 1; n <= nsrc; n++) {

      if (motionx[n] == MOTIONUNKNOWN) { // motion data is unknown for needed frame
        // (note: if inputlogfile has been read, all motion data is always known)

        // Request frame n from the DePanData clip.
        // It must contains coded motion data in framebuffer
        dataframe = DePanData->GetFrame(n, env);
        // get pointer to data frame
        datap = dataframe->GetReadPtr();
        // get motiondata from DePanData clip framebuffer
        error = read_depan_data(datap, motionx, motiony, motionzoom, motionrot, n);
        // check if correct
        if (error != 0) env->ThrowError("DePan: data clip is NOT good DePanData clip !");
      }

      motion2transform(motionx[n], motiony[n], motionrot[n], motionzoom[n], pixaspect / nfields, xcenter, ycenter, 0, fractoffset, &tr);
      sumtransform(trsum, tr, &trsum);
      if (motionx[n] == MOTIONBAD) motgood = 0; // if any strictly =0,  than no good
    }
    if (motgood == 0) {
      sumtransform(trnull, trnull, &trsum); // null transform if any is no good
    }

    if ((fieldbased != 0) && (matchfields != 0)) {
      // reverse 1 line motion correction if matchfields mode
      isnodd = ndest % 2;  // =1 for odd field number n, =0 for even field number n
      if (TFF != 0) {
        trsum.dyc -= 0.5f - isnodd;  // TFF
      }
      else {
        trsum.dyc -= -0.5f + isnodd;  // BFF (or undefined?)
      }
    }


    // summary motion from summary transform
    transform2motion(trsum, 0, xcenter, ycenter, pixaspect / nfields, &dxsum, &dysum, &rotsum, &zoomsum);

  }

  // PF mod from here 
  // variables to minimize ugly copy-paste (from DepanStabilize) modifications
  int frame_to_copy = nsrc;
  transform trY = trsum;
  transform trUV;

  // same as in DepanStabilize::GetFrame
  // ---------------------------------------------------------------------------
  // Create destination frame

  VideoInfo vi_dst;
  dst = env->NewVideoFrame(vi); // v8 frameprop copy later
  vi_dst = vi; // same

  // YV12/YV16/YV24/YUV420P16/YUV422P16/YUV444P16/Y8/Y16
  dstp = dst->GetWritePtr();
  dst_pitch = dst->GetPitch();

  if (vi.IsYUY2()) // v1.6
  {
    const int cpuFlags = env->GetCPUFlags();
    src = child->GetFrame(frame_to_copy, env);
    if (has_at_least_v8) env->copyFrameProps(src, dst); // inherit frameprops

    src_pitch = src->GetPitch();
    src_width = vi.width;
    src_height = src->GetHeight();
    srcp = src->GetReadPtr();

    // create planes from YUY2
    void YUY2ToPlanes(const unsigned char* pSrcYUY2, int nSrcPitchYUY2, int nWidth, int nHeight,
      const unsigned char* pSrc1, int _srcPitch,
      const unsigned char* pSrcU1, const unsigned char* pSrcV1, int _srcPitchUV, int cpuFlags);

    YUY2ToPlanes(srcp, src_pitch, src_width, src_height, YUY2data.srcplaneY, YUY2data.planeYpitch, YUY2data.srcplaneU, YUY2data.srcplaneV, YUY2data.planeUVpitch, cpuFlags);

    // Process Luma Plane
    border = 0;  // luma=0, black

    // move src frame plane by vector to motion compensated position		
    if (subpixel == 2) { // bicubic interpolation
      compensate_plane_bicubic2<uint8_t>(YUY2data.dstplaneY, YUY2data.planeYpitch, YUY2data.srcplaneY, YUY2data.planeYpitch, YUY2data.planeYwidth, src_height, trsum, mirror, border, blur, bits_per_pixel);
    }
    else if (subpixel == 1) { // bilinear interpolation
      compensate_plane_bilinear2<uint8_t>(YUY2data.dstplaneY, YUY2data.planeYpitch, YUY2data.srcplaneY, YUY2data.planeYpitch, YUY2data.planeYwidth, src_height, trsum, mirror, border, blur, bits_per_pixel);
    }
    else {  //subpixel=0, nearest pixel accuracy
      compensate_plane_nearest2<uint8_t>(YUY2data.dstplaneY, YUY2data.planeYpitch, YUY2data.srcplaneY, YUY2data.planeYpitch, YUY2data.planeYwidth, src_height, trsum, mirror, border, blur, bits_per_pixel);
    }

    int borderUV = 128; // border color = grey if both U,V=128

    // scale transform for UV  (for half UV plane horizontal sizes)

    trsum.dxc = trsum.dxc / 2;
    trsum.dxx = trsum.dxx;
    trsum.dxy = trsum.dxy / 2;
    trsum.dyc = trsum.dyc;
    trsum.dyx = trsum.dyx * 2;
    trsum.dyy = trsum.dyy;

    // Process U plane
    if (subpixel == 2) { // bicubic interpolation
      compensate_plane_bicubic2<uint8_t>(YUY2data.dstplaneU, YUY2data.planeUVpitch, YUY2data.srcplaneU, YUY2data.planeUVpitch, YUY2data.planeUVwidth, src_height, trsum, mirror, borderUV, blur / 2, bits_per_pixel);
    }
    else if (subpixel == 1) {  // bilinear interpolation
      compensate_plane_bilinear2<uint8_t>(YUY2data.dstplaneU, YUY2data.planeUVpitch, YUY2data.srcplaneU, YUY2data.planeUVpitch, YUY2data.planeUVwidth, src_height, trsum, mirror, borderUV, blur / 2, bits_per_pixel); //  devide dx, dy by 2 for UV plane
    }
    else {  //subpixel=0, nearest pixel accuracy
      compensate_plane_nearest2<uint8_t>(YUY2data.dstplaneU, YUY2data.planeUVpitch, YUY2data.srcplaneU, YUY2data.planeUVpitch, YUY2data.planeUVwidth, src_height, trsum, mirror, borderUV, blur / 2, bits_per_pixel); //  devide dx, dy by 2 for UV plane
    }

    // Process V plane 
    if (subpixel == 2) { // bicubic interpolation
      compensate_plane_bicubic2<uint8_t>(YUY2data.dstplaneV, YUY2data.planeUVpitch, YUY2data.srcplaneV, YUY2data.planeUVpitch, YUY2data.planeUVwidth, src_height, trsum, mirror, borderUV, blur / 2, bits_per_pixel);
    }
    else if (subpixel == 1) {  // bilinear interpolation
      compensate_plane_bilinear2<uint8_t>(YUY2data.dstplaneV, YUY2data.planeUVpitch, YUY2data.srcplaneV, YUY2data.planeUVpitch, YUY2data.planeUVwidth, src_height, trsum, mirror, borderUV, blur / 2, bits_per_pixel); //  devide dx, dy by 2 for UV plane
    }
    else {  //subpixel=0, nearest pixel accuracy
      compensate_plane_nearest2<uint8_t>(YUY2data.dstplaneV, YUY2data.planeUVpitch, YUY2data.srcplaneV, YUY2data.planeUVpitch, YUY2data.planeUVwidth, src_height, trsum, mirror, borderUV, blur / 2, bits_per_pixel); //  devide dx, dy by 2 for UV plane
    }

    // create YUY2 from planes
    YUY2FromPlanes(dstp, dst_pitch, src_width, src_height, YUY2data.dstplaneY, YUY2data.planeYpitch, YUY2data.dstplaneU, YUY2data.dstplaneV, YUY2data.planeUVpitch, cpuFlags);

    int xmsg = (fieldbased != 0) ? (ndest % 2) * 15 : 0; // x-position of odd fields message
    if (info) { // show text info on frame image
      const int BUFSIZE = 1024;
      char messagebuf[BUFSIZE+1];
      sprintf_s(messagebuf, BUFSIZE, " DePan");
      DrawStringYUY2(dst, xmsg, 1, messagebuf);
      sprintf_s(messagebuf, BUFSIZE, " offset= %5.2f", offset);
      DrawStringYUY2(dst, xmsg, 2, messagebuf);
      sprintf_s(messagebuf, BUFSIZE, " %d to %d", ndest - intoffset, ndest);
    }
  }
  else {
  //YUV planar

  // additional info on the U and V planes
    dstpU = dst->GetWritePtr(PLANAR_U);
    dstpV = dst->GetWritePtr(PLANAR_V);
    dst_pitchUV = dst->GetPitch(PLANAR_U);	// The pitch,height and width information

    // --------------------------------------------------------------------

    // -------------
    // copy "frame_to_copy" frame

    //VideoInfo vi_src;

    src = child->GetFrame(frame_to_copy, env);
    if(has_at_least_v8) env->copyFrameProps(src, dst); // inherit frameprops v8

    // we are working with 
    //   PVideoFrame src  
    //   VideoInfo vi_src 
    // from now on

    int planes[] = { PLANAR_Y, PLANAR_U, PLANAR_V };
    int plane_count = vi.IsY() ? 1 : 3;
    // alpha plane undefined

    for (int p = 0; p < plane_count; p++)
    {
      const int plane = planes[p];

      int blur_current;
      int width_sub = vi.GetPlaneWidthSubsampling(plane);
      int height_sub = vi.GetPlaneHeightSubsampling(plane);

      border = (plane == PLANAR_Y) ? 0 : (1 << (bits_per_pixel - 1)); // no prev, fill by black

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
      auto t_start = std::chrono::high_resolution_clock::now();
#endif
    // move src frame plane by vector to partially motion compensated position
    // fillprev/next: always "nearest"
      if (subpixel == 0) {
        //subpixel=0, nearest pixel accuracy
        if (pixelsize==1)
          compensate_plane_nearest2<uint8_t>(dstp_current, dst_pitch_current, srcp, src_pitch, src_width, src_height, *tr_current, mirror, border, blur_current, bits_per_pixel);
        else
          compensate_plane_nearest2<uint16_t>(dstp_current, dst_pitch_current, srcp, src_pitch, src_width, src_height, *tr_current, mirror, border, blur_current, bits_per_pixel);
      }
      else if (subpixel == 1) {  // bilinear interpolation
        if (pixelsize==1)
          compensate_plane_bilinear2<uint8_t>(dstp_current, dst_pitch_current, srcp, src_pitch, src_width, src_height, *tr_current, mirror, border, blur_current, bits_per_pixel);
        else
          compensate_plane_bilinear2<uint16_t>(dstp_current, dst_pitch_current, srcp, src_pitch, src_width, src_height, *tr_current, mirror, border, blur_current, bits_per_pixel);
      }
      else if (subpixel == 2) { // bicubic interpolation
        if (pixelsize==1)
          compensate_plane_bicubic2<uint8_t>(dstp_current, dst_pitch_current, srcp, src_pitch, src_width, src_height, *tr_current, mirror, border, blur_current, bits_per_pixel);
        else
          compensate_plane_bicubic2<uint16_t>(dstp_current, dst_pitch_current, srcp, src_pitch, src_width, src_height, *tr_current, mirror, border, blur_current, bits_per_pixel);
      }
#ifdef _DEBUG
      auto t_end = std::chrono::high_resolution_clock::now();
      std::chrono::duration<double> elapsed_seconds = t_end - t_start;
      _RPT2(0, "Compensate plane %d %f\r\n", p, elapsed_seconds);
#endif
    } // for planes

    int xmsg = (fieldbased != 0) ? (ndest % 2) * 15 : 0; // x-position of odd fields message
    if (info) { // show text info on frame image
      char messagebuf[32];
      sprintf_s(messagebuf, " DePan");
      DrawString(dst, vi, xmsg, 1, messagebuf);
      sprintf_s(messagebuf, " offset= %5.2f", offset);
      DrawString(dst, vi, xmsg, 2, messagebuf);
      sprintf_s(messagebuf, " %d to %d", ndest - intoffset, ndest);
      DrawString(dst, vi, xmsg, 3, messagebuf);
      sprintf_s(messagebuf, " dx  = %7.2f", dxsum);
      DrawString(dst, vi, xmsg, 4, messagebuf);
      sprintf_s(messagebuf, " dy  = %7.2f", dysum);
      DrawString(dst, vi, xmsg, 5, messagebuf);
      sprintf_s(messagebuf, " zoom= %7.5f", zoomsum);
      DrawString(dst, vi, xmsg, 6, messagebuf);
      sprintf_s(messagebuf, " rot= %7.3f", rotsum);
      DrawString(dst, vi, xmsg, 7, messagebuf);
    }
  } // end YUV planar


  return dst;
#if 0
  // PF: old code, kept here for reference
  // ---------------------------------------------------------------------------
      // get original source frame
  src = child->GetFrame(nsrc, env);

  // Request a Read pointer from the source frame.
  // This will return the position of the upperleft pixel in YV12 images,
  srcp = src->GetReadPtr();
  src_width = src->GetRowSize();
  src_height = src->GetHeight();
  // Requests pitch (length of a line) of the destination image.
  // (short version - pitch is always equal to or greater than width to allow for seriously fast assembly code)
  src_pitch = src->GetPitch();


  // Construct a new frame based on the information of the current frame
// contained in the "vi" struct.
  dst = env->NewVideoFrame(vi);
  // Request a Write pointer from the newly created destination image.
  dstp = dst->GetWritePtr();
  dst_pitch = dst->GetPitch();

  // Ready to make motion compensation

  if (isYUY2) // v1.6
  {
    // create planes from YUY2
    YUY2ToPlanes(srcp, src_height, src_width, src_pitch, srcplaneY, planeYpitch, srcplaneU, planeUVpitch, srcplaneV, planeUVpitch);

    // Process Luma Plane
    border = 0;  // luma=0, black

    // move src frame plane by vector to motion compensated position		
    if (subpixel == 2) { // bicubic interpolation
      compensate_plane_bicubic(dstplaneY, planeYpitch, srcplaneY, planeYpitch, planeYwidth, src_height, trsum, mirror, border, work2width4356, blur);
    }
    else if (subpixel == 1) { // bilinear interpolation
      compensate_plane_bilinear(dstplaneY, planeYpitch, srcplaneY, planeYpitch, planeYwidth, src_height, trsum, mirror, border, work2width4356, blur);
    }
    else {  //subpixel=0, nearest pixel accuracy
      compensate_plane_nearest(dstplaneY, planeYpitch, srcplaneY, planeYpitch, planeYwidth, src_height, trsum, mirror, border, work2width4356, blur);
    }

    borderUV = 128; // border color = grey if both U,V=128

    // scale transform for UV  (for half UV plane horizontal sizes)

    trsumUV.dxc = trsum.dxc / 2;
    trsumUV.dxx = trsum.dxx;
    trsumUV.dxy = trsum.dxy / 2;
    trsumUV.dyc = trsum.dyc;
    trsumUV.dyx = trsum.dyx * 2;
    trsumUV.dyy = trsum.dyy;

    // Process U plane
    if (subpixel == 2) { // bicubic interpolation
      compensate_plane_bicubic(dstplaneU, planeUVpitch, srcplaneU, planeUVpitch, planeUVwidth, src_height, trsumUV, mirror, borderUV, work2width4356, blur / 2);
    }
    else if (subpixel == 1) {  // bilinear interpolation
      compensate_plane_bilinear(dstplaneU, planeUVpitch, srcplaneU, planeUVpitch, planeUVwidth, src_height, trsumUV, mirror, borderUV, work2width4356, blur / 2); //  devide dx, dy by 2 for UV plane
    }
    else {  //subpixel=0, nearest pixel accuracy
      compensate_plane_nearest(dstplaneU, planeUVpitch, srcplaneU, planeUVpitch, planeUVwidth, src_height, trsumUV, mirror, borderUV, work2width4356, blur / 2); //  devide dx, dy by 2 for UV plane
    }

    // Process V plane 
    if (subpixel == 2) { // bicubic interpolation
      compensate_plane_bicubic(dstplaneV, planeUVpitch, srcplaneV, planeUVpitch, planeUVwidth, src_height, trsumUV, mirror, borderUV, work2width4356, blur / 2);
    }
    else if (subpixel == 1) {  // bilinear interpolation
      compensate_plane_bilinear(dstplaneV, planeUVpitch, srcplaneV, planeUVpitch, planeUVwidth, src_height, trsumUV, mirror, borderUV, work2width4356, blur / 2); //  devide dx, dy by 2 for UV plane
    }
    else {  //subpixel=0, nearest pixel accuracy
      compensate_plane_nearest(dstplaneV, planeUVpitch, srcplaneV, planeUVpitch, planeUVwidth, src_height, trsumUV, mirror, borderUV, work2width4356, blur / 2); //  devide dx, dy by 2 for UV plane
    }

    // create YUY2 from planes
    PlanesToYUY2(dstp, src_height, src_width, dst_pitch, dstplaneY, planeYpitch, dstplaneU, planeUVpitch, dstplaneV, planeUVpitch);

    int xmsg = (fieldbased != 0) ? (ndest % 2) * 15 : 0; // x-position of odd fields message
    if (info) { // show text info on frame image
      char messagebuf[32];
      sprintf_s(messagebuf, " DePan");
      DrawString(dst, xmsg, 1, messagebuf, isYUY2);
      sprintf_s(messagebuf, " offset= %5.2f", offset);
      DrawString(dst, xmsg, 2, messagebuf, isYUY2);
      sprintf_s(messagebuf, " %d to %d", ndest - intoffset, ndest);
      DrawString(dst, xmsg, 3, messagebuf, isYUY2);
      sprintf_s(messagebuf, " dx  = %7.2f", dxsum);
      DrawString(dst, xmsg, 4, messagebuf, isYUY2);
      sprintf_s(messagebuf, " dy  = %7.2f", dysum);
      DrawString(dst, xmsg, 5, messagebuf, isYUY2);
      sprintf_s(messagebuf, " zoom= %7.5f", zoomsum);
      DrawString(dst, xmsg, 6, messagebuf, isYUY2);
      sprintf_s(messagebuf, " rot= %7.3f", rotsum);
      DrawString(dst, xmsg, 7, messagebuf, isYUY2);

    }
  }
  else // YV12
  {
    // This code deals with YV12 colourspace where the Y, U and V information are
    // stored in completely separate memory areas

    // Process Luma Plane
    border = 0;  // luma=0, black

      // move src frame plane by vector to motion compensated position		
    if (subpixel == 2) { // bicubic interpolation
      compensate_plane_bicubic(dstp, dst_pitch, srcp, src_pitch, src_width, src_height, trsum, mirror, border, work2width4356, blur);
    }
    else if (subpixel == 1) {  // bilinear interpolation
      compensate_plane_bilinear(dstp, dst_pitch, srcp, src_pitch, src_width, src_height, trsum, mirror, border, work2width4356, blur);
    }
    else {  //subpixel=0, nearest pixel accuracy
      compensate_plane_nearest(dstp, dst_pitch, srcp, src_pitch, src_width, src_height, trsum, mirror, border, work2width4356, blur);
    }

    int xmsg = (fieldbased != 0) ? (ndest % 2) * 15 : 0; // x-position of odd fields message
    if (info) { // show text info on frame image
      char messagebuf[32];
      sprintf_s(messagebuf, " DePan");
      DrawString(dst, xmsg, 1, messagebuf, isYUY2);
      sprintf_s(messagebuf, " offset= %5.2f", offset);
      DrawString(dst, xmsg, 2, messagebuf, isYUY2);
      sprintf_s(messagebuf, " %d to %d", ndest - intoffset, ndest);
      DrawString(dst, xmsg, 3, messagebuf, isYUY2);
      sprintf_s(messagebuf, " dx  = %7.2f", dxsum);
      DrawString(dst, xmsg, 4, messagebuf, isYUY2);
      sprintf_s(messagebuf, " dy  = %7.2f", dysum);
      DrawString(dst, xmsg, 5, messagebuf, isYUY2);
      sprintf_s(messagebuf, " zoom= %7.5f", zoomsum);
      DrawString(dst, xmsg, 6, messagebuf, isYUY2);
      sprintf_s(messagebuf, " rot= %7.3f", rotsum);
      DrawString(dst, xmsg, 7, messagebuf, isYUY2);
    }


    // This section of code deals with the U and V planes of planar formats (e.g. YV12)
    // So first of all we have to get the additional info on the U and V planes
    dst_pitchUV = dst->GetPitch(PLANAR_U);	// The pitch,height and width information
    src_pitchUV = src->GetPitch(PLANAR_U);	// is guaranted to be the same for both
    src_widthUV = src->GetRowSize(PLANAR_U);	// the U and V planes so we only the U
    src_heightUV = src->GetHeight(PLANAR_U);	//plane values and use them for V as well

    borderUV = 128; // border color = grey if both U,V=128

    // scale transform for UV  (for half UV12 plane sizes)

    trsumUV.dxc = trsum.dxc / 2;
    trsumUV.dxx = trsum.dxx;
    trsumUV.dxy = trsum.dxy;
    trsumUV.dyc = trsum.dyc / 2;
    trsumUV.dyx = trsum.dyx;
    trsumUV.dyy = trsum.dyy;

    // Process U plane
    srcp = src->GetReadPtr(PLANAR_U);
    dstp = dst->GetWritePtr(PLANAR_U);
    if (subpixel == 2) { // bicubic interpolation
      compensate_plane_bicubic(dstp, dst_pitchUV, srcp, src_pitchUV, src_widthUV, src_heightUV, trsumUV, mirror, borderUV, work2width4356, blur / 2);
    }
    else if (subpixel == 1) {  // bilinear interpolation
      compensate_plane_bilinear(dstp, dst_pitchUV, srcp, src_pitchUV, src_widthUV, src_heightUV, trsumUV, mirror, borderUV, work2width4356, blur / 2); //  devide dx, dy by 2 for UV plane
    }
    else {  //subpixel=0, nearest pixel accuracy
      compensate_plane_nearest(dstp, dst_pitchUV, srcp, src_pitchUV, src_widthUV, src_heightUV, trsumUV, mirror, borderUV, work2width4356, blur / 2); //  devide dx, dy by 2 for UV plane
    }

    // Process V plane 
    srcp = src->GetReadPtr(PLANAR_V);
    dstp = dst->GetWritePtr(PLANAR_V);
    if (subpixel == 2) { // bicubic interpolation
      compensate_plane_bicubic(dstp, dst_pitchUV, srcp, src_pitchUV, src_widthUV, src_heightUV, trsumUV, mirror, borderUV, work2width4356, blur / 2);
    }
    else if (subpixel == 1) {  // bilinear interpolation
      compensate_plane_bilinear(dstp, dst_pitchUV, srcp, src_pitchUV, src_widthUV, src_heightUV, trsumUV, mirror, borderUV, work2width4356, blur / 2); //  devide dx, dy by 2 for UV plane
    }
    else {  //subpixel=0, nearest pixel accuracy
      compensate_plane_nearest(dstp, dst_pitchUV, srcp, src_pitchUV, src_widthUV, src_heightUV, trsumUV, mirror, borderUV, work2width4356, blur / 2); //  devide dx, dy by 2 for UV plane
    }
  }
  return dst;
#endif	
}

//****************************************************************************
// This is the function that created the filter, when the filter has been called.
// This can be used for simple parameter checking, so it is possible to create different filters,
// based on the arguments recieved.

AVSValue __cdecl Create_DePan(AVSValue args, void* user_data, IScriptEnvironment* env) {
  return new DePan(args[0].AsClip(), // the 0th parameter is the source clip
    args[1].AsClip(),		// parameter  - motion data clip.
    (float)args[2].AsFloat(0),	// parameter  - offset
    args[3].AsInt(2),		// parameter  - subpixel 
    (float)args[4].AsFloat(1.),	// parameter  - pixaspect
    args[5].AsBool(true),	// parameter  - matchfields
    args[6].AsInt(0),		// parameter  - mirror
    args[7].AsInt(0),		// parameter  - blur mirror length
    args[8].AsBool(false),	// parameter  - info
    args[9].AsString(""),  // inputlog
    env);
  // Calls the constructor with the arguments provided.
}
//****************************************************************************











//****************************************************************************
//****************************************************************************
//
//                    DePanInterleave function
//
// Uses DePan and Interleave(internal) function
// to make long interleaved clip with motion compensated frames
//
//****************************************************************************
//****************************************************************************
//****************************************************************************
class DePanInterleave : public GenericVideoFilter {
  // DePanInterleave defines the name of your filter class. 
  // This name is only used internally, and does not affect the name of your filter or similar.
  // This filter extends GenericVideoFilter, which incorporates basic functionality.
  // All functions present in the filter must also be present here.

  PClip DePanData;      // motion data clip
  float offset; // offset of first clip in series
  int prev; // number of prev compesated frames
  int next; // number of next compensated frames
  int subpixel;     // subpixel accuracy by interpolation
  float pixaspect; // pixel aspect
  int matchfields;  // match vertical position of fields for better denoising etc
  int mirror; //		use mirror to fill border
  int blur; // blur mirror length
  int info;   // show motion info on frame
  const char * inputlog;

  PClip interleaved; // interleaved clip
  AVSValue * allclips; //array of all motion compensated clips
  int num;  // number of motion compensated clips


public:
  // This defines that these functions are present in your class.
  // These functions must be that same as those actually implemented.
  // Since the functions are "public" they are accessible to other classes.
  // Otherwise they can only be called from functions within the class itself.

  DePanInterleave(PClip _child, PClip _DePanData, int _prev, int _next, int _subpixel, float _pixaspect, int _matchfields,
    int _mirror, int _blur, int _info, const char * _inputlog, IScriptEnvironment* env);
  // This is the constructor. It does not return any value, and is always used, 
  //  when an instance of the class is created.
  // Since there is no code in this, this is the definition.

  ~DePanInterleave();
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
DePanInterleave::DePanInterleave(PClip _child, PClip _DePanData, int _prev, int _next, int _subpixel, float _pixaspect, int _matchfields,
  int _mirror, int _blur, int _info, const char * _inputlog, IScriptEnvironment* env) :
  GenericVideoFilter(_child), DePanData(_DePanData), prev(_prev), next(_next), subpixel(_subpixel), pixaspect(_pixaspect), matchfields(_matchfields),
  mirror(_mirror), blur(_blur), info(_info), inputlog(_inputlog) {
  // This is the implementation of the constructor.
  // The child clip (source clip) is inherited by the GenericVideoFilter,
  //  where the following variables gets defined:
  //   PClip child;   // Contains the source clip.
  //   VideoInfo vi;  // Contains videoinfo on the source clip.

  float offset; // offset of current clip
  int i;

  int num = prev + next + 1;

  if (!vi.IsYUY2() && !vi.IsYUV() && !vi.IsYUVA())
    env->ThrowError("DePanInterleave: input must be planar YUV or YUY2 !"); //v1.6

  if ((DePanData->GetVideoInfo().num_frames) != vi.num_frames)
    env->ThrowError("DePanInterleave: The length of input clip must be same as motion data clip !");
  if (prev < 0 || next < 0 || num <= 0)
    env->ThrowError("DePanInterleave: The number of clips must be positive !");



#ifdef AVISYNTH_2_5
  child->SetCacheHints(CACHE_25_RANGE, num); // enabled in v1.6 CACHE_RANGE -> CACHE_25_RANGE
#endif

  // make clips array
  allclips = new AVSValue[num];

  for (i = 0; i < prev; i++) { // cycle up to range 
    // integer offset values for compensated clips
    offset = float(prev - i);
    // create forwarded motion compensated clip from prev
    allclips[i] = new DePan(child, DePanData, offset, subpixel, pixaspect, matchfields, mirror, blur, info, inputlog, env);
  }

  allclips[prev] = child;  // central clip is input source

  for (i = 0; i < next; i++) { // cycle up to range 
    // integer offset values for compensated clips
    offset = float(-i - 1);
    // create backwarded clip (from motion conpensated next frames)
    allclips[i + prev + 1] = new DePan(child, DePanData, offset, subpixel, pixaspect, matchfields, mirror, blur, info, inputlog, env);
  }


  // make interleaved clip by using INVOKE with INTERLEAVE
  interleaved = env->Invoke("Interleave", AVSValue(allclips, num)).AsClip();

  // set internal cache for invoked
  AVSValue args[1] = { interleaved };
  interleaved = env->Invoke("InternalCache", AVSValue(args, 1)).AsClip();
  // set cache range
#ifdef AVISYNTH_2_5
  child->SetCacheHints(CACHE_25_RANGE, num); // enabled in v1.6 CACHE_RANGE -> CACHE_25_RANGE
#endif

  // increase number of frames in structure of interleaved clip  (which will be result)
  vi.num_frames = vi.num_frames * (num);
  // increase fps
  vi.fps_numerator = vi.fps_numerator * (num);

}

//****************************************************************************
// This is where any actual destructor code used goes
DePanInterleave::~DePanInterleave() {
  // This is where you can deallocate any memory you might have used.
  delete[] allclips;

}

//
// ****************************************************************************
//
PVideoFrame __stdcall DePanInterleave::GetFrame(int ndest, IScriptEnvironment* env) {
  // This is the implementation of the GetFrame function.
  // See the header definition for further info.
  PVideoFrame src;
  // simple return frame from interleaved clip, as it is ready!
  src = interleaved->GetFrame(ndest, env);
  return src;
}

//****************************************************************************
// This is the function that created the filter, when the filter has been called.
// This can be used for simple parameter checking, so it is possible to create different filters,
// based on the arguments recieved.

AVSValue __cdecl Create_DePanInterleave(AVSValue args, void* user_data, IScriptEnvironment* env) {
  return new DePanInterleave(args[0].AsClip(), // the 0th parameter is the source clip
    args[1].AsClip(),	// parameter  - motion data clip.
    args[2].AsInt(1),	// parameter  - prev
    args[3].AsInt(1),	// parameter  - next
    args[4].AsInt(1),	// parameter  - subpixel - changed default to 1 in v1.6
    (float)args[5].AsFloat(1.0f),	// parameter  - pixaspect
    args[6].AsBool(true),	// parameter  - matchfields
    args[7].AsInt(0),	// parameter  - mirror
    args[8].AsInt(0),	// parameter  - blur mirror length
    args[9].AsBool(false),	// parameter  - info
    args[10].AsString(""),  // inputlog filename
    env);
  // Calls the constructor with the arguments provided.
}

