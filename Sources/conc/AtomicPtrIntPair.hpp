/*****************************************************************************

        AtomicPtrIntPair.hpp
        Author: Laurent de Soras, 2011

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if ! defined (conc_AtomicPtrIntPair_CODEHEADER_INCLUDED)
#define	conc_AtomicPtrIntPair_CODEHEADER_INCLUDED



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#include	<cassert>



namespace conc
{



/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



template <class T>
AtomicPtrIntPair <T>::AtomicPtrIntPair ()
:	_data ()
{
	conc_CHECK_CT (DataSize, sizeof (RealContent) == sizeof (DataType));

	set (0, 0);
}


template <class T>
void	AtomicPtrIntPair <T>::set (T * ptr, ptrdiff_t val)
{
	_data._content._ptr = ptr;
	_data._content._val = val;
}



template <class T>
void	AtomicPtrIntPair <T>::get (T * &ptr, ptrdiff_t &val) const
{
	assert (&ptr != 0);
	assert (&val != 0);

	Combi				res;
	Combi				old;
	do
	{
		res = _data;
		cas_combi (old, const_cast <Combi &> (_data), _data, _data);
	}
	while (old._storage != res._storage);

	ptr = res._content._ptr;
	val = res._content._val;
}



template <class T>
T *	AtomicPtrIntPair <T>::get_ptr () const
{
	return (_data._content._ptr);
}



template <class T>
ptrdiff_t	AtomicPtrIntPair <T>::get_val () const
{
	return (_data._content._val);
}



template <class T>
bool	AtomicPtrIntPair <T>::cas (T *new_ptr, T *comp_ptr)
{
	T *				old_ptr = Interlocked::cas_ptr (
		_data._content._ptr,
		new_ptr,
		comp_ptr
	);

	return (old_ptr == comp_ptr);
}



template <class T>
bool	AtomicPtrIntPair <T>::cas2 (T *new_ptr, ptrdiff_t new_val, T *comp_ptr, ptrdiff_t comp_val)
{
	Combi				newx;
	newx._content._ptr = new_ptr;
	newx._content._val = new_val;

	Combi				comp;
	comp._content._ptr = comp_ptr;
	comp._content._val = comp_val;

	Combi				old;
	cas_combi (old, _data, newx, comp);

	return (old._storage == comp._storage);
}



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



template <class T>
void	AtomicPtrIntPair <T>::cas_combi (Combi &old, Combi &dest, const Combi &excg, const Combi &comp)
{
#if (conc_WORD_SIZE == 64)

	Interlocked::cas (
		old._storage,
		dest._storage,
		excg._storage,
		comp._storage
	);

#else		// conc_WORD_SIZE

	old._storage = Interlocked::cas (
		dest._storage,
		excg._storage,
		comp._storage
	);

#endif	// conc_WORD_SIZE
}



}	// namespace conc



#endif	// conc_AtomicPtrIntPair_CODEHEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
