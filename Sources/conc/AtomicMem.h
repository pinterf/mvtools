/*****************************************************************************

        AtomicMem.h
        Author: Laurent de Soras, 2011

Internal class for AtomicInt.

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if ! defined (conc_AtomicMem_HEADER_INCLUDED)
#define	conc_AtomicMem_HEADER_INCLUDED

#if defined (_MSC_VER)
	#pragma once
	#pragma warning (4 : 4250)
#endif



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#include	"conc/def.h"
#include	"types.h"
#include	<intrin.h>


namespace conc
{



template <int SL2>
class AtomicMem
{
public:
	typedef	int32_t	DataType;
	conc_TYPEDEF_ALIGN (4, DataType, DataTypeAlign);

	conc_FORCEINLINE static DataType
						swap (volatile DataType &dest, DataType excg);
	conc_FORCEINLINE static DataType
						cas (volatile DataType &dest, DataType excg, DataType comp);
};	// class AtomicMem



template <>
class AtomicMem <3>
{
public:
	typedef	int64_t	DataType;
	conc_TYPEDEF_ALIGN (8, DataType, DataTypeAlign);

	conc_FORCEINLINE static DataType
						swap (volatile DataType &dest, DataType excg);
	conc_FORCEINLINE static DataType
						cas (volatile DataType &dest, DataType excg, DataType comp);
};	// class AtomicMem



#if defined (conc_HAS_CAS_128)

template <>
class AtomicMem <4>
{
public:
	typedef	__m128i	DataType;
	conc_TYPEDEF_ALIGN (16, DataType, DataTypeAlign);

	conc_FORCEINLINE static DataType
						swap (volatile DataType &dest, DataType excg);
	conc_FORCEINLINE static DataType
						cas (volatile DataType &dest, DataType excg, DataType comp);
};	// class AtomicMem <4>

#endif	// conc_HAS_CAS_128



template <>
class AtomicMem <-1>
{
	// Nothing
};	// class AtomicMem <-1>



}	// namespace conc



#include	"conc/AtomicMem.hpp"



#endif	// conc_AtomicMem_HEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
