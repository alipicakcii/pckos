// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_PS2_MOUSE_HH
#define PCKOS_PS2_MOUSE_HH

#include <stdint.h>

namespace device
{
    struct MouseEvent
    {
        int32_t delta_x;
        int32_t delta_y;
        uint8_t buttons;
    };

    bool mouse_initialize();
    bool mouse_read_event(MouseEvent *event);
    bool mouse_is_ready();
}

#endif
