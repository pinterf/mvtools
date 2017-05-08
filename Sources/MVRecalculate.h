// Make a motion compensate temporal denoiser

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

#ifndef __MV_RECALCULATE__
#define __MV_RECALCULATE__

#define	NOGDI
#define	NOMINMAX
#define	WIN32_LEAN_AND_MEAN

#include	"conc/ObjPool.h"
#include "commonfunctions.h"
#include "DCTFactory.h"
#include "GroupOfPlanes.h"
#include "MVAnalysisData.h"
#include "yuy2planes.h"
#include	"SharedPtr.h"

#include	<memory>
#include	<vector>



class MVGroupOfFrames;

class MVRecalculate
:	public GenericVideoFilter
{
protected:

	class SrcRefData
	{
	public:
	   MVAnalysisData _analysis_data;
	   MVAnalysisData _analysis_data_divided;

		SharedPtr <MVClip>
		               _clip_sptr;
	};
	typedef	std::vector <SrcRefData>	SrcRefArray;

	SrcRefArray    _srd_arr;

	/*! \brief Frames of blocks for which motion vectors will be computed */
	std::auto_ptr <GroupOfPlanes>
	               _vectorfields_aptr;	// Temporary data, structure initialised once.

   /*! \brief isse optimisations enabled */
	bool           isse;

   /*! \brief motion vecteur cost factor */
   int            nLambda;

   /*! \brief search type chosen for refinement in the EPZ */
   SearchType     searchType;

   /*! \brief additionnal parameter for this search */
	int            nSearchParam;  // usually search radius

//	int            nPelSearch;    // search radius at finest level

	sad_t            lsad;          // SAD limit for lambda using - added by Fizick
	int            pnew;          // penalty to cost for new canditate - added by Fizick
	int            plen;          // penalty factor (similar to lambda) for vector length - added by Fizick
	int            plevel;        // penalty factors (lambda, plen) level scaling - added by Fizick
//	bool           global;        // use global motion predictor
	const char *   outfilename;   // vectors output file
//	PClip          pelclip;       // upsized source clip with doubled frame width and heigth (used for pel=2)
	int            divideExtra;   // divide blocks on sublocks with median motion
	int            smooth;        // smooth vector interpolation or by nearest neighbors
	bool           meander;       //meander (alternate) scan blocks (even row left to right, odd row right to left

	FILE *         outfile;
	short *        outfilebuf;

	YUY2Planes *   SrcPlanes;
	YUY2Planes *   RefPlanes;
	YUY2Planes *   Src2xPlanes;
	YUY2Planes *   Ref2xPlanes;

	std::auto_ptr <DCTFactory>
	               _dct_factory_ptr;	// Not instantiated if not needed
	conc::ObjPool <DCTClass>
	               _dct_pool;

	int headerSize;

	sad_t            thSAD;

	MVGroupOfFrames *             //v2.0
	               pSrcGOF;
	MVGroupOfFrames *
	               pRefGOF;
	int            nModeYUV;

	int            _nbr_srd;
	bool           _mt_flag;

    int pixelsize; // PF
    int bits_per_pixel;

public :

	MVRecalculate (
		PClip _super, PClip _vectors, sad_t thSAD, int smooth,
		int _blksizex, int _blksizey, int st, int stp, int lambda, bool chroma,
		int _pnew, int _overlapx, int _overlapy, const char* _outfilename,
		int _dctmode, int _divide, int _sadx264, bool _isse, bool _meander,
		int trad, bool mt_flag, int _chromaSADScale, IScriptEnvironment* env
	);
	~MVRecalculate();

	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);

  int __stdcall SetCacheHints(int cachehints, int frame_range) override {
    return cachehints == CACHE_GET_MTMODE ? MT_MULTI_INSTANCE : 0;
  }

private:

	void				load_src_frame (MVGroupOfFrames &gof, ::PVideoFrame &src, const MVAnalysisData &ana_data);

};

#endif
