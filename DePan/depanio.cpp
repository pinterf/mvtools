/*
    DePan & DepanEstimate plugin for Avisynth 2.5 - global motion
  Version 1.9 November 5, 2006.
  (input-output internal functions)
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

*/

#include <avisynth.h>
#include "stdio.h"

#include "depanio.h"

#define DEPANSIGNATURE "depan06"


//****************************************************************************
typedef struct depanheaderstruct {  // structure of header depandata in framebuffer
  char signature[8];  // signature for check
  int reserved;  // for future using
  int nframes;  // number of records with frames motion data in current framebuffer
} depanheader;

typedef struct depandatastruct {  // structure of every frame data record in framebuffer
  int frame;  // frame number
  float dx;  // x shift (in pixels) for this frame
  float dy;  // y shift (in pixels, corresponded to pixel aspect = 1)
  float zoom; // zoom
  float rot;  // rotation (in degrees), =0 (no rotation estimated data in current version)
} depandata;

//****************************************************************************
// return size (bytes) of depan data
int depan_data_bytes(int framenumbers)
{
  depanheader header;
  depandata framedata;
  return (sizeof(header) + framenumbers * sizeof(framedata));
}


//****************************************************************************
// write coded motion data to start of special clip frame buffer
//
void write_depan_data(uint8_t *dstp, int framefirst, int framelast, float motionx[], float motiony[], float motionzoom[])
{
  char signaturegood[8] = DEPANSIGNATURE;
  depanheader header;
  depandata framedata;
  int frame;

  for (int i = 0; i < sizeof(signaturegood); i++) {
    header.signature[i] = signaturegood[i];
  }

  header.nframes = framelast - framefirst + 1; // number of frames to write motion info about

  int sizeheader = sizeof(header);
  int sizedata = sizeof(framedata);

  // write date to first line of frame
  memcpy(dstp, &header, sizeheader);
  dstp += sizeheader;

  for (frame = framefirst; frame < framelast + 1; frame++) {
    framedata.frame = frame; // some frame number
    framedata.dx = motionx[frame]; // motion x for frame
    framedata.dy = motiony[frame]; // motion y for frame
    framedata.zoom = motionzoom[frame]; // zoom for frame
    framedata.rot = 0; // rotation for frame - not estimated in current version
    memcpy(dstp, &framedata, sizedata);
    dstp += sizedata;
  }

}



//****************************************************************************
// read coded motion data from start of special data clip frame
//
int read_depan_data(const uint8_t *data, float motionx[], float motiony[], float motionzoom[], float motionrot[], int neededframe)
{
  char signaturegood[8] = DEPANSIGNATURE;
  depanheader header;
  depandata framedata;

  int i, w;

  int error;

  error = -1; // set firstly as BAD (frame data not found)

  // First line of frame
  memcpy(&header, data, sizeof(header)); // read magic word

  // compare readed magic word with good
  for (w = 0; w < sizeof(signaturegood); w++) {
    // every byte must be equal
    if (header.signature[w] != signaturegood[w]) {
      // bad compare, the frame is not from DePan data clip, return BAD
      return error;
    }

  }
  data += sizeof(header);

  // read motion data for frames from framebuffer
  for (i = 0; i < header.nframes; i++) {  // number of frames, for which motion data are writed
    memcpy(&framedata, data, sizeof(framedata)); // get frame number in motion table

    if (framedata.frame == neededframe) error = 0; // GOOD, needed frame is found!

    motionx[framedata.frame] = framedata.dx; // motion x for frame
    motiony[framedata.frame] = framedata.dy; // motion y for frame
    motionzoom[framedata.frame] = framedata.zoom; // motion zoom for frame
    motionrot[framedata.frame] = framedata.rot; // motion rotation for frame (degree)
    data += sizeof(framedata);

  }

  return error;  // return error code

}

//
//*************************************************************************
// read motion data from Deshaker (or DepanEstimate) log file
//
int read_deshakerlog(const char *inputlog, int num_frames, float motionx[], float motiony[], float motionrot[], float motionzoom[], int *loginterlaced)
{

  FILE *logfile;
  char line[48];
  int field;
  float dx = 0, dy = 0, rot = 0, zoom = 1;
  int i, n;

  // Open DESHAKER.LOG  file
  logfile = fopen(inputlog, "rt");
  if (logfile == NULL) return -1;  // file not found

//   Initialize motion data by nulls
//    for (i=0; i<2*num_frames+1; ++i) {
  for (n = 0; n < num_frames; n++) {
    motionx[n] = 0;
    motiony[n] = 0;
    motionrot[n] = 0;
    motionzoom[n] = 1;
  }

  // Read motion data from DESHAKER.LOG file
  while (!feof(logfile)) {
    fgets(line, 40, logfile);
    if ((line[6] == 'A') || (line[6] == 'a') || (line[6] == 'B') || (line[6] == 'b')) {
      i = 0;
      if (sscanf(line, "%6d%1x %f %f %f %f", &i, &field, &dx, &dy, &rot, &zoom) >= 5) {      // Interlaced
        //   decode frame (or field) number
        switch (field) {
        case 10:            // 0xa=10, field A of frame (first in time)
          i = i * 2;           // if interlaced, set number to field number (floatd frame number)
          *loginterlaced = 1;
          break;
        case 11:            // 0xb=11, field B of frame (second in time)
          i = i * 2 + 1;         // if interlaced, set number to field number (floatd frame number +1)
          *loginterlaced = 1;
          break;
        default:
          fclose(logfile);
          return -2; //  Error motion log file format
        }
        if (i >= num_frames) {
          fclose(logfile);
          return -3;  // Too many frames in log file
        }

        // Put motion data to arrays
        motionx[i] = dx;
        motiony[i] = dy;
        motionrot[i] = rot;
        motionzoom[i] = zoom;
      }
    }
    else {             // check as progressive
      i = 0;
      if (sscanf(line, "%7d %f %f %f %f", &i, &dx, &dy, &rot, &zoom) >= 5) {
        if (i >= num_frames) {
          fclose(logfile);
          return -3;  // Too many frames in log file
        }

        *loginterlaced = 0; //  progressive,
                           //  i - frame number
        motionx[i] = dx;
        motiony[i] = dy;
        motionrot[i] = rot;
        motionzoom[i] = zoom;
      }
    }
  } // end of while

  fclose(logfile);
  return 0; // OK
}

//
//*************************************************************************
// write motion data and trust (line) for current frame to extended log file
// file must be open
//
void write_extlog(FILE *extlogfile, int IsFieldBased, int IsTFF, int ndest, float motionx[], float motiony[], float motionzoom[], float trust[])
{

  float rotation = 0.0; // no rotation estimation in current version


    // write frame number, dx, dy, rotation and zoom in Deshaker log format
  if (IsFieldBased) { // fields from interlaced clip, A or B ( A is time first in Deshaker log )
    if ((ndest % 2 == 0)) { // even TFF or BFF fields - bug fixed in v.1.4.1
      fprintf(extlogfile, " %5dA %7.2f %7.2f %7.3f %7.5f %7.3f\n", ndest / 2, motionx[ndest], motiony[ndest], rotation, motionzoom[ndest], trust[ndest]);
    }
    else { // odd TFF or BFF fields
      fprintf(extlogfile, " %5dB %7.2f %7.2f %7.3f %7.5f %7.3f\n", ndest / 2, motionx[ndest], motiony[ndest], rotation, motionzoom[ndest], trust[ndest]);
    }
  }
  else { // progressive
    fprintf(extlogfile, " %6d %7.2f %7.2f %7.3f %7.5f %7.3f\n", ndest, motionx[ndest], motiony[ndest], rotation, motionzoom[ndest], trust[ndest]);
  }



}

//*************************************************************************
// write motion data (line) for current frame to log file in Deshaker format
// file must be open
//
void write_deshakerlog(FILE *logfile, int IsFieldBased, int IsTFF, int ndest, float motionx[], float motiony[], float motionzoom[])
{

  float rotation = 0.0; // no rotation estimation in current version


    // write frame number, dx, dy, rotation and zoom in Deshaker log format
  if (IsFieldBased) { // fields from interlaced clip, A or B ( A is time first in Deshaker log )
    if ((ndest % 2 == 0)) { // even TFF or BFF fields - bug fixed in v.1.4.1
      fprintf(logfile, " %5dA %7.2f %7.2f %7.3f %7.5f\n", ndest / 2, motionx[ndest], motiony[ndest], rotation, motionzoom[ndest]);
    }
    else { // odd TFF or BFF fields
      fprintf(logfile, " %5dB %7.2f %7.2f %7.3f %7.5f\n", ndest / 2, motionx[ndest], motiony[ndest], rotation, motionzoom[ndest]);
    }
  }
  else { // progressive
    fprintf(logfile, " %6d %7.2f %7.2f %7.3f %7.5f\n", ndest, motionx[ndest], motiony[ndest], rotation, motionzoom[ndest]);
  }



}

