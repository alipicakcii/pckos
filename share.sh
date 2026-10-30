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
cp kernel/user/apps/terminal_process.bin "$STAGING/apps/terminal.bin"
cp kernel/user/apps/file_manager_process.bin "$STAGING/apps/file_manager.bin"
cp kernel/user/apps/task_manager_process.bin "$STAGING/apps/task_manager.bin"
cp kernel/user/apps/calculator_process.bin "$STAGING/apps/calculator.bin"
cp kernel/user/apps/notepad_process.bin "$STAGING/apps/notepad.bin"
python3 tools/mkpckfs.py --source "$STAGING" --output pckfs.img
