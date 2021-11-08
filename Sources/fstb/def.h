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



#pragma once
#if ! defined (fstb_def_HEADER_INCLUDED)
#define	fstb_def_HEADER_INCLUDED

#if defined (_MSC_VER)
	#pragma warning (4 : 4250)
#endif



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



namespace fstb
{



#define fstb_IS(prop, val) (defined (fstb_##prop##_##val) && (fstb_##prop) == (fstb_##prop##_##val))

#define fstb_ARCHI_X86	(1)
#define fstb_ARCHI_ARM	(2)

#if defined (__i386__) || defined (_M_IX86) || defined (_X86_) || defined (_M_X64) || defined (__x86_64__) || defined (__INTEL__)
	#define fstb_ARCHI	fstb_ARCHI_X86
#elif defined (__arm__) || defined (__arm) || defined (__arm64__) || defined (__arm64) || defined (_M_ARM) || defined (__aarch64__)
	#define fstb_ARCHI	fstb_ARCHI_ARM
#else
	#error
#endif



// Native word size, in power of 2 bits
#if defined (_WIN64) || defined (__64BIT__) || defined (__amd64__) || defined (__x86_64__) || defined (__aarch64__) || defined (__arm64__) || defined (__arm64)
	#define fstb_WORD_SIZE_L2      (6)
	#define fstb_WORD_SIZE        (64)
	#define fstb_WORD_SIZE_BYTE    (8)
#else
	#define fstb_WORD_SIZE_L2      (5)
	#define fstb_WORD_SIZE        (32)
	#define fstb_WORD_SIZE_BYTE    (4)
#endif



// Endianness
#define fstb_ENDIAN_BIG    (1)
#define fstb_ENDIAN_LITTLE (2)

#if (fstb_ARCHI == fstb_ARCHI_X86)
	#define fstb_ENDIAN fstb_ENDIAN_LITTLE
#elif (fstb_ARCHI == fstb_ARCHI_ARM)
	#if defined (__ARMEL__) || defined (__LITTLE_ENDIAN__)
		#define fstb_ENDIAN fstb_ENDIAN_LITTLE
	#else
		#define fstb_ENDIAN fstb_ENDIAN_BIG
	#endif
#else
	#error
#endif



// 64-bit and 128-bit Compare-And-Swap
#define	fstb_HAS_CAS_64	1

#if fstb_WORD_SIZE == 64
	// We just ignore the early AMD 64-bit CPU without CMPXCHG16B
	#if (fstb_ARCHI == fstb_ARCHI_X86)
		#define	fstb_HAS_CAS_128	1

	#elif (fstb_ARCHI == fstb_ARCHI_ARM)
		#define	fstb_HAS_CAS_128	1

	#endif
#endif



// System
#define fstb_SYS_UNDEF (1)
#define fstb_SYS_WIN   (2)
#define fstb_SYS_LINUX (3)
#define fstb_SYS_MACOS (4)
#if defined (linux) || defined (__linux) || defined (__linux__)
	#define fstb_SYS fstb_SYS_LINUX
#elif defined (_WIN32) || defined (WIN32) || defined (__WIN32__) || defined (__CYGWIN__) || defined (__CYGWIN32__)
	#define fstb_SYS fstb_SYS_WIN
#elif defined (macintosh) || defined (__APPLE__) || defined (__APPLE_CC__)
	#define fstb_SYS fstb_SYS_MACOS
#else
	#define fstb_SYS fstb_SYS_UNDEF
#endif



// Compiler
#define fstb_COMPILER_GCC  (1)
#define fstb_COMPILER_MSVC (2)

#if defined (__GNUC__)
	#define fstb_COMPILER fstb_COMPILER_GCC
#elif defined (_MSC_VER)
	#define fstb_COMPILER fstb_COMPILER_MSVC
	#if _MSC_VER >= 2000 && __cplusplus < 201402L
		// The MS compiler keeps __cplusplus at 199711L, even if C++14 or above
		// is enforced and standard compliance is activated. C++11 is not
		// officially supported, but almost works with _MSC_VER >= 1900.
		// /Zc:__cplusplus sets the macro to the right value for C++ >= 2014.
		#error Please compile with /Zc:__cplusplus
	#endif
#else
	#error
#endif



// Function inlining
#if defined (_MSC_VER)
   #define  fstb_FORCEINLINE   __forceinline
#elif defined (__GNUC__)
	#define  fstb_FORCEINLINE   inline __attribute__((always_inline))
#else
   #define  fstb_FORCEINLINE   inline
#endif



// Alignment
#if defined (_MSC_VER)
	#define	fstb_TYPEDEF_ALIGN( alignsize, srctype, dsttype)	\
		typedef __declspec (align (alignsize)) srctype dsttype
#elif defined (__GNUC__)
	#define	fstb_TYPEDEF_ALIGN( alignsize, srctype, dsttype)	\
		typedef srctype dsttype __attribute__ ((aligned (alignsize)))
#else
	#error Undefined for this compiler
#endif



// Restrict for pointers
#define fstb_RESTRICT __restrict



// Calling conventions and export functions
#if defined (_WIN32) && ! defined (_WIN64)
	#define fstb_CDECL __cdecl
#else
	#define fstb_CDECL
#endif
#if fstb_IS (SYS, WIN)
	#if defined (__GNUC__)
		#define fstb_EXPORT(f) extern "C" __attribute__((dllexport)) f
	#else
		#define fstb_EXPORT(f) extern "C" __declspec(dllexport) f
	#endif
#else
	#define fstb_EXPORT(f) extern "C" __attribute__((visibility("default"))) f
#endif



// constexpr functions without too much restrictions
#if (__cplusplus >= 201402L)
	#define fstb_CONSTEXPR14 constexpr
#else
	#define fstb_CONSTEXPR14
#endif



// SIMD instruction set availability
#undef fstb_HAS_SIMD
#if fstb_ARCHI == fstb_ARCHI_ARM
	#if defined (__ARM_NEON_FP)
		#define fstb_HAS_SIMD (1)
	#endif
#elif fstb_ARCHI == fstb_ARCHI_X86
	#if (fstb_WORD_SIZE == 64)
		#define fstb_HAS_SIMD (1)
	#elif fstb_COMPILER == fstb_COMPILER_MSVC
		#if defined (_M_IX86_FP) && _M_IX86_FP >= 2
			#define fstb_HAS_SIMD (1)
		#endif
	#else
		#if defined (__SSE2__)
			#define fstb_HAS_SIMD (1)
		#endif
	#endif
#endif



// Convenient helper to declare unused function parameters
template <typename... T> inline void unused (T &&...) {}



constexpr double PI      = 3.1415926535897932384626433832795;
constexpr double LN2     = 0.69314718055994530941723212145818;
constexpr double LN10    = 2.3025850929940456840179914546844;
constexpr double LOG10_2 = 0.30102999566398119521373889472449;
constexpr double LOG2_E  = 1.0  / LN2;
constexpr double LOG2_10 = LN10 / LN2;
constexpr double EXP1    = 2.7182818284590452353602874713527;
constexpr double SQRT2   = 1.4142135623730950488016887242097;

// Exact representation in 32-bit float
constexpr float  TWOP16  = 65536.f;
constexpr float  TWOP32  = TWOP16 * TWOP16;
constexpr float  TWOP64  = TWOP32 * TWOP32;
constexpr float  TWOPM16 = 1.f / TWOP16;
constexpr float  TWOPM32 = 1.f / TWOP32;
constexpr float  TWOPM64 = 1.f / TWOP64;

constexpr float  ANTI_DENORMAL_F32     = 1e-20f;
constexpr double ANTI_DENORMAL_F64     = 1e-290;
constexpr float  ANTI_DENORMAL_F32_CUB = 1e-10f;  // Anti-denormal for float numbers aimed to be raised to the power of 2 or 3.
constexpr double ANTI_DENORMAL_F64_CUB = 1e-100;



}	// namespace fstb



//#include "fstb/def.hpp"



#endif	// fstb_def_HEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
