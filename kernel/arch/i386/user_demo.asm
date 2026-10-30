; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) PCK
BITS 32

USER_CODE_ADDRESS equ 0x40000000
SYS_EXIT equ 0
SYS_WRITE equ 1
SYS_GET_TICKS equ 3

section .rodata
global pckos_user_demo_start
global pckos_user_demo_end

pckos_user_demo_start:
    mov eax, SYS_WRITE
    mov ebx, 1
    mov ecx, USER_CODE_ADDRESS + pckos_user_demo_message - pckos_user_demo_start
    mov edx, pckos_user_demo_message_end - pckos_user_demo_message
    int 0x80

    mov eax, SYS_GET_TICKS
    int 0x80

    mov eax, SYS_EXIT
    xor ebx, ebx
    int 0x80

.unexpected_return:
    jmp .unexpected_return

pckos_user_demo_message:
    db '[Ring3] Kullanici programi ve int 0x80 calisti.', 10
pckos_user_demo_message_end:
pckos_user_demo_end:
