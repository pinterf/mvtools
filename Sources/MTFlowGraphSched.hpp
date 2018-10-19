/*****************************************************************************

        MTFlowGraphSched.hpp
        Author: Laurent de Soras, 2012

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if ! defined (MTFlowGraphSched_CODEHEADER_INCLUDED)
#define	MTFlowGraphSched_CODEHEADER_INCLUDED



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#include	"AvstpWrapper.h"

#include	<cassert>
#include	<cstring>



// Internal utility functions

template <class T, class GD>
class MTFlowGraphSched_Access
{
public:
	static inline T *	access (GD *ptr) { return (ptr->_this_ptr); }
};

template <class T>
class MTFlowGraphSched_Access <T, T>
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

template <class T, class GR, class GD, int MAXT>
MTFlowGraphSched <T, GR, GD, MAXT>::MTFlowGraphSched (bool mt_flag)
:	_avstp (AvstpWrapper::use_instance ())
,	_proc_ptr (0)
,	_dispatcher_ptr (0)
,	_dep_graph_ptr (0)
,	_task_data_arr ()
,	_in_cnt_arr ()
,	_mt_flag ()
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

template <class T, class GR, class GD, int MAXT>
MTFlowGraphSched <T, GR, GD, MAXT>::~MTFlowGraphSched ()
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

template <class T, class GR, class GD, int MAXT>
bool	MTFlowGraphSched <T, GR, GD, MAXT>::is_mt () const
{
	return (_mt_flag);
}



/*
==============================================================================
Name: start
Description:
	Starts the parallel execution of a set of tasks linked by their
	dependencies. The tasks are launched in parallel when possible, but the
	actual thread use depends on what the dependencies allow.
	Task are identified by an index. There is no constraint on the numbering
	scheme, but it is recommended to keep the task set as compact as possible.
	Anyway, the root task is always located at index 0.
	The function returns as soon as the root task is enqueued. Dependent tasks
	are enqueued gradually as other tasks are terminated. Use wait() to wait
	for their completion. In monothreaded mode, processing is performed before
	the function returns, but a subsequent call to wait() is still required to
	keep the object consistency.
Input parameters:
	- dep_graph: This is the graph object containing the defined dependencies
		between the tasks. Please do not change the dependencies nor modify the
		object when the tasks are running!
	- proc_ptr: Pointer on the processing callback function to be called on
		each task. When called back, the function is provided with a TaskData
		structure, filled with the required pointers to private data and the
		index of the task to execute. You don't have to care about launching
		next tasks, the scheduler will do it for you.
	- glob_data: A structure containing data accessed by all the working
	threads.
Throws: Depends on dispatcher creation failures.
==============================================================================
*/

template <class T, class GR, class GD, int MAXT>
void	MTFlowGraphSched <T, GR, GD, MAXT>::start (const GR &dep_graph, GD &glob_data, ProcPtr proc_ptr)
{
	assert (&dep_graph != 0);
	assert (&glob_data != 0);
	assert (proc_ptr != 0);

	_proc_ptr      = proc_ptr;
	_dep_graph_ptr = &dep_graph;

	const int		last_node_index = _dep_graph_ptr->get_last_node ();
	memset (	// Not very clean but should work correctly.
		&_in_cnt_arr [0],
		0,
		sizeof (_in_cnt_arr [0]) * (last_node_index + 1)
	);

	// Enqueues the root task
	TaskData &		root = _task_data_arr [0];
	root._glob_data_ptr = &glob_data;
	root._scheduler_ptr = this;
	root._task_index    = 0;

	if (_mt_flag)
	{
		_dispatcher_ptr = _avstp.create_dispatcher ();
		_avstp.enqueue_task (_dispatcher_ptr, &redirect_task, &root);
	}
	else
	{
		T *				this_ptr =
			MTFlowGraphSched_Access <T, GD>::access (&glob_data);
		((*this_ptr).*(proc_ptr)) (root);

		complete_task (root);
	}
}



/*
==============================================================================
Name: wait
Description:
	Waits for the completion of whole task set previously enqueued with
	start(). A call to wait() is mandatory after a start(), and you cannot call
	wait() if nothing has been started.
Throws: Nothing.
==============================================================================
*/

template <class T, class GR, class GD, int MAXT>
void	MTFlowGraphSched <T, GR, GD, MAXT>::wait ()
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
	_dep_graph_ptr = 0;
}



/*
==============================================================================
Name: use_dispatcher
Description:
	A function to access the dispatcher from the callback processing function.
	This is useful to enqueue other tasks on the dispatcher, they will be
	waited by wait() even if they are not directly related to the initial task
	graph.
	Important: before calling this function, you must ensure first that you are
	running with multi-threading enabled.
Returns:
	A pointer to the dispatcher.
Throws: Nothing.
==============================================================================
*/

template <class T, class GR, class GD, int MAXT>
avstp_TaskDispatcher *	MTFlowGraphSched <T, GR, GD, MAXT>::use_dispatcher () const
{
	assert (_mt_flag);
	assert (_dispatcher_ptr != 0);

	return (_dispatcher_ptr);
}



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



// When a task is finished, updates the dependency counter on the dependent
// tasks and enqueues them if they become ready.
template <class T, class GR, class GD, int MAXT>
void	MTFlowGraphSched <T, GR, GD, MAXT>::complete_task(TaskData &td)
{
  assert(&td != 0);

  for (typename GR::Iterator it = _dep_graph_ptr->get_out_node_it(td._task_index)
    ;	it.cont()
    ;	it.next())
  {
    const int		out_index = it.get_index();
    assert(out_index >= 0);
    assert(out_index < MAXT);
    const int		count_new = ++_in_cnt_arr[out_index];
    const int		nbr_in = _dep_graph_ptr->get_nbr_in(out_index);
    if (count_new >= nbr_in)
    {
      TaskData &		out_node = _task_data_arr[out_index];
      out_node._glob_data_ptr = td._glob_data_ptr;
      out_node._scheduler_ptr = this;
      out_node._task_index = out_index;

      if (_mt_flag)
      {
        assert(_dispatcher_ptr != 0);
        _avstp.enqueue_task(_dispatcher_ptr, &redirect_task, &out_node);
      }
      else
      {
        T *				this_ptr =
          MTFlowGraphSched_Access <T, GD>::access(out_node._glob_data_ptr);

        ((*this_ptr).*(_proc_ptr)) (out_node);

        complete_task(out_node);
      }
    }
  }
}



template <class T, class GR, class GD, int MAXT>
void	MTFlowGraphSched <T, GR, GD, MAXT>::redirect_task (avstp_TaskDispatcher *dispatcher_ptr, void *data_ptr)
{
	TaskData *		td_ptr   = reinterpret_cast <TaskData *> (data_ptr);
	assert (td_ptr != 0);
	assert (td_ptr->_glob_data_ptr != 0);
	assert (td_ptr->_scheduler_ptr != 0);

	T *				this_ptr =
		MTFlowGraphSched_Access <T, GD>::access (td_ptr->_glob_data_ptr);
	assert (this_ptr != 0);

	ThisType &		scheduler = *(td_ptr->_scheduler_ptr);
	ProcPtr			proc_ptr = scheduler._proc_ptr;
	assert (proc_ptr != 0);

	((*this_ptr).*(proc_ptr)) (*td_ptr);

	scheduler.complete_task (*td_ptr);
}



#endif	// MTFlowGraphSched_CODEHEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
