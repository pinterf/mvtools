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

   int xRatioUV; // ratio of luma plane height to chroma plane width (fixed to 2 for YV12 and YUY2)

//	int sharp; // pel2 interpolation type

//	bool usePelClip; // use extra clip with upsized 2x frame size

	int nHPadding; // Horizontal padding - v1.8.1

	int nVPadding; // Vertical padding - v1.8.1


public :

   inline void SetFlags(int _nFlags) { nFlags |= _nFlags; }
   inline int GetFlags() const { return nFlags; }
   inline bool IsChromaMotion() const { return (nFlags & MOTION_USE_CHROMA_MOTION) != 0; }
   inline int GetBlkSizeX() const { return nBlkSizeX; }
   inline int GetPel() const { return nPel; }
   inline int GetLevelCount() const { return nLvCount; }
   inline bool IsBackward() const { return isBackward; }
   inline int GetMagicKey() const { return nMagicKey; }
   inline int GetDeltaFrame() const { return nDeltaFrame; }
   inline int GetWidth() const { return nWidth; }
   inline int GetHeight() const { return nHeight; }
   inline int GetOverlapX() const { return nOverlapX; }
   inline int GetBlkX() const { return nBlkX; }
   inline int GetBlkY() const { return nBlkY; }
   inline int GetPixelType() const { return pixelType; }
   inline int GetYRatioUV() const { return yRatioUV; }
   inline int GetXRatioUV() const { return xRatioUV; }
//	inline int GetSharp() const { return sharp; }
//	inline bool UsePelClip() const { return usePelClip; }
   inline int GetBlkSizeY() const { return nBlkSizeY; }
   inline int GetOverlapY() const { return nOverlapY; }
   inline int GetHPadding() const { return nHPadding; }
   inline int GetVPadding() const { return nVPadding; }

};

#pragma pack (pop)



#endif	// __MV_MVAnalysisData__
