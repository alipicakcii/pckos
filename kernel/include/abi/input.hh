// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_ABI_INPUT_HH
#define PCKOS_ABI_INPUT_HH

#include <stdint.h>

namespace abi
{
    struct InputEvent
    {
        uint32_t type; // 1 = klavye, 2 = fare, 3 = zaman aşımı
        int32_t value0; // tuş karakteri veya farenin X eksenindeki hareketi
        int32_t value1; // farenin Y eksenindeki hareketi
        uint32_t buttons;
    };

    static_assert(sizeof(InputEvent) == 16);
}

#endif
