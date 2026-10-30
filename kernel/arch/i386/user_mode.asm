; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) PCK
BITS 32

global user_mode_enter
global user_mode_exit_trampoline
extern pckos_user_saved_kernel_esp
extern pckos_user_saved_kernel_eflags
extern pckos_user_saved_kernel_cr3

USER_CODE_SELECTOR equ 0x1B
USER_DATA_SELECTOR equ 0x23
KERNEL_DATA_SELECTOR equ 0x10

section .text

user_mode_enter:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi
    mov [pckos_user_saved_kernel_esp], esp

    pushfd
    pop eax
    mov [pckos_user_saved_kernel_eflags], eax

    mov eax, cr3
    mov [pckos_user_saved_kernel_cr3], eax
    mov eax, [ebp + 16]
    mov cr3, eax

    push dword USER_DATA_SELECTOR
    push dword [ebp + 12]
    push dword 0x202
    push dword USER_CODE_SELECTOR
    push dword [ebp + 8]

    mov ax, USER_DATA_SELECTOR
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    iret

user_mode_exit_trampoline:
    cli
    mov eax, [pckos_user_saved_kernel_cr3]
    mov cr3, eax
    mov esp, [pckos_user_saved_kernel_esp]

    mov ax, KERNEL_DATA_SELECTOR
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov eax, [pckos_user_saved_kernel_eflags]
    test eax, 0x200
    jz .interrupts_were_disabled
    sti
.interrupts_were_disabled:
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret
