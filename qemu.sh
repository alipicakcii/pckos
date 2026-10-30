#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) PCK

set -e
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$SCRIPT_DIR"

. ./clean.sh
. ./iso.sh
. ./share.sh

AUDIO_BACKEND=none
if command -v pactl >/dev/null 2>&1 && pactl info >/dev/null 2>&1; then
    AUDIO_BACKEND=pa
fi

CPU_ARGS="-cpu qemu32 -accel tcg"
if [ -r /dev/kvm ] && [ -w /dev/kvm ]; then
    CPU_ARGS="-cpu host -enable-kvm"
fi

DISPLAY_ARGS="-display none"
if command -v xdpyinfo >/dev/null 2>&1; then
    if xdpyinfo >/dev/null 2>&1; then
        DISPLAY_ARGS="-display gtk"
    fi
elif [ -n "${DISPLAY:-}" ] || [ -n "${WAYLAND_DISPLAY:-}" ]; then
    DISPLAY_ARGS="-display gtk"
fi

# -cpu host -enable-kvm  = cpu configurasyon
# -s -S gdb icin debugu etkinleştiri
# cpu registirlerini görmek icin bayrak -monitor stdio
qemu-system-i386 \
-m 2G \
$CPU_ARGS \
$DISPLAY_ARGS \
-cdrom "$SCRIPT_DIR/pckos.iso" \
-drive file="$SCRIPT_DIR/pckfs.img",format=raw,if=ide,index=0,media=disk \
-serial "file:$SCRIPT_DIR/serial" \
-audiodev "$AUDIO_BACKEND",id=snd0 -machine pcspk-audiodev=snd0 \
-monitor stdio \
-machine pc,smbios-entry-point-type=32 \
-rtc base=localtime


