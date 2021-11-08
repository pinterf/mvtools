/*****************************************************************************

        AioSub.h
        Author: Laurent de Soras, 2011

This class is distinct from AioAdd just to avoid compiler warnings when using
negation on unsigned types.

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if ! defined (conc_AioSub_HEADER_INCLUDED)
#define	conc_AioSub_HEADER_INCLUDED

#if defined (_MSC_VER)
	#pragma once
	#pragma warning (4 : 4250)
#endif



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



namespace conc
{



template <class T>
class AioSub
{

/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

public:

	explicit inline
	               AioSub (T operand);

	inline T       operator () (T old_val) const;



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

protected:



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:

	T              _operand;



/*\\\ FORBIDDEN MEMBER FUNCTIONS \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:

	               AioSub ()                                   = delete;
	               AioSub (const AioSub <T> &other)            = delete;
	               AioSub (const AioSub <T> &&other)           = delete;
	AioSub <T> &   operator = (const AioSub <T> &other)        = delete;
	AioSub <T> &   operator = (const AioSub <T> &&other)       = delete;
	bool           operator == (const AioSub <T> &other) const = delete;
	bool           operator != (const AioSub <T> &other) const = delete;

};	// class AioSub



}	// namespace conc



#include "conc/AioSub.hpp"



#endif	// conc_AioSub_HEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
