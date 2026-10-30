#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) PCK
set -e
. ./config.sh

for PROJECT in $PROJECTS; do
  (cd $PROJECT && $MAKE clean)
done

rm -rf sysroot
rm -rf isodir
rm -f pckos.iso
rm -f pckfs.img
rm -f serial
rm -rf pckfs-vbox.vdi
# Generated regression captures and Python bytecode.
rm -rf tools/tests/gui-output
find tools -type d -name __pycache__ -prune -exec rm -rf {} +
