// Make a motion compensate temporal denoiser
// Author: Manao
// Copyright(c)2006 A.G.Balakhnin aka Fizick - YUY2

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

#include "MVSCDetection.h"
#include "CopyCode.h"
#include "avs\minmax.h"

MVSCDetection::MVSCDetection(PClip _child, PClip vectors, float Ysc, sad_t nSCD1, int nSCD2, bool isse, IScriptEnvironment* env) :
GenericVideoFilter(_child),
mvClip(vectors, nSCD1, nSCD2, env, 1, 0),
MVFilter(vectors, "MSCDetection", env, 1, 0)
{
  bits_per_pixel = _child->GetVideoInfo().BitsPerComponent();
  pixelsize = _child->GetVideoInfo().ComponentSize();
  if (pixelsize == 4)
    sceneChangeValue_f = clamp(Ysc, 0.0f, 1.0f); // (Ysc < 0) ? 0 : ((Ysc > 255) ? 255 : Ysc);
  else
    sceneChangeValue = clamp(int(Ysc), 0, (1 << bits_per_pixel) - 1);
}

MVSCDetection::~MVSCDetection()
{
}

PVideoFrame __stdcall MVSCDetection::GetFrame(int n, IScriptEnvironment* env)
{
   PVideoFrame dst = env->NewVideoFrame(vi);

	PVideoFrame mvn = mvClip.GetFrame(n, env);
   mvClip.Update(mvn, env);

   if ( mvClip.IsUsable() )
	{
	   if(vi.IsYUV() && !vi.IsYUY2())
	   {
//      MemZoneSet(YWPLAN(dst), 0, nWidth, nHeight, 0, 0, YPITCH(dst));
//      MemZoneSet(UWPLAN(dst), 0, nWidth/2, nHeight/2, 0, 0, UPITCH(dst));
//      MemZoneSet(VWPLAN(dst), 0, nWidth/2, nHeight/2, 0, 0, VPITCH(dst));
       // todo: MemZoneSet to uint16_t and float
		  MemZoneSet(dst->GetWritePtr(PLANAR_Y), 0, dst->GetRowSize(PLANAR_Y), dst->GetHeight(PLANAR_Y), 0, 0, dst->GetPitch(PLANAR_Y));
		  MemZoneSet(dst->GetWritePtr(PLANAR_U), 0, dst->GetRowSize(PLANAR_U), dst->GetHeight(PLANAR_U), 0, 0, dst->GetPitch(PLANAR_U));
		  MemZoneSet(dst->GetWritePtr(PLANAR_V), 0, dst->GetRowSize(PLANAR_V), dst->GetHeight(PLANAR_V), 0, 0, dst->GetPitch(PLANAR_V));
	   }
	   else
	   {
		  MemZoneSet(dst->GetWritePtr(), 0, dst->GetRowSize(), dst->GetHeight(), 0, 0, dst->GetPitch());
	   }
	}
   else {
	   if(vi.IsYUV() && !vi.IsYUY2())
	   {
//      MemZoneSet(YWPLAN(dst), sceneChangeValue, nWidth, nHeight, 0, 0, YPITCH(dst));
//      MemZoneSet(UWPLAN(dst), sceneChangeValue, nWidth/2, nHeight/2, 0, 0, UPITCH(dst));
//      MemZoneSet(VWPLAN(dst), sceneChangeValue, nWidth/2, nHeight/2, 0, 0, VPITCH(dst));
// todo: MemZoneSet to uint16_t and float
       MemZoneSet(dst->GetWritePtr(PLANAR_Y), sceneChangeValue, dst->GetRowSize(PLANAR_Y), dst->GetHeight(PLANAR_Y), 0, 0, dst->GetPitch(PLANAR_Y));
		  MemZoneSet(dst->GetWritePtr(PLANAR_U), sceneChangeValue, dst->GetRowSize(PLANAR_U), dst->GetHeight(PLANAR_U), 0, 0, dst->GetPitch(PLANAR_U));
		  MemZoneSet(dst->GetWritePtr(PLANAR_V), sceneChangeValue, dst->GetRowSize(PLANAR_V), dst->GetHeight(PLANAR_V), 0, 0, dst->GetPitch(PLANAR_V));
	   }
	   else
	   {
		  MemZoneSet(dst->GetWritePtr(), sceneChangeValue, dst->GetRowSize(), dst->GetHeight(), 0, 0, dst->GetPitch());
	   }
   }

	return dst;
}
