/*****************************************************************************

        SharedPtr.h
        Author: Laurent de Soras, 2011

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if ! defined (SharedPtr_HEADER_INCLUDED)
#define	SharedPtr_HEADER_INCLUDED

#if defined (_MSC_VER)
	#pragma once
	#pragma warning (4 : 4250)
#endif



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



template <class T>
class SharedPtr
{

/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

public:

	typedef	T	DataType;

						SharedPtr ();
						SharedPtr (const SharedPtr <T> &other);
	explicit			SharedPtr (T *ptr);
						~SharedPtr ();

	SharedPtr <T> &operator = (const SharedPtr <T> &other);
	void				swap (SharedPtr<T> &other);
	
	template <class U>
	void				assign_static (const SharedPtr <U> &other);
	template <class U>
	void				assign_dynamic (const SharedPtr <U> &other);
	template <class U>
	void				assign_reinterpret (const SharedPtr <U> &other);
	
						operator SharedPtr <const T> () const;

	T *				get () const;
	T *				operator -> () const;
	T &				operator * () const;
	void				destroy ();

	long				get_count () const;

	bool				is_valid () const;



/*\\\ INTERNAL \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

	// Do not use those functions (for implementation only)
						SharedPtr (T *other_ptr, long *count_ptr);
	long *			get_counter_ref () const;



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

protected:



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:

	bool				is_consistent (const SharedPtr <T> &other) const;

	void				add_ref ();
	void				destroy_complete ();

	DataType *		_obj_ptr;		// 0 if no associated object
	long *			_count_ptr;		// 0 if no associated object



/*\\\ FORBIDDEN MEMBER FUNCTIONS \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:

};	// class SharedPtr



template <class T, class U>
bool	operator == (const SharedPtr <T> &l_op, const SharedPtr <U> &r_op)
{
	return (l_op.get () == r_op.get ());
}

template <class T, class U>
bool	operator != (const SharedPtr <T> &l_op, const SharedPtr <U> &r_op)
{
	return (l_op.get () != r_op.get ());
}

template <class T, class U>
bool	operator < (const SharedPtr <T> &l_op, const SharedPtr <U> &r_op)
{
	return (l_op.get () < r_op.get ());
}



#include	"SharedPtr.hpp"



#endif	// SharedPtr_HEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
