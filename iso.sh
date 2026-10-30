#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) PCK
set -e
. ./build.sh

mkdir -p isodir
mkdir -p isodir/boot
mkdir -p isodir/boot/grub

cp sysroot/boot/pckos.bin isodir/boot/pckos.bin
cat > isodir/boot/grub/grub.cfg << EOF
set default=0
set timeout=0
menuentry "pckos" {
	set gfxpayload=1024x768x32
	multiboot /boot/pckos.bin
    boot
}
EOF
grub-mkrescue -o pckos.iso isodir
