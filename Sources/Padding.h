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

#include "avisynth.h"

#include <cstdio>

// edge padding of the MSuper planes
class Padding
{
public:
  // replicates the edge pixels of the width x height area into a border of hPad (left), vPad (top),
  // hPadRight (right) and vPadBottom (bottom) pixels; frame points to the top-left corner of the border
  template<typename pixel_t>
  static void PadReferenceFrame(unsigned char *frame, int pitch, int hPad, int vPad, int width, int height, int hPadRight, int vPadBottom);

  template<typename pixel_t>
  static void PadReferenceFrame(unsigned char *frame, int pitch, int hPad, int vPad, int width, int height)
  {
    PadReferenceFrame<pixel_t>(frame, pitch, hPad, vPad, width, height, hPad, vPad);
  }
};



#endif	// __MV_PADDING_H__
