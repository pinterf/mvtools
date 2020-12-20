/*****************************************************************************

        Mutex.hpp
        Author: Laurent de Soras, 2011

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if ! defined (conc_Mutex_CODEHEADER_INCLUDED)
#define	conc_Mutex_CODEHEADER_INCLUDED



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



namespace conc
{



/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



Mutex::Mutex ()
:	_crit_sec ()
{
#ifdef _WIN32
	::InitializeCriticalSection (&_crit_sec);
#endif
}



Mutex::~Mutex ()
{
#ifdef _WIN32
  ::DeleteCriticalSection (&_crit_sec);
#else
  unlock();
#endif
}



void	Mutex::lock ()
{
#ifdef _WIN32
	::EnterCriticalSection (&_crit_sec);
#else
  _crit_sec.lock();
#endif
}



void	Mutex::unlock ()
{
#ifdef _WIN32
	::LeaveCriticalSection (&_crit_sec);
#else
_crit_sec.unlock();
#endif
}



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



}	// namespace conc



#endif	// conc_Mutex_CODEHEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
