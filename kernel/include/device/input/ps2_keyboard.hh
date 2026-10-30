// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_DEVICE_INPUT_PS2_KEYBOARD_HH
#define PCKOS_DEVICE_INPUT_PS2_KEYBOARD_HH

namespace device
{
    void keyboard_initialize();
    // Returns one translated character, or -1 when the queue is empty.
    int keyboard_read_char();
}

#endif
