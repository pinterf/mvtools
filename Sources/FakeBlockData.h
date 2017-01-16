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

#ifndef	__MV_FakeBlockData__
#define	__MV_FakeBlockData__



#include	"VECTOR.h"
#include "def.h"



class FakeBlockData
{
	int x;
	int y;
	VECTOR vector;
//	int nSad;
//	int nLength;
//	int nVariance;
//	int nLuma;
//	int nRefLuma;
//	int nPitch;

//	const unsigned char *pRef;

//	inline static int SquareLength(const VECTOR& v)
//	{ return v.x * v.x + v.y * v.y; }

public :
    FakeBlockData();
    FakeBlockData(int _x, int _y);
    ~FakeBlockData();

    void Init(int _x, int _y);
	void Update(const int *array);

  MV_FORCEINLINE int GetX() const { return x; }
  MV_FORCEINLINE int GetY() const { return y; }
  MV_FORCEINLINE VECTOR GetMV() const { return vector; }
  MV_FORCEINLINE sad_t GetSAD() const { return vector.sad; }
//	inline int GetMVLength() const { return nLength; }
//	inline int GetVariance() const { return nVariance; }
//	inline int GetLuma() const { return nLuma; }
//	inline int GetRefLuma() const { return nRefLuma; }
//	inline const unsigned char *GetRef() const { return pRef; }
//	inline int GetPitch() const { return nPitch; }
};



#endif	// __MV_FakeBlockData__
