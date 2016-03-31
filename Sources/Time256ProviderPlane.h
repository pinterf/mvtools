/* This file is free software. It comes without any warranty, to
 * the extent permitted by applicable law. You can redistribute it
 * and/or modify it under the terms of the Do What The Fuck You Want
 * To Public License, Version 2, as published by Sam Hocevar. See
 * http://sam.zoy.org/wtfpl/COPYING for more details. */ 
 
#ifndef __TIME256PROVIDERPLANE__
#define __TIME256PROVIDERPLANE__

class Time256ProviderPlane
{
public:
	// Both lut_b and lut_f can be 0, depending on the need of LUT.
	// You have to be careful if you initialise the object with null pointer.
	Time256ProviderPlane (const BYTE *pt256, int t256_pitch, const short (*lut_b) [256], const short (*lut_f) [256])
	:	_pt256 (pt256)
	,	_t256_pitch (t256_pitch)
	,	_lut_b (lut_b)
	,	_lut_f (lut_f)
	{
		// Nothing
	}
	inline bool		is_half () const
	{
		return (false);
	}
	inline int		get_t (int x) const
	{
		return (_pt256 [x]);
	}
	inline int		get_vect_b (int time256, int v) const
	{
		return (_lut_b [time256] [v]);
	}
	inline int		get_vect_f (int time256, int v) const
	{
		return (_lut_f [time256] [v]);
	}
	inline void		jump_to_next_row ()
	{
		_pt256 += _t256_pitch;
	}

private:
	const BYTE *	_pt256;
	const int		_t256_pitch;
	const short (*	_lut_b) [256];
	const short (*	_lut_f) [256];
};

#endif
