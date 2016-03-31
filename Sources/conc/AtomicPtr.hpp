/*****************************************************************************

        AtomicPtr.hpp
        Author: Laurent de Soras, 2011

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if ! defined (conc_AtomicPtr_CODEHEADER_INCLUDED)
#define	conc_AtomicPtr_CODEHEADER_INCLUDED



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#include	"conc/fnc.h"
#include	"conc/Interlocked.h"

#include	<cassert>



namespace conc
{



/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



template <class T>
AtomicPtr <T>::AtomicPtr ()
:	_ptr ()
{
	assert (is_ptr_aligned_nz ((const void *) (&_ptr), sizeof (_ptr)));
}



template <class T>
AtomicPtr <T>::AtomicPtr (T *ptr)
:	_ptr (ptr)
{
	assert (is_ptr_aligned_nz ((const void *) (&_ptr), sizeof (_ptr)));
}



template <class T>
AtomicPtr <T> &	AtomicPtr <T>::operator = (T *other_ptr)
{
	Interlocked::swap (reinterpret_cast <void * volatile &> (_ptr), other_ptr);

	return (*this);
}



template <class T>
AtomicPtr <T>::operator T * () const
{
	return (read_ptr ());
}



template <class T>
bool	AtomicPtr <T>::operator == (T *other_ptr) const
{
	const T *		ptr = read_ptr ();

	return (ptr == other_ptr);
}



template <class T>
bool	AtomicPtr <T>::operator != (T *other_ptr) const
{
	return (! ((*this) == other_ptr));
}



template <class T>
T *	AtomicPtr <T>::swap (T *other_ptr)
{
	return (static_cast <T *> (Interlocked::swap (
		reinterpret_cast <void * volatile &> (_ptr),
		other_ptr
	)));
}



template <class T>
T *	AtomicPtr <T>::cas (T *other_ptr, T *comp_ptr)
{
	return (static_cast <T *> (Interlocked::cas (
		reinterpret_cast <void * volatile &> (_ptr),
		other_ptr,
		comp_ptr
	)));
}



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



template <class T>
T *	AtomicPtr <T>::read_ptr () const
{
	return (static_cast <T *> (_ptr));
}



}	// namespace conc



#endif	// conc_AtomicPtr_CODEHEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
