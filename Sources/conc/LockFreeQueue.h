/*****************************************************************************

        LockFreeQueue.h
        Author: Laurent de Soras, 2011

The object is placed in a cell which is sent to the queue. The cell is NOT
copied during the process of enqueue/dequeue and therefore is not kept by the
queue. Only its link is changed.

It is to the user's responsibility to handle the cell memory and to keep it
safe until the cell is dequeued.

Template parameters:

- T: type of the contained object.

Algorithm from:

Dominique Fober, Yann Orlarey, Stephane Letz,
Optimised Lock-Free FIFO Queue,
January 2001

Dominique Fober, Yann Orlarey, Stephane Letz,
Optimised Lock-Free FIFO Queue continued,
May 2001

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if ! defined (conc_LockFreeQueue_HEADER_INCLUDED)
#define	conc_LockFreeQueue_HEADER_INCLUDED

#if defined (_MSC_VER)
	#pragma once
	#pragma warning (4 : 4250)
#endif



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#include	"conc/AtomicPtrIntPair.h"
#include	"conc/LockFreeCell.h"

#include	<cstddef>



namespace conc
{



template <class T>
class LockFreeQueue
{

/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

public:

	typedef	T	ValueType;
	typedef	LockFreeCell <T>	CellType;

						LockFreeQueue ();
	virtual			~LockFreeQueue () {}

	void				enqueue (CellType &cell);
	CellType *		dequeue ();



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

protected:



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:

	typedef	void *	InternalType;

	enum {			SL2	= (sizeof (InternalType) == 8) ? 4 : 3	};

	typedef	AtomicPtrIntPair <CellType>	SafePointer;

	SafePointer		_head;	// Contains ocount
	SafePointer		_tail;	// Contains icount

	CellType			_dummy;



/*\\\ FORBIDDEN MEMBER FUNCTIONS \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:

						LockFreeQueue (const LockFreeQueue <T> &other);
	LockFreeQueue <T> &
						operator = (const LockFreeQueue <T> &other);
	bool				operator == (const LockFreeQueue <T> &other) const;
	bool				operator != (const LockFreeQueue <T> &other) const;

};	// class LockFreeQueue



}	// namespace conc



#include	"conc/LockFreeQueue.hpp"



#endif	// conc_LockFreeQueue_HEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
