/*****************************************************************************

        ClipFnc.h
        Author: Laurent de Soras, 2011

Various utility functions related to avisynth clips.

*Tab=3***********************************************************************/



#if ! defined (ClipFnc_HEADER_INCLUDED)
#define	ClipFnc_HEADER_INCLUDED

#if defined (_MSC_VER)
	#pragma once
	#pragma warning (4 : 4250)
#endif



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



/*! \brief Macros for accessing easily frame pointers & pitch */
#define YRPLAN(a) ((a)->GetReadPtr(PLANAR_Y))
#define YWPLAN(a) ((a)->GetWritePtr(PLANAR_Y))
#define URPLAN(a) ((a)->GetReadPtr(PLANAR_U))
#define UWPLAN(a) ((a)->GetWritePtr(PLANAR_U))
#define VRPLAN(a) ((a)->GetReadPtr(PLANAR_V))
#define VWPLAN(a) ((a)->GetWritePtr(PLANAR_V))
#define YPITCH(a) ((a)->GetPitch(PLANAR_Y))
#define UPITCH(a) ((a)->GetPitch(PLANAR_U))
#define VPITCH(a) ((a)->GetPitch(PLANAR_V))



// avisynth.h forward declarations
class IScriptEnvironment;
class PClip;
struct VideoInfo;



class ClipFnc
{

/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

public:

	static int		compute_fieldshift (::PClip &clp, bool field_flag, int npel, int nsrc, int nref);
	static void		format_vector_clip (::VideoInfo &vi, bool single_line_flag, int nbr_blk_x, const char *vccs_0, int size_bytes, const char *funcname_0, ::IScriptEnvironment &env);
	static int		compute_mvclip_best_width (int nbr_blk_x, int unit_size, int align);
	static int		interpolate_thsad (int thsad1, int thsad2, int d, int tr);



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

protected:



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:



/*\\\ FORBIDDEN MEMBER FUNCTIONS \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:

						ClipFnc ();
						ClipFnc (const ClipFnc &other);
	virtual			~ClipFnc () {}
	ClipFnc &		operator = (const ClipFnc &other);
	bool				operator == (const ClipFnc &other) const;
	bool				operator != (const ClipFnc &other) const;

};	// class ClipFnc



//#include	"ClipFnc.hpp"



#endif	// ClipFnc_HEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
