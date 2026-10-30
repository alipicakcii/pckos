// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_CPU_REGISTERS_HH
#define PCKOS_CPU_REGISTERS_HH

#include <stdint.h>

namespace cpu
{
    // Current page directory; used when switching user address spaces.
    extern "C" uint32_t read_cr3();
}

#endif
