/*****************************************************************************

        Interlocked.h
        Author: Laurent de Soras, 2011

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if ! defined (conc_Interlocked_HEADER_INCLUDED)
#define	conc_Interlocked_HEADER_INCLUDED

#if defined (_MSC_VER)
	#pragma once
	#pragma warning (4 : 4250)
#endif



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#include <stdint.h>
#include	"conc/def.h"
#include	"types.h"


namespace conc
{



class Interlocked
{

/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

public:

	static conc_FORCEINLINE int32_t
						swap (int32_t volatile &dest, int32_t excg);
	static conc_FORCEINLINE int32_t
						cas (int32_t volatile &dest, int32_t excg, int32_t comp);

	static conc_FORCEINLINE int64_t
						swap (int64_t volatile &dest, int64_t excg);
	static conc_FORCEINLINE int64_t
						cas (int64_t volatile &dest, int64_t excg, int64_t comp);

#if defined (conc_HAS_CAS_128)

	class Data128
	{
	public:
		conc_FORCEINLINE bool
							operator == (const Data128 & other) const;
		conc_FORCEINLINE bool
							operator != (const Data128 & other) const;
							
		uint8_t		_data [16];
	};

	static void		swap (Data128 &old, volatile Data128 &dest, const Data128 &excg);
	static void		cas (Data128 &old, volatile Data128 &dest, const Data128 &excg, const Data128 &comp);

#endif

	static conc_FORCEINLINE void *
						swap (void * volatile &dest_ptr, void *excg_ptr);
	static conc_FORCEINLINE void *
						cas (void * volatile &dest_ptr, void *excg_ptr, void *comp_ptr);



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

protected:



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:

	#if (conc_WORD_SIZE == 64)
		typedef	int64_t	IntPtr;
	#else
		typedef	int32_t	IntPtr;
	#endif
	conc_CHECK_CT (IntPtrSize, (sizeof (IntPtr) == sizeof (void *)));



/*\\\ FORBIDDEN MEMBER FUNCTIONS \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:

						Interlocked ();
						Interlocked (const Interlocked &other);
	virtual			~Interlocked () {}
	Interlocked &	operator = (const Interlocked &other);
	bool				operator == (const Interlocked &other) const;
	bool				operator != (const Interlocked &other) const;

};	// class Interlocked



}	// namespace conc



#include	"conc/Interlocked.hpp"



#endif	// conc_Interlocked_HEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
