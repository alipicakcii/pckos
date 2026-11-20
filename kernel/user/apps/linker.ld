/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) PCK */
ENTRY(_start)

PHDRS
{
    headers PT_LOAD FILEHDR PHDRS FLAGS(5);
    text PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
    dynamic PT_DYNAMIC FLAGS(6);
}

SECTIONS
{
    . = 0x40000000 + SIZEOF_HEADERS;
    .text :
    {
        KEEP(*(.text._start))
        *(.text .text.*)
        *(.plt .plt.*)
    } :headers

    . = ALIGN(0x1000);
    .rodata :
    {
        *(.rodata .rodata.*)
    } :text

    .hash : { *(.hash) } :text
    .gnu.hash : { *(.gnu.hash) } :text
    .dynsym : { *(.dynsym) } :text
    .dynstr : { *(.dynstr) } :text
    .gnu.version : { *(.gnu.version .gnu.version.*) } :text

    .rel.dyn : { *(.rel.dyn) } :text
    .rel.plt : { *(.rel.plt) } :text

    . = ALIGN(0x1000);
    .data :
    {
        *(.data .data.*)
        *(.got .got.* .got.plt)
    } :data
    .dynamic : { *(.dynamic) } :data :dynamic
    .bss :
    {
        *(.bss .bss.*)
        *(COMMON)
    } :data

    /DISCARD/ :
    {
        *(.eh_frame .eh_frame.*)
        *(.comment)
        *(.note .note.*)
    }
}
