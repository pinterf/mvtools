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

BITS 32

%macro cglobal 1
	%ifdef PREFIX
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
    push ebx

    mov eax, [esp+4+ 4]		; cur
    mov ecx, [esp+4+ 8]		; cpitch
    mov ebx, [esp+4+12]		; ref
    mov edx, [esp+4+16]		; dpitch

    pxor mm0, mm0
    pxor mm1, mm1

%if %1 == 1 ; height == 1
	movq mm2, [eax]
	movq mm3, [eax + 8]

	psadbw mm2, [ebx]
	psadbw mm3, [ebx + 8]

	paddd mm0, mm2
	paddd mm1, mm3
%endif

%assign i 0 ; height > 1
%rep %1/2
	movq mm2, [eax]
	movq mm3, [eax + 8]
	movq mm4, [eax + ecx]
	movq mm5, [eax + ecx + 8]

	psadbw mm2, [ebx]
	psadbw mm3, [ebx + 8]
	psadbw mm4, [ebx + edx]
	psadbw mm5, [ebx + edx + 8]
%assign i i+2
%if i < %1
	lea eax, [eax + 2 * ecx]
	lea ebx, [ebx + 2 * edx]
%endif
	paddd mm0, mm2
	paddd mm1, mm3
	paddd mm0, mm4
	paddd mm1, mm5
%endrep

	paddd  mm0, mm1
	movd   eax, mm0

	pop ebx

	ret
%endmacro

%macro SAD32x1 0
	movq mm2, [eax]
	movq mm3, [eax + 8]
	movq mm4, [eax + 16]
	movq mm5, [eax + 24]

	psadbw mm2, [ebx]
	psadbw mm3, [ebx + 8]
	psadbw mm4, [ebx + 16]
	psadbw mm5, [ebx + 24]

	add eax, ecx
	add ebx, edx

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
		push edi
		push esi

		mov edi, [esp + 12] ; cur
		mov edx, [esp + 16] ; curpitch
		mov esi, [esp + 20] ; ref
		mov ecx, [esp + 24] ; refpitch

		pxor mm4, mm4 ; zero

        movq mm0, [esi]				; 0 ->
		psadbw mm0, [edi]
        movq mm1, [esi + ecx]       ; 1 ->
		psadbw mm1, [edi + edx]
        movq mm2, [esi + 2 * ecx]   ; 2 ->
		psadbw mm2, [edi + 2 *edx]
        movq mm3, [esi + 4 * ecx]   ; 4 ->
        psadbw mm3, [edi + 4 * edx]

		paddd mm0, mm1
		paddd mm2, mm3

		paddd mm4, mm0
		paddd mm4, mm2

        lea esi, [esi + ecx * 2]    ; 2
        lea edi, [edi + edx * 2]    ; 2

        movq mm0, [esi + ecx]       ; 2 + 1 ->
        psadbw mm0, [edi + edx]
        movq mm1, [esi + 4 * ecx]   ; 2 + 4 ->
        psadbw mm1, [edi + 4 * edx]

        lea esi, [esi + ecx]        ; 3
        lea edi, [edi + edx]        ; 3

        movq mm2, [esi + 2 * ecx]   ; 3 + 2 ->
        psadbw mm2, [edi + 2 * edx]
        movq mm3, [esi + 4 * ecx]   ; 3 + 4 ->
        psadbw mm3, [edi + 4 * edx]

		paddd mm0, mm1
		paddd mm2, mm3

		paddd mm4, mm0
		paddd mm4, mm2

; pointer to next 8 lines

        lea esi, [esi + ecx *4]        ; 7
        lea edi, [edi + edx *4]        ; 7

        lea esi, [esi + ecx]        ; 8
        lea edi, [edi + edx]        ; 8

; next 8 lines

        movq mm0, [esi]				; 0 ->
		psadbw mm0, [edi]
        movq mm1, [esi + ecx]       ; 1 ->
		psadbw mm1, [edi + edx]
        movq mm2, [esi + 2 * ecx]   ; 2 ->
		psadbw mm2, [edi + 2 *edx]
        movq mm3, [esi + 4 * ecx]   ; 4 ->
        psadbw mm3, [edi + 4 * edx]

		paddd mm0, mm1
		paddd mm2, mm3

		paddd mm4, mm0
		paddd mm4, mm2

        lea esi, [esi + ecx * 2]    ; 2
        lea edi, [edi + edx * 2]    ; 2

        movq mm0, [esi + ecx]       ; 2 + 1 ->
        psadbw mm0, [edi + edx]
        movq mm1, [esi + 4 * ecx]   ; 2 + 4 ->
        psadbw mm1, [edi + 4 * edx]

        lea esi, [esi + ecx]        ; 3
        lea edi, [edi + edx]        ; 3

        movq mm2, [esi + 2 * ecx]   ; 3 + 2 ->
        psadbw mm2, [edi + 2 * edx]
        movq mm3, [esi + 4 * ecx]   ; 3 + 4 ->
        psadbw mm3, [edi + 4 * edx]

		paddd mm0, mm1
		paddd mm2, mm3

		paddd mm4, mm0
		paddd mm4, mm2

		movd eax, mm4

        pop esi
        pop edi

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

    push ebx

    mov eax, [esp+4+ 4]		; cur
    mov ecx, [esp+4+ 8]		; cpitch
    mov ebx, [esp+4+12]		; ref
    mov edx, [esp+4+16]		; dpitch

    push esi
    push edi

    lea	esi, [ecx+2*ecx]    ; 3*cpitch
    lea edi, [edx+2*edx]	; 3*dpitch

 	movq   mm0, [eax]		;0
	psadbw mm0, [ebx]
	movq   mm1, [eax+ecx]	;1
	psadbw mm1, [ebx+edx]

	movq   mm2, [eax+2*ecx]	;2
	psadbw mm2, [ebx+2*edx]
	movq   mm3, [eax+esi]	;3
	psadbw mm3, [ebx+edi]

	paddd  mm0, mm1

	movq   mm4, [eax+4*ecx]	;4
	psadbw mm4, [ebx+4*edx]
	movq   mm5, [eax+2*esi]	;6
	psadbw mm5, [ebx+2*edi]

	paddd  mm2,	mm3
	paddd  mm0,	mm2

	lea esi, [esi+2*ecx]	; 5*cpitch
	lea edi, [edi+2*edx]	; 5*dpitch

	lea ecx, [esi+2*ecx]	; 7*cpitch
	lea edx, [edi+2*edx]	; 7*cpitch

	movq   mm6, [eax+esi]	;5
	psadbw mm6, [ebx+edi]
	movq   mm7, [eax+ecx]	;7
	psadbw mm7, [ebx+edx]

	paddd  mm4, mm5
	paddd  mm6, mm7
	paddd  mm0, mm4

	paddd  mm0, mm6
	movd   eax, mm0

	pop edi
	pop esi
	pop ebx


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
	push ebx
    mov eax, [esp+4+ 4]		; cur
    mov ecx, [esp+4+ 8]		; cpitch
    mov ebx, [esp+4+12]		; ref
    mov edx, [esp+4+16]		; dpitch
    push esi
    push edi

    lea	esi, [ecx+2*ecx]    ; 3*cpitch
    lea edi, [edx+2*edx]	; 3*dpitch

	movq   mm0, [eax]		;0
	psadbw mm0, [ebx]
	movq   mm1, [eax+ecx]	;1
	psadbw mm1, [ebx+edx]

	movq   mm2, [eax+2*ecx]	;2
	psadbw mm2, [ebx+2*edx]
	movq   mm3, [eax+esi]	;3
	psadbw mm3, [ebx+edi]

	paddd  mm0, mm1

	paddd  mm2,	mm3
	paddd  mm0,	mm2

	pop edi
	pop esi
	pop ebx

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
	push ebx

	pxor mm0, mm0
	pxor mm1, mm1

    mov eax, [esp+4+ 4]		; cur
    mov ecx, [esp+4+ 8]		; cpitch
    mov ebx, [esp+4+12]		; ref
    mov edx, [esp+4+16]		; dpitch

	pxor mm2, mm2
	pxor mm3, mm3

    push esi
    push edi

    lea	esi, [ecx+2*ecx]    ; 3*cpitch
    lea edi, [edx+2*edx]	; 3*dpitch

	movd   mm0, [eax]		;0
	movd   mm2, [ebx]
	movd   mm1, [eax+ecx]   ;1
	movd   mm3, [ebx+edx]

	pxor   mm4, mm4
	pxor   mm5, mm5
	pxor   mm6, mm6
	pxor   mm7, mm7

	psadbw mm0, mm2			;0
	psadbw mm1, mm3			;1

	movd   mm4, [eax+2*ecx]	;2
	movd   mm5, [ebx+2*edx]
	movd   mm6, [eax+esi]	;3
	movd   mm7, [ebx+edi]

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

	movd   mm3, [eax+4*ecx]	;4
	movd   mm2, [ebx+4*edx]

	movd   mm7, [eax+2*esi]	;6
	movd   mm5, [ebx+2*edi]

	paddd  mm0, mm4			;0+1+2+3

	psadbw mm2, mm3			;4
	psadbw mm5, mm7			;6

	lea esi, [esi+2*ecx]	; 5*cpitch
	lea edi, [edi+2*edx]	; 5*dpitch

	paddd  mm2, mm5			;4+6

	lea ecx, [esi+2*ecx]	; 7*cpitch
	lea edx, [edi+2*edx]	; 7*cpitch

	pxor   mm5, mm5
	pxor   mm4, mm4

	movd   mm6, [eax+esi]	;5
	movd   mm4, [ebx+edi]
	movd   mm5, [eax+ecx]	;7
	movd   mm1, [ebx+edx]

	psadbw mm6, mm4			;5
	psadbw mm5, mm1			;7

	paddd  mm0, mm2			;0+1+2+3+4+6
	paddd  mm5, mm6			;5+7

	pop edi
	pop esi
	pop ebx

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
	push ebx

	;pxor mm0, mm0
	;pxor mm1, mm1

    mov eax, [esp+4+ 4]		; cur
    mov ecx, [esp+4+ 8]		; cpitch
    mov ebx, [esp+4+12]		; ref
    mov edx, [esp+4+16]		; dpitch

    ;pxor mm2, mm2
    ;pxor mm3, mm3

    push esi
    push edi

    lea	esi, [ecx+2*ecx]    ; 3*cpitch
    lea edi, [edx+2*edx]	; 3*dpitch

	movd   mm0, [eax]		;0
	movd   mm2, [ebx]
	movd   mm1, [eax+ecx]   ;1
	movd   mm3, [ebx+edx]

	;pxor   mm4, mm4
	;pxor   mm5, mm5
	;pxor   mm6, mm6
	;pxor   mm7, mm7

	psadbw mm0, mm2			;0
	psadbw mm1, mm3			;1

	movd   mm4, [eax+2*ecx]	;2
	movd   mm5, [ebx+2*edx]
	movd   mm6, [eax+esi]	;3
	movd   mm7, [ebx+edi]

	psadbw mm4, mm5			;2
	psadbw mm6, mm7			;3

	paddd  mm0, mm1
	paddd  mm4, mm6

	pop edi
	pop esi
	pop ebx

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
	push ebx

    mov eax, [esp+4+ 4]		; cur
    mov ecx, [esp+4+ 8]		; cpitch
    mov ebx, [esp+4+12]		; ref
    mov edx, [esp+4+16]		; dpitch

	movd   mm0, [eax]		;0
	movd   mm2, [ebx]
	movd   mm1, [eax+ecx]   ;1
	movd   mm3, [ebx+edx]

    PUNPCKLDQ mm0, mm1 ; combine both low 4 bytes to one 8 byte register
    PUNPCKLDQ mm2, mm3

	psadbw mm0, mm2

	movd   eax, mm0

	pop ebx

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
	push ebx
    mov eax, [esp+4+ 4]		; cur
    mov ecx, [esp+4+ 8]		; cpitch
    mov ebx, [esp+4+12]		; ref
    mov edx, [esp+4+16]		; dpitch

	movq   mm0, [eax]		;0
	psadbw mm0, [ebx]
	movq   mm1, [eax+ecx]	;1
	psadbw mm1, [ebx+edx]

	paddd  mm0, mm1

	pop ebx

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
	push ebx
    mov eax, [esp+4+ 4]		; cur
    mov ebx, [esp+4+12]		; ref

	movq   mm0, [eax]		;0
	psadbw mm0, [ebx]

	pop ebx

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
    mov eax, [esp+0+ 4]		; cur
    mov ecx, [esp+0+ 8]		; cpitch
    mov edx, [eax] ; 2 source bytes
    shl edx, 16;  move to hi word of ebx
    mov dx, word [eax+ecx];  put 2 bytes (next line) to low word, got all 4 src bytes in ebx
    movd mm0, edx;

    mov eax, [esp+0+12]		; ref
    mov ecx, [esp+0+16]		; dpitch
    mov edx, [eax]; 2 source bytes
    shl edx, 16; // move to hi word of ebx
    mov dx, word [eax+ecx]; // put 2 bytes (next line) to low word, got all 4 src bytes in ebx
    movd mm1, edx;

	psadbw mm0, mm1

	movd   eax, mm0

	ret

;ALIGN 16				; the original version (MVAnalyse all)~3% slower than slightly optimized
;Sad2x2_iSSE_O:
;	push ebx
;	mov eax, [esp+4+ 4]		; cur
;	mov ecx, [esp+4+ 8]		; cpitch
;	mov bx, word [eax] ; 2 source bytes
;	shl ebx, 16;  move to hi word of ebx
;	mov bx, word [eax+ecx];  put 2 bytes (next line) to low word, got all 4 src bytes in ebx
;	movd mm0, ebx;

;	mov eax, [esp+4+12]		; ref
;	mov ecx, [esp+4+16]		; dpitch
;	mov bx, word [eax]; 2 source bytes
;	shl ebx, 16; // move to hi word of ebx
;	mov bx, word [eax+ecx]; // put 2 bytes (next line) to low word, got all 4 src bytes in ebx
;	movd mm1, ebx;

;	psadbw mm0, mm1
;	movd   eax, mm0
;	pop ebx
;	ret


ALIGN 16
Sad2x2_iSSE_T:	; whole MVAnalyse ~3% faster than with Sad2x2_iSSE
    mov eax, [esp+0+ 4]		; cur

	movd mm0, [eax]		;as source is now aligned AND linear packed only one copy needed

    mov eax, [esp+0+12]		; ref
    mov ecx, [esp+0+16]		; dpitch

	psllq mm0, 32		; move to left  position (so we only need to shift left below)

	movd mm1, [eax]
	punpcklwd mm1, [eax+ecx]
	psllq mm1, 32		; move to left  position and delete rubbish

	psadbw mm0, mm1
	movd   eax, mm0

	ret

ALIGN 16
Sad2x4_iSSE_T:
    mov  eax, [esp+0+ 4]		; cur
	movq mm0, [eax]				; as source is now aligned AND linear packed only one copy needed

    mov eax, [esp+0+12]		; ref
    mov ecx, [esp+0+16]		; dpitch
    lea edx, [ecx+ecx*2]

	movd mm1, [eax]
	movd mm2, [eax+ecx]
	movd mm3, [eax+ecx*2]
	movd mm4, [eax+edx]

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
    mov eax, [esp+0+ 4]		; cur
    mov ecx, [esp+0+ 8]		; cpitch
    mov edx, [eax] ; 2 source bytes
    shl edx, 16;  move to hi word of ebx
    mov dx, word [eax+ecx];  put 2 bytes (next line) to low word, got all 4 src bytes in ebx
    movd mm0, edx; // store

    add eax, ecx ;
    add eax, ecx ; to next 2 lines
    mov edx, [eax] ; 2 source bytes
    shl edx, 16;  move to hi word of ebx
    mov dx, word [eax+ecx];  put 2 bytes (next line) to low word, got all 4 src bytes in ebx
    movd mm1, edx;

    PUNPCKLDQ mm0, mm1 ; combine both low 4 bytes to one 8 byte register

    mov eax, [esp+0+12]		; ref
    mov ecx, [esp+0+16]		; dpitch
    mov edx, [eax]; 2 ref bytes
    shl edx, 16; // move to hi word of ebx
    mov dx, word [eax+ecx]; // put 2 bytes (next line) to low word, got all 4 ref bytes in ebx
    movd mm2, edx; store

    add eax, ecx ;
    add eax, ecx ; to next 2 lines
    mov edx, [eax] ; 2 ref bytes
    shl edx, 16;  move to hi word of ebx
    mov dx, word [eax+ecx];  put 2 bytes (next line) to low word, got all 4 ref bytes in ebx
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

    push ebx

    mov eax, [esp+4+ 4]		; cur
    mov ecx, [esp+4+ 8]		; cpitch
    mov ebx, [esp+4+12]		; ref
    mov edx, [esp+4+16]		; dpitch

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

	pop ebx

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

    push ebx

    mov eax, [esp+4+ 4]		; cur
    mov ecx, [esp+4+ 8]		; cpitch
    mov ebx, [esp+4+12]		; ref
    mov edx, [esp+4+16]		; dpitch

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

	pop ebx

	ret

