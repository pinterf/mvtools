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


#ifndef	__MV_SuperParams64Bits__
#define	__MV_SuperParams64Bits__



// MVSuper parameters packed to 64 bit num_audio_samples
class SuperParams64Bits
{
public:
  uint16_t nHeight;
  unsigned char nHPad;
  unsigned char nVPad;
  unsigned char nPel;
  unsigned char nModeYUV;
  unsigned char nLevels;
  unsigned char param;
};



#endif	// __MV_SuperParams64Bits__