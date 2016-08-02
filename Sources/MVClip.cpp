// Author: Manao
// Copyright(c)2006 A.G.Balakhnin aka Fizick - overlap, YUY2, sharp
// See legal notice in Copying.txt for more information
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA, or visit
// http://www.gnu.org/copyleft/gpl.html .

#include "MVClip.h"

#include	<cassert>



MVClip::MVClip(const PClip &vectors, int _nSCD1, int _nSCD2, IScriptEnvironment *env, int group_len, int group_ofs)
:	GenericVideoFilter(vectors) 
,	_group_len (group_len)
,	_group_ofs (group_ofs)
,	_frame_update_flag (true)
{
	vi.num_frames = (vi.num_frames - group_ofs + group_len - 1) / group_len;
	vi.MulDivFPS (1, group_len);
/* Memo: filled up with hack:
#if !defined(_WIN64)
	vi.nchannels = reinterpret_cast <uintptr_t> (&_mad);
#else
	uintptr_t p = reinterpret_cast <uintptr_t> (&_mad);
	vi.nchannels = 0x80000000L | (int)(p >> 32);
	vi.sample_type = (int)(p & 0xffffffffUL);
#endif
*/
   // we fetch the handle on the analyze filter
   // hacked into vi.nchannels and vi.sample_type

#if !defined(_WIN64)
    MVAnalysisData *pAnalyseFilter = reinterpret_cast<MVAnalysisData *>(vi.nchannels);
#else
    uintptr_t p = (((uintptr_t)(unsigned int)vi.nchannels ^ 0x80000000) << 32) | (uintptr_t)(unsigned int)vi.sample_type;
    MVAnalysisData *pAnalyseFilter = reinterpret_cast<MVAnalysisData *>(p); 
#endif
	if (vi.nchannels >= 0 &&  vi.nchannels < 9) // seems some normal clip instead of vectors
	     env->ThrowError("MVTools: invalid vector stream");

	// 'magic' key, just to check :
	if ( pAnalyseFilter->GetMagicKey() != MVAnalysisData::MOTION_MAGIC_KEY )
      env->ThrowError("MVTools: invalid vector stream");

	update_analysis_data (*pAnalyseFilter);

   // SCD thresholds
    nSCD1 = _nSCD1;
    if (pixelsize == 2)
        nSCD1 = int(nSCD1 / 255.0 * 65535.0);
    nSCD1 = _nSCD1 * (nBlkSizeX * nBlkSizeY) / (8 * 8);
    if (pAnalyseFilter->IsChromaMotion())
        nSCD1 += nSCD1 / (xRatioUV * yRatioUV) * 2; // *2: two additional planes: UV

   nSCD2 = _nSCD2 * nBlkCount / 256;
   if (pixelsize == 2)
       nSCD2 = int(nSCD2 / 255.0 * 65535.0); // todo: check if do we need it here?

   // FakeGroupOfPlane creation
   FakeGroupOfPlanes::Create(nBlkSizeX, nBlkSizeY, nLvCount, nPel, nOverlapX, nOverlapY, xRatioUV, yRatioUV, nBlkX, nBlkY);// todo xRatioUV?
}

MVClip::~MVClip()
{
}



/* disabled in v1.11.4
void MVClip::SetVectorsNeed(bool srcluma, bool refluma, bool var,
                            bool compy, bool compu, bool compv) const
{
   int nFlags = 0;

   nFlags |= srcluma ? MOTION_CALC_SRC_LUMA        : 0;
   nFlags |= refluma ? MOTION_CALC_REF_LUMA        : 0;
   nFlags |= var     ? MOTION_CALC_VAR             : 0;
   nFlags |= compy   ? MOTION_COMPENSATE_LUMA      : 0;
   nFlags |= compu   ? MOTION_COMPENSATE_CHROMA_U  : 0;
   nFlags |= compv   ? MOTION_COMPENSATE_CHROMA_V  : 0;

   MVAnalysisData *pAnalyseFilter = reinterpret_cast<MVAnalysisData *>(vi.nchannels);
   pAnalyseFilter->SetFlags(nFlags);
}
*/



void	MVClip::update_analysis_data (const MVAnalysisData &adata)
{
	assert (&adata != 0);

   // MVAnalysisData
   nBlkSizeX   = adata.GetBlkSizeX();
   nBlkSizeY   = adata.GetBlkSizeY();
   nPel        = adata.GetPel();
   isBackward  = adata.IsBackward();
   nLvCount    = adata.GetLevelCount();
   nDeltaFrame = adata.GetDeltaFrame();
   nWidth      = adata.GetWidth();
   nHeight     = adata.GetHeight();
   nMagicKey   = adata.GetMagicKey();
   nOverlapX   = adata.GetOverlapX();
   nOverlapY   = adata.GetOverlapY();
   pixelType   = adata.GetPixelType();
   yRatioUV    = adata.GetYRatioUV(); // PFtodo: GetXRatioUV
//	sharp       = adata.GetSharp();
//	usePelClip  = adata.UsePelClip();
	nVPadding   = adata.GetVPadding();
	nHPadding   = adata.GetHPadding();
	nFlags      = adata.GetFlags();

   // MVClip
//	nVPadding   = nBlkSizeY;
//	nHPadding   = nBlkSizeX;
	nBlkX       = adata.GetBlkX();
	nBlkY       = adata.GetBlkY();
   nBlkCount   = nBlkX * nBlkY;
}



int	MVClip::get_child_frame_index (int n) const
{
	return (n * _group_len + _group_ofs);
}



::PVideoFrame __stdcall	MVClip::GetFrame (int n, IScriptEnvironment* env_ptr)
{
	const int		child_n = get_child_frame_index (n);
	::PVideoFrame	frame_ptr = child->GetFrame (child_n, env_ptr);
	if (_frame_update_flag)
	{
		const BYTE *		frame_data_ptr = frame_ptr->GetReadPtr ();
		const MVAnalysisData &	ana_data =
			*reinterpret_cast <const MVAnalysisData *> (frame_data_ptr + sizeof (int));
		if (ana_data.nMagicKey != MVAnalysisData::MOTION_MAGIC_KEY)
		{
			env_ptr->ThrowError("MVTools: invalid vector stream");
		}
		if (ana_data.nVersion != MVAnalysisData::VERSION)
		{
			env_ptr->ThrowError("MVTools: incompatible version of vector stream");
		}
		update_analysis_data (ana_data);

		_frame_update_flag = false;
	}

	return (frame_ptr);
}



bool __stdcall	MVClip::GetParity (int n)
{
	const int		child_n = get_child_frame_index (n);

	return (child->GetParity (child_n));
}



void MVClip::Update (::PVideoFrame &fn, ::IScriptEnvironment *env)
{
	assert (&fn != 0);
	assert (env != 0);

	const int		bytes_per_pix = vi.BitsPerPixel () >> 3;
	const int		line_size = vi.width * bytes_per_pix;	// in bytes
	int				data_size = vi.height * line_size / sizeof(int);	// in 32-bit words

	const int *pMv = reinterpret_cast<const int*>(fn->GetReadPtr());
	if (vi.height > 1 && fn->GetPitch () != line_size)
	{
		env->ThrowError("MVTools: width and pitch are not equal in this multi-line vector clip");
	}
	int header_size = pMv[0];
	int nMagicKey1 = pMv[1];
	if (nMagicKey1 != MVAnalysisData::MOTION_MAGIC_KEY)
	{
		env->ThrowError("MVTools: invalid vector stream");
	}
	int nVersion1 = pMv[2];
	if (nVersion1 != MVAnalysisData::VERSION)
	{
		env->ThrowError("MVTools: incompatible version of vector stream");
	}

	const int		hs_i32 = header_size / sizeof(int);
	pMv       += hs_i32;									// go to data - v1.8.1
	data_size -= hs_i32;
	const bool		ok_flag = FakeGroupOfPlanes::Update(pMv, data_size);	// fixed a bug with lost frames
	if (! ok_flag)
	{
		env->ThrowError("MVTools: vector clip is too small (corrupted?)");
	}
}



// usable_flag is an input and output variable, it must be initialised
// before calling the function.
void	MVClip::use_ref_frame (int &ref_index, bool &usable_flag, ::PClip &super, int n, ::IScriptEnvironment *env_ptr)
{
	if (usable_flag)
	{
		int				off = GetDeltaFrame ();
		if (off > 0)
		{
			off *= (IsBackward ()) ? 1 : -1;
			ref_index = n + off;
		}
		else
		{
			ref_index = -off;
		}

		const ::VideoInfo &	vi_super = super->GetVideoInfo ();
		if (ref_index < 0 || ref_index >= vi_super.num_frames)
		{
			usable_flag = false;
		}
	}
}

void	MVClip::use_ref_frame (::PVideoFrame &ref, bool &usable_flag, ::PClip &super, int n, ::IScriptEnvironment *env_ptr)
{
	int				ref_index;
	use_ref_frame (ref_index, usable_flag, super, n, env_ptr);
	if (usable_flag)
	{
		ref = super->GetFrame (ref_index, env_ptr);
	}
}



bool  MVClip::IsUsable(int nSCD1_, int nSCD2_) const
{
   return (!FakeGroupOfPlanes::IsSceneChange(nSCD1_, nSCD2_)) && FakeGroupOfPlanes::IsValid();
}
