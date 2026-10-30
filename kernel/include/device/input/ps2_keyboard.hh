// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_DEVICE_INPUT_PS2_KEYBOARD_HH
#define PCKOS_DEVICE_INPUT_PS2_KEYBOARD_HH

namespace device
{
    void keyboard_initialize();
    // Kuyruktan çevrilmiş bir karakter döndürür; kuyruk boşsa -1 döndürür.
    int keyboard_read_char();
}

#endif
