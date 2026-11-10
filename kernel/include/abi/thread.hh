// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_ABI_THREAD_HH
#define PCKOS_ABI_THREAD_HH

#include <stdint.h>

namespace abi
{
    constexpr uint32_t MAX_THREADS = 64;
    struct ThreadInfo
    {
        uint32_t id;
        uint32_t process_id;
        uint32_t state;
        uint32_t flags;
        uint32_t cpu_ticks;
        uint32_t stack_bytes;
        uint32_t cpu_id;
        char name[24];
    };

    using ThreadEntry = void (*)(void *argument);

    static_assert(sizeof(ThreadInfo) == 52);
}

#endif
