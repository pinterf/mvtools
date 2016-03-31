/*****************************************************************************

        CellPool.h
        Author: Laurent de Soras, 2011

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if ! defined (conc_CellPool_HEADER_INCLUDED)
#define	conc_CellPool_HEADER_INCLUDED

#if defined (_MSC_VER)
	#pragma once
	#pragma warning (4 : 4250)
#endif



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#include	"conc/Array.h"
#include	"conc/AtomicPtr.h"
#include	"conc/AtomicInt.h"
#include	"conc/LockFreeCell.h"
#include	"conc/LockFreeStack.h"
#include	"conc/Mutex.h"

#include	<cstddef>



namespace conc
{



template <class T>
class CellPool
{

/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

public:

	typedef	T	DataType;
	typedef	LockFreeCell <T>	CellType;

						CellPool ();
	virtual			~CellPool ();

	void				clear_all ();
	void				expand_to (size_t nbr_cells);

	inline CellType *
						take_cell (bool autogrow_flag = false);
	inline void		return_cell (CellType &cell);



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

protected:



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:

	enum {			MAX_NBR_ZONES	= 64	};
	enum {			GROW_RATE_NUM	= 3	};
	enum {			GROW_RATE_DEN	= 2	};
	enum {			BASE_SIZE		= 64	};		// Number of cells for the first zone

	typedef	LockFreeStack <T>	CellStack;
	typedef	AtomicInt <size_t>	CountCells;
	typedef	AtomicInt <int>		CountZones;
	typedef	Array <AtomicPtr <CellType>, MAX_NBR_ZONES>	ZoneList;

	void				allocate_zone (int zone_index, size_t cur_size, AtomicPtr <CellType> & zone_ptr_ref);

	static inline size_t
						compute_grown_size (size_t prev_size);
	static inline size_t
						compute_total_size_for_zones (int nbr_zones);

	CellStack		_cell_stack;
	ZoneList			_zone_list;
	CountCells		_nbr_avail_cells;
	CountZones		_nbr_zones;
	Mutex				_alloc_mutex;



/*\\\ FORBIDDEN MEMBER FUNCTIONS \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:

						CellPool (const CellPool <T> &other);
	CellPool <T> &	operator = (const CellPool <T> &other);
	bool				operator == (const CellPool <T> &other) const;
	bool				operator != (const CellPool <T> &other) const;

};	// class CellPool



}	// namespace conc



#include	"conc/CellPool.hpp"



#endif	// conc_CellPool_HEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
