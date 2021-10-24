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



#ifndef	__MV_SearchType__
#define	__MV_SearchType__



/*! \brief Search type : defines the algorithm used for minimizing the SAD */
enum SearchType
{
  ONETIME     = 1,
  NSTEP       = 2,
  LOGARITHMIC = 4,
  EXHAUSTIVE  = 8,
  HEX2SEARCH  = 16,   // v.2
  UMHSEARCH   = 32,   // v.2
  HSEARCH     = 64,   // v.2.5.11
  VSEARCH     = 128   // v.2.5.11
};



#endif	// __MV_SearchType__
