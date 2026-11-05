// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_ABI_IO_HH
#define PCKOS_ABI_IO_HH

#include <stdint.h>

namespace abi
{
    enum : uint32_t
    {
        stdin_descriptor = 0,
        stdout_descriptor = 1,
        stderr_descriptor = 2
    };
}

#endif
