// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <user/memory/heap.hh>
#include <user/syscall/call.hh>

namespace user
{
    int32_t get_heap_usage()
    {
        return call(syscall_get_heap_usage);
    }

    void *heap_alloc(uint32_t size, uint32_t alignment)
    {
        const int32_t result = call(syscall_heap_alloc, size, alignment);
        return result < 0 ? nullptr : reinterpret_cast<void *>(result);
    }

    int32_t heap_free(void *address)
    {
        return call(syscall_heap_free, reinterpret_cast<uint32_t>(address));
    }
}
