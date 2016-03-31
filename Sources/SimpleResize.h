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

// I (Fizick) borrow code from Tom Barry's SimpleResize here

#ifndef __SIMPLERESIZE__
#define __SIMPLERESIZE__

#include	"types.h"



class SimpleResize 
{
	int newwidth;
	int newheight;
	int oldwidth;
	int oldheight;
	unsigned int* hControl;		// weighting masks, alternating dwords for Y & UV
								// 1 qword for mask, 1 dword for src offset, 1 unused dword
	unsigned int* hControlUV;	// added by Fizick
	unsigned int* vOffsets;		// Vertical offsets of the source lines we will use
	unsigned int* vWeights;		// weighting masks, alternating dwords for Y & UV
	unsigned int* vOffsetsUV;		// Vertical offsets of the source lines we will use
	unsigned int* vWeightsUV;		// weighting masks, alternating dwords for Y & UV
	unsigned int* vWorkY;		// weighting masks 0Y0Y 0Y0Y...
	unsigned int* vWorkUV;		// weighting masks UVUV UVUV...
	bool SSE2enabled; 
	bool SSEMMXenabled; 

	void InitTables_YV12(void);

public:
	SimpleResize(int _newwidth, int _newheight, int _oldwidth, int _oldheight, long CPUFlags);
	~SimpleResize();
	void SimpleResizeDo(uint8_t *dstp,  int dst_row_size, int dst_height, int dst_pitch, 
						  const uint8_t* srcp, int src_row_size, int src_pitch, int Plane_Type); 
};



#endif	// __SIMPLERESIZE__