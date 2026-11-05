// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_ABI_GRAPHICS_HH
#define PCKOS_ABI_GRAPHICS_HH

#include <stdint.h>

namespace abi
{
    struct GraphicsInfo
    {
        uint32_t width;
        uint32_t height;
    };

    static_assert(sizeof(GraphicsInfo) == 8);
}

#endif
