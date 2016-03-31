/*****************************************************************************

        AtomicInt.hpp
        Author: Laurent de Soras, 2011

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if ! defined (conc_AtomicInt_CODEHEADER_INCLUDED)
#define	conc_AtomicInt_CODEHEADER_INCLUDED



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#include	"conc/AioAdd.h"
#include	"conc/AioSub.h"
#include	"conc/AtomicIntOp.h"
#include	"conc/fnc.h"
#include	"conc/Interlocked.h"

#include	<cassert>



using namespace conc;

namespace conc
{



/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



// 'initializing' : conversion from 'size_t' to 'volatile int', possible loss of data
#pragma warning (push)
#pragma warning (4 : 4267)



template <class T>
AtomicInt <T>::AtomicInt ()
:	_val ()
{
	assert (is_ptr_aligned_nz ((const void *) (&_val), sizeof (_val)));
}



template <class T>
AtomicInt <T>::AtomicInt (T val)
:	_val (val)
{
	assert (is_ptr_aligned_nz ((const void *) (&_val), sizeof (_val)));
}



template <class T>
AtomicInt <T>::AtomicInt (const AtomicInt <T> &other)
:	_val (other._val)
{
	assert (is_ptr_aligned_nz ((const void *) (&_val), sizeof (_val)));
	assert (&other != 0);
}



template <class T>
AtomicInt <T> &	AtomicInt <T>::operator = (T other)
{
	StoredTypeWrapper::swap (_val, other);

	return (*this);
}



template <class T>
AtomicInt <T>::operator T () const
{
	return (T (_val));
}



template <class T>
T	AtomicInt <T>::swap (T other)
{
	return (T (StoredTypeWrapper::swap (_val, other)));
}



template <class T>
T	AtomicInt <T>::cas (T other, T comp)
{
	return (T (StoredTypeWrapper::cas (_val, other, comp)));
}



template <class T>
AtomicInt <T> &	AtomicInt <T>::operator += (const T &other)
{
	assert (&other != 0);

	AioAdd <T>	ftor (other);
	AtomicIntOp::exec (*this, ftor);

	return (*this);
}



template <class T>
AtomicInt <T> &	AtomicInt <T>::operator -= (const T &other)
{
	assert (&other != 0);

	AioSub <T>	ftor (other);
	AtomicIntOp::exec (*this, ftor);

	return (*this);
}



template <class T>
AtomicInt <T> &	AtomicInt <T>::operator ++ ()
{
	return ((*this) += 1);
}



template <class T>
T	AtomicInt <T>::operator ++ (int)
{
	const T			prev = _val;

	++ (*this);

	return (prev);
}



template <class T>
AtomicInt <T> &	AtomicInt <T>::operator -- ()
{
	return ((*this) -= 1);
}



template <class T>
T	AtomicInt <T>::operator -- (int)
{
	const T			prev = _val;

	-- (*this);

	return (prev);
}



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



#pragma warning (pop)



}	// namespace conc



#endif	// conc_AtomicInt_CODEHEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
