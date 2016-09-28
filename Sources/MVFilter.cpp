// Author: Manao
// Copyright(c)2006 A.G.Balakhnin aka Fizick - overlap, yRatioUV
// See legal notice in Copying.txt for more information
//
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



#define	NOMINMAX
#define	WIN32_LEAN_AND_MEAN
#define	NOGDI
#include	"Windows.h"
#include	"avisynth.h"

#include "MVClip.h"
#include "MVFilter.h"



MVFilter::MVFilter(const PClip &vector, const char *filterName, IScriptEnvironment *env, int group_len, int group_ofs)
{
	if (vector == 0)
		env->ThrowError("Error in %s : vector clip must be specified", filterName); //v1.8

	MVClip mvClip(vector, 0, 0, env, group_len, group_ofs);

   nWidth = mvClip.GetWidth();
   nHeight = mvClip.GetHeight();
   nHPadding = mvClip.GetHPadding();
   nVPadding = mvClip.GetVPadding();
   nBlkCount = mvClip.GetBlkCount();
   nBlkSizeX = mvClip.GetBlkSizeX();
   nBlkSizeY = mvClip.GetBlkSizeY();
   nBlkX = mvClip.GetBlkX();
   nBlkY = mvClip.GetBlkY();
   nPel = mvClip.GetPel();
   nOverlapX = mvClip.GetOverlapX();
   nOverlapY = mvClip.GetOverlapY();
   pixelType = mvClip.GetPixelType();
   xRatioUV = mvClip.GetXRatioUV(); // PF
   yRatioUV = mvClip.GetYRatioUV();
   pixelsize = mvClip.GetPixelSize(); // PF
   bits_per_pixel = mvClip.GetBitsPerPixel();

   name = filterName;
}

void MVFilter::CheckSimilarity(const MVClip &vector, const char *vectorName, IScriptEnvironment *env)
{
   if ( nWidth != vector.GetWidth() )
      env->ThrowError("Error in %s : %s's width is incorrect", name, vectorName); //v1.8
//      env->ThrowError("Error in %s : %s's width is incorrect", name.c_str(), vectorName);

   if ( nHeight != vector.GetHeight() )
      env->ThrowError("Error in %s : %s's height is incorrect", name, vectorName);
//      env->ThrowError("Error in %s : %s's height is incorrect", name.c_str(), vectorName);

   if ( nBlkSizeX != vector.GetBlkSizeX() || nBlkSizeY != vector.GetBlkSizeY())
      env->ThrowError("Error in %s : %s's block size is incorrect", name, vectorName);
//      env->ThrowError("Error in %s : %s's block size is incorrect", name.c_str(), vectorName);

   if ( nPel != vector.GetPel() )
      env->ThrowError("Error in %s : %s's pel precision is incorrect", name, vectorName);
//      env->ThrowError("Error in %s : %s's pel precision is incorrect", name.c_str(), vectorName);

   if ( nOverlapX != vector.GetOverlapX() ||  nOverlapY != vector.GetOverlapY())
      env->ThrowError("Error in %s : %s's overlap size is incorrect", name, vectorName);
//      env->ThrowError("Error in %s : %s's overlap size is incorrect", name.c_str(), vectorName);

   if ( yRatioUV != vector.GetYRatioUV() )
      env->ThrowError("Error in %s : %s's Y Ratio UV is incorrect", name, vectorName);
//      env->ThrowError("Error in %s : %s's Y Ratio UV is incorrect", name.c_str(), vectorName);
   if ( xRatioUV != vector.GetXRatioUV() )
       env->ThrowError("Error in %s : %s's X Ratio UV is incorrect", name, vectorName);

   if ( pixelsize != vector.GetPixelSize() )
       env->ThrowError("Error in %s : %s's pixel size (bit depth) is incorrect", name, vectorName);

   if ( bits_per_pixel != vector.GetBitsPerPixel() )
     env->ThrowError("Error in %s : %s's bit depth is incorrect", name, vectorName);

}
