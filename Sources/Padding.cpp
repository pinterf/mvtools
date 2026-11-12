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
#include <cstring>
#include <stdint.h>


template<typename pixel_t>
static inline void fill_pixels(pixel_t *p, pixel_t v, int n)
{
  if constexpr (sizeof(pixel_t) == 1)
    memset(p, v, n);
  else
    std::fill_n(p, n, v);
}

template<typename pixel_t>
void Padding::PadReferenceFrame(unsigned char *refFrame8, int refPitch, int hPad, int vPad, int width, int height, int hPadRight, int vPadBottom)
{
  pixel_t *refFrame = reinterpret_cast<pixel_t *>(refFrame8);
  refPitch /= sizeof(pixel_t);

  pixel_t *pfoff = refFrame + vPad * refPitch + hPad;

  // Left and right: repeat the first / last pixel of each row
  for (int i = 0; i < height; i++)
  {
    pixel_t *p_row = pfoff + i * refPitch;
    fill_pixels<pixel_t>(p_row - hPad, p_row[0], hPad);
    fill_pixels<pixel_t>(p_row + width, p_row[width - 1], hPadRight);
  }

  // Top and bottom: copies of the first / last row with its left and right padding, so the corners repeat the
  // corner pixels. Whole row copies: filling the border column by column would stride over the rows (slow, more
  // so with the deep bottom border of MSuper edgepad).
  const int full_width = hPad + width + hPadRight;
  const pixel_t *p_first = refFrame + vPad * refPitch;
  const pixel_t *p_last = refFrame + (vPad + height - 1) * refPitch;
  for (int j = 0; j < vPad; j++)
    memcpy(refFrame + j * refPitch, p_first, full_width * sizeof(pixel_t));
  for (int j = 0; j < vPadBottom; j++)
    memcpy(refFrame + (vPad + height + j) * refPitch, p_last, full_width * sizeof(pixel_t));
}

template void Padding::PadReferenceFrame<uint8_t>(unsigned char* refFrame8, int refPitch, int hPad, int vPad, int width, int height, int hPadRight, int vPadBottom);
template void Padding::PadReferenceFrame<uint16_t>(unsigned char* refFrame8, int refPitch, int hPad, int vPad, int width, int height, int hPadRight, int vPadBottom);
template void Padding::PadReferenceFrame<float>(unsigned char* refFrame8, int refPitch, int hPad, int vPad, int width, int height, int hPadRight, int vPadBottom);

