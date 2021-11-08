/*****************************************************************************

        CellPool.hpp
        Author: Laurent de Soras, 2011

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if ! defined (conc_CellPool_CODEHEADER_INCLUDED)
#define	conc_CellPool_CODEHEADER_INCLUDED



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#include "conc/AioMax.h"
#include "conc/AtomicIntOp.h"

#include <type_traits>

#include <cassert>



namespace conc
{



/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



template <class T>
CellPool <T>::CellPool ()
:	_cell_stack ()
,	_alloc_mutex ()
,	_m_ptr ()
{
	static_assert (BASE_SIZE * GROW_RATE_NUM / GROW_RATE_DEN > BASE_SIZE, "");

	_m_ptr->_nbr_avail_cells = 0;
	_m_ptr->_nbr_zones       = 0;

	for (int zone_index = 0; zone_index < MAX_NBR_ZONES; ++zone_index)
	{
		_m_ptr->_zone_list [zone_index] = nullptr;
	}
}



template <class T>
CellPool <T>::~CellPool ()
{
	clear_all ();
}



// Not thread-safe.
// All cells must have been returned before calling this function.
template <class T>
void	CellPool <T>::clear_all ()
{
#if !defined (NDEBUG)
	const size_t   nbr_total_cells =
		compute_total_size_for_zones (_m_ptr->_nbr_zones);
	
	assert (_m_ptr->_nbr_avail_cells == nbr_total_cells);
#endif
	
	while (_cell_stack.pop () != nullptr)
	{
		continue;
	}

	const int      nbr_zones = _m_ptr->_nbr_zones;
	for (int zone_index = 0; zone_index < nbr_zones; ++zone_index)
	{
		AtomicPtr <CellType> &  zone_ptr_ref = _m_ptr->_zone_list [zone_index];
		CellType *     zone_ptr = zone_ptr_ref;
		if (zone_ptr != nullptr)
		{
			dealloc_cells (zone_ptr);
			zone_ptr_ref = nullptr;
		}
	}
	_m_ptr->_nbr_zones       = 0;
	_m_ptr->_nbr_avail_cells = 0;
}



// Thread-safe but locks
template <class T>
void	CellPool <T>::expand_to (size_t nbr_cells)
{
	assert (nbr_cells > 0);

	size_t         cur_size = BASE_SIZE;
	size_t         total_size = 0;
	int            zone_index = 0;
	while (total_size < nbr_cells && zone_index < MAX_NBR_ZONES)
	{
		AtomicPtr <CellType> &  zone_ptr_ref = _m_ptr->_zone_list [zone_index];
		const CellType *  zone_ptr = zone_ptr_ref;
		if (zone_ptr == nullptr)
		{
			allocate_zone (cur_size, zone_ptr_ref);
		}

		total_size += cur_size;
		cur_size = compute_grown_size (cur_size);
		++ zone_index;
	}

	AioMax <int>	ftor (zone_index);
	AtomicIntOp::exec (_m_ptr->_nbr_zones, ftor);
}



// Thread-safe
template <class T>
typename CellPool <T>::CellType *	CellPool <T>::take_cell (bool autogrow_flag)
{
	CellType *     cell_ptr = nullptr;
	
	const int      nbr_zones = _m_ptr->_nbr_zones;

	do
	{
		cell_ptr = _cell_stack.pop ();

		if (   cell_ptr == nullptr
		    && autogrow_flag
		    && nbr_zones < MAX_NBR_ZONES)
		{
			const size_t	new_size =
				compute_total_size_for_zones (nbr_zones + 1);
			expand_to (new_size);
		}
	}
	while (   cell_ptr == nullptr
	       && autogrow_flag
	       && nbr_zones < MAX_NBR_ZONES);

	if (cell_ptr != nullptr)
	{
		-- _m_ptr->_nbr_avail_cells;
	}

	return cell_ptr;
}



// Thread-safe
template <class T>
void	CellPool <T>::return_cell (CellType &cell)
{
	_cell_stack.push (cell);

	++ _m_ptr->_nbr_avail_cells;
}



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



template <class T>
void	CellPool <T>::allocate_zone (size_t cur_size, AtomicPtr <CellType> & zone_ptr_ref)
{
	std::lock_guard <std::mutex>  lock (_alloc_mutex);

	CellType *     zone_ptr = alloc_cells (cur_size);

	if (zone_ptr_ref.cas (zone_ptr, nullptr) != static_cast <CellType *> (nullptr))
	{
		// CAS has failed, meaning that another thread is allocating this zone.
		dealloc_cells (zone_ptr);

		// Note: because of the mutex, this part shouldn't be accessed.
	}

	else
	{
		// Success: fill the stack with the new cells.
		for (size_t pos = 0; pos < cur_size; ++pos)
		{
			CellType &     cell = zone_ptr [pos];
			_cell_stack.push (cell);

			++ _m_ptr->_nbr_avail_cells;
		}
	}
}



template <class T>
size_t	CellPool <T>::compute_grown_size (size_t prev_size)
{
	assert (prev_size >= BASE_SIZE);

	return (prev_size * GROW_RATE_NUM / GROW_RATE_DEN);
}



template <class T>
size_t	CellPool <T>::compute_total_size_for_zones (int nbr_zones)
{
	assert (nbr_zones >= 0);
	assert (nbr_zones <= MAX_NBR_ZONES);

	size_t         cur_size = BASE_SIZE;
	size_t         total_size = 0;
	int            zone_index = 0;
	while (zone_index < nbr_zones)
	{
		total_size += cur_size;
		cur_size = compute_grown_size (cur_size);
		++ zone_index;
	}

	return total_size;
}



template <class T>
typename CellPool <T>::CellType *	CellPool <T>::alloc_cells (size_t n)
{
	const size_t   nbr_bytes =
		sizeof (AliAllo) + (n + 1) * sizeof (CellType);
	const int      align     = std::alignment_of <CellType>::value;
	uint8_t *      raw_ptr   = new uint8_t [nbr_bytes];
	const intptr_t cell_zone =
		(  reinterpret_cast <intptr_t> (raw_ptr)
		 + sizeof (AliAllo)
		 + sizeof (CellType)) & -align;
	CellType *     cell_ptr  = reinterpret_cast <CellType *> (cell_zone);
	AliAllo *      info_ptr  = reinterpret_cast <AliAllo *> (cell_zone) - 1;

	info_ptr->_ptr     = raw_ptr;
	info_ptr->_nbr_elt = n;

	size_t         count = 0;
	try
	{
		for (count = 0; count < n; ++count)
		{
			new (cell_ptr + count) CellType;
		}
	}
	catch (...)
	{
		for (size_t i = count; i > 0; --i)
		{
			(cell_ptr + i - 1)->~CellType ();
		}
		delete [] raw_ptr;
		throw;
	}

	return cell_ptr;
}


template <class T>
void	CellPool <T>::dealloc_cells (CellType *ptr)
{
	AliAllo *      info_ptr = reinterpret_cast <AliAllo *> (ptr) - 1;
	const size_t   n        = info_ptr->_nbr_elt;
	uint8_t *      raw_ptr  = info_ptr->_ptr;

	for (size_t i = n; i > 0; --i)
	{
		(ptr + i - 1)->~CellType ();
	}

	delete [] raw_ptr;
}



}	// namespace conc



#endif	// conc_CellPool_CODEHEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
