/*****************************************************************************

        AtomicMem.hpp
        Author: Laurent de Soras, 2011

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if ! defined (conc_AtomicMem_CODEHEADER_INCLUDED)
#define	conc_AtomicMem_CODEHEADER_INCLUDED



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#include "conc/def.h"



namespace conc
{



/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



template <int SL2>
typename AtomicMem <SL2>::DataType	AtomicMem <SL2>::swap (volatile DataType &dest, DataType excg)
{
	static_assert ((SL2 >= 0 && SL2 <= 2), "");

	return (Interlocked::swap (dest, excg));
}



template <int SL2>
typename AtomicMem <SL2>::DataType	AtomicMem <SL2>::cas (volatile DataType &dest, DataType excg, DataType comp)
{
	static_assert ((SL2 >= 0 && SL2 <= 2), "");

	return (Interlocked::cas (dest, excg, comp));
}



AtomicMem <3>::DataType	AtomicMem <3>::swap (volatile DataType &dest, DataType excg)
{
	return (Interlocked::swap (dest, excg));
}



AtomicMem <3>::DataType	AtomicMem <3>::cas (volatile DataType &dest, DataType excg, DataType comp)
{
	return (Interlocked::cas (dest, excg, comp));
}



#if defined (fstb_HAS_CAS_128)



AtomicMem <4>::DataType	AtomicMem <4>::swap (volatile DataType &dest, DataType excg)
{
	Interlocked::Data128 old;

	Interlocked::swap (
		old,
		*reinterpret_cast <volatile Interlocked::Data128 *> (&dest),
		*reinterpret_cast <const Interlocked::Data128 *> (&excg)
	);

	const DataType	result = *reinterpret_cast <DataType *> (&old);

	return (result);
}



AtomicMem <4>::DataType	AtomicMem <4>::cas (volatile DataType &dest, DataType excg, DataType comp)
{
	Interlocked::Data128 old;

	Interlocked::cas (
		old,
		*reinterpret_cast <volatile Interlocked::Data128 *> (&dest),
		*reinterpret_cast <const Interlocked::Data128 *> (&excg),
		*reinterpret_cast <const Interlocked::Data128 *> (&comp)
	);

	const DataType	result = *reinterpret_cast <DataType *> (&old);

	return (result);
}



#endif	// fstb_HAS_CAS_128



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



}	// namespace conc



#endif	// conc_AtomicMem_CODEHEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
