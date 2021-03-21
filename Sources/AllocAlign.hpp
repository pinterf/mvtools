/*****************************************************************************

        AllocAlign.hpp
        Author: Laurent de Soras, 2011

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if ! defined (AllocAlign_CODEHEADER_INCLUDED)
#define	AllocAlign_CODEHEADER_INCLUDED



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#include	"def.h"

#include	<cassert>



/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/


template <class T, int N>
class aligned_allocator
{

public:

  typedef T value_type;
  typedef T& reference;
  typedef const T& const_reference;
  typedef T* pointer;
  typedef const T* const_pointer;
  typedef size_t size_type;
  typedef ptrdiff_t difference_type;

  template <class U>
  struct rebind
  {
    typedef aligned_allocator<U, N> other;
  };

  inline aligned_allocator() throw() {}
  inline aligned_allocator(const aligned_allocator&) throw() {}

  template <class U>
  inline aligned_allocator(const aligned_allocator<U, N>&) throw() {}

  inline ~aligned_allocator() throw() {}

  inline pointer address(reference r) { return &r; }
  inline const_pointer address(const_reference r) const { return &r; }

  pointer allocate(size_type n, typename std::allocator_traits <std::allocator<void>>::const_pointer hint = 0);
  inline void deallocate(pointer p, size_type);

  inline void construct(pointer p, const_reference value) { new (p) value_type(value); }
  inline void destroy(pointer p) { p->~value_type(); }

  inline size_type max_size() const throw() { return size_type(-1) / sizeof(T); }

  inline bool operator==(const aligned_allocator&) { return true; }
  inline bool operator!=(const aligned_allocator& rhs) { return !operator==(rhs); }
};

template <class T, int N>
typename aligned_allocator<T, N>::pointer aligned_allocator<T, N>::allocate(size_type n, typename std::allocator_traits<std::allocator<void>>::const_pointer hint)
{
  pointer res = reinterpret_cast<pointer>(_aligned_malloc(sizeof(T) * n, N));
  if (res == 0)
    throw std::bad_alloc();
  return res;
}

template <class T, int N>
void aligned_allocator<T, N>::deallocate(pointer p, size_type)
{
  _aligned_free(p);
}
#endif	// AllocAlign_CODEHEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
