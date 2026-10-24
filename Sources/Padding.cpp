// Author: Manao
// Copyright(c)2006 A.G.Balakhnin aka Fizick - YUY2

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

#include "Padding.h"
#include "ClipFnc.h"
#include <algorithm>
#include <stdint.h>


template<typename pixel_t>
void PadCorner(pixel_t *p, pixel_t v, int hPad, int vPad, int refPitch)
{
  // refPitch here is pixel_t aware
  for (int i = 0; i < vPad; i++)
  {
    if constexpr(sizeof(pixel_t) == 1)
      memset(p, v, hPad); // faster than loop
    else {
      std::fill_n(p, hPad, v);
    }
    p += refPitch;
  }
}

template<typename pixel_t>
void Padding::PadReferenceFrame(unsigned char *refFrame8, int refPitch, int hPad, int vPad, int width, int height)
{
  pixel_t *refFrame = reinterpret_cast<pixel_t *>(refFrame8);
  refPitch /= sizeof(pixel_t);

  pixel_t *pfoff = refFrame + vPad * refPitch + hPad;

  // Up-Left
  PadCorner<pixel_t>(refFrame, pfoff[0], hPad, vPad, refPitch); // PF refPitch is already pixelsize aware!
  // Up-Right
  PadCorner<pixel_t>(refFrame + hPad + width, pfoff[width - 1], hPad, vPad, refPitch);
  // Down-Left
  PadCorner<pixel_t>(refFrame + (vPad + height) * refPitch,
    pfoff[(height - 1) * refPitch], hPad, vPad, refPitch);
  // Down-Right
  PadCorner<pixel_t>(refFrame + hPad + width + (vPad + height) * refPitch,
    pfoff[(height - 1) * refPitch + width - 1], hPad, vPad, refPitch);

  // Top and bottom
  // LDS: would multiple memcpy be faster? The inner loop is very short
  // and the offset calculations are not trivial.
  for (int i = 0; i < width; i++)
  {
    pixel_t value_t = pfoff[i];
    pixel_t value_b = pfoff[i + (height - 1) * refPitch];
    pixel_t *p_t = refFrame + hPad + i;
    pixel_t *p_b = p_t + (height + vPad) * refPitch;
    for (int j = 0; j < vPad; j++)
    {
      p_t[0] = value_t;
      p_b[0] = value_b;
      p_t += refPitch;
      p_b += refPitch;
    }
  }

  // Left and right
  for (int i = 0; i < height; i++)
  {
    pixel_t value_l = pfoff[i * refPitch];
    pixel_t value_r = pfoff[i * refPitch + width - 1];
    pixel_t *p_l = refFrame + (vPad + i) * refPitch;
    pixel_t *p_r = p_l + width + hPad;
    for (int j = 0; j < hPad; j++)
    {
      p_l[j] = value_l;
      p_r[j] = value_r;
    }
  }
}

template void Padding::PadReferenceFrame<uint8_t>(unsigned char* refFrame8, int refPitch, int hPad, int vPad, int width, int height);
template void Padding::PadReferenceFrame<uint16_t>(unsigned char* refFrame8, int refPitch, int hPad, int vPad, int width, int height);
template void Padding::PadReferenceFrame<float>(unsigned char* refFrame8, int refPitch, int hPad, int vPad, int width, int height);

