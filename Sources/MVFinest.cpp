// Pixels flow motion function
// Copyright(c)2005 A.G.Balakhnin aka Fizick

// See legal notice in Copying.txt for more information

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; version 2 of the License.
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

#include "ClipFnc.h"
#include "CopyCode.h"
#include "maskfun.h"
#include "MVFinest.h"
#include "MVFrame.h"
#include "MVGroupOfFrames.h"
#include "MVPlane.h"
#include "profile.h"
#include "SuperParams64Bits.h"


MVFinest::MVFinest(PClip _super, bool _isse, IScriptEnvironment* env) :
GenericVideoFilter(_super)
{
  cpuFlags = _isse ? env->GetCPUFlags() : 0;

	// get parameters of prepared super clip - v2.0
	SuperParams64Bits params;
	memcpy(&params, &child->GetVideoInfo().num_audio_samples, 8);
	int nHeightS = params.nHeight;
	nSuperHPad = params.nHPad;
	nSuperVPad = params.nVPad;
	int nSuperPel = params.nPel;
	int nSuperModeYUV = params.nModeYUV;
	int nSuperLevels = params.nLevels;
	nPel = nSuperPel;
	nSuperWidth = _super->GetVideoInfo().width;
	nSuperHeight = _super->GetVideoInfo().height;
	int nWidth = nSuperWidth - 2*nSuperHPad;
	int nHeight = nHeightS;
    int xRatioUV;
    int yRatioUV;
    if(!vi.IsY() && !vi.IsRGB()) {
        xRatioUV = vi.IsYUY2() ? 2 : (1 << vi.GetPlaneWidthSubsampling(PLANAR_U));
        yRatioUV = vi.IsYUY2() ? 1 : (1 << vi.GetPlaneHeightSubsampling(PLANAR_U));;
    }
    else {
        xRatioUV = 1;
        yRatioUV = 1;
    }

    pixelsize = _super->GetVideoInfo().ComponentSize();
    bits_per_pixel = _super->GetVideoInfo().BitsPerComponent();

	pRefGOF = new MVGroupOfFrames(nSuperLevels, nWidth, nHeight, nSuperPel, nSuperHPad, nSuperVPad, nSuperModeYUV, cpuFlags, xRatioUV, yRatioUV, pixelsize, bits_per_pixel, true);

//	if (nHeight != nHeightS || nHeight != vi.height || nWidth != nSuperWidth-nSuperHPad*2 || nWidth != vi.width)
//		env->ThrowError("MVFinest : different frame sizes of input clips");

	vi.width = (nWidth + 2*nSuperHPad)*nSuperPel;
	vi.height = (nHeight + 2*nSuperVPad)*nSuperPel;

}

MVFinest::~MVFinest()
{

	 delete pRefGOF;
}

//-------------------------------------------------------------------------
PVideoFrame __stdcall MVFinest::GetFrame(int n, IScriptEnvironment* env)
{
	PVideoFrame	ref	= child->GetFrame(n, env);
   BYTE *pDst[3];
	const BYTE *pRef[3];
	int nDstPitches[3], nRefPitches[3];
//	int nref;
//	unsigned char *pDstYUY2;
//	int nDstPitchYUY2;

	PVideoFrame dst = env->NewVideoFrame(vi);

  int planecount;
  if (nPel == 1) // simply copy top lines
  {
    if ((vi.pixel_type & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2)
    {
      env->BitBlt(dst->GetWritePtr(), dst->GetPitch(),
        ref->GetReadPtr(), ref->GetPitch(), dst->GetRowSize(), dst->GetHeight());
    }
    else // YUV, RGB
    {
      int planes_y[4] = { PLANAR_Y, PLANAR_U, PLANAR_V, PLANAR_A };
      int planes_r[4] = { PLANAR_G, PLANAR_B, PLANAR_R, PLANAR_A };
      int *planes = (vi.IsYUV() || vi.IsYUVA()) ? planes_y : planes_r;
      planecount = std::min(vi.NumComponents(), 3);
      for (int p = 0; p < planecount; ++p) {
        const int plane = planes[p];
        env->BitBlt(dst->GetWritePtr(plane), dst->GetPitch(plane),
          ref->GetReadPtr(plane), ref->GetPitch(plane), dst->GetRowSize(plane), dst->GetHeight(plane));
      }
    }
  }
	else	// nPel > 1
	{
		PROFILE_START(MOTION_PROFILE_2X);

		if ( (vi.pixel_type & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2 )
		{
			// planar data packed to interleaved format (same as interleved2planar by kassandro) - v2.0.0.5
			pRef[0] = ref->GetReadPtr();
			pRef[1] = pRef[0] + nSuperWidth;
			pRef[2] = pRef[1] + nSuperWidth/2;
			nRefPitches[0] = ref->GetPitch();
			nRefPitches[1] = nRefPitches[0];
			nRefPitches[2] = nRefPitches[0];
			pDst[0] = dst->GetWritePtr();
			pDst[1] = pDst[0] + dst->GetRowSize()/2;
			pDst[2] = pDst[1] + dst->GetRowSize()/4;
			nDstPitches[0] = dst->GetPitch();
			nDstPitches[1] = nDstPitches[0];
			nDstPitches[2] = nDstPitches[0];
      planecount = 3;
		}
    else
    {
      int planes_y[4] = { PLANAR_Y, PLANAR_U, PLANAR_V, PLANAR_A };
      int planes_r[4] = { PLANAR_G, PLANAR_B, PLANAR_R, PLANAR_A };
      int *planes = (vi.IsYUV() || vi.IsYUVA()) ? planes_y : planes_r;
      planecount = std::min(vi.NumComponents(), 3);
      for (int p = 0; p < planecount; ++p) {
        const int plane = planes[p];
        pDst[p] = dst->GetWritePtr(plane);
        nDstPitches[p] = dst->GetPitch(plane);
        pRef[p] = ref->GetReadPtr(plane);
        nRefPitches[p] = ref->GetPitch(plane);
      }
    }

    MVPlaneSet nMode = vi.IsY() ? YPLANE : YUVPLANES;
		pRefGOF->Update(nMode, (BYTE*)pRef[0], nRefPitches[0], (BYTE*)pRef[1], nRefPitches[1], (BYTE*)pRef[2], nRefPitches[2]);// v2.0

		MVPlane *pPlanes[3];

    MVPlaneSet planes[3] = { YPLANE, UPLANE, VPLANE };
    for (int p = 0; p < planecount; ++p) {
      const MVPlaneSet plane = planes[p];
      pPlanes[p] = pRefGOF->GetFrame(0)->GetPlane(plane);
    }

		if (nPel == 2)
		{
      for (int p = 0; p < planecount; p++) {
        MVPlane *plane = pPlanes[p];
        if (plane) {
          // merge refined planes to big singpe plane
          Merge4PlanesToBig(pDst[p], nDstPitches[p], 
            plane->GetAbsolutePointer(0,0), plane->GetAbsolutePointer(1,0), 
            plane->GetAbsolutePointer(0,1), plane->GetAbsolutePointer(1,1), 
            plane->GetExtendedWidth(), plane->GetExtendedHeight(), plane->GetPitch(), pixelsize, cpuFlags
          );
        }
      }
		}
		else if (nPel==4)
		{
      for (int p = 0; p < planecount; p++) {
        MVPlane *plane = pPlanes[p];
        if (plane) {
          // merge refined planes to big single plane
          Merge16PlanesToBig(pDst[p], nDstPitches[p],
            plane->GetAbsolutePointer(0,0), plane->GetAbsolutePointer(1,0),
            plane->GetAbsolutePointer(2,0), plane->GetAbsolutePointer(3,0),
            plane->GetAbsolutePointer(0,1), plane->GetAbsolutePointer(1,1),
            plane->GetAbsolutePointer(2,1), plane->GetAbsolutePointer(3,1),
            plane->GetAbsolutePointer(0,2), plane->GetAbsolutePointer(1,2),
            plane->GetAbsolutePointer(2,2), plane->GetAbsolutePointer(3,2),
            plane->GetAbsolutePointer(0,3), plane->GetAbsolutePointer(1,3),
            plane->GetAbsolutePointer(2,3), plane->GetAbsolutePointer(3,3),
            plane->GetExtendedWidth(), plane->GetExtendedHeight(), plane->GetPitch(), pixelsize, cpuFlags
          );
        }
      }
		}

		PROFILE_STOP(MOTION_PROFILE_2X);
	}

	return dst;
}
