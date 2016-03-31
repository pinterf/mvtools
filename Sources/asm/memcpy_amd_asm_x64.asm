.code

;memcpy_amd proc dest:dword,src:dword,n:dword
; dest = rcx
; src = rdx
; n = r8d

memcpy_amd proc public frame

	push rdi
	.pushreg rdi
	push rsi
	.pushreg rsi
	push r12
	.pushreg r12
	push r13
	.pushreg r13
	.endprolog
   
    mov r9,rcx       ; copy dest in r9d
	mov r10,64
	mov r11,128
	
	mov r12,offset $memcpy_align_done
	mov r13,offset $memcpy_last_few

	mov		rcx,r8		; number of bytes to copy
	mov		rdi,r9		; destination
	mov		rsi,rdx		; source

	cld
	cmp		rcx, r10
	jb		$memcpy_ic_3	; tiny? skip mmx copy

	cmp		rcx, 32*1024		; don't align between 32k-64k because
	jbe		$memcpy_do_align	;  it appears to be slower
	cmp		rcx, 64*1024
	jbe		$memcpy_align_done
$memcpy_do_align:
	mov		rcx, 8			; a trick that's faster than rep movsb...
	sub		rcx, rdi		; align destination to qword
	and		rcx, 111b		; get the low bits
	sub		r8, rcx		; update copy count
	neg		rcx				; set up to jump into the array
	add		rcx, r12
	jmp		rcx				; jump to array of movsb's

align 4
	movsb
	movsb
	movsb
	movsb
	movsb
	movsb
	movsb
	movsb

$memcpy_align_done:			; destination is dword aligned
	mov		rcx, r8		; number of bytes left to copy
	shr		rcx, 6			; get 64-byte block count
	jz		$memcpy_ic_2	; finish the last few bytes

	cmp		rcx, 1024*64/64	; too big 4 cache? use uncached copy
	jae		$memcpy_uc_test

align 16
$memcpy_ic_1:			; 64-byte block copies, in-cache copy

	prefetchnta [rsi + (200*64/34+192)]		; start reading ahead

	movq	mm0, [rsi+0]	; read 64 bits
	movq	mm1, [rsi+8]
	movq	[rdi+0], mm0	; write 64 bits
	movq	[rdi+8], mm1	;    note:  the normal movq writes the
	movq	mm2, [rsi+16]	;    data to cache; a cache line will be
	movq	mm3, [rsi+24]	;    allocated as needed, to store the data
	movq	[rdi+16], mm2
	movq	[rdi+24], mm3
	movq	mm0, [rsi+32]
	movq	mm1, [rsi+40]
	movq	[rdi+32], mm0
	movq	[rdi+40], mm1
	movq	mm2, [rsi+48]
	movq	mm3, [rsi+56]
	movq	[rdi+48], mm2
	movq	[rdi+56], mm3

	add		rsi, r10			; update source pointer
	add		rdi, r10			; update destination pointer
	dec		rcx				; count down
	jnz		$memcpy_ic_1	; last 64-byte block?

$memcpy_ic_2:
	mov		rcx, r8		; has valid low 6 bits of the byte count
$memcpy_ic_3:
	shr		rcx, 2			; dword count
	and		rcx, 1111b		; only look at the "remainder" bits
	neg		rcx				; set up to jump into the array
	add		rcx, r13
	jmp		rcx				; jump to array of movsd's

$memcpy_uc_test:
	cmp		rcx, 197*1024/64	; big enough? use block prefetch copy
	jae		$memcpy_bp_1

$memcpy_64_test:
	or		rcx, rcx		; tail end of block prefetch will jump here
	jz		$memcpy_ic_2	; no more 64-byte blocks left

align 16
$memcpy_uc_1:				; 64-byte blocks, uncached copy

	prefetchnta [rsi + (200*64/34+192)]		; start reading ahead

	movq	mm0,[rsi+0]		; read 64 bits
	add		rdi,r10			; update destination pointer
	movq	mm1,[rsi+8]
	add		rsi,r10			; update source pointer
	movq	mm2,[rsi-48]
	movntq	[rdi-64], mm0	; write 64 bits, bypassing the cache
	movq	mm0,[rsi-40]	;    note: movntq also prevents the CPU
	movntq	[rdi-56], mm1	;    from READING the destination address
	movq	mm1,[rsi-32]	;    into the cache, only to be over-written
	movntq	[rdi-48], mm2	;    so that also helps performance
	movq	mm2,[rsi-24]
	movntq	[rdi-40], mm0
	movq	mm0,[rsi-16]
	movntq	[rdi-32], mm1
	movq	mm1,[rsi-8]
	movntq	[rdi-24], mm2
	movntq	[rdi-16], mm0
	dec		rcx
	movntq	[rdi-8], mm1
	jnz		$memcpy_uc_1	; last 64-byte block?

	jmp		$memcpy_ic_2		; almost done

$memcpy_bp_1:			; large blocks, block prefetch copy

	cmp		rcx, r11			; big enough to run another prefetch loop?
	jl		$memcpy_64_test			; no, back to regular uncached copy

	mov		rax, 128 / 2		; block prefetch loop, unrolled 2X
	add		rsi, 128 * 64	; move to the top of the block
align 16
$memcpy_bp_2:
	mov		rdx, [rsi-64]		; grab one address per cache line
	mov		rdx, [rsi-128]		; grab one address per cache line
	sub		rsi, r11			; go reverse order
	dec		rax					; count down the cache lines
	jnz		$memcpy_bp_2		; keep grabbing more lines into cache

	mov		rax, r11		; now that it's in cache, do the copy
align 16
$memcpy_bp_3:
	movq	mm0, [rsi   ]		; read 64 bits
	movq	mm1, [rsi+ 8]
	movq	mm2, [rsi+16]
	movq	mm3, [rsi+24]
	movq	mm4, [rsi+32]
	movq	mm5, [rsi+40]
	movq	mm6, [rsi+48]
	movq	mm7, [rsi+56]
	add		rsi, r10				; update source pointer
	movntq	[rdi   ], mm0		; write 64 bits, bypassing cache
	movntq	[rdi+ 8], mm1		;    note: movntq also prevents the CPU
	movntq	[rdi+16], mm2		;    from READING the destination address 
	movntq	[rdi+24], mm3		;    into the cache, only to be over-written,
	movntq	[rdi+32], mm4		;    so that also helps performance
	movntq	[rdi+40], mm5
	movntq	[rdi+48], mm6
	movntq	[rdi+56], mm7
	add		rdi, r10				; update dest pointer

	dec		rax					; count down

	jnz		$memcpy_bp_3		; keep copying
	sub		rcx, r11		; update the 64-byte block count
	jmp		$memcpy_bp_1		; keep processing chunks

align 4
	movsd
	movsd			; perform last 1-15 dword copies
	movsd
	movsd
	movsd
	movsd
	movsd
	movsd
	movsd
	movsd			; perform last 1-7 dword copies
	movsd
	movsd
	movsd
	movsd
	movsd
	movsd

$memcpy_last_few:		; dword aligned from before movsd's
	mov		rcx, r8	; has valid low 2 bits of the byte count
	and		rcx, 11b	; the last few cows must come home
	jz		$memcpy_final	; no more, let's leave
	rep		movsb		; the last 1, 2, or 3 bytes

$memcpy_final: 
	emms				; clean up the MMX state
	sfence				; flush the write buffer
	
	pop r13
	pop r12
	pop rsi
	pop rdi
	
	mov		rax,r9	; ret value = destination pointer
	
	ret
	
memcpy_amd endp

end


