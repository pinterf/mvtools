/*****************************************************************************

        AvstpWrapper.cpp
        Author: Laurent de Soras, 2012

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if defined (_MSC_VER)
	#pragma warning (1 : 4130 4223 4705 4706)
	#pragma warning (4 : 4355 4786 4800)
#endif



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#define	NOGDI
#define	NOMINMAX
#define	WIN32_LEAN_AND_MEAN

#include	"conc/CritSec.h"
#include	"conc/Mutex.h"
#include	"AvstpFinder.h"
#include	"AvstpWrapper.h"

#include	"Windows.h"

#include	<stdexcept>

#include	<cassert>



/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



/*
==============================================================================
Name: dtor
	Please do not destroy directly the object. This will be done automatically
	at the end of the process.
==============================================================================
*/

AvstpWrapper::~AvstpWrapper ()
{
	::FreeLibrary (reinterpret_cast < ::HMODULE> (_dll_hnd));
	_dll_hnd = 0;
}



/*
==============================================================================
Name: use_instance
Description:
	Obtain an access to the AvstpWrapper singleton. It is created if not
	accessed previously.
Returns:
	A reference on the wrapper.
Throws:
	The first call may throw something related to the object creation failure.
	It may be caused by an unsupported library version, a loading failure or
	something like that.
==============================================================================
*/

AvstpWrapper &	AvstpWrapper::use_instance ()
{
	// First check
	if (! _singleton_init_flag)
	{
		// Ensure serialization (guard constructor acquires mutex_new).
		static conc::Mutex	mutex_new;
		conc::CritSec	guard (mutex_new);

		// Double check.
		if (! _singleton_init_flag)
		{
			assert (! _singleton_init_flag && _singleton_aptr.get () == 0);
			_singleton_aptr = std::unique_ptr <AvstpWrapper> (new AvstpWrapper);
			_singleton_init_flag = true;
		}

		// guard destructor releases mutex_new.
	}

	return (*_singleton_aptr);
}



// Below are the AVSTP wrapped function.
// See the documentation for more details.



int	AvstpWrapper::get_interface_version () const
{
	return (_avstp_get_interface_version_ptr ());
}



avstp_TaskDispatcher *	AvstpWrapper::create_dispatcher ()
{
	return (_avstp_create_dispatcher_ptr ());
}



void	AvstpWrapper::destroy_dispatcher (avstp_TaskDispatcher *td_ptr)
{
	_avstp_destroy_dispatcher_ptr (td_ptr);
}



int	AvstpWrapper::get_nbr_threads () const
{
	return (_avstp_get_nbr_threads_ptr ());
}



int	AvstpWrapper::enqueue_task (avstp_TaskDispatcher *td_ptr, avstp_TaskPtr task_ptr, void *user_data_ptr)
{
	return (_avstp_enqueue_task_ptr (td_ptr, task_ptr, user_data_ptr));
}



int	AvstpWrapper::wait_completion (avstp_TaskDispatcher *td_ptr)
{
	return (_avstp_wait_completion_ptr (td_ptr));
}



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



AvstpWrapper::AvstpWrapper ()
:	_dll_hnd (AvstpFinder::find_lib ())
,	_avstp_get_interface_version_ptr (0)
,	_avstp_create_dispatcher_ptr (0)
,	_avstp_destroy_dispatcher_ptr (0)
,	_avstp_get_nbr_threads_ptr (0)
,	_avstp_enqueue_task_ptr (0)
,	_avstp_wait_completion_ptr (0)
{
	if (_dll_hnd == 0)
	{
		::OutputDebugStringW (
			L"AvstpWrapper: cannot find avstp.dll."
			L"Usage restricted to single threading.\n"
		);
//		throw std::runtime_error ("Cannot find avstp.dll.");
		assign_fallback ();
	}

	else
	{
		// Now resolves the function names
		assign_normal ();
	}
}



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



template <class T>
void	AvstpWrapper::resolve_name (T &fnc_ptr, const char *name_0)
{
	assert (&fnc_ptr != 0);
	assert (name_0 != 0);
	assert (_dll_hnd != 0);

	fnc_ptr = reinterpret_cast <T> (
		::GetProcAddress (reinterpret_cast < ::HMODULE> (_dll_hnd), name_0)
	);
	if (fnc_ptr == 0)
	{
		::FreeLibrary (reinterpret_cast < ::HMODULE> (_dll_hnd));
		_dll_hnd = 0;
		throw std::runtime_error ("Function missing in avstp.dll.");
	}
}



void	AvstpWrapper::assign_normal ()
{
	resolve_name (_avstp_get_interface_version_ptr, "avstp_get_interface_version");
	resolve_name (_avstp_create_dispatcher_ptr,     "avstp_create_dispatcher");
	resolve_name (_avstp_destroy_dispatcher_ptr,    "avstp_destroy_dispatcher");
	resolve_name (_avstp_get_nbr_threads_ptr,       "avstp_get_nbr_threads");
	resolve_name (_avstp_enqueue_task_ptr,          "avstp_enqueue_task");
	resolve_name (_avstp_wait_completion_ptr,       "avstp_wait_completion");
}



void	AvstpWrapper::assign_fallback ()
{
	_avstp_get_interface_version_ptr = &fallback_get_interface_version_ptr;
	_avstp_create_dispatcher_ptr     = &fallback_create_dispatcher_ptr;
	_avstp_destroy_dispatcher_ptr    = &fallback_destroy_dispatcher_ptr;
	_avstp_get_nbr_threads_ptr       = &fallback_get_nbr_threads_ptr;
	_avstp_enqueue_task_ptr          = &fallback_enqueue_task_ptr;
	_avstp_wait_completion_ptr       = &fallback_wait_completion_ptr;
}



int	AvstpWrapper::fallback_get_interface_version_ptr ()
{
	return (avstp_INTERFACE_VERSION);
}



avstp_TaskDispatcher *	AvstpWrapper::fallback_create_dispatcher_ptr ()
{
	return ((avstp_TaskDispatcher *) (&_dummy_dispatcher));
}



void	AvstpWrapper::fallback_destroy_dispatcher_ptr (avstp_TaskDispatcher *td_ptr)
{
	assert (td_ptr == (avstp_TaskDispatcher *) (&_dummy_dispatcher));
}



int	AvstpWrapper::fallback_get_nbr_threads_ptr ()
{
	return (1);
}



int	AvstpWrapper::fallback_enqueue_task_ptr (avstp_TaskDispatcher *td_ptr, avstp_TaskPtr task_ptr, void *user_data_ptr)
{
	int				ret_val = avstp_Err_OK;

	if (   td_ptr != (avstp_TaskDispatcher *) (&_dummy_dispatcher)
	    || task_ptr == 0)
	{
		ret_val = avstp_Err_INVALID_ARG;
	}
	else
	{
		task_ptr (td_ptr, user_data_ptr);
	}

	return (ret_val);
}



int	AvstpWrapper::fallback_wait_completion_ptr (avstp_TaskDispatcher *td_ptr)
{
	int				ret_val = avstp_Err_OK;

	if (td_ptr != (avstp_TaskDispatcher *) (&_dummy_dispatcher))
	{
		ret_val = avstp_Err_INVALID_ARG;
	}

	return (ret_val);
}



std::unique_ptr <AvstpWrapper>	AvstpWrapper::_singleton_aptr;
volatile bool	AvstpWrapper::_singleton_init_flag = false;

int	AvstpWrapper::_dummy_dispatcher;



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
