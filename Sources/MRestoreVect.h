/*****************************************************************************

        MRestoreVect.h
        Authors: Laurent de Soras, Vit, 2011

*Tab=3***********************************************************************/



#if ! defined (MRestoreVect_HEADER_INCLUDED)
#define	MRestoreVect_HEADER_INCLUDED

#if defined (_MSC_VER)
  #pragma once
  #pragma warning (4 : 4250)
#endif



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#include	"def.h"
#include "MVAnalysisData.h"
#include	"types.h"

#include "avisynth.h"
#include <stdint.h>



class MRestoreVect
:	public ::GenericVideoFilter
{

/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

public:

  explicit			MRestoreVect (::PClip src, int clip_index, ::IScriptEnvironment *env);
  virtual			~MRestoreVect () {}

  // GenericVideoFilter
  ::PVideoFrame __stdcall
            GetFrame (int n, ::IScriptEnvironment *env_ptr);



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

protected:



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:
  bool has_at_least_v8 = true;

  CHECK_COMPILE_TIME (SizeOfInt, (sizeof (int) == sizeof (int32_t)));

  void				read_frame_info (int &data_offset_bytes, int &data_len, ::PVideoFrame frame_ptr, ::IScriptEnvironment *env) const;
  void				read_from_clip (int &src_pos, const uint8_t base_ptr [], void *dst_ptr, int len, int stride) const;

  MVAnalysisData	_mad;
  int				_clip_index;		// Starting from 0
  int				_bytes_per_pix;	// Input and output clips
  int				_row_size;			// Number of bytes contained in a line of the input clip, not taking pitch into account
  int				_available_size;	// Number of bytes contained in a frame of the input clip



/*\\\ FORBIDDEN MEMBER FUNCTIONS \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:

            MRestoreVect ();
            MRestoreVect (const MRestoreVect &other);
  MRestoreVect &	operator = (const MRestoreVect &other);
  bool				operator == (const MRestoreVect &other) const;
  bool				operator != (const MRestoreVect &other) const;

};	// class MRestoreVect



//#include	"MRestoreVect.hpp"



#endif	// MRestoreVect_HEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
