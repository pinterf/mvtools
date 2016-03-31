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

typedef	void (__cdecl *avstp_TaskPtr) (avstp_TaskDispatcher *td_ptr, void *user_data_ptr);

enum
{
	avstp_Err_OK = 0,

	avstp_Err_EXCEPTION = -999,
	avstp_Err_INVALID_ARG
};

enum {	avstp_INTERFACE_VERSION = 1	};



__declspec (dllexport) int __cdecl	avstp_get_interface_version ();
__declspec (dllexport) avstp_TaskDispatcher * __cdecl	avstp_create_dispatcher ();
__declspec (dllexport) void __cdecl	avstp_destroy_dispatcher (avstp_TaskDispatcher *td_ptr);

__declspec (dllexport) int __cdecl	avstp_get_nbr_threads ();
__declspec (dllexport) int __cdecl	avstp_enqueue_task (avstp_TaskDispatcher *td_ptr, avstp_TaskPtr task_ptr, void *user_data_ptr);
__declspec (dllexport) int __cdecl	avstp_wait_completion (avstp_TaskDispatcher *td_ptr);



#ifdef __cplusplus
}
#endif



#endif	// avstp_HEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
