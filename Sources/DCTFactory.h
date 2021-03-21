/*****************************************************************************

        DCTFactory.h
        Author: Laurent de Soras, 2012

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if ! defined (DCTFactory_HEADER_INCLUDED)
#define	DCTFactory_HEADER_INCLUDED

#if defined (_MSC_VER)
#pragma once
#pragma warning (4 : 4250)
#endif

/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#include	"conc/ObjFactoryInterface.h"
#include	"DCTClass.h"
#include "fftwlite.h"




class IScriptEnvironment;

class DCTFactory
  : public conc::ObjFactoryInterface <DCTClass>
{

/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

public:

  explicit DCTFactory(int dctmode, bool isse, int blksizex, int blksizey, int pixelsize, int bits_per_pixel, ::IScriptEnvironment &env);
  virtual ~DCTFactory();

  int get_dctmode() const;
  bool use_fftw() const;
  int cpuflags;



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

protected:

  // ObjFactoryInterface
  virtual DCTClass * do_create();



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:

  FFTFunctionPointers fftfp;
  const int _dctmode;
  const int _blksizex;
  const int _blksizey;
  const bool _isse;
  const bool _fftw_flag;
  const int _pixelsize; // PF
  const int _bits_per_pixel;



/*\\\ FORBIDDEN MEMBER FUNCTIONS \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:

  DCTFactory();
  DCTFactory(const DCTFactory &other);
  DCTFactory & operator = (const DCTFactory &other);
  bool operator == (const DCTFactory &other) const;
  bool operator != (const DCTFactory &other) const;

};	// class DCTFactory



//#include	"DCTFactory.hpp"



#endif	// DCTFactory_HEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
