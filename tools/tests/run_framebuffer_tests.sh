#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) PCK
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
OUT="$ROOT/tools/tests/.framebuffer-render-test"
trap 'rm -f "$OUT"' EXIT HUP INT TERM
# Kernel address wrappers intentionally use 32-bit casts. Host tests exercise
# only rendering; unused paging/allocator methods are discarded by the linker.
g++ -std=c++17 -O2 -fpermissive -w -ffunction-sections -fdata-sections \
    -I"$ROOT/kernel/include" \
    "$ROOT/tools/tests/framebuffer_render_test.cpp" \
    "$ROOT/kernel/device/video/frame_buffer.cpp" \
    -Wl,--gc-sections -o "$OUT"
"$OUT"
