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


#ifndef	__MV_profile__
#define	__MV_profile__



// profiling
#define MOTION_PROFILE_ME              0
#define MOTION_PROFILE_LUMA_VAR        1
#define MOTION_PROFILE_COMPENSATION    2
#define MOTION_PROFILE_INTERPOLATION   3
#define MOTION_PROFILE_PREDICTION      4
#define MOTION_PROFILE_2X              5
#define MOTION_PROFILE_MASK            6
#define MOTION_PROFILE_RESIZE          7
#define MOTION_PROFILE_FLOWINTER       8
#define MOTION_PROFILE_YUY2CONVERT     9
#define MOTION_PROFILE_COUNT           10



#ifdef MOTION_PROFILE



#include <emmintrin.h>
#include	"debugprintf.h"

extern int ProfTimeTable[MOTION_PROFILE_COUNT];
extern int ProfResults[MOTION_PROFILE_COUNT * 2];
extern int64_t ProfCumulatedResults[MOTION_PROFILE_COUNT * 2];

#define PROFILE_START(x) \
{ \
   ProfTimeTable[x] =_rdtsc();
}

#define PROFILE_STOP(x) \
{ \
   ProfResults[2 * x] += _rdtsc() - ProfTimeTable[x]; \
   ProfResults[2 * x + 1]++; \
}

inline static void PROFILE_INIT()
{
   for ( int i = 0; i < MOTION_PROFILE_COUNT; i++ )
   {
      ProfTimeTable[i]                 = 0;
      ProfResults[2 * i]               = 0;
      ProfResults[2 * i + 1]           = 0;
      ProfCumulatedResults[2 * i]      = 0;
      ProfCumulatedResults[2 * i + 1]  = 0;
   }
}
inline static void PROFILE_CUMULATE()
{
   for ( int i = 0; i < MOTION_PROFILE_COUNT; i++ )
   {
      ProfCumulatedResults[2 * i]     += ProfResults[2 * i];
      ProfCumulatedResults[2 * i + 1] += ProfResults[2 * i + 1];
      ProfResults[2 * i]     = 0;
      ProfResults[2 * i + 1] = 0;
   }
}
inline static void PROFILE_SHOW()
{
   int64_t nTotal = 0;
   for ( int i = 0; i < MOTION_PROFILE_COUNT; i++ )
      nTotal += ProfCumulatedResults[2*i];

   DebugPrintf("ME            : %2.1f %%", (double)ProfCumulatedResults[2 * MOTION_PROFILE_ME] * 100.0f / (nTotal + 1));
   DebugPrintf("LUMA & VAR    : %2.1f %%", (double)ProfCumulatedResults[2 * MOTION_PROFILE_LUMA_VAR] * 100.0f / (nTotal + 1));
   DebugPrintf("COMPENSATION  : %2.1f %%", (double)ProfCumulatedResults[2 * MOTION_PROFILE_COMPENSATION] * 100.0f / (nTotal + 1));
   DebugPrintf("INTERPOLATION : %2.1f %%", (double)ProfCumulatedResults[2 * MOTION_PROFILE_INTERPOLATION] * 100.0f / (nTotal + 1));
   DebugPrintf("PREDICTION    : %2.1f %%", (double)ProfCumulatedResults[2 * MOTION_PROFILE_PREDICTION] * 100.0f / (nTotal + 1));
   DebugPrintf("2X            : %2.1f %%", (double)ProfCumulatedResults[2 * MOTION_PROFILE_2X] * 100.0f / (nTotal + 1));
   DebugPrintf("MASK          : %2.1f %%", (double)ProfCumulatedResults[2 * MOTION_PROFILE_MASK] * 100.0f / (nTotal + 1));
   DebugPrintf("RESIZE        : %2.1f %%", (double)ProfCumulatedResults[2 * MOTION_PROFILE_RESIZE] * 100.0f / (nTotal + 1));
   DebugPrintf("FLOWINTER     : %2.1f %%", (double)ProfCumulatedResults[2 * MOTION_PROFILE_FLOWINTER] * 100.0f / (nTotal + 1));
   DebugPrintf("YUY2CONVERT     : %2.1f %%", (double)ProfCumulatedResults[2 * MOTION_PROFILE_YUY2CONVERT] * 100.0f / (nTotal + 1));
}



#else	// MOTION_PROFILE



#define PROFILE_START(x)
#define PROFILE_STOP(x)
#define PROFILE_INIT()
#define PROFILE_CUMULATE()
#define PROFILE_SHOW()



#endif	// MOTION_PROFILE



#endif	// __MV_profile__