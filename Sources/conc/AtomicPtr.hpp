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

#include "conc/fnc.h"
#if (conc_ARCHI == conc_ARCHI_X86)
	#include "conc/Interlocked.h"
#endif // conc_ARCHI

#include <cassert>



namespace conc
{



/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



template <class T>
AtomicPtr <T>::AtomicPtr ()
:	_ptr ()
{
#if (conc_ARCHI == conc_ARCHI_X86)
	assert (is_ptr_aligned_nz ((const void *) (&_ptr), sizeof (_ptr)));
#endif // conc_ARCHI
}



template <class T>
AtomicPtr <T>::AtomicPtr (T *ptr)
#if (conc_ARCHI == conc_ARCHI_X86)
:	_ptr ()
#else  // conc_ARCHI
:	_ptr (ptr)
#endif // conc_ARCHI
{
#if (conc_ARCHI == conc_ARCHI_X86)
	assert (is_ptr_aligned_nz ((const void *) (&_ptr), sizeof (_ptr)));
	_ptr._void_ptr = ptr;
#endif // conc_ARCHI
}



template <class T>
AtomicPtr <T> &	AtomicPtr <T>::operator = (T *other_ptr)
{
#if (conc_ARCHI == conc_ARCHI_X86)
	Interlocked::swap (_ptr._void_ptr, other_ptr);
#else  // conc_ARCHI
	_ptr.store (other_ptr);
#endif // conc_ARCHI

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
	const T *      ptr = read_ptr ();

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
#if (conc_ARCHI == conc_ARCHI_X86)
	return (static_cast <T *> (Interlocked::swap (
		_ptr._void_ptr,
		other_ptr
	)));
#else  // conc_ARCHI
	return (_ptr.exchange (other_ptr));
#endif // conc_ARCHI
}



template <class T>
T *	AtomicPtr <T>::cas (T *other_ptr, T *comp_ptr)
{
#if (conc_ARCHI == conc_ARCHI_X86)
	return (static_cast <T *> (Interlocked::cas (
		_ptr._void_ptr,
		other_ptr,
		comp_ptr
	)));
#else  // conc_ARCHI
	// Some algorithms do something specific upon failure, so we need to
	// use the strong version.
	_ptr.compare_exchange_strong (comp_ptr, other_ptr);
	return (comp_ptr);
#endif // conc_ARCHI
}



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



template <class T>
T *	AtomicPtr <T>::read_ptr () const
{
#if (conc_ARCHI == conc_ARCHI_X86)
	return _ptr._t_ptr;
#else  // conc_ARCHI
	return _ptr.load ();
#endif // conc_ARCHI
}



}	// namespace conc



#endif	// conc_AtomicPtr_CODEHEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
