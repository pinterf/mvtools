/*
    DePan plugin for Avisynth 2.5 - global motion compensation
	Version 1.9, November 5, 2006. 
	(DePan header file)
	Copyright(c) 2004-2006, A.G. Balakhnin aka Fizick
	bag@hotmail.ru

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

	DePan plugin at first stage estimates global motion (pan) in frames (by phase-shift method),
	and at second stage shifts frame images for global motion compensation
*/
#ifndef __DEPAN_H__
#define __DEPAN_H__

#include <stdint.h>
#include <type_traits>

//#define MAX(x,y) ((x) > (y) ? (x) : (y))
//#define MIN(x,y) ((x) < (y) ? (x) : (y))
//#define MOTIONUNKNOWN 9999
//#define MOTIONBAD 0

typedef struct transformstruct {  // structure of transform
	float dxc;
	float dxx;
	float dxy;
	float dyc;
	float dyx;
	float dyy;
} transform;

#define MIRROR_TOP 1
#define MIRROR_BOTTOM 2
#define MIRROR_LEFT 4
#define MIRROR_RIGHT 8


//void DrawString(PVideoFrame &dst, int x, int y, const char *s); 
void motion2transform (float dx, float dy, float rotdegree, float zoom, float pixaspect, float xcenter, float ycenter, int forward, float offsetfracture, transform *tr);
void sumtransform(transform ta, transform tb, transform *tba);
void transform2motion (transform, int forward, float xcenter, float ycenter, float pixaspect, float *dx, float *dy, float *rotd, float *zoom);
void inversetransform(transform ta, transform *tinv);

/*
void compensate_plane_bicubic (uint8_t *dstp,  int dst_pitch, const uint8_t * srcp,  int src_pitch,  int src_width, int src_height, transform tr, int mirror, int border, int * work2width1030, int blurmax);
void compensate_plane_bilinear (uint8_t *dstp,  int dst_pitch, const uint8_t * srcp,  int src_pitch,  int src_width, int src_height, transform tr, int mirror, int border, int * work2width, int blurmax);
void compensate_plane_nearest (uint8_t *dstp,  int dst_pitch, const uint8_t * srcp,  int src_pitch,  int src_width, int src_height, transform tr, int mirror, int border, int * work1width, int blurmax);
*/
template <typename pixel_t>
void compensate_plane_nearest2(uint8_t *dstp8, int dst_pitch, const uint8_t * srcp8, int src_pitch, int src_width, int src_height, transform tr, int mirror, int border, int blurmax, int bits_per_pixel);

template <typename pixel_t>
void compensate_plane_bilinear2(uint8_t *dstp8, int dst_pitch, const uint8_t * srcp8, int src_pitch, int row_size, int height, transform tr, int mirror, int border, int blurmax, int bits_per_pixel);

template <typename pixel_t>
void compensate_plane_bicubic2(uint8_t *dstp8, int dst_pitch, const uint8_t * srcp8, int src_pitch, int row_size, int height, transform tr, int mirror, int border, int blurmax, int bits_per_pixel);

//void compensate_plane_nearest_stacked(uint8_t *dstp, int dst_pitch, const uint8_t * srcp, int src_pitch, int src_width, int src_height, transform tr, int mirror, int border, int * work1width, int blurmax);

//int read_depan_data(const uint8_t *data, float motionx[], float motiony[], float motionzoom[], float motionrot[],int neededframe);
//void write_depan_data(uint8_t *dstp, int framefirst,int framelast, float motionx[], float motiony[], float motionzoom[]);
//int depan_data_bytes(int framenumbers);
//int read_deshakerlog(const char *inputlog, int num_frames, float motionx[], float motiony[], float motionrotd[], float motionzoom[] , int *loginterlaced);
//void write_deshakerlog(FILE *logfile, int IsFieldBased, int IsTFF, int ndest, float motionx[], float motiony[], float motionzoom[]);


#endif
