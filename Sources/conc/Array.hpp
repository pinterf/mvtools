/*****************************************************************************

        Array.hpp
        Author: Laurent de Soras, 2011

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if ! defined (conc_Array_CODEHEADER_INCLUDED)
#define	conc_Array_CODEHEADER_INCLUDED



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#include	<cassert>



namespace conc
{



/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



template <class T, long LENGTH>
const typename Array <T, LENGTH>::Element &	Array <T, LENGTH>::operator [] (long pos) const
{
	assert (pos >= 0);
	assert (pos < LENGTH);

	return (_data [pos]);
}



template <class T, long LENGTH>
typename Array <T, LENGTH>::Element &	Array <T, LENGTH>::operator [] (long pos)
{
	assert (pos >= 0);
	assert (pos < LENGTH);

	return (_data [pos]);
}



template <class T, long LENGTH>
long	Array <T, LENGTH>::size ()
{
	return (LENGTH);
}



template <class T, long LENGTH>
long	Array <T, LENGTH>::length ()
{
	return (size ());
}



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



}	// namespace conc



#endif	// conc_Array_CODEHEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
