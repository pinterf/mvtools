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


#ifndef	__MV_AnaFlags__
#define	__MV_AnaFlags__



enum AnaFlags
{
//	MOTION_CALC_SRC_LUMA       = 0x00000001,
//	MOTION_CALC_REF_LUMA       = 0x00000002,
//	MOTION_CALC_VAR            = 0x00000004,
	MOTION_CALC_BLOCK          = 0x00000008,
	MOTION_USE_MMX             = 0x00000010,
	MOTION_USE_ISSE            = 0x00000020,
	MOTION_IS_BACKWARD         = 0x00000040,
	MOTION_SMALLEST_PLANE      = 0x00000080,
//	MOTION_COMPENSATE_LUMA     = 0x00000100,
//	MOTION_COMPENSATE_CHROMA_U = 0x00000200,
//	MOTION_COMPENSATE_CHROMA_V = 0x00000400,
	MOTION_USE_CHROMA_MOTION   = 0x00000800,

	// cpu capability flags from x264 cpu.h  // added in 1.9.5.4
	CPU_CACHELINE_32           = 0x00001000, // avoid memory loads that span the border between two cachelines
	CPU_CACHELINE_64           = 0x00002000, // 32/64 is the size of a cacheline in bytes
	CPU_MMX                    = 0x00004000,
	CPU_MMXEXT                 = 0x00008000, // MMX2 aka MMXEXT aka ISSE
	CPU_SSE                    = 0x00010000,
	CPU_SSE2                   = 0x00020000,
	CPU_SSE2_IS_SLOW           = 0x00040000, // avoid most SSE2 functions on Athlon64
	CPU_SSE2_IS_FAST           = 0x00080000, // a few functions are only faster on Core2 and Phenom
	CPU_SSE3                   = 0x00100000,
	CPU_SSSE3                  = 0x00200000,
	CPU_PHADD_IS_FAST          = 0x00400000, // pre-Penryn Core2 have a uselessly slow PHADD instruction
	CPU_SSE4                   = 0x00800000, // SSE4.1
	// force MVAnalyse to use a different function for SAD / SADCHROMA (debug)
	MOTION_USE_SSD             = 0x01000000,
	MOTION_USE_SATD            = 0x02000000
};



#endif	// __MV_AnaFlags__
