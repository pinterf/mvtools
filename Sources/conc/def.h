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



#define conc_ARCHI_X86	1
#define conc_ARCHI_ARM	2

#if defined (__i386__) || defined (_M_IX86) || defined (_X86_) || defined (_M_X64) || defined (__x86_64__) || defined (__INTEL__)
	#define conc_ARCHI conc_ARCHI_X86
#elif defined (__arm__) || defined (__arm) || defined (__arm64__) || defined (__arm64) || defined (_M_ARM) || defined (__aarch64__)
	#define conc_ARCHI conc_ARCHI_ARM
#else
	#error
#endif



// Native word size, in power of 2 bits
#if defined (_WIN64) || defined (__64BIT__) || defined (__amd64__) || defined (__x86_64__) || defined (__aarch64__) || defined (__arm64__) || defined (__arm64)
	#define conc_WORD_SIZE_L2      6
	#define conc_WORD_SIZE        64
	#define conc_WORD_SIZE_BYTE    8
#else
	#define conc_WORD_SIZE_L2      5
	#define conc_WORD_SIZE        32
	#define conc_WORD_SIZE_BYTE    4
#endif



// 128-bit compare and swap
#if (conc_WORD_SIZE_L2 >= 6) && (conc_ARCHI == conc_ARCHI_X86 || conc_ARCHI == conc_ARCHI_ARM)
	#define conc_HAS_CAS_128   1
#endif



#if defined (_MSC_VER)
	#define conc_FORCEINLINE   __forceinline
#elif defined (__GNUC__)
	#define conc_FORCEINLINE   inline __attribute__((always_inline))
#else
	#define conc_FORCEINLINE   inline
#endif



#if defined (_MSC_VER)
	#define conc_TYPEDEF_ALIGN( alignsize, srctype, dsttype)	\
		typedef __declspec (align (alignsize)) srctype dsttype
#elif defined (__GNUC__)
	#define conc_TYPEDEF_ALIGN( alignsize, srctype, dsttype)	\
		typedef srctype __attribute__ ((aligned (alignsize))) dsttype
#else
	#error Undefined for this compiler
#endif



}	// namespace conc



//#include "conc/def.hpp"



#endif	// conc_def_HEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
