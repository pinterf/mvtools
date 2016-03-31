// Pixels flow motion interpolation function to change FPS
// Copyright(c)2005 A.G.Balakhnin aka Fizick

// See legal notice in Copying.txt for more information

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; version 2 of the License.
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

#include "ClipFnc.h"
#include "commonfunctions.h"
#include "MaskFun.h"
#include "MVFinest.h"
#include "MVFlowFps.h"
#include "profile.h"
#include "SuperParams64Bits.h"
#include "Time256ProviderCst.h"


MVFlowFps::MVFlowFps(PClip _child, PClip super, PClip _mvbw, PClip _mvfw,  unsigned int _num, unsigned int _den, int _maskmode, double _ml,
                           bool _blend, int nSCD1, int nSCD2, bool _isse, bool _planar, IScriptEnvironment* env) :
GenericVideoFilter(_child),
MVFilter(_mvfw, "MFlowFps", env, 1, 0),
mvClipB(_mvbw, nSCD1, nSCD2, env, 1, 0),
mvClipF(_mvfw, nSCD1, nSCD2, env, 1, 0)
{
	numeratorOld = vi.fps_numerator;
	denominatorOld = vi.fps_denominator;

    if (_num != 0 && _den != 0)
    {
        numerator = _num;
        denominator = _den;
    }
    else if (numeratorOld < (1<<30))
    {
        numerator = (numeratorOld<<1); // double fps by default
        denominator = denominatorOld;
    }
    else // very big numerator
    {
        numerator = numeratorOld;
        denominator = (denominatorOld>>1);// double fps by default
    }

    //  safe for big numbers since v2.1
    fa = __int64(denominator)*__int64(numeratorOld);
    fb = __int64(numerator)*__int64(denominatorOld);
    __int64 fgcd = gcd(fa, fb); // general common divisor
    fa /= fgcd;
    fb /= fgcd;

	vi.SetFPS(numerator, denominator);

	vi.num_frames = (int)(1 + __int64(vi.num_frames-1) * fb/fa );

    maskmode = _maskmode; // speed mode
   ml = _ml;
//   nIdx = _nIdx;
   isse = _isse;
   planar = _planar;
   blend = _blend;

   CheckSimilarity(mvClipB, "mvbw", env);
   CheckSimilarity(mvClipF, "mvfw", env);

   if (nWidth  != vi.width || nHeight  != vi.height )
		env->ThrowError("MFlowFps: inconsistent source and vector frame size");


	if (!((nWidth + nHPadding*2) == super->GetVideoInfo().width && (nHeight + nVPadding*2) <= super->GetVideoInfo().height))
		env->ThrowError("MFlowFps: inconsistent clips frame size!");

//	if (isSuper)
//	{
		// get parameters of prepared super clip - v2.0
		SuperParams64Bits params;
		memcpy(&params, &super->GetVideoInfo().num_audio_samples, 8);
		int nHeightS = params.nHeight;
		int nSuperHPad = params.nHPad;
		int nSuperVPad = params.nVPad;
		int nSuperPel = params.nPel;
		int nSuperModeYUV = params.nModeYUV;
		int nSuperLevels = params.nLevels;
		int nSuperWidth = super->GetVideoInfo().width; // really super
		int nSuperHeight = super->GetVideoInfo().height;
//		pRefBGOF = new MVGroupOfFrames(nSuperLevels, nWidth, nHeight, nSuperPel, nSuperHPad, nSuperVPad, nSuperModeYUV, isse, yRatioUV);
//		pRefFGOF = new MVGroupOfFrames(nSuperLevels, nWidth, nHeight, nSuperPel, nSuperHPad, nSuperVPad, nSuperModeYUV, isse, yRatioUV);

		if (   nHeight != nHeightS
		    || nWidth  != nSuperWidth - nSuperHPad * 2
		    || nPel    != nSuperPel)
		{
			env->ThrowError("MFlowFps : wrong super frame clip");
		}
//	}

	if (nPel==1)
	{
		finest = super; // v2.0.9.1
	}
	else
	{
		finest = new MVFinest(super, isse, env);
//		AVSValue finest_args[2] = { super, isse };
//		finest = env->Invoke("MVFinest", AVSValue(finest_args,2)).AsClip();
		AVSValue cache_args[1] = { finest };
		finest = env->Invoke("InternalCache", AVSValue(cache_args,1)).AsClip(); // add cache for speed
	}

	// may be padded for full frame cover
	nBlkXP = (nBlkX*(nBlkSizeX - nOverlapX) + nOverlapX < nWidth) ? nBlkX+1 : nBlkX;
	nBlkYP = (nBlkY*(nBlkSizeY - nOverlapY) + nOverlapY < nHeight) ? nBlkY+1 : nBlkY;
	nWidthP = nBlkXP*(nBlkSizeX - nOverlapX) + nOverlapX;
	nHeightP = nBlkYP*(nBlkSizeY - nOverlapY) + nOverlapY;
	// for YV12
	nWidthPUV = nWidthP/2;
	nHeightPUV = nHeightP/yRatioUV;
	nHeightUV = nHeight/yRatioUV;
	nWidthUV = nWidth/2;

	nHPaddingUV = nHPadding/2;
	nVPaddingUV = nVPadding/yRatioUV;

	VPitchY = (nWidthP + 15) & (~15);
	VPitchUV = (nWidthPUV + 15) & (~15);

	VXFullYB = new BYTE [nHeightP*VPitchY];
	VXFullUVB = new BYTE [nHeightPUV*VPitchUV];
	VYFullYB = new BYTE [nHeightP*VPitchY];
	VYFullUVB = new BYTE [nHeightPUV*VPitchUV];

	VXFullYF = new BYTE [nHeightP*VPitchY];
	VXFullUVF = new BYTE [nHeightPUV*VPitchUV];
	VYFullYF = new BYTE [nHeightP*VPitchY];
	VYFullUVF = new BYTE [nHeightPUV*VPitchUV];

	VXSmallYB = new BYTE [nBlkXP*nBlkYP];
	VYSmallYB = new BYTE [nBlkXP*nBlkYP];
	VXSmallUVB = new BYTE [nBlkXP*nBlkYP];
	VYSmallUVB = new BYTE [nBlkXP*nBlkYP];

	VXSmallYF = new BYTE [nBlkXP*nBlkYP];
	VYSmallYF = new BYTE [nBlkXP*nBlkYP];
	VXSmallUVF = new BYTE [nBlkXP*nBlkYP];
	VYSmallUVF = new BYTE [nBlkXP*nBlkYP];

	VXFullYBB = new BYTE [nHeightP*VPitchY];
	VXFullUVBB = new BYTE [nHeightPUV*VPitchUV];
	VYFullYBB = new BYTE [nHeightP*VPitchY];
	VYFullUVBB = new BYTE [nHeightPUV*VPitchUV];

	VXFullYFF = new BYTE [nHeightP*VPitchY];
	VXFullUVFF = new BYTE [nHeightPUV*VPitchUV];
	VYFullYFF = new BYTE [nHeightP*VPitchY];
	VYFullUVFF = new BYTE [nHeightPUV*VPitchUV];

	VXSmallYBB = new BYTE [nBlkXP*nBlkYP];
	VYSmallYBB = new BYTE [nBlkXP*nBlkYP];
	VXSmallUVBB = new BYTE [nBlkXP*nBlkYP];
	VYSmallUVBB = new BYTE [nBlkXP*nBlkYP];

	VXSmallYFF = new BYTE [nBlkXP*nBlkYP];
	VYSmallYFF = new BYTE [nBlkXP*nBlkYP];
	VXSmallUVFF = new BYTE [nBlkXP*nBlkYP];
	VYSmallUVFF = new BYTE [nBlkXP*nBlkYP];

	MaskSmallB = new BYTE [nBlkXP*nBlkYP];
	MaskFullYB = new BYTE [nHeightP*VPitchY];
	MaskFullUVB = new BYTE [nHeightPUV*VPitchUV];

	MaskSmallF = new BYTE [nBlkXP*nBlkYP];
	MaskFullYF = new BYTE [nHeightP*VPitchY];
	MaskFullUVF = new BYTE [nHeightPUV*VPitchUV];

	SADMaskSmallB = new BYTE [nBlkXP*nBlkYP];
	SADMaskSmallF = new BYTE [nBlkXP*nBlkYP];

	int CPUF_Resize = env->GetCPUFlags();
	if (!isse) CPUF_Resize = (CPUF_Resize & !CPUF_INTEGER_SSE) & !CPUF_SSE2;

	upsizer = new SimpleResize(nWidthP, nHeightP, nBlkXP, nBlkYP, CPUF_Resize);
	upsizerUV = new SimpleResize(nWidthPUV, nHeightPUV, nBlkXP, nBlkYP, CPUF_Resize);

	LUTVB = new short[256];
	LUTVF = new short[256];

	nleftLast = -1000;
	nrightLast = -1000;

	if ( (pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2 && !planar)
   {
//		SrcPlanes =  new YUY2Planes(nSuperWidth, nSuperHeight); // v2.0
//		RefPlanes =  new YUY2Planes(nSuperWidth, nSuperHeight);
		DstPlanes =  new YUY2Planes(nWidth, nHeight);
//		if (usePelClipHere)
//		{
//			Ref2xPlanes =  new YUY2Planes(nWidth*nPel, nHeight*nPel);
//			Src2xPlanes =  new YUY2Planes(nWidth*nPel, nHeight*nPel);
//		}
   }

}

MVFlowFps::~MVFlowFps()
{
   if ( (pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2  && !planar)
   {
//	delete SrcPlanes;
//	delete RefPlanes;
	delete DstPlanes;
//	if (usePelClipHere)
//	{
//		delete Ref2xPlanes;
//		delete Src2xPlanes;
//	}
   }

	delete upsizer;
	delete upsizerUV;

	delete [] VXFullYB;
	delete [] VXFullUVB;
	delete [] VYFullYB;
	delete [] VYFullUVB;
	delete [] VXSmallYB;
	delete [] VYSmallYB;
	delete [] VXSmallUVB;
	delete [] VYSmallUVB;
	delete [] VXFullYF;
	delete [] VXFullUVF;
	delete [] VYFullYF;
	delete [] VYFullUVF;
	delete [] VXSmallYF;
	delete [] VYSmallYF;
	delete [] VXSmallUVF;
	delete [] VYSmallUVF;

	delete [] VXFullYBB;
	delete [] VXFullUVBB;
	delete [] VYFullYBB;
	delete [] VYFullUVBB;
	delete [] VXSmallYBB;
	delete [] VYSmallYBB;
	delete [] VXSmallUVBB;
	delete [] VYSmallUVBB;
	delete [] VXFullYFF;
	delete [] VXFullUVFF;
	delete [] VYFullYFF;
	delete [] VYFullUVFF;
	delete [] VXSmallYFF;
	delete [] VYSmallYFF;
	delete [] VXSmallUVFF;
	delete [] VYSmallUVFF;

	delete [] MaskSmallB;
	delete [] MaskFullYB;
	delete [] MaskFullUVB;
	delete [] MaskSmallF;
	delete [] MaskFullYF;
	delete [] MaskFullUVF;

	delete [] SADMaskSmallB;
	delete [] SADMaskSmallF;
	 delete [] LUTVB;
	 delete [] LUTVF;

//    if (isSuper)
//    {
//       delete pRefBGOF; // v2.0
//       delete pRefFGOF;
//    }
}

//-------------------------------------------------------------------------
PVideoFrame __stdcall MVFlowFps::GetFrame(int n, IScriptEnvironment* env)
{
	_mm_empty();
 	int nleft = (int) ( __int64(n)* fa/fb );
	// intermediate product may be very large! Now I know how to multiply int64
	int time256 = int( (double(n)*double(fa)/double(fb) - nleft)*256 + 0.5);

   PVideoFrame dst;
   BYTE *pDst[3];
	const BYTE *pRef[3], *pSrc[3];
    int nDstPitches[3], nRefPitches[3], nSrcPitches[3];
	unsigned char *pDstYUY2;
	int nDstPitchYUY2;
//	const unsigned char *pRef2xY, *pRef2xU, *pRef2xV;
//	int nRef2xPitchY, nRef2xPitchUV;
//	const unsigned char *pSrc2xY, *pSrc2xU, *pSrc2xV;
//	int nSrc2xPitchY, nSrc2xPitchUV;


	int off = mvClipB.GetDeltaFrame(); // integer offset of reference frame
	if (off <= 0)
	{
		env->ThrowError ("MFlowFps: cannot use motion vectors with absolute frame references.");
	}
	// usually off must be = 1
	if (off > 1)
		time256 = time256/off;

	int nright = nleft+off;

// v2.0
	if (time256 ==0){
        dst = child->GetFrame(nleft,env); // simply left
		return dst;
	}
	else if (time256==256){
		dst = child->GetFrame(nright,env); // simply right
		return dst;
		}

	PVideoFrame mvF = mvClipF.GetFrame(nright, env);
	mvClipF.Update(mvF, env);// forward from current to next
	mvF = 0;
	PVideoFrame mvB = mvClipB.GetFrame(nleft, env);
	mvClipB.Update(mvB, env);// backward from next to current
	mvB = 0;

	// Checked here instead of the constructor to allow using multi-vector
	// clips, because the backward flag is not reliable before the vector
	// data are actually read from the frame.
	if (!mvClipB.IsBackward())
			env->ThrowError("MFlowFps: wrong backward vectors");
	if (mvClipF.IsBackward())
			env->ThrowError("MFlowFps: wrong forward vectors");

	PVideoFrame	src	= finest->GetFrame(nleft, env); // move here - v2.0
	PVideoFrame ref = finest->GetFrame(nright, env);//  right frame for  compensation

	Create_LUTV(time256, LUTVB, LUTVF); // lookup table
	const Time256ProviderCst	t256_prov_cst (time256, LUTVB, LUTVF);

//   int sharp = mvClipB.GetSharp();

		dst = env->NewVideoFrame(vi);

   if ( mvClipB.IsUsable()  && mvClipF.IsUsable())
   {

//		MVFrames *pFrames = mvCore->GetFrames(nIdx);
//         PMVGroupOfFrames pRefGOFF = pFrames->GetFrame(nleft); // forward ref
//         PMVGroupOfFrames pRefGOFB = pFrames->GetFrame(nright); // backward ref

      PROFILE_START(MOTION_PROFILE_YUY2CONVERT);

		if ( (pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2 )
		{
            // planar data packed to interleaved format (same as interleved2planar by kassandro) - v2.0.0.5
            pSrc[0] = src->GetReadPtr();
            pSrc[1] = pSrc[0] + src->GetRowSize()/2;
            pSrc[2] = pSrc[1] + src->GetRowSize()/4;
            nSrcPitches[0] = src->GetPitch();
            nSrcPitches[1] = nSrcPitches[0];
            nSrcPitches[2] = nSrcPitches[0];
            // planar data packed to interleaved format (same as interleved2planar by kassandro) - v2.0.0.5
            pRef[0] = ref->GetReadPtr();
            pRef[1] = pRef[0] + ref->GetRowSize()/2;
            pRef[2] = pRef[1] + ref->GetRowSize()/4;
            nRefPitches[0] = ref->GetPitch();
            nRefPitches[1] = nRefPitches[0];
            nRefPitches[2] = nRefPitches[0];

            if (!planar)
            {
			pDstYUY2 = dst->GetWritePtr();
			nDstPitchYUY2 = dst->GetPitch();
			pDst[0] = DstPlanes->GetPtr();
			pDst[1] = DstPlanes->GetPtrU();
			pDst[2] = DstPlanes->GetPtrV();
			nDstPitches[0]  = DstPlanes->GetPitch();
			nDstPitches[1]  = DstPlanes->GetPitchUV();
			nDstPitches[2]  = DstPlanes->GetPitchUV();
			}
			else
			{
                pDst[0] = dst->GetWritePtr();
                pDst[1] = pDst[0] + dst->GetRowSize()/2;
                pDst[2] = pDst[1] + dst->GetRowSize()/4;;
                nDstPitches[0] = dst->GetPitch();
                nDstPitches[1] = nDstPitches[0];
                nDstPitches[2] = nDstPitches[0];
			}
		}
		else
		{
         pDst[0] = YWPLAN(dst);
         pDst[1] = UWPLAN(dst);
         pDst[2] = VWPLAN(dst);
         nDstPitches[0] = YPITCH(dst);
         nDstPitches[1] = UPITCH(dst);
         nDstPitches[2] = VPITCH(dst);

         pRef[0] = YRPLAN(ref);
         pRef[1] = URPLAN(ref);
         pRef[2] = VRPLAN(ref);
         nRefPitches[0] = YPITCH(ref);
         nRefPitches[1] = UPITCH(ref);
         nRefPitches[2] = VPITCH(ref);

         pSrc[0] = YRPLAN(src);
         pSrc[1] = URPLAN(src);
         pSrc[2] = VRPLAN(src);
         nSrcPitches[0] = YPITCH(src);
         nSrcPitches[1] = UPITCH(src);
         nSrcPitches[2] = VPITCH(src);
		}

      PROFILE_STOP(MOTION_PROFILE_YUY2CONVERT);

//         MVPlane *pPlanesB[3];
//         MVPlane *pPlanesF[3];

/*    if (isSuper)
    {
            pRefBGOF->Update(YUVPLANES, (BYTE*)pRef[0], nRefPitches[0], (BYTE*)pRef[1], nRefPitches[1], (BYTE*)pRef[2], nRefPitches[2]);// v2.0
            pRefFGOF->Update(YUVPLANES, (BYTE*)pSrc[0], nSrcPitches[0], (BYTE*)pSrc[1], nSrcPitches[1], (BYTE*)pSrc[2], nSrcPitches[2]);

         pPlanesB[0] = pRefBGOF->GetFrame(0)->GetPlane(YPLANE);
         pPlanesB[1] = pRefBGOF->GetFrame(0)->GetPlane(UPLANE);
         pPlanesB[2] = pRefBGOF->GetFrame(0)->GetPlane(VPLANE);

         pPlanesF[0] = pRefFGOF->GetFrame(0)->GetPlane(YPLANE);
         pPlanesF[1] = pRefFGOF->GetFrame(0)->GetPlane(UPLANE);
         pPlanesF[2] = pRefFGOF->GetFrame(0)->GetPlane(VPLANE);
    }
*/
    int nOffsetY = nRefPitches[0] * nVPadding*nPel + nHPadding*nPel;
    int nOffsetUV = nRefPitches[1] * nVPaddingUV*nPel + nHPaddingUV*nPel;

		int dummyplane = PLANAR_Y; // use luma plane resizer code for all planes if we resize from luma small mask

		 if (nright != nrightLast)
		 {
      PROFILE_START(MOTION_PROFILE_MASK);
			// make  vector vx and vy small masks
			// 1. ATTENTION: vectors are assumed SHORT (|vx|, |vy| < 127) !
			// 2. they will be zeroed if not
			// 3. added 128 to all values
			MakeVectorSmallMasks(mvClipB, nBlkX, nBlkY, VXSmallYB, nBlkXP, VYSmallYB, nBlkXP);
			if (nBlkXP > nBlkX) // fill right
			{
				for (int j=0; j<nBlkY; j++)
				{
					VXSmallYB[j*nBlkXP + nBlkX] = std::min(VXSmallYB[j*nBlkXP + nBlkX-1], uint8_t (128));
					VYSmallYB[j*nBlkXP + nBlkX] = VYSmallYB[j*nBlkXP + nBlkX-1];
				}
			}
			if (nBlkYP > nBlkY) // fill bottom
			{
				for (int i=0; i<nBlkXP; i++)
				{
					VXSmallYB[nBlkXP*nBlkY +i] = VXSmallYB[nBlkXP*(nBlkY-1) +i];
					VYSmallYB[nBlkXP*nBlkY +i] = std::min(VYSmallYB[nBlkXP*(nBlkY-1) +i], uint8_t (128));
				}
			}
			VectorSmallMaskYToHalfUV(VXSmallYB, nBlkXP, nBlkYP, VXSmallUVB, 2);
			VectorSmallMaskYToHalfUV(VYSmallYB, nBlkXP, nBlkYP, VYSmallUVB, yRatioUV);

      PROFILE_STOP(MOTION_PROFILE_MASK);
			// upsize (bilinear interpolate) vector masks to fullframe size
      PROFILE_START(MOTION_PROFILE_RESIZE);

		  upsizer->SimpleResizeDo(VXFullYB, nWidthP, nHeightP, VPitchY, VXSmallYB, nBlkXP, nBlkXP, dummyplane);
		  upsizer->SimpleResizeDo(VYFullYB, nWidthP, nHeightP, VPitchY, VYSmallYB, nBlkXP, nBlkXP, dummyplane);
		  upsizerUV->SimpleResizeDo(VXFullUVB, nWidthPUV, nHeightPUV, VPitchUV, VXSmallUVB, nBlkXP, nBlkXP, dummyplane);
		  upsizerUV->SimpleResizeDo(VYFullUVB, nWidthPUV, nHeightPUV, VPitchUV, VYSmallUVB, nBlkXP, nBlkXP, dummyplane);
      PROFILE_STOP(MOTION_PROFILE_RESIZE);

		 }
		// analyse vectors field to detect occlusion
      PROFILE_START(MOTION_PROFILE_MASK);
//		double occNormB = (256-time256)/(256*ml);
//		MakeVectorOcclusionMask(mvClipB, nBlkX, nBlkY, occNormB, 1.0, nPel, MaskSmallB, nBlkXP);
        MakeVectorOcclusionMaskTime(mvClipB, nBlkX, nBlkY, ml, 1.0, nPel, MaskSmallB, nBlkXP, (256-time256), nBlkSizeX - nOverlapX, nBlkSizeY - nOverlapY);
		if (nBlkXP > nBlkX) // fill right
		{
			for (int j=0; j<nBlkY; j++)
			{
				MaskSmallB[j*nBlkXP + nBlkX] = MaskSmallB[j*nBlkXP + nBlkX-1];
			}
		}
		if (nBlkYP > nBlkY) // fill bottom
		{
			for (int i=0; i<nBlkXP; i++)
			{
				MaskSmallB[nBlkXP*nBlkY +i] = MaskSmallB[nBlkXP*(nBlkY-1) +i];
			}
		}
      PROFILE_STOP(MOTION_PROFILE_MASK);
      PROFILE_START(MOTION_PROFILE_RESIZE);
	  // upsize (bilinear interpolate) vector masks to fullframe size
	    upsizer->SimpleResizeDo(MaskFullYB, nWidthP, nHeightP, VPitchY, MaskSmallB, nBlkXP, nBlkXP, dummyplane);
		upsizerUV->SimpleResizeDo(MaskFullUVB, nWidthPUV, nHeightPUV, VPitchUV, MaskSmallB, nBlkXP, nBlkXP, dummyplane);
      PROFILE_STOP(MOTION_PROFILE_RESIZE);

		 nrightLast = nright;


		 if(nleft != nleftLast)
		 {
			// make  vector vx and vy small masks
			// 1. ATTENTION: vectors are assumed SHORT (|vx|, |vy| < 127) !
			// 2. they will be zeroed if not
			// 3. added 128 to all values
      PROFILE_START(MOTION_PROFILE_MASK);
			MakeVectorSmallMasks(mvClipF, nBlkX, nBlkY, VXSmallYF, nBlkXP, VYSmallYF, nBlkXP);
			if (nBlkXP > nBlkX) // fill right
			{
				for (int j=0; j<nBlkY; j++)
				{
					VXSmallYF[j*nBlkXP + nBlkX] = std::min(VXSmallYF[j*nBlkXP + nBlkX-1], uint8_t (128));
					VYSmallYF[j*nBlkXP + nBlkX] = VYSmallYF[j*nBlkXP + nBlkX-1];
				}
			}
			if (nBlkYP > nBlkY) // fill bottom
			{
				for (int i=0; i<nBlkXP; i++)
				{
					VXSmallYF[nBlkXP*nBlkY +i] = VXSmallYF[nBlkXP*(nBlkY-1) +i];
					VYSmallYF[nBlkXP*nBlkY +i] = std::min(VYSmallYF[nBlkXP*(nBlkY-1) +i], uint8_t (128));
				}
			}
			VectorSmallMaskYToHalfUV(VXSmallYF, nBlkXP, nBlkYP, VXSmallUVF, 2);
			VectorSmallMaskYToHalfUV(VYSmallYF, nBlkXP, nBlkYP, VYSmallUVF, yRatioUV);

      PROFILE_STOP(MOTION_PROFILE_MASK);
			// upsize (bilinear interpolate) vector masks to fullframe size
      PROFILE_START(MOTION_PROFILE_RESIZE);

		  upsizer->SimpleResizeDo(VXFullYF, nWidthP, nHeightP, VPitchY, VXSmallYF, nBlkXP, nBlkXP, dummyplane);
		  upsizer->SimpleResizeDo(VYFullYF, nWidthP, nHeightP, VPitchY, VYSmallYF, nBlkXP, nBlkXP, dummyplane);
		  upsizerUV->SimpleResizeDo(VXFullUVF, nWidthPUV, nHeightPUV, VPitchUV, VXSmallUVF, nBlkXP, nBlkXP, dummyplane);
		  upsizerUV->SimpleResizeDo(VYFullUVF, nWidthPUV, nHeightPUV, VPitchUV, VYSmallUVF, nBlkXP, nBlkXP, dummyplane);
      PROFILE_STOP(MOTION_PROFILE_RESIZE);

		 }
		// analyse vectors field to detect occlusion
      PROFILE_START(MOTION_PROFILE_MASK);
//		double occNormF = time256/(256*ml);
//		MakeVectorOcclusionMask(mvClipF, nBlkX, nBlkY, occNormF, 1.0, nPel, MaskSmallF, nBlkXP);
        MakeVectorOcclusionMaskTime(mvClipF, nBlkX, nBlkY, ml, 1.0, nPel, MaskSmallF, nBlkXP, time256, nBlkSizeX - nOverlapX, nBlkSizeY - nOverlapY);
		if (nBlkXP > nBlkX) // fill right
		{
			for (int j=0; j<nBlkY; j++)
			{
				MaskSmallF[j*nBlkXP + nBlkX] = MaskSmallF[j*nBlkXP + nBlkX-1];
			}
		}
		if (nBlkYP > nBlkY) // fill bottom
		{
			for (int i=0; i<nBlkXP; i++)
			{
				MaskSmallF[nBlkXP*nBlkY +i] = MaskSmallF[nBlkXP*(nBlkY-1) +i];
			}
		}
      PROFILE_STOP(MOTION_PROFILE_MASK);
      PROFILE_START(MOTION_PROFILE_RESIZE);
	  // upsize (bilinear interpolate) vector masks to fullframe size
	    upsizer->SimpleResizeDo(MaskFullYF, nWidthP, nHeightP, VPitchY, MaskSmallF, nBlkXP, nBlkXP, dummyplane);
		upsizerUV->SimpleResizeDo(MaskFullUVF, nWidthPUV, nHeightPUV, VPitchUV, MaskSmallF, nBlkXP, nBlkXP, dummyplane);
      PROFILE_STOP(MOTION_PROFILE_RESIZE);

		 nleftLast = nleft;

    // Get motion info from more frames for occlusion areas
	PVideoFrame mvFF = mvClipF.GetFrame(nleft, env);
    mvClipF.Update(mvFF, env);// forward from prev to cur
    mvFF = 0;

	PVideoFrame mvBB = mvClipB.GetFrame(nright, env);
    mvClipB.Update(mvBB, env);// backward from next next to next
    mvBB = 0;

   if ( mvClipB.IsUsable()  && mvClipF.IsUsable() && maskmode==2) // slow method with extra frames
   {
    // get vector mask from extra frames
      PROFILE_START(MOTION_PROFILE_MASK);
	MakeVectorSmallMasks(mvClipB, nBlkX, nBlkY, VXSmallYBB, nBlkXP, VYSmallYBB, nBlkXP);
	MakeVectorSmallMasks(mvClipF, nBlkX, nBlkY, VXSmallYFF, nBlkXP, VYSmallYFF, nBlkXP);
	if (nBlkXP > nBlkX) // fill right
	{
		for (int j=0; j<nBlkY; j++)
		{
			VXSmallYBB[j*nBlkXP + nBlkX] = std::min(VXSmallYBB[j*nBlkXP + nBlkX-1], uint8_t (128));
			VYSmallYBB[j*nBlkXP + nBlkX] = VYSmallYBB[j*nBlkXP + nBlkX-1];
			VXSmallYFF[j*nBlkXP + nBlkX] = std::min(VXSmallYFF[j*nBlkXP + nBlkX-1], uint8_t (128));
			VYSmallYFF[j*nBlkXP + nBlkX] = VYSmallYFF[j*nBlkXP + nBlkX-1];
		}
	}
	if (nBlkYP > nBlkY) // fill bottom
	{
		for (int i=0; i<nBlkXP; i++)
		{
			VXSmallYBB[nBlkXP*nBlkY +i] = VXSmallYBB[nBlkXP*(nBlkY-1) +i];
			VYSmallYBB[nBlkXP*nBlkY +i] = std::min(VYSmallYBB[nBlkXP*(nBlkY-1) +i], uint8_t (128));
			VXSmallYFF[nBlkXP*nBlkY +i] = VXSmallYFF[nBlkXP*(nBlkY-1) +i];
			VYSmallYFF[nBlkXP*nBlkY +i] = std::min(VYSmallYFF[nBlkXP*(nBlkY-1) +i], uint8_t (128));
		}
	}
	VectorSmallMaskYToHalfUV(VXSmallYBB, nBlkXP, nBlkYP, VXSmallUVBB, 2);
	VectorSmallMaskYToHalfUV(VYSmallYBB, nBlkXP, nBlkYP, VYSmallUVBB, yRatioUV);
	VectorSmallMaskYToHalfUV(VXSmallYFF, nBlkXP, nBlkYP, VXSmallUVFF, 2);
	VectorSmallMaskYToHalfUV(VYSmallYFF, nBlkXP, nBlkYP, VYSmallUVFF, yRatioUV);
      PROFILE_STOP(MOTION_PROFILE_MASK);

      PROFILE_START(MOTION_PROFILE_RESIZE);
    // upsize vectors to full frame
	  upsizer->SimpleResizeDo(VXFullYBB, nWidthP, nHeightP, VPitchY, VXSmallYBB, nBlkXP, nBlkXP, dummyplane);
	  upsizer->SimpleResizeDo(VYFullYBB, nWidthP, nHeightP, VPitchY, VYSmallYBB, nBlkXP, nBlkXP, dummyplane);
	  upsizerUV->SimpleResizeDo(VXFullUVBB, nWidthPUV, nHeightPUV, VPitchUV, VXSmallUVBB, nBlkXP, nBlkXP, dummyplane);
	  upsizerUV->SimpleResizeDo(VYFullUVBB, nWidthPUV, nHeightPUV, VPitchUV, VYSmallUVBB, nBlkXP, nBlkXP, dummyplane);

	  upsizer->SimpleResizeDo(VXFullYFF, nWidthP, nHeightP, VPitchY, VXSmallYFF, nBlkXP, nBlkXP, dummyplane);
	  upsizer->SimpleResizeDo(VYFullYFF, nWidthP, nHeightP, VPitchY, VYSmallYFF, nBlkXP, nBlkXP, dummyplane);
	  upsizerUV->SimpleResizeDo(VXFullUVFF, nWidthPUV, nHeightPUV, VPitchUV, VXSmallUVFF, nBlkXP, nBlkXP, dummyplane);
	  upsizerUV->SimpleResizeDo(VYFullUVFF, nWidthPUV, nHeightPUV, VPitchUV, VYSmallUVFF, nBlkXP, nBlkXP, dummyplane);
      PROFILE_STOP(MOTION_PROFILE_RESIZE);

      PROFILE_START(MOTION_PROFILE_FLOWINTER);
/*      if (isSuper)
      {
		  if (nPel>=2)
		  {
			FlowInterExtraPel(pDst[0], nDstPitches[0], pPlanesB[0], pPlanesF[0], 0,
				VXFullYB, VXFullYF, VYFullYB, VYFullYF, MaskFullYB, MaskFullYF, VPitchY,
				nWidth, nHeight, time256, nPel, LUTVB, LUTVF, VXFullYBB, VXFullYFF, VYFullYBB, VYFullYFF);
			if (nSuperModeYUV & UPLANE) FlowInterExtraPel(pDst[1], nDstPitches[1], pPlanesB[1], pPlanesF[1], 0,
				VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, MaskFullUVB, MaskFullUVF, VPitchUV,
				nWidthUV, nHeightUV, time256, nPel, LUTVB, LUTVF, VXFullUVBB, VXFullUVFF, VYFullUVBB, VYFullUVFF);
			if (nSuperModeYUV & VPLANE) FlowInterExtraPel(pDst[2], nDstPitches[2], pPlanesB[2], pPlanesF[2], 0,
				VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, MaskFullUVB, MaskFullUVF, VPitchUV,
				nWidthUV, nHeightUV, time256, nPel, LUTVB, LUTVF, VXFullUVBB, VXFullUVFF, VYFullUVBB, VYFullUVFF);

		  }
		  else if (nPel==1)
		  {
			FlowInterExtra(pDst[0], nDstPitches[0], pPlanesB[0]->GetPointerPel1(0,0), pPlanesF[0]->GetPointerPel1(0,0), pPlanesB[0]->GetPitch(),
				VXFullYB, VXFullYF, VYFullYB, VYFullYF, MaskFullYB, MaskFullYF, VPitchY,
				nWidth, nHeight, time256, nPel, LUTVB, LUTVF, VXFullYBB, VXFullYFF, VYFullYBB, VYFullYFF);
			if (nSuperModeYUV & UPLANE) FlowInterExtra(pDst[1], nDstPitches[1], pPlanesB[1]->GetPointerPel1(0,0), pPlanesF[1]->GetPointerPel1(0,0), pPlanesB[1]->GetPitch(),
				VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, MaskFullUVB, MaskFullUVF, VPitchUV,
				nWidthUV, nHeightUV, time256, nPel, LUTVB, LUTVF, VXFullUVBB, VXFullUVFF, VYFullUVBB, VYFullUVFF);
			if (nSuperModeYUV & VPLANE) FlowInterExtra(pDst[2], nDstPitches[2], pPlanesB[2]->GetPointerPel1(0,0), pPlanesF[2]->GetPointerPel1(0,0), pPlanesB[2]->GetPitch(),
				VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, MaskFullUVB, MaskFullUVF, VPitchUV,
				nWidthUV, nHeightUV, time256, nPel, LUTVB, LUTVF, VXFullUVBB, VXFullUVFF, VYFullUVBB, VYFullUVFF);
		  }
      }
      else // finest mode
*/      {
			FlowInterExtra(pDst[0], nDstPitches[0], pRef[0] + nOffsetY, pSrc[0] + nOffsetY, nRefPitches[0],
				VXFullYB, VXFullYF, VYFullYB, VYFullYF, MaskFullYB, MaskFullYF, VPitchY,
				nWidth, nHeight, nPel, t256_prov_cst, VXFullYBB, VXFullYFF, VYFullYBB, VYFullYFF);
			FlowInterExtra(pDst[1], nDstPitches[1], pRef[1] + nOffsetUV, pSrc[1] + nOffsetUV, nRefPitches[1],
				VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, MaskFullUVB, MaskFullUVF, VPitchUV,
				nWidthUV, nHeightUV, nPel, t256_prov_cst, VXFullUVBB, VXFullUVFF, VYFullUVBB, VYFullUVFF);
			FlowInterExtra(pDst[2], nDstPitches[2], pRef[2] + nOffsetUV, pSrc[2] + nOffsetUV, nRefPitches[2],
				VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, MaskFullUVB, MaskFullUVF, VPitchUV,
				nWidthUV, nHeightUV, nPel, t256_prov_cst, VXFullUVBB, VXFullUVFF, VYFullUVBB, VYFullUVFF);
      }
      PROFILE_STOP(MOTION_PROFILE_FLOWINTER);
   }
   else if (maskmode==1) // old method without extra frames
   {
     PROFILE_START(MOTION_PROFILE_FLOWINTER);
/*     if (isSuper)
     {
		  if (nPel>=2)
		  {
			FlowInterPel(pDst[0], nDstPitches[0], pPlanesB[0], pPlanesF[0], 0,
				VXFullYB, VXFullYF, VYFullYB, VYFullYF, MaskFullYB, MaskFullYF, VPitchY,
				nWidth, nHeight, time256, nPel, LUTVB, LUTVF);
			if (nSuperModeYUV & UPLANE) FlowInterPel(pDst[1], nDstPitches[1], pPlanesB[1], pPlanesF[1], 0,
				VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, MaskFullUVB, MaskFullUVF, VPitchUV,
				nWidthUV, nHeightUV, time256, nPel, LUTVB, LUTVF);
			if (nSuperModeYUV & VPLANE) FlowInterPel(pDst[2], nDstPitches[2], pPlanesB[2], pPlanesF[2], 0,
				VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, MaskFullUVB, MaskFullUVF, VPitchUV,
				nWidthUV, nHeightUV, time256, nPel, LUTVB, LUTVF);

		  }
		  else if (nPel==1)
		  {
			FlowInter(pDst[0], nDstPitches[0], pPlanesB[0]->GetPointerPel1(0,0), pPlanesF[0]->GetPointerPel1(0,0), pPlanesB[0]->GetPitch(),
				VXFullYB, VXFullYF, VYFullYB, VYFullYF, MaskFullYB, MaskFullYF, VPitchY,
				nWidth, nHeight, time256, nPel, LUTVB, LUTVF);
			if (nSuperModeYUV & UPLANE) FlowInter(pDst[1], nDstPitches[1], pPlanesB[1]->GetPointerPel1(0,0), pPlanesF[1]->GetPointerPel1(0,0), pPlanesB[1]->GetPitch(),
				VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, MaskFullUVB, MaskFullUVF, VPitchUV,
				nWidthUV, nHeightUV, time256, nPel, LUTVB, LUTVF);
			if (nSuperModeYUV & VPLANE) FlowInter(pDst[2], nDstPitches[2], pPlanesB[2]->GetPointerPel1(0,0), pPlanesF[2]->GetPointerPel1(0,0), pPlanesB[2]->GetPitch(),
				VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, MaskFullUVB, MaskFullUVF, VPitchUV,
				nWidthUV, nHeightUV, time256, nPel, LUTVB, LUTVF);
		  }
     }
     else // finest
*/     {
			FlowInter(pDst[0], nDstPitches[0], pRef[0] + nOffsetY, pSrc[0] + nOffsetY, nRefPitches[0],
				VXFullYB, VXFullYF, VYFullYB, VYFullYF, MaskFullYB, MaskFullYF, VPitchY,
				nWidth, nHeight, nPel, t256_prov_cst);
			FlowInter(pDst[1], nDstPitches[1], pRef[1] + nOffsetUV, pSrc[1] + nOffsetUV, nRefPitches[1],
				VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, MaskFullUVB, MaskFullUVF, VPitchUV,
				nWidthUV, nHeightUV, nPel, t256_prov_cst);
			FlowInter(pDst[2], nDstPitches[2], pRef[2] + nOffsetUV, pSrc[2] + nOffsetUV, nRefPitches[2],
				VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, MaskFullUVB, MaskFullUVF, VPitchUV,
				nWidthUV, nHeightUV, nPel, t256_prov_cst);
     }
    PROFILE_STOP(MOTION_PROFILE_FLOWINTER);
   }
   else // mode=0, faster simple method
   {

     PROFILE_START(MOTION_PROFILE_FLOWINTER);
/*     if (isSuper)
     {
		  if (nPel>=2)
		  {
			FlowInterSimplePel(pDst[0], nDstPitches[0], pPlanesB[0], pPlanesF[0], 0,
				VXFullYB, VXFullYF, VYFullYB, VYFullYF, MaskFullYB, MaskFullYF, VPitchY,
				nWidth, nHeight, time256, nPel, LUTVB, LUTVF);
			if (nSuperModeYUV & UPLANE) FlowInterSimplePel(pDst[1], nDstPitches[1], pPlanesB[1], pPlanesF[1], 0,
				VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, MaskFullUVB, MaskFullUVF, VPitchUV,
				nWidthUV, nHeightUV, time256, nPel, LUTVB, LUTVF);
			if (nSuperModeYUV & VPLANE) FlowInterSimplePel(pDst[2], nDstPitches[2], pPlanesB[2], pPlanesF[2], 0,
				VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, MaskFullUVB, MaskFullUVF, VPitchUV,
				nWidthUV, nHeightUV, time256, nPel, LUTVB, LUTVF);

		  }
		  else if (nPel==1)
		  {
			FlowInterSimple(pDst[0], nDstPitches[0], pPlanesB[0]->GetPointerPel1(0,0), pPlanesF[0]->GetPointerPel1(0,0), pPlanesB[0]->GetPitch(),
				VXFullYB, VXFullYF, VYFullYB, VYFullYF, MaskFullYB, MaskFullYF, VPitchY,
				nWidth, nHeight, time256, nPel, LUTVB, LUTVF);
			if (nSuperModeYUV & UPLANE) FlowInterSimple(pDst[1], nDstPitches[1], pPlanesB[1]->GetPointerPel1(0,0), pPlanesF[1]->GetPointerPel1(0,0), pPlanesB[1]->GetPitch(),
				VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, MaskFullUVB, MaskFullUVF, VPitchUV,
				nWidthUV, nHeightUV, time256, nPel, LUTVB, LUTVF);
			if (nSuperModeYUV & VPLANE) FlowInterSimple(pDst[2], nDstPitches[2], pPlanesB[2]->GetPointerPel1(0,0), pPlanesF[2]->GetPointerPel1(0,0), pPlanesB[2]->GetPitch(),
				VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, MaskFullUVB, MaskFullUVF, VPitchUV,
				nWidthUV, nHeightUV, time256, nPel, LUTVB, LUTVF);
		  }
     }
     else // finest
*/     {
			FlowInterSimple(pDst[0], nDstPitches[0], pRef[0] + nOffsetY, pSrc[0] + nOffsetY, nRefPitches[0],
				VXFullYB, VXFullYF, VYFullYB, VYFullYF, MaskFullYB, MaskFullYF, VPitchY,
				nWidth, nHeight, nPel, t256_prov_cst);
			FlowInterSimple(pDst[1], nDstPitches[1], pRef[1] + nOffsetUV, pSrc[1] + nOffsetUV, nRefPitches[1],
				VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, MaskFullUVB, MaskFullUVF, VPitchUV,
				nWidthUV, nHeightUV, nPel, t256_prov_cst);
			FlowInterSimple(pDst[2], nDstPitches[2], pRef[2] + nOffsetUV, pSrc[2] + nOffsetUV, nRefPitches[2],
				VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, MaskFullUVB, MaskFullUVF, VPitchUV,
				nWidthUV, nHeightUV, nPel, t256_prov_cst);
     }
     PROFILE_STOP(MOTION_PROFILE_FLOWINTER);
   }
      PROFILE_START(MOTION_PROFILE_YUY2CONVERT);
		if ( (pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2 && !planar)
		{
			YUY2FromPlanes(pDstYUY2, nDstPitchYUY2, nWidth, nHeight,
								  pDst[0], nDstPitches[0], pDst[1], pDst[2], nDstPitches[1], isse);
		}
      PROFILE_STOP(MOTION_PROFILE_YUY2CONVERT);
		return dst;
   }
   else
   {
        // poor estimation

        // prepare pointers
        PVideoFrame src = child->GetFrame(nleft,env); // it is easy to use child here - v2.0

	if (blend) //let's blend src with ref frames like ConvertFPS
	{
        PVideoFrame ref = child->GetFrame(nright,env);
      PROFILE_START(MOTION_PROFILE_FLOWINTER);
        if ( (pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2 )
		{
			pSrc[0] = src->GetReadPtr(); // we can blend YUY2
			nSrcPitches[0] = src->GetPitch();
			pRef[0] = ref->GetReadPtr();
			nRefPitches[0]  = ref->GetPitch();
			pDstYUY2 = dst->GetWritePtr();
			nDstPitchYUY2 = dst->GetPitch();
            Blend(pDstYUY2, pSrc[0], pRef[0], nHeight, nWidth*2, nDstPitchYUY2, nSrcPitches[0], nRefPitches[0], t256_prov_cst, isse);
		}
		else
		{
         pDst[0] = YWPLAN(dst);
         pDst[1] = UWPLAN(dst);
         pDst[2] = VWPLAN(dst);
         nDstPitches[0] = YPITCH(dst);
         nDstPitches[1] = UPITCH(dst);
         nDstPitches[2] = VPITCH(dst);

         pRef[0] = YRPLAN(ref);
         pRef[1] = URPLAN(ref);
         pRef[2] = VRPLAN(ref);
         nRefPitches[0] = YPITCH(ref);
         nRefPitches[1] = UPITCH(ref);
         nRefPitches[2] = VPITCH(ref);

         pSrc[0] = YRPLAN(src);
         pSrc[1] = URPLAN(src);
         pSrc[2] = VRPLAN(src);
         nSrcPitches[0] = YPITCH(src);
         nSrcPitches[1] = UPITCH(src);
         nSrcPitches[2] = VPITCH(src);
       // blend with time weight
        Blend(pDst[0], pSrc[0], pRef[0], nHeight, nWidth, nDstPitches[0], nSrcPitches[0], nRefPitches[0], t256_prov_cst, isse);
        Blend(pDst[1], pSrc[1], pRef[1], nHeightUV, nWidthUV, nDstPitches[1], nSrcPitches[1], nRefPitches[1], t256_prov_cst, isse);
        Blend(pDst[2], pSrc[2], pRef[2], nHeightUV, nWidthUV, nDstPitches[2], nSrcPitches[2], nRefPitches[2], t256_prov_cst, isse);
		}
     PROFILE_STOP(MOTION_PROFILE_FLOWINTER);

		return dst;
	}
	else
	{
		return src; // like ChangeFPS
	}

   }

}
