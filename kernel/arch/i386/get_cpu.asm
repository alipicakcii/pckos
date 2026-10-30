; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) PCK
BITS 32
section .text

global read_cr3
read_cr3:
    mov eax, cr3
    ret
