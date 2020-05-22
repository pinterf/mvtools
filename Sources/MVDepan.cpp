// Get the motion vectors and create Global motion data for DePan plugin
// Copyright (c) A.G.Balaknhin aka Fizick. bag@hotmail.ru

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

#include "info.h"
#include "MVDepan.h"

#include	<algorithm>

#include <cmath>



#define MOTIONUNKNOWN 9999



MVDepan::MVDepan(PClip _child, PClip mvs, PClip _mask, bool _zoom, bool _rot, float _pixaspect,
  float _error, bool _info, const char * _logfilename, float _wrong, float _zerow, int _range, sad_t nSCD1, int nSCD2, bool isse, bool _planar, IScriptEnvironment* env) :
  GenericVideoFilter(_child),
  mvclip(mvs, nSCD1, nSCD2, env, 1, 0),
  MVFilter(mvs, "MDepan", env, 1, 0),
  mask(_mask),
  planar(_planar),
  wrongDif(_wrong),
  zeroWeight(_zerow), range(_range),
  ifZoom(_zoom), ifRot(_rot), pixaspect(_pixaspect), error(_error), info(_info), logfilename(_logfilename)
{
  has_at_least_v8 = true;
  try { env->CheckVersion(8); }
  catch (const AvisynthError&) { has_at_least_v8 = false; }

  blockDx = new float[nBlkX * nBlkY]; // dx vector
  blockDy = new float[nBlkX * nBlkY]; // dy
  blockSAD = new sad_t[nBlkX * nBlkY];
  blockX = new int[nBlkX * nBlkY]; // block x position
  blockY = new int[nBlkX * nBlkY];
  blockWeight = new float[nBlkX * nBlkY];
  blockWeightMask = new float[nBlkX * nBlkY];

  if (lstrlen(logfilename) > 0) { // v.1.2.3
    logfile = fopen(logfilename, "wt");
    if (logfile == NULL)	env->ThrowError("MDePan: Log file can not be created!");
  }
  else
    logfile = NULL;

  if (mvclip.nDeltaFrame != 1)
    env->ThrowError("MDePan: motion vectors delta must be =1!");

  motionx = new float[vi.num_frames + 1];
  motiony = new float[vi.num_frames + 1];
  motionzoom = new float[vi.num_frames + 1];
  motionrot = new float[vi.num_frames + 1];

  for (int i = 0; i <= vi.num_frames; i++)
    motionx[i] = MOTIONUNKNOWN;
}


MVDepan::~MVDepan()
{
  delete[] blockDx;
  delete[] blockDy;
  delete[] blockSAD;
  delete[] blockX;
  delete[] blockY;
  delete[] blockWeight;
  delete[] blockWeightMask;
  if (logfile != NULL)
    fclose(logfile);

  delete[] motionx;
  delete[] motiony;
  delete[] motionzoom;
  delete[] motionrot;
}

//-----------------------------------------------------------------------------
// interface for DEPAN plugin

#define DEPANSIGNATURE "depan06"

typedef struct depanheaderstruct {  // structure of header depandata in framebuffer
  char signature[8];  // signature for check
  int reserved;  // for future using
  int nframes;  // number of records with frames motion data in current framebuffer
} depanheader;

typedef struct depandatastruct {  // structure of every frame data record in framebuffer
  int frame;  // frame number
  float dx;  // x shift (in pixels) for this frame
  float dy;  // y shift (in pixels, corresponded to pixel aspect = 1)
  float zoom; // zoom
  float rot;  // rotation (in degrees), =0 (no rotation estimated data in current version)
} depandata;


// write coded motion data to start of special clip frame buffer
//
// Simplified version with motion data for one frame only
void MVDepan::write_depan_data1(unsigned char *dstp, int frame, float motionx, float motiony, float motionzoom, float motionrot)
{
  char signaturegood[8] = DEPANSIGNATURE;
  depanheader header;
  depandata framedata;
//	int frame;

  for (int i = 0; i < sizeof(signaturegood); i++) {
    header.signature[i] = signaturegood[i];
  }

  header.nframes = 1;// framelast - framefirst +1; // number of frames to write motion info about

  int sizeheader = sizeof(header);
  int sizedata = sizeof(framedata);

  // write date to first line of frame
  memcpy(dstp, &header, sizeheader);
  dstp += sizeheader;

//	for (frame=framefirst; frame < framelast+1; frame++) {
  framedata.frame = frame; // some frame number
  framedata.dx = motionx;//[frame]; // motion x for frame
  framedata.dy = motiony;//[frame]; // motion y for frame
  framedata.zoom = motionzoom;//[frame]; // zoom for frame
  framedata.rot = motionrot;//0; // rotation for frame
  memcpy(dstp, &framedata, sizedata);
  dstp += sizedata;
//	}

}
// write coded motion data to start of special clip frame buffer
//
void MVDepan::write_depan_data(unsigned char *dstp, int framefirst, int framelast, float motionx[], float motiony[], float motionzoom[], float motionrot[])
{
  char signaturegood[8] = DEPANSIGNATURE;
  depanheader header;
  depandata framedata;
//	int frame;

  for (int i = 0; i < sizeof(signaturegood); i++) {
    header.signature[i] = signaturegood[i];
  }

  header.nframes = framelast - framefirst + 1; // number of frames to write motion info about

  int sizeheader = sizeof(header);
  int sizedata = sizeof(framedata);

  // write date to first line of frame
  memcpy(dstp, &header, sizeheader);
  dstp += sizeheader;

  for (int frame = framefirst; frame < framelast + 1; frame++) {
    framedata.frame = frame; // some frame number
    framedata.dx = motionx[frame]; // motion x for frame
    framedata.dy = motiony[frame]; // motion y for frame
    framedata.zoom = motionzoom[frame]; // zoom for frame
    framedata.rot = motionrot[frame]; // rotation for frame
    memcpy(dstp, &framedata, sizedata);
    dstp += sizedata;
  }

}
//
//*************************************************************************
// write motion data (line) for current frame to log file in Deshaker format
// file must be open
//
void MVDepan::write_deshakerlog1(FILE *logfile, int IsFieldBased, int IsTFF, int ndest, float motionx, float motiony, float motionzoom, float rotation)
{

  //	float rotation = 0.0; // no rotation estimation in current version


      // write frame number, dx, dy, rotation and zoom in Deshaker log format
  if (IsFieldBased) { // fields from interlaced clip, A or B ( A is time first in Deshaker log )
    if ((ndest % 2 == 0)) { // even TFF or BFF fields
      fprintf(logfile, " %5dA %7.2f %7.2f %7.3f %7.5f\n", ndest / 2, motionx, motiony, rotation, motionzoom);
    }
    else { // odd TFF or BFF fields
      fprintf(logfile, " %5dB %7.2f %7.2f %7.3f %7.5f\n", ndest / 2, motionx, motiony, rotation, motionzoom);
    }
  }
  else { // progressive
    fprintf(logfile, " %6d %7.2f %7.2f %7.3f %7.5f\n", ndest, motionx, motiony, rotation, motionzoom);
  }
}

//----------------------------------------------------------------
// Global motion trasform definition from DEPAN plugin
//  motion to transform
//  get  coefficients for coordinates transformation,
//  which defines source (xsrc, ysrc)  for current destination (x,y)
//
//  fractoffset is fracture of deformation:
//  from 0 to 1 for forward (from prev frame to cur),
//  from -1 to 0 for backward time direction (from next frame to cur)
//
//
//  return:
//  vector:
//   t[0] = dxc, t[1] = dxx, t[2] = dxy, t[3] = dyc, t[4] = dyx, t[5] = dyy
//
//
//   xsrc = dxc + dxx*x + dxy*y
//   ysrc = dyc + dyx*x + dyy*y
// But really only 4  parameters (dxc, dxx, dxy, dyc) are independent in used model
//
//  if no rotation, then dxy, dyx = 0,
//  if no rotation and zoom, then also dxx, dyy = 1.
//

//--------------------------------------------------------------------------
void MVDepan::motion2transform(float dx1, float dy1, float rot, float zoom1, float pixaspect, float xcenter, float ycenter, int forward, float fractoffset, transform *tr)
{
  const float PI = 3.1415926535897932384626433832795f;
  float rotradian, sinus, cosinus, dx, dy, zoom;

  // fractoffset > 0 for forward, <0 for backward
  dx = fractoffset*dx1;
  dy = fractoffset*dy1;
  rotradian = fractoffset*rot*PI / 180;   // from degree to radian
  if (fabs(rotradian) < 1e-6) rotradian = 0;  // for some stability of rounding precision
  zoom = exp(fractoffset*log(zoom1));         // zoom**(fractoffset) = exp(fractoffset*ln(zoom))
  if (fabs(zoom - 1.) < 1e-6) zoom = 1.;			// for some stability of rounding precision

  sinus = sin(rotradian);
  cosinus = cos(rotradian);

  //	xcenter = row_size_p*(1+uv)/2.0;      //  middle x
  //    ycenter = height_p*(1+uv)/2.0;       //  middle y

  if (forward != 0) {         //  get coefficients for forward
    tr->dxc = xcenter + (-xcenter*cosinus + ycenter / pixaspect*sinus)*zoom + dx;// dxc            /(1+uv);
    tr->dxx = cosinus*zoom; // dxx
    tr->dxy = -sinus / pixaspect*zoom;  // dxy

    tr->dyc = ycenter + (((-ycenter) / pixaspect*cosinus + (-xcenter)*sinus)*zoom + dy)*pixaspect;// dyc      /(1+uv);
    tr->dyx = sinus*zoom*pixaspect; // dyx
    tr->dyy = cosinus*zoom;  // dyy
  }
  else {					// coefficients for backward
    tr->dxc = xcenter + ((-xcenter + dx)*cosinus - ((-ycenter) / pixaspect + dy)*sinus)*zoom;//     /(1+uv);
    tr->dxx = cosinus*zoom;
    tr->dxy = -sinus / pixaspect*zoom;

    tr->dyc = ycenter + (((-ycenter) / pixaspect + dy)*cosinus + (-xcenter + dx)*sinus)*zoom*pixaspect; //      /(1+uv);
    tr->dyx = sinus*zoom*pixaspect;
    tr->dyy = cosinus*zoom;
  }
}

//-----------------------------------------------------------------------------
//void transform2motion (float tr[], int forward, float xcenter, float ycenter, float pixaspect, float *dx, float *dy, float *rot, float *zoom)
void MVDepan::transform2motion(transform tr, int forward, float xcenter, float ycenter, float pixaspect, float *dx, float *dy, float *rot, float *zoom)
{
  const float PI = 3.1415926535897932384626433832795f;
  float rotradian, sinus, cosinus;

  if (forward != 0) {         //  get motion for forward
    rotradian = -atan(pixaspect*tr.dxy / tr.dxx);
    *rot = rotradian * 180 / PI;
    sinus = sin(rotradian);
    cosinus = cos(rotradian);
    *zoom = tr.dxx / cosinus;
    *dx = tr.dxc - xcenter - (-xcenter*cosinus + ycenter / pixaspect*sinus)*(*zoom);
    *dy = tr.dyc / pixaspect - ycenter / pixaspect - ((-ycenter) / pixaspect*cosinus + (-xcenter)*sinus)*(*zoom);// dyc

  }
  else {					// coefficients for backward
    rotradian = -atan(pixaspect*tr.dxy / tr.dxx);
    *rot = rotradian * 180 / PI;
    sinus = sin(rotradian);
    cosinus = cos(rotradian);
    *zoom = tr.dxx / cosinus;


    //		tr.dxc/(*zoom) = xcenter/(*zoom) + (-xcenter + dx)*cosinus - ((-ycenter)/pixaspect + dy)*sinus ;
    //		tr.dyc/(*zoom)/pixaspect = ycenter/(*zoom)/pixaspect +  ((-ycenter)/pixaspect +dy)*cosinus + (-xcenter + dx)*sinus ;
    // *cosinus:
    //		tr.dxc/(*zoom)*cosinus = xcenter/(*zoom)*cosinus + (-xcenter + dx)*cosinus*cosinus - ((-ycenter)/pixaspect + dy)*sinus*cosinus ;
    // *sinus:
    //		tr.dyc/(*zoom)/pixaspect*sinus = ycenter/(*zoom)/pixaspect*sinus +  ((-ycenter)/pixaspect +dy)*cosinus*sinus + (-xcenter + dx)*sinus*sinus ;
    // summa:
    //		tr.dxc/(*zoom)*cosinus + tr.dyc/(*zoom)/pixaspect*sinus = xcenter/(*zoom)*cosinus + (-xcenter + dx) + ycenter/(*zoom)/pixaspect*sinus   ;
    *dx = tr.dxc / (*zoom)*cosinus + tr.dyc / (*zoom) / pixaspect*sinus - xcenter / (*zoom)*cosinus + xcenter - ycenter / (*zoom) / pixaspect*sinus;

    // *sinus:
    //		tr.dxc/(*zoom)*sinus = xcenter/(*zoom)*sinus + (-xcenter + dx)*cosinus*sinus - ((-ycenter)/pixaspect + dy)*sinus*sinus ;
    // *cosinus:
    //		tr.dyc/(*zoom)/pixaspect*cosinus = ycenter/(*zoom)/pixaspect*cosinus +  ((-ycenter)/pixaspect +dy)*cosinus*cosinus + (-xcenter + dx)*sinus*cosinus ;
    // diff:
    //		tr.dxc/(*zoom)*sinus - tr.dyc/(*zoom)/pixaspect*cosinus = xcenter/(*zoom)*sinus - (-ycenter/pixaspect + dy) - ycenter/(*zoom)/pixaspect*cosinus   ;
    *dy = -tr.dxc / (*zoom)*sinus + tr.dyc / (*zoom) / pixaspect*cosinus + xcenter / (*zoom)*sinus - (-ycenter / pixaspect) - ycenter / (*zoom) / pixaspect*cosinus;


  }
}

//------------------------------------------------------------
//  get  coefficients for inverse coordinates transformation,
//  fransform_inv ( transform_A ) = null transform
void MVDepan::inversetransform(transform ta, transform *tinv)
{
  float pixaspect;

  if (ta.dxy != 0) pixaspect = sqrt(-ta.dyx / ta.dxy);
  else pixaspect = 1.0;

  tinv->dxx = ta.dxx / ((ta.dxx)*ta.dxx + ta.dxy*ta.dxy*pixaspect*pixaspect);
  tinv->dyy = tinv->dxx;
  tinv->dxy = -tinv->dxx * ta.dxy / ta.dxx;
  tinv->dyx = -tinv->dxy * pixaspect*pixaspect;
  tinv->dxc = -tinv->dxx * ta.dxc - tinv->dxy * ta.dyc;
  tinv->dyc = -tinv->dyx * ta.dxc - tinv->dyy * ta.dyc;
}

//----------------------------------------------------------------------------
void MVDepan::TrasformUpdate(transform *tr, float blockDx[], float blockDy[], sad_t blockSAD[], int blockX[], int blockY[], float blockWeight[], int nBlkX, int nBlkY, float safety, bool ifZoom1, bool ifRot1, float *error1, float pixaspect)
{
  transform trderiv;
  int n = nBlkX*nBlkY;
  trderiv.dxc = 0;
  trderiv.dxx = 0;
  trderiv.dxy = 0;
  trderiv.dyc = 0;
  trderiv.dyx = 0;
  trderiv.dyy = 0;
  float norm = 0.1f;
  float x2 = 0.1f;
  float y2 = 0.1f;
  float error2 = 0.1f;
  for (int i = 0; i < n; i++)
  {
    float bw = blockWeight[i];
    float xdif = (tr->dxc + tr->dxx*blockX[i] + tr->dxy*blockY[i] - blockX[i] - blockDx[i]);
    trderiv.dxc += 2 * xdif*bw;
    if (ifZoom1) trderiv.dxx += 2 * blockX[i] * xdif*bw;
    if (ifRot1)	trderiv.dxy += 2 * blockY[i] * xdif*bw;
    float ydif = (tr->dyc + tr->dyx*blockX[i] + tr->dyy*blockY[i] - blockY[i] - blockDy[i]);
    trderiv.dyc += 2 * ydif*bw;
    if (ifRot1) trderiv.dyx += 2 * blockX[i] * ydif*bw;
    if (ifZoom1) trderiv.dyy += 2 * blockY[i] * ydif*bw;
    norm += bw;
    x2 += blockX[i] * blockX[i] * bw;
    y2 += blockY[i] * blockY[i] * bw;
    error2 += (xdif*xdif + ydif*ydif)*bw;
  }
  trderiv.dxc /= norm * 2;
  trderiv.dxx /= x2 * 2 * 1.5f; // with additional safety factors
  trderiv.dxy /= y2 * 2 * 3;
  trderiv.dyc /= norm * 2;
  trderiv.dyx /= x2 * 2 * 3;
  trderiv.dyy /= y2 * 2 * 1.5f;

  error2 /= norm;
  *error1 = sqrtf(error2);

  tr->dxc -= safety*trderiv.dxc;
  if (ifZoom1) 	tr->dxx -= safety*0.5f*(trderiv.dxx + trderiv.dyy);

  tr->dxy -= safety*0.5f*(trderiv.dxy - trderiv.dyx / (pixaspect*pixaspect));
  tr->dyc -= safety*trderiv.dyc;
//	tr->dyx -= safety*trderiv.dyx;
//	tr->dyy -= safety*trderiv.dyy;
  if (ifZoom1) 	tr->dyy = tr->dxx;
//	float pixaspect=1; // was for test and forgot remove?! disabled in v1.2.5
  tr->dyx = -pixaspect*pixaspect * tr->dxy;

}
//----------------------------------------------------------------------------

void MVDepan::RejectBadBlocks(transform tr, float blockDx[], float blockDy[], sad_t blockSAD[], int blockX[], int blockY[], float blockWeight[], int nBlkX, int nBlkY, float wrongDif, float globalDif, int thSCD1, float zeroWeight, float blockWeightMask[], int ignoredBorder)
{
  for (int j = 0; j < nBlkY; j++)
  {
    for (int i = 0; i < nBlkX; i++)
    {
      int n = j*nBlkX + i;
      if (i < ignoredBorder || i >= nBlkX - ignoredBorder || j < ignoredBorder || j >= nBlkY - ignoredBorder)
      {
        blockWeight[n] = 0; // disable  blocks near frame borders
      }
      else if (blockSAD[n] > thSCD1)
      {
        blockWeight[n] = 0; // disable bad block with big SAD
      }
      else if (i > 0 && i < (nBlkX - 1) && (fabs((blockDx[n - 1 - nBlkX] + blockDx[n - nBlkX] + blockDx[n + 1 - nBlkX] +
        blockDx[n - 1] + blockDx[n + 1] +
        blockDx[n - 1 + nBlkX] + blockDx[n + nBlkX] + blockDx[n + 1 + nBlkX]) / 8 - blockDx[n]) > wrongDif))
      {
        blockWeight[n] = 0; // disable blocks very different from neighbours
      }
      else if (j > 0 && j < (nBlkY - 1) && (fabs((blockDy[n - 1 - nBlkX] + blockDy[n - nBlkX] + blockDy[n + 1 - nBlkX] +
        blockDy[n - 1] + blockDy[n + 1] +
        blockDy[n - 1 + nBlkX] + blockDy[n + nBlkX] + blockDy[n + 1 + nBlkX]) / 8 - blockDy[n]) > wrongDif))
      {
        blockWeight[n] = 0; // disable blocks very different from neighbours
      }
      else if (fabs(tr.dxc + tr.dxx*blockX[n] + tr.dxy*blockY[n] - blockX[n] - blockDx[n]) > globalDif)
      {
        blockWeight[n] = 0; // disable blocks very different from global
      }
      else if (fabs(tr.dyc + tr.dyx*blockX[n] + tr.dyy*blockY[n] - blockY[n] - blockDy[n]) > globalDif)
      {
        blockWeight[n] = 0; // disable blocks very different from global
      }
      else if (blockDx[n] == 0 && blockDy[n] == 0)
      {
        blockWeight[n] = zeroWeight*blockWeightMask[n];//0.05f; // decrease weight of blocks with strictly zero motion - added in v1.2.5
      }
      else
      {
        blockWeight[n] = blockWeightMask[n]; // good block
      }
    }
  }

}

//------------------------------------------------------------------------------------

PVideoFrame __stdcall MVDepan::GetFrame(int ndest, IScriptEnvironment* env)
{
//	isBackward = false;
//	bool ifZoom = true;
//	bool ifRot = true;
//	float pixaspect = 1;
//	float error = 15;
//	bool info = true;

  int nFields = (vi.IsFieldBased()) ? 2 : 1;

  PVideoFrame	src = child->GetFrame(ndest, env);
  PVideoFrame	dst = has_at_least_v8 ? env->NewVideoFrameP(vi, &src) : env->NewVideoFrame(vi); // frame property support

  PVideoFrame maskf;
  if (mask)
    maskf = mask->GetFrame(ndest, env);

  const BYTE * srcp;
  BYTE * dstp;
  int src_width;
  int src_height;
  int src_pitch;
  int dst_pitch;
  const BYTE * maskp;
  int mask_width;
  int mask_height;
  int mask_pitch;

  if ((vi.IsYUV() || vi.IsYUVA()) && !vi.IsYUY2())
  {
  // copy U
    srcp = src->GetReadPtr(PLANAR_U);
    src_width = src->GetRowSize(PLANAR_U);
    src_height = src->GetHeight(PLANAR_U);
    src_pitch = src->GetPitch(PLANAR_U);
    dstp = dst->GetWritePtr(PLANAR_U);
    dst_pitch = dst->GetPitch(PLANAR_U);
    env->BitBlt(dstp, dst_pitch, srcp, src_pitch, src_width, src_height);

    // copy V
    srcp = src->GetReadPtr(PLANAR_V);
    src_width = src->GetRowSize(PLANAR_V);
    src_height = src->GetHeight(PLANAR_V);
    src_pitch = src->GetPitch(PLANAR_V);
    dstp = dst->GetWritePtr(PLANAR_V);
    dst_pitch = dst->GetPitch(PLANAR_V);
    env->BitBlt(dstp, dst_pitch, srcp, src_pitch, src_width, src_height);

    // copy Y and prepare for output data
    srcp = src->GetReadPtr(PLANAR_Y);
    src_width = src->GetRowSize(PLANAR_Y);
    src_height = src->GetHeight(PLANAR_Y);
    src_pitch = src->GetPitch(PLANAR_Y);
    dstp = dst->GetWritePtr(PLANAR_Y);
    dst_pitch = dst->GetPitch(PLANAR_Y);
    env->BitBlt(dstp, dst_pitch, srcp, src_pitch, src_width, src_height);
    if (mask)
    {
      maskp = maskf->GetReadPtr(PLANAR_Y);
      mask_width = maskf->GetRowSize(PLANAR_Y);
      mask_height = maskf->GetHeight(PLANAR_Y);
      mask_pitch = maskf->GetPitch(PLANAR_Y);
    }
  }
  else
  { // YUY2
    srcp = src->GetReadPtr();
    src_width = src->GetRowSize();
    src_height = src->GetHeight();
    src_pitch = src->GetPitch();
    dstp = dst->GetWritePtr();
    dst_pitch = dst->GetPitch();
    env->BitBlt(dstp, dst_pitch, srcp, src_pitch, src_width, src_height);
    if (mask)
    {
      maskp = maskf->GetReadPtr();
      mask_width = maskf->GetRowSize();
      mask_height = maskf->GetHeight();
      mask_pitch = maskf->GetPitch();
    }
  }

  float dPel = 1.0f / nPel;  // subpixel precision value

  // declare motion transform structure
  transform tr;

  int backward;
  if (mvclip.IsBackward())
    backward = 1; // for backward transform
  else
    backward = 0;

  int framefirst = std::max(ndest - range, 0);
  int framelast = std::min(ndest + range, vi.num_frames - 1);

  float safety;
  float errorcur;
  int iter;
  float errordif = 0.01f; // error difference to terminate iterations
  //int itermax = 150; // maximum iteration number

  for (int nframe = framefirst; nframe <= framelast; nframe++)
  {

    // init motion transform as null
    tr.dxc = 0;
    tr.dxx = 1;
    tr.dxy = 0;
    tr.dyc = 0;
    tr.dyx = 0;
    tr.dyy = 1;

    errorcur = error * 2; // v1.2.3
    iter = 0; // start iteration

    if (motionx[nframe] == MOTIONUNKNOWN)
    {
      int nframemv = (backward) ? nframe - 1 : nframe; // set prev frame number as data frame if backward
      PVideoFrame mvn = mvclip.GetFrame(nframemv, env);
      mvclip.Update(mvn, env);

      int BPP; // step bytes luma per pixel
      if (!planar && vi.IsYUY2())
        BPP = 2;
      else
        BPP = 1;

      if (nframemv >= 0 && mvclip.IsUsable())
      {
    //		float testDx = 0;
    //		float testDy = 0;

        for (int j = 0; j < nBlkY; j++)
        {
          for (int i = 0; i < nBlkX; i++)
          {
            int nb = j*nBlkX + i;
            blockDx[nb] = mvclip.GetBlock(0, nb).GetMV().x * dPel;
            blockDy[nb] = mvclip.GetBlock(0, nb).GetMV().y * dPel;
            blockSAD[nb] = mvclip.GetBlock(0, nb).GetSAD();
            blockX[nb] = mvclip.GetBlock(0, nb).GetX() + nBlkSizeX / 2;//i*nBlkSize + nBlkSize/2;// rewritten in v1.2.5
            blockY[nb] = mvclip.GetBlock(0, nb).GetY() + nBlkSizeY / 2;//j*nBlkSize + nBlkSize/2;//
            if (mask && blockX[nb] < vi.width && blockY[nb] < vi.height)
              blockWeightMask[nb] = maskp[blockX[nb] * BPP + blockY[nb] * mask_pitch];
            else
              blockWeightMask[nb] = 1;
            blockWeight[nb] = blockWeightMask[nb];

    //				testDx += blockDx[nb];
    //				testDy += blockDy[nb];
          }
        }
    //		testDx /= nBlkX*nBlkY;
    //		testDy /= nBlkX*nBlkY;

    //		char debugbuf[200];
    //		sprintf(debugbuf,"MVDEPAN: %d %f %f %d %d", n, testDx, testDy, blockX[nBlkX*nBlkY-1], blockY[nBlkX*nBlkY-1]);
    //		OutputDebugString(debugbuf);

        // begin with translation only
        safety = 0.3f; // begin with small safety factor
        bool ifRot0 = false;
        bool ifZoom0 = false;
        float globalDif0 = 1000.0f;
        int ignoredBorder;
        if (mask)
          ignoredBorder = 0;
        else
          ignoredBorder = 4; // old pre v.2.4.3 method


        for (; iter < 5; iter++)
        {
          TrasformUpdate(&tr, blockDx, blockDy, blockSAD, blockX, blockY, blockWeight, nBlkX, nBlkY, safety, ifZoom0, ifRot0, &errorcur, pixaspect / nFields);
          RejectBadBlocks(tr, blockDx, blockDy, blockSAD, blockX, blockY, blockWeight, nBlkX, nBlkY, wrongDif, globalDif0, mvclip.GetThSCD1(), zeroWeight, blockWeightMask, ignoredBorder);
        }


        for (; iter < 100; iter++)
        {
          if (iter < 8)
            safety = 0.3f; // use for safety
          else if (iter < 10)
            safety = 0.6f;
          else
            safety = 1.0f;
          float errorprev = errorcur;
          TrasformUpdate(&tr, blockDx, blockDy, blockSAD, blockX, blockY, blockWeight, nBlkX, nBlkY, safety, ifZoom, ifRot, &errorcur, pixaspect / nFields);
          if (((errorprev - errorcur) < errordif*0.5 && iter > 9) || errorcur < errordif) break; // check convergence, accuracy increased in v1.2.5
          float globalDif = errorcur * 2;
          RejectBadBlocks(tr, blockDx, blockDy, blockSAD, blockX, blockY, blockWeight, nBlkX, nBlkY, wrongDif, globalDif, mvclip.GetThSCD1(), zeroWeight, blockWeightMask, ignoredBorder);
        }

      }

      // we get transform (null if scenechange)

      float xcenter = (float)vi.width / 2;
      float ycenter = (float)vi.height / 2;

      motionx[nframe] = 0;
      motiony[nframe] = 0;
      motionrot[nframe] = 0;
      motionzoom[nframe] = 1;

      if (errorcur < error) // if not bad result
      {
        // convert transform data to ordinary motion format
        if (mvclip.IsBackward())
        {
          transform trinv;
          inversetransform(tr, &trinv);
          transform2motion(trinv, 0, xcenter, ycenter, pixaspect / nFields, &motionx[nframe], &motiony[nframe], &motionrot[nframe], &motionzoom[nframe]);
        }
        else
          transform2motion(tr, 1, xcenter, ycenter, pixaspect / nFields, &motionx[nframe], &motiony[nframe], &motionrot[nframe], &motionzoom[nframe]);

        // fieldbased correction - added in v1.2.3
        int isnframeodd = nframe % 2;  // =0 for even,    =1 for odd
        float yadd = 0;
        if (vi.IsFieldBased()) { // correct line shift for fields, if not scenechange
          // correct unneeded fields matching
          {
            if (vi.IsTFF())
              yadd += 0.5f - isnframeodd; // TFF
            else
              yadd += -0.5f + isnframeodd; // BFF (or undefined?)
          }
          // scale dy for fieldbased frame by factor 2 (for compatibility)
          yadd = yadd * 2;
          motiony[nframe] += yadd;
        }

        if (fabs(motionx[nframe]) < 0.01f)  // if it is accidentally very small, reset it to small, but non-zero value ,
          motionx[nframe] = (2 * rand() - RAND_MAX) > 0 ? 0.011f : -0.011f; // to differ from pure 0, which be interpreted as bad value mark (scene change)

      }
    //		char debugbuf[200];
    //		sprintf(debugbuf,"MVDEPAN: %d %d %d", ndest, nframe, nframemv);
    //		OutputDebugString(debugbuf);
    }
    if (info && nframe == ndest) // type text info to output frame
    {
      int xmsg = 0;
      int ymsg = 1;

      sprintf_s(messagebuf, "MVDepan data");
      DrawString(dst, vi, xmsg, ymsg, messagebuf);

      sprintf_s(messagebuf, "fn=%5d iter=%3d error=%7.3f", nframe, iter, errorcur);
      ymsg++;
      DrawString(dst, vi, xmsg, ymsg, messagebuf);

      sprintf_s(messagebuf, "     dx      dy     rot    zoom");
      ymsg++;
      DrawString(dst, vi, xmsg, ymsg, messagebuf);

      sprintf_s(messagebuf, "%7.2f %7.2f %7.3f %7.5f", motionx[nframe], motiony[nframe], motionrot[nframe], motionzoom[nframe]);
      ymsg++;
      DrawString(dst, vi, xmsg, ymsg, messagebuf);
    }

  }

    // write global motion data in Depan plugin format to start of dest frame buffer
  write_depan_data(dst->GetWritePtr(), framefirst, framelast, motionx, motiony, motionzoom, motionrot);

  int nf = (backward) ? ndest : ndest; // set next frame number as data frame if backward
//	nframe = ndest; // set next frame number as data frame if backward

  if (logfile != NULL) // write frame number, dx, dy, rotation and zoom in Deshaker log format - aaded in v.1.2.3
    write_deshakerlog1(logfile, vi.IsFieldBased(), vi.IsTFF(), nf, motionx[nf], motiony[nf], motionzoom[nf], motionrot[nf]);


  return dst;
}
