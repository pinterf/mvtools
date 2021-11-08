/*****************************************************************************

        avstp.h
        Author: Laurent de Soras, 2011

	Main avstp public interface.

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if ! defined (avstp_HEADER_INCLUDED)
#define	avstp_HEADER_INCLUDED

#if defined (_MSC_VER)
	#pragma once
	#pragma warning (4 : 4250)
#endif



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



#if defined (_WIN32) || defined (WIN32) || defined (__WIN32__) || defined (__CYGWIN__) || defined (__CYGWIN32__)
 #define avstp_CC __cdecl
 #define avstp_EXPORT(ret) __declspec(dllexport) ret avstp_CC

#else
 #define avstp_CC
 #if defined (__GNUC__) && __GNUC__ >= 4
  #define avstp_EXPORT(ret) __attribute__((visibility("default"))) ret avstp_CC
 #else
  #define avstp_EXPORT(ret) ret avstp_CC
 #endif

#endif



#ifdef __cplusplus
extern "C"
{
#endif



#ifdef __cplusplus
	namespace avstp { class TaskDispatcher; }
	typedef	avstp::TaskDispatcher	avstp_TaskDispatcher;
#else
	typedef	struct avstp_TaskDispatcher	avstp_TaskDispatcher;
#endif // __cplusplus

typedef	void (avstp_CC *avstp_TaskPtr) (avstp_TaskDispatcher *td_ptr, void *user_data_ptr);

enum
{
	avstp_Err_OK = 0,

	avstp_Err_EXCEPTION = -999,
	avstp_Err_INVALID_ARG
};

enum {	avstp_INTERFACE_VERSION = 1	};



avstp_EXPORT (int)   avstp_get_interface_version ();
avstp_EXPORT (avstp_TaskDispatcher *)  avstp_create_dispatcher ();
avstp_EXPORT (void)  avstp_destroy_dispatcher (avstp_TaskDispatcher *td_ptr);

avstp_EXPORT (int)   avstp_get_nbr_threads ();
avstp_EXPORT (int)   avstp_enqueue_task (avstp_TaskDispatcher *td_ptr, avstp_TaskPtr task_ptr, void *user_data_ptr);
avstp_EXPORT (int)   avstp_wait_completion (avstp_TaskDispatcher *td_ptr);



#ifdef __cplusplus
}
#endif



#endif	// avstp_HEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
