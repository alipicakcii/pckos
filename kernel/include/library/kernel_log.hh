// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_KERNEL_LOG_HH
#define PCKOS_KERNEL_LOG_HH

#include <stddef.h>

namespace lib
{
    constexpr size_t KERNEL_LOG_CAPACITY = 4096;

    // Seri porta yazar ve aynı baytları çekirdek günlüğünde saklar.
    void kernel_log_write_char(char character);

    // Günlüğün en yeni baytlarını kronolojik sırayla kopyalar ve sonuna null karakteri ekler.
    size_t kernel_log_snapshot(char *destination, size_t capacity);
}

#endif
