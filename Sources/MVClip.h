// See legal notice in Copying.txt for more information

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


#ifndef	__MV_MVClip__
#define	__MV_MVClip__



#define	NOMINMAX
#define	WIN32_LEAN_AND_MEAN
#define	NOGDI
#include	"Windows.h"
#include	"avisynth.h"

#include	"FakeGroupOfPlanes.h"
#include	"FakePlaneOfBlocks.h"
#include	"MVAnalysisData.h"



class MVClip
:	public ::GenericVideoFilter
,	public FakeGroupOfPlanes
,	public MVAnalysisData
{
	/*! \brief Number of blocks at the first level */
	int nBlkCount;

   /*! \brief Horizontal padding */
   int nHPadding;

   /*! \brief Vertical padding */
   int nVPadding;

   /*! \brief First Scene Change Detection threshold ( compared against SAD value of the block ) */
   sad_t nSCD1;

   /*! \brief Second Scene Change Detection threshold ( compared against the number of block over the first threshold */
   int nSCD2;

   int nHeaderSize; // offset to data

	int				_group_len;
	int				_group_ofs;
	bool				_frame_update_flag;

public :
	MVClip(const ::PClip &vectors, sad_t nSCD1, int nSCD2, ::IScriptEnvironment *env, int group_len, int group_ofs);
   ~MVClip();

	int				get_child_frame_index (int n) const;
	void				update_analysis_data (const MVAnalysisData &adata);

	::PVideoFrame __stdcall GetFrame (int n, IScriptEnvironment* env_ptr);
	bool __stdcall	GetParity (int n);

//   void SetVectorsNeed(bool srcluma, bool refluma, bool var,
//                       bool compy, bool compu, bool compv) const;

	void				Update (::PVideoFrame &fn, ::IScriptEnvironment *env); // v1.4.13
	void				use_ref_frame (int &ref_index, bool &usable_flag, ::PClip &super, int n, ::IScriptEnvironment *env_ptr);
	void				use_ref_frame (::PVideoFrame &ref, bool &usable_flag, ::PClip &super, int n, ::IScriptEnvironment *env_ptr);

   // encapsulation
   MV_FORCEINLINE int GetBlkCount() const { return nBlkCount; }
   MV_FORCEINLINE int GetHPadding() const { return nHPadding; }
   MV_FORCEINLINE int GetVPadding() const { return nVPadding; }
   MV_FORCEINLINE sad_t GetThSCD1() const { return nSCD1; }
   MV_FORCEINLINE int GetThSCD2() const { return nSCD2; }
   MV_FORCEINLINE const FakeBlockData& GetBlock(int nLevel, int nBlk) const { return GetPlane(nLevel)[nBlk]; }
   bool IsUsable(sad_t nSCD1_, int nSCD2_) const;
   bool IsUsable() const { return IsUsable(nSCD1, nSCD2); }
   bool IsSceneChange() const { return FakeGroupOfPlanes::IsSceneChange(nSCD1, nSCD2); }
};



#endif	// __MV_MVClip__

