// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_DEVICE_SERIAL_HH
#define PCKOS_DEVICE_SERIAL_HH

namespace device
{
    void serial_initialize();
    void serial_write_char(char character);
    void serial_write_text(const char *text);
    bool serial_data_ready();
    char serial_read_char();
}

#endif
