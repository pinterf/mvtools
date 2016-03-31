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

#ifndef __GOPLANES__
#define __GOPLANES__



#include "PlaneOfBlocks.h"



class MVGroupOfFrames;

class GroupOfPlanes
{
	int            nBlkSizeX;
	int            nBlkSizeY;
	int            nLevelCount;
	int            nPel;
	int            nFlags;
	int            nOverlapX;
	int            nOverlapY;
	int            yRatioUV;
	int            divideExtra;
	bool           _mt_flag;

	conc::ObjPool <DCTClass> *
	               _dct_pool_ptr;
	PlaneOfBlocks **
	               planes;

public :
	GroupOfPlanes (
		int _nBlkSizeX, int _nBlkSizeY, int _nLevelCount, int _nPel, int _nFlags,
		int _nOverlapX, int _nOverlapY, int _nBlkX, int _nBlkY, int _yRatioUV,
		int _divideExtra, conc::ObjPool <DCTClass> *dct_pool_ptr, bool mt_flag);
	~GroupOfPlanes ();
	void           SearchMVs (
		MVGroupOfFrames *pSrcGOF, MVGroupOfFrames *pRefGOF,
		SearchType searchType, int nSearchParam, int _PelSearch, int _nLambda,
		int _lsad, int _pnew, int _plevel, bool _global, int flags, int *out,
		short * outfilebuf, int fieldShift, int _pzero, int _pglobal, int badSAD,
		int badrange, bool meander, int *vecPrev, bool tryMany);
	void           WriteDefaultToArray (int *array);
	int            GetArraySize ();
	void           ExtraDivide (int *out, int flags);
	void           RecalculateMVs (
		MVClip &mvClip, MVGroupOfFrames *pSrcGOF, MVGroupOfFrames *pRefGOF,
		SearchType _searchType, int _nSearchParam, int _nLambda, int _lsad,
		int _pnew, int flags, int *out, short * outfilebuf, int fieldShift,
		int thSAD, int smooth, bool meander);
};

#endif
