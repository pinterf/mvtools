/*****************************************************************************

        ObjPool.hpp
        Author: Laurent de Soras, 2012

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if ! defined (conc_ObjPool_CODEHEADER_INCLUDED)
#define	conc_ObjPool_CODEHEADER_INCLUDED



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#include <cassert>

#include <stdexcept>



namespace conc
{



/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



/*
==============================================================================
Name: ctor
Description:
	Create the pool, empty of any object.
Throws: Most likely memory allocation exceptions.
==============================================================================
*/

template <class T>
ObjPool <T>::ObjPool ()
:	_factory_ptr (0)
,	_stack_free ()
,	_stack_all ()
,	_obj_cell_pool_ptr ()
{
	_obj_cell_pool_ptr->expand_to (1024);
}



/*
==============================================================================
Name: dtor
Throws: Nothing (I hope so).
==============================================================================
*/

template <class T>
ObjPool <T>::~ObjPool ()
{
	cleanup ();
}



/*
==============================================================================
Name: set_factory
Description:
	Link the pool to the object factory. This function must be called at least
	once before any call to take_obj().
Input parameters:
	- fact: Reference on the factory.
Throws: Nothing
==============================================================================
*/

template <class T>
void	ObjPool <T>::set_factory (Factory &fact)
{
	_factory_ptr = &fact;
}



template <class T>
typename ObjPool <T>::Factory &	ObjPool <T>::use_factory () const
{
	assert (_factory_ptr != 0);

	return (*_factory_ptr);
}



/*
==============================================================================
Name: take_obj
Description:
	Borrow an object from the pool, or try to construct a new one if none are
	available. Objects must be returned to the pool with return_obj(), under
	the penalty of resource leaks.
Returns:
	A pointer on the object, or 0 if no object is available and cannot be
	created for any reason.
Throws: Nothing
==============================================================================
*/

template <class T>
T *	ObjPool <T>::take_obj ()
{
	assert (_factory_ptr != 0);

	ObjType *      obj_ptr = 0;

	PtrCell *      cell_ptr = _stack_free.pop ();
	if (cell_ptr == 0)
	{
		obj_ptr = _factory_ptr->create ();

		if (obj_ptr != 0)
		{
			bool        ok_flag = false;
			try
			{
				cell_ptr = _obj_cell_pool_ptr->take_cell (true);
				if (cell_ptr != 0)
				{
					cell_ptr->_val = obj_ptr;
					_stack_all.push (*cell_ptr);
					ok_flag = true;
				}
			}
			catch (...)
			{
				// Nothing
			}

			if (! ok_flag)
			{
				delete obj_ptr;
				obj_ptr = 0;
			}
		}
	}
	else
	{
		obj_ptr = cell_ptr->_val;
		_obj_cell_pool_ptr->return_cell (*cell_ptr);
	}

	return (obj_ptr);
}



/*
==============================================================================
Name: return_obj
Description:
	Return to the pool an object previously borrowed with take_obj().
	These details are not checked but are very important:
	- Do not return twice the same object
	- Do not return an object you didn't get from take_obj()
Input parameters:
	- obj: Reference on the returned object.
Throws: Nothing
==============================================================================
*/

template <class T>
void	ObjPool <T>::return_obj (T &obj)
{
	PtrCell *      cell_ptr = 0;
	try
	{
		cell_ptr = _obj_cell_pool_ptr->take_cell (true);
	}
	catch (...)
	{
		// Nothing
	}

	if (cell_ptr == 0)
	{
		throw std::runtime_error ("return_obj(): cannot allocate a new cell.");
	}
	else
	{
		cell_ptr->_val = &obj;
		_stack_free.push (*cell_ptr);
	}
}



/*
==============================================================================
Name: cleanup
Description:
	Preliminary deletion of the pool content, also used during the pool
	destruction.
	Do not call it if some objects are still out of the pool!
	Use with care.
Throws: Nothing
==============================================================================
*/

template <class T>
void	ObjPool <T>::cleanup ()
{
#if ! defined (NDEBUG)
	const int      count_free =
#endif
		delete_obj_stack  (_stack_free, false);
#if ! defined (NDEBUG)
	const int      count_all  =
#endif
		delete_obj_stack  (_stack_all,  true);

	// False would mean that some cells are still out, in use.
	assert (count_free == count_all);
}



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



template <class T>
int	ObjPool <T>::delete_obj_stack (PtrStack &ptr_stack, bool destroy_flag)
{
	typename PtrStack::CellType *   cell_ptr = 0;
	int            count = 0;
	do
	{
		cell_ptr = ptr_stack.pop ();
		if (cell_ptr != 0)
		{
			if (destroy_flag)
			{
				ObjType * &		obj_ptr = cell_ptr->_val;
				delete obj_ptr;
				obj_ptr = 0;
			}

			_obj_cell_pool_ptr->return_cell (*cell_ptr);
			++ count;
		}
	}
	while (cell_ptr != 0);

	return (count);
}



}	// namespace conc



#endif	// conc_ObjPool_CODEHEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
