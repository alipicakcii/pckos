// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef ARCH_I386_IO_H
#define ARCH_I386_IO_H

#include <stdint.h>

namespace cpu
{
    extern "C" void outb(uint16_t port, uint8_t data);
    extern "C" uint8_t inb(uint16_t port);


} // namespace kernel

#endif