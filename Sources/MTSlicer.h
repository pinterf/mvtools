/*****************************************************************************

        MTSlicer.h
        Author: Laurent de Soras, 2012

Slices a task of indexed elements in sub-tasks made of subsets of adjacent
elements. Here the elements could be the rows of a frame.

This class can be instantiated on the stack (required for compatibility with
MT mode 1).

Template parameters:

- T: Main processing class (most likely your Avisynth filter).

- GD: Global task data. If different of T, it requires:
	T * GD::_this_ptr;

- MAXT: limit to the number of threads (because task-specific data is allocated
	on the stack).

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if ! defined (MTSlicer_HEADER_INCLUDED)
#define	MTSlicer_HEADER_INCLUDED

#if defined (_MSC_VER)
	#pragma once
	#pragma warning (4 : 4250)
#endif



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#include	"conc/Array.h"
#include	"avstp.h"



class AvstpWrapper;

template <class T, class GD = T, int MAXT = 64>
class MTSlicer
{

/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

public:

	typedef	MTSlicer <T, GD, MAXT>	ThisType;

	class TaskData
	{
	public:
		GD *				_glob_data_ptr;
		ThisType *		_slicer_ptr;
		int				_y_beg;	// Inclusive
		int				_y_end;	// Exclusive (last element + 1)
	};

	typedef	void (T::*ProcPtr) (TaskData &td);

	explicit			MTSlicer (bool mt_flag = true);
	virtual			~MTSlicer ();

	inline bool		is_mt () const;
	void				start (int height, GD &glob_data, ProcPtr proc_ptr, int min_slice_h = 1);
	void				wait ();

	inline avstp_TaskDispatcher *
						use_dispatcher () const;



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

protected:



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:

	static void		redirect_task (avstp_TaskDispatcher *dispatcher_ptr, void *data_ptr);

	AvstpWrapper &	_avstp;

	ProcPtr volatile
						_proc_ptr;
	avstp_TaskDispatcher * volatile
						_dispatcher_ptr;
	conc::Array <TaskData, MAXT>
						_task_data_arr;
	const bool		_mt_flag;	// No need to be const, but must not be changed while task are enqueued.



/*\\\ FORBIDDEN MEMBER FUNCTIONS \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:

						MTSlicer (const MTSlicer &other);
	MTSlicer &		operator = (const MTSlicer &other);
	bool				operator == (const MTSlicer &other) const;
	bool				operator != (const MTSlicer &other) const;

};	// class MTSlicer



#include	"MTSlicer.hpp"



#endif	// MTSlicer_HEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
