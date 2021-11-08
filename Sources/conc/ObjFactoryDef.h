/*****************************************************************************

        ObjFactoryDef.h
        Author: Laurent de Soras, 2012

Simple factory for ObjPool using the class default constructor.

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if ! defined (conc_ObjFactoryDef_HEADER_INCLUDED)
#define	conc_ObjFactoryDef_HEADER_INCLUDED

#if defined (_MSC_VER)
	#pragma once
	#pragma warning (4 : 4250)
#endif



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#include "conc/ObjFactoryInterface.h"



namespace conc
{



template <class T>
class ObjFactoryDef
:	public ObjFactoryInterface
{

/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

public:

	               ObjFactoryDef ()  = default;
	virtual        ~ObjFactoryDef () = default;

	static ObjFactoryDef <T>
	               _fact;



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

protected:

	// conc::ObjFactoryInterface
	virtual T *    do_create ();



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:



/*\\\ FORBIDDEN MEMBER FUNCTIONS \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:

	               ObjFactoryDef (const ObjFactoryDef &other)     = delete;
	ObjFactoryDef& operator = (const ObjFactoryDef &other)        = delete;
	bool           operator == (const ObjFactoryDef &other) const = delete;
	bool           operator != (const ObjFactoryDef &other) const = delete;

};	// class ObjFactoryDef



}	// namespace conc



#include "conc/ObjFactoryDef.hpp"



#endif	// conc_ObjFactoryDef_HEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
