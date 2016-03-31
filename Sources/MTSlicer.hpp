/*****************************************************************************

        MTSlicer.hpp
        Author: Laurent de Soras, 2012

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if ! defined (MTSlicer_CODEHEADER_INCLUDED)
#define	MTSlicer_CODEHEADER_INCLUDED



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#include	"AvstpWrapper.h"

#include	<cassert>



// Internal utility functions

template <class T, class GD>
class MTSlicer_Access
{
public:
	static inline T *	access (GD *ptr) { return (ptr->_this_ptr); }
};

template <class T>
class MTSlicer_Access <T, T>
{
public:
	static inline T *	access (T *ptr) { return (ptr); }
};



/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



/*
==============================================================================
Name: constructor
Input parameters:
	- mt_flag: set it to false to disable multi-threading. If set to true, the
		actual number of threads used will depend on the AVSTP settings.
Throws: an exception if AvstpWrapper cannot be accessed or constructed.
==============================================================================
*/

template <class T, class GD, int MAXT>
MTSlicer <T, GD, MAXT>::MTSlicer (bool mt_flag)
:	_avstp (AvstpWrapper::use_instance ())
,	_proc_ptr (0)
,	_dispatcher_ptr (0)
,	_task_data_arr ()
,	_mt_flag (mt_flag)
{
	// Nothing
}



/*
==============================================================================
Name: dtor
Description:
	Before destruction, waits for tasks still running to be completed.
==============================================================================
*/

template <class T, class GD, int MAXT>
MTSlicer <T, GD, MAXT>::~MTSlicer ()
{
	if (_dispatcher_ptr != 0)
	{
		wait ();
	}
}



/*
==============================================================================
Name: is_mt
Description:
	Indicates if the object has been constructed with multi-threading ability.
Returns: true if multi-threading is enabled.
Throws: Nothing
==============================================================================
*/

template <class T, class GD, int MAXT>
bool	MTSlicer <T, GD, MAXT>::is_mt () const
{
	return (_mt_flag);
}



/*
==============================================================================
Name: start
Description:
	Run a task by slicing the input data in multiple subsets and processing
	them in parallel. The slicing depends on the number of active threads.
	In monothreading, the whole set is processed at once.
	The function returns as soon as the tasks are enqueued. Use wait() to wait
	for their completion. In monothreaded mode, processing is performed before
	the function returns, but a subsequent call to wait() is still required to
	keep the object consistency.
Input parameters:
	- height: Number of elements of the entire set, > 0.
	- proc_ptr: Pointer on the processing callback function to be called on
		each subset. When called back, the function is provided with a TaskData
		structure, filled with the required pointers to private data and the
		bounds of the subset to process (half-open range).
	- min_slice_h: Minimum number of elements to process at once. This can
		reduce the number of parallel operations if the total number of elements
		is small enough. Use this parameter to reduce the threading overhead
		when a large number of threads is available and the data set is known
		to be small. > 0. It's legal to provide a number greater than the total
		number of elements.
	- glob_data: A structure containing data accessed by all the working
	threads.
Throws: Depends on dispatcher creation failures.
==============================================================================
*/

template <class T, class GD, int MAXT>
void	MTSlicer <T, GD, MAXT>::start (int height, GD &glob_data, ProcPtr proc_ptr, int min_slice_h)
{
	assert (height > 0);
	assert (&glob_data != 0);
	assert (proc_ptr != 0);
	assert (min_slice_h > 0);

	_proc_ptr = proc_ptr;

	if (_mt_flag)
	{
		int				nbr_threads = _avstp.get_nbr_threads ();
		nbr_threads = std::min (nbr_threads, int (MAXT));
		nbr_threads = std::min (nbr_threads, height / min_slice_h);
		nbr_threads = std::max (nbr_threads, 1);

		int				y_beg = 0;
		for (int t_cnt = 0; t_cnt < nbr_threads; ++t_cnt)
		{
			const int		y_end = (t_cnt + 1) * height / nbr_threads;
			TaskData &		task_data = _task_data_arr [t_cnt];
			task_data._glob_data_ptr = &glob_data;
			task_data._slicer_ptr    = this;
			task_data._y_beg         = y_beg;
			task_data._y_end         = y_end;
			y_beg = y_end;
		}
		assert (y_beg == height);

		_dispatcher_ptr = _avstp.create_dispatcher ();

		for (int t_cnt = 0; t_cnt < nbr_threads; ++t_cnt)
		{
			_avstp.enqueue_task (
				_dispatcher_ptr,
				&redirect_task,
				&_task_data_arr [t_cnt]
			);
		}
	}

	// Multi-threading disabled
	else
	{
		TaskData			task_data;
		task_data._glob_data_ptr = &glob_data;
		task_data._slicer_ptr    = this;
		task_data._y_beg         = 0;
		task_data._y_end         = height;

		T *				this_ptr =
			MTSlicer_Access <T, GD>::access (&glob_data);
		((*this_ptr).*(proc_ptr)) (task_data);
	}
}



/*
==============================================================================
Name: wait
Description:
	Waits for the completion of tasks previously enqueued with start().
	A call to wait() is mandatory after a start(), and you cannot call wait()
	if nothing has been started.
Throws: Nothing.
==============================================================================
*/

template <class T, class GD, int MAXT>
void	MTSlicer <T, GD, MAXT>::wait ()
{
	assert (_proc_ptr != 0);

	if (_mt_flag)
	{
		assert (_dispatcher_ptr != 0);

		_avstp.wait_completion (_dispatcher_ptr);
		_avstp.destroy_dispatcher (_dispatcher_ptr);
	}
	
	_proc_ptr = 0;
	_dispatcher_ptr = 0;
}



/*
==============================================================================
Name: use_dispatcher
Description:
	A function to access the dispatcher from the callback processing function.
	This is useful to enqueue other tasks on the dispatcher, they will be
	waited by wait() even if they are not directly related to the slices.
	Important: before calling this function, you must ensure first that you are
	running with multi-threading enabled.
Returns:
	A pointer to the dispatcher.
Throws: Nothing.
==============================================================================
*/

template <class T, class GD, int MAXT>
avstp_TaskDispatcher *	MTSlicer <T, GD, MAXT>::use_dispatcher () const
{
	assert (is_mt ());
	assert (_dispatcher_ptr != 0);

	return (_dispatcher_ptr);
}



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



template <class T, class GD, int MAXT>
void	MTSlicer <T, GD, MAXT>::redirect_task (avstp_TaskDispatcher *dispatcher_ptr, void *data_ptr)
{
	TaskData *		td_ptr   = reinterpret_cast <TaskData *> (data_ptr);
	assert (td_ptr != 0);
	assert (td_ptr->_glob_data_ptr != 0);
	assert (td_ptr->_slicer_ptr != 0);

	T *				this_ptr =
		MTSlicer_Access <T, GD>::access (td_ptr->_glob_data_ptr);
	assert (this_ptr != 0);

	ProcPtr			proc_ptr = td_ptr->_slicer_ptr->_proc_ptr;
	assert (proc_ptr != 0);

	((*this_ptr).*(proc_ptr)) (*td_ptr);
}



#endif	// MTSlicer_CODEHEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
