;*****************************************************************************
;* sad-a.asm: x86 sad functions
;*****************************************************************************
;* Copyright (C) 2003-2014 x264 project
;*
;* Authors: Loren Merritt <lorenm@u.washington.edu>
;*          Fiona Glaser <fiona@x264.com>
;*          Laurent Aimar <fenrir@via.ecp.fr>
;*          Alex Izvorski <aizvorksi@gmail.com>
;*
;* This program is free software; you can redistribute it and/or modify
;* it under the terms of the GNU General Public License as published by
;* the Free Software Foundation; either version 2 of the License, or
;* (at your option) any later version.
;*
;* This program is distributed in the hope that it will be useful,
;* but WITHOUT ANY WARRANTY; without even the implied warranty of
;* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;* GNU General Public License for more details.
;*
;* You should have received a copy of the GNU General Public License
;* along with this program; if not, write to the Free Software
;* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111, USA.
;*
;* This program is also available under a commercial proprietary license.
;* For more information, contact us at licensing@x264.com.
;*****************************************************************************

%include "include/x86inc.asm"
%include "include/x86util.asm"

SECTION_RODATA 32

pb_shuf8x8c2: times 2 db 0,0,0,0,8,8,8,8,-1,-1,-1,-1,-1,-1,-1,-1
hpred_shuf: db 0,0,2,2,8,8,10,10,1,1,3,3,9,9,11,11

SECTION .text

cextern pb_3
cextern pb_shuf8x8c
cextern pw_8
cextern sw_64

;=============================================================================
; SAD MMX
;=============================================================================

%macro SAD_INC_2x16P 0
    movq    mm1,    [r0]
    movq    mm2,    [r0+8]
    movq    mm3,    [r0+r1]
    movq    mm4,    [r0+r1+8]
    psadbw  mm1,    [r2]
    psadbw  mm2,    [r2+8]
    psadbw  mm3,    [r2+r3]
    psadbw  mm4,    [r2+r3+8]
    lea     r0,     [r0+2*r1]
    paddw   mm1,    mm2
    paddw   mm3,    mm4
    lea     r2,     [r2+2*r3]
    paddw   mm0,    mm1
    paddw   mm0,    mm3
%endmacro

%macro SAD_INC_2x8P 0
    movq    mm1,    [r0]
    movq    mm2,    [r0+r1]
    psadbw  mm1,    [r2]
    psadbw  mm2,    [r2+r3]
    lea     r0,     [r0+2*r1]
    paddw   mm0,    mm1
    paddw   mm0,    mm2
    lea     r2,     [r2+2*r3]
%endmacro

%macro SAD_INC_2x4P 0
    movd    mm1,    [r0]
    movd    mm2,    [r2]
    punpckldq mm1,  [r0+r1]
    punpckldq mm2,  [r2+r3]
    psadbw  mm1,    mm2
    paddw   mm0,    mm1
    lea     r0,     [r0+2*r1]
    lea     r2,     [r2+2*r3]
%endmacro

;-----------------------------------------------------------------------------
; int pixel_sad_16x16( uint8_t *, intptr_t, uint8_t *, intptr_t )
;-----------------------------------------------------------------------------
%macro SAD 2
cglobal pixel_sad_%1x%2_mmx2, 4,4
%if ARCH_X86_64
	movsxd r1q, r1d
	movsxd r3q, r3d
%endif
    pxor    mm0, mm0
%rep %2/2
    SAD_INC_2x%1P
%endrep
    movd    eax, mm0
    RET
%endmacro

SAD 16, 16
SAD 16,  8
SAD 16,  4
SAD 16,  2
SAD  8, 16
SAD  8,  8
SAD  8,  4
SAD  8,  2
SAD  4, 16
SAD  4,  8
SAD  4,  4
SAD  4,  2



;=============================================================================
; SAD XMM
;=============================================================================

%macro SAD_END_SSE2 0
    movhlps m1, m0
    paddw   m0, m1
    movd   eax, m0
    RET
%endmacro

%macro SAD_W16 0
;-----------------------------------------------------------------------------
; int pixel_sad_16x16( uint8_t *, intptr_t, uint8_t *, intptr_t )
;-----------------------------------------------------------------------------
cglobal pixel_sad_16x16, 4,4,8
%if ARCH_X86_64
	movsxd r1q, r1d
	movsxd r3q, r3d
%endif
    movu    m0, [r2]
    movu    m1, [r2+r3]
    lea     r2, [r2+2*r3]
    movu    m2, [r2]
    movu    m3, [r2+r3]
    lea     r2, [r2+2*r3]
    psadbw  m0, [r0]
    psadbw  m1, [r0+r1]
    lea     r0, [r0+2*r1]
    movu    m4, [r2]
    paddw   m0, m1
    psadbw  m2, [r0]
    psadbw  m3, [r0+r1]
    lea     r0, [r0+2*r1]
    movu    m5, [r2+r3]
    lea     r2, [r2+2*r3]
    paddw   m2, m3
    movu    m6, [r2]
    movu    m7, [r2+r3]
    lea     r2, [r2+2*r3]
    paddw   m0, m2
    psadbw  m4, [r0]
    psadbw  m5, [r0+r1]
    lea     r0, [r0+2*r1]
    movu    m1, [r2]
    paddw   m4, m5
    psadbw  m6, [r0]
    psadbw  m7, [r0+r1]
    lea     r0, [r0+2*r1]
    movu    m2, [r2+r3]
    lea     r2, [r2+2*r3]
    paddw   m6, m7
    movu    m3, [r2]
    paddw   m0, m4
    movu    m4, [r2+r3]
    lea     r2, [r2+2*r3]
    paddw   m0, m6
    psadbw  m1, [r0]
    psadbw  m2, [r0+r1]
    lea     r0, [r0+2*r1]
    movu    m5, [r2]
    paddw   m1, m2
    psadbw  m3, [r0]
    psadbw  m4, [r0+r1]
    lea     r0, [r0+2*r1]
    movu    m6, [r2+r3]
    lea     r2, [r2+2*r3]
    paddw   m3, m4
    movu    m7, [r2]
    paddw   m0, m1
    movu    m1, [r2+r3]
    paddw   m0, m3
    psadbw  m5, [r0]
    psadbw  m6, [r0+r1]
    lea     r0, [r0+2*r1]
    paddw   m5, m6
    psadbw  m7, [r0]
    psadbw  m1, [r0+r1]
    paddw   m7, m1
    paddw   m0, m5
    paddw   m0, m7
    SAD_END_SSE2

;-----------------------------------------------------------------------------
; int pixel_sad_16x8( uint8_t *, intptr_t, uint8_t *, intptr_t )
;-----------------------------------------------------------------------------
cglobal pixel_sad_16x8, 4,4
%if ARCH_X86_64
	movsxd r1q, r1d
	movsxd r3q, r3d
%endif
    movu    m0, [r2]
    movu    m2, [r2+r3]
    lea     r2, [r2+2*r3]
    movu    m3, [r2]
    movu    m4, [r2+r3]
    psadbw  m0, [r0]
    psadbw  m2, [r0+r1]
    lea     r0, [r0+2*r1]
    psadbw  m3, [r0]
    psadbw  m4, [r0+r1]
    lea     r0, [r0+2*r1]
    lea     r2, [r2+2*r3]
    paddw   m0, m2
    paddw   m3, m4
    paddw   m0, m3
    movu    m1, [r2]
    movu    m2, [r2+r3]
    lea     r2, [r2+2*r3]
    movu    m3, [r2]
    movu    m4, [r2+r3]
    psadbw  m1, [r0]
    psadbw  m2, [r0+r1]
    lea     r0, [r0+2*r1]
    psadbw  m3, [r0]
    psadbw  m4, [r0+r1]
    lea     r0, [r0+2*r1]
    lea     r2, [r2+2*r3]
    paddw   m1, m2
    paddw   m3, m4
    paddw   m0, m1
    paddw   m0, m3
    SAD_END_SSE2
%endmacro

; pixel_sad_16x16_sse2, pixel_sad_16x8_sse2
INIT_XMM sse2
SAD_W16
INIT_XMM sse3
SAD_W16
INIT_XMM sse2, aligned
SAD_W16

%macro SAD_INC_4x8P_SSE 1
    movq    m1, [r0]
    movq    m2, [r0+r1]
    lea     r0, [r0+2*r1]
    movq    m3, [r2]
    movq    m4, [r2+r3]
    lea     r2, [r2+2*r3]
    movhps  m1, [r0]
    movhps  m2, [r0+r1]
    movhps  m3, [r2]
    movhps  m4, [r2+r3]
    lea     r0, [r0+2*r1]
    psadbw  m1, m3
    psadbw  m2, m4
    lea     r2, [r2+2*r3]
    ACCUM paddw, 0, 1, %1
    paddw   m0, m2
%endmacro

INIT_XMM
;Even on Nehalem, no sizes other than 8x16 benefit from this method.
cglobal pixel_sad_8x16_sse2, 4,4
%if ARCH_X86_64
	movsxd r1q, r1d
	movsxd r3q, r3d
%endif
    SAD_INC_4x8P_SSE 0
    SAD_INC_4x8P_SSE 1
    SAD_INC_4x8P_SSE 1
    SAD_INC_4x8P_SSE 1
    SAD_END_SSE2
    RET


;=============================================================================
; SAD cacheline split
;=============================================================================

; Core2 (Conroe) can load unaligned data just as quickly as aligned data...
; unless the unaligned data spans the border between 2 cachelines, in which
; case it's really slow. The exact numbers may differ, but all Intel cpus prior
; to Nehalem have a large penalty for cacheline splits.
; (8-byte alignment exactly half way between two cachelines is ok though.)
; LDDQU was supposed to fix this, but it only works on Pentium 4.
; So in the split case we load aligned data and explicitly perform the
; alignment between registers. Like on archs that have only aligned loads,
; except complicated by the fact that PALIGNR takes only an immediate, not
; a variable alignment.
; It is also possible to hoist the realignment to the macroblock level (keep
; 2 copies of the reference frame, offset by 32 bytes), but the extra memory
; needed for that method makes it often slower.

; sad 16x16 costs on Core2:
; good offsets: 49 cycles (50/64 of all mvs)
; cacheline split: 234 cycles (14/64 of all mvs. ammortized: +40 cycles)
; page split: 3600 cycles (14/4096 of all mvs. ammortized: +11.5 cycles)
; cache or page split with palignr: 57 cycles (ammortized: +2 cycles)

; computed jump assumes this loop is exactly 80 bytes
%macro SAD16_CACHELINE_LOOP_SSE2 1 ; alignment
ALIGN 16
sad_w16_align%1_sse2:
%if ARCH_X86_64
	movsxd r1q, r1d
	movsxd r3q, r3d
%endif
    movdqa  xmm1, [r2+16]
    movdqa  xmm2, [r2+r3+16]
    movdqa  xmm3, [r2]
    movdqa  xmm4, [r2+r3]
    pslldq  xmm1, 16-%1
    pslldq  xmm2, 16-%1
    psrldq  xmm3, %1
    psrldq  xmm4, %1
    por     xmm1, xmm3
    por     xmm2, xmm4
    psadbw  xmm1, [r0]
    psadbw  xmm2, [r0+r1]
    paddw   xmm0, xmm1
    paddw   xmm0, xmm2
    lea     r0,   [r0+2*r1]
    lea     r2,   [r2+2*r3]
    dec     r4
    jg sad_w16_align%1_sse2
    ret
%endmacro

; computed jump assumes this loop is exactly 64 bytes
%macro SAD16_CACHELINE_LOOP_SSSE3 1 ; alignment
ALIGN 16
sad_w16_align%1_ssse3:
%if ARCH_X86_64
	movsxd r1q, r1d
	movsxd r3q, r3d
%endif
    movdqa  xmm1, [r2+16]
    movdqa  xmm2, [r2+r3+16]
    palignr xmm1, [r2], %1
    palignr xmm2, [r2+r3], %1
    psadbw  xmm1, [r0]
    psadbw  xmm2, [r0+r1]
    paddw   xmm0, xmm1
    paddw   xmm0, xmm2
    lea     r0,   [r0+2*r1]
    lea     r2,   [r2+2*r3]
    dec     r4
    jg sad_w16_align%1_ssse3
    ret
%endmacro

%macro SAD16_CACHELINE_FUNC 2 ; cpu, height
cglobal pixel_sad_16x%2_cache64_%1
%if ARCH_X86_64
	movsxd r1q, r1d
	movsxd r3q, r3d
%endif
    mov     eax, r2m
    and     eax, 0x37
    cmp     eax, 0x30
    jle pixel_sad_16x%2_sse2
    PROLOGUE 4,6
    mov     r4d, r2d
    and     r4d, 15
%ifidn %1, ssse3
    shl     r4d, 6  ; code size = 64
%else
    lea     r4, [r4*5]
    shl     r4d, 4  ; code size = 80
%endif
%define sad_w16_addr (sad_w16_align1_%1 + (sad_w16_align1_%1 - sad_w16_align2_%1))
%ifdef PIC
    lea     r5, [sad_w16_addr]
    add     r5, r4
%else
    lea     r5, [sad_w16_addr + r4]
%endif
    and     r2, ~15
    mov     r4d, %2/2
    pxor    xmm0, xmm0
    call    r5
    movhlps xmm1, xmm0
    paddw   xmm0, xmm1
    movd    eax,  xmm0
    RET
%endmacro

%macro SAD_CACHELINE_START_MMX2 4 ; width, height, iterations, cacheline
%if ARCH_X86_64
	movsxd r1q, r1d
	movsxd r3q, r3d
%endif
    mov    eax, r2m
    and    eax, 0x17|%1|(%4>>1)
    cmp    eax, 0x10|%1|(%4>>1)
    jle pixel_sad_%1x%2_mmx2
    and    eax, 7
    shl    eax, 3
    movd   mm6, [sw_64]
    movd   mm7, eax
    psubw  mm6, mm7
    PROLOGUE 4,5
    and    r2, ~7
    mov    r4d, %3
    pxor   mm0, mm0
%endmacro

%macro SAD16_CACHELINE_FUNC_MMX2 2 ; height, cacheline
cglobal pixel_sad_16x%1_cache%2_mmx2
    SAD_CACHELINE_START_MMX2 16, %1, %1, %2
.loop:
    movq   mm1, [r2]
    movq   mm2, [r2+8]
    movq   mm3, [r2+16]
    movq   mm4, mm2
    psrlq  mm1, mm7
    psllq  mm2, mm6
    psllq  mm3, mm6
    psrlq  mm4, mm7
    por    mm1, mm2
    por    mm3, mm4
    psadbw mm1, [r0]
    psadbw mm3, [r0+8]
    paddw  mm0, mm1
    paddw  mm0, mm3
    add    r2, r3
    add    r0, r1
    dec    r4
    jg .loop
    movd   eax, mm0
    RET
%endmacro

%macro SAD8_CACHELINE_FUNC_MMX2 2 ; height, cacheline
cglobal pixel_sad_8x%1_cache%2_mmx2
    SAD_CACHELINE_START_MMX2 8, %1, %1/2, %2
.loop:
    movq   mm1, [r2+8]
    movq   mm2, [r2+r3+8]
    movq   mm3, [r2]
    movq   mm4, [r2+r3]
    psllq  mm1, mm6
    psllq  mm2, mm6
    psrlq  mm3, mm7
    psrlq  mm4, mm7
    por    mm1, mm3
    por    mm2, mm4
    psadbw mm1, [r0]
    psadbw mm2, [r0+r1]
    paddw  mm0, mm1
    paddw  mm0, mm2
    lea    r2, [r2+2*r3]
    lea    r0, [r0+2*r1]
    dec    r4
    jg .loop
    movd   eax, mm0
    RET
%endmacro


; instantiate the aligned sads

INIT_MMX
;%if ARCH_X86_64 == 0
SAD16_CACHELINE_FUNC_MMX2  8, 32
SAD16_CACHELINE_FUNC_MMX2 16, 32
SAD8_CACHELINE_FUNC_MMX2   4, 32
SAD8_CACHELINE_FUNC_MMX2   8, 32
SAD8_CACHELINE_FUNC_MMX2  16, 32
SAD16_CACHELINE_FUNC_MMX2  8, 64
SAD16_CACHELINE_FUNC_MMX2 16, 64
;%endif ; !ARCH_X86_64
SAD8_CACHELINE_FUNC_MMX2   4, 64
SAD8_CACHELINE_FUNC_MMX2   8, 64
SAD8_CACHELINE_FUNC_MMX2  16, 64

;%if ARCH_X86_64 == 0
SAD16_CACHELINE_FUNC sse2, 8
SAD16_CACHELINE_FUNC sse2, 16
%assign i 1
%rep 15
SAD16_CACHELINE_LOOP_SSE2 i
%assign i i+1
%endrep

SAD16_CACHELINE_FUNC ssse3, 8
SAD16_CACHELINE_FUNC ssse3, 16
%assign i 1
%rep 15
SAD16_CACHELINE_LOOP_SSSE3 i
%assign i i+1
%endrep
