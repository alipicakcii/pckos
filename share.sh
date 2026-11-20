#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) PCK
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$SCRIPT_DIR"
./build.sh

STAGING="$SCRIPT_DIR/sysroot/pckfs-source"
rm -rf "$STAGING"
mkdir -p "$STAGING"
if [ -d "$SCRIPT_DIR/host-share" ]; then
    cp -R "$SCRIPT_DIR/host-share/." "$STAGING"/
fi
mkdir -p "$STAGING/apps"
find "$STAGING/apps" -type f -name '*.bin' -delete
mkdir -p "$STAGING/lib"
mkdir -p "$STAGING/system/icons"
cp "$SCRIPT_DIR"/assets/app-icons/*.png "$STAGING/system/icons/"
cp "$SCRIPT_DIR/kernel/user/lib/libpckos.so" "$STAGING/lib/libpckos.so"
cp kernel/user/apps/terminal_process.elf "$STAGING/apps/terminal.elf"
cp kernel/user/apps/file_manager_process.elf "$STAGING/apps/file_manager.elf"
cp kernel/user/apps/task_manager_process.elf "$STAGING/apps/task_manager.elf"
cp kernel/user/apps/calculator_process.elf "$STAGING/apps/calculator.elf"
cp kernel/user/apps/notepad_process.elf "$STAGING/apps/notepad.elf"
cp kernel/user/apps/demo_process.elf "$STAGING/apps/demo.elf"
cp kernel/user/apps/hello_word.elf "$STAGING/apps/hello_word.elf"
cp kernel/user/apps/system_info_process.elf "$STAGING/apps/system_info.elf"
cp kernel/user/apps/clock_process.elf "$STAGING/apps/clock.elf"
cp kernel/user/apps/unit_converter_process.elf "$STAGING/apps/unit_converter.elf"
cp kernel/user/apps/memory_hunter_process.elf "$STAGING/apps/memory_hunter.elf"
python3 tools/mkpckfs.py --source "$STAGING" --output pckfs.img
