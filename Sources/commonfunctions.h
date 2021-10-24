#ifndef __COMMON_F__
#define __COMMON_F__

#include <stddef.h>
#include <stdint.h>

#define MV_UNUSED(x) (void)(x)

// returns a > 0 ? a : 0
inline static int satz(int a)
{
  return ~(a >> (sizeof(int)*8 - 1)) & a;
}

// returns maximum(a, b)
inline static int imax(int a, int b)
{
  return a + satz(b - a);
}

// returns minimum(a, b)
inline static int imin(int a, int b)
{
  return a - satz(a - b);
}

/* returns the biggest integer x such as 2^x <= i */
inline static int ilog2(int i)
{
  int result = 0;
  while ( i > 1 ) { i /= 2; result++; }
  return result;
}

/* computes 2^i */
inline static int iexp2(int i)
{
  return 1 << satz(i);
// 	int result = 1;
// 	while ( i > 0 ) { result *= 2; i--; }
// 	return result;
}

// general common divisor (from wikipedia)
inline static int64_t gcd(int64_t u, int64_t v)
 {
     int shift;

     /* GCD(0,x) := x */
     if (u == 0 || v == 0)
       return u | v;

     /* Let shift := lg K, where K is the greatest power of 2
        dividing both u and v. */
     for (shift = 0; ((u | v) & 1) == 0; ++shift) {
         u >>= 1;
         v >>= 1;
     }

     while ((u & 1) == 0)
       u >>= 1;

     /* From here on, u is always odd. */
     do {
         while ((v & 1) == 0)  /* Loop X */
           v >>= 1;

         /* Now u and v are both odd, so diff(u, v) is even.
            Let u = min(u, v), v = diff(u, v)/2. */
         if (u < v) {
             v -= u;
         } else {
             int64_t diff = u - v;
             u = v;
             v = diff;
         }
         v >>= 1;
     } while (v != 0);

     return u << shift;
}

// Least common multiple
inline static int64_t	lcm (int64_t u, int64_t v)
{
  const int64_t		prod = u * v;
  const int64_t		gcd_uv = gcd (u, v);
  const int64_t		lcm_uv = prod / gcd_uv;

  return (lcm_uv);
}

// Checks if a postive number is a power of two.
// Special case: returns true for 0.
template <class T>
bool	is_pow_2 (T x)
{
  return ((x & -x) == x);
}

// from avs+
// Functions and macros to help work with alignment requirements.

// Tells if a number is a power of two.
#define IS_POWER2(n) ((n) && !((n) & ((n) - 1)))

// Tells if the pointer "ptr" is aligned to "align" bytes.
#define IS_PTR_ALIGNED(ptr, align) (((uintptr_t)ptr & ((uintptr_t)(align-1))) == 0)

// Rounds up the number "n" to the next greater multiple of "align"
#define ALIGN_NUMBER(n, align) (((n) + (align)-1) & (~((align)-1)))

// Rounds up the pointer address "ptr" to the next greater multiple of "align"
#define ALIGN_POINTER(ptr, align) (((uintptr_t)(ptr) + (align)-1) & (~(uintptr_t)((align)-1)))

#ifdef __cplusplus

#include <cassert>
/*#include <cassert>
#include <cstdlib>
#include <cstdint>
#include <avs/config.h>
*/
#if defined(MSVC)
// needed for VS2013, otherwise C++11 'alignas' works
#define avs_alignas(x) __declspec(align(x))
#else
// assumes C++11 support
#define avs_alignas(x) alignas(x)
#endif

template<typename T>
static bool IsPtrAligned(T* ptr, size_t align)
{
  assert(IS_POWER2(align));
  return (bool)IS_PTR_ALIGNED(ptr, align);
}

template<typename T>
static T AlignNumber(T n, T align)
{
  assert(IS_POWER2(align));
  return ALIGN_NUMBER(n, align);
}

template<typename T>
static T* AlignPointer(T* ptr, size_t align)
{
  assert(IS_POWER2(align));
  return (T*)ALIGN_POINTER(ptr, align);
}

// The point of these undef's is to force using the template functions
// if we are in C++ mode. For C, the user can rely only on the macros.
#undef IS_PTR_ALIGNED
#undef ALIGN_NUMBER
#undef ALIGN_POINTER

#endif  // __cplusplus


#endif
