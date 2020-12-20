/*****************************************************************************
 * cpu.c: h264 encoder library
 *****************************************************************************
 * Copyright (C) 2003 Laurent Aimar
 * $Id: cpu.c,v 1.1 2004/06/03 19:27:06 fenrir Exp $
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/
// adapted to be used by MVTools by TSchniede

#include "AnaFlags.h"
#include "cpu.h"
#include "debugprintf.h"
#include "string.h"
#include <avisynth.h>

#define uint32_t unsigned int

int conv_cpuf_flags_to_cpu(int avscpu)
{
  int acpu = 0;
  if (avscpu & CPUF_SSE2) acpu |= CPU_SSE2;
  if (avscpu & CPUF_SSE3) acpu |= CPU_SSE3;
  if (avscpu & CPUF_SSSE3) acpu |= CPU_SSSE3;
  if (avscpu & CPUF_SSE4_1) acpu |= CPU_SSE4;
  if (avscpu & CPUF_SSE4_2) acpu |= CPU_SSE42;
  if (avscpu & CPUF_AVX) acpu |= CPU_AVX;
  if (avscpu & CPUF_AVX2) acpu |= CPU_AVX2;
  return acpu;
}


#undef uint32_t
