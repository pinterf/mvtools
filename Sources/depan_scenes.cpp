/* 
  DePan plugin for Avisynth 2.6 - global motion compensation
	Version 1.9, November 5, 2006. 
	(DePanScenes function)
	Copyright(c) 2004-2006, A.G. Balakhnin aka Fizick
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


	DePanScenes function -  
	generate clip with pixel values =255 (65535 for 16 bit) for defined plane at scenechange and pixel values =0 at rest frames,
	using motion data previously calculated by DePanEstimate

	Parameters of DePanScenes:
		clip - input clip (special service clip with coded motion data, produced by DePanEstimate)
		inputlog - name of input log file in Deshaker format (default - none, not read)
		plane - code of plane to mark (1 - Y, 2 - U, 4 - V, sum - combination, default=1)

  Modded by pinterf June 2016
  -  planar color spaces (Avisynth interface version 6)
  - 
*/

#include "windows.h"
#include <avisynth.h>
#include "stdio.h"
#include <stdint.h>

#include "depanio.h"
#include "depan.h"



//****************************************************************************
//****************************************************************************
//****************************************************************************
//
//                DePanScenes function
//	generate clip with luma=255 at scenechange and luma=0 at rest frames,
//	using motion data previously calculated by DePanEstimate
//
//****************************************************************************
//****************************************************************************
//****************************************************************************

class DePanScenes : public GenericVideoFilter {   
  // DePan defines the name of your filter class. 
  // This name is only used internally, and does not affect the name of your filter or similar.
  // This filter extends GenericVideoFilter, which incorporates basic functionality.
  // All functions present in the filter must also be present here.

	// filter parameters
//	child  - motion data clip
	int plane; // which plane to mark
	const char *inputlog;  // filename of input log file in Deshaker format

// motion tables
	float * motionx;
	float * motiony;
	float * motionrot;
	float * motionzoom;

  int pixelsize;
  int bits_per_pixel;

public:
  // This defines that these functions are present in your class.
  // These functions must be that same as those actually implemented.
  // Since the functions are "public" they are accessible to other classes.
  // Otherwise they can only be called from functions within the class itself.

	DePanScenes(PClip _child, int _plane, const char * _inputlog, IScriptEnvironment* env);
  // This is the constructor. It does not return any value, and is always used, 
  //  when an instance of the class is created.
  // Since there is no code in this, this is the definition.

  ~DePanScenes();
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
DePanScenes::DePanScenes(PClip _child, int _plane, const char * _inputlog, IScriptEnvironment* env) :
	GenericVideoFilter(_child), plane(_plane), inputlog(_inputlog) {
  // This is the implementation of the constructor.
  // The child clip (source clip) is inherited by the GenericVideoFilter,
  //  where the following variables gets defined:
  //   PClip child;   // Contains the source clip.
  //   VideoInfo vi;  // Contains videoinfo on the source clip.


	int error;
	int loginterlaced;

  pixelsize = vi.ComponentSize();
  bits_per_pixel = vi.BitsPerComponent();

	if (!vi.IsYUV() && !vi.IsYUY2())
		env->ThrowError("DePanScenes: input to filter must be in YUV or YUY2!");

	if (plane <1 || plane > 7)
		env->ThrowError("DePanScenes: Plane parameter must be from 1 to 7!");

  motionx = new float[vi.num_frames]; 
	motiony = new float[vi.num_frames]; 
	motionrot = new float[vi.num_frames]; 
	motionzoom = new float[vi.num_frames]; 	

//	child->SetCacheHints(CACHE_RANGE,0); - disabled in v.1.9

	if (inputlog != "") { // motion data will be readed from deshaker.log file once at start
		error = read_deshakerlog(inputlog,vi.num_frames,motionx,motiony,motionrot,motionzoom,&loginterlaced);
		if (error==-1)	env->ThrowError("DePanScenes: Input log file not found!");
		if (error==-2)	env->ThrowError("DePanScenes: Error input log file format!");
		if (error==-3)	env->ThrowError("DePanScenes: Too many frames in input log file!");
//		if(vi.IsFieldBased  && loginterlaced==0)	env->ThrowError("DePanScenes: Input log must be in interlaced for fieldbased!");
	}
	else { // motion data will be requesred from DepanEstimate

		for (int i=0; i<vi.num_frames; i++) {
			motionx [i] = MOTIONUNKNOWN;  // init as unknown for all frames
//			motionrot[i] = 0;  // zero rotation for all frames (it is not estimated in current version) 
		}
	}

	//crop clip to min size of depan data for more fast procesing
//	vi.height = 4;  // safe value. depan need in 1 only, but 1 is not possible for YV12

}

//****************************************************************************
// This is where any actual destructor code used goes
DePanScenes::~DePanScenes() {
  // This is where you can deallocate any memory you might have used.

	delete[] motionx;
  delete[] motiony;
  delete[] motionrot;
  delete[] motionzoom;

}



//
// ****************************************************************************
//
PVideoFrame __stdcall DePanScenes::GetFrame(int ndest, IScriptEnvironment* env) {
  // This is the implementation of the GetFrame function.

  int h, w;
  const BYTE * srcp;
  BYTE * dstp;
  int src_width, src_height, src_pitch;
  int dst_pitch;

  // get source depan data frame (with motion data)
  PVideoFrame src = child->GetFrame(ndest, env);
  // get pointer to data frame
  srcp = src->GetReadPtr();

  bool isSceneChange = false;
  // get motion info about frames in interval from prev source to dest

  if (motionx[ndest] == MOTIONUNKNOWN) { // motion data is unknown for needed frame
    // note: if inputlogfile has been read, all motion data are always known

    // get motiondata from DePanData clip framebuffer
    int error = read_depan_data(srcp, motionx, motiony, motionzoom, motionrot, ndest);
    // check if correct
    if (error != 0) env->ThrowError("DePanScenes: input clip is NOT good DePanEstimate clip !");
  }

  if (motionx[ndest] == MOTIONBAD) isSceneChange = true; // if any strictly =0,  than no good

  int mark = isSceneChange ? (1 << bits_per_pixel) -1 : 0; // mark scenechange as max value

  // Construct a new frame based on the information of the current frame
  // contained in the "vi" struct.

  PVideoFrame dst = env->NewVideoFrame(vi);
  // Request a Write pointer from the newly created destination image.

	if (!vi.IsYUY2())
  {
    int planes[] = { PLANAR_Y, PLANAR_U, PLANAR_V };
    int plane_count = vi.IsY() ? 1 : 3;

    for (int p = 0; p < plane_count; p++)
    {
      const int workplane = planes[p];

      srcp = src->GetReadPtr(workplane);

      src_width = vi.width >> (1 << vi.GetPlaneWidthSubsampling(workplane));    // src->GetRowSize(workplane);
      src_height = vi.height >> (1 << vi.GetPlaneHeightSubsampling(workplane)); //  src->GetHeight(workplane);
      src_pitch = src->GetPitch(workplane);

      dstp = dst->GetWritePtr(workplane);
      dst_pitch = dst->GetPitch(workplane);

      bool plane_match = (plane & (1 << p)) != 0;  // Y: &1  U: &2  V: &4

      if (plane_match)
      {
        // mark 
        for (h = 0; h < src_height; h++)
        {
          if (pixelsize==1) {
            for (w = 0; w < src_width; w++)
              dstp[w] = mark;
          }
          else {
            for (w = 0; w < src_width; w++)
              (reinterpret_cast<uint16_t *>(dstp))[w] = mark;
          }
          srcp += src_pitch;
          dstp += dst_pitch;
        }
      }
      else {
        // copy 
        for (h = 0; h < src_height; h++)
        {
          if (pixelsize==1) {
            for (w = 0; w < src_width; w++)
              dstp[w] = srcp[w];
          }
          else {
            for (w = 0; w < src_width; w++)
              (reinterpret_cast<uint16_t *>(dstp))[w] = (reinterpret_cast<const uint16_t *>(srcp))[w];
          }
          srcp += src_pitch;
          dstp += dst_pitch;
        }
      }
    } // for
	}
  else
  { // YUY2
    srcp = src->GetReadPtr();
    src_width = src->GetRowSize();
    src_height = src->GetHeight();
    src_pitch = src->GetPitch();
    dstp = dst->GetWritePtr();
    dst_pitch = dst->GetPitch();

    // fill dest frame
    for (h = 0; h < src_height; h++)
    {
      if (plane == 1) // Y
      { // mark
        for (w = 0; w < src_width; w += 4)
        {
          dstp[w] = mark;
          dstp[w + 1] = srcp[w + 1];
          dstp[w + 2] = mark;
          dstp[w + 3] = srcp[w + 3];
        }
      }
      else if (plane = 2) // U
      { // copy
        for (w = 0; w < src_width; w += 4)
        {
          dstp[w] = srcp[w];
          dstp[w + 1] = mark;
          dstp[w + 2] = srcp[w + 2];
          dstp[w + 3] = srcp[w + 3];
        }
      }
      else if (plane = 3) // Y,U
      { // copy
        for (w = 0; w < src_width; w += 4)
        {
          dstp[w] = mark;
          dstp[w + 1] = mark;
          dstp[w + 2] = mark;
          dstp[w + 3] = srcp[w + 3];
        }
      }
      else if (plane = 4) // V
      { // copy
        for (w = 0; w < src_width; w += 4)
        {
          dstp[w] = srcp[w];
          dstp[w + 1] = srcp[w + 1];
          dstp[w + 2] = srcp[w + 2];
          dstp[w + 3] = mark;
        }
      }
      else if (plane = 5) // Y,V
      { // copy
        for (w = 0; w < src_width; w += 4)
        {
          dstp[w] = mark;
          dstp[w + 1] = srcp[w + 1];
          dstp[w + 2] = mark;
          dstp[w + 3] = mark;
        }
      }
      else if (plane = 6) // U,V
      { // copy
        for (w = 0; w < src_width; w += 4)
        {
          dstp[w] = srcp[w];
          dstp[w + 1] = mark;
          dstp[w + 2] = srcp[w + 2];
          dstp[w + 3] = mark;
        }
      }
      else if (plane = 7) // Y,U,V
      { // copy
        for (w = 0; w < src_width; w += 4)
        {
          dstp[w] = mark;
          dstp[w + 1] = mark;
          dstp[w + 2] = mark;
          dstp[w + 3] = mark;
        }
      }
      srcp += src_pitch;
      dstp += dst_pitch;
    }

  }

  return dst;
}

//****************************************************************************
// This is the function that created the filter, when the filter has been called.
// This can be used for simple parameter checking, so it is possible to create different filters,
// based on the arguments recieved.

AVSValue __cdecl Create_DePanScenes(AVSValue args, void* user_data, IScriptEnvironment* env) {
    return new DePanScenes(args[0].AsClip(), // the 0th parameter is the motion data clip
		 args[1].AsInt(1),  // plane
		 args[2].AsString(""),  // inputlog
		 env);  
    // Calls the constructor with the arguments provided.
}
//****************************************************************************




