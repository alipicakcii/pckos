/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) PCK */
ENTRY(_start)
SECTIONS
{
    . = 0x40000000;
    .text : { KEEP(*(.text._start)) *(.text*) }
    .rodata : { *(.rodata*) }
    .data : { *(.data*) }
    .bss : { *(.bss*) *(COMMON) }
    /DISCARD/ : { *(.eh_frame*) *(.comment*) *(.note*) *(.got*) }
}
