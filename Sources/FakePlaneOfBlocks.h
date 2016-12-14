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

#ifndef	__MV_FakePlaneOfBlocks__
#define	__MV_FakePlaneOfBlocks__



#include	"FakeBlockData.h"



class FakePlaneOfBlocks
{

	int nWidth_Bi;
	int nHeight_Bi;
	int nBlkX;
	int nBlkY;
	int nBlkSizeX;
	int nBlkSizeY;
	int nBlkCount;
	int nPel;
	int nLogPel;
	int nScale;
	int nLogScale;
	int nOverlapX;
	int nOverlapY;

	FakeBlockData *blocks;

public :

	FakePlaneOfBlocks(int sizex,  int sizey, int lv, int pel, int overlapx, int overlapy, int nBlkX, int nBlkY);
	~FakePlaneOfBlocks();

	void Update(const int *array);
	bool IsSceneChange(sad_t nTh1, int nTh2) const;

	__forceinline bool IsInFrame(int i) const
	{
		return (( i >= 0 ) && ( i < nBlkCount ));
	}

	__forceinline const FakeBlockData& operator[](const int i) const {
		return (blocks[i]);
	}

	__forceinline int GetBlockCount() const { return nBlkCount; }
  __forceinline int GetReducedWidth() const { return nBlkX; }
  __forceinline int GetReducedHeight() const { return nBlkY; }
  __forceinline int GetWidth() const { return nWidth_Bi; }
  __forceinline int GetHeight() const { return nHeight_Bi; }
  __forceinline int GetScaleLevel() const { return nLogScale; }
  __forceinline int GetEffectiveScale() const { return nScale; }
  __forceinline int GetBlockSizeX() const { return nBlkSizeX; }
  __forceinline int GetBlockSizeY() const { return nBlkSizeY; }
  __forceinline int GetPel() const { return nPel; }
  __forceinline const FakeBlockData& GetBlock(int i) const { return (blocks[i]); }
  __forceinline int GetOverlapX() const { return nOverlapX; }
  __forceinline int GetOverlapY() const { return nOverlapY; }
};



#endif	// __MV_FakePlaneOfBlocks__
