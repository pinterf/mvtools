/*****************************************************************************

        LockFreeStack.hpp
        Author: Laurent de Soras, 2011

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if ! defined (conc_LockFreeStack_CODEHEADER_INCLUDED)
#define	conc_LockFreeStack_CODEHEADER_INCLUDED



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#include	<cassert>



namespace conc
{



/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



template <class T>
LockFreeStack <T>::LockFreeStack ()
:	_head_ptr ()
{
	_head_ptr.set (0, 0);
}



template <class T>
void	LockFreeStack <T>::push (CellType &cell)
{
	assert (&cell != 0);

	CellType *		head_ptr;
	ptrdiff_t		count;
	do
	{
		head_ptr = _head_ptr.get_ptr ();
		count    = _head_ptr.get_val ();
		cell._next_ptr = head_ptr;
	}
	while (! _head_ptr.cas2 (&cell, count + 1, head_ptr, count));
}



// Returns 0 if the stack is empty.
template <class T>
typename LockFreeStack <T>::CellType *	LockFreeStack <T>::pop ()
{
	CellType *		cell_ptr;
	bool				cont_flag = true;
	do
	{
		cell_ptr = _head_ptr.get_ptr ();

		if (cell_ptr == 0)
		{
			cont_flag = false;	// Empty stack.
		}

		else
		{
			const ptrdiff_t	count = _head_ptr.get_val ();
			if (cell_ptr != 0)
			{
				CellType *		next_ptr = cell_ptr->_next_ptr;
				if (_head_ptr.cas2 (next_ptr, count + 1, cell_ptr, count))
				{
					cell_ptr->_next_ptr = 0;
					cont_flag = false;
				}
			}
		}
	}
	while (cont_flag);

	return (cell_ptr);
}



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



}	// namespace conc



#endif	// conc_LockFreeStack_CODEHEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
