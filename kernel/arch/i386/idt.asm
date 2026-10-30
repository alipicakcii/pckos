; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) PCK
[GLOBAL flush_idt]

flush_idt:
    mov eax, [esp+4] 
    lidt [eax]
    ret