// Make a motion compensate temporal denoiser
// Copyright(c)2006 A.G.Balakhnin aka Fizick
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

#include "ClipFnc.h"
#include "MVDegrain1.h"
#include "MVFrame.h"
#include	"MVGroupOfFrames.h"
#include "MVPlane.h"
#include "profile.h"
#include "SuperParams64Bits.h"
#define __INTEL_COMPILER_USE_INTRINSIC_PROTOTYPES 1 // P.F. https://software.intel.com/en-us/articles/visual-studio-intellisense-stopped-recognizing-many-of-the-avx-avx2-intrinsics
#include	<mmintrin.h>

#if 0
// PF 160926: common MDegrain1 to 5

MVDegrain1::Denoise1Function* MVDegrain1::get_denoise1_function(int BlockX, int BlockY, int pixelsize, arch_t arch)
{
    // 8 bit only (pixelsize==1)
    //---------- DENOISE/DEGRAIN
    // BlkSizeX, BlkSizeY, pixelsize, arch_t
    std::map<std::tuple<int, int, int, arch_t>, Denoise1Function*> func_degrain;
    using std::make_tuple;

    func_degrain[make_tuple(32, 32, 1, NO_SIMD)] = Degrain1_C<32, 32>;
    func_degrain[make_tuple(32, 16, 1, NO_SIMD)] = Degrain1_C<32, 16>;
    func_degrain[make_tuple(32, 8 , 1, NO_SIMD)] = Degrain1_C<32, 8>;
    func_degrain[make_tuple(16, 32, 1, NO_SIMD)] = Degrain1_C<16, 32>;
    func_degrain[make_tuple(16, 16, 1, NO_SIMD)] = Degrain1_C<16, 16>;
    func_degrain[make_tuple(16, 8 , 1, NO_SIMD)] = Degrain1_C<16, 8>;
    func_degrain[make_tuple(16, 4 , 1, NO_SIMD)] = Degrain1_C<16, 4>;
    func_degrain[make_tuple(16, 2 , 1, NO_SIMD)] = Degrain1_C<16, 2>;
    func_degrain[make_tuple(8 , 16, 1, NO_SIMD)] = Degrain1_C<8 , 16>;
    func_degrain[make_tuple(8 , 8 , 1, NO_SIMD)] = Degrain1_C<8 , 8>;
    func_degrain[make_tuple(8 , 4 , 1, NO_SIMD)] = Degrain1_C<8 , 4>;
    func_degrain[make_tuple(8 , 2 , 1, NO_SIMD)] = Degrain1_C<8 , 2>;
    func_degrain[make_tuple(8 , 1 , 1, NO_SIMD)] = Degrain1_C<8 , 1>;
    func_degrain[make_tuple(4 , 8 , 1, NO_SIMD)] = Degrain1_C<4 , 8>;
    func_degrain[make_tuple(4 , 4 , 1, NO_SIMD)] = Degrain1_C<4 , 4>;
    func_degrain[make_tuple(4 , 2 , 1, NO_SIMD)] = Degrain1_C<4 , 2>;
    func_degrain[make_tuple(2 , 4 , 1, NO_SIMD)] = Degrain1_C<2 , 4>;
    func_degrain[make_tuple(2 , 2 , 1, NO_SIMD)] = Degrain1_C<2 , 2>;

#ifndef _M_X64
    func_degrain[make_tuple(32, 32, 1, USE_MMX)] = Degrain1_mmx<32, 32>;
    func_degrain[make_tuple(32, 16, 1, USE_MMX)] = Degrain1_mmx<32, 16>;
    func_degrain[make_tuple(32, 8 , 1, USE_MMX)] = Degrain1_mmx<32, 8>;
    func_degrain[make_tuple(16, 32, 1, USE_MMX)] = Degrain1_mmx<16, 32>;
    func_degrain[make_tuple(16, 16, 1, USE_MMX)] = Degrain1_mmx<16, 16>;
    func_degrain[make_tuple(16, 8 , 1, USE_MMX)] = Degrain1_mmx<16, 8>;
    func_degrain[make_tuple(16, 4 , 1, USE_MMX)] = Degrain1_mmx<16, 4>;
    func_degrain[make_tuple(16, 2 , 1, USE_MMX)] = Degrain1_mmx<16, 2>;
    func_degrain[make_tuple(8 , 16, 1, USE_MMX)] = Degrain1_mmx<8 , 16>;
    func_degrain[make_tuple(8 , 8 , 1, USE_MMX)] = Degrain1_mmx<8 , 8>;
    func_degrain[make_tuple(8 , 4 , 1, USE_MMX)] = Degrain1_mmx<8 , 4>;
    func_degrain[make_tuple(8 , 2 , 1, USE_MMX)] = Degrain1_mmx<8 , 2>;
    func_degrain[make_tuple(8 , 1 , 1, USE_MMX)] = Degrain1_mmx<8 , 1>;
    func_degrain[make_tuple(4 , 8 , 1, USE_MMX)] = Degrain1_mmx<4 , 8>;
    func_degrain[make_tuple(4 , 4 , 1, USE_MMX)] = Degrain1_mmx<4 , 4>;
    func_degrain[make_tuple(4 , 2 , 1, USE_MMX)] = Degrain1_mmx<4 , 2>;
    func_degrain[make_tuple(2 , 4 , 1, USE_MMX)] = Degrain1_mmx<2 , 4>;
    func_degrain[make_tuple(2 , 2 , 1, USE_MMX)] = Degrain1_mmx<2 , 2>;
#endif
    func_degrain[make_tuple(32, 32, 1, USE_SSE2)] = Degrain1_sse2<32, 32>;
    func_degrain[make_tuple(32, 16, 1, USE_SSE2)] = Degrain1_sse2<32, 16>;
    func_degrain[make_tuple(32, 8 , 1, USE_SSE2)] = Degrain1_sse2<32, 8>;
    func_degrain[make_tuple(16, 32, 1, USE_SSE2)] = Degrain1_sse2<16, 32>;
    func_degrain[make_tuple(16, 16, 1, USE_SSE2)] = Degrain1_sse2<16, 16>;
    func_degrain[make_tuple(16, 8 , 1, USE_SSE2)] = Degrain1_sse2<16, 8>;
    func_degrain[make_tuple(16, 4 , 1, USE_SSE2)] = Degrain1_sse2<16, 4>;
    func_degrain[make_tuple(16, 2 , 1, USE_SSE2)] = Degrain1_sse2<16, 2>;
    func_degrain[make_tuple(8 , 16, 1, USE_SSE2)] = Degrain1_sse2<8 , 16>;
    func_degrain[make_tuple(8 , 8 , 1, USE_SSE2)] = Degrain1_sse2<8 , 8>;
    func_degrain[make_tuple(8 , 4 , 1, USE_SSE2)] = Degrain1_sse2<8 , 4>;
    func_degrain[make_tuple(8 , 2 , 1, USE_SSE2)] = Degrain1_sse2<8 , 2>;
    func_degrain[make_tuple(8 , 1 , 1, USE_SSE2)] = Degrain1_sse2<8 , 1>;
    func_degrain[make_tuple(4 , 8 , 1, USE_SSE2)] = Degrain1_sse2<4 , 8>;
    func_degrain[make_tuple(4 , 4 , 1, USE_SSE2)] = Degrain1_sse2<4 , 4>;
    func_degrain[make_tuple(4 , 2 , 1, USE_SSE2)] = Degrain1_sse2<4 , 2>;
    func_degrain[make_tuple(2 , 4 , 1, USE_SSE2)] = Degrain1_sse2<2 , 4>;
    func_degrain[make_tuple(2 , 2 , 1, USE_SSE2)] = Degrain1_sse2<2 , 2>;

    return func_degrain[make_tuple(BlockX, BlockY, pixelsize, arch)];
}



//-------------------------------------------------------------------------

MVDegrain1::MVDegrain1(
	PClip _child, PClip _super, PClip mvbw, PClip mvfw,
	int _thSAD, int _thSADC, int _YUVplanes, int _nLimit, int _nLimitC,
	int _nSCD1, int _nSCD2, bool _isse2, bool _planar, bool _lsb_flag,
	bool mt_flag, IScriptEnvironment* env
)
:	GenericVideoFilter(_child)
,	MVFilter ((! mvfw) ? mvbw : mvfw,  "MDegrain1",    env, (! mvfw) ? 2 : 1, (! mvfw) ? 1 : 0)
,	mvClipF  ((! mvfw) ? mvbw : mvfw,  _nSCD1, _nSCD2, env, (! mvfw) ? 2 : 1, (! mvfw) ? 1 : 0)
,	mvClipB  ((! mvfw) ? mvbw : mvbw,  _nSCD1, _nSCD2, env, (! mvfw) ? 2 : 1, (! mvfw) ? 0 : 0)
,	super (_super)
,	lsb_flag (_lsb_flag)
,	height_lsb_mul ((_lsb_flag) ? 2 : 1)
,	DstShort (0)
,	DstInt (0)
{
	thSAD = _thSAD*mvClipB.GetThSCD1()/_nSCD1; // normalize to block SAD
	thSADC = _thSADC*mvClipB.GetThSCD1()/_nSCD1; // chroma threshold, normalized to block SAD
	YUVplanes = _YUVplanes;
	nLimit = _nLimit;
	nLimitC = _nLimitC;

	isse_flag = _isse2;
	planar = _planar;

	CheckSimilarity(mvClipF, "mvfw", env);
	CheckSimilarity(mvClipB, "mvbw", env);

	if (mvClipB.GetDeltaFrame() <= 0 || mvClipB.GetDeltaFrame() <= 0) // 2.5.11.22
		env->ThrowError("MDegrain1: cannot use motion vectors with absolute frame references.");

	const ::VideoInfo &	vi_super = _super->GetVideoInfo ();

    pixelsize = vi.ComponentSize();
    pixelsize_super = vi_super.ComponentSize();

	// get parameters of prepared super clip - v2.0
	SuperParams64Bits params;
	memcpy(&params, &vi_super.num_audio_samples, 8);
	int nHeightS = params.nHeight;
	int nSuperHPad = params.nHPad;
	int nSuperVPad = params.nVPad;
	int nSuperPel = params.nPel;
	nSuperModeYUV = params.nModeYUV;
	int nSuperLevels = params.nLevels;

	pRefBGOF = new MVGroupOfFrames(nSuperLevels, nWidth, nHeight, nSuperPel, nSuperHPad, nSuperVPad, nSuperModeYUV, isse_flag, xRatioUV, yRatioUV, pixelsize_super, mt_flag);
	pRefFGOF = new MVGroupOfFrames(nSuperLevels, nWidth, nHeight, nSuperPel, nSuperHPad, nSuperVPad, nSuperModeYUV, isse_flag, xRatioUV, yRatioUV, pixelsize_super, mt_flag);
	int nSuperWidth  = vi_super.width;
	int nSuperHeight = vi_super.height;

	if (   nHeight != nHeightS
	    || nHeight != vi.height
	    || nWidth  != nSuperWidth-nSuperHPad*2
	    || nWidth  != vi.width
	    || nPel    != nSuperPel)
	{
		env->ThrowError("MDegrain1 : wrong source or super frame size");
	}


   if ( (pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2 && !planar)
   {
		DstPlanes =  new YUY2Planes(nWidth, nHeight * height_lsb_mul);
		SrcPlanes =  new YUY2Planes(nWidth, nHeight);
   }

   dstShortPitch = ((nWidth + 15)/16)*16;
	dstIntPitch = dstShortPitch;
    // todo pixelsize, bit DstInt seems to be enough
   if (nOverlapX >0 || nOverlapY>0)
   {
		OverWins = new OverlapWindows(nBlkSizeX, nBlkSizeY, nOverlapX, nOverlapY);
		OverWinsUV = new OverlapWindows(nBlkSizeX/xRatioUV, nBlkSizeY/yRatioUV, nOverlapX/xRatioUV, nOverlapY/yRatioUV);
		if (lsb_flag)
		{
			DstInt = new int [dstIntPitch * nHeight];
		}
		else
		{
			DstShort = new unsigned short[dstShortPitch*nHeight];
		}
   }

   // in overlaps.h
   // OverlapsLsbFunction
   // OverlapsFunction
   // in M(V)DegrainX: DenoiseXFunction
   arch_t arch;
   if ((pixelsize == 1) && (((env->GetCPUFlags() & CPUF_SSE2) != 0) & isse_flag))
       arch = USE_SSE2;
   else if ((pixelsize == 1) && isse_flag)
       arch = USE_MMX;
   else
       arch = NO_SIMD;

   // C only -> NO_SIMD
   OVERSLUMALSB   = get_overlaps_lsb_function(nBlkSizeX, nBlkSizeY, sizeof(uint8_t), NO_SIMD);
   OVERSCHROMALSB = get_overlaps_lsb_function(nBlkSizeX/xRatioUV, nBlkSizeY/yRatioUV, sizeof(uint8_t), NO_SIMD);

   OVERSLUMA   = get_overlaps_function(nBlkSizeX, nBlkSizeY, pixelsize, arch);
   OVERSCHROMA = get_overlaps_function(nBlkSizeX/xRatioUV, nBlkSizeY/yRatioUV, pixelsize, arch);

   DEGRAINLUMA = get_denoise1_function(nBlkSizeX, nBlkSizeY, pixelsize, arch);
   DEGRAINCHROMA = get_denoise1_function(nBlkSizeX/xRatioUV, nBlkSizeY/yRatioUV, pixelsize, arch);

	const int		tmp_size = 32 * 32;
	tmpBlock = new BYTE[tmp_size * height_lsb_mul]; // * pixelsize
	tmpBlockLsb = (lsb_flag) ? (tmpBlock + tmp_size) : 0;

	if (lsb_flag)
	{
		vi.height <<= 1;
	}
}


MVDegrain1::~MVDegrain1()
{
   if ( (pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2 && !planar)
   {
	delete DstPlanes;
	delete SrcPlanes;
   }
   if (nOverlapX >0 || nOverlapY>0)
   {
	   delete OverWins;
	   delete OverWinsUV;
	   delete [] DstShort;
	   delete [] DstInt;
   }
   delete [] tmpBlock;
   delete pRefFGOF; // v2.0
   delete pRefBGOF;
}

PVideoFrame __stdcall MVDegrain1::GetFrame(int n, IScriptEnvironment* env)
{
	int nWidth_B = nBlkX*(nBlkSizeX - nOverlapX) + nOverlapX;
	int nHeight_B = nBlkY*(nBlkSizeY - nOverlapY) + nOverlapY;

	PVideoFrame	src	= child->GetFrame(n, env);
	PVideoFrame dst;
	BYTE *pDst[3], *pDstCur[3];
	const BYTE *pSrcCur[3];
	const BYTE *pSrc[3];
	const BYTE *pRefB[3];
	const BYTE *pRefF[3];
	int nDstPitches[3], nSrcPitches[3];
	int nRefBPitches[3], nRefFPitches[3];
	unsigned char *pDstYUY2;
	const unsigned char *pSrcYUY2;
	int nDstPitchYUY2;
	int nSrcPitchYUY2;
	bool isUsableB, isUsableF;
	int tmpPitch = nBlkSizeX;
	int nLogPel = (nPel==4) ? 2 : (nPel==2) ? 1 : 0;
	// nLogPel=0 for nPel=1, 1 for nPel=2, 2 for nPel=4, i.e. (x*nPel) = (x<<nLogPel)

	PVideoFrame mvF = mvClipF.GetFrame(n, env);
	mvClipF.Update(mvF, env);
	isUsableF = mvClipF.IsUsable();
	mvF = 0; // v2.0.9.2 -  it seems, we do not need in vectors clip anymore when we finished copiing them to fakeblockdatas
	PVideoFrame mvB = mvClipB.GetFrame(n, env);
	mvClipB.Update(mvB, env);
	isUsableB = mvClipB.IsUsable();
	mvB = 0;

	int				lsb_offset_y = 0;
	int				lsb_offset_u = 0;
	int				lsb_offset_v = 0;

//   int sharp = mvClipB.GetSharp();
    int xSubUV = (xRatioUV == 2) ? 1 : 0;
    int ySubUV = (yRatioUV == 2) ? 1 : 0;
//   if ( mvClipB.IsUsable() && mvClipF.IsUsable() && mvClipB2.IsUsable() && mvClipF2.IsUsable() )
//   {
	dst = env->NewVideoFrame(vi);
	if ( (pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2 )
	{
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
			pSrcYUY2 = src->GetReadPtr();
			nSrcPitchYUY2 = src->GetPitch();
			pSrc[0] = SrcPlanes->GetPtr();
			pSrc[1] = SrcPlanes->GetPtrU();
			pSrc[2] = SrcPlanes->GetPtrV();
			nSrcPitches[0]  = SrcPlanes->GetPitch();
			nSrcPitches[1]  = SrcPlanes->GetPitchUV();
			nSrcPitches[2]  = SrcPlanes->GetPitchUV();
			YUY2ToPlanes(pSrcYUY2, nSrcPitchYUY2, nWidth, nHeight,
			pSrc[0], nSrcPitches[0], pSrc[1], pSrc[2], nSrcPitches[1], isse_flag);
		}
		else
		{
			pDst[0] = dst->GetWritePtr();
			pDst[1] = pDst[0] + nWidth;
			pDst[2] = pDst[1] + nWidth/2; // YUY2
			nDstPitches[0] = dst->GetPitch();
			nDstPitches[1] = nDstPitches[0];
			nDstPitches[2] = nDstPitches[0];
			pSrc[0] = src->GetReadPtr();
			pSrc[1] = pSrc[0] + nWidth;
			pSrc[2] = pSrc[1] + nWidth/2;
			nSrcPitches[0] = src->GetPitch();
			nSrcPitches[1] = nSrcPitches[0];
			nSrcPitches[2] = nSrcPitches[0];
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
		pSrc[0] = YRPLAN(src);
		pSrc[1] = URPLAN(src);
		pSrc[2] = VRPLAN(src);
		nSrcPitches[0] = YPITCH(src);
		nSrcPitches[1] = UPITCH(src);
		nSrcPitches[2] = VPITCH(src);
	}

	lsb_offset_y = nDstPitches [0] *  nHeight;
	lsb_offset_u = nDstPitches [1] * (nHeight / yRatioUV);
	lsb_offset_v = nDstPitches [2] * (nHeight / yRatioUV);

	if (lsb_flag)
	{
		memset (pDst [0] + lsb_offset_y, 0, lsb_offset_y);
		if (! planar)
		{
			memset (pDst [1] + lsb_offset_u, 0, lsb_offset_u);
			memset (pDst [2] + lsb_offset_v, 0, lsb_offset_v);
		}
	}

//	MVFrames *pFrames = mvCore->GetFrames(nIdx);
	PVideoFrame refB, refF;

//	PVideoFrame refB2x, refF2x;

	mvClipF.use_ref_frame (refF, isUsableF, super, n, env);
	mvClipB.use_ref_frame (refB, isUsableB, super, n, env);

	if ( (pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2 )
	{
		// planar data packed to interleaved format (same as interleved2planar by kassandro) - v2.0.0.5
		if (isUsableF)
		{
			pRefF[0] = refF->GetReadPtr();
			pRefF[1] = pRefF[0] + refF->GetRowSize()/2; // YUY2
			pRefF[2] = pRefF[1] + refF->GetRowSize()/4;
			nRefFPitches[0]  = refF->GetPitch();
			nRefFPitches[1]  = nRefFPitches[0];
			nRefFPitches[2]  = nRefFPitches[0];
		}
		if (isUsableB)
		{
			pRefB[0] = refB->GetReadPtr();
			pRefB[1] = pRefB[0] + refB->GetRowSize()/2;
			pRefB[2] = pRefB[1] + refB->GetRowSize()/4;
			nRefBPitches[0]  = refB->GetPitch();
			nRefBPitches[1]  = nRefBPitches[0];
			nRefBPitches[2]  = nRefBPitches[0];
		}
	}
	else
	{
		if (isUsableF)
		{
			pRefF[0] = YRPLAN(refF);
			pRefF[1] = URPLAN(refF);
			pRefF[2] = VRPLAN(refF);
			nRefFPitches[0] = YPITCH(refF);
			nRefFPitches[1] = UPITCH(refF);
			nRefFPitches[2] = VPITCH(refF);
		}
		if (isUsableB)
		{
			pRefB[0] = YRPLAN(refB);
			pRefB[1] = URPLAN(refB);
			pRefB[2] = VRPLAN(refB);
			nRefBPitches[0] = YPITCH(refB);
			nRefBPitches[1] = UPITCH(refB);
			nRefBPitches[2] = VPITCH(refB);
		}
	}



	MVPlane *pPlanesB[3] = { 0 };
	MVPlane *pPlanesF[3] = { 0 };

	if (isUsableF)
	{
		pRefFGOF->Update(YUVplanes, (BYTE*)pRefF[0], nRefFPitches[0], (BYTE*)pRefF[1], nRefFPitches[1], (BYTE*)pRefF[2], nRefFPitches[2]);
		if (YUVplanes & YPLANE)
			pPlanesF[0] = pRefFGOF->GetFrame(0)->GetPlane(YPLANE);
		if (YUVplanes & UPLANE)
			pPlanesF[1] = pRefFGOF->GetFrame(0)->GetPlane(UPLANE);
		if (YUVplanes & VPLANE)
			pPlanesF[2] = pRefFGOF->GetFrame(0)->GetPlane(VPLANE);
	}
	if (isUsableB)
	{
		pRefBGOF->Update(YUVplanes, (BYTE*)pRefB[0], nRefBPitches[0], (BYTE*)pRefB[1], nRefBPitches[1], (BYTE*)pRefB[2], nRefBPitches[2]);// v2.0
		if (YUVplanes & YPLANE)
			pPlanesB[0] = pRefBGOF->GetFrame(0)->GetPlane(YPLANE);
		if (YUVplanes & UPLANE)
			pPlanesB[1] = pRefBGOF->GetFrame(0)->GetPlane(UPLANE);
		if (YUVplanes & VPLANE)
			pPlanesB[2] = pRefBGOF->GetFrame(0)->GetPlane(VPLANE);
	}

	PROFILE_START(MOTION_PROFILE_COMPENSATION);
	pDstCur[0] = pDst[0];
	pDstCur[1] = pDst[1];
	pDstCur[2] = pDst[2];
	pSrcCur[0] = pSrc[0];
	pSrcCur[1] = pSrc[1];
	pSrcCur[2] = pSrc[2];

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// LUMA plane Y

	if (!(YUVplanes & YPLANE))
	{
		BitBlt(pDstCur[0], nDstPitches[0], pSrcCur[0], nSrcPitches[0], nWidth*pixelsize, nHeight, isse_flag);
	}

	else
	{
		if (nOverlapX==0 && nOverlapY==0)
		{
			for (int by=0; by<nBlkY; by++)
			{
				int xx = 0;
				for (int bx=0; bx<nBlkX; bx++)
				{
					int i = by*nBlkX + bx;
					const BYTE * pB, *pF;
					int npB, npF;
					int WSrc, WRefB, WRefF;

					use_block_y (pB , npB , WRefB , isUsableB , mvClipB , i, pPlanesB  [0], pSrcCur [0], xx, nSrcPitches [0]);
					use_block_y (pF , npF , WRefF , isUsableF , mvClipF , i, pPlanesF  [0], pSrcCur [0], xx, nSrcPitches [0]);
					norm_weights (WSrc, WRefB, WRefF);

					// luma
					DEGRAINLUMA(pDstCur[0] + xx, pDstCur[0] + lsb_offset_y + xx,
						lsb_flag, nDstPitches[0], pSrcCur[0]+ xx, nSrcPitches[0],
						pB, npB, pF, npF, WSrc, WRefB, WRefF);

					xx += (nBlkSizeX) * pixelsize;

					if (bx == nBlkX-1 && nWidth_B < nWidth) // right non-covered region
					{
						// luma
						BitBlt(pDstCur[0] + nWidth_B*pixelsize, nDstPitches[0],
							pSrcCur[0] + nWidth_B*pixelsize, nSrcPitches[0], (nWidth-nWidth_B)*pixelsize, nBlkSizeY, isse_flag);
					}
				}	// for bx

				pDstCur[0] += ( nBlkSizeY ) * (nDstPitches[0]);
				pSrcCur[0] += ( nBlkSizeY ) * (nSrcPitches[0]);

				if (by == nBlkY-1 && nHeight_B < nHeight) // bottom uncovered region
				{
					// luma
					BitBlt(pDstCur[0], nDstPitches[0], pSrcCur[0], nSrcPitches[0], nWidth*pixelsize, nHeight-nHeight_B, isse_flag);
				}
			}	// for by
		}	// nOverlapX==0 && nOverlapY==0

// -----------------------------------------------------------------

		else // overlap
		{
			unsigned short *pDstShort = DstShort;
			int *pDstInt = DstInt;
			const int tmpPitch = nBlkSizeX;

			if (lsb_flag)
			{
				MemZoneSet(reinterpret_cast<unsigned char*>(pDstInt), 0,
					nWidth_B*4, nHeight_B, 0, 0, dstIntPitch*4);
			}
			else
			{
				MemZoneSet(reinterpret_cast<unsigned char*>(pDstShort), 0,
					nWidth_B*2, nHeight_B, 0, 0, dstShortPitch*2);
			}

			for (int by=0; by<nBlkY; by++)
			{
				int wby = ((by + nBlkY - 3)/(nBlkY - 2))*3;
				int xx = 0;
				for (int bx=0; bx<nBlkX; bx++)
				{
					// select window
					int wbx = (bx + nBlkX - 3)/(nBlkX - 2);
					short *			winOver = OverWins->GetWindow(wby + wbx);

					int i = by*nBlkX + bx;
					const BYTE * pB, *pF;
					int npB, npF;
					int WSrc, WRefB, WRefF;

					use_block_y (pB , npB , WRefB , isUsableB , mvClipB , i, pPlanesB  [0], pSrcCur [0], xx, nSrcPitches [0]);
					use_block_y (pF , npF , WRefF , isUsableF , mvClipF , i, pPlanesF  [0], pSrcCur [0], xx, nSrcPitches [0]);
					norm_weights (WSrc, WRefB, WRefF);

					// luma
					DEGRAINLUMA(tmpBlock, tmpBlockLsb, lsb_flag, tmpPitch, pSrcCur[0]+ xx, nSrcPitches[0],
						pB, npB, pF, npF, WSrc, WRefB, WRefF);
					if (lsb_flag)
					{
						OVERSLUMALSB(pDstInt + xx, dstIntPitch, tmpBlock, tmpBlockLsb, tmpPitch, winOver, nBlkSizeX);
					}
					else
					{
						OVERSLUMA(pDstShort + xx, dstShortPitch, tmpBlock, tmpPitch, winOver, nBlkSizeX);
					}

					xx += (nBlkSizeX - nOverlapX);
				}	// for bx

				pSrcCur[0] += (nBlkSizeY - nOverlapY) * (nSrcPitches[0]);
				pDstShort += (nBlkSizeY - nOverlapY) * dstShortPitch;
				pDstInt += (nBlkSizeY - nOverlapY) * dstIntPitch;
			}	// for by
			if (lsb_flag)
			{ // todo pixelsize
				Short2BytesLsb(pDst[0], pDst[0] + lsb_offset_y, nDstPitches[0], DstInt, dstIntPitch, nWidth_B, nHeight_B);
			}
			else
			{
				Short2Bytes(pDst[0], nDstPitches[0], DstShort, dstShortPitch, nWidth_B, nHeight_B);
			}
			if (nWidth_B < nWidth)
			{
				BitBlt(pDst[0] + nWidth_B*pixelsize, nDstPitches[0],
					pSrc[0] + nWidth_B*pixelsize, nSrcPitches[0],
					(nWidth-nWidth_B) * pixelsize, nHeight_B, isse_flag);
			}
			if (nHeight_B < nHeight) // bottom noncovered region
			{
				BitBlt(pDst[0] + nHeight_B*nDstPitches[0], nDstPitches[0],
					pSrc[0] + nHeight_B*nSrcPitches[0], nSrcPitches[0],
					nWidth, nHeight-nHeight_B, isse_flag);
			}
		}	// overlap - end

		if (nLimit < (pixelsize == 1 ? 255 : 65535))
		{
			if ((pixelsize==1) && isse_flag)
			{
				LimitChanges_sse2(pDst[0], nDstPitches[0], pSrc[0], nSrcPitches[0], nWidth, nHeight, nLimit);
			}
			else
			{
                if(pixelsize==1)
				  LimitChanges_c<uint8_t>(pDst[0], nDstPitches[0], pSrc[0], nSrcPitches[0], nWidth, nHeight, nLimit);
                else
                  LimitChanges_c<uint16_t>(pDst[0], nDstPitches[0], pSrc[0], nSrcPitches[0], nWidth, nHeight, nLimit);
            }
		}
	}

//----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// CHROMA plane U

	process_chroma (
		UPLANE & nSuperModeYUV,
		pDst [1], pDstCur [1], nDstPitches [1], pSrc [1], pSrcCur [1], nSrcPitches [1],
		isUsableB, isUsableF, pPlanesB [1], pPlanesF [1],
		lsb_offset_u, nWidth_B, nHeight_B
	);

//----------------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// CHROMA plane V

	process_chroma (
		VPLANE & nSuperModeYUV,
		pDst [2], pDstCur [2], nDstPitches [2], pSrc [2], pSrcCur [2], nSrcPitches [2],
		isUsableB, isUsableF, pPlanesB [2], pPlanesF [2],
		lsb_offset_v, nWidth_B, nHeight_B
	);

//--------------------------------------------------------------------------------

#ifndef _M_X64
  _mm_empty ();	// (we may use double-float somewhere) Fizick
#endif
	PROFILE_STOP(MOTION_PROFILE_COMPENSATION);


	if ( (pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2 && !planar)
	{
		YUY2FromPlanes(pDstYUY2, nDstPitchYUY2, nWidth, nHeight * height_lsb_mul,
					  pDst[0], nDstPitches[0], pDst[1], pDst[2], nDstPitches[1], isse_flag);
	}
   return dst;
}



void	MVDegrain1::process_chroma (int plane_mask, BYTE *pDst, BYTE *pDstCur, int nDstPitch, const BYTE *pSrc, const BYTE *pSrcCur, int nSrcPitch, bool isUsableB, bool isUsableF, MVPlane *pPlanesB, MVPlane *pPlanesF, int lsb_offset_uv, int nWidth_B, int nHeight_B)
{
	if (!(YUVplanes & plane_mask))
	{
		BitBlt(pDstCur, nDstPitch, pSrcCur, nSrcPitch, nWidth/xRatioUV * pixelsize, nHeight/yRatioUV, isse_flag);
	}

	else
	{
		if (nOverlapX==0 && nOverlapY==0)
		{
			for (int by=0; by<nBlkY; by++)
			{
				int xx = 0;
				for (int bx=0; bx<nBlkX; bx++)
				{
					int i = by*nBlkX + bx;
					const BYTE * pBV, *pFV;
					int npBV, npFV;
					int WSrc, WRefB, WRefF;

					use_block_uv (pBV , npBV , WRefB , isUsableB , mvClipB , i, pPlanesB , pSrcCur, xx, nSrcPitch);
					use_block_uv (pFV , npFV , WRefF , isUsableF , mvClipF , i, pPlanesF , pSrcCur, xx, nSrcPitch);
					norm_weights (WSrc, WRefB, WRefF);

					// chroma
					DEGRAINCHROMA(pDstCur + (xx/xRatioUV), pDstCur + (xx/xRatioUV) + lsb_offset_uv,
						lsb_flag, nDstPitch, pSrcCur + (xx/xRatioUV), nSrcPitch,
						pBV, npBV, pFV, npFV, WSrc, WRefB, WRefF);

					xx += (nBlkSizeX)*pixelsize;

					if (bx == nBlkX-1 && nWidth_B < nWidth) // right non-covered region
					{
						// chroma
						BitBlt(pDstCur + (nWidth_B/xRatioUV)*pixelsize, nDstPitch,
							pSrcCur + (nWidth_B/xRatioUV)*pixelsize, nSrcPitch, (nWidth-nWidth_B)/xRatioUV*pixelsize, (nBlkSizeY)/yRatioUV, isse_flag);
					}
				}	// for bx

				pDstCur += ( nBlkSizeY )/yRatioUV * nDstPitch;
				pSrcCur += ( nBlkSizeY )/yRatioUV * nSrcPitch;

				if (by == nBlkY-1 && nHeight_B < nHeight) // bottom uncovered region
				{
					// chroma
					BitBlt(pDstCur, nDstPitch, pSrcCur, nSrcPitch, nWidth/xRatioUV*pixelsize, (nHeight-nHeight_B)/yRatioUV, isse_flag);
				}
			}	// for by
		}	// nOverlapX==0 && nOverlapY==0

// -----------------------------------------------------------------

		else // overlap
		{
			unsigned short *pDstShort = DstShort;
			int *pDstInt = DstInt;
			const int tmpPitch = nBlkSizeX;

			if (lsb_flag)
			{
				MemZoneSet(reinterpret_cast<unsigned char*>(pDstInt), 0,
					nWidth_B*4/xRatioUV, nHeight_B/yRatioUV, 0, 0, dstIntPitch*4);
			}
			else
			{
				MemZoneSet(reinterpret_cast<unsigned char*>(pDstShort), 0,
					nWidth_B*2/xRatioUV, nHeight_B/yRatioUV, 0, 0, dstShortPitch*2);
			}

			for (int by=0; by<nBlkY; by++)
			{
				int wby = ((by + nBlkY - 3)/(nBlkY - 2))*3;
				int xx = 0;
				for (int bx=0; bx<nBlkX; bx++)
				{
					// select window
					int wbx = (bx + nBlkX - 3)/(nBlkX - 2);
					short *			winOverUV = OverWinsUV->GetWindow(wby + wbx);

					int i = by*nBlkX + bx;
					const BYTE * pBV, *pFV;
					int npBV, npFV;
					int WSrc, WRefB, WRefF;

					use_block_uv (pBV , npBV , WRefB , isUsableB , mvClipB , i, pPlanesB , pSrcCur, xx, nSrcPitch);
					use_block_uv (pFV , npFV , WRefF , isUsableF , mvClipF , i, pPlanesF , pSrcCur, xx, nSrcPitch);
					norm_weights (WSrc, WRefB, WRefF);

					// chroma
					DEGRAINCHROMA(tmpBlock, tmpBlockLsb, lsb_flag, tmpPitch, pSrcCur + (xx/xRatioUV)*pixelsize, nSrcPitch,
						pBV, npBV, pFV, npFV, WSrc, WRefB, WRefF);
					if (lsb_flag)
					{
						OVERSCHROMALSB(pDstInt + (xx/xRatioUV), dstIntPitch, tmpBlock, tmpBlockLsb, tmpPitch, winOverUV, nBlkSizeX/xRatioUV);
					}
					else
					{
						OVERSCHROMA(pDstShort + (xx/xRatioUV), dstShortPitch, tmpBlock, tmpPitch, winOverUV, nBlkSizeX/xRatioUV);
					}

					xx += (nBlkSizeX - nOverlapX); // no *pixelsize here, we work info pDstShort or pDstInt

				}	// for bx

				pSrcCur += (nBlkSizeY - nOverlapY)/yRatioUV * nSrcPitch ;
				pDstShort += (nBlkSizeY - nOverlapY)/yRatioUV * dstShortPitch;
				pDstInt += (nBlkSizeY - nOverlapY)/yRatioUV * dstIntPitch;
			}	// for by

			if (lsb_flag)
			{  // todo pixelsize
				Short2BytesLsb(pDst, pDst + lsb_offset_uv, nDstPitch, DstInt, dstIntPitch, nWidth_B/xRatioUV, nHeight_B/yRatioUV);
			}
			else
			{
				Short2Bytes(pDst, nDstPitch, DstShort, dstShortPitch, nWidth_B/xRatioUV, nHeight_B/yRatioUV);
			}
			if (nWidth_B < nWidth)
			{
				BitBlt(pDst + (nWidth_B/xRatioUV)*pixelsize, nDstPitch,
					pSrc + (nWidth_B/xRatioUV)*pixelsize, nSrcPitch,
					(nWidth-nWidth_B)/xRatioUV*pixelsize, nHeight_B/yRatioUV, isse_flag);
			}
			if (nHeight_B < nHeight) // bottom noncovered region
			{
				BitBlt(pDst + nDstPitch*nHeight_B/yRatioUV, nDstPitch,
					pSrc + nSrcPitch*nHeight_B/yRatioUV, nSrcPitch,
					nWidth/xRatioUV*pixelsize, (nHeight-nHeight_B)/yRatioUV, isse_flag);
			}
		}	// overlap - end

		if (nLimitC < (pixelsize == 1 ? 255 : 65535))
		{
			if ((pixelsize==1) && isse_flag)
			{
				LimitChanges_sse2(pDst, nDstPitch, pSrc, nSrcPitch, nWidth/xRatioUV, nHeight/yRatioUV, nLimitC);
			}
			else
			{
                if(pixelsize==1)
                    LimitChanges_c<uint8_t>(pDst, nDstPitch, pSrc, nSrcPitch, nWidth/xRatioUV, nHeight/yRatioUV, nLimitC);
                else
                    LimitChanges_c<uint16_t>(pDst, nDstPitch, pSrc, nSrcPitch, nWidth/xRatioUV, nHeight/yRatioUV, nLimitC);
            }
		}
	}
}


void	MVDegrain1::use_block_y (const BYTE * &p, int &np, int &WRef, bool isUsable, const MVClip &mvclip, int i, const MVPlane *pPlane, const BYTE *pSrcCur, int xx, int nSrcPitch)
{
	if (isUsable)
	{
		const FakeBlockData &block = mvclip.GetBlock(0, i);
		int blx = block.GetX() * nPel + block.GetMV().x;
		int bly = block.GetY() * nPel + block.GetMV().y;
		p = pPlane->GetPointer(blx, bly);
		np = pPlane->GetPitch();
		int blockSAD = block.GetSAD();
		WRef = DegrainWeight(thSAD, blockSAD);
	}
	else
	{
		p = pSrcCur + xx;
		np = nSrcPitch;
		WRef = 0;
	}
}

void	MVDegrain1::use_block_uv (const BYTE * &p, int &np, int &WRef, bool isUsable, const MVClip &mvclip, int i, const MVPlane *pPlane, const BYTE *pSrcCur, int xx, int nSrcPitch)
{
	if (isUsable)
	{
		const FakeBlockData &block = mvclip.GetBlock(0, i);
		int blx = block.GetX() * nPel + block.GetMV().x;
		int bly = block.GetY() * nPel + block.GetMV().y;
		p = pPlane->GetPointer(blx/xRatioUV, bly/yRatioUV);
		np = pPlane->GetPitch();
		int blockSAD = block.GetSAD();
		WRef = DegrainWeight(thSADC, blockSAD);
	}
	else
	{
		p = pSrcCur + (xx/xRatioUV);
		np = nSrcPitch;
		WRef = 0;
	}
}



void	MVDegrain1::norm_weights (int &WSrc, int &WRefB, int &WRefF)
{
	WSrc = 256;
	int WSum = WRefB + WRefF + WSrc + 1;
	WRefB  = WRefB*256/WSum; // normalize weights to 256
	WRefF  = WRefF*256/WSum;
	WSrc = 256 - WRefB - WRefF;
}
#endif // #if 0