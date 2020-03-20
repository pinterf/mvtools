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

#ifndef __MV_DEPAN__
#define __MV_DEPAN__

#include "MVClip.h"
#include "MVFilter.h"

#include	<cstdio>



typedef struct transformstruct {
// structure of global motion transform
//  which defines source (xsrc, ysrc)  for current destination (x,y)
//   xsrc = dxc + dxx*x + dxy*y
//   ysrc = dyc + dyx*x + dyy*y
// But really only 4  parameters (dxc, dxx, dxy, dyc) are independent in used model
  float dxc;
  float dxx;
  float dxy;
  float dyc;
  float dyx;
  float dyy;
} transform;

/*! \brief Filter which get global motion gata for Depan
 */
class MVDepan
  : public GenericVideoFilter
  , public MVFilter
{
private:
  MVClip mvclip;
  bool ifZoom;
  bool ifRot;
  float pixaspect;
  float error;
  bool info;
  const char *logfilename;
  float wrongDif;
  float zeroWeight;
  int range;
  PClip mask;
  bool planar;

  FILE *logfile;

  float *blockDx; // dx vector
  float *blockDy; // dy
  sad_t *blockSAD;
  int *blockX; // blocks x position
  int *blockY;
  float *blockWeight;
  float *blockWeightMask;
  float *motionx;
  float *motiony;
  float *motionzoom;
  float *motionrot;

  char messagebuf[128];

//	FakeGroupOfPlanes *fgop;

  void write_deshakerlog1(FILE *logfile, int IsFieldBased, int IsTFF, int ndest, float motionx, float motiony, float motionzoom, float rotation);
  void write_depan_data1(unsigned char *dstp, int frame, float motionx, float motiony, float motionzoom, float motionrot);
  void write_depan_data(unsigned char *dstp, int startframe, int lastframe, float motionx[], float motiony[], float motionzoom[], float motionrot[]);
  void motion2transform(float dx1, float dy1, float rot, float zoom1, float pixaspect, float xcenter, float ycenter, int forward, float fractoffset, transform *tr);
  void transform2motion(transform tr, int forward, float xcenter, float ycenter, float pixaspect, float *dx, float *dy, float *rot, float *zoom);
  void inversetransform(transform ta, transform *tinv);
  void TrasformUpdate(transform *tr, float blockDx[], float blockDy[], sad_t blockSAD[], int blockX[], int blockY[], float blockWeight[], int nBlkX, int nBlkY, float safety, bool ifZoom, bool ifRot, float *error, float pixaspect);
  void RejectBadBlocks(transform tr, float blockDx[], float blockDy[], sad_t blockSAD[], int blockX[], int blockY[], float blockWeight[], int nBlkX, int nBlkY, float neighboursDif, float globalDif, int thSCD1, float zeroWeight, float blockWeightMask[], int ignoredBorder);

public:
  MVDepan(PClip _child, PClip mvs, PClip _mask, bool _zoom, bool _rot, float _pixaspect,
    float _error, bool _info, const char * _logfilename, float _wrong, float _zerow, int _range, sad_t nSCD1, int nSCD2, bool isse, bool _planar, IScriptEnvironment* env);
  ~MVDepan();
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env) override;

  int __stdcall SetCacheHints(int cachehints, int frame_range) override {
    return cachehints == CACHE_GET_MTMODE ? (logfile == nullptr ? MT_MULTI_INSTANCE : MT_SERIALIZED) : 0;
  }

};

#endif
