/* This file is free software. It comes without any warranty, to
 * the extent permitted by applicable law. You can redistribute it
 * and/or modify it under the terms of the Do What The Fuck You Want
 * To Public License, Version 2, as published by Sam Hocevar. See
 * http://sam.zoy.org/wtfpl/COPYING for more details. */ 
 
#ifndef __TIME256PROVIDERCST__
#define __TIME256PROVIDERCST__

class Time256ProviderCst
{
public:
  // Both lut_b and lut_f can be 0, depending on the need of LUT.
  // You have to be careful if you initialise the object with null pointer.
  Time256ProviderCst (int t256, const short *lut_b, const short *lut_f)
  :	_t256 (t256)
  ,	_lut_b (lut_b)
  ,	_lut_f (lut_f)
  {
    // Nothing
  }
  inline bool		is_half () const
  {
    return (_t256 == 128);
  }
  inline int		get_t (int /*x*/) const
  {
    return (_t256);
  }
  inline int		get_vect_b (int /*time256*/, int v) const
  {
    return (_lut_b [v]);
  }
  inline int		get_vect_f (int /*time256*/, int v) const
  {
    return (_lut_f [v]);
  }
  inline void		jump_to_next_row () const
  {
    // Nothing
  }

private:
  int				_t256;
  const short *	_lut_b;
  const short *	_lut_f;
};

#endif
