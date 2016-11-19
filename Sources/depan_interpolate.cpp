/*
  DePan plugin for Avisynth 2.6 interface - global motion estimation and compensation of camera pan
  Version 1.9, November 5, 2006.
  Version 1.10.0, April 29, 2007
  Version 1.10.1
  Version 1.13.1, April 6, 2016
  Version 2.13.1, November 19, 2016 by pinterf
  (tranformation internal functions)
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

//#include "windows.h"
#include "avisynth.h"
#include <stdint.h>
#include <cmath>
//#include "float.h"

#include "depan.h"

/* moved to depan.h
#define MIRROR_TOP 1
#define MIRROR_BOTTOM 2
#define MIRROR_LEFT 4
#define MIRROR_RIGHT 8
*/

template <typename pixel_t>
void compensate_plane_nearest2(BYTE *dstp8, int dst_pitch, const BYTE * srcp8, int src_pitch, int row_size, int height, transform tr, int mirror, int border, int blurmax, int bits_per_pixel)
{
  dst_pitch /= sizeof(pixel_t);
  src_pitch /= sizeof(pixel_t);  // src_pitch = src->GetRowSize(plane) in bytes
  row_size /= sizeof(pixel_t);   // src_width = src->GetRowSize(plane) in bytes

  pixel_t *dstp = reinterpret_cast<pixel_t *>(dstp8);
  const pixel_t *srcp = reinterpret_cast<const pixel_t *>(srcp8);

  // if border >=0, then we fill empty edge (border) pixels by that value
  // work1row_size is work array, it must have size >= 1*row_size

  // if mirror > 0, than we fill empty edge (border) pixels by mirrored (reflected) pixels from border,
  // according to bit set of "mirror" parameter:                   (added in v.0.9)
  // mirror = 1 - only top
  // mirror = 2 - only bottom
  // mirror = 4 - only left
  // mirror = 8 - only right
  // any combination - sum of above

  int h, row;
  int rowleft, hlow;
  float xsrc, ysrc;
  int w0;
  int inttr0;
  int *rowleftwork = new int[row_size];

  int smoothed;
  int blurlen;
  int i;

  // for mirror

  int mtop = mirror & MIRROR_TOP;
  int mbottom = mirror & MIRROR_BOTTOM;
  int mleft = mirror & MIRROR_LEFT;
  int mright = mirror & MIRROR_RIGHT;

  // select if rotation, zoom?

  if (tr.dxy == 0 && tr.dyx == 0 && tr.dxx == 1 && tr.dyy == 1) { // only translation - fast

    for (h = 0; h < height; h++) {

      ysrc = tr.dyc + h;
      hlow = (int)floor(ysrc + 0.5);

      inttr0 = (int)floor(tr.dxc + 0.5);
      rowleft = inttr0;
      //			xsrc = tr[0];
      //			rowleft = (int)floor(xsrc);  // low

      if (hlow < 0 && mtop) hlow = -hlow;  // mirror borders
      if (hlow >= height && mbottom) hlow = height + height - hlow - 2;

      w0 = hlow*src_pitch;
      if ((hlow >= 0) && (hlow < height)) {  // middle lines

        for (row = 0; row < row_size; row++) {
          rowleft = inttr0 + row;

          //					xsrc = tr[0]+row;            // hided simle formulas,
          //					rowleft = (int)floor(xsrc);  // but slow

          //  x,y point is in square: (rowleft,hlow) to (rowleft+1,hlow+1)

          if ((rowleft >= 0) && (rowleft < row_size)) {
            dstp[row] = srcp[w0 + rowleft];
          }
          else if (rowleft < 0 && mleft) {
            if (blurmax > 0) {
              blurlen = min(blurmax, -rowleft);
              smoothed = 0;
              for (i = -rowleft - blurlen + 1; i <= -rowleft; i++)
                smoothed += srcp[w0 + i];
              dstp[row] = smoothed / blurlen;   // v. 1.3
            }
            else { // no blur
              dstp[row] = srcp[w0 - rowleft];    // not very precise - may be bicubic?
            }
          }
          else if (rowleft >= row_size && mright) {
            if (blurmax > 0) {
              blurlen = min(blurmax, rowleft - row_size + 1);
              smoothed = 0;
              for (i = row_size + row_size - rowleft - 2; i < row_size + row_size - rowleft - 2 + blurlen; i++)
                smoothed += srcp[w0 + i];
              dstp[row] = smoothed / blurlen;  // v. 1.3
            }
            else {// no blur
              dstp[row] = srcp[w0 + row_size + row_size - rowleft - 2];   // not very precise - may be bicubic?
            }
          }
          else if (border >= 0) {   // if shifted point is out of frame, fill using border value
            dstp[row] = border;
          }


        } // end for
      }
      else if (border >= 0) { // out lines
        for (row = 0; row < row_size; row++) {
          dstp[row] = border;
        }
      }

      dstp += dst_pitch; // next line
    }
  }
  //-----------------------------------------------------------------------------
  else if (tr.dxy == 0 && tr.dyx == 0) { 
    // no rotation, only zoom and translation  - fast

    // prepare positions   (they are not dependent from h) for fast processing
    for (row = 0; row < row_size; row++) {
      xsrc = tr.dxc + tr.dxx*row;
      rowleftwork[row] = (int)floor(xsrc + 0.5);
      rowleft = rowleftwork[row];
    }


    for (h = 0; h < height; h++) {

      ysrc = tr.dyc + tr.dyy*h;

      hlow = (int)floor(ysrc + 0.5);

      if (hlow < 0 && mtop) hlow = -hlow;  // mirror borders
      if (hlow >= height && mbottom) hlow = height + height - hlow - 2;

      w0 = hlow*src_pitch;
      if ((hlow >= 0) && (hlow < height)) {  // incide


        for (row = 0; row < row_size; row++) {

          //					xsrc = tr[0]+tr[1]*row;
          //					rowleft = floor(xsrc);
          rowleft = rowleftwork[row]; //(int)(xsrc);

          if ((rowleft >= 0) && (rowleft < row_size)) {
            dstp[row] = srcp[w0 + rowleft];
          }
          else if (rowleft < 0 && mleft) {
            if (blurmax > 0) {
              blurlen = min(blurmax, -rowleft);
              smoothed = 0;
              for (i = -rowleft - blurlen + 1; i <= -rowleft; i++)
                smoothed += srcp[w0 + i];
              dstp[row] = smoothed / blurlen;   // v. 1.3
            }
            else { // no blur
              dstp[row] = srcp[w0 - rowleft];    // not very precise - may be bicubic?
            }
          }
          else if (rowleft >= row_size && mright) {
            if (blurmax > 0) {
              blurlen = min(blurmax, rowleft - row_size + 1);
              smoothed = 0;
              for (i = row_size + row_size - rowleft - 2; i < row_size + row_size - rowleft - 2 + blurlen; i++)
                smoothed += srcp[w0 + i];
              dstp[row] = smoothed / blurlen;  // v. 1.3
            }
            else {// no blur
              dstp[row] = srcp[w0 + row_size + row_size - rowleft - 2];   // not very precise - may be bicubic?
            }
          }
          else  if (border >= 0) {   // if shifted point is out of frame, fill using border value
            dstp[row] = border;
          }
        } // end for
      }
      else  if (border >= 0) { // out lines
        for (row = 0; row < row_size; row++) {
          dstp[row] = border;
        }
      }

      dstp += dst_pitch; // next line

    }
  }
  //-----------------------------------------------------------------------------
  else { // rotation, zoom and translation - slow

    for (h = 0; h < height; h++) {

      xsrc = tr.dxc + tr.dxy*h;  // part not dependent from row
      ysrc = tr.dyc + tr.dyy*h;

      for (row = 0; row < row_size; row++) {

        rowleft = (int)(xsrc + 0.5); // use simply fast (int), not floor(), since followed check

                                     //				if (xsrc  < rowleft) {
                                     //					rowleft -=1;
                                     //				}

        hlow = (int)(ysrc + 0.5);  // use simply fast  (int), not floor(), since followed check

                                   //				if (ysrc <  hlow) {
                                   //					hlow -=1;
                                   //				}


        if ((rowleft >= 0) && (rowleft < row_size) && (hlow >= 0) && (hlow < height)) {
          dstp[row] = srcp[hlow*src_pitch + rowleft];
        }
        else { // try fill by mirror. Probability of these cases is small
          if (hlow < 0 && mtop) hlow = -hlow;  // mirror borders
          if (hlow >= height && mbottom) hlow = height + height - hlow - 2;
          if (rowleft < 0 && mleft) rowleft = -rowleft;
          if (rowleft >= row_size && mright) rowleft = row_size + row_size - rowleft - 2;
          // check mirrowed
          if ((rowleft >= 0) && (rowleft < row_size) && (hlow >= 0) && (hlow < height)) {
            dstp[row] = srcp[hlow*src_pitch + rowleft];
          }
          else  if (border >= 0) {   // if shifted point is out of frame, fill using border value
            dstp[row] = border;
          }
        }
        xsrc += tr.dxx; // next
        ysrc += tr.dyx;
      } // end for row

      dstp += dst_pitch; // next line
    } //end for h
  } // end if rotation

  delete[] rowleftwork;

}

//****************************************************************************
// move plane of nextp frame to dstp for motion compensation by transform tr[]
// with BILINEAR interpolation of discrete neighbour source pixels
//   t[0] = dxc, t[1] = dxx, t[2] = dxy, t[3] = dyc, t[4] = dyx, t[5] = dyy
//
template <typename pixel_t>
void compensate_plane_bilinear2(BYTE *dstp8, int dst_pitch, const BYTE * srcp8, int src_pitch, int row_size, int height, transform tr, int mirror, int border, int blurmax, int bits_per_pixel)
{
  // work2row_size is work array, it must have size >= 2*row_size
  dst_pitch /= sizeof(pixel_t);
  src_pitch /= sizeof(pixel_t);  // src_pitch = src->GetRowSize(plane) in bytes
  row_size /= sizeof(pixel_t);   // src_width = src->GetRowSize(plane) in bytes

  pixel_t *dstp = reinterpret_cast<pixel_t *>(dstp8);
  const pixel_t *srcp = reinterpret_cast<const pixel_t *>(srcp8);

  int h, row;
  int pixel;
  int rowleft, hlow;
  float sx, sy;
  //	float t0, t1, t2, t3, c0,c1,c2,c3;
  float xsrc, ysrc;
  int w0;
  int intcoef2d[4];
  int ix2, iy2;
  int i, j;
  int inttr0;

  int *rowleftwork = new int[row_size];
  int *ix2work = new int[row_size];

  int intcoef[66]; // 2 * (32 + 1)
  int intcoef2dzoom0[66 * 66]; // ix2work + row_size;//[66][66]; // 4356
  int *intcoef2dzoom = &intcoef2dzoom0[0]; // pointer

  int w;

  int smoothed;
  int blurlen;

  // for mirror
  int mtop = mirror & MIRROR_TOP;
  int mbottom = mirror & MIRROR_BOTTOM;
  int mleft = mirror & MIRROR_LEFT;
  int mright = mirror & MIRROR_RIGHT;

  int rowgoodstart, rowgoodend, rowbadstart, rowbadend;

  // prepare interpolation coefficients tables
  // for position of xsrc in integer grid
  //		sx = (xsrc-rowleft);
  //		sy = (ysrc-hlow);
  //
  //			cx0 = (1-sx);
  //			cx1 = sx;
  //
  //			cy0 = (1-sy);
  //			cy1 = sy;
  //
  // now sx = i/32, sy = j/32  (discrete approximation)

  // float coeff. are changed by integer coeff. scaled by 32
  for (i = 0; i <= 32; i += 1) {
    intcoef[i * 2] = (32 - i);
    intcoef[i * 2 + 1] = i;

  }

  //	select if rotation, zoom?

  if (tr.dxy == 0 && tr.dyx == 0 && tr.dxx == 1 && tr.dyy == 1) { // only translation - fast

    for (h = 0; h < height; h++) {

      ysrc = tr.dyc + h;
      hlow = (int)floor(ysrc);
      iy2 = 2 * ((int)floor((ysrc - hlow) * 32)); //v.1.8.2

      inttr0 = (int)floor(tr.dxc);
      rowleft = inttr0;
      //			xsrc = tr[0];
      //			rowleft = (int)floor(xsrc);  // low
      ix2 = 2 * ((int)floor((tr.dxc - rowleft) * 32)); //v.1.8.2

      for (j = 0; j < 2; j++) {
        for (i = 0; i < 2; i++) {
          intcoef2d[j * 2 + i] = (intcoef[j + iy2] * intcoef[i + ix2]); // 4 coeff. for bilinear 2D
        }
      }

      if (hlow < 0 && mtop) hlow = -hlow;  // mirror borders
      if (hlow >= height && mbottom) hlow = height + height - hlow - 2;

      w0 = hlow*src_pitch;

      if ((hlow >= 0) && (hlow < height - 1)) {  // middle lines

        if (inttr0 >= 0) {
          rowgoodstart = 0;
          rowgoodend = row_size - 1 - inttr0;
          rowbadstart = rowgoodend;
          rowbadend = row_size;
        }
        else {
          rowbadstart = 0;
          rowbadend = -inttr0;
          rowgoodstart = rowbadend;
          rowgoodend = row_size;
        }
        //				int rowgoodendpaired = (rowgoodend/2)*2; //even - but it was a little not optimal
        int rowgoodendpaired = rowgoodstart + ((rowgoodend - rowgoodstart) / 2) * 2;//even length - small fix in v.1.8
        w = w0 + inttr0 + rowgoodstart;
        for (row = rowgoodstart; row < rowgoodendpaired; row += 2) { // paired unroll for speed
                                                                     //					xsrc = tr[0]+row;            // hided simle formulas,
                                                                     //					rowleft = (int)floor(xsrc);  // but slow
                                                                     //					rowleft = inttr0 + row;
                                                                     //					w = w0+rowleft;
                                                                     //  x,y point is in square: (rowleft,hlow) to (rowleft+1,hlow+1)
                                                                     //					if ( (rowleft >= 0) && (rowleft<row_size-1)  )
          dstp[row] = (intcoef2d[0] * srcp[w] + intcoef2d[1] * srcp[w + 1] \
            + intcoef2d[2] * srcp[w + src_pitch] + intcoef2d[3] * srcp[w + src_pitch + 1]) >> 10; // i.e. divide by 32*32
          dstp[row + 1] = (intcoef2d[0] * srcp[w + 1] + intcoef2d[1] * srcp[w + 2] \
            + intcoef2d[2] * srcp[w + src_pitch + 1] + intcoef2d[3] * srcp[w + src_pitch + 2]) >> 10; // i.e. divide by 32*32
          w += 2;
        }
        for (row = rowgoodendpaired - 1; row < rowgoodend; row++) { // if odd, process  very last
          w = w0 + inttr0 + row;
          dstp[row] = (intcoef2d[0] * srcp[w] + intcoef2d[1] * srcp[w + 1] + \
            intcoef2d[2] * srcp[w + src_pitch] + intcoef2d[3] * srcp[w + src_pitch + 1]) >> 10; // i.e. divide by 32*32
        }
        for (row = rowbadstart; row < rowbadend; row++) {
          rowleft = inttr0 + row;
          w = w0 + rowleft;

          if (rowleft < 0 && mleft) {
            if (blurmax > 0) {
              blurlen = min(blurmax, -rowleft);
              smoothed = 0;
              for (i = -rowleft - blurlen + 1; i <= -rowleft; i++)
                smoothed += srcp[w0 + i];
              dstp[row] = smoothed / blurlen;   // v. 1.3
            }
            else { // no blur
              dstp[row] = srcp[w0 - rowleft];    // not very precise - may be bicubic?
            }
          }
          else if (rowleft >= row_size - 1 && mright) {
            if (blurmax > 0) {
              blurlen = min(blurmax, rowleft - row_size + 2); // v.1.10.1
              smoothed = 0;
              for (i = row_size + row_size - rowleft - 2; i < row_size + row_size - rowleft - 2 + blurlen; i++)
                smoothed += srcp[w0 + i];
              dstp[row] = smoothed / blurlen;  // v. 1.3
            }
            else {// no blur
              dstp[row] = srcp[w0 + row_size + row_size - rowleft - 2];   // not very precise - may be bicubic?
            }
          }
          else  if (border >= 0) {   // if shifted point is out of frame, fill using border value
            dstp[row] = border;
          }

        } // end for
      }
      else if (hlow == height - 1) { // edge (top, bottom) lines
        for (row = 0; row < row_size; row++) {
          rowleft = inttr0 + row;
          if ((rowleft >= 0) && (rowleft < row_size)) {
            dstp[row] = srcp[w0 + rowleft]; 	// nearest pixel, may be bilinear is better
          }
          else if (rowleft < 0 && mleft) {
            dstp[row] = srcp[w0 - rowleft];
          }
          else if (rowleft >= row_size - 1 && mright) {
            dstp[row] = srcp[w0 + row_size + row_size - rowleft - 2];
          }
          else  if (border >= 0) { // left or right
            dstp[row] = border;
          }
        }
      }
      else  if (border >= 0) { // out lines
        for (row = 0; row < row_size; row++) {
          dstp[row] = border;
        }
      }

      dstp += dst_pitch; // next line
    }
  }
  //-----------------------------------------------------------------------------
  else if (tr.dxy == 0 && tr.dyx == 0) { // no rotation, only zoom and translation  - fast

                                         // prepare positions   (they are not dependent from h) for fast processing
    for (row = 0; row < row_size; row++) {
      xsrc = tr.dxc + tr.dxx*row;
      rowleftwork[row] = (int)floor(xsrc);
      rowleft = rowleftwork[row];
      ix2work[row] = 2 * ((int)floor((xsrc - rowleft) * 32)); //v1.8.2
    }

    for (j = 0; j < 66; j++) {
      for (i = 0; i < 66; i++) {
        intcoef2dzoom[i] = (intcoef[j] * intcoef[i]); //  coeff. for bilinear 2D
      }
      intcoef2dzoom += 66;
    }
    intcoef2dzoom -= 66 * 66; //restore

    for (h = 0; h < height; h++) {

      ysrc = tr.dyc + tr.dyy*h;

      hlow = (int)floor(ysrc);
      iy2 = 2 * ((int)floor((ysrc - hlow) * 32));//v1.8.2

      if (hlow < 0 && mtop) hlow = -hlow;  // mirror borders
      if (hlow >= height && mbottom) hlow = height + height - hlow - 2;

      w0 = hlow*src_pitch;

      if ((hlow >= 0) && (hlow < height - 1)) {  // incide

        intcoef2dzoom = intcoef2dzoom0;
        intcoef2dzoom += iy2 * 66;


        for (row = 0; row < row_size; row++) {

          //					xsrc = tr[0]+tr[1]*row;
          rowleft = rowleftwork[row]; //(int)(xsrc);
          //					rowleft = floor(xsrc);

          //  x,y point is in square: (rowleft,hlow) to (rowleft+1,hlow+1)

          if ((rowleft >= 0) && (rowleft < row_size - 1)) {

            //						ix2 = 2*((int)((xsrc-rowleft)*32));
            ix2 = ix2work[row];
            w = w0 + rowleft;

            // pixel = ( intcoef[iy2]*(intcoef[ix2]*srcp[w] + intcoef[ix2+1]*srcp[w+1] ) + \
            //           intcoef[iy2+1]*(intcoef[ix2]*srcp[w+src_pitch] + intcoef[ix2+1]*srcp[w+src_pitch+1] ) )/1024;
            pixel = (intcoef2dzoom[ix2] * srcp[w] + intcoef2dzoom[ix2 + 1] * srcp[w + 1] + \
              intcoef2dzoom[ix2 + 66] * srcp[w + src_pitch] + intcoef2dzoom[ix2 + 67] * srcp[w + src_pitch + 1]) >> 10; // v1.6

            //						dstp[row] = max(min(pixel,255),0);
            dstp[row] = pixel;   // maxmin disabled in v1.6
          }
          else if (rowleft < 0 && mleft) {
            if (blurmax > 0) { // 1.13 fix
              blurlen = min(blurmax, -rowleft);
              smoothed = 0;
              for (i = -rowleft - blurlen + 1; i <= -rowleft; i++)
                smoothed += srcp[w0 + i];
              dstp[row] = smoothed / blurlen;   // v. 1.3
            }
            else { // no blur
              dstp[row] = srcp[w0 - rowleft];
            }
          }
          else if (rowleft >= row_size - 1 && mright) {
            if (blurmax > 0) {
              blurlen = min(blurmax, rowleft - row_size + 2); // v1.10.1
              smoothed = 0;
              for (i = row_size + row_size - rowleft - 2; i < row_size + row_size - rowleft - 2 + blurlen; i++)
                smoothed += srcp[w0 + i];
              dstp[row] = smoothed / blurlen;  // v. 1.3
            }
            else {// no blur
              dstp[row] = srcp[w0 + row_size + row_size - rowleft - 2];   // not very precise - may be bicubic?
            }
          }
          else if (border >= 0) {   // if shifted point is out of frame, fill using border value
            dstp[row] = border;
          }
        } // end for
      }
      else if (hlow == height - 1) { // edge ( bottom) lines
        for (row = 0; row < row_size; row++) {
          rowleft = rowleftwork[row];
          if ((rowleft >= 0) && (rowleft < row_size)) {
            dstp[row] = srcp[rowleft + hlow*src_pitch]; 	// nearest pixel, may be bilinear is better
          }
          else  if (border >= 0) { // left or right
            dstp[row] = border;
          }
        }
      }
      else  if (border >= 0) { // out lines
        for (row = 0; row < row_size; row++) {
          dstp[row] = border;
        }
      }

      dstp += dst_pitch; // next line

    }
  }
  //-----------------------------------------------------------------------------
  else { // rotation, zoom and translation - slow

    for (h = 0; h < height; h++) {

      xsrc = tr.dxc + tr.dxy*h;  // part not dependent from row
      ysrc = tr.dyc + tr.dyy*h;

      for (row = 0; row < row_size; row++) {

        rowleft = (int)(xsrc); // use simply fast (int), not floor(), since followed check >1
        sx = xsrc - rowleft;
        if (sx < 0) {
          sx += 1;
          rowleft -= 1;
        }

        hlow = (int)(ysrc);  // use simply fast  (int), not floor(), since followed check >1
        sy = ysrc - hlow;
        if (sy < 0) {
          sy += 1;
          hlow -= 1;
        }

        //  x,y point is in square: (rowleft,hlow) to (rowleft+1,hlow+1)


        if ((rowleft >= 0) && (rowleft < row_size - 1) && (hlow >= 0) && (hlow < height - 1)) {

          ix2 = ((int)(sx * 32)) << 1; // i.e. *2
          iy2 = ((int)(sy * 32)) << 1;   // i.e. *2
          w0 = rowleft + hlow*src_pitch;

          pixel = ((intcoef[ix2] * srcp[w0] + intcoef[ix2 + 1] * srcp[w0 + 1])*intcoef[iy2] + \
            (intcoef[ix2] * srcp[w0 + src_pitch] + intcoef[ix2 + 1] * srcp[w0 + src_pitch + 1])*intcoef[iy2 + 1]) >> 10;

          //					dstp[row] = max(min(pixel,255),0);
          dstp[row] = pixel;       //maxmin disabled in v1.6
        }
        else {
          if (hlow < 0 && mtop) hlow = -hlow;  // mirror borders
          if (hlow >= height && mbottom) hlow = height + height - hlow - 2;
          if (rowleft < 0 && mleft) rowleft = -rowleft;
          if (rowleft >= row_size && mright) rowleft = row_size + row_size - rowleft - 2;
          // check mirrowed
          if ((rowleft >= 0) && (rowleft < row_size) && (hlow >= 0) && (hlow < height)) {
            dstp[row] = srcp[hlow*src_pitch + rowleft];
          }
          else  if (border >= 0) {   // if shifted point is out of frame, fill using border value
            dstp[row] = border;
          }
        }
        xsrc += tr.dxx;  // next
        ysrc += tr.dyx;
      } // end for row

      dstp += dst_pitch; // next line
    } //end for h
  } // end if rotation

  delete[] rowleftwork;
  delete[] ix2work;
}

//****************************************************************************
// move plane of nextp frame to dstp for motion compensation by transform tr[]
// with BICUBIC interpolation of discrete neighbour source pixels
//
//   t[0] = dxc, t[1] = dxx, t[2] = dxy, t[3] = dyc, t[4] = dyx, t[5] = dyy
//
template <typename pixel_t>
void compensate_plane_bicubic2(BYTE *dstp8, int dst_pitch, const BYTE * srcp8, int src_pitch, int row_size, int height, transform tr, int mirror, int border, int blurmax, int bits_per_pixel)
{
  // work2width1030 is integer work array, it must have size >= 2*row_size+1030
  dst_pitch /= sizeof(pixel_t);
  src_pitch /= sizeof(pixel_t);  // src_pitch = src->GetRowSize(plane) in bytes
  row_size /= sizeof(pixel_t);   // src_width = src->GetRowSize(plane) in bytes

  pixel_t *dstp = reinterpret_cast<pixel_t *>(dstp8);
  const pixel_t *srcp = reinterpret_cast<const pixel_t *>(srcp8);

  int h, row;
  int pixel;
  int rowleft, hlow;
  float sx, sy;
  //	float t0, t1, t2, t3, c0,c1,c2,c3;
  float xsrc, ysrc;
  int w0;
  int ix4, iy4;
  int i, j;
  int inttr0, inttr3;
  int *rowleftwork = new int[row_size];
  //int * rowleftwork = work2width1030;
  int *ix4work = new int[row_size]; 
  //int * ix4work = work2width1030 + row_size;
  //	int intcoef[1030];
  //int *intcoef = ix4work + row_size;
  int *intcoef = new int[4*(256 + 1)]; //ix4work + row_size;

  //     int for pixel_size uint8_t
  // __int64 for pixel_size uint16_t, otherwise int overflow in intermediate calculations
  std::conditional < sizeof(pixel_t) == 1, int, __int64 >::type ts[4];
  //__int64 ts[4];         // P.F. 16.06.09 Bicubic 2D force __int64 calc if used
  int intcoef2d[16]; // P.F. 16.06.09 Bicubic 2D + 16 bit pixel -> oveflow: int->_int64  

  int w;
  int smoothed;
  int blurlen;

  // for mirror
  int mtop = mirror & MIRROR_TOP;
  int mbottom = mirror & MIRROR_BOTTOM;
  int mleft = mirror & MIRROR_LEFT;
  int mright = mirror & MIRROR_RIGHT;

  int pixel_max = (1 << bits_per_pixel) - 1;
#ifdef OLD_NOT_USED
  _controlfp(_MCW_RC, _RC_CHOP); // set rounding mode to truncate to zero mode (C++ standard) for /QIfist compiler option (which is for faster float-int conversion)
#endif
                                 // prepare interpolation coefficients tables
                                 // for position of xsrc in integer grid
                                 //		sx = (xsrc-rowleft);
                                 //		sy = (ysrc-hlow);
                                 //
                                 //			cx0 = -sx*(1-sx)*(1-sx);
                                 //			cx1 = (1-2*sx*sx+sx*sx*sx);
                                 //			cx2 =  sx*(1+sx-sx*sx);
                                 //			cx3 =  -sx*sx*(1-sx);
                                 //
                                 //			cy0 = -sy*(1-sy)*(1-sy);
                                 //			cy1 = (1-2*sy*sy+sy*sy*sy);
                                 //			cy2 =  sy*(1+sy-sy*sy);
                                 //			cy3 =  -sy*sy*(1-sy);
                                 //
                                 // now sx = i/256, sy = j/256  (discrete approximation)

                                 // float coeff. are changed by integer coeff. scaled by 256*256*256/8192 = 2048
  for (i = 0; i <= 256; i += 1) { // 257 steps, 1028 numbers
    intcoef[i * 4] = -((i*(256 - i)*(256 - i))) / 8192;
    intcoef[i * 4 + 1] = (256 * 256 * 256 - 2 * 256 * i*i + i*i*i) / 8192;
    intcoef[i * 4 + 2] = (i*(256 * 256 + 256 * i - i*i)) / 8192;
    intcoef[i * 4 + 3] = -(i*i*(256 - i)) / 8192;
    // max: 24 bit(8+8+8) / 13 bit(8192) = 11 bits 
  }

  //	select if rotation, zoom

  if (tr.dxy == 0 && tr.dyx == 0 && tr.dxx == 1 && tr.dyy == 1) { // only translation - fast

    for (h = 0; h < height; h++) {

      ysrc = tr.dyc + h;
      inttr3 = (int)floor(tr.dyc);
      hlow = (int)floor(ysrc);
      iy4 = 4 * ((int)((ysrc - hlow) * 256));

      inttr0 = (int)floor(tr.dxc);
      rowleft = inttr0;
      //			xsrc = tr[0];
      //			rowleft = (int)floor(xsrc);  // low
      ix4 = 4 * ((int)((tr.dxc - inttr0) * 256));

      for (j = 0; j < 4; j++) {
        for (i = 0; i < 4; i++) {
          intcoef2d[j * 4 + i] = ((intcoef[j + iy4] * intcoef[i + ix4])) / 2048; // 16 coeff. for bicubic 2D, scaled by 2048
                                                                                 // 11 bits
        }
      }

      if (hlow < 0 && mtop) hlow = -hlow;  // mirror borders
      if (hlow >= height && mbottom) hlow = height + height - hlow - 2;

      w0 = hlow*src_pitch;

      if ((hlow >= 1) && (hlow < height - 2)) {  // middle lines

        for (row = 0; row < row_size; row++) {

          rowleft = inttr0 + row;

          //					xsrc = tr[0]+row;

          //  x,y point is in square: (rowleft,hlow) to (rowleft+1,hlow+1)

          if ((rowleft >= 1) && (rowleft < row_size - 2)) {
            w = w0 + rowleft;

            // max (if pixel==16 bit) 
            // Intermediate result 1: one tag: (11+16=27 bits bit)
            // Intermediate result 2: sum of 16 tags (*4) 27+4 = 31 bits
            // finally >> 11 (/2048) --> 31-11 = 20 bits
            pixel = 
              (intcoef2d[0] * srcp[w - src_pitch - 1] + intcoef2d[1] * srcp[w - src_pitch] + intcoef2d[2] * srcp[w - src_pitch + 1] + intcoef2d[3] * srcp[w - src_pitch + 2] + \
                intcoef2d[4] * srcp[w - 1] + intcoef2d[5] * srcp[w] + intcoef2d[6] * srcp[w + 1] + intcoef2d[7] * srcp[w + 2] + \
                intcoef2d[8] * srcp[w + src_pitch - 1] + intcoef2d[9] * srcp[w + src_pitch] + intcoef2d[10] * srcp[w + src_pitch + 1] + intcoef2d[11] * srcp[w + src_pitch + 2] + \
                intcoef2d[12] * srcp[w + src_pitch * 2 - 1] + intcoef2d[13] * srcp[w + src_pitch * 2] + intcoef2d[14] * srcp[w + src_pitch * 2 + 1] + intcoef2d[15] * srcp[w + src_pitch * 2 + 2] + \
                // +1024: fix in 1.13.1
                1024) >> 11;  // i.e. /2048

            dstp[row] = max(min(pixel, pixel_max), 0); // PF

          }
          else if (rowleft < 0 && mleft) {
            if (blurmax > 0) {
              blurlen = min(blurmax, -rowleft);
              smoothed = 0;
              for (i = -rowleft - blurlen + 1; i <= -rowleft; i++)
                smoothed += srcp[w0 + i];
              dstp[row] = smoothed / blurlen;   // v. 1.3
            }
            else { // no blur
              dstp[row] = srcp[w0 - rowleft];    // not very precise - may be bicubic?
            }
          }
          else if (rowleft >= row_size && mright) {
            if (blurmax > 0) {
              blurlen = min(blurmax, rowleft - row_size + 1);
              smoothed = 0;
              for (i = row_size + row_size - rowleft - 2; i < row_size + row_size - rowleft - 2 + blurlen; i++)
                smoothed += srcp[w0 + i];
              dstp[row] = smoothed / blurlen;  // v. 1.3
            }
            else {// no blur
              dstp[row] = srcp[w0 + row_size + row_size - rowleft - 2];   // not very precise - may be bicubic?
            }
          }
          else if (rowleft == 0 || rowleft == row_size - 1 || rowleft == row_size - 2) { // edges
            dstp[row] = srcp[w0 + rowleft];
          }
          else  if (border >= 0) {   // if shifted point is out of frame, fill using border value
            dstp[row] = border;
          }

        } // end for
      }
      else if (hlow == 0 || hlow == height - 2) { // near edge (top-1, bottom-1) lines
        for (row = 0; row < row_size; row++) {
          rowleft = inttr0 + row;
          sx = tr.dxc - inttr0;
          sy = tr.dyc - inttr3;
          if ((rowleft >= 0) && (rowleft < row_size - 1)) { // bug fixed for right edge in v.1.1.1
            w = w0 + rowleft;
            dstp[row] = (int)((1.0 - sy)*((1.0 - sx)*srcp[w] + sx*srcp[w + 1]) + \
              sy*((1.0 - sx)*srcp[w + src_pitch] + sx*srcp[w + src_pitch + 1])); // bilinear
          }
          else if (rowleft == row_size - 1) { // added in v.1.1.1
            dstp[row] = srcp[rowleft + w0];
          }
          else if (rowleft < 0 && mleft) {
            dstp[row] = srcp[w0 - rowleft];       // not very precise - may be bicubic?
          }
          else if (rowleft >= row_size && mright) {
            dstp[row] = srcp[w0 + row_size + row_size - rowleft - 2];
          }
          else  if (border >= 0) { // left or right
            dstp[row] = border;
          }
        }
      }
      else if (hlow == height - 1) { // bottom line
        for (row = 0; row < row_size; row++) {
          rowleft = inttr0 + row;
          if (rowleft >= 0 && rowleft < row_size) {
            dstp[row] = srcp[w0 + rowleft];
          }
          else if (rowleft < 0 && mleft) {
            dstp[row] = srcp[w0 - rowleft];       // not very precise - may be bicubic?
          }
          else if (rowleft >= row_size && mright) {
            dstp[row] = srcp[w0 + row_size + row_size - rowleft - 2];
          }
          else  if (border >= 0) { // left or right
            dstp[row] = border;
          }
        }
      }
      else  if (border >= 0) { // out lines
        for (row = 0; row < row_size; row++) {
          dstp[row] = border;
        }
      }

      dstp += dst_pitch; // next line
    }
  }
  //-----------------------------------------------------------------------------
  else if (tr.dxy == 0 && tr.dyx == 0) { 
    // no rotation, only zoom and translation  - fast
    // prepare positions   (they are not dependent from h) for fast processing
    for (row = 0; row < row_size; row++) {
      xsrc = tr.dxc + tr.dxx*row;
      rowleftwork[row] = (int)floor(xsrc);
      rowleft = rowleftwork[row];
      ix4work[row] = 4 * ((int)((xsrc - rowleft) * 256));
    }


    for (h = 0; h < height; h++) {

      ysrc = tr.dyc + tr.dyy*h;

      hlow = (int)floor(ysrc);
      iy4 = 4 * ((int)((ysrc - hlow) * 256));

      sy = ysrc - hlow;

      if (hlow < 0 && mtop) hlow = -hlow;  // mirror borders
      if (hlow >= height && mbottom) hlow = height + height - hlow - 2;

      w0 = hlow*src_pitch;
      if ((hlow >= 1) && (hlow < height - 2)) {  // incide

        for (row = 0; row < row_size; row++) {

          //					xsrc = tr[0]+tr[1]*row;
          //					rowleft = floor(xsrc);
          rowleft = rowleftwork[row]; //(int)(xsrc);

          //  x,y point is in square: (rowleft,hlow) to (rowleft+1,hlow+1)

          if ((rowleft >= 1) && (rowleft < row_size - 2)) {

            //						ix4 = 4*((int)((xsrc-rowleft)*256));
            ix4 = ix4work[row];
            w = w0 + rowleft;


            srcp -= src_pitch;         // prev line
            ts[0] = (intcoef[ix4] * srcp[w - 1] + intcoef[ix4 + 1] * srcp[w] + intcoef[ix4 + 2] * srcp[w + 1] + intcoef[ix4 + 3] * srcp[w + 2]);
            srcp += src_pitch;         // next line
            ts[1] = (intcoef[ix4] * srcp[w - 1] + intcoef[ix4 + 1] * srcp[w] + intcoef[ix4 + 2] * srcp[w + 1] + intcoef[ix4 + 3] * srcp[w + 2]);
            srcp += src_pitch;         // next line
            ts[2] = (intcoef[ix4] * srcp[w - 1] + intcoef[ix4 + 1] * srcp[w] + intcoef[ix4 + 2] * srcp[w + 1] + intcoef[ix4 + 3] * srcp[w + 2]);
            srcp += src_pitch;         // next line
            ts[3] = (intcoef[ix4] * srcp[w - 1] + intcoef[ix4 + 1] * srcp[w] + intcoef[ix4 + 2] * srcp[w + 1] + intcoef[ix4 + 3] * srcp[w + 2]);

            srcp -= (src_pitch << 1);  // restore pointer, changed to shift in v 1.1.1

            pixel = (int)((intcoef[iy4] * ts[0] + intcoef[iy4 + 1] * ts[1] + intcoef[iy4 + 2] * ts[2] + intcoef[iy4 + 3] * ts[3]) >> 22); // 22=2*11 bit (2*2048) scale factor

            dstp[row] = max(min(pixel, pixel_max), 0); // PF
          }
          else if (rowleft < 0 && mleft) {
            if (blurmax > 0) {
              blurlen = min(blurmax, -rowleft);
              smoothed = 0;
              for (i = -rowleft - blurlen + 1; i <= -rowleft; i++)
                smoothed += srcp[w0 + i];
              dstp[row] = smoothed / blurlen;   // v. 1.3
            }
            else { // no blur
              dstp[row] = srcp[w0 - rowleft];    // not very precise - may be bicubic?
            }
          }
          else if (rowleft >= row_size && mright) {
            if (blurmax > 0) {
              blurlen = min(blurmax, rowleft - row_size + 1);
              smoothed = 0;
              for (i = row_size + row_size - rowleft - 2; i < row_size + row_size - rowleft - 2 + blurlen; i++)
                smoothed += srcp[w0 + i];
              dstp[row] = smoothed / blurlen;  // v. 1.3
            }
            else {// no blur
              dstp[row] = srcp[w0 + row_size + row_size - rowleft - 2];   // not very precise - may be bicubic?
            }
          }
          else if (rowleft == 0 || rowleft == row_size - 1 || rowleft == row_size - 2) { // edges
            dstp[row] = srcp[w0 + rowleft];
          }
          else  if (border >= 0) {   // if shifted point is out of frame, fill using border value
            dstp[row] = border;
          }
        } // end for
      }
      else if (hlow == 0 || hlow == height - 2) { // near edge (top-1, bottom-1) lines
        for (row = 0; row < row_size; row++) {
          rowleft = rowleftwork[row];
          if ((rowleft >= 0) && (rowleft < row_size - 1)) { // bug fixed for right bound in v.1.10.0
            xsrc = tr.dxc + tr.dxx*row;
            sx = xsrc - rowleft;
            w = w0 + rowleft;
            pixel = (int)((1.0 - sy)*((1.0 - sx)*srcp[w] + sx*srcp[w + 1]) + \
              sy*((1.0 - sx)*srcp[w + src_pitch] + sx*srcp[w + src_pitch + 1])); // bilinear
            dstp[row] = max(min(pixel, pixel_max), 0);
          }
          else if (rowleft == row_size - 1) { // added in v.1.1.1
            dstp[row] = srcp[rowleft + w0];
          }
          else if (rowleft < 0 && mleft) {
            dstp[row] = srcp[w0 - rowleft];       // not very precise - may be bicubic?
          }
          else if (rowleft >= row_size && mright) {
            dstp[row] = srcp[w0 + row_size + row_size - rowleft - 2];
          }
          else  if (border >= 0) { // left or right
            dstp[row] = border;
          }
        }
      }
      else if (hlow == height - 1) { // bottom line
        for (row = 0; row < row_size; row++) {
          rowleft = rowleftwork[row];
          if (rowleft >= 0 && rowleft < row_size) {
            dstp[row] = (srcp[w0 + rowleft] + srcp[w0 + rowleft - src_pitch]) / 2; // for some smoothing
          }
          else if (rowleft < 0 && mleft) {
            dstp[row] = srcp[w0 - rowleft];
          }
          else if (rowleft >= row_size && mright) {
            dstp[row] = srcp[w0 + row_size + row_size - rowleft - 2];
          }
          else  if (border >= 0) { // left or right
            dstp[row] = border;
          }
        }
      }
      else  if (border >= 0) { // out lines
        for (row = 0; row < row_size; row++) {
          // bug fixed here in v. 0.9.1 (access violation - bad w0)
          /*					rowleft = rowleftwork[row];
          if (rowleft >=0 && rowleft < row_size) {
          dstp[row] = srcp[w0+rowleft];
          }
          else if ( rowleft < 0 && mleft) {
          dstp[row] = srcp[w0-rowleft];       // not very precise - may be bicubic?
          }
          else if ( rowleft >= row_size && mright) {
          dstp[row] = srcp[w0+row_size + row_size - rowleft -2];
          }
          else  if (border >=0 ){ // left or right
          */
          dstp[row] = border;
          //					}
        }
      }

      dstp += dst_pitch; // next line

    }
  }
  //-----------------------------------------------------------------------------
  else { // rotation, zoom and translation - slow

    for (h = 0; h < height; h++) {

      for (row = 0; row < row_size; row++) {

        xsrc = tr.dxc + tr.dxx*row + tr.dxy*h;
        ysrc = tr.dyc + tr.dyx*row + tr.dyy*h;
        rowleft = (int)(xsrc); // use simply fast (int), not floor(), since followed check >1
        if (xsrc < rowleft) {
          rowleft -= 1;
        }

        hlow = (int)(ysrc);  // use simply fast  (int), not floor(), since followed check >1
        if (ysrc < hlow) {
          hlow -= 1;
        }

        //  x,y point is in square: (rowleft,hlow) to (rowleft+1,hlow+1)

        if ((rowleft >= 1) && (rowleft < row_size - 2) && (hlow >= 1) && (hlow < height - 2)) {

          ix4 = 4 * ((int)((xsrc - rowleft) * 256));

          w0 = rowleft + hlow*src_pitch;

          srcp -= src_pitch;         // prev line
          ts[0] = (intcoef[ix4] * srcp[w0 - 1] + intcoef[ix4 + 1] * srcp[w0] + intcoef[ix4 + 2] * srcp[w0 + 1] + intcoef[ix4 + 3] * srcp[w0 + 2]);
          srcp += src_pitch;         // next line
          ts[1] = (intcoef[ix4] * srcp[w0 - 1] + intcoef[ix4 + 1] * srcp[w0] + intcoef[ix4 + 2] * srcp[w0 + 1] + intcoef[ix4 + 3] * srcp[w0 + 2]);
          srcp += src_pitch;         // next line
          ts[2] = (intcoef[ix4] * srcp[w0 - 1] + intcoef[ix4 + 1] * srcp[w0] + intcoef[ix4 + 2] * srcp[w0 + 1] + intcoef[ix4 + 3] * srcp[w0 + 2]);
          srcp += src_pitch;         // next line
          ts[3] = (intcoef[ix4] * srcp[w0 - 1] + intcoef[ix4 + 1] * srcp[w0] + intcoef[ix4 + 2] * srcp[w0 + 1] + intcoef[ix4 + 3] * srcp[w0 + 2]);

          srcp -= (src_pitch << 1);  // restore pointer, changed to shift in v.1.1.1


          iy4 = ((int)((ysrc - hlow) * 256)) << 2; //changed to shift in v.1.1.1

          // 16 bit samples: 32 bit overflow. ts[] is __int64 for word sized pixels
          pixel = (int)((intcoef[iy4] * ts[0] + intcoef[iy4 + 1] * ts[1] + intcoef[iy4 + 2] * ts[2] + intcoef[iy4 + 3] * ts[3]) >> 22); // 22=2*11 scale factor 
          dstp[row] = max(min(pixel, pixel_max), 0); // PF
        }
        else {
          if (hlow < 0 && mtop) hlow = -hlow;  // mirror borders
          if (hlow >= height && mbottom) hlow = height + height - hlow - 2;
          if (rowleft < 0 && mleft) rowleft = -rowleft;
          if (rowleft >= row_size && mright) rowleft = row_size + row_size - rowleft - 2;
          // check mirrowed
          if ((rowleft >= 0) && (rowleft < row_size) && (hlow >= 0) && (hlow < height)) {
            dstp[row] = srcp[hlow*src_pitch + rowleft];
          }
          else  if (border >= 0) {   // if shifted point is out of frame, fill using border value
            dstp[row] = border;
          }
        }
      } // end for row

      dstp += dst_pitch; // next line
    } //end for h
  } // end if rotation

  delete[] rowleftwork;
  delete[] ix4work;
  delete[] intcoef;

}

// instantiate
template void compensate_plane_nearest2<uint8_t>(BYTE *dstp8, int dst_pitch, const BYTE * srcp8, int src_pitch, int src_width, int src_height, transform tr, int mirror, int border, int blurmax, int bits_per_pixel);
template void compensate_plane_nearest2<uint16_t>(BYTE *dstp8, int dst_pitch, const BYTE * srcp8, int src_pitch, int src_width, int src_height, transform tr, int mirror, int border, int blurmax, int bits_per_pixel);

template void compensate_plane_bilinear2<uint8_t>(BYTE *dstp8, int dst_pitch, const BYTE * srcp8, int src_pitch, int row_size, int height, transform tr, int mirror, int border, int blurmax, int bits_per_pixel);
template void compensate_plane_bilinear2<uint16_t>(BYTE *dstp8, int dst_pitch, const BYTE * srcp8, int src_pitch, int row_size, int height, transform tr, int mirror, int border, int blurmax, int bits_per_pixel);

template void compensate_plane_bicubic2<uint8_t>(BYTE *dstp8, int dst_pitch, const BYTE * srcp8, int src_pitch, int row_size, int height, transform tr, int mirror, int border, int blurmax, int bits_per_pixel);
template void compensate_plane_bicubic2<uint16_t>(BYTE *dstp8, int dst_pitch, const BYTE * srcp8, int src_pitch, int row_size, int height, transform tr, int mirror, int border, int blurmax, int bits_per_pixel);


//****************************************************************************
// move plane of nextp frame to dstp for motion compensation by trc, trm with NEAREST pixels
//
#if 0
// moved to template 8/16 bit
void compensate_plane_nearest(BYTE *dstp, int dst_pitch, const BYTE * srcp, int src_pitch, int row_size, int height, transform tr, int mirror, int border, int *work1row_size, int blurmax)
{
  // if border >=0, then we fill empty edge (border) pixels by that value
  // work1row_size is work array, it must have size >= 1*row_size

  // if mirror > 0, than we fill empty edge (border) pixels by mirrored (reflected) pixels from border,
  // according to bit set of "mirror" parameter:                   (added in v.0.9)
  // mirror = 1 - only top
  // mirror = 2 - only bottom
  // mirror = 4 - only left
  // mirror = 8 - only right
  // any combination - sum of above

  int h, row;
  int rowleft, hlow;
  float xsrc, ysrc;
  int w0;
  int inttr0;
  int * rowleftwork = work1row_size;

  int smoothed;
  int blurlen;
  int i;

  // for mirror

  int mtop = mirror & MIRROR_TOP;
  int mbottom = mirror & MIRROR_BOTTOM;
  int mleft = mirror & MIRROR_LEFT;
  int mright = mirror & MIRROR_RIGHT;

#ifdef OLD_NOT_USED
  // Since it or the final restoring affects SSE states also. VapourSynth x64 detected
  _controlfp(_MCW_RC, _RC_CHOP); // set rounding mode to truncate to zero mode (C++ standard) for /QIfist compiler option (which is for faster float-int conversion)
  // QIfist deprecated! https://msdn.microsoft.com/en-us/library/z8dh4h17%28v=vs.80%29.aspx
  // No compiler option is needed.The compiler has made significant improvements in float to int conversion speed.
  // Regarding rounding, / QIfist results in generation of the fistp instruction, which uses whatever ambient rounding mode is to do the conversion.
#endif

//	select if rotation, zoom?

  if (tr.dxy == 0 && tr.dyx == 0 && tr.dxx == 1 && tr.dyy == 1) { // only translation - fast

    for (h = 0; h < height; h++) {

      ysrc = tr.dyc + h;
      hlow = (int)floor(ysrc + 0.5);

      inttr0 = (int)floor(tr.dxc + 0.5);
      rowleft = inttr0;
      //			xsrc = tr[0];
      //			rowleft = (int)floor(xsrc);  // low

      if (hlow < 0 && mtop) hlow = -hlow;  // mirror borders
      if (hlow >= height && mbottom) hlow = height + height - hlow - 2;

      w0 = hlow*src_pitch;
      if ((hlow >= 0) && (hlow < height)) {  // middle lines


        for (row = 0; row < row_size; row++) {
          rowleft = inttr0 + row;

          //					xsrc = tr[0]+row;            // hided simle formulas,
          //					rowleft = (int)floor(xsrc);  // but slow

                    //  x,y point is in square: (rowleft,hlow) to (rowleft+1,hlow+1)

          if ((rowleft >= 0) && (rowleft < row_size)) {
            dstp[row] = srcp[w0 + rowleft];
          }
          else if (rowleft < 0 && mleft) {
            if (blurmax > 0) {
              blurlen = min(blurmax, -rowleft);
              smoothed = 0;
              for (i = -rowleft - blurlen + 1; i <= -rowleft; i++)
                smoothed += srcp[w0 + i];
              dstp[row] = smoothed / blurlen;   // v. 1.3
            }
            else { // no blur
              dstp[row] = srcp[w0 - rowleft];    // not very precise - may be bicubic?
            }
          }
          else if (rowleft >= row_size && mright) {
            if (blurmax > 0) {
              blurlen = min(blurmax, rowleft - row_size + 1);
              smoothed = 0;
              for (i = row_size + row_size - rowleft - 2; i < row_size + row_size - rowleft - 2 + blurlen; i++)
                smoothed += srcp[w0 + i];
              dstp[row] = smoothed / blurlen;  // v. 1.3
            }
            else {// no blur
              dstp[row] = srcp[w0 + row_size + row_size - rowleft - 2];   // not very precise - may be bicubic?
            }
          }
          else if (border >= 0) {   // if shifted point is out of frame, fill using border value
            dstp[row] = border;
          }


        } // end for
      }
      else if (border >= 0) { // out lines
        for (row = 0; row < row_size; row++) {
          dstp[row] = border;
        }
      }

      dstp += dst_pitch; // next line
    }
  }
  //-----------------------------------------------------------------------------
  else if (tr.dxy == 0 && tr.dyx == 0) { // no rotation, only zoom and translation  - fast

    // prepare positions   (they are not dependent from h) for fast processing
    for (row = 0; row < row_size; row++) {
      xsrc = tr.dxc + tr.dxx*row;
      rowleftwork[row] = (int)floor(xsrc + 0.5);
      rowleft = rowleftwork[row];
    }


    for (h = 0; h < height; h++) {

      ysrc = tr.dyc + tr.dyy*h;

      hlow = (int)floor(ysrc + 0.5);

      if (hlow < 0 && mtop) hlow = -hlow;  // mirror borders
      if (hlow >= height && mbottom) hlow = height + height - hlow - 2;

      w0 = hlow*src_pitch;
      if ((hlow >= 0) && (hlow < height)) {  // incide


        for (row = 0; row < row_size; row++) {

          //					xsrc = tr[0]+tr[1]*row;
          //					rowleft = floor(xsrc);
          rowleft = rowleftwork[row]; //(int)(xsrc);

          if ((rowleft >= 0) && (rowleft < row_size)) {
            dstp[row] = srcp[w0 + rowleft];
          }
          else if (rowleft < 0 && mleft) {
            if (blurmax > 0) {
              blurlen = min(blurmax, -rowleft);
              smoothed = 0;
              for (i = -rowleft - blurlen + 1; i <= -rowleft; i++)
                smoothed += srcp[w0 + i];
              dstp[row] = smoothed / blurlen;   // v. 1.3
            }
            else { // no blur
              dstp[row] = srcp[w0 - rowleft];    // not very precise - may be bicubic?
            }
          }
          else if (rowleft >= row_size && mright) {
            if (blurmax > 0) {
              blurlen = min(blurmax, rowleft - row_size + 1);
              smoothed = 0;
              for (i = row_size + row_size - rowleft - 2; i < row_size + row_size - rowleft - 2 + blurlen; i++)
                smoothed += srcp[w0 + i];
              dstp[row] = smoothed / blurlen;  // v. 1.3
            }
            else {// no blur
              dstp[row] = srcp[w0 + row_size + row_size - rowleft - 2];   // not very precise - may be bicubic?
            }
          }
          else  if (border >= 0) {   // if shifted point is out of frame, fill using border value
            dstp[row] = border;
          }
        } // end for
      }
      else  if (border >= 0) { // out lines
        for (row = 0; row < row_size; row++) {
          dstp[row] = border;
        }
      }

      dstp += dst_pitch; // next line

    }
  }
  //-----------------------------------------------------------------------------
  else { // rotation, zoom and translation - slow

    for (h = 0; h < height; h++) {

      xsrc = tr.dxc + tr.dxy*h;  // part not dependent from row
      ysrc = tr.dyc + tr.dyy*h;

      for (row = 0; row < row_size; row++) {

        rowleft = (int)(xsrc + 0.5); // use simply fast (int), not floor(), since followed check

//				if (xsrc  < rowleft) {
//					rowleft -=1;
//				}

        hlow = (int)(ysrc + 0.5);  // use simply fast  (int), not floor(), since followed check

//				if (ysrc <  hlow) {
//					hlow -=1;
//				}


        if ((rowleft >= 0) && (rowleft < row_size) && (hlow >= 0) && (hlow < height)) {
          dstp[row] = srcp[hlow*src_pitch + rowleft];
        }
        else { // try fill by mirror. Probability of these cases is small
          if (hlow < 0 && mtop) hlow = -hlow;  // mirror borders
          if (hlow >= height && mbottom) hlow = height + height - hlow - 2;
          if (rowleft < 0 && mleft) rowleft = -rowleft;
          if (rowleft >= row_size && mright) rowleft = row_size + row_size - rowleft - 2;
          // check mirrowed
          if ((rowleft >= 0) && (rowleft < row_size) && (hlow >= 0) && (hlow < height)) {
            dstp[row] = srcp[hlow*src_pitch + rowleft];
          }
          else  if (border >= 0) {   // if shifted point is out of frame, fill using border value
            dstp[row] = border;
          }
        }
        xsrc += tr.dxx; // next
        ysrc += tr.dyx;
      } // end for row

      dstp += dst_pitch; // next line
    } //end for h
  } // end if rotation

#ifdef OLD_NOT_USED
  _controlfp(_MCW_RC, _RC_NEAR); // restore rounding mode to default (nearest) mode for /QIfist compiler option
#endif

}

//****************************************************************************
// move plane of nextp frame to dstp for motion compensation by trc, trm with NEAREST pixels
//
void compensate_plane_nearest_stacked(BYTE *dstp, int dst_pitch, const BYTE * srcp, int src_pitch, int row_size, int height, transform tr, int mirror, int border, int *work1row_size, int blurmax)
{
  // if border >=0, then we fill empty edge (border) pixels by that value
  // work1row_size is work array, it must have size >= 1*row_size

  // if mirror > 0, than we fill empty edge (border) pixels by mirrored (reflected) pixels from border,
  // according to bit set of "mirror" parameter:                   (added in v.0.9)
  // mirror = 1 - only top
  // mirror = 2 - only bottom
  // mirror = 4 - only left
  // mirror = 8 - only right
  // any combination - sum of above

  int h, row;
  int rowleft, hlow;
  float xsrc, ysrc;
  int w0;
  int w0_lsb; // stacked
  int inttr0;
  int * rowleftwork = work1row_size;

  int smoothed;
  int blurlen;
  int i;

  // for mirror

  int mtop = mirror & MIRROR_TOP;
  int mbottom = mirror & MIRROR_BOTTOM;
  int mleft = mirror & MIRROR_LEFT;
  int mright = mirror & MIRROR_RIGHT;

  int height_half = height / 2;
  BYTE *dstp_lsb = dstp + height_half*dst_pitch;

#ifdef OLD_NOT_USED
  _controlfp(_MCW_RC, _RC_CHOP); // set rounding mode to truncate to zero mode (C++ standard) for /QIfist compiler option (which is for faster float-int conversion)
#endif

                   //	select if rotation, zoom?

  if (tr.dxy == 0 && tr.dyx == 0 && tr.dxx == 1 && tr.dyy == 1) { // only translation - fast

    for (h = 0; h < height_half; h++) { // height_half instead of height (stacked 16 bit!)

      ysrc = tr.dyc + h;
      hlow = (int)floor(ysrc + 0.5);

      inttr0 = (int)floor(tr.dxc + 0.5);
      rowleft = inttr0;
      //			xsrc = tr[0];
      //			rowleft = (int)floor(xsrc);  // low

      if (hlow < 0 && mtop) hlow = -hlow;  // mirror borders
      if (hlow >= height_half && mbottom) hlow = height_half + height_half - hlow - 2;

      w0 = hlow*src_pitch;
      w0_lsb = (hlow + height_half) * src_pitch; // stacked
      if ((hlow >= 0) && (hlow < height_half)) {  // middle lines


        for (row = 0; row < row_size; row++) {
          rowleft = inttr0 + row;

          //					xsrc = tr[0]+row;            // hided simle formulas,
          //					rowleft = (int)floor(xsrc);  // but slow

          //  x,y point is in square: (rowleft,hlow) to (rowleft+1,hlow+1)

          if ((rowleft >= 0) && (rowleft < row_size)) {
            dstp[row] = srcp[w0 + rowleft];
            dstp_lsb[row] = srcp[w0_lsb + rowleft];
          }
          else if (rowleft < 0 && mleft) {
            if (blurmax > 0) {
              blurlen = min(blurmax, -rowleft);
              smoothed = 0;
              for (i = -rowleft - blurlen + 1; i <= -rowleft; i++)
                smoothed += (srcp[w0 + i] << 8) + (srcp[w0_lsb + i]);
              dstp[row] = (smoothed / blurlen) >> 8;   // v. 1.3
              dstp_lsb[row] = (smoothed / blurlen) & 0xFF;   // v. 1.3
            }
            else { // no blur
              dstp[row] = srcp[w0 - rowleft];    // not very precise - may be bicubic?
              dstp_lsb[row] = srcp[w0_lsb - rowleft];    // not very precise - may be bicubic?
            }
          }
          else if (rowleft >= row_size && mright) {
            if (blurmax > 0) {
              blurlen = min(blurmax, rowleft - row_size + 1);
              smoothed = 0;
              for (i = row_size + row_size - rowleft - 2; i < row_size + row_size - rowleft - 2 + blurlen; i++)
                smoothed += (srcp[w0 + i] << 8) + (srcp[w0_lsb + i]); // smoothed += srcp[w0 + i];
              dstp[row] = (smoothed / blurlen) >> 8;   // v. 1.3
              dstp_lsb[row] = (smoothed / blurlen) & 0xFF;   // v. 1.3
            }
            else {// no blur
              dstp[row] = srcp[w0 + row_size + row_size - rowleft - 2];   // not very precise - may be bicubic?
              dstp_lsb[row] = srcp[w0_lsb + row_size + row_size - rowleft - 2];   // not very precise - may be bicubic?
            }
          }
          else if (border >= 0) {   // if shifted point is out of frame, fill using border value
            dstp[row] = border >> 8;
            dstp_lsb[row] = border & 0xFF;
          }


        } // end for
      }
      else if (border >= 0) { // out lines
        for (row = 0; row < row_size; row++) {
          dstp[row] = border >> 8;
          dstp_lsb[row] = border & 0xFF;
        }
      }

      dstp += dst_pitch; // next line
      dstp_lsb += dst_pitch; // next line of stacked lsb part PF
    }
  }
  //-----------------------------------------------------------------------------
  else if (tr.dxy == 0 && tr.dyx == 0) { // no rotation, only zoom and translation  - fast

                       // prepare positions   (they are not dependent from h) for fast processing
    for (row = 0; row < row_size; row++) {
      xsrc = tr.dxc + tr.dxx*row;
      rowleftwork[row] = (int)floor(xsrc + 0.5);
      rowleft = rowleftwork[row];
    }


    for (h = 0; h < height; h++) {

      ysrc = tr.dyc + tr.dyy*h;

      hlow = (int)floor(ysrc + 0.5);

      if (hlow < 0 && mtop) hlow = -hlow;  // mirror borders
      if (hlow >= height && mbottom) hlow = height + height - hlow - 2;

      w0 = hlow*src_pitch;
      if ((hlow >= 0) && (hlow < height)) {  // incide


        for (row = 0; row < row_size; row++) {

          //					xsrc = tr[0]+tr[1]*row;
          //					rowleft = floor(xsrc);
          rowleft = rowleftwork[row]; //(int)(xsrc);

          if ((rowleft >= 0) && (rowleft < row_size)) {
            dstp[row] = srcp[w0 + rowleft];
          }
          else if (rowleft < 0 && mleft) {
            if (blurmax > 0) {
              blurlen = min(blurmax, -rowleft);
              smoothed = 0;
              for (i = -rowleft - blurlen + 1; i <= -rowleft; i++)
                smoothed += srcp[w0 + i];
              dstp[row] = smoothed / blurlen;   // v. 1.3
            }
            else { // no blur
              dstp[row] = srcp[w0 - rowleft];    // not very precise - may be bicubic?
            }
          }
          else if (rowleft >= row_size && mright) {
            if (blurmax > 0) {
              blurlen = min(blurmax, rowleft - row_size + 1);
              smoothed = 0;
              for (i = row_size + row_size - rowleft - 2; i < row_size + row_size - rowleft - 2 + blurlen; i++)
                smoothed += srcp[w0 + i];
              dstp[row] = smoothed / blurlen;  // v. 1.3
            }
            else {// no blur
              dstp[row] = srcp[w0 + row_size + row_size - rowleft - 2];   // not very precise - may be bicubic?
            }
          }
          else  if (border >= 0) {   // if shifted point is out of frame, fill using border value
            dstp[row] = border;
          }
        } // end for
      }
      else  if (border >= 0) { // out lines
        for (row = 0; row < row_size; row++) {
          dstp[row] = border;
        }
      }

      dstp += dst_pitch; // next line

    }
  }
  //-----------------------------------------------------------------------------
  else { // rotation, zoom and translation - slow

    for (h = 0; h < height; h++) {

      xsrc = tr.dxc + tr.dxy*h;  // part not dependent from row
      ysrc = tr.dyc + tr.dyy*h;

      for (row = 0; row < row_size; row++) {

        rowleft = (int)(xsrc + 0.5); // use simply fast (int), not floor(), since followed check

                       //				if (xsrc  < rowleft) {
                       //					rowleft -=1;
                       //				}

        hlow = (int)(ysrc + 0.5);  // use simply fast  (int), not floor(), since followed check

                       //				if (ysrc <  hlow) {
                       //					hlow -=1;
                       //				}


        if ((rowleft >= 0) && (rowleft < row_size) && (hlow >= 0) && (hlow < height)) {
          dstp[row] = srcp[hlow*src_pitch + rowleft];
        }
        else { // try fill by mirror. Probability of these cases is small
          if (hlow < 0 && mtop) hlow = -hlow;  // mirror borders
          if (hlow >= height && mbottom) hlow = height + height - hlow - 2;
          if (rowleft < 0 && mleft) rowleft = -rowleft;
          if (rowleft >= row_size && mright) rowleft = row_size + row_size - rowleft - 2;
          // check mirrowed
          if ((rowleft >= 0) && (rowleft < row_size) && (hlow >= 0) && (hlow < height)) {
            dstp[row] = srcp[hlow*src_pitch + rowleft];
          }
          else  if (border >= 0) {   // if shifted point is out of frame, fill using border value
            dstp[row] = border;
          }
        }
        xsrc += tr.dxx; // next
        ysrc += tr.dyx;
      } // end for row

      dstp += dst_pitch; // next line
    } //end for h
  } // end if rotation

#ifdef OLD_NOT_USED
  _controlfp(_MCW_RC, _RC_NEAR); // restore rounding mode to default (nearest) mode for /QIfist compiler option
#endif
}



//****************************************************************************
// move plane of nextp frame to dstp for motion compensation by transform tr[]
// with BILINEAR interpolation of discrete neighbour source pixels
//   t[0] = dxc, t[1] = dxx, t[2] = dxy, t[3] = dyc, t[4] = dyx, t[5] = dyy
//
void compensate_plane_bilinear(BYTE *dstp, int dst_pitch, const BYTE * srcp, int src_pitch, int row_size, int height, transform tr, int mirror, int border, int *work2row_size4356, int blurmax)
{
  // work2row_size is work array, it must have size >= 2*row_size

  int h, row;
  int pixel;
  int rowleft, hlow;
  float sx, sy;
  //	float t0, t1, t2, t3, c0,c1,c2,c3;
  float xsrc, ysrc;
  int w0;
  int intcoef[66];
  int intcoef2d[4];
  int ix2, iy2;
  int i, j;
  int inttr0;
  int * rowleftwork = work2row_size4356;
  int * ix2work = rowleftwork + row_size;
  int *intcoef2dzoom0 = ix2work + row_size;//[66][66]; // 4356
  int *intcoef2dzoom = intcoef2dzoom0; // pointer

  int w;

  int smoothed;
  int blurlen;

  // for mirror
  int mtop = mirror & MIRROR_TOP;
  int mbottom = mirror & MIRROR_BOTTOM;
  int mleft = mirror & MIRROR_LEFT;
  int mright = mirror & MIRROR_RIGHT;

  int rowgoodstart, rowgoodend, rowbadstart, rowbadend;

#ifdef OLD_NOT_USED
  _controlfp(_MCW_RC, _RC_CHOP); // set rounding mode to truncate to zero mode (C++ standard) for /QIfist compiler option (which is for faster float-int conversion)
#endif
  // prepare interpolation coefficients tables
  // for position of xsrc in integer grid
  //		sx = (xsrc-rowleft);
  //		sy = (ysrc-hlow);
  //
  //			cx0 = (1-sx);
  //			cx1 = sx;
  //
  //			cy0 = (1-sy);
  //			cy1 = sy;
  //
  // now sx = i/32, sy = j/32  (discrete approximation)

  // float coeff. are changed by integer coeff. scaled by 32
  for (i = 0; i <= 32; i += 1) {
    intcoef[i * 2] = (32 - i);
    intcoef[i * 2 + 1] = i;

  }

  //	select if rotation, zoom?

  if (tr.dxy == 0 && tr.dyx == 0 && tr.dxx == 1 && tr.dyy == 1) { // only translation - fast

    for (h = 0; h < height; h++) {

      ysrc = tr.dyc + h;
      hlow = (int)floor(ysrc);
      iy2 = 2 * ((int)floor((ysrc - hlow) * 32)); //v.1.8.2

      inttr0 = (int)floor(tr.dxc);
      rowleft = inttr0;
      //			xsrc = tr[0];
      //			rowleft = (int)floor(xsrc);  // low
      ix2 = 2 * ((int)floor((tr.dxc - rowleft) * 32)); //v.1.8.2

      for (j = 0; j < 2; j++) {
        for (i = 0; i < 2; i++) {
          intcoef2d[j * 2 + i] = (intcoef[j + iy2] * intcoef[i + ix2]); // 4 coeff. for bilinear 2D
        }
      }

      if (hlow < 0 && mtop) hlow = -hlow;  // mirror borders
      if (hlow >= height && mbottom) hlow = height + height - hlow - 2;

      w0 = hlow*src_pitch;

      if ((hlow >= 0) && (hlow < height - 1)) {  // middle lines

        if (inttr0 >= 0) {
          rowgoodstart = 0;
          rowgoodend = row_size - 1 - inttr0;
          rowbadstart = rowgoodend;
          rowbadend = row_size;
        }
        else {
          rowbadstart = 0;
          rowbadend = -inttr0;
          rowgoodstart = rowbadend;
          rowgoodend = row_size;
        }
        //				int rowgoodendpaired = (rowgoodend/2)*2; //even - but it was a little not optimal
        int rowgoodendpaired = rowgoodstart + ((rowgoodend - rowgoodstart) / 2) * 2;//even length - small fix in v.1.8
        w = w0 + inttr0 + rowgoodstart;
        for (row = rowgoodstart; row < rowgoodendpaired; row += 2) { // paired unroll for speed
//					xsrc = tr[0]+row;            // hided simle formulas,
//					rowleft = (int)floor(xsrc);  // but slow
//					rowleft = inttr0 + row;
//					w = w0+rowleft;
          //  x,y point is in square: (rowleft,hlow) to (rowleft+1,hlow+1)
//					if ( (rowleft >= 0) && (rowleft<row_size-1)  )
          dstp[row] = (intcoef2d[0] * srcp[w] + intcoef2d[1] * srcp[w + 1] \
            + intcoef2d[2] * srcp[w + src_pitch] + intcoef2d[3] * srcp[w + src_pitch + 1]) >> 10; // i.e. divide by 32*32
          dstp[row + 1] = (intcoef2d[0] * srcp[w + 1] + intcoef2d[1] * srcp[w + 2] \
            + intcoef2d[2] * srcp[w + src_pitch + 1] + intcoef2d[3] * srcp[w + src_pitch + 2]) >> 10; // i.e. divide by 32*32
          w += 2;
        }
        for (row = rowgoodendpaired - 1; row < rowgoodend; row++) { // if odd, process  very last
          w = w0 + inttr0 + row;
          dstp[row] = (intcoef2d[0] * srcp[w] + intcoef2d[1] * srcp[w + 1] + \
            intcoef2d[2] * srcp[w + src_pitch] + intcoef2d[3] * srcp[w + src_pitch + 1]) >> 10; // i.e. divide by 32*32
        }
        for (row = rowbadstart; row < rowbadend; row++) {
          rowleft = inttr0 + row;
          w = w0 + rowleft;

          if (rowleft < 0 && mleft) {
            if (blurmax > 0) {
              blurlen = min(blurmax, -rowleft);
              smoothed = 0;
              for (i = -rowleft - blurlen + 1; i <= -rowleft; i++)
                smoothed += srcp[w0 + i];
              dstp[row] = smoothed / blurlen;   // v. 1.3
            }
            else { // no blur
              dstp[row] = srcp[w0 - rowleft];    // not very precise - may be bicubic?
            }
          }
          else if (rowleft >= row_size - 1 && mright) {
            if (blurmax > 0) {
              blurlen = min(blurmax, rowleft - row_size + 2); // v.1.10.1
              smoothed = 0;
              for (i = row_size + row_size - rowleft - 2; i < row_size + row_size - rowleft - 2 + blurlen; i++)
                smoothed += srcp[w0 + i];
              dstp[row] = smoothed / blurlen;  // v. 1.3
            }
            else {// no blur
              dstp[row] = srcp[w0 + row_size + row_size - rowleft - 2];   // not very precise - may be bicubic?
            }
          }
          else  if (border >= 0) {   // if shifted point is out of frame, fill using border value
            dstp[row] = border;
          }

        } // end for
      }
      else if (hlow == height - 1) { // edge (top, bottom) lines
        for (row = 0; row < row_size; row++) {
          rowleft = inttr0 + row;
          if ((rowleft >= 0) && (rowleft < row_size)) {
            dstp[row] = srcp[w0 + rowleft]; 	// nearest pixel, may be bilinear is better
          }
          else if (rowleft < 0 && mleft) {
            dstp[row] = srcp[w0 - rowleft];
          }
          else if (rowleft >= row_size - 1 && mright) {
            dstp[row] = srcp[w0 + row_size + row_size - rowleft - 2];
          }
          else  if (border >= 0) { // left or right
            dstp[row] = border;
          }
        }
      }
      else  if (border >= 0) { // out lines
        for (row = 0; row < row_size; row++) {
          dstp[row] = border;
        }
      }

      dstp += dst_pitch; // next line
    }
  }
  //-----------------------------------------------------------------------------
  else if (tr.dxy == 0 && tr.dyx == 0) { // no rotation, only zoom and translation  - fast

    // prepare positions   (they are not dependent from h) for fast processing
    for (row = 0; row < row_size; row++) {
      xsrc = tr.dxc + tr.dxx*row;
      rowleftwork[row] = (int)floor(xsrc);
      rowleft = rowleftwork[row];
      ix2work[row] = 2 * ((int)floor((xsrc - rowleft) * 32)); //v1.8.2
    }

    for (j = 0; j < 66; j++) {
      for (i = 0; i < 66; i++) {
        intcoef2dzoom[i] = (intcoef[j] * intcoef[i]); //  coeff. for bilinear 2D
      }
      intcoef2dzoom += 66;
    }
    intcoef2dzoom -= 66 * 66; //restore

    for (h = 0; h < height; h++) {

      ysrc = tr.dyc + tr.dyy*h;

      hlow = (int)floor(ysrc);
      iy2 = 2 * ((int)floor((ysrc - hlow) * 32));//v1.8.2

      if (hlow < 0 && mtop) hlow = -hlow;  // mirror borders
      if (hlow >= height && mbottom) hlow = height + height - hlow - 2;

      w0 = hlow*src_pitch;

      if ((hlow >= 0) && (hlow < height - 1)) {  // incide

        intcoef2dzoom = intcoef2dzoom0;
        intcoef2dzoom += iy2 * 66;


        for (row = 0; row < row_size; row++) {

          //					xsrc = tr[0]+tr[1]*row;
          rowleft = rowleftwork[row]; //(int)(xsrc);
//					rowleft = floor(xsrc);

          //  x,y point is in square: (rowleft,hlow) to (rowleft+1,hlow+1)

          if ((rowleft >= 0) && (rowleft < row_size - 1)) {

            //						ix2 = 2*((int)((xsrc-rowleft)*32));
            ix2 = ix2work[row];
            w = w0 + rowleft;

            //						pixel = ( intcoef[iy2]*(intcoef[ix2]*srcp[w] + intcoef[ix2+1]*srcp[w+1] ) + \
            //								intcoef[iy2+1]*(intcoef[ix2]*srcp[w+src_pitch] + intcoef[ix2+1]*srcp[w+src_pitch+1] ) )/1024;
            pixel = (intcoef2dzoom[ix2] * srcp[w] + intcoef2dzoom[ix2 + 1] * srcp[w + 1] + \
              intcoef2dzoom[ix2 + 66] * srcp[w + src_pitch] + intcoef2dzoom[ix2 + 67] * srcp[w + src_pitch + 1]) >> 10; // v1.6

//						dstp[row] = max(min(pixel,255),0);
            dstp[row] = pixel;   // maxmin disabled in v1.6
          }
          else if (rowleft < 0 && mleft) {
            if (blurmax > 0) { // 1.13 fix
              blurlen = min(blurmax, -rowleft);
              smoothed = 0;
              for (i = -rowleft - blurlen + 1; i <= -rowleft; i++)
                smoothed += srcp[w0 + i];
              dstp[row] = smoothed / blurlen;   // v. 1.3
            }
            else { // no blur
              dstp[row] = srcp[w0 - rowleft];
            }
          }
          else if (rowleft >= row_size - 1 && mright) {
            if (blurmax > 0) {
              blurlen = min(blurmax, rowleft - row_size + 2); // v1.10.1
              smoothed = 0;
              for (i = row_size + row_size - rowleft - 2; i < row_size + row_size - rowleft - 2 + blurlen; i++)
                smoothed += srcp[w0 + i];
              dstp[row] = smoothed / blurlen;  // v. 1.3
            }
            else {// no blur
              dstp[row] = srcp[w0 + row_size + row_size - rowleft - 2];   // not very precise - may be bicubic?
            }
          }
          else if (border >= 0) {   // if shifted point is out of frame, fill using border value
            dstp[row] = border;
          }
        } // end for
      }
      else if (hlow == height - 1) { // edge ( bottom) lines
        for (row = 0; row < row_size; row++) {
          rowleft = rowleftwork[row];
          if ((rowleft >= 0) && (rowleft < row_size)) {
            dstp[row] = srcp[rowleft + hlow*src_pitch]; 	// nearest pixel, may be bilinear is better
          }
          else  if (border >= 0) { // left or right
            dstp[row] = border;
          }
        }
      }
      else  if (border >= 0) { // out lines
        for (row = 0; row < row_size; row++) {
          dstp[row] = border;
        }
      }

      dstp += dst_pitch; // next line

    }
  }
  //-----------------------------------------------------------------------------
  else { // rotation, zoom and translation - slow

    for (h = 0; h < height; h++) {

      xsrc = tr.dxc + tr.dxy*h;  // part not dependent from row
      ysrc = tr.dyc + tr.dyy*h;

      for (row = 0; row < row_size; row++) {

        rowleft = (int)(xsrc); // use simply fast (int), not floor(), since followed check >1
        sx = xsrc - rowleft;
        if (sx < 0) {
          sx += 1;
          rowleft -= 1;
        }

        hlow = (int)(ysrc);  // use simply fast  (int), not floor(), since followed check >1
        sy = ysrc - hlow;
        if (sy < 0) {
          sy += 1;
          hlow -= 1;
        }

        //  x,y point is in square: (rowleft,hlow) to (rowleft+1,hlow+1)


        if ((rowleft >= 0) && (rowleft < row_size - 1) && (hlow >= 0) && (hlow < height - 1)) {

          ix2 = ((int)(sx * 32)) << 1; // i.e. *2
          iy2 = ((int)(sy * 32)) << 1;   // i.e. *2
          w0 = rowleft + hlow*src_pitch;

          pixel = ((intcoef[ix2] * srcp[w0] + intcoef[ix2 + 1] * srcp[w0 + 1])*intcoef[iy2] + \
            (intcoef[ix2] * srcp[w0 + src_pitch] + intcoef[ix2 + 1] * srcp[w0 + src_pitch + 1])*intcoef[iy2 + 1]) >> 10;

          //					dstp[row] = max(min(pixel,255),0);
          dstp[row] = pixel;       //maxmin disabled in v1.6
        }
        else {
          if (hlow < 0 && mtop) hlow = -hlow;  // mirror borders
          if (hlow >= height && mbottom) hlow = height + height - hlow - 2;
          if (rowleft < 0 && mleft) rowleft = -rowleft;
          if (rowleft >= row_size && mright) rowleft = row_size + row_size - rowleft - 2;
          // check mirrowed
          if ((rowleft >= 0) && (rowleft < row_size) && (hlow >= 0) && (hlow < height)) {
            dstp[row] = srcp[hlow*src_pitch + rowleft];
          }
          else  if (border >= 0) {   // if shifted point is out of frame, fill using border value
            dstp[row] = border;
          }
        }
        xsrc += tr.dxx;  // next
        ysrc += tr.dyx;
      } // end for row

      dstp += dst_pitch; // next line
    } //end for h
  } // end if rotation

#ifdef OLD_NOT_USED
  _controlfp(_MCW_RC, _RC_NEAR); // restore rounding mode to default (nearest) mode for /QIfist compiler option
#endif
}

//****************************************************************************
// move plane of nextp frame to dstp for motion compensation by transform tr[]
// with BICUBIC interpolation of discrete neighbour source pixels
//
//   t[0] = dxc, t[1] = dxx, t[2] = dxy, t[3] = dyc, t[4] = dyx, t[5] = dyy
//
void compensate_plane_bicubic(BYTE *dstp, int dst_pitch, const BYTE * srcp, int src_pitch, int row_size, int height, transform tr, int mirror, int border, int *work2width1030, int blurmax)
{
  // work2width1030 is integer work array, it must have size >= 2*row_size+1030

  int h, row;
  int pixel;
  int rowleft, hlow;
  float sx, sy;
  //	float t0, t1, t2, t3, c0,c1,c2,c3;
  float xsrc, ysrc;
  int w0;
  int ix4, iy4;
  int i, j;
  int inttr0, inttr3;
  int * rowleftwork = work2width1030;
  int * ix4work = work2width1030 + row_size;
  //	int intcoef[1030];
  int *intcoef = ix4work + row_size;
  int ts[4];
  int intcoef2d[16];

  int w;
  int smoothed;
  int blurlen;

  // for mirror
  int mtop = mirror & MIRROR_TOP;
  int mbottom = mirror & MIRROR_BOTTOM;
  int mleft = mirror & MIRROR_LEFT;
  int mright = mirror & MIRROR_RIGHT;

#ifdef OLD_NOT_USED
  _controlfp(_MCW_RC, _RC_CHOP); // set rounding mode to truncate to zero mode (C++ standard) for /QIfist compiler option (which is for faster float-int conversion)
#endif
  // prepare interpolation coefficients tables
  // for position of xsrc in integer grid
  //		sx = (xsrc-rowleft);
  //		sy = (ysrc-hlow);
  //
  //			cx0 = -sx*(1-sx)*(1-sx);
  //			cx1 = (1-2*sx*sx+sx*sx*sx);
  //			cx2 =  sx*(1+sx-sx*sx);
  //			cx3 =  -sx*sx*(1-sx);
  //
  //			cy0 = -sy*(1-sy)*(1-sy);
  //			cy1 = (1-2*sy*sy+sy*sy*sy);
  //			cy2 =  sy*(1+sy-sy*sy);
  //			cy3 =  -sy*sy*(1-sy);
  //
  // now sx = i/256, sy = j/256  (discrete approximation)

  // float coeff. are changed by integer coeff. scaled by 256*256*256/8192 = 2048
  for (i = 0; i <= 256; i += 1) { // 257 steps, 1028 numbers
    intcoef[i * 4] = -((i*(256 - i)*(256 - i))) / 8192;
    intcoef[i * 4 + 1] = (256 * 256 * 256 - 2 * 256 * i*i + i*i*i) / 8192;
    intcoef[i * 4 + 2] = (i*(256 * 256 + 256 * i - i*i)) / 8192;
    intcoef[i * 4 + 3] = -(i*i*(256 - i)) / 8192;

  }

  //	select if rotation, zoom

  if (tr.dxy == 0 && tr.dyx == 0 && tr.dxx == 1 && tr.dyy == 1) { // only translation - fast

    for (h = 0; h < height; h++) {

      ysrc = tr.dyc + h;
      inttr3 = (int)floor(tr.dyc);
      hlow = (int)floor(ysrc);
      iy4 = 4 * ((int)((ysrc - hlow) * 256));

      inttr0 = (int)floor(tr.dxc);
      rowleft = inttr0;
      //			xsrc = tr[0];
      //			rowleft = (int)floor(xsrc);  // low
      ix4 = 4 * ((int)((tr.dxc - inttr0) * 256));

      for (j = 0; j < 4; j++) {
        for (i = 0; i < 4; i++) {
          intcoef2d[j * 4 + i] = ((intcoef[j + iy4] * intcoef[i + ix4])) / 2048; // 16 coeff. for bicubic 2D, scaled by 2048
        }
      }

      if (hlow < 0 && mtop) hlow = -hlow;  // mirror borders
      if (hlow >= height && mbottom) hlow = height + height - hlow - 2;

      w0 = hlow*src_pitch;

      if ((hlow >= 1) && (hlow < height - 2)) {  // middle lines

        for (row = 0; row < row_size; row++) {

          rowleft = inttr0 + row;

          //	xsrc = tr[0]+row;

          //  x,y point is in square: (rowleft,hlow) to (rowleft+1,hlow+1)

          if ((rowleft >= 1) && (rowleft < row_size - 2)) {
            w = w0 + rowleft;

            pixel = (intcoef2d[0] * srcp[w - src_pitch - 1] + intcoef2d[1] * srcp[w - src_pitch] + intcoef2d[2] * srcp[w - src_pitch + 1] + intcoef2d[3] * srcp[w - src_pitch + 2] + \
              intcoef2d[4] * srcp[w - 1] + intcoef2d[5] * srcp[w] + intcoef2d[6] * srcp[w + 1] + intcoef2d[7] * srcp[w + 2] + \
              intcoef2d[8] * srcp[w + src_pitch - 1] + intcoef2d[9] * srcp[w + src_pitch] + intcoef2d[10] * srcp[w + src_pitch + 1] + intcoef2d[11] * srcp[w + src_pitch + 2] + \
              intcoef2d[12] * srcp[w + src_pitch * 2 - 1] + intcoef2d[13] * srcp[w + src_pitch * 2] + intcoef2d[14] * srcp[w + src_pitch * 2 + 1] + intcoef2d[15] * srcp[w + src_pitch * 2 + 2] + \
              // +1024: fix in 1.13.1
              1024) >> 11;  // i.e. /2048

            dstp[row] = max(min(pixel, 255), 0);

          }
          else if (rowleft < 0 && mleft) {
            if (blurmax > 0) {
              blurlen = min(blurmax, -rowleft);
              smoothed = 0;
              for (i = -rowleft - blurlen + 1; i <= -rowleft; i++)
                smoothed += srcp[w0 + i];
              dstp[row] = smoothed / blurlen;   // v. 1.3
            }
            else { // no blur
              dstp[row] = srcp[w0 - rowleft];    // not very precise - may be bicubic?
            }
          }
          else if (rowleft >= row_size && mright) {
            if (blurmax > 0) {
              blurlen = min(blurmax, rowleft - row_size + 1);
              smoothed = 0;
              for (i = row_size + row_size - rowleft - 2; i < row_size + row_size - rowleft - 2 + blurlen; i++)
                smoothed += srcp[w0 + i];
              dstp[row] = smoothed / blurlen;  // v. 1.3
            }
            else {// no blur
              dstp[row] = srcp[w0 + row_size + row_size - rowleft - 2];   // not very precise - may be bicubic?
            }
          }
          else if (rowleft == 0 || rowleft == row_size - 1 || rowleft == row_size - 2) { // edges
            dstp[row] = srcp[w0 + rowleft];
          }
          else  if (border >= 0) {   // if shifted point is out of frame, fill using border value
            dstp[row] = border;
          }

        } // end for
      }
      else if (hlow == 0 || hlow == height - 2) { // near edge (top-1, bottom-1) lines
        for (row = 0; row < row_size; row++) {
          rowleft = inttr0 + row;
          sx = tr.dxc - inttr0;
          sy = tr.dyc - inttr3;
          if ((rowleft >= 0) && (rowleft < row_size - 1)) { // bug fixed for right edge in v.1.1.1
            w = w0 + rowleft;
            dstp[row] = (int)((1.0 - sy)*((1.0 - sx)*srcp[w] + sx*srcp[w + 1]) + \
              sy*((1.0 - sx)*srcp[w + src_pitch] + sx*srcp[w + src_pitch + 1])); // bilinear
          }
          else if (rowleft == row_size - 1) { // added in v.1.1.1
            dstp[row] = srcp[rowleft + w0];
          }
          else if (rowleft < 0 && mleft) {
            dstp[row] = srcp[w0 - rowleft];       // not very precise - may be bicubic?
          }
          else if (rowleft >= row_size && mright) {
            dstp[row] = srcp[w0 + row_size + row_size - rowleft - 2];
          }
          else  if (border >= 0) { // left or right
            dstp[row] = border;
          }
        }
      }
      else if (hlow == height - 1) { // bottom line
        for (row = 0; row < row_size; row++) {
          rowleft = inttr0 + row;
          if (rowleft >= 0 && rowleft < row_size) {
            dstp[row] = srcp[w0 + rowleft];
          }
          else if (rowleft < 0 && mleft) {
            dstp[row] = srcp[w0 - rowleft];       // not very precise - may be bicubic?
          }
          else if (rowleft >= row_size && mright) {
            dstp[row] = srcp[w0 + row_size + row_size - rowleft - 2];
          }
          else  if (border >= 0) { // left or right
            dstp[row] = border;
          }
        }
      }
      else  if (border >= 0) { // out lines
        for (row = 0; row < row_size; row++) {
          dstp[row] = border;
        }
      }

      dstp += dst_pitch; // next line
    }
  }
  //-----------------------------------------------------------------------------
  else if (tr.dxy == 0 && tr.dyx == 0) { // no rotation, only zoom and translation  - fast

    // prepare positions   (they are not dependent from h) for fast processing
    for (row = 0; row < row_size; row++) {
      xsrc = tr.dxc + tr.dxx*row;
      rowleftwork[row] = (int)floor(xsrc);
      rowleft = rowleftwork[row];
      ix4work[row] = 4 * ((int)((xsrc - rowleft) * 256));
    }


    for (h = 0; h < height; h++) {

      ysrc = tr.dyc + tr.dyy*h;

      hlow = (int)floor(ysrc);
      iy4 = 4 * ((int)((ysrc - hlow) * 256));

      sy = ysrc - hlow;

      if (hlow < 0 && mtop) hlow = -hlow;  // mirror borders
      if (hlow >= height && mbottom) hlow = height + height - hlow - 2;

      w0 = hlow*src_pitch;
      if ((hlow >= 1) && (hlow < height - 2)) {  // incide

        for (row = 0; row < row_size; row++) {

          //					xsrc = tr[0]+tr[1]*row;
          //					rowleft = floor(xsrc);
          rowleft = rowleftwork[row]; //(int)(xsrc);

          //  x,y point is in square: (rowleft,hlow) to (rowleft+1,hlow+1)

          if ((rowleft >= 1) && (rowleft < row_size - 2)) {

            //						ix4 = 4*((int)((xsrc-rowleft)*256));
            ix4 = ix4work[row];
            w = w0 + rowleft;


            srcp -= src_pitch;         // prev line
            ts[0] = (intcoef[ix4] * srcp[w - 1] + intcoef[ix4 + 1] * srcp[w] + intcoef[ix4 + 2] * srcp[w + 1] + intcoef[ix4 + 3] * srcp[w + 2]);
            srcp += src_pitch;         // next line
            ts[1] = (intcoef[ix4] * srcp[w - 1] + intcoef[ix4 + 1] * srcp[w] + intcoef[ix4 + 2] * srcp[w + 1] + intcoef[ix4 + 3] * srcp[w + 2]);
            srcp += src_pitch;         // next line
            ts[2] = (intcoef[ix4] * srcp[w - 1] + intcoef[ix4 + 1] * srcp[w] + intcoef[ix4 + 2] * srcp[w + 1] + intcoef[ix4 + 3] * srcp[w + 2]);
            srcp += src_pitch;         // next line
            ts[3] = (intcoef[ix4] * srcp[w - 1] + intcoef[ix4 + 1] * srcp[w] + intcoef[ix4 + 2] * srcp[w + 1] + intcoef[ix4 + 3] * srcp[w + 2]);

            srcp -= (src_pitch << 1);  // restore pointer, changed to shift in v 1.1.1

            pixel = (intcoef[iy4] * ts[0] + intcoef[iy4 + 1] * ts[1] + intcoef[iy4 + 2] * ts[2] + intcoef[iy4 + 3] * ts[3]) >> 22;

            dstp[row] = max(min(pixel, 255), 0);
          }
          else if (rowleft < 0 && mleft) {
            if (blurmax > 0) {
              blurlen = min(blurmax, -rowleft);
              smoothed = 0;
              for (i = -rowleft - blurlen + 1; i <= -rowleft; i++)
                smoothed += srcp[w0 + i];
              dstp[row] = smoothed / blurlen;   // v. 1.3
            }
            else { // no blur
              dstp[row] = srcp[w0 - rowleft];    // not very precise - may be bicubic?
            }
          }
          else if (rowleft >= row_size && mright) {
            if (blurmax > 0) {
              blurlen = min(blurmax, rowleft - row_size + 1);
              smoothed = 0;
              for (i = row_size + row_size - rowleft - 2; i < row_size + row_size - rowleft - 2 + blurlen; i++)
                smoothed += srcp[w0 + i];
              dstp[row] = smoothed / blurlen;  // v. 1.3
            }
            else {// no blur
              dstp[row] = srcp[w0 + row_size + row_size - rowleft - 2];   // not very precise - may be bicubic?
            }
          }
          else if (rowleft == 0 || rowleft == row_size - 1 || rowleft == row_size - 2) { // edges
            dstp[row] = srcp[w0 + rowleft];
          }
          else  if (border >= 0) {   // if shifted point is out of frame, fill using border value
            dstp[row] = border;
          }
        } // end for
      }
      else if (hlow == 0 || hlow == height - 2) { // near edge (top-1, bottom-1) lines
        for (row = 0; row < row_size; row++) {
          rowleft = rowleftwork[row];
          if ((rowleft >= 0) && (rowleft < row_size - 1)) { // bug fixed for right bound in v.1.10.0
            xsrc = tr.dxc + tr.dxx*row;
            sx = xsrc - rowleft;
            w = w0 + rowleft;
            pixel = (int)((1.0 - sy)*((1.0 - sx)*srcp[w] + sx*srcp[w + 1]) + \
              sy*((1.0 - sx)*srcp[w + src_pitch] + sx*srcp[w + src_pitch + 1])); // bilinear
            dstp[row] = max(min(pixel, 255), 0);
          }
          else if (rowleft == row_size - 1) { // added in v.1.1.1
            dstp[row] = srcp[rowleft + w0];
          }
          else if (rowleft < 0 && mleft) {
            dstp[row] = srcp[w0 - rowleft];       // not very precise - may be bicubic?
          }
          else if (rowleft >= row_size && mright) {
            dstp[row] = srcp[w0 + row_size + row_size - rowleft - 2];
          }
          else  if (border >= 0) { // left or right
            dstp[row] = border;
          }
        }
      }
      else if (hlow == height - 1) { // bottom line
        for (row = 0; row < row_size; row++) {
          rowleft = rowleftwork[row];
          if (rowleft >= 0 && rowleft < row_size) {
            dstp[row] = (srcp[w0 + rowleft] + srcp[w0 + rowleft - src_pitch]) / 2; // for some smoothing
          }
          else if (rowleft < 0 && mleft) {
            dstp[row] = srcp[w0 - rowleft];
          }
          else if (rowleft >= row_size && mright) {
            dstp[row] = srcp[w0 + row_size + row_size - rowleft - 2];
          }
          else  if (border >= 0) { // left or right
            dstp[row] = border;
          }
        }
      }
      else  if (border >= 0) { // out lines
        for (row = 0; row < row_size; row++) {
          // bug fixed here in v. 0.9.1 (access violation - bad w0)
/*					rowleft = rowleftwork[row];
          if (rowleft >=0 && rowleft < row_size) {
            dstp[row] = srcp[w0+rowleft];
          }
          else if ( rowleft < 0 && mleft) {
            dstp[row] = srcp[w0-rowleft];       // not very precise - may be bicubic?
          }
          else if ( rowleft >= row_size && mright) {
            dstp[row] = srcp[w0+row_size + row_size - rowleft -2];
          }
          else  if (border >=0 ){ // left or right
*/
          dstp[row] = border;
          //					}
        }
      }

      dstp += dst_pitch; // next line

    }
  }
  //-----------------------------------------------------------------------------
  else { // rotation, zoom and translation - slow

    for (h = 0; h < height; h++) {

      for (row = 0; row < row_size; row++) {

        xsrc = tr.dxc + tr.dxx*row + tr.dxy*h;
        ysrc = tr.dyc + tr.dyx*row + tr.dyy*h;
        rowleft = (int)(xsrc); // use simply fast (int), not floor(), since followed check >1
        if (xsrc < rowleft) {
          rowleft -= 1;
        }

        hlow = (int)(ysrc);  // use simply fast  (int), not floor(), since followed check >1
        if (ysrc < hlow) {
          hlow -= 1;
        }

        //  x,y point is in square: (rowleft,hlow) to (rowleft+1,hlow+1)

        if ((rowleft >= 1) && (rowleft < row_size - 2) && (hlow >= 1) && (hlow < height - 2)) {

          ix4 = 4 * ((int)((xsrc - rowleft) * 256));

          w0 = rowleft + hlow*src_pitch;

          srcp -= src_pitch;         // prev line
          ts[0] = (intcoef[ix4] * srcp[w0 - 1] + intcoef[ix4 + 1] * srcp[w0] + intcoef[ix4 + 2] * srcp[w0 + 1] + intcoef[ix4 + 3] * srcp[w0 + 2]);
          srcp += src_pitch;         // next line
          ts[1] = (intcoef[ix4] * srcp[w0 - 1] + intcoef[ix4 + 1] * srcp[w0] + intcoef[ix4 + 2] * srcp[w0 + 1] + intcoef[ix4 + 3] * srcp[w0 + 2]);
          srcp += src_pitch;         // next line
          ts[2] = (intcoef[ix4] * srcp[w0 - 1] + intcoef[ix4 + 1] * srcp[w0] + intcoef[ix4 + 2] * srcp[w0 + 1] + intcoef[ix4 + 3] * srcp[w0 + 2]);
          srcp += src_pitch;         // next line
          ts[3] = (intcoef[ix4] * srcp[w0 - 1] + intcoef[ix4 + 1] * srcp[w0] + intcoef[ix4 + 2] * srcp[w0 + 1] + intcoef[ix4 + 3] * srcp[w0 + 2]);

          srcp -= (src_pitch << 1);  // restore pointer, changed to shift in v.1.1.1


          iy4 = ((int)((ysrc - hlow) * 256)) << 2; //changed to shift in v.1.1.1

          pixel = (intcoef[iy4] * ts[0] + intcoef[iy4 + 1] * ts[1] + intcoef[iy4 + 2] * ts[2] + intcoef[iy4 + 3] * ts[3]) >> 22;
          dstp[row] = max(min(pixel, 255), 0);
        }
        else {
          if (hlow < 0 && mtop) hlow = -hlow;  // mirror borders
          if (hlow >= height && mbottom) hlow = height + height - hlow - 2;
          if (rowleft < 0 && mleft) rowleft = -rowleft;
          if (rowleft >= row_size && mright) rowleft = row_size + row_size - rowleft - 2;
          // check mirrowed
          if ((rowleft >= 0) && (rowleft < row_size) && (hlow >= 0) && (hlow < height)) {
            dstp[row] = srcp[hlow*src_pitch + rowleft];
          }
          else  if (border >= 0) {   // if shifted point is out of frame, fill using border value
            dstp[row] = border;
          }
        }
      } // end for row

      dstp += dst_pitch; // next line
    } //end for h
  } // end if rotation

#ifdef OLD_NOT_USED
  _controlfp(_MCW_RC, _RC_NEAR); // restore rounding mode to default (nearest) mode for /QIfist compiler option
#endif

}
#endif 
