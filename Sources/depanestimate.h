/*
    DePan plugin for Avisynth 2.5 - global motion estimation and compensation of camera pan
	Version 0.6, May 28, 2004. 
	(DePan header file)
	(c) 2004, A.G. Balakhnin aka Fizick
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

#include "windows.h"
#include "stdio.h"

#define MAX(x,y) ((x) > (y) ? (x) : (y))
#define MIN(x,y) ((x) < (y) ? (x) : (y))
//#define MOTIONUNKNOWN 9999
//#define MOTIONBAD 0


int read_depan_data(const BYTE *data, float motionx[], float motiony[], float motionzoom[], float motionrot[],int neededframe);
void write_depan_data(BYTE *dstp, int framefirst,int framelast, float motionx[], float motiony[], float motionzoom[]);
int depan_data_bytes(int framenumbers);
int read_deshakerlog(const char *inputlog, int num_frames, float motionx[], float motiony[], float motionrotd[], float motionzoom[] , int *loginterlaced);
void write_deshakerlog(FILE *logfile, int IsFieldBased, int IsTFF, int ndest, float motionx[], float motiony[], float motionzoom[]);

#endif
