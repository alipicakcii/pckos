// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_USER_HEAP_HH
#define PCKOS_USER_HEAP_HH

#include <user/syscall/call.hh>

namespace user
{
    inline int32_t get_heap_usage()
    {
        return call(syscall_get_heap_usage);
    }

    inline void *heap_alloc(uint32_t size, uint32_t alignment = 16)
    {
        const int32_t result = call(syscall_heap_alloc, size, alignment);
        return result < 0 ? nullptr : reinterpret_cast<void *>(result);
    }

    inline int32_t heap_free(void *address)
    {
        return call(syscall_heap_free, reinterpret_cast<uint32_t>(address));
    }
}

#endif
