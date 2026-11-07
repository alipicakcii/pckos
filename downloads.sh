#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) PCK
set -eu
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$SCRIPT_DIR"
./iso.sh
./share.sh
mkdir -p downloads
cp pckos.iso downloads/pckos.iso
cp pckfs.img downloads/pckfs.img
if [ -f pckfs-vbox.vdi ]; then
    cp pckfs-vbox.vdi downloads/pckfs-vbox.vdi
fi
(
    cd downloads
    set -- pckos.iso pckfs.img
    if [ -f pckfs-vbox.vdi ]; then
        set -- "$@" pckfs-vbox.vdi
    fi
    sha256sum "$@" > SHA256SUMS
)
