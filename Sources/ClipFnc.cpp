/*****************************************************************************

        ClipFnc.cpp
        Author: Laurent de Soras, 2011

*Tab=3***********************************************************************/



#if defined (_MSC_VER)
	#pragma warning (1 : 4130 4223 4705 4706)
	#pragma warning (4 : 4355 4786 4800)
#endif



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#include	"commonfunctions.h"
#include	"ClipFnc.h"
#include	"def.h"
#include	"MVInterface.h"

#define	NOGDI
#define	NOMINMAX
#define	WIN32_LEAN_AND_MEAN
#include "windows.h"
#include	"avisynth.h"

#include	<algorithm>

#include	<cassert>
#include	<climits>
#include	<cmath>
#include	<cstring>



/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



// vertical shift of fields for fieldbased video at finest level
int	ClipFnc::compute_fieldshift (::PClip &clp, bool field_flag, int npel, int nsrc, int nref)
{
	assert (clp != 0);
	assert (npel > 0);
	assert (nsrc >= 0);
	assert (nsrc < clp->GetVideoInfo ().num_frames);
	assert (nref >= 0);
	assert (nref < clp->GetVideoInfo ().num_frames);

	int				fieldshift = 0;

	if (field_flag && npel > 1 && ((nsrc - nref) & 1) != 0)
	{
		const bool		parity_src = clp->GetParity (nsrc);
		const bool		parity_ref = clp->GetParity (nref);

		if (parity_src && ! parity_ref)
		{
			fieldshift = npel / 2;
		}
		else if (parity_ref && ! parity_src)
		{
			fieldshift = -(npel / 2);
		}
	}

	return (fieldshift);
}



// Function used by MAnalyse MRecalculate and MStoreVect to format the output
// vector clip with the given spec.
// vccs_0 == 0: keeps the vi colorspace.
void	ClipFnc::format_vector_clip (::VideoInfo &vi, bool single_line_flag, int nbr_blk_x, const char *vccs_0, int size_bytes, const char *funcname_0, ::IScriptEnvironment &env)
{
	assert (&vi != 0);
	assert (nbr_blk_x > 0);
	assert (size_bytes > 0);
	assert (funcname_0 != 0);
	assert (&env != 0);

	int				nbr_pix_per_group = 1;
	if (vccs_0 == 0)
	{
		if (   vi.pixel_type != VideoInfo::CS_BGR32
		    && vi.pixel_type != VideoInfo::CS_BGR24
		    && vi.pixel_type != VideoInfo::CS_YUY2)
		{
			env.ThrowError (
				"%s: unsupported colorspace for the vector clip.",
				funcname_0
			);
		}
	}
	else if (vccs_0 [0] == '\0' || _stricmp (vccs_0, "rgb32") == 0)
	{
		vi.pixel_type = VideoInfo::CS_BGR32;
	}
	else if (_stricmp (vccs_0, "rgb24") == 0)
	{
		vi.pixel_type = VideoInfo::CS_BGR24;
	}
	else if (_stricmp (vccs_0, "yuy2") == 0)
	{
		vi.pixel_type = VideoInfo::CS_YUY2;
		nbr_pix_per_group = 2;
	}
	else
	{
		env.ThrowError (
			"%s: unsupported colorspace for the vector clip.",
			funcname_0
		);
	}

	const int		bytes_per_pix = vi.BitsPerPixel () >> 3;
	const int		unit_size = bytes_per_pix * nbr_pix_per_group;

	if (single_line_flag)
	{
		const int		nbr_groups = (size_bytes + unit_size - 1) / unit_size;
		vi.width  = nbr_groups * nbr_pix_per_group;
		vi.height = 1;
	}
	else
	{
		const int		width_bytes =
			compute_mvclip_best_width (nbr_blk_x, unit_size, FRAME_ALIGN);
		vi.width  = width_bytes / bytes_per_pix;
		vi.height = (size_bytes + width_bytes - 1) / width_bytes;
	}

	vi.audio_samples_per_second = 0; //v1.8.1
}



// Returns a width in *bytes*.
// - unit_size is the size in bytes of a indivisible pixel group. For example,
// it will be 4 bytes in YUY2, or 3 bytes in RGB24.
// - align is the desired alignment, must be a power of 2 (most likely
// FRAME_ALIGN)
int	ClipFnc::compute_mvclip_best_width (int nbr_blk_x, int unit_size, int align)
{
	assert (nbr_blk_x > 0);
	assert (unit_size > 0);
	assert (align > 0);
	assert (is_pow_2 (align));

	// Grows the unit size to make it aligned
	const int		aligned_unit = int (lcm (unit_size, align));

	const int		blocksize = sizeof (int) * N_PER_BLOCK; // Bytes
	const int		line_width = nbr_blk_x * blocksize;     // Bytes

	const int		best_width = int (lcm (line_width, aligned_unit));	// Guess what

	assert (best_width % unit_size == 0);
	assert (best_width % align     == 0);

	return (best_width);
}



// d ranges from 1 to tr, or d = tr = 0.
int	ClipFnc::interpolate_thsad (int thsad1, int thsad2, int d, int tr)
{
	assert (thsad1 >= 0);
	assert (thsad2 >= 0);
	assert (d >= 0);
	assert (tr >= 0);
	//assert (d <= tr); PF 

	int			thsad = thsad1;

	if (d > 1)
	{
		assert (tr > 1);
		const double   x    = (d - 1) * PI / (tr - 1);
		const double   lerp = (1 - cos (x)) * 0.5;
		thsad = int (floor (thsad1 + lerp * (thsad2  - thsad1) + 0.5));
	}

	return (thsad);
}



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

