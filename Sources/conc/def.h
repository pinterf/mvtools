/*****************************************************************************

        def.h
        Author: Laurent de Soras, 2011

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if ! defined (conc_def_HEADER_INCLUDED)
#define	conc_def_HEADER_INCLUDED

#if defined (_MSC_VER)
	#pragma once
	#pragma warning (4 : 4250)
#endif



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



namespace conc
{



// Native word size, in power of 2 bits
#if UINTPTR_MAX == 0xffffffffffffffff || defined (_M_IA64) || defined (_WIN64) || defined (__64BIT__) || defined (__x86_64__)
	#define	conc_WORD_SIZE_L2	6
	#define	conc_WORD_SIZE		64
	#define	conc_WORD_SIZE_BYTE 8
#else
	#define	conc_WORD_SIZE_L2	5
	#define	conc_WORD_SIZE		32
	#define	conc_WORD_SIZE_BYTE	4
#endif



// 128-bit compare and swap
#if (conc_WORD_SIZE_L2 >= 6)
	#define	conc_HAS_CAS_128	1
#endif

#ifndef conc_FORCEINLINE
#if defined(__clang__)
// Check clang first. clang-cl also defines __MSC_VER
// We set MSVC because they are mostly compatible
#   define CLANG
#if defined(_MSC_VER)
#   define MSVC
#   define conc_FORCEINLINE __attribute__((always_inline)) inline
#else
#   define conc_FORCEINLINE __attribute__((always_inline)) inline
#endif
#elif   defined(_MSC_VER)
#   define MSVC
#   define MSVC_PURE
#   define conc_FORCEINLINE __forceinline
#elif defined(__GNUC__)
#   define GCC
#   define conc_FORCEINLINE __attribute__((always_inline)) inline
#else
#   error Unsupported compiler.
#   define conc_FORCEINLINE inline
#   undef __forceinline
#   define __forceinline inline
#endif 

#endif




#define  conc_CHECK_CT(name, cond)	\
	typedef int conc_CHECK_CT##name##_##__LINE__ [(cond) ? 1 : -1]



#if defined (_MSC_VER)
	#define	conc_TYPEDEF_ALIGN( alignsize, srctype, dsttype)	\
		typedef __declspec (align (alignsize)) srctype dsttype
#else
  // gcc syntax allows no __declspec
#define	conc_TYPEDEF_ALIGN( alignsize, srctype, dsttype)	\
		typedef __attribute__((aligned (alignsize))) srctype dsttype
#endif



}	// namespace conc



//#include	"conc/def.hpp"



#endif	// conc_def_HEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
