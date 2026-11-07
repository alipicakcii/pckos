#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) PCK
set -eu
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$SCRIPT_DIR"
./iso.sh
./share.sh
./vbox.sh
mkdir -p downloads
cp pckos.iso downloads/pckos.iso
cp pckfs.img downloads/pckfs.img
cp pckfs-vbox.vdi downloads/pckfs-vbox.vdi
(cd downloads && sha256sum pckos.iso pckfs.img pckfs-vbox.vdi > SHA256SUMS)
