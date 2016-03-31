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

#ifndef	__MV_debugprintf__
#define	__MV_debugprintf__

/*! \brief debug printing information */
#ifdef MOTION_DEBUG



#define	NOGDI
#define	NOMINMAX
#define	WIN32_LEAN_AND_MEAN
#include "Windows.h"

#include <cstdarg>
#include <cstdio>	//required for Debug output and outfile, fixed in 1.9.5

static inline void DebugPrintf(char *fmt, ...)
{
   va_list args;
   char buf[1024];

   va_start(args, fmt);
   vsprintf(buf, fmt, args);
   OutputDebugString(buf);
}



#else	// MOTION_DEBUG



static inline void DebugPrintf(char *fmt, ...)
{
	// Nothing
}



#endif	// MOTION_DEBUG



#endif	// __MV_debugprintf__
