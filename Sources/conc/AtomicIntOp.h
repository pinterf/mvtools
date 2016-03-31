/*****************************************************************************

        AtomicIntOp.h
        Author: Laurent de Soras, 2011

A convenience tool to wrap an operation on an AtomicInt in a Compare-And-Swap
loop. The goal is to guarantee that no other thread has _changed_ the value
between the reading and the update of the variable. If this happens, the
variable is not written and a read-compute-CAS cycle is started again,
until the atomicity criterion is met. Of course, in case of heavy contention,
long operations are more prone to fail and may be excessively slow to
complete, so they should be avoided.

Note: AtomicIntOp is not intended to address the A-B-A problem, occuring when
another thread changes the value multiple times and finally set it to the
value initially read by the first thread. To fix this issue, reserve some
bits of your variable for a counter incremented at each operation.

The exec_* functions differ only by the returned value (before or after the
operation).

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if ! defined (conc_AtomicIntOp_HEADER_INCLUDED)
#define	conc_AtomicIntOp_HEADER_INCLUDED

#if defined (_MSC_VER)
	#pragma once
	#pragma warning (4 : 4250)
#endif



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#include	"conc/AtomicInt.h"



namespace conc
{



class AtomicIntOp
{

/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

public:

	template <class T, class F>
	static inline void
						exec (AtomicInt <T> &atom, F &ftor);
	template <class T, class F>
	static inline T
						exec_old (AtomicInt <T> &atom, F &ftor);
	template <class T, class F>
	static inline T
						exec_new (AtomicInt <T> &atom, F &ftor);
	template <class T, class F>
	static inline void
						exec_both (AtomicInt <T> &atom, F &ftor, T &val_old, T &val_new);



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

protected:



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:



/*\\\ FORBIDDEN MEMBER FUNCTIONS \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:

						AtomicIntOp ();
						AtomicIntOp (const AtomicIntOp &other);
	virtual			~AtomicIntOp () {}
	AtomicIntOp &	operator = (const AtomicIntOp &other);
	bool				operator == (const AtomicIntOp &other) const;
	bool				operator != (const AtomicIntOp &other) const;

};	// class AtomicIntOp



}	// namespace conc



#include	"conc/AtomicIntOp.hpp"



#endif	// conc_AtomicIntOp_HEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
