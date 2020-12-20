/*****************************************************************************

        Mutex.h
        Author: Laurent de Soras, 2011

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if ! defined (conc_Mutex_HEADER_INCLUDED)
#define	conc_Mutex_HEADER_INCLUDED

#if defined (_MSC_VER)
	#pragma once
	#pragma warning (4 : 4250)
#endif


// use std::mutex instead
#ifdef _WIN32

/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
#ifdef _WIN32
#define	NOMINMAX
#define	NOGDI
#define	WIN32_LEAN_AND_MEAN
#include	<windows.h>
#endif

#ifndef _WIN32
#include <mutex>
#endif


namespace conc
{



class Mutex
{

/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

public:

	inline			Mutex ();
	inline			~Mutex ();

	inline void		lock ();
	inline void		unlock ();



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

protected:



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:
#ifdef _WIN32
	::CRITICAL_SECTION _crit_sec;
#else
  std::mutex _crit_sec;
#endif



/*\\\ FORBIDDEN MEMBER FUNCTIONS \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:

						Mutex (const Mutex &other);
	Mutex &			operator = (const Mutex &other);
	bool				operator == (const Mutex &other) const;
	bool				operator != (const Mutex &other) const;

};	// class Mutex



}	// namespace conc



#include	"conc/Mutex.hpp"

#endif

#endif	// conc_Mutex_HEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
