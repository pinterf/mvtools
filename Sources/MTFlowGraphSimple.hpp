/*****************************************************************************

        MTFlowGraphSimple.hpp
        Author: Laurent de Soras, 2012

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if ! defined (MTFlowGraphSimple_CODEHEADER_INCLUDED)
#define	MTFlowGraphSimple_CODEHEADER_INCLUDED



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#include	<algorithm>

#include	<cassert>



/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



/*
==============================================================================
Name: ctor
Description:
	Creates the graph. There is only one node, the root.
Throws: Nothing
==============================================================================
*/

template <int MAXT>
MTFlowGraphSimple <MAXT>::MTFlowGraphSimple ()
:	_last_node (0)
,	_node_arr ()
{
	clear ();
}



/*
==============================================================================
Name: add_dep
Description:
	Add a dependency between two tasks, so index_from is always executed before
	index_to. Note that the task must be connected (indirectly) to the root to
	be executed. Root has index 0.
	It doesn't check if a link is added twice, anyway it should work.
	Circular dependencies are not checked neither, and they must be avoided.
Input parameters:
	- index_from: index of the task to execute first. Range [0 ; MAXT[.
	- index_to: index of the task dependent on index_from. Range ]0 ; MAXT[.
Throws: Nothing
==============================================================================
*/

template <int MAXT>
void	MTFlowGraphSimple <MAXT>::add_dep (int index_from, int index_to)
{
	assert (index_from >= 0);
	assert (index_from < MAXT);
	assert (_node_arr [index_from]._nbr_out < MAXT);
	assert (index_to > 0);	// Excludes the root node
	assert (index_to < MAXT);
	assert (index_from != index_to);

	Node &			node_from = _node_arr [index_from];
	node_from._out_arr [node_from._nbr_out] = index_to;
	++ node_from._nbr_out;

	Node &			node_to   = _node_arr [index_to];
	++ node_to._nbr_in;

	_last_node = std::max (_last_node, index_from);
	_last_node = std::max (_last_node, index_to);
}



/*
==============================================================================
Name: clear
Description:
	Removes all the dependencies, thus all tasks excepted the root.
Throws: Nothing
==============================================================================
*/

template <int MAXT>
void	MTFlowGraphSimple <MAXT>::clear ()
{
	_last_node = 0;
	for (int k = 0; k < MAXT; ++k)
	{
		Node &			node = _node_arr [k];
		node._nbr_in  = 0;
		node._nbr_out = 0;
	}
}



/*
==============================================================================
Name: get_last_node
Description:
	Indicates the highest referenced task index. This is distinct to the last
	task in execution order, because index and order are not linked.
Returns: The last task index, range [0 ; MAXT[
Throws: Nothing
==============================================================================
*/

template <int MAXT>
int	MTFlowGraphSimple <MAXT>::get_last_node () const
{
	assert (_last_node >= 0);
	assert (_last_node < MAXT);

	return (_last_node);
}



/*
==============================================================================
Name: get_nbr_in
Description:
	Gives the number of input nodes for a task, that is the number of its
	dependencies.
Input parameters:
	- task_index: Index of the desired task, > 0.
Returns: The number of direct preceding tasks, >= 0.
Throws: Nothing
==============================================================================
*/

template <int MAXT>
int	MTFlowGraphSimple <MAXT>::get_nbr_in (int task_index) const
{
	assert (task_index >= 0);
	assert (task_index < MAXT);

	return (_node_arr [task_index]._nbr_in);
}



/*
==============================================================================
Name: get_out_node_it
Description:
	Returns an iterator on the list of tasks depending on the provided task.
	The iterator is initialised to the first task of the list.
	List order is not specified.
Input parameters:
	- task_index: index of the task we want to know its dependent task list.
Returns:
	The iterator, pointing on the first element (or terminated if the node
	is a leaf and the list is empty).
Throws: Nothing.
==============================================================================
*/

template <int MAXT>
typename MTFlowGraphSimple <MAXT>::Iterator	MTFlowGraphSimple <MAXT>::get_out_node_it (int task_index) const
{
	assert (task_index >= 0);
	assert (task_index < MAXT);

	return (Iterator (*this, task_index));
}



/*
==============================================================================
Name: ctor
Description:
	Iterator constructor, internal, not for public use.
Throws: Nothing
==============================================================================
*/

template <int MAXT>
MTFlowGraphSimple <MAXT>::Iterator::Iterator (const ThisType &fg, int node)
:	_fg (fg)
,	_node (node)
,	_pos (0)
{
	assert (node >= 0);
	assert (node < MAXT);
}



/*
==============================================================================
Name: next
Description:
	Make the iterator point on the next list element.
	The iterator must be valid (iteration not terminated yet).
Throws: Nothing
==============================================================================
*/

template <int MAXT>
void	MTFlowGraphSimple <MAXT>::Iterator::next ()
{
	assert (cont ());

	++ _pos;
}



/*
==============================================================================
Name: cont
Description:
	Indicates if the list continues, if the iterator points on a valid element.
	Must be checked before any iterator use after a get_out_node_it() or a
	next().
Returns: true if there is a valid element, false if the list is terminated.
Throws: Nothing
==============================================================================
*/

template <int MAXT>
bool	MTFlowGraphSimple <MAXT>::Iterator::cont () const
{
	return (_pos < _fg._node_arr [_node]._nbr_out);
}



/*
==============================================================================
Name: get_index
Description:
	Given the iterator is valid, returns the task index for this position in
	the list.
Returns: The dependent task index.
Throws: Nothing
==============================================================================
*/

template <int MAXT>
int	MTFlowGraphSimple <MAXT>::Iterator::get_index () const
{
	assert (cont ());

	return (_fg._node_arr [_node]._out_arr [_pos]);
}



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



#endif	// MTFlowGraphSimple_CODEHEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
