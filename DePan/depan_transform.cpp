/* 
  DePan plugin for Avisynth 2.6 interface - global motion estimation and compensation of camera pan
  Version 1.1.4, December 15, 2004.
  Version 1.9, November 5, 2006.
  Version 1.10.0, April 29, 2007
  Version 1.10.1
  Version 1.13, February 18, 2016.
  Version 1.13.1, April 6, 2016
  Version 2.13.1, November 19, 2016 by pinterf
  (motion transform internal functions)
  Copyright(c)2004-2016, A.G. Balakhnin aka Fizick
  bag@hotmail.ru

  10-16 bit depth support for Avisynth+ by pinterf

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifdef _WIN32
#define NOGDI
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "windows.h"
#endif

#include "math.h"

#include "depan.h"

//****************************************************************************
//  motion to transform
//  get  coefficients for coordinates transformation,
//  which defines source (xsrc, ysrc)  for current destination (x,y)
//
//  delta is fracture of deformation (from 0 to 1 for forward,  from -1 to 0 for backward time direction)
//
//  
//  return:
//  vector: 
//   t[0] = dxc, t[1] = dxx, t[2] = dxy, t[3] = dyc, t[4] = dyx, t[5] = dyy
//
//
//   xsrc = dxc + dxx*x + dxy*y
//   ysrc = dyc + dyx*x + dyy*y
//
//  if no rotation, then dxy, dyx = 0, 
//  if no rotation and zoom, then also dxx, dyy = 1.
//
void motion2transform (float dx1, float dy1, float rot, float zoom1, float pixaspect, float xcenter, float ycenter, int forward, float fractoffset, transform *tr)
{
  const float PI=3.1415926535897932384626433832795f;
  float rotradian, sinus, cosinus, dx, dy, zoom;

  // fractoffset > 0 for forward, <0 for backward
	dx = fractoffset*dx1;        
    dy = fractoffset*dy1;
    rotradian = fractoffset*rot*PI/180;   // from degree to radian
	if (fabs(rotradian) < 1e-6) rotradian = 0 ;  // for some stability of rounding precision
    zoom = expf(fractoffset*logf(zoom1));         // zoom**(fractoffset) = exp(fractoffset*ln(zoom))
	if (fabsf(zoom-1.0f)< 1e-6f) zoom = 1.;			// for some stability of rounding precision

	sinus = sinf(rotradian);
    cosinus = cosf(rotradian);
 
//	xcenter = row_size_p*(1+uv)/2.0;      //  middle x
//    ycenter = height_p*(1+uv)/2.0;       //  middle y

	if (forward !=0) {         //  get coefficients for forward      
        tr->dxc = xcenter + (-xcenter*cosinus + ycenter/pixaspect*sinus)*zoom + dx;// dxc            /(1+uv);
      	tr->dxx = cosinus*zoom; // dxx
      	tr->dxy = -sinus/pixaspect*zoom;  // dxy
      
      	tr->dyc = ycenter + ( ((-ycenter)/pixaspect*cosinus + (-xcenter)*sinus)*zoom + dy )*pixaspect;// dyc      /(1+uv);
      	tr->dyx = sinus*zoom*pixaspect; // dyx
      	tr->dyy = cosinus*zoom ;  // dyy
      }	
      else {					// coefficients for backward
		tr->dxc = xcenter + ( (-xcenter + dx)*cosinus - ((-ycenter)/pixaspect + dy)*sinus )*zoom;//     /(1+uv);
		tr->dxx = cosinus*zoom;
		tr->dxy = -sinus/pixaspect*zoom;
	
		tr->dyc = ycenter + ( ((-ycenter)/pixaspect +dy)*cosinus + (-xcenter + dx)*sinus )*zoom*pixaspect; //      /(1+uv);
		tr->dyx = sinus*zoom*pixaspect;
		tr->dyy = cosinus*zoom;
      }	
}

//****************************************************************************
//void transform2motion (float tr[], int forward, float xcenter, float ycenter, float pixaspect, float *dx, float *dy, float *rot, float *zoom)
void transform2motion (transform tr, int forward, float xcenter, float ycenter, float pixaspect, float *dx, float *dy, float *rot, float *zoom)
{
  const float PI=3.1415926535897932384626433832795f;
  float rotradian, sinus, cosinus;

	if (forward !=0) {         //  get motion for forward      
		rotradian = - atanf(pixaspect*tr.dxy/tr.dxx);
		*rot = rotradian*180/PI;
		sinus = sinf(rotradian);
	    cosinus = cosf(rotradian);
		*zoom = tr.dxx/cosinus;
		*dx = tr.dxc - xcenter - (-xcenter*cosinus + ycenter/pixaspect*sinus)*(*zoom);
		*dy = tr.dyc/pixaspect - ycenter/pixaspect - ((-ycenter)/pixaspect*cosinus + (-xcenter)*sinus)*(*zoom) ;// dyc      

     }	
      else {					// coefficients for backward
		rotradian = - atanf(pixaspect*tr.dxy/tr.dxx);
		*rot = rotradian*180/PI;
		sinus = sinf(rotradian);
	    cosinus = cosf(rotradian);
		*zoom = tr.dxx/cosinus;


//		tr.dxc/(*zoom) = xcenter/(*zoom) + (-xcenter + dx)*cosinus - ((-ycenter)/pixaspect + dy)*sinus ; 
//		tr.dyc/(*zoom)/pixaspect = ycenter/(*zoom)/pixaspect +  ((-ycenter)/pixaspect +dy)*cosinus + (-xcenter + dx)*sinus ; 
// *cosinus:
//		tr.dxc/(*zoom)*cosinus = xcenter/(*zoom)*cosinus + (-xcenter + dx)*cosinus*cosinus - ((-ycenter)/pixaspect + dy)*sinus*cosinus ;   
// *sinus:
//		tr.dyc/(*zoom)/pixaspect*sinus = ycenter/(*zoom)/pixaspect*sinus +  ((-ycenter)/pixaspect +dy)*cosinus*sinus + (-xcenter + dx)*sinus*sinus ;  
// summa:
//		tr.dxc/(*zoom)*cosinus + tr.dyc/(*zoom)/pixaspect*sinus = xcenter/(*zoom)*cosinus + (-xcenter + dx) + ycenter/(*zoom)/pixaspect*sinus   ;  
		*dx = tr.dxc/(*zoom)*cosinus + tr.dyc/(*zoom)/pixaspect*sinus - xcenter/(*zoom)*cosinus  + xcenter - ycenter/(*zoom)/pixaspect*sinus   ;  

// *sinus:
//		tr.dxc/(*zoom)*sinus = xcenter/(*zoom)*sinus + (-xcenter + dx)*cosinus*sinus - ((-ycenter)/pixaspect + dy)*sinus*sinus ;  
// *cosinus:
//		tr.dyc/(*zoom)/pixaspect*cosinus = ycenter/(*zoom)/pixaspect*cosinus +  ((-ycenter)/pixaspect +dy)*cosinus*cosinus + (-xcenter + dx)*sinus*cosinus ;    
// diff:
//		tr.dxc/(*zoom)*sinus - tr.dyc/(*zoom)/pixaspect*cosinus = xcenter/(*zoom)*sinus - (-ycenter/pixaspect + dy) - ycenter/(*zoom)/pixaspect*cosinus   ;
		*dy = - tr.dxc/(*zoom)*sinus + tr.dyc/(*zoom)/pixaspect*cosinus + xcenter/(*zoom)*sinus - (-ycenter/pixaspect) - ycenter/(*zoom)/pixaspect*cosinus;


      }	
}


//****************************************************************************
//  get  summary coefficients for summary combined coordinates transformation,
//  transform_BA = fransform_B ( transform_A )
//   t[0] = dxc, t[1] = dxx, t[2] = dxy, t[3] = dyc, t[4] = dyx, t[5] = dyy
//void sumtransform(float ta[], float tb[], float tba[])
void sumtransform(transform ta, transform tb, transform *tba)
{
//	float tba0, tba1, tba2, tba3, tba4, tba5;
	float dxc_ba, dxx_ba, dxy_ba, dyc_ba, dyx_ba, dyy_ba;  

	

	//dxc_ba = dxc_b + dxx_b*dxc_a + dxy_b*dyc_a
	dxc_ba = tb.dxc + tb.dxx*ta.dxc + tb.dxy*ta.dyc; // fixed stupid bug with rotation in v.1.8.2
//	tba0 = tb[0] + tb[1]*ta[0] + tb[2]*ta[3];

	//dxx_ba = dxx_b*dxx_a + dxy_b*dyx_a
	dxx_ba = tb.dxx*ta.dxx + tb.dxy*ta.dyx;
//	tba1 = tb[1]*ta[1] + tb[2]*ta[4];

	//dxy_ba = dxx_b*dxy_a + dxy_b*dyy_a
	dxy_ba = tb.dxx*ta.dxy + tb.dxy*ta.dyy;
//	tba2 = tb[1]*ta[2] + tb[2]*ta[5];

	//dyc_ba = dyc_b + dyx_b*dxc_a + dyy_b*dyc_a
	dyc_ba = tb.dyc + tb.dyx*ta.dxc + tb.dyy*ta.dyc;
//	tba3 = tb[3] + tb[4]*ta[0] + tb[5]*ta[3];

	//dyx_ba = dyx_b*dxx_a + dyy_b*dyx_a
	dyx_ba = tb.dyx*ta.dxx + tb.dyy*ta.dyx;
//	tba4 = tb[4]*ta[1] + tb[5]*ta[4];

	//dyy_ba = dyx_b*dxy_a + dyy_b*dyy_a
	dyy_ba = tb.dyx*ta.dxy + tb.dyy*ta.dyy;
//	tba5 = tb[4]*ta[2] + tb[5]*ta[5];

	tba->dxc = dxc_ba;
	tba->dxx = dxx_ba;
	tba->dxy = dxy_ba;
	tba->dyc = dyc_ba;
	tba->dyx = dyx_ba;
	tba->dyy = dyy_ba;
}

//****************************************************************************
//  get  coefficients for inverse coordinates transformation,
//  fransform_inv ( transform_A ) = null transform
void inversetransform(transform ta, transform *tinv)
{
	float pixaspect;

	if (ta.dxy !=0 ) pixaspect = sqrtf(- ta.dyx / ta.dxy);
	else pixaspect = 1.0;

	tinv->dxx = ta.dxx /( (ta.dxx)*ta.dxx + ta.dxy*ta.dxy*pixaspect*pixaspect);
	tinv->dyy = tinv->dxx;
	tinv->dxy = - tinv->dxx * ta.dxy / ta.dxx;
	tinv->dyx = - tinv->dxy * pixaspect*pixaspect;
	tinv->dxc = - tinv->dxx * ta.dxc - tinv->dxy * ta.dyc;
	tinv->dyc = - tinv->dyx * ta.dxc - tinv->dyy * ta.dyc;
}



