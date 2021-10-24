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

#ifndef	__MV_FakeGroupOfPlanes__
#define	__MV_FakeGroupOfPlanes__

#include "def.h"
#include "types.h"
#include <mutex>

class FakePlaneOfBlocks;

class FakeGroupOfPlanes
{
  int nLvCount_;
  bool validity;
   int nWidth_B;
   int nHeight_B;
//   int nOverlap;
   int xRatioUV_B; // PF
   int yRatioUV_B; 
  FakePlaneOfBlocks **planes;
//   const unsigned char *compensatedPlane;
//   const unsigned char *compensatedPlaneU;
//   const unsigned char *compensatedPlaneV;
  MV_FORCEINLINE static bool GetValidity(const int *array) { return (array[1] == 1); }
  std::mutex cs;

public :
   FakeGroupOfPlanes();
//	FakeGroupOfPlanes(int w, int h, int size, int lv, int pel);
  ~FakeGroupOfPlanes();
    // we need for _xRatioUV, but _yRatioUV is not used
   void Create(int _nBlkSizeX, int _nBlkSizeY, int _nLevelCount, int _nPel, int _nOverlapX, int _nOverlapY, int _xRatioUV, int _yRatioUV, int _nBlkX, int _nBlkY); 

  bool Update(const int *array, int data_size);
  bool IsSceneChange(sad_t nThSCD1, int nThSCD2) const;

  MV_FORCEINLINE const FakePlaneOfBlocks& operator[](const int i) const {
    return *(planes[i]);
  }


  MV_FORCEINLINE bool IsValid() const { return validity; }
//   inline const unsigned char *GetCompensatedPlane() const { return compensatedPlane; }
//   inline const unsigned char *GetCompensatedPlaneU() const { return compensatedPlaneU; }
//   inline const unsigned char *GetCompensatedPlaneV() const { return compensatedPlaneV; }
  MV_FORCEINLINE int GetPitch() const { return nWidth_B; }
  MV_FORCEINLINE int GetPitchUV() const { return nWidth_B / xRatioUV_B; }

  MV_FORCEINLINE const FakePlaneOfBlocks& GetPlane(int i) const { return *(planes[i]); }
};



#endif	// __MV_FakeGroupOfPlanes__
