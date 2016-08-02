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


#ifndef __MV_PADDING_H__
#define __MV_PADDING_H__

#define	NOGDI
#define	NOMINMAX
#define	WIN32_LEAN_AND_MEAN
#include "windows.h"
#include "avisynth.h"
#include "yuy2planes.h"

#include <cstdio>



class Padding
:	public GenericVideoFilter
{

private:
	int horizontalPadding;
	int verticalPadding;
	bool planar;

	int width;
	int height;
	YUY2Planes *DstPlanes;
	YUY2Planes *SrcPlanes;

public:
	Padding(PClip _child, int hPad, int vPad, bool _planar, IScriptEnvironment* env);
	~Padding();
	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
    
    template<typename pixel_t>
	static void PadReferenceFrame(unsigned char *frame, int pitch, int hPad, int vPad, int width, int height);

};



#endif	// __MV_PADDING_H__
