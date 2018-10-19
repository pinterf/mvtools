/*****************************************************************************

        LockFreeStack.h
        Author: Laurent de Soras, 2011

The object is placed in a cell which is sent to the stack. The cell is NOT
copied during the process of push/pop and therefore is not kept by the stack.
Only its link is changed.

It is to the user's responsibility to handle the cell memory and to keep it
safe until the cell is popped.

It has also to keep the cell's memory location valid until ALL the reading
clients terminate their operations, because the pop() operation may be reading
the next pointer of a cell which as been already popped by another reader.

Template parameters:

- T: type of the contained objects.

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if ! defined (conc_LockFreeStack_HEADER_INCLUDED)
#define	conc_LockFreeStack_HEADER_INCLUDED

#if defined (_MSC_VER)
	#pragma once
	#pragma warning (4 : 4250)
#endif



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#include	"conc/AtomicPtrIntPair.h"
#include	"conc/LockFreeCell.h"



namespace conc
{



template <class T>
class LockFreeStack
{

/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

public:

  using ValueType = T;
  using CellType = typename LockFreeCell <T>;

						LockFreeStack ();
	virtual			~LockFreeStack () {}

	void				push (CellType &cell);
	CellType *		pop ();



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

protected:



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:

	AtomicPtrIntPair <CellType>
						_head_ptr;



/*\\\ FORBIDDEN MEMBER FUNCTIONS \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:

						LockFreeStack (const LockFreeStack <T> &other);
	LockFreeStack <T> &operator = (const LockFreeStack <T> &other);
	bool				operator == (const LockFreeStack <T> &other) const;
	bool				operator != (const LockFreeStack <T> &other) const;

};	// class LockFreeStack



}	// namespace conc



#include	"conc/LockFreeStack.hpp"



#endif	// conc_LockFreeStack_HEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
