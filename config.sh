# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) PCK
SYSTEM_HEADER_PROJECTS="kernel"
PROJECTS="kernel"

export MAKE=${MAKE:-make}
export HOST=i686-elf

export AR=${HOST}-ar
export AS=${HOST}-as
export CC=${HOST}-g++

export PREFIX=/usr
export EXEC_PREFIX=$PREFIX
export BOOTDIR=/boot
export LIBDIR=$EXEC_PREFIX/lib
export INCLUDEDIR=$PREFIX/include

export CFLAGS=
export CPPFLAGS=''

# kök dizini
export SYSROOT="$(pwd)/sysroot"
export CC="$CC --sysroot=$SYSROOT"

# -elf gcc hedeflerinin bir sistem başlık dosyası dizinine (system include directory) sahip olmaması durumuna yönelik geçici çözüm;
# zira bu hedefler --with-sysroot yerine --without-headers ile yapılandırılmıştır.
if echo "$HOST" | grep -Eq -- '-elf($|-)'; then
  export CC="$CC -isystem=$INCLUDEDIR"
fi
