; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) PCK
	
[GLOBAL _start]  ; kernel endpointimiz 
[EXTERN kernel_main]

	MBOOT_PAGE_ALIGN    equ 1<<0
	MBOOT_MEM_INFO      equ 1<<1
	MBOOT_USE_GFX       equ 1<<2
	MBOOT_HEADER_MAGIC  equ 0x1BADB002
	MBOOT_HEADER_FLAGS  equ MBOOT_PAGE_ALIGN | MBOOT_MEM_INFO | MBOOT_USE_GFX
	MBOOT_CHECKSUM      equ -(MBOOT_HEADER_MAGIC + MBOOT_HEADER_FLAGS)

section .multiboot
align 4
    dd  MBOOT_HEADER_MAGIC
    dd  MBOOT_HEADER_FLAGS
    dd  MBOOT_CHECKSUM
    
    ; Grafik modu açıldığında GRUB bunları zorunlu bekler:)
    dd  0          ; header_addr
    dd  0          ; load_addr
    dd  0          ; load_end_addr
    dd  0          ; bss_end_addr
    dd  0          ; entry_addr
    
    ; Grafik İstek Alani(Linear Framebuffer)
    dd  0          ; mode_type (0 = grafik modu)
    dd  1024       ; width
    dd  768        ; height
    dd  32         ; depth (bpp)

section .bss
	align 16

	stack_bottom:
		resb 16384 ; 16 KiB
	stack_top:

section .text

_start:
	mov esp, stack_top
	; magic (sihirli) numara
	push eax
	; multiboot yapı adresi
	push ebx

	; main foksinoyunu cagir
	call kernel_main

	; buraya asla ulaşılmaz
	cli
	hlt
