#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) PCK
rm -f pckfs-vbox.vdi
VBoxManage convertfromraw pckfs.img pckfs-vbox.vdi --format VDI --uuid=23ae6946-f6e0-4fff-8408-aff844c8f4f2
