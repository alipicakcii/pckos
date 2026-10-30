; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) PCK
[GLOBAL flush_gdt]
[GLOBAL flush_tss]


flush_gdt:
    mov eax, [esp+4] ;[esp+4] parametre
    lgdt [eax]

    mov ax, 0x10  ;0x10 veri segmentimizin uzaklığıdır
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    jmp 0x08:.flush ;0x08 kod segmentimizin uzaklığıdır
.flush:
    ret



flush_tss:
    mov ax, 0x28
    ltr ax
    ret

