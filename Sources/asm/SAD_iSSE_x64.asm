;/****************************************************************************
; * $Id$
; Author: Manao

; Addition: Sad4x2_iSSE, Sad16x8_iSSE, Sad16x2_iSSE, Sad8x2_iSSE, Sad8x1_iSSE, Sad2x2_iSSE, Sad2x4_iSSE, sad32x16
; (c) 2006-2007 Fizick
; swapped 2nd and 3rd parameter, added SadDummy by TSchniede

; See legal notice in Copying.txt for more information

; This program is free software; you can redistribute it and/or modify
; it under the terms of the GNU General Public License as published by
; the Free Software Foundation; either version 2 of the License, or
; (at your option) any later version.
;
; This program is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
; GNU General Public License for more details.
;
; You should have received a copy of the GNU General Public License
; along with this program; if not, write to the Free Software
; Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA, or visit
; http://www.gnu.org/copyleft/gpl.html .
; *
; ***************************************************************************/

BITS 64

%macro cglobal 1
	%ifndef NO_PREFIX
		global _%1
		%define %1 _%1
	%else
		global %1
	%endif
%endmacro

%ifdef __DEBUG
	%define __emms emms
%else
	%define __emms
%endif

;=============================================================================
; Read only data
;=============================================================================

SECTION .rodata align=16

ALIGN 16
mmx_one:
	times 4	dw 1

;=============================================================================
; Macros
;=============================================================================

%macro SAD16xY 1 ; height
ALIGN 16
Sad16x%1_iSSE:

    mov rax, rcx			; cur
    movsxd rcx, edx			; cpitch
;    mov r8, r8				; ref
    movsxd rdx, r9d			; dpitch

    pxor mm0, mm0
    pxor mm1, mm1

%if %1 == 1 ; height == 1
	movq mm2, [rax]
	movq mm3, [rax + 8]

	psadbw mm2, [r8]
	psadbw mm3, [r8 + 8]

	paddd mm0, mm2
	paddd mm1, mm3
%endif

%assign i 0 ; height > 1
%rep %1/2
	movq mm2, [rax]
	movq mm3, [rax + 8]
	movq mm4, [rax + rcx]
	movq mm5, [rax + rcx + 8]

	psadbw mm2, [r8]
	psadbw mm3, [r8 + 8]
	psadbw mm4, [r8 + rdx]
	psadbw mm5, [r8 + rdx + 8]
%assign i i+2
%if i < %1
	lea rax, [rax + 2 * rcx]
	lea r8, [r8 + 2 * rdx]
%endif
	paddd mm0, mm2
	paddd mm1, mm3
	paddd mm0, mm4
	paddd mm1, mm5
%endrep

	paddd  mm0, mm1
	movd   eax, mm0

	ret
%endmacro

%macro SAD32x1 0
	movq mm2, [rax]
	movq mm3, [rax + 8]
	movq mm4, [rax + 16]
	movq mm5, [rax + 24]

	psadbw mm2, [r8]
	psadbw mm3, [r8 + 8]
	psadbw mm4, [r8 + 16]
	psadbw mm5, [r8 + 24]

	add rax, rcx
	add r8, rdx

	paddd mm0, mm2
	paddd mm1, mm3
	paddd mm0, mm4
	paddd mm1, mm5
%endmacro

;=============================================================================
; Code
;=============================================================================

SECTION .text

cglobal Sad32x32_iSSE
cglobal Sad16x32_iSSE
cglobal Sad32x16_iSSE
cglobal Sad16x16_iSSE
cglobal Sad16x8_iSSE
cglobal Sad16x2_iSSE
cglobal Sad16x1_iSSE
cglobal Sad8x8_iSSE
cglobal Sad8x16_iSSE
cglobal Sad8x4_iSSE
cglobal Sad4x8_iSSE
cglobal Sad4x4_iSSE
cglobal Sad4x2_iSSE
cglobal Sad8x2_iSSE
cglobal Sad8x1_iSSE
cglobal Sad2x2_iSSE
cglobal Sad2x4_iSSE
cglobal Sad2x2_iSSE_T	; special custom SAD functions, use the fact, that a packed aligned copy of source block is created AND minimizes slow register copies
cglobal Sad2x4_iSSE_T	;
; cglobal Sad2x2_iSSE_O


cglobal SadDummy

; "NOP"
ALIGN 16
SadDummy:
	xor eax, eax	; return 0
	ret

SAD16xY 32
SAD16xY 16
SAD16xY 8
SAD16xY 2
SAD16xY 1



;-----------------------------------------------------------------------------
;
; unsigned int Sad8x16_iSSE(unsigned char *pCur, int nCurPitch, const unsigned char *pRef,
;				  int nRefPitch); 8 bytes, 16 lines - added by Fizick
;
;-----------------------------------------------------------------------------

ALIGN 16

Sad8x16_iSSE:
		mov r11, rcx		; cur
		movsxd rdx, edx		; curpitch
		mov r10, r8			; ref
		movsxd rcx, r9d		; refpitch

		pxor mm4, mm4 ; zero

        movq mm0, [r10]				; 0 ->
		psadbw mm0, [r11]
        movq mm1, [r10 + rcx]       ; 1 ->
		psadbw mm1, [r11 + rdx]
        movq mm2, [r10 + 2 * rcx]   ; 2 ->
		psadbw mm2, [r11 + 2 *rdx]
        movq mm3, [r10 + 4 * rcx]   ; 4 ->
        psadbw mm3, [r11 + 4 * rdx]

		paddd mm0, mm1
		paddd mm2, mm3

		paddd mm4, mm0
		paddd mm4, mm2

        lea r10, [r10 + rcx * 2]    ; 2
        lea r11, [r11 + rdx * 2]    ; 2

        movq mm0, [r10 + rcx]       ; 2 + 1 ->
        psadbw mm0, [r11 + rdx]
        movq mm1, [r10 + 4 * rcx]   ; 2 + 4 ->
        psadbw mm1, [r11 + 4 * rdx]

        lea r10, [r10 + rcx]        ; 3
        lea r11, [r11 + rdx]        ; 3

        movq mm2, [r10 + 2 * rcx]   ; 3 + 2 ->
        psadbw mm2, [r11 + 2 * rdx]
        movq mm3, [r10 + 4 * rcx]   ; 3 + 4 ->
        psadbw mm3, [r11 + 4 * rdx]

		paddd mm0, mm1
		paddd mm2, mm3

		paddd mm4, mm0
		paddd mm4, mm2

; pointer to next 8 lines

        lea r10, [r10 + rcx *4]        ; 7
        lea r11, [r11 + rdx *4]        ; 7

        lea r10, [r10 + rcx]        ; 8
        lea r11, [r11 + rdx]        ; 8

; next 8 lines

        movq mm0, [r10]				; 0 ->
		psadbw mm0, [r11]
        movq mm1, [r10 + rcx]       ; 1 ->
		psadbw mm1, [r11 + rdx]
        movq mm2, [r10 + 2 * rcx]   ; 2 ->
		psadbw mm2, [r11 + 2 *rdx]
        movq mm3, [r10 + 4 * rcx]   ; 4 ->
        psadbw mm3, [r11 + 4 * rdx]

		paddd mm0, mm1
		paddd mm2, mm3

		paddd mm4, mm0
		paddd mm4, mm2

        lea r10, [r10 + rcx * 2]    ; 2
        lea r11, [r11 + rdx * 2]    ; 2

        movq mm0, [r10 + rcx]       ; 2 + 1 ->
        psadbw mm0, [r11 + rdx]
        movq mm1, [r10 + 4 * rcx]   ; 2 + 4 ->
        psadbw mm1, [r11 + 4 * rdx]

        lea r10, [r10 + rcx]        ; 3
        lea r11, [r11 + rdx]        ; 3

        movq mm2, [r10 + 2 * rcx]   ; 3 + 2 ->
        psadbw mm2, [r11 + 2 * rdx]
        movq mm3, [r10 + 4 * rcx]   ; 3 + 4 ->
        psadbw mm3, [r11 + 4 * rdx]

		paddd mm0, mm1
		paddd mm2, mm3

		paddd mm4, mm0
		paddd mm4, mm2

		movd eax, mm4


        ret

;-----------------------------------------------------------------------------
;
; unsigned int Sad8x8_iSSE(const unsigned char * cur,
;						   const unsigned int cpitch,
;						   const unsigned char * ref,
;						   const unsigned int rpitch);
;
;-----------------------------------------------------------------------------

ALIGN 16
Sad8x8_iSSE:

    mov rax, rcx			; cur
    movsxd rcx, edx			; cpitch
;    mov r8, r8				; ref
    movsxd rdx, r9d			; dpitch

    lea	r10, [rcx+2*rcx]    ; 3*cpitch
    lea r11, [rdx+2*rdx]	; 3*dpitch

 	movq   mm0, [rax]		;0
	psadbw mm0, [r8]
	movq   mm1, [rax+rcx]	;1
	psadbw mm1, [r8+rdx]

	movq   mm2, [rax+2*rcx]	;2
	psadbw mm2, [r8+2*rdx]
	movq   mm3, [rax+r10]	;3
	psadbw mm3, [r8+r11]

	paddd  mm0, mm1

	movq   mm4, [rax+4*rcx]	;4
	psadbw mm4, [r8+4*rdx]
	movq   mm5, [rax+2*r10]	;6
	psadbw mm5, [r8+2*r11]

	paddd  mm2,	mm3
	paddd  mm0,	mm2

	lea r10, [r10+2*rcx]	; 5*cpitch
	lea r11, [r11+2*rdx]	; 5*dpitch

	lea rcx, [r10+2*rcx]	; 7*cpitch
	lea rdx, [r11+2*rdx]	; 7*cpitch

	movq   mm6, [rax+r10]	;5
	psadbw mm6, [r8+r11]
	movq   mm7, [rax+rcx]	;7
	psadbw mm7, [r8+rdx]

	paddd  mm4, mm5
	paddd  mm6, mm7
	paddd  mm0, mm4

	paddd  mm0, mm6
	movd   eax, mm0


	ret


;-----------------------------------------------------------------------------
;
; unsigned int SAD8x4_iSSE(const unsigned char * cur,
;						   const unsigned int cpitch,
;						   const unsigned char * ref,
;						   const unsigned int rpitch);
;
;-----------------------------------------------------------------------------

ALIGN 16
Sad8x4_iSSE:
    mov rax, rcx			; cur
    movsxd rcx, edx			; cpitch
;   mov r8, r8				; ref
    movsxd rdx, r9d			; dpitch
    lea	r10, [rcx+2*rcx]    ; 3*cpitch
    lea r11, [rdx+2*rdx]	; 3*dpitch

	movq   mm0, [rax]		;0
	psadbw mm0, [r8]
	movq   mm1, [rax+rcx]	;1
	psadbw mm1, [r8+rdx]

	movq   mm2, [rax+2*rcx]	;2
	psadbw mm2, [r8+2*rdx]
	movq   mm3, [rax+r10]	;3
	psadbw mm3, [r8+r11]

	paddd  mm0, mm1

	paddd  mm2,	mm3
	paddd  mm0,	mm2

	movd   eax, mm0

	ret

;-----------------------------------------------------------------------------
;
; unsigned int Sad4x8_iSSE(const unsigned char * cur,
;						   const unsigned int cpitch,
;						   const unsigned char * ref,
;						   const unsigned int rpitch);
;
;-----------------------------------------------------------------------------

ALIGN 16
Sad4x8_iSSE:
	pxor mm0, mm0
	pxor mm1, mm1

    mov rax, rcx			; cur
    movsxd rcx, edx			; cpitch
;   mov r8, r8				; ref
    movsxd rdx, r9d			; dpitch

	pxor mm2, mm2
	pxor mm3, mm3

    lea	r10, [rcx+2*rcx]    ; 3*cpitch
    lea r11, [rdx+2*rdx]	; 3*dpitch

	movd   mm0, [rax]		;0
	movd   mm2, [r8]
	movd   mm1, [rax+rcx]   ;1
	movd   mm3, [r8+rdx]

	pxor   mm4, mm4
	pxor   mm5, mm5
	pxor   mm6, mm6
	pxor   mm7, mm7

	psadbw mm0, mm2			;0
	psadbw mm1, mm3			;1

	movd   mm4, [rax+2*rcx]	;2
	movd   mm5, [r8+2*rdx]
	movd   mm6, [rax+r10]	;3
	movd   mm7, [r8+r11]

	pxor   mm2, mm2
	pxor   mm3, mm3

	psadbw mm4, mm5			;2
	psadbw mm6, mm7			;3

	pxor   mm5, mm5
	pxor   mm7, mm7

	paddd  mm0, mm1			;0+1
	paddd  mm4, mm6			;2+3

	pxor   mm1, mm1
	pxor   mm6, mm6

	movd   mm3, [rax+4*rcx]	;4
	movd   mm2, [r8+4*rdx]

	movd   mm7, [rax+2*r10]	;6
	movd   mm5, [r8+2*r11]

	paddd  mm0, mm4			;0+1+2+3

	psadbw mm2, mm3			;4
	psadbw mm5, mm7			;6

	lea r10, [r10+2*rcx]	; 5*cpitch
	lea r11, [r11+2*rdx]	; 5*dpitch

	paddd  mm2, mm5			;4+6

	lea rcx, [r10+2*rcx]	; 7*cpitch
	lea rdx, [r11+2*rdx]	; 7*cpitch

	pxor   mm5, mm5
	pxor   mm4, mm4

	movd   mm6, [rax+r10]	;5
	movd   mm4, [r8+r11]
	movd   mm5, [rax+rcx]	;7
	movd   mm1, [r8+rdx]

	psadbw mm6, mm4			;5
	psadbw mm5, mm1			;7

	paddd  mm0, mm2			;0+1+2+3+4+6
	paddd  mm5, mm6			;5+7

	paddd  mm0, mm5			;0+1+2+3+4+5+6+7
	movd   eax, mm0

	ret

;-----------------------------------------------------------------------------
;
; unsigned int Sad4x4_iSSE(const unsigned char * cur,
;						   const unsigned int cpitch,
;						   const unsigned char * ref,
;						   const unsigned int rpitch);
;
;-----------------------------------------------------------------------------

ALIGN 16
Sad4x4_iSSE:
	;pxor mm0, mm0
	;pxor mm1, mm1

    mov rax, rcx			; cur
    movsxd rcx, edx			; cpitch
;   mov r8, r8				; ref
    movsxd rdx, r9d			; dpitch

    ;pxor mm2, mm2
    ;pxor mm3, mm3

    lea	r10, [rcx+2*rcx]    ; 3*cpitch
    lea r11, [rdx+2*rdx]	; 3*dpitch

	movd   mm0, [rax]		;0
	movd   mm2, [r8]
	movd   mm1, [rax+rcx]   ;1
	movd   mm3, [r8+rdx]

	;pxor   mm4, mm4
	;pxor   mm5, mm5
	;pxor   mm6, mm6
	;pxor   mm7, mm7

	psadbw mm0, mm2			;0
	psadbw mm1, mm3			;1

	movd   mm4, [rax+2*rcx]	;2
	movd   mm5, [r8+2*rdx]
	movd   mm6, [rax+r10]	;3
	movd   mm7, [r8+r11]

	psadbw mm4, mm5			;2
	psadbw mm6, mm7			;3

	paddd  mm0, mm1
	paddd  mm4, mm6

	paddd  mm0, mm4
	movd   eax, mm0

	ret

;-----------------------------------------------------------------------------
; Added by Fizick:

;-----------------------------------------------------------------------------
;
; unsigned int Sad4x2_iSSE(const unsigned char * cur,
;						   const unsigned int cpitch,
;						   const unsigned char * ref,
;						   const unsigned int rpitch);
;
;-----------------------------------------------------------------------------

ALIGN 16
Sad4x2_iSSE:
    mov rax, rcx			; cur
    movsxd rcx, edx			; cpitch
;   mov r8, r8				; ref
    movsxd rdx, r9d			; dpitch

	movd   mm0, [rax]		;0
	movd   mm2, [r8]
	movd   mm1, [rax+rcx]   ;1
	movd   mm3, [r8+rdx]

    PUNPCKLDQ mm0, mm1 ; combine both low 4 bytes to one 8 byte register
    PUNPCKLDQ mm2, mm3

	psadbw mm0, mm2

	movd   eax, mm0

	ret



;-----------------------------------------------------------------------------
;
; unsigned int SAD8x2_iSSE(const unsigned char * cur,
;						   const unsigned int cpitch,
;						   const unsigned char * ref,
;						   const unsigned int rpitch);
;
;-----------------------------------------------------------------------------

ALIGN 16
Sad8x2_iSSE:
    mov rax, rcx			; cur
    movsxd rcx, edx			; cpitch
;   mov r8, r8				; ref
    movsxd rdx, r9d			; dpitch

	movq   mm0, [rax]		;0
	psadbw mm0, [r8]
	movq   mm1, [rax+rcx]	;1
	psadbw mm1, [r8+rdx]

	paddd  mm0, mm1

	movd   eax, mm0

	ret

;-----------------------------------------------------------------------------
;
; unsigned int SAD8x1_iSSE(const unsigned char * cur,
;						   const unsigned int cpitch,
;						   const unsigned char * ref,
;						   const unsigned int rpitch);
;
;-----------------------------------------------------------------------------

ALIGN 16
Sad8x1_iSSE:
    mov rax, rcx			; cur
    mov rdx, r8				; ref

	movq   mm0, [rax]		;0
	psadbw mm0, [rdx]

	movd   eax, mm0

	ret

;-----------------------------------------------------------------------------
;
; unsigned int Sad2x2_iSSE(const unsigned char * cur,
;						   const unsigned int cpitch,
;						   const unsigned char * ref,
;						   const unsigned int rpitch);
;
;-----------------------------------------------------------------------------
; the read in actually reverses memory alignment of the two lines
; replaceed ebx with edx => prevents 1x push&pop, replaced one sement register with regular one (faster)
ALIGN 16
Sad2x2_iSSE:
    mov rax, rcx			; cur
    movsxd rcx, edx			; cpitch
    mov edx, [rax] ; 2 source bytes
    shl edx, 16;  move to hi word of ebx
    mov dx, word [rax+rcx];  put 2 bytes (next line) to low word, got all 4 src bytes in ebx
    movd mm0, edx;

    mov rax, r8				; ref
    movsxd rcx, r9d			; dpitch
    mov edx, [rax]; 2 source bytes
    shl edx, 16; // move to hi word of ebx
    mov dx, word [rax+rcx]; // put 2 bytes (next line) to low word, got all 4 src bytes in ebx
    movd mm1, edx;

	psadbw mm0, mm1

	movd   eax, mm0

	ret

;ALIGN 16				; the original version (MVAnalyse all)~3% slower than slightly optimized
;Sad2x2_iSSE_O:
;	mov rax, [esp+4+ 4]		; cur
;	mov rcx, [esp+4+ 8]		; cpitch
;	mov bx, word [rax] ; 2 source bytes
;	shl ebx, 16;  move to hi word of ebx
;	mov bx, word [rax+rcx];  put 2 bytes (next line) to low word, got all 4 src bytes in ebx
;	movd mm0, ebx;

;	mov rax, [esp+4+12]		; ref
;	mov rcx, [esp+4+16]		; dpitch
;	mov bx, word [rax]; 2 source bytes
;	shl ebx, 16; // move to hi word of ebx
;	mov bx, word [rax+rcx]; // put 2 bytes (next line) to low word, got all 4 src bytes in ebx
;	movd mm1, ebx;

;	psadbw mm0, mm1
;	movd   eax, mm0
;	ret


ALIGN 16
Sad2x2_iSSE_T:	; whole MVAnalyse ~3% faster than with Sad2x2_iSSE
    mov rax, rcx		; cur

	movd mm0, [rax]		;as source is now aligned AND linear packed only one copy needed

    mov rax, r8			; ref
    movsxd rcx, r9d		; dpitch

	psllq mm0, 32		; move to left  position (so we only need to shift left below)

	movd mm1, [rax]
	punpcklwd mm1, [rax+rcx]
	psllq mm1, 32		; move to left  position and delete rubbish

	psadbw mm0, mm1
	movd   eax, mm0

	ret

ALIGN 16
Sad2x4_iSSE_T:
    mov  rax, rcx			; cur
	movq mm0, [rax]				; as source is now aligned AND linear packed only one copy needed

    mov rax, r8				; ref
    movsxd rcx, r9d			; dpitch
    lea rdx, [rcx+rcx*2]

	movd mm1, [rax]
	movd mm2, [rax+rcx]
	movd mm3, [rax+rcx*2]
	movd mm4, [rax+rdx]

	punpcklwd mm1, mm2	; adds the 2 bytes of source after dest
	punpcklwd mm3, mm4	; adds the 2 bytes of source after dest
	punpckldq mm1, mm3	; adds the 4 bytes of source after dest, removes 4byte rubbish from mm1&mm3

	psadbw mm0, mm1
	movd   eax, mm0

	ret

;-----------------------------------------------------------------------------
;
; unsigned int Sad2x4_iSSE(const unsigned char * cur,
;						   const unsigned int cpitch,
;						   const unsigned char * ref,
;						   const unsigned int rpitch);
;
;-----------------------------------------------------------------------------

ALIGN 16
Sad2x4_iSSE:
    mov rax, rcx		; cur
    movsxd rcx, edx		; cpitch
    mov edx, [rax] ; 2 source bytes
    shl edx, 16;  move to hi word of ebx
    mov dx, word [rax+rcx];  put 2 bytes (next line) to low word, got all 4 src bytes in ebx
    movd mm0, edx; // store

    add rax, rcx ;
    add rax, rcx ; to next 2 lines
    mov edx, [rax] ; 2 source bytes
    shl edx, 16;  move to hi word of ebx
    mov dx, word [rax+rcx];  put 2 bytes (next line) to low word, got all 4 src bytes in ebx
    movd mm1, edx;

    PUNPCKLDQ mm0, mm1 ; combine both low 4 bytes to one 8 byte register

    mov rax, r8				; ref
    movsxd rcx, r9d			; dpitch
    mov edx, [rax]; 2 ref bytes
    shl edx, 16; // move to hi word of ebx
    mov dx, word [rax+rcx]; // put 2 bytes (next line) to low word, got all 4 ref bytes in ebx
    movd mm2, edx; store

    add rax, rcx ;
    add rax, rcx ; to next 2 lines
    mov edx, [rax] ; 2 ref bytes
    shl edx, 16;  move to hi word of ebx
    mov dx, word [rax+rcx];  put 2 bytes (next line) to low word, got all 4 ref bytes in ebx
    movd mm3, edx;

    PUNPCKLDQ mm2, mm3 ; combine both low 4 bytes to one 8 byte register

	psadbw mm0, mm2

	movd   eax, mm0


	ret


;-----------------------------------------------------------------------------
;
; unsigned int Sad32x16_iSSE(const unsigned char * cur,
;							 const unsigned int cpitch,
;							 const unsigned char * ref,
;							 const unsigned int rpitch);
;
;-----------------------------------------------------------------------------

ALIGN 16
Sad32x16_iSSE:

    mov rax, rcx			; cur
    movsxd rcx, edx			; cpitch
;   mov r8, r8				; ref
    movsxd rdx, r9d			; dpitch

    pxor mm0, mm0
    pxor mm1, mm1

    SAD32x1
    SAD32x1
    SAD32x1
    SAD32x1
    SAD32x1
    SAD32x1
    SAD32x1
    SAD32x1

    SAD32x1
    SAD32x1
    SAD32x1
    SAD32x1
    SAD32x1
    SAD32x1
    SAD32x1
    SAD32x1

	paddd  mm0, mm1
	movd   eax, mm0

	ret

;-----------------------------------------------------------------------------
;
; unsigned int Sad32x32_iSSE(const unsigned char * cur,
;							 const unsigned int cpitch,
;							 const unsigned char * ref,
;							 const unsigned int rpitch);
;
;-----------------------------------------------------------------------------

ALIGN 16
Sad32x32_iSSE:

    mov rax, rcx			; cur
    movsxd rcx, edx			; cpitch
;   mov r8, r8				; ref
    movsxd rdx, r9d			; dpitch

    pxor mm0, mm0
    pxor mm1, mm1

    SAD32x1
    SAD32x1
    SAD32x1
    SAD32x1
    SAD32x1
    SAD32x1
    SAD32x1
    SAD32x1

    SAD32x1
    SAD32x1
    SAD32x1
    SAD32x1
    SAD32x1
    SAD32x1
    SAD32x1
    SAD32x1

    SAD32x1
    SAD32x1
    SAD32x1
    SAD32x1
    SAD32x1
    SAD32x1
    SAD32x1
    SAD32x1

    SAD32x1
    SAD32x1
    SAD32x1
    SAD32x1
    SAD32x1
    SAD32x1
    SAD32x1
    SAD32x1

	paddd  mm0, mm1
	movd   eax, mm0

	ret

