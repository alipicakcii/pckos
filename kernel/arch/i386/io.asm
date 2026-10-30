; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) PCK


section .text


global outb
global inb


outb:
    push   ebp
    mov    ebp,esp
    sub    esp,0x8
    mov    edx,DWORD  [ebp+0x8]
    mov    eax,DWORD  [ebp+0xc]
    mov    WORD  [ebp-0x4],dx
    mov    BYTE  [ebp-0x8],al
    movzx  edx,WORD  [ebp-0x4]
    movzx  eax,BYTE  [ebp-0x8]
    out    dx,al
    nop
    leave
    ret

inb:
    push   ebp
    mov    ebp,esp
    sub    esp,0x14
    mov    eax,DWORD  [ebp+0x8]
    mov    WORD  [ebp-0x14],ax
    mov    eax,  [ebp-0x14]
    mov    edx,eax
    in     al,dx
    mov    BYTE  [ebp-0x1],al
    movzx  eax,BYTE  [ebp-0x1]
    leave
    ret

