; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) PCK
BITS 32

section .rodata
global pckos_user_desktop_start
global pckos_user_desktop_end

pckos_user_desktop_start:
    incbin "user/apps/desktop.bin"
pckos_user_desktop_end:
