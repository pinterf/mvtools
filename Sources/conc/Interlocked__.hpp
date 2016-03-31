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



#if ! defined (conc_Interlocked_CODEHEADER_INCLUDED)
#define	conc_Interlocked_CODEHEADER_INCLUDED

#if defined(_M_IX86)
#define rax	eax
#define rbx	ebx
#define rcx	ecx
#define rdx	edx
#define rsi	esi
#define rdi	edi
#define rbp	ebp
#define rsp	esp
#define movzx_int mov
#define movsx_int mov
#else
#define rax	rax
#define rbx	rbx
#define rcx	rcx
#define rdx	rdx
#define rsi	rsi
#define rdi	rdi
#define rbp	rbp
#define rsp	r15
#define movzx_int movzxd
#define movsx_int movsxd
#endif



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#include	"conc/fnc.h"

#include	<intrin.h>

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
//		push				ebx
		mov				rsi, [dest]
		mov				ebx, [dword ptr excg    ]
		mov				ecx, [dword ptr excg + 4]

	cas_loop:
		mov				eax, [rsi    ]
		mov				edx, [rsi + 4]
		lock cmpxchg8b	[rsi]
		jnz				cas_loop

		mov				[dword ptr old    ], eax
		mov				[dword ptr old + 4], edx
//		pop				ebx
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



void	Interlocked::swap (Data128 &old, volatile Data128 &dest, const Data128 &excg, const Data128 &comp)
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
