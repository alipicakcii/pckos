// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include "internal.hh"

namespace kernel::syscalls
{
    mem::Vmm *active_virtual_memory = nullptr;

    bool copy_user_string(uint32_t address, char *destination, uint32_t capacity)
    {
        if (destination == nullptr || capacity == 0 ||
            active_virtual_memory == nullptr)
            return false;

        for (uint32_t i = 0; i < capacity; ++i)
        {
            if (address > UINT32_MAX - i ||
                !active_virtual_memory->is_user_range(address + i, 1, false))
                return false;
            const char character = *reinterpret_cast<const char *>(address + i);
            destination[i] = character;
            if (character == '\0')
                return i != 0;
        }
        destination[capacity - 1] = '\0';
        return false;
    }
}
