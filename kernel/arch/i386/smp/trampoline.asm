; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) PCK
section .rodata
BITS 16

global smp_trampoline_start
global smp_trampoline_end

BASE equ 0x8000
MAILBOX equ 0x9000

smp_trampoline_start:
    cli
    cld
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    lgdt [BASE + .gdt_pointer - smp_trampoline_start]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp dword 0x08:(BASE + .protected - smp_trampoline_start)

BITS 32
.protected:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax
    mov eax, [MAILBOX]
    mov cr3, eax
    mov eax, cr4
    or eax, 0x10
    mov cr4, eax
    mov eax, cr0
    or eax, 0x80010000
    mov cr0, eax
    mov esp, [MAILBOX + 4]
    xor ebp, ebp
    sub esp, 12
    push dword [MAILBOX + 12]
    mov eax, [MAILBOX + 8]
    call eax
.halt:
    cli
    hlt
    jmp .halt

align 8
.gdt:
    dq 0
    dq 0x00CF9A000000FFFF
    dq 0x00CF92000000FFFF
.gdt_end:
.gdt_pointer:
    dw .gdt_end - .gdt - 1
    dd BASE + .gdt - smp_trampoline_start
smp_trampoline_end:

section .note.GNU-stack noalloc noexec nowrite progbits
