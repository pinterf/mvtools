/*****************************************************************************

        MRestoreVect.cpp
        Authors: Laurent de Soras, Vit, 2011

*Tab=3***********************************************************************/



#if defined (_MSC_VER)
  #pragma warning (1 : 4130 4223 4705 4706)
  #pragma warning (4 : 4355 4786 4800)
#endif



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#include	"ClipFnc.h"
#include	"MRestoreVect.h"

#include	<algorithm>

#include	<cassert>



/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



MRestoreVect::MRestoreVect (::PClip src, int clip_index, IScriptEnvironment *env)
:	::GenericVideoFilter (src)
,	_mad ()
,	_clip_index (clip_index)
,	_bytes_per_pix (0)
,	_row_size (0)
,	_available_size (0)
{
  has_at_least_v8 = true;
  try { env->CheckVersion(8); }
  catch (const AvisynthError&) { has_at_least_v8 = false; }

  if (_clip_index < 0)
  {
    env->ThrowError ("MRestoreVect: invalid index value.");
  }

  _bytes_per_pix  = vi.BitsPerPixel () >> 3;
  _row_size       = vi.width  * _bytes_per_pix;
  _available_size = vi.height * _row_size;
  if (_available_size < 4 * sizeof (int32_t))
  {
    env->ThrowError ("MRestoreVect: frame too small to contain valid data.");
  }

  ::PVideoFrame	frame_ptr = src->GetFrame (0, env);
  int				data_offset     = 0;		// Bytes
  int				data_len        = 0;		// Bytes
  read_frame_info (data_offset, data_len, frame_ptr, env);

  const uint8_t*	data_ptr = frame_ptr->GetReadPtr ();
  const int		pitch    = frame_ptr->GetPitch ();
  int				pos      = data_offset + sizeof (int32_t);
  read_from_clip (pos, data_ptr, &_mad, sizeof (_mad), pitch);
  if (_mad.GetMagicKey () != MVAnalysisData::MOTION_MAGIC_KEY)
  {
    env->ThrowError ("MRestoreVect: unexpected or corrupted contained data.");
  }

  ClipFnc::format_vector_clip (
    vi, true, _mad.nBlkX, 0, data_len, "MRestoreVect", env
  );
  CHECK_COMPILE_TIME (SizeOfIntPtr, (sizeof (int) <= sizeof (void *)));
#if !defined(MV_64BIT)
  vi.nchannels = reinterpret_cast <uintptr_t> (&_mad);
#else
  // hack!
  uintptr_t p = reinterpret_cast <uintptr_t> (&_mad);
  vi.nchannels = 0x80000000L | (int)(p >> 32);
  vi.sample_type = (int)(p & 0xffffffffUL);
#endif
}



::PVideoFrame __stdcall	MRestoreVect::GetFrame (int n, ::IScriptEnvironment *env_ptr)
{
  assert (n >= 0);
  assert (n < vi.num_frames);
  assert (env_ptr != 0);

  ::PVideoFrame	dst_ptr;
  ::PVideoFrame	src_ptr = child->GetFrame (n, env_ptr);

  int				data_offset     = 0;		// Bytes
  int				data_len        = 0;		// Bytes
  read_frame_info (data_offset, data_len, src_ptr, env_ptr);
  if (data_len > vi.width * vi.height * _bytes_per_pix)
  {
    env_ptr->ThrowError ("MRestoreVect: invalid content at frame %d.", n);
  }

  dst_ptr = has_at_least_v8 ? env_ptr->NewVideoFrameP(vi, &src_ptr) : env_ptr->NewVideoFrame(vi); // frame property support

  const uint8_t*	src_data_ptr = src_ptr->GetReadPtr ();
  const int		src_pitch    = src_ptr->GetPitch ();
  uint8_t *		dst_data_ptr = dst_ptr->GetWritePtr ();
  const int		dst_pitch    = dst_ptr->GetPitch ();
  assert (vi.height == 1 || dst_pitch == vi.width * _bytes_per_pix);
  read_from_clip (data_offset, src_data_ptr, dst_data_ptr, data_len, src_pitch);

  assert (dst_ptr != 0);

  return (dst_ptr);
}



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



// data_offset and data_len are in bytes
void	MRestoreVect::read_frame_info (int &data_offset, int &data_len, ::PVideoFrame frame_ptr, ::IScriptEnvironment *env) const
{
  assert (&data_offset != 0);
  assert (frame_ptr != 0);
  assert (&env != 0);

  const uint8_t*	data_ptr = frame_ptr->GetReadPtr ();
  const int		pitch    = frame_ptr->GetPitch ();
  int				pos      = 0;

  int32_t			key;
  int32_t			ver;
  int32_t			nbr_clips;
  read_from_clip (pos, data_ptr, &key, sizeof (key), pitch);
  read_from_clip (pos, data_ptr, &ver, sizeof (ver), pitch);
  read_from_clip (pos, data_ptr, &nbr_clips, sizeof (nbr_clips), pitch);

  if (key != MVAnalysisData::STORE_KEY)
  {
    env->ThrowError ("MRestoreVect: clip does not wrap motion vector data.");
  }
  if (ver != MVAnalysisData::STORE_VERSION)
  {
    env->ThrowError (
      "MRestoreVect: unsupported version (%d) of the motion vector wrapper.",
      ver
    );
  }

  if (_clip_index >= nbr_clips)
  {
    env->ThrowError ("MRestoreVect: clip not found (invalid index value).");
  }
  pos += _clip_index * sizeof (int32_t);

  int32_t			offset_beg;
  int32_t			offset_end;
  read_from_clip (pos, data_ptr, &offset_beg, sizeof (offset_beg), pitch);
  read_from_clip (pos, data_ptr, &offset_end, sizeof (offset_end), pitch);
  data_offset =               offset_beg  * sizeof (int32_t);
  data_len    = (offset_end - offset_beg) * sizeof (int32_t);
  if (   data_offset            <  pos
      || data_len               <= sizeof (int32_t) + sizeof (_mad)
      || data_offset + data_len >= _available_size)
  {
    env->ThrowError ("MRestoreVect: corrupted data.");
  }

}



void	MRestoreVect::read_from_clip (int &src_pos, const uint8_t base_ptr [], void *dst_ptr, int len, int stride) const
{
  assert (src_pos >= 0);
  assert (src_pos + len <= _available_size);
  assert (base_ptr != 0);
  assert (dst_ptr != 0);
  assert (len > 0);
  assert (stride > 0);

  if (stride == _row_size)
  {
    memcpy (dst_ptr, base_ptr + src_pos, len);
    src_pos += len;
  }
  else
  {
    int				dst_pos  = 0;
    while (dst_pos < len)
    {
      const int		y = src_pos / _row_size;
      const int		x = src_pos - y * _row_size;	// In bytes
      const int		work_len = std::min (len - dst_pos, _row_size - x);
      memcpy (
        reinterpret_cast <uint8_t *> (dst_ptr) + dst_pos,
        base_ptr + y * stride + x,
        work_len
      );
      dst_pos += work_len;
      src_pos += work_len;
    }
  }
}



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
