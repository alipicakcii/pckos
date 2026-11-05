// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_ABI_PROCESS_HH
#define PCKOS_ABI_PROCESS_HH

#include <stdint.h>

namespace abi
{
    struct ProcessInfo
    {
        uint32_t id;
        uint32_t parent_id;
        uint32_t main_thread_id;
        uint32_t thread_count;
        uint32_t cpu_ticks;
        char name[24];
    };

    static_assert(sizeof(ProcessInfo) == 44);
}

#endif
