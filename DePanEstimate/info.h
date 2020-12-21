/*
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

		The author can be contacted at:
		Donald Graft
		neuron2@attbi.com.
*/

// Borrowed from the author of IT.dll, whose name I
// could not determine. Modified for YV12 by Donald Graft.

#ifndef __INFO___H__
#define __INFO___H__

#include "avisynth.h"

class PVideoFrame;

template <typename pixel_t>
void DrawDigit(PVideoFrame &dst, int x, int y, int num, int bits_per_pixel, int xRatioShift, int yRatioShift, bool chroma);

void DrawString(PVideoFrame &dst, VideoInfo &vi, int x, int y, const char *s);
void	DrawDigitYUY2(PVideoFrame &dst, int x, int y, int num);
void	DrawStringYUY2(PVideoFrame &dst, int x, int y, const char *s);

#endif	// __INFO___H__
