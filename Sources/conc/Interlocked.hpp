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
#include "include/avs/config.h"
#include	"conc/def.h"
#include	"conc/fnc.h"
#include	"conc/Interlocked.h"
#include	<xmmintrin.h>
#include <stdint.h>

#if 0
info on gcc builtins
http://www.rowleydownload.co.uk/arm/documentation/gnu/gcc/_005f_005fsync-Builtins.html

// another set of replacement functions for gcc/clang
// see apple code also https://github.com/bittorrent/libbtutils/blob/master/src/interlock.h

inline LONG InterlockedAdd(LONG* ptr, LONG value) {
  return __sync_add_and_fetch(ptr, value);
}

inline long long int InterlockedAdd64(long long int* ptr, long long int value) {
  return __sync_add_and_fetch(ptr, value);
}

inline LONG InterlockedIncrement(LONG* ptr) {
  return __sync_add_and_fetch(ptr, 1);
}

inline long long int InterlockedIncrement64(long long int* ptr) {
  return __sync_add_and_fetch(ptr, 1);
}

inline LONG InterlockedDecrement(LONG* ptr) {
  return __sync_sub_and_fetch(ptr, 1);
}

inline long long int InterlockedDecrement64(long long int* ptr) {
  return __sync_sub_and_fetch(ptr, 1);
}

inline LONG InterlockedExchange(LONG* ptr, LONG value) {
  return __sync_lock_test_and_set(ptr, value);
}

inline LONG InterlockedExchange64(long long int* ptr, long long int value) {
  return __sync_lock_test_and_set(ptr, value);
}

#ifdef _LP64
#define InterlockedAddPointer InterlockedAdd64
#else
#define InterlockedAddPointer InterlockedAdd
#endif // _LP64

inline void* InterlockedExchangePointer(void** ptr, void* value) {
  return __sync_lock_test_and_set(ptr, value);
}

inline void* InterlockedCompareExchangePointer(void** ptr, void* newvalue, void* oldvalue) {
  return __sync_val_compare_and_swap(ptr, oldvalue, newvalue);
}
#endif // if #0

#if defined(_WIN32) && !defined(__clang__) && !defined(__GNUC__)
#include <intrin.h>
inline static int32_t interlockedIncrement(volatile int32_t* a) { return _InterlockedIncrement((long*)a); }
inline static int32_t interlockedDecrement(volatile int32_t* a) { return _InterlockedDecrement((long*)a); }
inline static int32_t interlockedCompareExchange(volatile int32_t* a, int32_t b, int32_t c) { return _InterlockedCompareExchange((long*)a, (long)b, (long)c); }
#if defined(_M_X64)
inline static int64_t interlockedIncrement64(volatile int64_t* a) { return _InterlockedIncrement64(a); }
inline static int64_t interlockedDecrement64(volatile int64_t* a) { return _InterlockedDecrement64(a); }
inline static int64_t interlockedExchangeAdd64(volatile int64_t* a, int64_t b) { return _InterlockedExchangeAdd64(a, b); }
inline static int64_t interlockedExchange64(volatile int64_t* a, int64_t b) { return _InterlockedExchange64(a, b); }
inline static int64_t interlockedOr64(volatile int64_t* a, int64_t b) { return _InterlockedOr64(a, b); }
#endif
#elif defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_8)
#include <xmmintrin.h>
inline static int32_t interlockedIncrement(volatile int32_t* a) { return __sync_add_and_fetch(a, 1); }
inline static int64_t interlockedIncrement64(volatile int64_t* a) { return __sync_add_and_fetch(a, 1); }
inline static int32_t interlockedDecrement(volatile int32_t* a) { return __sync_add_and_fetch(a, -1); }
inline static int64_t interlockedDecrement64(volatile int64_t* a) { return __sync_add_and_fetch(a, -1); }
inline static int32_t interlockedCompareExchange(volatile int32_t* a, int32_t b, int32_t c) { return __sync_val_compare_and_swap(a, c, b); }
inline static int32_t interlockedCompareExchange64(volatile int64_t* a, int64_t b, int64_t c) { return __sync_val_compare_and_swap(a, c, b); }
inline static int64_t interlockedExchange(volatile int32_t* a, int32_t b) {
  // PF: why should we have full memory barrier here using synchronize, and in other implementations why not?
  __sync_synchronize();
  return __sync_lock_test_and_set(a, b);
}
inline static int64_t interlockedExchangeAdd64(volatile int64_t* a, int64_t b) { return __sync_fetch_and_add(a, b); }
inline static int64_t interlockedExchange64(volatile int64_t* a, int64_t b) {
  // PF: why should we have full memory barrier here using synchronize, and in other implementations why not?
  __sync_synchronize();
  return __sync_lock_test_and_set(a, b);
}
inline static int64_t interlockedOr64(volatile int64_t* a, int64_t b) { return __sync_fetch_and_or(a, b); }
#else
#error No implementation of atomic instructions
#endif

#include	<cassert>



namespace conc
{



/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



int32_t	Interlocked::swap (int32_t volatile &dest, int32_t excg)
{
	assert (is_ptr_aligned_nz (&dest));

#if defined(GCC)
  return (
    interlockedExchange(reinterpret_cast <volatile int32_t*> (&dest), excg)
    );
#else
  return (
		_InterlockedExchange (reinterpret_cast <volatile long *> (&dest), excg)
	);
#endif
}



int32_t	Interlocked::cas (int32_t volatile &dest, int32_t excg, int32_t comp)
{
	assert (is_ptr_aligned_nz (&dest));

#if defined(GCC)
  return (interlockedCompareExchange(
    reinterpret_cast <volatile int32_t*> (&dest),
    excg,
    comp
  ));
#else
  return (_InterlockedCompareExchange (
		reinterpret_cast <volatile long *> (&dest),
		excg,
		comp
	));
#endif
}



int64_t	Interlocked::swap (int64_t volatile &dest, int64_t excg)
{
	assert (is_ptr_aligned_nz (&dest));

	int64_t			old;

#if conc_WORD_SIZE == 64

#if defined(GCC)
  old = interlockedExchange64(&dest, excg);
#else
	old = _InterlockedExchange64 (&dest, excg);
#endif

#else	// conc_WORD_SIZE

	__asm
	{
		push				ebx
		mov				esi, [dest]
		mov				ebx, dword ptr[excg    ]
		mov				ecx, dword ptr[excg + 4]

	cas_loop:
		mov				eax, [esi    ]
		mov				edx, [esi + 4]
		lock cmpxchg8b	[esi]
		jnz				cas_loop

		mov				dword ptr[old    ], eax
		mov				dword ptr[old + 4], edx
		pop				ebx
	}
  
#endif	// conc_WORD_SIZE

	return (old);
}



int64_t	Interlocked::cas (int64_t volatile &dest, int64_t excg, int64_t comp)
{
	assert (is_ptr_aligned_nz (&dest));

#if defined(GCC)
  return (interlockedCompareExchange64 (&dest, excg, comp));
#else
  return (_InterlockedCompareExchange64(&dest, excg, comp));
#endif
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

// clang-cl LLVM with VS2019 15.6.3: 
// 1.) _InterlockedCompareExchange128' needs target feature cx16
// 2.) Unresolved external for clang + x64: __sync_val_compare_and_swap_16, unless the whole project 
//     in general is flagged as cx16 (along with sse4.1) -msse4.1 -mcx16
#if defined(GCC) || defined(CLANG)
__attribute__((__target__("cx16")))
#endif 
void	Interlocked::cas (Data128 &old, volatile Data128 &dest, const Data128 &excg, const Data128 &comp)
{
	assert (&old != 0);
	assert (is_ptr_aligned_nz (&dest));

  /*
_InterlockedCompareExchange128 intrinsic functions
Performs a 128-bit interlocked compare and exchange.

Parameters

Destination
[in, out] Pointer to the destination, which is an array of two 64-bit integers considered as a 128-bit field. The destination data must be 16-byte aligned to avoid a general protection fault.

ExchangeHigh
[in] A 64-bit integer that may be exchanged with the high part of the destination.

ExchangeLow
[in] A 64-bit integer that may be exchanged with the low part of the destination.

ComparandResult
[in, out] Pointer to an array of two 64-bit integers (considered as a 128-bit field) to compare with the destination. On output, this array is overwritten with the original value of the destination.
The value of ComparandResult is always overwritten.
After the lock instruction, this intrinsic immediately copies the initial value of Destination to ComparandResult.
For this reason, ComparandResult and Destination should point to separate memory locations to avoid unexpected behavior.

Remarks
Header file <intrin.h>
The _InterlockedCompareExchange128 intrinsic generates the 
cmpxchg16b instruction (with the lock prefix) to perform a 
128-bit locked compare and exchange. Early versions of AMD 64-bit 
hardware don't support this instruction. 
To check for hardware support for the cmpxchg16b instruction, 
call the __cpuid intrinsic with InfoType=0x00000001 
(standard function 1). 
Bit 13 of CPUInfo[2] (ECX) is 1 if the instruction is supported.
*/

		const int64_t	excg_lo = ((const int64_t *) &excg) [0];
		const int64_t	excg_hi = ((const int64_t *) &excg) [1];

		old = comp;

#if defined(GCC)
    __sync_val_compare_and_swap(reinterpret_cast <volatile __int128*>(&dest), *(reinterpret_cast < __int128*>(&old)), *(reinterpret_cast <const __int128*>(&excg)));
    //AtomicCAS128(reinterpret_cast <volatile __int128*>(&dest), )
    /*
    // dest = dest, expected = ComparandResult, newval = Exchange
    gcc_InterlockedCompareExchange128(volatile __int128 *dest, __int128 Exchange, __int128 *ComparandResult)
    {
      bool ret;
      __int128 current;
      current = __sync_val_compare_and_swap(&dest, *ComparandResult, Exchange);
      ret = current == *ComparandResult;
      *ComparandResult = current;
      return ret;
    }
    */
#elif ((_MSC_VER / 100) >= 15)	|| defined(CLANG)// VC2008 or above

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
