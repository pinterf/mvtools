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



#if ! defined (fstb_AllocAlign_CODEHEADER_INCLUDED)
#define	fstb_AllocAlign_CODEHEADER_INCLUDED



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#include "fstb/def.h"

#include <cassert>
#if ! defined (_MSC_VER)
	#include <cstdlib>
#endif



namespace fstb
{



/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



template <class T, long ALIG>
typename AllocAlign <T, ALIG>::pointer	AllocAlign <T, ALIG>::address (reference r)
{
	return &r;
}



template <class T, long ALIG>
typename AllocAlign <T, ALIG>::const_pointer	AllocAlign <T, ALIG>::address (const_reference r)
{
	return &r;
}



template <class T, long ALIG>
typename AllocAlign <T, ALIG>::pointer	AllocAlign <T, ALIG>::allocate (size_type n, typename std::allocator <void>::const_pointer ptr)
{
	fstb::unused (ptr);
	assert (n >= 0);

	const size_t   nbr_bytes = sizeof (T) * n;

#if defined (_MSC_VER)

	pointer        zone_ptr = static_cast <pointer> (
		_aligned_malloc (nbr_bytes, ALIG)
	);

//#elif ! defined (__MINGW32__) && ! defined (__MINGW64__) && ! defined (__CYGWIN__)
#elif (defined (_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200112L) \
	&& ! defined (STM32H750xx)

	pointer        zone_ptr = 0;
	void *         tmp_ptr;
	if (posix_memalign (&tmp_ptr, ALIG, nbr_bytes) == 0)
	{
		zone_ptr = reinterpret_cast <pointer> (tmp_ptr);
	}

#else // Platform-independent implementation

	const size_t   ptr_size = sizeof (void *);
	const size_t   offset = ptr_size + ALIG - 1;
	const size_t   alloc_bytes = offset + nbr_bytes;
	void *         alloc_ptr = new char [alloc_bytes];
	pointer        zone_ptr = 0;
	if (alloc_ptr != 0)
	{
		const intptr_t    alloc_l = reinterpret_cast <intptr_t> (alloc_ptr);
		const intptr_t    zone_l  = (alloc_l + offset) & (-ALIG);
		assert (zone_l >= intptr_t (alloc_l + ptr_size));
		void **        ptr_ptr = reinterpret_cast <void **> (zone_l - ptr_size);
		*ptr_ptr = alloc_ptr;
		zone_ptr = reinterpret_cast <pointer> (zone_l);
	}

#endif

	if (zone_ptr == nullptr)
	{
#if defined (__cpp_exceptions) || ! defined (__GNUC__)
		throw std::bad_alloc ();
#endif
	}

	return (zone_ptr);
}



template <class T, long ALIG>
void	AllocAlign <T, ALIG>::deallocate (pointer ptr, size_type n)
{
	fstb::unused (n);

	if (ptr != nullptr)
	{

#if defined (_MSC_VER)

		try
		{
			_aligned_free (ptr);
		}
		catch (...)
		{
			assert (false);
		}

//#elif ! defined (__MINGW32__) && ! defined (__MINGW64__) && ! defined (__CYGWIN__)
#elif (defined (_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200112L) \
	&& ! defined (STM32H750xx)

		free (ptr);

#else // Platform-independent implementation

		const size_t   ptr_size  = sizeof (void *);
		const intptr_t zone_l    = reinterpret_cast <intptr_t> (ptr);
		void **			ptr_ptr   = reinterpret_cast <void **> (zone_l - ptr_size);
		void *			alloc_ptr = *ptr_ptr;
		assert (alloc_ptr != 0);
		assert (reinterpret_cast <intptr_t> (alloc_ptr) < zone_l);

		delete [] reinterpret_cast <char *> (alloc_ptr);

#endif
	}
}



template <class T, long ALIG>
typename AllocAlign <T, ALIG>::size_type	AllocAlign <T, ALIG>::max_size () const
{
	static_assert ((static_cast <size_type> (-1) > 0), "");

	return (static_cast <size_type> (-1) / sizeof (T));
}



template <class T, long ALIG>
void	AllocAlign <T, ALIG>::construct (pointer ptr, const T &t)
{
	assert (ptr != nullptr);

	new (ptr) T (t);
}



template <class T, long ALIG>
void	AllocAlign <T, ALIG>::destroy (pointer ptr)
{
	assert (ptr != nullptr);

	ptr->~T ();
}



template <class T, long ALIG>
bool	AllocAlign <T, ALIG>::operator == (AllocAlign <T, ALIG> const &other)
{
	fstb::unused (other);

	return true;
}



template <class T, long ALIG>
bool	AllocAlign <T, ALIG>::operator != (AllocAlign <T, ALIG> const &other)
{
	return (! operator == (other));
}



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



}	// namespace fstb



#endif	// fstb_AllocAlign_CODEHEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
