; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) PCK
BITS 32

section .rodata
; bu iki global tanımı
; şu an için user_mode.cpp kullanıp başlanguc ve bitiş adresini kullanıyorusz
global pckos_user_desktop_start
global pckos_user_desktop_end

pckos_user_desktop_start:
    ; NASM'ın derleme sırasında  baytlarını mevcuta eklemesins
    incbin "user/apps/desktop.bin"
pckos_user_desktop_end:
