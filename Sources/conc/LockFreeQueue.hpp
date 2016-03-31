/*****************************************************************************

        LockFreeQueue.hpp
        Author: Laurent de Soras, 2011

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if ! defined (conc_LockFreeQueue_CODEHEADER_INCLUDED)
#define	conc_LockFreeQueue_CODEHEADER_INCLUDED



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#include	<cassert>



namespace conc
{



/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



template <class T>
LockFreeQueue <T>::LockFreeQueue ()
:	_head ()
,	_tail ()
,	_dummy ()
{
	_dummy._next_ptr = 0;
	_head.set (&_dummy, 0);
	_tail.set (&_dummy, 0);
}



template <class T>
void	LockFreeQueue <T>::enqueue (CellType &cell)
{
	assert (&cell != 0);

	cell._next_ptr = 0;	// set the cell next pointer to NULL

	CellType *		tail_ptr;
	ptrdiff_t		icount;

	bool				cont_flag = true;
	do	// try until enqueue is done
	{
		icount   = _tail.get_val ();	// read the tail modification count
		tail_ptr = _tail.get_ptr ();	// read the tail cell

		// try to link the cell to the tail cell
		void *			old_ptr = tail_ptr->_next_ptr.cas (&cell, 0);

		if (old_ptr == 0)
		{
			cont_flag = false;	// enqueue is done, exit the loop
		}
		else
		{
			// tail was not pointing to the last cell, try to set tail to the next cell
			_tail.cas2 (tail_ptr->_next_ptr, icount + 1, tail_ptr, icount);
		}
	}
	while (cont_flag);

	_tail.cas2 (&cell, icount + 1, tail_ptr, icount);
}



// Returns 0 if the queue is empty.
template <class T>
typename LockFreeQueue <T>::CellType *	LockFreeQueue <T>::dequeue ()
{
	ptrdiff_t		ocount;
	ptrdiff_t		icount;
	CellType *		head_ptr;
	CellType *		next_ptr;

	do	// try until dequeue is done
	{
		ocount = _head.get_val ();	// read the head modification count
		icount = _tail.get_val ();	// read the tail modification count
		head_ptr = _head.get_ptr ();		// read the head cell
		next_ptr = head_ptr->_next_ptr;	// read the next cell

		if (ocount == _head.get_val ())	// ensures that next is a valid pointer to avoid failure when reading next value
		{
			if (head_ptr == _tail.get_ptr ())	// is queue empty or tail falling behind ?
			{
				if (next_ptr == 0)	// is queue empty ?
				{
					return (0);	// queue is empty: return NULL
				}
				// tail is pointing to head in a non empty queue, try to set tail to the next cell
				_tail.cas2 (next_ptr, icount + 1, head_ptr, icount);
			}
			else if (next_ptr != 0)	// if we are not competing on the dummy next
			{
				// try to set tail to the next cell
				if (_head.cas2 (next_ptr, ocount + 1, head_ptr, ocount))
				{
					break;	// dequeue done, exit the loop
				}
			}
		}
	}
	while (true);

	if (head_ptr == &_dummy)	// check wether we’re trying to pop the dummy cell
	{
		enqueue (*head_ptr);		// this is the dummy cell: push it back to the fifo
		head_ptr = dequeue ();	// and pop a cell again
	}

	return (head_ptr);
}



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



}	// namespace conc



#endif	// conc_LockFreeQueue_CODEHEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
