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
(cd downloads && sha256sum pckos.iso pckfs.img > SHA256SUMS)
