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



// Checks a constant expression to make the compiler fail if false.
// Name is a string containing only alpha+num+underscore, free of double quotes.
// Requires a ";" at the end.
#define  CHECK_COMPILE_TIME(name, cond)	\
	typedef int CHECK_COMPILE_TIME_##name##_##__LINE__ [(cond) ? 1 : -1]

const long double	PI  = 3.1415926535897932384626433832795L;
const long double	LN2 = 0.69314718055994530941723212145818L;

#ifndef MV_FORCEINLINE
#define MV_FORCEINLINE __forceinline
#endif
/*
C++ does not allow macroizing reserved words like inline
#ifdef FORCE_INLINE // set compiler variable for Release with fastest run speed, but slow compile!
#define inline __forceinline
#else
#define inline inline
#endif
*/


#endif	// def_HEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
