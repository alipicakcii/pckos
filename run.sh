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
elif [ -n "${XDG_RUNTIME_DIR:-}" ] && [ -S "$XDG_RUNTIME_DIR/pipewire-0" ]; then
    AUDIO_BACKEND=pipewire
elif [ -d /dev/snd ]; then
    AUDIO_BACKEND=alsa
fi
echo "QEMU ses arka ucu: $AUDIO_BACKEND"

QEMU_CPUS=${PCKOS_CPUS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 1)}
case "$QEMU_CPUS" in
    ''|*[!0-9]*|0) echo "PCKOS_CPUS pozitif bir tamsayi olmali" >&2; exit 1 ;;
esac
if [ "$QEMU_CPUS" -gt 32 ]; then QEMU_CPUS=32; fi

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
# QEMU'ya verilecek mantıksal işlemci sayısını belirle
qemu-system-i386 \
-m 2G \
-smp "$QEMU_CPUS" \
$CPU_ARGS \
$DISPLAY_ARGS \
-cdrom "$SCRIPT_DIR/pckos.iso" \
-drive file="$SCRIPT_DIR/pckfs.img",format=raw,if=ide,index=0,media=disk \
-serial file://home/pck/Masaüstü/projects/pckos/pckos/serial \
-audiodev "$AUDIO_BACKEND",id=snd0 \
-monitor stdio \
-machine pc,pcspk-audiodev=snd0,smbios-entry-point-type=32 \
-rtc base=localtime


