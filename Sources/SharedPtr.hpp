/*****************************************************************************

        SharedPtr.hpp
        Author: Laurent de Soras, 2011

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if ! defined (SharedPtr_CODEHEADER_INCLUDED)
#define	SharedPtr_CODEHEADER_INCLUDED



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#include	<cassert>
#include <memory>



/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



template <class T>
SharedPtr <T>::SharedPtr ()
:	_obj_ptr (0)
,	_count_ptr (0)
{
  // Nothing
}



template <class T>
SharedPtr <T>::SharedPtr (T *ptr)
:	_obj_ptr (ptr)
,	_count_ptr ((ptr != 0) ? new long (1) : 0)
{
  // Nothing
}



template <class T>
SharedPtr <T>::SharedPtr (const SharedPtr <T> &other)
:	_obj_ptr (other._obj_ptr)
,	_count_ptr (other._count_ptr)
{
  assert (other.is_valid ());
  add_ref ();
  assert (is_consistent (other));
}



template <class T>
SharedPtr <T>::~SharedPtr ()
{
  assert (is_valid ());

  destroy_complete ();
}



template <class T>
SharedPtr <T> &	SharedPtr <T>::operator = (const SharedPtr <T> &other)
{
  assert (is_valid ());
  assert (is_consistent (other));

  if (other._obj_ptr != _obj_ptr)
  {
    destroy_complete ();
    _obj_ptr = other._obj_ptr;
    _count_ptr = other._count_ptr;
    add_ref ();
  }
  assert (is_consistent (other));

  return (*this);
}



template <class T>
void	SharedPtr <T>::swap (SharedPtr <T> &other)
{
  assert (&other != 0);

  DataType *		tmp_obj_ptr   = _obj_ptr;
  long *			tmp_count_ptr = _count_ptr;

  _obj_ptr = other._obj_ptr;
  _count_ptr = other._count_ptr;

  other._obj_ptr = tmp_obj_ptr;
  other._count_ptr = tmp_count_ptr;
}

// 20181019 problems with vs2017 15.8.7: problems with shared_*_cast
// These names are deprecated. Use *_pointer_cast instead.

template <class T>
template <class U>
void	SharedPtr <T>::assign_static (const SharedPtr <U> & other)
{
  assert (&other != 0);
  
  operator = (
    //shared_static_cast <T, U> (other)
    std::static_pointer_cast <T, U> (other)
    );
}



template <class T>
template <class U>
void	SharedPtr <T>::assign_dynamic (const SharedPtr <U> & other)
{
  assert (&other != 0);
  
  operator = (
    //shared_dynamic_cast <T, U> (other)
    std::dynamic_pointer_cast <T, U> (other)
  );
}



template <class T>
template <class U>
void	SharedPtr <T>::assign_reinterpret (const SharedPtr <U> & other)
{
  assert (&other != 0);
  
  operator = (
    //shared_reinterpret_cast <T, U> (other)
    std::reinterpret_pointer_cast <T, U> (other)
  );
}



template <class T>
SharedPtr <T>::operator SharedPtr <const T> () const 
{
  return (reinterpret_cast <const SharedPtr<const T> &> (*this));
}



template <class T>
T	*SharedPtr <T>::get () const
{
  return (_obj_ptr);
}



template <class T>
T	*SharedPtr <T>::operator -> () const
{
  assert (_obj_ptr != 0);
  
  return (_obj_ptr);
}



template <class T>
T	&SharedPtr <T>::operator * () const
{
  assert (_obj_ptr != 0);
  
  return (*_obj_ptr);
}



template <class T>
void	SharedPtr <T>::destroy ()
{
  assert (is_valid ());

  destroy_complete ();
  _obj_ptr = 0;
  _count_ptr = 0;
}



template <class T>
long	SharedPtr <T>::get_count () const
{
  assert (is_valid ());

  long				count = 0;
  if (_count_ptr != 0)
  {
    count = *_count_ptr;
    assert (count > 0);
  }

  return (count);
}



template <class T>
bool	SharedPtr <T>::is_valid () const
{
  return (   (   (_obj_ptr == 0 && _count_ptr == 0)
              || (_obj_ptr != 0 && _count_ptr != 0))
          && (_count_ptr == 0 || (*_count_ptr) > 0));
}



/*\\\ INTERNAL \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



template <class T>
SharedPtr <T>::SharedPtr (T *other_ptr, long *count_ptr)
:	_obj_ptr (other_ptr)
,	_count_ptr (count_ptr)
{
  assert (is_valid ());
  add_ref ();
}



template <class T>
long *	SharedPtr <T>::get_counter_ref () const
{
  return (_count_ptr);
}



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



template <class T>
void	SharedPtr <T>::add_ref ()
{
  if (_count_ptr != 0)
  {
    ++ *_count_ptr;
  }
}



template <class T>
bool	SharedPtr <T>::is_consistent (const SharedPtr <T> &other) const
{
  assert (&other != 0);

  return (   other.is_valid ()
          && (   (_obj_ptr == other._obj_ptr) && (_count_ptr == other._count_ptr)
              || (_obj_ptr != other._obj_ptr) && (_count_ptr != other._count_ptr)));
}



template <class T>
void	SharedPtr <T>::destroy_complete ()
{
  assert (is_valid ());

  if (_obj_ptr != 0)
  {
    -- *_count_ptr;
    
    if (*_count_ptr == 0)
    {
      delete _obj_ptr;
      _obj_ptr = 0;

      delete _count_ptr;
      _count_ptr = 0;
    }
  }
}



#endif	// SharedPtr_CODEHEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
