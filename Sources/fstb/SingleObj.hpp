/*****************************************************************************

        SingleObj.hpp
        Author: Laurent de Soras, 2015

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if ! defined (fstb_SingleObj_CODEHEADER_INCLUDED)
#define	fstb_SingleObj_CODEHEADER_INCLUDED



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#include <cassert>



namespace fstb
{



/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



template <class T, class A>
SingleObj <T, A>::SingleObj ()
:	_allo ()
,	_obj_ptr (_allo.allocate (1))
{
	if (_obj_ptr == nullptr)
	{
#if defined (__cpp_exceptions) || ! defined (__GNUC__)
		throw std::bad_alloc ();
#endif
	}

#if defined (__cpp_exceptions) || ! defined (__GNUC__)
	try
#endif
	{
		new (_obj_ptr) T ();
	}
#if defined (__cpp_exceptions) || ! defined (__GNUC__)
	catch (...)
	{
		_allo.deallocate (_obj_ptr, 1);
		throw;
	}
#endif
}



template <class T, class A>
SingleObj <T, A>::~SingleObj ()
{
	_obj_ptr->~T ();
	_allo.deallocate (_obj_ptr, 1);
	_obj_ptr = nullptr;
}



template <class T, class A>
T *	SingleObj <T, A>::operator -> () const
{
	assert (_obj_ptr != nullptr);

	return _obj_ptr;
}



template <class T, class A>
T &	SingleObj <T, A>::operator * () const
{
	assert (_obj_ptr != 0);

	return *_obj_ptr;
}



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



}	// namespace fstb



#endif	// fstb_SingleObj_CODEHEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
