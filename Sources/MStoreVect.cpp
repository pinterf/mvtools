/*****************************************************************************

        MStoreVect.cpp
        Authors: Laurent de Soras, Vit, 2011

*Tab=3***********************************************************************/



#if defined (_MSC_VER)
	#pragma warning (1 : 4130 4223 4705 4706)
	#pragma warning (4 : 4355 4786 4800)
#endif



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#include	"ClipFnc.h"
#include	"MStoreVect.h"

#include	<algorithm>

#include	<cassert>
#include	<cstring>



/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



MStoreVect::MStoreVect (std::vector <::PClip> clip_arr, const char *vccs_0, ::IScriptEnvironment &env)
:	GenericVideoFilter (clip_arr [0])
,	_vect_arr ()
{
	assert (! clip_arr.empty ());
	assert (&env != 0);

	const int		nbr_clips = clip_arr.size ();
	_vect_arr.resize (nbr_clips);

	int				max_blk_x   = 0;
	int				data_offset = 3 + nbr_clips + 1;
	for (int clip_cnt = 0; clip_cnt < nbr_clips; ++clip_cnt)
	{
		VectData &		vect_data   = _vect_arr [clip_cnt];
		const ::VideoInfo &	vd_vi = clip_arr [clip_cnt]->GetVideoInfo ();

		// Checks basic parameters
		if (vd_vi.num_frames != vi.num_frames)
		{
			env.ThrowError (
				"MStoreVect: all vector clips should have the same number of frames."
			);
		}

		// Checks if the clip contains vectors
		if (vd_vi.nchannels >= 0 && vd_vi.nchannels < 9)
		{
			env.ThrowError ("MStoreVect: invalid vector stream.");
		}
#if !defined(_WIN64)
		const MVAnalysisData &	mad =
			*reinterpret_cast <MVAnalysisData *> (vd_vi.nchannels);
#else
		// hack!
		uintptr_t p = (((uintptr_t)(unsigned int)vi.nchannels ^ 0x80000000) << 32) | (uintptr_t)(unsigned int)vi.sample_type;
		const MVAnalysisData &	mad = *reinterpret_cast <MVAnalysisData *> (p);
#endif
		if (mad.GetMagicKey () != MVAnalysisData::MOTION_MAGIC_KEY)
		{
			env.ThrowError ("MStoreVect: invalid vector stream.");
		}

		max_blk_x = std::max (max_blk_x, mad.nBlkX);

		// Computes the data size
		const int		data_len_pix   = vd_vi.width * vd_vi.height;
		const int		bytes_per_pix  = vd_vi.BitsPerPixel () >> 3;
		const int		data_len_bytes = data_len_pix * bytes_per_pix;
		const int		data_len       = data_len_bytes / sizeof (int32_t);

		// Done with this one
		vect_data._clip_sptr   = clip_arr [clip_cnt];
		vect_data._data_offset = data_offset;
		vect_data._data_len    = data_len;
		data_offset += data_len;
	}
	_end_offset = data_offset;
	assert (max_blk_x > 0);

	const int		total_len_bytes = _end_offset * sizeof (int32_t);
	ClipFnc::format_vector_clip (
		vi, false, max_blk_x, vccs_0, total_len_bytes, "MStoreVect", env
	);

	// Rounds to a mod-2 height to make the codec happy.
	vi.height = (vi.height + 1) & -2;
}



::PVideoFrame __stdcall	MStoreVect::GetFrame (int n, ::IScriptEnvironment *env_ptr)
{
	assert (n >= 0);
	assert (n < vi.num_frames);
	assert (env_ptr != 0);

	::PVideoFrame	dst_ptr = env_ptr->NewVideoFrame (vi);

	uint8_t *		data_ptr = dst_ptr->GetWritePtr ();
	const int		pitch    = dst_ptr->GetPitch ();
	int				dst_pos = 0;

	// Header
	static const int32_t	key = MVAnalysisData::STORE_KEY;
	static const int32_t	ver = MVAnalysisData::STORE_VERSION;
	write_to_clip (dst_pos, data_ptr, &key, sizeof (key), pitch);
	write_to_clip (dst_pos, data_ptr, &ver, sizeof (ver), pitch);
	const int32_t	nbr_clips = (int32_t)_vect_arr.size ();
	write_to_clip (dst_pos, data_ptr, &nbr_clips, sizeof (nbr_clips), pitch);

	// Clip offsets
	for (int clip_cnt = 0; clip_cnt < nbr_clips; ++clip_cnt)
	{
		const VectData &	vect_info = _vect_arr [clip_cnt];
		const int32_t	data_offset = vect_info._data_offset;
		write_to_clip (dst_pos, data_ptr, &data_offset, sizeof (data_offset), pitch);
	}
	const int32_t	end_offset = _end_offset;
	write_to_clip (dst_pos, data_ptr, &end_offset, sizeof (end_offset), pitch);

	// Data
	for (int clip_cnt = 0; clip_cnt < nbr_clips; ++clip_cnt)
	{
		const VectData &	vect_info = _vect_arr [clip_cnt];
		assert (vect_info._data_offset * sizeof (int32_t) == dst_pos);

		::PVideoFrame	clip_ptr = vect_info._clip_sptr->GetFrame (n, env_ptr);
		const uint8_t*	src_ptr = clip_ptr->GetReadPtr ();
		const int		len_bytes = vect_info._data_len * sizeof (int32_t);
		write_to_clip (dst_pos, data_ptr, src_ptr, len_bytes, pitch);
	}

	return (dst_ptr);
}



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



// dst_pos = position in bytes in the output data array (not taking the pitch into account)
// base_ptr = picture start address
// len = length in bytes of the data to copy
void	MStoreVect::write_to_clip (int &dst_pos, uint8_t base_ptr [], const void *src_ptr, int len, int stride)
{
	const int		bytes_per_pix  = vi.BitsPerPixel () >> 3;
	const int		row_size       = vi.width  * bytes_per_pix;
	const int		available_size = vi.height * row_size;
	assert (dst_pos >= 0);
	assert (dst_pos + len <= available_size);
	assert (base_ptr != 0);
	assert (len > 0);
	assert (src_ptr != 0);
	assert (stride > 0);

	if (stride == row_size)
	{
		memcpy (base_ptr + dst_pos, src_ptr, len);
		dst_pos += len;
	}
	else
	{
		int				src_pos  = 0;
		while (src_pos < len)
		{
			const int		y = dst_pos / row_size;
			const int		x = dst_pos - y * row_size;	// In bytes
			const int		work_len = std::min (len - src_pos, row_size - x);
			memcpy (
				base_ptr + y * stride + x,
				reinterpret_cast <const uint8_t *> (src_ptr) + src_pos,
				work_len
			);
			dst_pos += work_len;
			src_pos += work_len;
		}
	}
}



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
