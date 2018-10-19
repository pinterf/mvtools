/*****************************************************************************

        MTFlowGraphSched.h
        Author: Laurent de Soras, 2012

Runs a graph of tasks linked by their dependencies. You specify the dependency
graph, and the scheduler (this class) takes care of finding the adequate
processing order.

This class can be instantiated on the stack (required for compatibility with
MT mode 1).

Template parameters:

- T: Main processing class (most likely your Avisynth filter).

- GR: The actual graph containing the dependencies. Typically
	MTFlowGraphSimple. It requires:
	int GR::get_last_node () const;
		Returns the index of the last node of the graph.
	int GR::get_nbr_in (int taks_index) const;
		Gives the number of input nodes for a task (number of dependencies).
	GR::Iterator GR::get_out_node_it (int task_index) const;
		Returns an iterator on the first element of a list of the output nodes
		for a given task.
	void GR::Iterator::next ();
	bool GR::Iterator::cont ();
	int GR::Iterator::get_index ();
		Use:
		for (it = gr.get_out_node_it (a); it.cont (); it.next ())
		{
			out = it.get_index ();
		}

- GD: Global task data. If different of T, it requires:
	T * GD::_this_ptr;

- MAXT: limit to the number of tasks (because task-specific data is allocated
	on the stack).

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if ! defined (MTFlowGraphSched_HEADER_INCLUDED)
#define	MTFlowGraphSched_HEADER_INCLUDED

#if defined (_MSC_VER)
	#pragma once
	#pragma warning (4 : 4250)
#endif



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#include	"conc/Array.h"
#include	"conc/AtomicInt.h"
#include	"avstp.h"



class AvstpWrapper;

template <class T, class GR, class GD, int MAXT>
class MTFlowGraphSched
{

/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

public:

  using ThisType =  MTFlowGraphSched <T, GR, GD, MAXT>;

	explicit			MTFlowGraphSched (bool mt_flag = true);
	virtual			~MTFlowGraphSched ();

	class TaskData
	{
	public:
		GD *				_glob_data_ptr;
		ThisType *		_scheduler_ptr;
		int				_task_index;
	};

	typedef	void (T::*ProcPtr) (TaskData &td);

	inline bool		is_mt () const;
	void				start (const GR &dep_graph, GD &glob_data, ProcPtr proc_ptr);
	void				wait ();

	inline avstp_TaskDispatcher *
						use_dispatcher () const;



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

protected:



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:

  // When a task is finished, updates the dependency counter on the dependent
  // tasks and enqueues them if they become ready.
  void				complete_task(TaskData &td);

	static void		redirect_task (avstp_TaskDispatcher *dispatcher_ptr, void *data_ptr);

	AvstpWrapper &	_avstp;

	ProcPtr volatile
						_proc_ptr;
	avstp_TaskDispatcher * volatile
						_dispatcher_ptr;
	const GR * volatile	_dep_graph_ptr;
	conc::Array <TaskData, MAXT>
						_task_data_arr;
	conc::Array <conc::AtomicInt <int>, MAXT>
						_in_cnt_arr;

	const bool		_mt_flag;



/*\\\ FORBIDDEN MEMBER FUNCTIONS \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:

						MTFlowGraphSched (const MTFlowGraphSched <T, GR, GD, MAXT> &other);
	MTFlowGraphSched <T, GR, GD, MAXT> &
						operator = (const MTFlowGraphSched <T, GR, GD, MAXT> &other);
	bool				operator == (const MTFlowGraphSched <T, GR, GD, MAXT> &other) const;
	bool				operator != (const MTFlowGraphSched <T, GR, GD, MAXT> &other) const;

};	// class MTFlowGraphSched



#include	"MTFlowGraphSched.hpp"



#endif	// MTFlowGraphSched_HEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
