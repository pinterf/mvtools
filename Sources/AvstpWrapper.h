/*****************************************************************************

        AvstpWrapper.h
        Author: Laurent de Soras, 2012

A convenient wrapper on top of the AVSTP low-level API.
Take care of:
- Library discovery and initialisation
- Fallback to mono-threaded mode if not found

This is a singleton, you cannot construct it directly. Use use_instance()
to access it from anywhere.

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if ! defined (AvstpWrapper_HEADER_INCLUDED)
#define	AvstpWrapper_HEADER_INCLUDED

#if defined (_MSC_VER)
	#pragma once
	#pragma warning (4 : 4250)
#endif



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#include	"avstp.h"

#include	<memory>



class AvstpWrapper
{

/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

public:

	virtual			~AvstpWrapper ();

	static AvstpWrapper &
						use_instance ();

	// Wrapped functions
	int				get_interface_version () const;
	avstp_TaskDispatcher *
						create_dispatcher ();
	void				destroy_dispatcher (avstp_TaskDispatcher *td_ptr);
	int				get_nbr_threads () const;
	int				enqueue_task (avstp_TaskDispatcher *td_ptr, avstp_TaskPtr task_ptr, void *user_data_ptr);
	int				wait_completion (avstp_TaskDispatcher *td_ptr);



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

protected:

						AvstpWrapper ();



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:

	template <class T>
	void				resolve_name (T &fnc_ptr, const char *name_0);

	void				assign_normal ();
	void				assign_fallback ();

	static int		fallback_get_interface_version_ptr ();
	static avstp_TaskDispatcher *
						fallback_create_dispatcher_ptr ();
	static void		fallback_destroy_dispatcher_ptr (avstp_TaskDispatcher *td_ptr);
	static int		fallback_get_nbr_threads_ptr ();
	static int		fallback_enqueue_task_ptr (avstp_TaskDispatcher *td_ptr, avstp_TaskPtr task_ptr, void *user_data_ptr);
	static int		fallback_wait_completion_ptr (avstp_TaskDispatcher *td_ptr);

	int				(*_avstp_get_interface_version_ptr) ();
	avstp_TaskDispatcher *
						(*_avstp_create_dispatcher_ptr) ();
	void				(*_avstp_destroy_dispatcher_ptr) (avstp_TaskDispatcher *td_ptr);
	int				(*_avstp_get_nbr_threads_ptr) ();
	int				(*_avstp_enqueue_task_ptr) (avstp_TaskDispatcher *td_ptr, avstp_TaskPtr task_ptr, void *user_data_ptr);
	int				(*_avstp_wait_completion_ptr) (avstp_TaskDispatcher *td_ptr);

	void *			_dll_hnd;	// Avoids loading windows.h just for HMODULE

	static std::auto_ptr <AvstpWrapper>
                  _singleton_aptr;
   static volatile bool
						_singleton_init_flag;

	static int		_dummy_dispatcher;



/*\\\ FORBIDDEN MEMBER FUNCTIONS \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:

						AvstpWrapper (const AvstpWrapper &other);
	AvstpWrapper &	operator = (const AvstpWrapper &other);
	bool				operator == (const AvstpWrapper &other) const;
	bool				operator != (const AvstpWrapper &other) const;

};	// class AvstpWrapper



//#include	"AvstpWrapper.hpp"



#endif	// AvstpWrapper_HEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
