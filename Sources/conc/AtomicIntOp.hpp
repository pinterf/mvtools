/*****************************************************************************

        AtomicIntOp.hpp
        Author: Laurent de Soras, 2011

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if ! defined (conc_AtomicIntOp_CODEHEADER_INCLUDED)
#define	conc_AtomicIntOp_CODEHEADER_INCLUDED



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#include	<cassert>



namespace conc
{



/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



template <class T, class F>
void	AtomicIntOp::exec (AtomicInt <T> &atom, F &ftor)
{
	assert (&atom != 0);
	assert (&ftor != 0);

	T					val_new;
	T					val_old;
	exec_both (atom, ftor, val_old, val_new);
}



template <class T, class F>
T	AtomicIntOp::exec_old (AtomicInt <T> &atom, F &ftor)
{
	assert (&atom != 0);
	assert (&ftor != 0);

	T					val_new;
	T					val_old;
	exec_both (atom, ftor, val_old, val_new);

	return (val_old);
}



template <class T, class F>
T	AtomicIntOp::exec_new (AtomicInt <T> &atom, F &ftor)
{
	assert (&atom != 0);
	assert (&ftor != 0);

	T					val_new;
	T					val_old;
	exec_both (atom, ftor, val_old, val_new);

	return (val_new);
}



template <class T, class F>
void	AtomicIntOp::exec_both (AtomicInt <T> &atom, F &ftor, T &val_old, T &val_new)
{
	assert (&atom != 0);
	assert (&ftor != 0);

	T					val_cur;
	do
	{
		val_cur = atom;
		val_new = ftor (val_cur);
		val_old = atom.cas (val_new, val_cur);
	}
	while (val_old != val_cur);
}



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



}	// namespace conc



#endif	// conc_AtomicIntOp_CODEHEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
