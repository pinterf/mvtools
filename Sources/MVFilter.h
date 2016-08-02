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


#ifndef	__MV_MVFilter__
#define	__MV_MVFilter__



class IScriptEnvironment;
class PClip;

class MVClip;



class MVFilter
{
protected:
	/*! \brief Number of blocks horizontaly, at the first level */
	int nBlkX;

	/*! \brief Number of blocks verticaly, at the first level */
	int nBlkY;

	/*! \brief Number of blocks at the first level */
	int nBlkCount;

	/*! \brief Number of blocks at the first level */
	int nBlkSizeX;

	int nBlkSizeY;

   /*! \brief Horizontal padding */
   int nHPadding;

   /*! \brief Vertical padding */
   int nVPadding;

	/*! \brief Width of the frame */
	int nWidth;

	/*! \brief Height of the frame */
	int nHeight;

   /*! \brief MVFrames idx */
   int nIdx;

   /*! \brief pixel refinement of the motion estimation */
   int nPel;

   int nOverlapX;
   int nOverlapY;

   int pixelType;
   int xRatioUV; // PF
   int yRatioUV;
   int pixelsize; // PF

   /*! \brief Filter's name */
//   std::string name;
   const char * name; //v1.8 replaced std::string (why it was used?)

	MVFilter(const ::PClip &vector, const char *filterName, ::IScriptEnvironment *env, int group_len, int group_ofs);

	void CheckSimilarity(const MVClip &vector, const char *vectorName, ::IScriptEnvironment *env);
};



#endif	// __MV_MVFilter__
