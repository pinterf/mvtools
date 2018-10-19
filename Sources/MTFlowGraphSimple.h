/*****************************************************************************

        MTFlowGraphSimple.h
        Author: Laurent de Soras, 2012

A simple dependency graph for MTFlowGraphSched.

Template parameters:

- MAXT: maximum number of tasks contained in the graph.

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if ! defined (MTFlowGraphSimple_HEADER_INCLUDED)
#define	MTFlowGraphSimple_HEADER_INCLUDED

#if defined (_MSC_VER)
	#pragma once
	#pragma warning (4 : 4250)
#endif



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#include	"conc/Array.h"



template <int MAXT>
class MTFlowGraphSimple
{

/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

public:

  using ThisType = MTFlowGraphSimple <MAXT>;

	class Iterator
	{
	public:
		inline 			Iterator (const ThisType &fg, int node);
		inline void		next ();
		inline bool		cont () const;
		inline int		get_index () const;

	private:
		const ThisType &
							_fg;
		int				_node;
		int				_pos;
	};

						MTFlowGraphSimple ();
	virtual			~MTFlowGraphSimple () {}

	void				add_dep (int index_from, int index_to);
	void				clear ();

	int				get_last_node () const;
	int				get_nbr_in (int task_index) const;
	Iterator			get_out_node_it (int task_index) const;



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

protected:



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:

	class Node
	{
	public:
		int				_nbr_in;
		int				_nbr_out;
		conc::Array <short, MAXT>
							_out_arr;
	};

	int				_last_node;
	conc::Array <Node, MAXT>
						_node_arr;



/*\\\ FORBIDDEN MEMBER FUNCTIONS \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:

						MTFlowGraphSimple (const MTFlowGraphSimple <MAXT> &other);
	MTFlowGraphSimple <MAXT> &
						operator = (const MTFlowGraphSimple <MAXT> &other);
	bool				operator == (const MTFlowGraphSimple <MAXT> &other) const;
	bool				operator != (const MTFlowGraphSimple <MAXT> &other) const;

};	// class MTFlowGraphSimple



#include	"MTFlowGraphSimple.hpp"



#endif	// MTFlowGraphSimple_HEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
