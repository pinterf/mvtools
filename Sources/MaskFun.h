// Create an overlay mask with the motion vectors

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

#ifndef __MASKFUN__
#define __MASKFUN__



#include	"types.h"
#include <stdint.h>


class MVClip;

void CheckAndPadSmallY(short *VXSmallY, short *VYSmallY, int nBlkXP, int nBlkYP, int nBlkX, int nBlkY);
void CheckAndPadSmallY_BF(short *VXSmallYB, short *VXSmallYF, short *VYSmallYB, short *VYSmallYF, int nBlkXP, int nBlkYP, int nBlkX, int nBlkY);
void CheckAndPadMaskSmall(BYTE *MaskSmall, int nBlkXP, int nBlkYP, int nBlkX, int nBlkY);
void CheckAndPadMaskSmall_BF(BYTE *MaskSmallB, BYTE *MaskSmallF, int nBlkXP, int nBlkYP, int nBlkX, int nBlkY);

void MakeVectorOcclusionMaskTime(MVClip &mvClip, int nBlkX, int nBlkY, double dMaskNormDivider, double fGamma, int nPel, uint8_t * occMask, int occMaskPitch, int time256, int nBlkStepX, int nBlkStepY);

// not in 2.5.11.22
/*
template <class OP>
void MakeVectorOcclusionMaskTimePlane (MVClip &mvClip, int nBlkX, int nBlkY, double dMaskNormFactor, int nPel, uint8_t * occMask, int occMaskPitch, const uint8_t *pt256, int t256_pitch, int blkSizeX, int blkSizeY);
*/
// not in 2.5.11.22 void VectorMasksToOcclusionMaskTime(uint8_t *VXMask, uint8_t *VYMask, int nBlkX, int nBlkY, double dMaskNormFactor, double fGamma, int nPel, uint8_t * occMask, int occMaskPitch, int time256, int blkSizeX, int blkSizeY);

// not in 2.5.11.22 void MakeVectorOcclusionMask(MVClip &mvClip, int nBlkX, int nBlkY, double dMaskNormFactor, double fGamma, int nPel, uint8_t * occMask, int occMaskPitch);

// not in 2.5.11.22 void VectorMasksToOcclusionMask(uint8_t *VX, uint8_t *VY, int nBlkX, int nBlkY, double fMaskNormFactor, double fGamma, int nPel, uint8_t * smallMask);
// new in 2.5.11.22:
void MakeSADMaskTime(MVClip &mvClip, int nBlkX, int nBlkY, double dMaskNormDivider, double fGamma, int nPel, uint8_t * Mask, int MaskPitch, int time256, int nBlkStepX, int nBlkStepY);
// in 2.5.11.22 BYTE * (uint_8*) -> short *
void MakeVectorSmallMasks(MVClip &mvClip, int nX, int nY, short *VXSmallY, int pitchVXSmallY, short *VYSmallY, int pitchVYSmallY);
void VectorSmallMaskYToHalfUV(short * VSmallY, int nBlkX, int nBlkY, short *VSmallUV, int RatioUV);
//void MakeVectorSmallMasks(MVClip &mvClip, int nX, int nY, uint8_t *VXSmallY, int pitchVXSmallY, uint8_t *VYSmallY, int pitchVYSmallY);
//void VectorSmallMaskYToHalfUV(uint8_t * VSmallY, int nBlkX, int nBlkY, uint8_t *VSmallUV, int ratioUV);

void Merge4PlanesToBig(uint8_t *pel2Plane, int pel2Pitch, const uint8_t *pPlane0, const uint8_t *pPlane1,
            const uint8_t *pPlane2, const uint8_t * pPlane3, int width, int height, int pitch, int pixelsize, int cpuFlags);

void Merge16PlanesToBig(
  uint8_t *pel4Plane, int pel4Pitch,
  const uint8_t *pPlane0,  const uint8_t *pPlane1,  const uint8_t *pPlane2,  const uint8_t * pPlane3,
  const uint8_t *pPlane4,  const uint8_t *pPlane5,  const uint8_t *pPlane6,  const uint8_t * pPlane7,
  const uint8_t *pPlane8,  const uint8_t *pPlane9,  const uint8_t *pPlane10, const uint8_t * pPlane11,
  const uint8_t *pPlane12, const uint8_t *pPlane13, const uint8_t *pPlane14, const uint8_t * pPlane15,
  int width, int height, int pitch, int pixelsize, int cpuFlags
);

unsigned char SADToMask(sad_t sad, sad_t sadnorm1024);

// 2.6.0.5 template <class T256P>
// void Blend(uint8_t * pdst, const uint8_t * psrc, const uint8_t * pref, int height, int width, int dst_pitch, int src_pitch, int ref_pitch, T256P &t256_provider, bool isse);
// back to 2.5.11.22
// 8+ bit support pixel_t template PF 16115
template<typename pixel_t>
void Blend(uint8_t * pdst8, const uint8_t * psrc8, const uint8_t * pref8, int height, int width, int dst_pitch, int src_pitch, int ref_pitch, int time256, int cpuFlags);
// in *.hpp


// lookup table size 256
void Create_LUTV(int time256, short *LUTVB, short *LUTVF);

//template <class T256P>
template<typename pixel_t>
  void FlowInterSimple(uint8_t * pdst, int dst_pitch, const uint8_t *prefB, const uint8_t *prefF, int ref_pitch,
  short *VXFullB, short *VXFullF, short *VYFullB, short *VYFullF, uint8_t *MaskB, uint8_t *MaskF,
  int VPitch, int width, int height, int time256 /*T256P &t256_provider*/, int nPel );

//template <class T256P>
template<typename pixel_t>
  void FlowInter(uint8_t * pdst, int dst_pitch, const uint8_t *prefB, const uint8_t *prefF, int ref_pitch,
  short *VXFullB, short *VXFullF, short *VYFullB, short *VYFullF, uint8_t *MaskB, uint8_t *MaskF,
  int VPitch, int width, int height, int time256 /*T256P &t256_provider*/, int nPel );

//template <class T256P>
template<typename pixel_t>
  void FlowInterExtra(uint8_t * pdst, int dst_pitch, const uint8_t *prefB, const uint8_t *prefF, int ref_pitch,
  short *VXFullB, short *VXFullF, short *VYFullB, short *VYFullF, uint8_t *MaskB, uint8_t *MaskF,
  int VPitch, int width, int height, int time256 /*T256P &t256_provider*/, int nPel,
  short *VXFullBB, short *VXFullFF, short *VYFullBB, short *VYFullFF);
/* in 2 5.11.22 
void FlowInterSimple(BYTE * pdst, int dst_pitch, const BYTE *prefB, const BYTE *prefF, int ref_pitch,
  short *VXFullB, short *VXFullF, short *VYFullB, short *VYFullF, BYTE *MaskB, BYTE *MaskF,
  int VPitch, int width, int height, int time256, int nPel);

void FlowInter(BYTE * pdst, int dst_pitch, const BYTE *prefB, const BYTE *prefF, int ref_pitch,
  short *VXFullB, short *VXFullF, short *VYFullB, short *VYFullF, BYTE *MaskB, BYTE *MaskF,
  int VPitch, int width, int height, int time256, int nPel);

  void FlowInterExtra(BYTE * pdst, int dst_pitch, const BYTE *prefB, const BYTE *prefF, int ref_pitch,
  short *VXFullB, short *VXFullF, short *VYFullB, short *VYFullF, BYTE *MaskB, BYTE *MaskF,
  int VPitch, int width, int height, int time256, int nPel,
  short *VXFullBB, short *VXFullFF, short *VYFullBB, short *VYFullFF);
*/


// Template function definition
#include	"MaskFun.hpp"



#endif
