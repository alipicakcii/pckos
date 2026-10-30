// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_KERNEL_LOG_HH
#define PCKOS_KERNEL_LOG_HH

#include <stddef.h>

namespace lib
{
    constexpr size_t KERNEL_LOG_CAPACITY = 4096;

    // Writes to the serial port and keeps the same bytes in the kernel log.
    void kernel_log_write_char(char character);

    // Copies the newest log bytes in chronological order and null-terminates them.
    size_t kernel_log_snapshot(char *destination, size_t capacity);
}

#endif
