/*****************************************************************************

        DCTFactory.cpp
        Author: Laurent de Soras, 2012

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if defined (_MSC_VER)
	#pragma warning (1 : 4130 4223 4705 4706)
	#pragma warning (4 : 4355 4786 4800)
#endif



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#define	NOMINMAX
#define	NOGDI
#define	WIN32_LEAN_AND_MEAN

#include	<windows.h>
#include	"avisynth.h"

#include	"DCTFactory.h"
#include	"DCTFFTW.h"
#include	"DCTINT.h"

#include	<cassert>



/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

DCTFactory::DCTFactory (int dctmode, bool isse, int blksizex, int blksizey, int pixelsize, int bits_per_pixel, ::IScriptEnvironment &env)
:	_fftw_hnd (0)
,	_dctmode (dctmode)
,	_isse (isse)
,	_blksizex (blksizex)
,	_blksizey (blksizey)
,	_fftw_flag (! (_isse && _blksizex == 8 && _blksizey == 8 && pixelsize==1)) // only 8x8 is implemented as an int FFT
,	_pixelsize(pixelsize)
,	_bits_per_pixel(bits_per_pixel)

{
	assert (dctmode != 0);

	if (_fftw_flag)
	{
    _fftw_hnd = ::LoadLibrary("libfftw3f-3.dll"); // delayed loading, original name
    if (_fftw_hnd == NULL)
      _fftw_hnd = ::LoadLibrary ("fftw3.dll"); // delayed loading
    if (_fftw_hnd == NULL)
		{
			env.ThrowError ("MAnalyse: Can not load libfftw3f-3.dll or fftw3.DLL!");
		}
	}
}



DCTFactory::~DCTFactory ()
{
	if (_fftw_hnd != 0)
	{
		::FreeLibrary (_fftw_hnd);
		_fftw_hnd = 0;
	}
}



int	DCTFactory::get_dctmode () const
{
	return (_dctmode);
}



bool	DCTFactory::use_fftw () const
{
	return (_fftw_flag);
}



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



DCTClass *	DCTFactory::do_create ()
{
	if (_fftw_flag)
	{
		return (new DCTFFTW (_blksizex, _blksizey, _fftw_hnd, _dctmode, _pixelsize, _bits_per_pixel));
	}
	return (new DCTINT (_blksizex, _blksizey, _dctmode));
}



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
