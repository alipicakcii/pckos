// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <user/system/info.hh>
#include <user/system/power.hh>
#include <user/syscall/call.hh>

namespace user
{
    int32_t get_ticks()
    {
        return call(syscall_get_ticks);
    }

    int32_t get_datetime(DateTime *value)
    {
        if (value == nullptr)
            return -22;
        return call(syscall_get_datetime, reinterpret_cast<uint32_t>(value));
    }

    int32_t get_system_info(SystemInfo *info)
    {
        return call(syscall_system_info, reinterpret_cast<uint32_t>(info));
    }

    int32_t get_kernel_log(char *buffer, uint32_t capacity)
    {
        return call(syscall_get_kernel_log,
                    reinterpret_cast<uint32_t>(buffer), capacity);
    }

    int32_t read_kernel_log(char *buffer, uint32_t capacity, uint32_t *cursor)
    {
        return call(syscall_read_kernel_log, reinterpret_cast<uint32_t>(buffer),
                    capacity, reinterpret_cast<uint32_t>(cursor));
    }

    int32_t shutdown()
    {
        return call(syscall_power_off);
    }
}
