/*****************************************************************************

        types.h
        Author: Laurent de Soras, 2010

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if ! defined (types_HEADER_INCLUDED)
#define	types_HEADER_INCLUDED
#pragma once

#if defined (_MSC_VER)
  #pragma warning (4 : 4250) // "Inherits via dominance."
#endif

#include <stdint.h>

// INTEL_INTRINSICS: the x86 SIMD code is compiled. CMake defines it (ENABLE_INTEL_SIMD),
// the MSVC solution does not: derive it from the target there.
#if defined(_MSC_VER) && !defined(INTEL_INTRINSICS) && !defined(NEON_INTRINSICS) && (defined(_M_IX86) || defined(_M_X64))
#define INTEL_INTRINSICS
#endif

typedef int sad_t;
typedef int64_t bigsad_t; // 16 bit overflow cases

// function tables
enum arch_t {
    NO_SIMD=0,
    USE_MMX,
    USE_SSE2,
    USE_SSE41,
    USE_SSE42,
    USE_AVX,
    USE_AVX2,
    USE_NEON // aarch64; the x86 values above are ordered by ISA level, this one is never reached from x86
};

typedef uint8_t BYTE;
/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

typedef unsigned int (SADFunction)(const uint8_t *pSrc, int nSrcPitch,
  const uint8_t *pRef, int nRefPitch);

#endif	// types_HEADER_INCLUDED


/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
