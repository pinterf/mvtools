// Pixels flow motion blur function
// Copyright(c)2005 A.G.Balakhnin aka Fizick

// See legal notice in Copying.txt for more information

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation;  version 2 of the License.
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
#include "MaskFun.h"
#include "MVFinest.h"
#include "MVFlowBlur.h"
#include "SuperParams64Bits.h"


MVFlowBlur::MVFlowBlur(PClip _child, PClip super, PClip _mvbw, PClip _mvfw,  int _blur256, int _prec,
                           int nSCD1, int nSCD2, bool _isse, bool _planar, IScriptEnvironment* env) :
GenericVideoFilter(_child),
MVFilter(_mvfw, "MFlowBlur", env, 1, 0),
mvClipB(_mvbw, nSCD1, nSCD2, env, 1, 0),
mvClipF(_mvfw, nSCD1, nSCD2, env, 1, 0)
{
   blur256 = _blur256;
   prec = _prec;
   isse = _isse;
   planar = _planar;

   CheckSimilarity(mvClipB, "mvbw", env);
   CheckSimilarity(mvClipF, "mvfw", env);
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

	if (   nHeight != nHeightS
	    || nWidth  != nSuperWidth - nSuperHPad * 2
	    || nPel    != nSuperPel)
	{
		env->ThrowError("MFlowBlur : wrong super frame clip");
	}

	if (nPel==1)
		finest = super; // v2.0.9.1
	else
	{
		finest = new MVFinest(super, isse, env);
		AVSValue cache_args[1] = { finest };
		finest = env->Invoke("InternalCache", AVSValue(cache_args,1)).AsClip(); // add cache for speed
	}

//	if (   nWidth  != vi.width  || (nWidth  + nHPadding*2)*nPel != finest->GetVideoInfo().width
//	    || nHeight != vi.height || (nHeight + nVPadding*2)*nPel != finest->GetVideoInfo().height)
//		env->ThrowError("MVFlowBlur: wrong source of finest frame size");


	nHeightUV = nHeight/yRatioUV;
	nWidthUV = nWidth/2;// for YV12
	nHPaddingUV = nHPadding/2;
	nVPaddingUV = nHPadding/yRatioUV;

	VPitchY = nWidth;
	VPitchUV= nWidthUV;

	VXFullYB = new BYTE [nHeight*VPitchY];
	VXFullUVB = new BYTE [nHeightUV*VPitchUV];
	VYFullYB = new BYTE [nHeight*VPitchY];
	VYFullUVB = new BYTE [nHeightUV*VPitchUV];

	VXFullYF = new BYTE [nHeight*VPitchY];
	VXFullUVF = new BYTE [nHeightUV*VPitchUV];
	VYFullYF = new BYTE [nHeight*VPitchY];
	VYFullUVF = new BYTE [nHeightUV*VPitchUV];

	VXSmallYB = new BYTE [nBlkX*nBlkY];
	VYSmallYB = new BYTE [nBlkX*nBlkY];
	VXSmallUVB = new BYTE [nBlkX*nBlkY];
	VYSmallUVB = new BYTE [nBlkX*nBlkY];

	VXSmallYF = new BYTE [nBlkX*nBlkY];
	VYSmallYF = new BYTE [nBlkX*nBlkY];
	VXSmallUVF = new BYTE [nBlkX*nBlkY];
	VYSmallUVF = new BYTE [nBlkX*nBlkY];

	MaskSmallB = new BYTE [nBlkX*nBlkY];
	MaskFullYB = new BYTE [nHeight*VPitchY];
	MaskFullUVB = new BYTE [nHeightUV*VPitchUV];

	MaskSmallF = new BYTE [nBlkX*nBlkY];
	MaskFullYF = new BYTE [nHeight*VPitchY];
	MaskFullUVF = new BYTE [nHeightUV*VPitchUV];

	int CPUF_Resize = env->GetCPUFlags();
	if (!isse) CPUF_Resize = (CPUF_Resize & !CPUF_INTEGER_SSE) & !CPUF_SSE2;

	upsizer = new SimpleResize(nWidth, nHeight, nBlkX, nBlkY, CPUF_Resize);
	upsizerUV = new SimpleResize(nWidthUV, nHeightUV, nBlkX, nBlkY, CPUF_Resize);

	if ( (pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2 && !planar)
	{
		DstPlanes =  new YUY2Planes(nWidth, nHeight);
	}
}

MVFlowBlur::~MVFlowBlur()
{
   if ( (pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2  && !planar)
   {
		delete DstPlanes;
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

	delete [] MaskSmallB;
	delete [] MaskFullYB;
	delete [] MaskFullUVB;
	delete [] MaskSmallF;
	delete [] MaskFullYF;
	delete [] MaskFullUVF;

}


void MVFlowBlur::FlowBlur(BYTE * pdst, int dst_pitch, const BYTE *pref, int ref_pitch,
			   BYTE *VXFullB, BYTE *VXFullF, BYTE *VYFullB, BYTE *VYFullF,
			   int VPitch, int width, int height, int blur256, int prec)
{
	// very slow, but precise motion blur
	if (nPel==1)
	{
		for (int h=0; h<height; h++)
		{
			for (int w=0; w<width; w++)
			{
				int bluredsum = pref[w];
				int vxF0 = ((VXFullF[w]-128)*blur256);
				int vyF0 = ((VYFullF[w]-128)*blur256);
				int mF = (std::max(abs(vxF0), abs(vyF0))/prec)>>8;
				if (mF>0)
				{
					vxF0 /= mF;
					vyF0 /= mF;
					int vxF = vxF0;
					int vyF = vyF0;
					for (int i=0; i<mF; i++)
					{
						int dstF = pref[(vyF>>8)*ref_pitch + (vxF>>8) + w];
						bluredsum += dstF;
						vxF += vxF0;
						vyF += vyF0;
					}
				}
				int vxB0 = ((VXFullB[w]-128)*blur256);
				int vyB0 = ((VYFullB[w]-128)*blur256);
				int mB = (std::max(abs(vxB0), abs(vyB0))/prec)>>8;
				if (mB>0)
				{
					vxB0 /= mB;
					vyB0 /= mB;
					int vxB = vxB0;
					int vyB = vyB0;
					for (int i=0; i<mB; i++)
					{
						int dstB = pref[(vyB>>8)*ref_pitch + (vxB>>8) + w];
						bluredsum += dstB;
						vxB += vxB0;
						vyB += vyB0;
					}
				}
				pdst[w] = bluredsum/(mF+mB+1);
			}
			pdst += dst_pitch;
			pref += ref_pitch;
			VXFullB += VPitch;
			VYFullB += VPitch;
			VXFullF += VPitch;
			VYFullF += VPitch;
		}
	}
	else if (nPel==2)
	{
		for (int h=0; h<height; h++)
		{
			for (int w=0; w<width; w++)
			{
				int bluredsum = pref[w<<1];
				int vxF0 = ((VXFullF[w]-128)*blur256);
				int vyF0 = ((VYFullF[w]-128)*blur256);
				int mF = (std::max(abs(vxF0), abs(vyF0))/prec)>>8;
				if (mF>0)
				{
					vxF0 /= mF;
					vyF0 /= mF;
					int vxF = vxF0;
					int vyF = vyF0;
					for (int i=0; i<mF; i++)
					{
						int dstF = pref[(vyF>>8)*ref_pitch + (vxF>>8) + (w<<1)];
						bluredsum += dstF;
						vxF += vxF0;
						vyF += vyF0;
					}
				}
				int vxB0 = ((VXFullB[w]-128)*blur256);
				int vyB0 = ((VYFullB[w]-128)*blur256);
				int mB = (std::max(abs(vxB0), abs(vyB0))/prec)>>8;
				if (mB>0)
				{
					vxB0 /= mB;
					vyB0 /= mB;
					int vxB = vxB0;
					int vyB = vyB0;
					for (int i=0; i<mB; i++)
					{
						int dstB = pref[(vyB>>8)*ref_pitch + (vxB>>8) + (w<<1)];
						bluredsum += dstB;
						vxB += vxB0;
						vyB += vyB0;
					}
				}
				pdst[w] = bluredsum/(mF+mB+1);
			}
			pdst += dst_pitch;
			pref += (ref_pitch<<1);
			VXFullB += VPitch;
			VYFullB += VPitch;
			VXFullF += VPitch;
			VYFullF += VPitch;
		}
	}
	else if (nPel==4)
	{
		for (int h=0; h<height; h++)
		{
			for (int w=0; w<width; w++)
			{
				int bluredsum = pref[w<<2];
				int vxF0 = ((VXFullF[w]-128)*blur256);
				int vyF0 = ((VYFullF[w]-128)*blur256);
				int mF = (std::max(abs(vxF0), abs(vyF0))/prec)>>8;
				if (mF>0)
				{
					vxF0 /= mF;
					vyF0 /= mF;
					int vxF = vxF0;
					int vyF = vyF0;
					for (int i=0; i<mF; i++)
					{
						int dstF = pref[(vyF>>8)*ref_pitch + (vxF>>8) + (w<<2)];
						bluredsum += dstF;
						vxF += vxF0;
						vyF += vyF0;
					}
				}
				int vxB0 = ((VXFullB[w]-128)*blur256);
				int vyB0 = ((VYFullB[w]-128)*blur256);
				int mB = (std::max(abs(vxB0), abs(vyB0))/prec)>>8;
				if (mB>0)
				{
					vxB0 /= mB;
					vyB0 /= mB;
					int vxB = vxB0;
					int vyB = vyB0;
					for (int i=0; i<mB; i++)
					{
						int dstB = pref[(vyB>>8)*ref_pitch + (vxB>>8) + (w<<2)];
						bluredsum += dstB;
						vxB += vxB0;
						vyB += vyB0;
					}
				}
				pdst[w] = bluredsum/(mF+mB+1);
			}
			pdst += dst_pitch;
			pref += (ref_pitch<<2);
			VXFullB += VPitch;
			VYFullB += VPitch;
			VXFullF += VPitch;
			VYFullF += VPitch;
		}
	}
}
//-------------------------------------------------------------------------
PVideoFrame __stdcall MVFlowBlur::GetFrame(int n, IScriptEnvironment* env)
{
   PVideoFrame dst;
   BYTE *pDst[3];
	const BYTE *pRef[3];
    int nDstPitches[3], nRefPitches[3];
	unsigned char *pDstYUY2;
	int nDstPitchYUY2;

   int off = mvClipB.GetDeltaFrame(); // integer offset of reference frame
	if (off <= 0)
	{
		env->ThrowError ("MFlowBlur: cannot use motion vectors with absolute frame references.");
	}
   if (!mvClipB.IsBackward())
	   off = - off;

	PVideoFrame mvF = mvClipF.GetFrame(n+off, env);
   mvClipF.Update(mvF, env);// forward from current to next
   mvF=0;

	PVideoFrame mvB = mvClipB.GetFrame(n-off, env);
   mvClipB.Update(mvB, env);// backward from current to prev
   mvB =0;

   if ( mvClipB.IsUsable()  && mvClipF.IsUsable() )
   {
		PVideoFrame ref = finest->GetFrame(n, env);//  ref for  compensation
		dst = env->NewVideoFrame(vi);

		if ( (pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2 )
		{
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
		}

    int nOffsetY = nRefPitches[0] * nVPadding*nPel + nHPadding*nPel;
    int nOffsetUV = nRefPitches[1] * nVPaddingUV*nPel + nHPaddingUV*nPel;


	  // make  vector vx and vy small masks
	 // 1. ATTENTION: vectors are assumed SHORT (|vx|, |vy| < 127) !
	 // 2. they will be zeroed if not
	// 3. added 128 to all values
	MakeVectorSmallMasks(mvClipB, nBlkX, nBlkY, VXSmallYB, nBlkX, VYSmallYB, nBlkX);
	VectorSmallMaskYToHalfUV(VXSmallYB, nBlkX, nBlkY, VXSmallUVB, 2);
	VectorSmallMaskYToHalfUV(VYSmallYB, nBlkX, nBlkY, VYSmallUVB, yRatioUV);

	MakeVectorSmallMasks(mvClipF, nBlkX, nBlkY, VXSmallYF, nBlkX, VYSmallYF, nBlkX);
	VectorSmallMaskYToHalfUV(VXSmallYF, nBlkX, nBlkY, VXSmallUVF, 2);
	VectorSmallMaskYToHalfUV(VYSmallYF, nBlkX, nBlkY, VYSmallUVF, yRatioUV);

	  // analyse vectors field to detect occlusion

	  // upsize (bilinear interpolate) vector masks to fullframe size


	  int dummyplane = PLANAR_Y; // use luma plane resizer code for all planes if we resize from luma small mask
	  upsizer->SimpleResizeDo(VXFullYB, nWidth, nHeight, VPitchY, VXSmallYB, nBlkX, nBlkX, dummyplane);
	  upsizer->SimpleResizeDo(VYFullYB, nWidth, nHeight, VPitchY, VYSmallYB, nBlkX, nBlkX, dummyplane);
	  upsizerUV->SimpleResizeDo(VXFullUVB, nWidthUV, nHeightUV, VPitchUV, VXSmallUVB, nBlkX, nBlkX, dummyplane);
	  upsizerUV->SimpleResizeDo(VYFullUVB, nWidthUV, nHeightUV, VPitchUV, VYSmallUVB, nBlkX, nBlkX, dummyplane);

	  upsizer->SimpleResizeDo(VXFullYF, nWidth, nHeight, VPitchY, VXSmallYF, nBlkX, nBlkX, dummyplane);
	  upsizer->SimpleResizeDo(VYFullYF, nWidth, nHeight, VPitchY, VYSmallYF, nBlkX, nBlkX, dummyplane);
	  upsizerUV->SimpleResizeDo(VXFullUVF, nWidthUV, nHeightUV, VPitchUV, VXSmallUVF, nBlkX, nBlkX, dummyplane);
	  upsizerUV->SimpleResizeDo(VYFullUVF, nWidthUV, nHeightUV, VPitchUV, VYSmallUVF, nBlkX, nBlkX, dummyplane);


			FlowBlur(pDst[0], nDstPitches[0], pRef[0] + nOffsetY, nRefPitches[0],
				VXFullYB, VXFullYF, VYFullYB, VYFullYF, VPitchY,
				nWidth, nHeight, blur256, prec);
			FlowBlur(pDst[1], nDstPitches[1], pRef[1] + nOffsetUV, nRefPitches[1],
				VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, VPitchUV,
				nWidthUV, nHeightUV, blur256, prec);
			FlowBlur(pDst[2], nDstPitches[2], pRef[2] + nOffsetUV, nRefPitches[2],
				VXFullUVB, VXFullUVF, VYFullUVB, VYFullUVF, VPitchUV,
				nWidthUV, nHeightUV, blur256, prec);

		if ( (pixelType & VideoInfo::CS_YUY2) == VideoInfo::CS_YUY2 && !planar)
		{
			YUY2FromPlanes(pDstYUY2, nDstPitchYUY2, nWidth, nHeight,
								  pDst[0], nDstPitches[0], pDst[1], pDst[2], nDstPitches[1], isse);
		}
		return dst;
   }
   else
   {
	   return child->GetFrame(n, env);
   }

}
