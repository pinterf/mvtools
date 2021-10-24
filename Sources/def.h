/*****************************************************************************

        def.h
        Author: Laurent de Soras, 2010

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if ! defined (def_HEADER_INCLUDED)
#define	def_HEADER_INCLUDED
#pragma once

#if defined (_MSC_VER)
  #pragma warning (4 : 4250) // "Inherits via dominance."
#endif



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
#include "avs/config.h" // WIN/POSIX/ETC defines
#include "types.h"

#ifdef _WIN32
#include "avs/win.h"
#endif




#ifndef _WIN32
#define OutputDebugString(x)
#endif

#if (defined(GCC) || defined(CLANG)) && !defined(_WIN32)
#include <stdlib.h>
#define _aligned_malloc(size, alignment) aligned_alloc(alignment, size)
#define _aligned_free(ptr) free(ptr)
#endif

#ifndef _WIN32
#include <stdio.h>
#ifdef AVS_POSIX
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 1
#endif
#include <limits.h>
#endif
#endif

// Checks a constant expression to make the compiler fail if false.
// Name is a string containing only alpha+num+underscore, free of double quotes.
// Requires a ";" at the end.
#define  CHECK_COMPILE_TIME(name, cond)	\
  typedef int CHECK_COMPILE_TIME_##name##_##__LINE__ [(cond) ? 1 : -1]

const long double	PI  = 3.1415926535897932384626433832795L;
const long double	LN2 = 0.69314718055994530941723212145818L;

#ifndef MV_FORCEINLINE
#if defined(__clang__)
// Check clang first. clang-cl also defines __MSC_VER
// We set MSVC because they are mostly compatible
#   define CLANG
#if defined(_MSC_VER)
#   define MSVC
#   define MV_FORCEINLINE __attribute__((always_inline)) inline
#else
#   define MV_FORCEINLINE __attribute__((always_inline)) inline
#endif
#elif   defined(_MSC_VER)
#   define MSVC
#   define MSVC_PURE
#   define MV_FORCEINLINE __forceinline
#elif defined(__GNUC__)
#   define GCC
#   define MV_FORCEINLINE __attribute__((always_inline)) inline
#else
#   error Unsupported compiler.
#   define MV_FORCEINLINE inline
#   undef __forceinline
#   define __forceinline inline
#endif 

#endif

#if UINTPTR_MAX == 0xffffffffffffffff || defined (_M_IA64) || defined (_WIN64) || defined (__64BIT__) || defined (__x86_64__)
#define MV_64BIT
#else
#define MV_32BIT
#endif


#define MAX_BLOCK_SIZE 64

// external asm related defines, disable them for non-windows
#if defined(_WIN32) && !defined(__GNUC__)
#define USE_COPYCODE_ASM
#define USE_OVERLAPS_ASM
#define USE_SAD_ASM
#define USE_SATD_ASM
#define USE_LUMA_ASM
#define USE_FDCT88INT_ASM
#define USE_AVSTP
#else
//#define USE_COPYCODE_ASM
//#define USE_OVERLAPS_ASM
//#define USE_SAD_ASM
//#define USE_SATD_ASM
//#define USE_LUMA_ASM
//#define USE_FDCT88INT_ASM
//#define USE_AVSTP
#endif

static MV_FORCEINLINE sad_t ScaleSadChroma(sad_t sad, int effective_scale) {
  // effective scale: 1 -> div 2
  //                  2 -> div 4 (YV24 default)
  //                 -2 -> *4
  //                 -1 -> *2
  if (effective_scale == 0) return sad;
  if (effective_scale > 0) return sad >> effective_scale;
  return sad << (-effective_scale);
}

#endif	// def_HEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
