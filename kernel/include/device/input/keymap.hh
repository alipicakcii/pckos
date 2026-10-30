// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_DEVICE_INPUT_KEYMAP_HH
#define PCKOS_DEVICE_INPUT_KEYMAP_HH

#include <stdint.h>

namespace device
{
    uint8_t keyboard_translate_scancode(uint8_t scancode, bool shift_pressed);
}

#endif
