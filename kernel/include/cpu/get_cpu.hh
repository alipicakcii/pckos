// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_CPU_REGISTERS_HH
#define PCKOS_CPU_REGISTERS_HH

#include <stdint.h>

namespace cpu
{
    // Geçerli sayfa dizini; kullanıcı adres alanları arasında geçişte kullanılır.
    extern "C" uint32_t read_cr3();
}

#endif
