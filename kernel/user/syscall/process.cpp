// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <user/task/process.hh>
#include <user/syscall/call.hh>

namespace user
{
    uint32_t process_count()
    {
        return static_cast<uint32_t>(call(syscall_process_count));
    }

    int32_t process_get_info(uint32_t index, ProcessInfo *info)
    {
        return call(syscall_process_info, index,
                    reinterpret_cast<uint32_t>(info));
    }

    int32_t process_spawn(const char *path, const char *name)
    {
        if (path == nullptr || name == nullptr)
            return -22;
        return call(syscall_process_spawn, reinterpret_cast<uint32_t>(path),
                    reinterpret_cast<uint32_t>(name));
    }

    int32_t process_minimize()
    {
        return call(syscall_process_minimize);
    }

    bool process_is_minimized(uint32_t process_id)
    {
        return call(syscall_process_is_minimized, process_id) != 0;
    }

    int32_t process_focus(uint32_t process_id)
    {
        return call(syscall_process_focus, process_id);
    }

    uint32_t process_focused_id()
    {
        return static_cast<uint32_t>(call(syscall_process_focused_id));
    }

    [[noreturn]] void exit(int32_t status)
    {
        (void)call(syscall_exit, static_cast<uint32_t>(status));
        for (;;)
            __asm__ volatile("pause");
    }
}
