/*****************************************************************************

        AllocAlign.h
        Author: Laurent de Soras, 2011

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if ! defined (AllocAlign_HEADER_INCLUDED)
#define	AllocAlign_HEADER_INCLUDED

#if defined (_MSC_VER)
	#pragma once
	#pragma warning (4 : 4250)
#endif



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#include	<memory>

#include	<cstddef>



template <typename T, long ALIG = 16>
class AllocAlign
{

/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

public:

	enum {				ALIGNMENT	= ALIG	};

	typedef	T	value_type;
	typedef	value_type *	pointer;
	typedef	const value_type *	const_pointer;
	typedef	value_type &	reference;
	typedef	const value_type &	const_reference;
	typedef	size_t	size_type;
	typedef	ptrdiff_t	difference_type;

						AllocAlign () {}
						AllocAlign (AllocAlign <T, ALIG> const &other) {}
	template <typename U>
						AllocAlign (AllocAlign <U, ALIG> const &other) {}
						~AllocAlign () {}

	// Address
	inline pointer	address (reference r);
	inline const_pointer
						address (const_reference r);

	// Memory allocation
	inline pointer	allocate (size_type n, typename std::allocator <void>::const_pointer ptr = 0);
	inline void		deallocate (pointer p, size_type n);

	// Size
	inline size_type
						max_size() const;

	// Construction/destruction
	inline void		construct (pointer ptr, const T & t);
	inline void		destroy (pointer ptr);

	inline bool		operator == (AllocAlign <T, ALIG> const &other);
	inline bool		operator != (AllocAlign <T, ALIG> const &other);

	template <typename U>
	struct rebind
	{
		typedef AllocAlign <U, ALIG> other;
	};



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

protected:



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:



/*\\\ FORBIDDEN MEMBER FUNCTIONS \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:

};	// class AllocAlign



#include	"AllocAlign.hpp"



#endif	// AllocAlign_HEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
