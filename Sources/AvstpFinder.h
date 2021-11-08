/*****************************************************************************

        AvstpFinder.h
        Author: Laurent de Soras, 2012

Private class used by AvstpWrapper on Windows.
Handles library publication and discovery.

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if ! defined (AvstpFinder_HEADER_INCLUDED)
#define	AvstpFinder_HEADER_INCLUDED

#if defined (_MSC_VER)
	#pragma once
	#pragma warning (4 : 4250)
#endif



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#define NOMINMAX

#include <Windows.h>



class AvstpFinder
{

/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

public:

	static void    publish_lib (::HMODULE hinst);
	static ::HMODULE
	               find_lib ();

	static const wchar_t
	               _lib_name_0 [];



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

protected:



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:

	enum {         BUFFER_LEN = 32767+1 }; // Characters

	static void    compose_mapped_filename (wchar_t mf_name_0 [], wchar_t mu_name_0 []);
	static ::HMODULE
	               get_code_module ();



/*\\\ FORBIDDEN MEMBER FUNCTIONS \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:

	               AvstpFinder ()                               = delete;
	               AvstpFinder (const AvstpFinder &other)       = delete;
	               AvstpFinder (AvstpFinder &&other)            = delete;
	               ~AvstpFinder ()                              = delete;
	AvstpFinder &  operator = (const AvstpFinder &other)        = delete;
	AvstpFinder &  operator = (AvstpFinder &&other)             = delete;
	bool           operator == (const AvstpFinder &other) const = delete;
	bool           operator != (const AvstpFinder &other) const = delete;

};	// class AvstpFinder



//#include "AvstpFinder.hpp"



#endif	// AvstpFinder_HEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
