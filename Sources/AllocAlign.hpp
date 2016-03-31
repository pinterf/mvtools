/*****************************************************************************

        AllocAlign.hpp
        Author: Laurent de Soras, 2011

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if ! defined (AllocAlign_CODEHEADER_INCLUDED)
#define	AllocAlign_CODEHEADER_INCLUDED



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#include	"def.h"

#include	<cassert>



/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



template <class T, long ALIG>
typename AllocAlign <T, ALIG>::pointer	AllocAlign <T, ALIG>::address (reference r)
{
	return (&r);
}



template <class T, long ALIG>
typename AllocAlign <T, ALIG>::const_pointer	AllocAlign <T, ALIG>::address (const_reference r)
{
	return (&r);
}



template <class T, long ALIG>
typename AllocAlign <T, ALIG>::pointer	AllocAlign <T, ALIG>::allocate (size_type n, typename std::allocator <void>::const_pointer /*ptr*/)
{
	CHECK_COMPILE_TIME (IntTypeSize, (sizeof (ptrdiff_t) >= sizeof (void *)));

	assert (n >= 0);

	using namespace std;

	const size_t	nbr_bytes = sizeof (T) * n;
	const size_t	ptr_size = sizeof (void *);
	const size_t	offset = ptr_size + ALIG - 1;
	const size_t	alloc_bytes = offset + nbr_bytes;
	void *			alloc_ptr = new char [alloc_bytes];
	pointer			zone_ptr = 0;
	if (alloc_ptr != 0)
	{
		const ptrdiff_t	alloc_l = reinterpret_cast <ptrdiff_t> (alloc_ptr);
		const ptrdiff_t	zone_l = (alloc_l + offset) & (-ALIG);
		assert (zone_l >= ptrdiff_t (alloc_l + ptr_size));
		void **			ptr_ptr = reinterpret_cast <void **> (zone_l - ptr_size);
		*ptr_ptr = alloc_ptr;
		zone_ptr = reinterpret_cast <pointer> (zone_l);
	}

	return (zone_ptr);
}



template <class T, long ALIG>
void	AllocAlign <T, ALIG>::deallocate (pointer ptr, size_type /*n*/)
{
	if (ptr != 0)
	{
		using namespace std;

		const size_t	ptr_size = sizeof (void *);
		const ptrdiff_t	zone_l = reinterpret_cast <ptrdiff_t> (ptr);
		void **			ptr_ptr = reinterpret_cast <void **> (zone_l - ptr_size);
		void *			alloc_ptr = *ptr_ptr;
		assert (alloc_ptr != 0);
		assert (reinterpret_cast <ptrdiff_t> (alloc_ptr) < zone_l);

		delete [] reinterpret_cast <char *> (alloc_ptr);
	}
}



template <class T, long ALIG>
typename AllocAlign <T, ALIG>::size_type	AllocAlign <T, ALIG>::max_size () const
{
	CHECK_COMPILE_TIME (Unsigned, (static_cast <size_type> (-1) > 0));

	return (static_cast <size_type> (-1) / sizeof (T));
}



template <class T, long ALIG>
void	AllocAlign <T, ALIG>::construct (pointer ptr, const T &t)
{
	assert (ptr != 0);

	new (ptr) T (t);
}



template <class T, long ALIG>
void	AllocAlign <T, ALIG>::destroy (pointer ptr)
{
	assert (ptr != 0);

	ptr->~T ();
}



template <class T, long ALIG>
bool	AllocAlign <T, ALIG>::operator == (AllocAlign <T, ALIG> const &other)
{
	return (true);
}



template <class T, long ALIG>
bool	AllocAlign <T, ALIG>::operator != (AllocAlign <T, ALIG> const &other)
{
	return (! operator == (other));
}



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



#endif	// AllocAlign_CODEHEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
