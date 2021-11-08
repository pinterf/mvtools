/*****************************************************************************

        AtomicInt.h
        Author: Laurent de Soras, 2011

This class wraps integer types and allows safe atomic concurrent operations.
See the AtomicIntOp template class to perform more complex things than
incrementing and decrementing the value.

Using some types like size_t triggers the 64-bit compatibility warning, you
can disable this compiler flag if you don't want to be annoyed.

Some thoughts about lockless programming:
Bruce Dawson, February 2007 (updated June 2008),
Lockless Programming Considerations for Xbox 360 and Microsoft Windows
http://msdn.microsoft.com/en-us/library/ee418650.aspx

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if ! defined (conc_AtomicInt_HEADER_INCLUDED)
#define	conc_AtomicInt_HEADER_INCLUDED

#if defined (_MSC_VER)
	#pragma once
	#pragma warning (4 : 4250)
#endif



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#include "conc/def.h"

#include <cstdint>

#if (conc_ARCHI == conc_ARCHI_X86)
#include "conc/AtomicMem.h"
#else  // conc_ARCHI
#include <atomic>
#endif // conc_ARCHI



namespace conc
{



template <class T>
class AtomicInt
{

/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

public:

	typedef	T	DataType;

	inline			AtomicInt ();
	inline explicit
						AtomicInt (T val);
	inline			AtomicInt (const AtomicInt <T> &other);
	inline AtomicInt <T> &
						operator = (T other);

	inline			operator T () const;

	inline T			swap (T other);
	inline T			cas (T other, T comp);

	// Beware while using the result of these operators, modification and
	// read is not atomic. Use directly the AtomicIntOp instead.
	inline AtomicInt <T> &
						operator += (const T &other);
	inline AtomicInt <T> &
						operator -= (const T &other);
	inline AtomicInt <T> &
						operator ++ ();
	inline T			operator ++ (int);
	inline AtomicInt <T> &
						operator -- ();
	inline T			operator -- (int);



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

protected:



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:

#if (conc_ARCHI == conc_ARCHI_X86)

	enum {			SZ  = sizeof (T)	};
	enum {			SL2 =    (SZ > 16) ? -1
						      : ((SZ >  8) ?  4
						      : ((SZ >  4) ?  3
						      : ((SZ >  2) ?  2
						      : ((SZ >  1) ?  1
						      :               0))))	};

	typedef	AtomicMem <SL2>	StoredTypeWrapper;
	typedef	typename StoredTypeWrapper::DataType	StoredType;
	typedef	typename StoredTypeWrapper::DataTypeAlign	StoredTypeAlign;

	static_assert (
		sizeof (T) <= sizeof (StoredType),
		"Data too big for the given storage type"
	);

	volatile StoredTypeAlign
	               _val;

#else  // conc_ARCHI

#if (__cplusplus >= 201703L)
	static_assert (
		std::atomic <T>::is_always_lock_free,
		"Atomic data must be lock-free."
	);
#endif
	std::atomic<T> _val;

#endif // conc_ARCHI



/*\\\ FORBIDDEN MEMBER FUNCTIONS \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:

};	// class AtomicInt



}	// namespace conc



#include "conc/AtomicInt.hpp"



#endif	// conc_AtomicInt_HEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
