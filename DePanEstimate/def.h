/*****************************************************************************

        def.h

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

/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
#include "avs/config.h" // WIN/POSIX/ETC defines

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

#endif	// def_HEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
