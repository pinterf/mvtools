/*****************************************************************************

        MStoreVect.h
        Authors: Laurent de Soras, Vit, 2011

*Tab=3***********************************************************************/



#if ! defined (MStoreVect_HEADER_INCLUDED)
#define	MStoreVect_HEADER_INCLUDED

#if defined (_MSC_VER)
	#pragma once
	#pragma warning (4 : 4250)
#endif



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#include	"def.h"
#include "MVAnalysisData.h"
#include	"types.h"

#define	NOGDI
#define	NOMINMAX
#define	WIN32_LEAN_AND_MEAN
#include "Windows.h"
#include	"avisynth.h"

#include	<vector>



class MStoreVect
:	public ::GenericVideoFilter
{

/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

public:

	explicit			MStoreVect (std::vector <::PClip> clip_arr, const char *vccs_0, ::IScriptEnvironment &env);
	virtual			~MStoreVect () {}

	// GenericVideoFilter
	::PVideoFrame __stdcall
						GetFrame (int n, ::IScriptEnvironment *env_ptr);



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

protected:



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:

	CHECK_COMPILE_TIME (SizeOfInt, (sizeof (int) == sizeof (int32_t)));

	class VectData
	{
	public:
		::PClip			_clip_sptr;
		int				_data_offset;	// int32_t words, based only on width, not pitch
		int				_data_len;		// int32_t words, same as above.
	};
	typedef	std::vector <VectData>	VectArray;

	void				write_to_clip (int &dst_pos, uint8_t base_ptr [], const void *src_ptr, int len, int stride);

	VectArray		_vect_arr;
	int				_end_offset;		// int32_t words



/*\\\ FORBIDDEN MEMBER FUNCTIONS \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:

						MStoreVect ();
						MStoreVect (const MStoreVect &other);
	MStoreVect &	operator = (const MStoreVect &other);
	bool				operator == (const MStoreVect &other) const;
	bool				operator != (const MStoreVect &other) const;

};	// class MStoreVect



//#include	"MStoreVect.hpp"



#endif	// MStoreVect_HEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
