/*****************************************************************************

        AtomicPtrIntPair.h
        Author: Laurent de Soras, 2011

A pointer and an integer in a single data.
The integer is intended to be used as a counter to prevent the A-B-A problem.

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if ! defined (conc_AtomicPtrIntPair_HEADER_INCLUDED)
#define	conc_AtomicPtrIntPair_HEADER_INCLUDED

#if defined (_MSC_VER)
	#pragma once
	#pragma warning (4 : 4250)
#endif



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#include	"conc/def.h"
#include	"conc/Interlocked.h"
#include	"types.h"

#include	<cstddef>



namespace conc
{



template <class T>
class AtomicPtrIntPair
{

/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

public:

						AtomicPtrIntPair ();
	virtual			~AtomicPtrIntPair () {}

	void				set (T * ptr, ptrdiff_t val);
	void				get (T * &ptr, ptrdiff_t &val) const;
	T *				get_ptr () const;
	ptrdiff_t		get_val () const;
	bool				cas (T *new_ptr, T *comp_ptr);
	bool				cas2 (T *new_ptr, ptrdiff_t new_val, T *comp_ptr, ptrdiff_t comp_val);



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

protected:



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:

#if conc_WORD_SIZE == 64

	typedef	Interlocked::Data128	DataType;
	conc_TYPEDEF_ALIGN (16, DataType, DataTypeAlign);

#else		// conc_WORD_SIZE

	typedef	int64_t	DataType;
	conc_TYPEDEF_ALIGN (8, DataType, DataTypeAlign);

#endif	// conc_WORD_SIZE

	class RealContent
	{
	public:
		T * volatile	_ptr;
		volatile ptrdiff_t
							_val;
	};

	union Combi
	{
		DataTypeAlign	_storage;
		RealContent		_content;
	};

	static void		cas_combi (Combi &old, Combi &dest, const Combi &excg, const Combi &comp);

	Combi				_data;



/*\\\ FORBIDDEN MEMBER FUNCTIONS \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:

						AtomicPtrIntPair (const AtomicPtrIntPair <T> &other);
	AtomicPtrIntPair <T> &
						operator = (const AtomicPtrIntPair <T> &other);
	bool				operator == (const AtomicPtrIntPair <T> &other);
	bool				operator != (const AtomicPtrIntPair <T> &other);

};	// class AtomicPtrIntPair



}	// namespace conc



#include	"conc/AtomicPtrIntPair.hpp"



#endif	// conc_AtomicPtrIntPair_HEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
