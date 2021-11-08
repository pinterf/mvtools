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



namespace conc
{



/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



template <class T>
LockFreeStack <T>::LockFreeStack ()
:	_head_ptr_ptr ()
{
	_head_ptr_ptr->set (nullptr, 0);
}



template <class T>
void	LockFreeStack <T>::push (CellType &cell)
{
	CellType *     head_ptr = nullptr;
	intptr_t       count    = 0;
	do
	{
		head_ptr = _head_ptr_ptr->get_ptr ();
		count    = _head_ptr_ptr->get_val ();
		cell._next_ptr = head_ptr;
	}
	while (! _head_ptr_ptr->cas2 (&cell, count + 1, head_ptr, count));
}



// Returns 0 if the stack is empty.
template <class T>
typename LockFreeStack <T>::CellType *	LockFreeStack <T>::pop ()
{
	CellType *     cell_ptr  = nullptr;
	bool           cont_flag = true;
	do
	{
		cell_ptr = _head_ptr_ptr->get_ptr ();

		if (cell_ptr == nullptr)
		{
			cont_flag = false; // Empty stack.
		}

		else
		{
			const intptr_t    count = _head_ptr_ptr->get_val ();
			if (cell_ptr != nullptr)
			{
				CellType *     next_ptr = cell_ptr->_next_ptr;
				if (_head_ptr_ptr->cas2 (next_ptr, count + 1, cell_ptr, count))
				{
					cell_ptr->_next_ptr = nullptr;
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
