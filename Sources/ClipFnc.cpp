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

#include	"avisynth.h"
#ifdef _WIN32
#include "avs/win.h"
#endif

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
void	ClipFnc::format_vector_clip (::VideoInfo &vi, bool single_line_flag, int nbr_blk_x, const char *vccs_0, int size_bytes, const char *funcname_0, IScriptEnvironment *env)
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
      env->ThrowError (
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
    env->ThrowError (
      "%s: unsupported colorspace for the vector clip.",
      funcname_0
    );
  }

  const int		bytes_per_pix = vi.BitsPerPixel () >> 3; // rgb32: 32/8 = 4
  const int		unit_size = bytes_per_pix * nbr_pix_per_group; // rgb32: 4*1=4

  if (single_line_flag)
  {
    // rgb32: (size_bytes + 4 - 1) / 4,    number of 4byte chunks
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
sad_t	ClipFnc::interpolate_thsad (sad_t thsad1, sad_t thsad2, int d, int tr)
{
  assert (thsad1 >= 0);
  assert (thsad2 >= 0);
  assert (d >= 0);
  assert (tr >= 0);
  //assert (d <= tr); PF 

  sad_t			thsad = thsad1;

  if (d > 1)
  {
    assert (tr > 1);
    const double   x    = (d - 1) * PI / (tr - 1);
    const double   lerp = (1 - cos (x)) * 0.5;
    thsad = sad_t (floor (thsad1 + lerp * (thsad2  - thsad1) + 0.5));
  }

  return (thsad);
}



bool	ClipFnc::is_ya (const ::VideoInfo &vi)
{
  // Y and A planes, no U/V. Not IsY(), but IsYUVA().
  // V13 interface function; on older cores it is resolved to false (AVS_LinkIsMissing), where YA cannot exist anyway.
  return vi.IsYA();
}



bool	ClipFnc::is_grey (const ::VideoInfo &vi)
{
  return vi.IsY() || is_ya(vi);
}



bool	ClipFnc::is_yuv_any (const ::VideoInfo &vi)
{
  return vi.IsYUV() || vi.IsYUVA();
}



bool	ClipFnc::has_alpha (const ::VideoInfo &vi)
{
  return vi.IsPlanar() && (vi.IsYUVA() || vi.IsPlanarRGBA());
}



int	ClipFnc::nbr_planes_processed (const ::VideoInfo &vi)
{
  if (vi.IsYUY2())
    return 3;
  if (is_grey(vi))
    return 1;
  return std::min(vi.NumComponents(), 3);
}



void	ClipFnc::copy_alpha (::PVideoFrame &dst, const ::PVideoFrame &src, const ::VideoInfo &vi, IScriptEnvironment *env)
{
  if (!has_alpha(vi))
    return;
  // common area only: src may be larger (e.g. a super clip frame with padding)
  env->BitBlt(
    dst->GetWritePtr(PLANAR_A), dst->GetPitch(PLANAR_A),
    src->GetReadPtr(PLANAR_A), src->GetPitch(PLANAR_A),
    std::min(dst->GetRowSize(PLANAR_A), src->GetRowSize(PLANAR_A)),
    std::min(dst->GetHeight(PLANAR_A), src->GetHeight(PLANAR_A))
  );
}



void	ClipFnc::fill_alpha_opaque (::PVideoFrame &dst, const ::VideoInfo &vi)
{
  if (!has_alpha(vi))
    return;
  uint8_t *p = dst->GetWritePtr(PLANAR_A);
  const int pitch = dst->GetPitch(PLANAR_A);
  const int width = dst->GetRowSize(PLANAR_A) / vi.ComponentSize();
  const int height = dst->GetHeight(PLANAR_A);
  const int bits = vi.BitsPerComponent();
  for (int y = 0; y < height; y++)
  {
    if (bits == 8)
      std::fill_n(p, width, uint8_t(255));
    else if (bits <= 16)
      std::fill_n(reinterpret_cast<uint16_t *>(p), width, uint16_t((1 << bits) - 1));
    else
      std::fill_n(reinterpret_cast<float *>(p), width, 1.0f);
    p += pitch;
  }
}



void	ClipFnc::copy_alpha_from (::PVideoFrame &dst, ::PClip &clip, int n, const ::VideoInfo &vi, IScriptEnvironment *env)
{
  if (!has_alpha(vi))
    return;
  ::PVideoFrame src = clip->GetFrame(n, env);
  copy_alpha(dst, src, vi, env);
}



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

