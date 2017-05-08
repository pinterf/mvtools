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


#ifndef	__MV_MVAnalysisData__
#define	__MV_MVAnalysisData__



#include	"AnaFlags.h"
#include "def.h"



#pragma pack (push, 16)

class MVAnalysisData
{
public:

	enum
	{
		VERSION          = 5,
		MOTION_MAGIC_KEY = 0x564D,	//'MV' is IMHO better 31415926 :)

		// Additional header for storage
		STORE_KEY        = 0xBEAD,
		STORE_VERSION    = 5
	};

   /*! \brief Unique identifier, not very useful */
   int nMagicKey; // placed to head in v.1.2.6

   int nVersion; // MVAnalysisData and outfile format version - added in v1.2.6

   /*! \brief size of a block, in pixel */
   int nBlkSizeX; // horizontal block size

	int nBlkSizeY; // vertical block size - v1.7

   /*! \brief pixel refinement of the motion estimation */
   int nPel;

   /*! \brief number of level for the hierarchal search */
   int nLvCount;

	// Important note: nDeltaFrame and isBackward fields may not be reliable
	// when fetched from the audio-hack structure, because the motion vectors
	// may have been generated in MAnalyse multi mode. Always waits for the
	// first GetFrame() and get the information from the frame content, which
	// should be exact.

   /*! \brief difference between the index of the reference and the index of the current frame */
	// If nDeltaFrame <= 0, the reference frame is the absolute value of nDeltaFrame.
	// Only a few functions accept negative nDeltaFrames.
   int nDeltaFrame;

   /*! \brief direction of the search ( forward / backward ) */
	bool isBackward;

   /*! \brief diverse flags to set up the search */
   int nFlags;	// AnaFlags

	/*! \brief Width of the frame */
	int nWidth;

	/*! \brief Height of the frame */
	int nHeight;

	int nOverlapX; // overlap block size - v1.1

	int nOverlapY; // vertical overlap - v1.7

   int nBlkX; // number of blocks along X

   int nBlkY; // number of blocks along Y

   int pixelType; // color format

   int yRatioUV; // ratio of luma plane height to chroma plane height
   int xRatioUV; // ratio of luma plane height to chroma plane width (fixed to 2 for YV12 and YUY2) PF used!

   int chromaSADScale; // P.F. chroma SAD ratio, 0:stay(YV12) 1:div2 2:div4(e.g.YV24)

   int pixelsize; // PF
   int bits_per_pixel;

//	int sharp; // pel2 interpolation type

//	bool usePelClip; // use extra clip with upsized 2x frame size

	int nHPadding; // Horizontal padding - v1.8.1

	int nVPadding; // Vertical padding - v1.8.1


public :

   MV_FORCEINLINE void SetFlags(int _nFlags) { nFlags |= _nFlags; }
   MV_FORCEINLINE int GetFlags() const { return nFlags; }
   MV_FORCEINLINE bool IsChromaMotion() const { return (nFlags & MOTION_USE_CHROMA_MOTION) != 0; }
   MV_FORCEINLINE int GetBlkSizeX() const { return nBlkSizeX; }
   MV_FORCEINLINE int GetPel() const { return nPel; }
   MV_FORCEINLINE int GetLevelCount() const { return nLvCount; }
   MV_FORCEINLINE bool IsBackward() const { return isBackward; }
   MV_FORCEINLINE int GetMagicKey() const { return nMagicKey; }
   MV_FORCEINLINE int GetDeltaFrame() const { return nDeltaFrame; }
   MV_FORCEINLINE int GetWidth() const { return nWidth; }
   MV_FORCEINLINE int GetHeight() const { return nHeight; }
   MV_FORCEINLINE int GetOverlapX() const { return nOverlapX; }
   MV_FORCEINLINE int GetBlkX() const { return nBlkX; }
   MV_FORCEINLINE int GetBlkY() const { return nBlkY; }
   MV_FORCEINLINE int GetPixelType() const { return pixelType; }
   MV_FORCEINLINE int GetYRatioUV() const { return yRatioUV; }
   MV_FORCEINLINE int GetXRatioUV() const { return xRatioUV; }
   MV_FORCEINLINE int GetChromaSADScale() const { return chromaSADScale; }
   MV_FORCEINLINE int GetPixelSize() const { return pixelsize; } // PF
   MV_FORCEINLINE int GetBitsPerPixel() const { return bits_per_pixel; } // PF
//	MV_FORCEINLINE int GetSharp() const { return sharp; }
//	MV_FORCEINLINE bool UsePelClip() const { return usePelClip; }
   MV_FORCEINLINE int GetBlkSizeY() const { return nBlkSizeY; }
   MV_FORCEINLINE int GetOverlapY() const { return nOverlapY; }
   MV_FORCEINLINE int GetHPadding() const { return nHPadding; }
   MV_FORCEINLINE int GetVPadding() const { return nVPadding; }

};

#pragma pack (pop)



#endif	// __MV_MVAnalysisData__
