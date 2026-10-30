; SPDX-License-Identifier: GPL-3.0-or-later
; Copyright (C) PCK
section .init
global _init
_init:
   push ebp
   mov ebp, esp

section .fini
global _fini
_fini:
   push ebp
   mov ebp, esp