/*****************************************************************************

        fnc.hpp
        Author: Laurent de Soras, 2011

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if ! defined (conc_fnc_CODEHEADER_INCLUDED)
#define	conc_fnc_CODEHEADER_INCLUDED



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#include	<cassert>



namespace conc
{



template <class T>
bool	is_ptr_aligned (const T *ptr, int align)
{
	assert (align > 0);
	assert ((align & -align) == align);

	return ((reinterpret_cast <ptrdiff_t> (ptr) & (align - 1)) == 0);
}

template <class T>
bool	is_ptr_aligned_nz (const T *ptr, int align)
{
	assert (align > 0);
	assert ((align & -align) == align);

	return (ptr != 0 && is_ptr_aligned (ptr, align));
}

template <class T>
bool	is_ptr_aligned_nz (const T *ptr)
{
	return (is_ptr_aligned_nz (ptr, sizeof (T)));
}



}	// namespace conc



#endif	// conc_fnc_CODEHEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
