/*****************************************************************************

        ObjPool.h
        Author: Laurent de Soras, 2012

Thread-safe pool of recyclable objects.

The pool is constructed empty, new objects are created on request.

Don't forget to provide and link the pool to your object factory (so you can
control the instantiation details and provide additional parameters), see the
ObjFactoryInterface template class. Use ObjFactoryDef<T>::_fact if it doesn't
need anything more than the constructor without argument.

Template parameters:

- T: your class of stored objects. Requires:
	T::~T();

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if ! defined (conc_ObjPool_HEADER_INCLUDED)
#define	conc_ObjPool_HEADER_INCLUDED

#if defined (_MSC_VER)
	#pragma once
	#pragma warning (4 : 4250)
#endif



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#include "conc/CellPool.h"
#include "conc/LockFreeStack.h"
#include "conc/ObjFactoryInterface.h"
#include "fstb/SingleObj.h"



namespace conc
{



template <class T>
class ObjPool
{

/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

public:

	typedef	T	ObjType;
	typedef	ObjFactoryInterface <ObjType>	Factory;

	               ObjPool ();
	virtual        ~ObjPool ();

	void           set_factory (Factory &fact);
	Factory &      use_factory () const;
	void           cleanup ();

	T *            take_obj ();
	void           return_obj (T &obj);



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

protected:



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:

	typedef	CellPool <ObjType *>	PtrPool;
	typedef	typename PtrPool::CellType	PtrCell;
	typedef	LockFreeStack <ObjType *>	PtrStack;

	int            delete_obj_stack (PtrStack &ptr_stack, bool destroy_flag);

	Factory *      _factory_ptr = 0;    // 0 = not set
	PtrStack       _stack_free;
	PtrStack       _stack_all;
	fstb::SingleObj <PtrPool>
	               _obj_cell_pool_ptr;



/*\\\ FORBIDDEN MEMBER FUNCTIONS \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:

	               ObjPool (const ObjPool <T> &other)           = delete;
	ObjPool <T> &  operator = (const ObjPool <T> &other)        = delete;
	bool           operator == (const ObjPool <T> &other) const = delete;
	bool           operator != (const ObjPool <T> &other) const = delete;

};	// class ObjPool



}	// namespace conc



#include "conc/ObjPool.hpp"



#endif	// conc_ObjPool_HEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
