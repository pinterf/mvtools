/*****************************************************************************

        Interlocked.hpp
        Author: Laurent de Soras, 2011

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if ! defined (conc_Interlocked_CODEHEADER_INCLUDED) && defined(conc_Interlocked_CODEBODY)
#define	conc_Interlocked_CODEHEADER_INCLUDED



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#include	"conc/def.h"
#include	"conc/fnc.h"
#include	"conc/Interlocked.h"
#include	<intrin.h>
#include <stdint.h>


#include	<cassert>



namespace conc
{



/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



int32_t	Interlocked::swap (int32_t volatile &dest, int32_t excg)
{
	assert (is_ptr_aligned_nz (&dest));

	return (
		_InterlockedExchange (reinterpret_cast <volatile long *> (&dest), excg)
	);
}



int32_t	Interlocked::cas (int32_t volatile &dest, int32_t excg, int32_t comp)
{
	assert (is_ptr_aligned_nz (&dest));

	return (_InterlockedCompareExchange (
		reinterpret_cast <volatile long *> (&dest),
		excg,
		comp
	));
}



int64_t	Interlocked::swap (int64_t volatile &dest, int64_t excg)
{
	assert (is_ptr_aligned_nz (&dest));

	int64_t			old;

#if conc_WORD_SIZE == 64

	old = _InterlockedExchange64 (&dest, excg);

#else	// conc_WORD_SIZE

	__asm
	{
		push				ebx
		mov				esi, [dest]
		mov				ebx, [dword ptr excg    ]
		mov				ecx, [dword ptr excg + 4]

	cas_loop:
		mov				eax, [esi    ]
		mov				edx, [esi + 4]
		lock cmpxchg8b	[esi]
		jnz				cas_loop

		mov				[dword ptr old    ], eax
		mov				[dword ptr old + 4], edx
		pop				ebx
	}

#endif	// conc_WORD_SIZE

	return (old);
}



int64_t	Interlocked::cas (int64_t volatile &dest, int64_t excg, int64_t comp)
{
	assert (is_ptr_aligned_nz (&dest));

	return (_InterlockedCompareExchange64 (&dest, excg, comp));
}



#if defined (conc_HAS_CAS_128)



void	Interlocked::swap (Data128 &old, volatile Data128 &dest, const Data128 &excg)
{
	assert (&old != 0);
	assert (is_ptr_aligned_nz (&dest));

	Data128			tmp;
	do
	{
		old = *(const Data128 *) (&dest);
		cas (tmp, dest, excg, old);
	}
	while (tmp != old);
}



void	Interlocked::cas (Data128 &old, volatile Data128 &dest, const Data128 &excg, const Data128 &comp)
{
	assert (&old != 0);
	assert (is_ptr_aligned_nz (&dest));

	#if ((_MSC_VER / 100) >= 15)	// VC2008 or above

		const int64_t	excg_lo = ((const int64_t *) &excg) [0];
		const int64_t	excg_hi = ((const int64_t *) &excg) [1];

		old = comp;

		_InterlockedCompareExchange128 (
			reinterpret_cast <volatile int64_t *> (&dest),
			excg_hi,
			excg_lo,
			reinterpret_cast <int64_t *> (&old)
		);

	#else

		/*** To do ***/
		#error Requires assembly code

	#endif
}



bool	Interlocked::Data128::operator == (const Data128 & other) const
{
  __m128i var1 = _mm_load_si128((__m128i*)_data);
  __m128i var2 = _mm_load_si128((__m128i*)other._data);
#if 0
  __m128i result = _mm_cmpeq_epi64(var1, var2); // SSE4.1!
  result = _mm_and_si128(result, _mm_srli_si128(result, 8));
  return !!_mm_cvtsi128_si32(result);
#else
  // sse2 from 2.7.22.20
  __m128i vcmp = _mm_cmpeq_epi32(var1, var2);
  int packedmask = _mm_movemask_epi8(vcmp); // Create mask from the most significant bit of each 8-bit element in a, and store the result in dst.
  return (packedmask == 0x0000ffff);
#endif
}



bool	Interlocked::Data128::operator != (const Data128 & other) const
{
  __m128i var1 = _mm_load_si128((__m128i*)_data);
  __m128i var2 = _mm_load_si128((__m128i*)other._data);
#if 0
  __m128i result = _mm_cmpeq_epi64(var1, var2); // SSE4.1!
  result = _mm_and_si128(result, _mm_srli_si128(result, 8));
  return !_mm_cvtsi128_si32(result);
#else
  // sse2 from 2.7.22.20
  __m128i vcmp = _mm_cmpeq_epi32(var1, var2);
  int packedmask = _mm_movemask_epi8(vcmp); // Create mask from the most significant bit of each 8-bit element in a, and store the result in dst.
  return (packedmask != 0x0000ffff);
#endif
}



#endif	// conc_HAS_CAS_128



#pragma warning (push)
#pragma warning (4 : 4311 4312)

void *	Interlocked::swap (void * volatile &dest_ptr, void *excg_ptr)
{
	assert (&dest_ptr != 0);

	return (reinterpret_cast <void *> (
		swap (
			*reinterpret_cast <IntPtr volatile *> (&dest_ptr),
			reinterpret_cast <IntPtr> (excg_ptr)
		)
	));
}



void *	Interlocked::cas (void * volatile &dest_ptr, void *excg_ptr, void *comp_ptr)
{
	assert (&dest_ptr != 0);

	return (reinterpret_cast <void *> (
		cas (
			*reinterpret_cast <IntPtr volatile *> (&dest_ptr),
			reinterpret_cast <IntPtr> (excg_ptr),
			reinterpret_cast <IntPtr> (comp_ptr)
		)
	));
}

#pragma warning (pop)



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



}	// namespace conc



#endif	// conc_Interlocked_CODEHEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
