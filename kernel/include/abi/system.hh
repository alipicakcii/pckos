// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_ABI_SYSTEM_HH
#define PCKOS_ABI_SYSTEM_HH

#include <stdint.h>

namespace abi
{
    struct SystemInfo
    {
        uint32_t total_pages;
        uint32_t free_pages;
        uint32_t kernel_heap_used;
        uint32_t ticks;
        uint32_t switch_count;
        uint32_t cpu_family;
        uint32_t cpu_model;
        uint32_t logical_cpu_count;
        char cpu_vendor[13];
        char cpu_brand[49];
    };

    struct DateTime
    {
        uint32_t year, month, day, hour, minute, second;
    };

    static_assert(sizeof(SystemInfo) == 96);
    static_assert(sizeof(DateTime) == 24);
}

#endif
