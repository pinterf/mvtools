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

#include "conc/AtomicPtr.h"
#include "conc/AtomicInt.h"
#include "conc/LockFreeCell.h"
#include "conc/LockFreeStack.h"
#include "fstb/SingleObj.h"

#include <array>
#include <mutex>

#include <cstddef>
#include <cstdint>



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
	virtual        ~CellPool ();

	void           clear_all ();
	void           expand_to (size_t nbr_cells);

	inline CellType *
	               take_cell (bool autogrow_flag = false);
	inline void    return_cell (CellType &cell);



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

protected:



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:

	static const int  MAX_NBR_ZONES  = 64;
	static const int  GROW_RATE_NUM  = 3;
	static const int  GROW_RATE_DEN  = 2;
	static const int  BASE_SIZE      = 64; // Number of cells for the first zone

	typedef  LockFreeStack <T>    CellStack;
	typedef  AtomicInt <size_t>   CountCells;
	typedef  AtomicInt <int>      CountZones;
	typedef  std::array <AtomicPtr <CellType>, MAX_NBR_ZONES>  ZoneList;

	class Members	// These ones must be aligned
	{
	public:
		CountCells     _nbr_avail_cells;
		CountZones     _nbr_zones;
		ZoneList       _zone_list;
	};

	class AliAllo
	{
	public:
		uint8_t *      _ptr;
		size_t         _nbr_elt;
	};

	void           allocate_zone (size_t cur_size, AtomicPtr <CellType> & zone_ptr_ref);

	static inline size_t
	               compute_grown_size (size_t prev_size);
	static inline size_t
	               compute_total_size_for_zones (int nbr_zones);
	static CellType *
	               alloc_cells (size_t n);
	static void    dealloc_cells (CellType *ptr);

	CellStack      _cell_stack;
	std::mutex     _alloc_mutex;
	fstb::SingleObj <Members>
	               _m_ptr;



/*\\\ FORBIDDEN MEMBER FUNCTIONS \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:

	               CellPool (const CellPool <T> &other)          = delete;
	               CellPool (CellPool <T> &&other)               = delete;
	CellPool <T> & operator = (const CellPool <T> &other)        = delete;
	CellPool <T> & operator = (CellPool <T> &&other)             = delete;
	bool           operator == (const CellPool <T> &other) const = delete;
	bool           operator != (const CellPool <T> &other) const = delete;

};	// class CellPool



}	// namespace conc



#include "conc/CellPool.hpp"



#endif	// conc_CellPool_HEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
