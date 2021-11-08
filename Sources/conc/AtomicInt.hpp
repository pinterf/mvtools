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

#include "conc/AioAdd.h"
#include "conc/AioSub.h"
#include "conc/AtomicIntOp.h"
#include "conc/fnc.h"
#if (conc_ARCHI == conc_ARCHI_X86)
	#include "conc/Interlocked.h"
#endif

#include <cassert>



namespace conc
{



/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



// 'initializing' : conversion from 'size_t' to 'volatile int', possible loss of data
#if defined (_MSC_VER)
	#pragma warning (push)
	#pragma warning (4 : 4267)
#endif



template <class T>
AtomicInt <T>::AtomicInt ()
:	_val ()
{
#if (conc_ARCHI == conc_ARCHI_X86)
	assert (is_ptr_aligned_nz ((const void *) (&_val), sizeof (_val)));
#endif // conc_ARCHI
}



template <class T>
AtomicInt <T>::AtomicInt (T val)
:	_val (val)
{
#if (conc_ARCHI == conc_ARCHI_X86)
	assert (is_ptr_aligned_nz ((const void *) (&_val), sizeof (_val)));
#endif // conc_ARCHI
}



template <class T>
AtomicInt <T>::AtomicInt (const AtomicInt <T> &other)
:	_val (T (other))
{
#if (conc_ARCHI == conc_ARCHI_X86)
	assert (is_ptr_aligned_nz ((const void *) (&_val), sizeof (_val)));
#endif // conc_ARCHI
}



template <class T>
AtomicInt <T> &	AtomicInt <T>::operator = (T other)
{
#if (conc_ARCHI == conc_ARCHI_X86)
	StoredTypeWrapper::swap (_val, other);
#else  // conc_ARCHI
	_val.store (other);
#endif // conc_ARCHI

	return (*this);
}



template <class T>
AtomicInt <T>::operator T () const
{
#if (conc_ARCHI == conc_ARCHI_X86)
	return (T (_val));
#else  // conc_ARCHI
	return (_val.load ());
#endif // conc_ARCHI
}



template <class T>
T	AtomicInt <T>::swap (T other)
{
#if (conc_ARCHI == conc_ARCHI_X86)
	return (T (StoredTypeWrapper::swap (_val, other)));
#else  // conc_ARCHI
	return (_val.exchange (other));
#endif // conc_ARCHI
}



template <class T>
T	AtomicInt <T>::cas (T other, T comp)
{
#if (conc_ARCHI == conc_ARCHI_X86)
	return (T (StoredTypeWrapper::cas (_val, other, comp)));
#else  // conc_ARCHI
	// Some algorithms do something specific upon failure, so we need to
	// use the strong version.
	_val.compare_exchange_strong (comp, other);
	return (comp);
#endif // conc_ARCHI
}



template <class T>
AtomicInt <T> &	AtomicInt <T>::operator += (const T &other)
{
#if (conc_ARCHI == conc_ARCHI_X86)
	AioAdd <T>	ftor (other);
	AtomicIntOp::exec (*this, ftor);
#else  // conc_ARCHI
	_val.fetch_add (other);
#endif // conc_ARCHI

	return (*this);
}



template <class T>
AtomicInt <T> &	AtomicInt <T>::operator -= (const T &other)
{
#if (conc_ARCHI == conc_ARCHI_X86)
	AioSub <T>	ftor (other);
	AtomicIntOp::exec (*this, ftor);
#else  // conc_ARCHI
	_val.fetch_sub (other);
#endif // conc_ARCHI

	return (*this);
}



template <class T>
AtomicInt <T> &	AtomicInt <T>::operator ++ ()
{
#if (conc_ARCHI == conc_ARCHI_X86)
	return ((*this) += 1);
#else
	++ _val;
	return (*this);
#endif
}



template <class T>
T	AtomicInt <T>::operator ++ (int)
{
#if (conc_ARCHI == conc_ARCHI_X86)
	const T        prev = _val;
	++ (*this);
	return (prev);
#else
	return (++ _val);
#endif
}



template <class T>
AtomicInt <T> &	AtomicInt <T>::operator -- ()
{
#if (conc_ARCHI == conc_ARCHI_X86)
	return ((*this) -= 1);
#else
	-- _val;
	return (*this);
#endif
}



template <class T>
T	AtomicInt <T>::operator -- (int)
{
#if (conc_ARCHI == conc_ARCHI_X86)
	const T        prev = _val;
	-- (*this);
	return (prev);
#else
	return (-- _val);
#endif
}



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



#if defined (_MSC_VER)
	#pragma warning (pop)
#endif



}	// namespace conc



#endif	// conc_AtomicInt_CODEHEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
