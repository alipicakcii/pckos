// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_USER_PROCESS_HH
#define PCKOS_USER_PROCESS_HH

#include <abi/process.hh>
#include <user/syscall.hh>

namespace user
{
    using abi::ProcessInfo;

    inline uint32_t process_count()
    {
        return static_cast<uint32_t>(call(syscall_process_count));
    }

    inline int32_t process_get_info(uint32_t index, ProcessInfo *info)
    {
        return call(syscall_process_info, index,
                    reinterpret_cast<uint32_t>(info));
    }

    inline int32_t process_spawn(const char *path, const char *name)
    {
        if (path == nullptr || name == nullptr)
            return -22;
        return call(syscall_process_spawn, reinterpret_cast<uint32_t>(path),
                    reinterpret_cast<uint32_t>(name));
    }

    inline int32_t process_minimize()
    {
        return call(syscall_process_minimize);
    }

    // Çağıran process’i sorgulamak için 0 gönderin.
    inline bool process_is_minimized(uint32_t process_id = 0)
    {
        return call(syscall_process_is_minimized, process_id) != 0;
    }

    inline int32_t process_focus(uint32_t process_id)
    {
        return call(syscall_process_focus, process_id);
    }

    inline uint32_t process_focused_id()
    {
        return static_cast<uint32_t>(call(syscall_process_focused_id));
    }

    [[noreturn]] inline void exit(int32_t status)
    {
        (void)call(syscall_exit, static_cast<uint32_t>(status));
        for (;;)
            __asm__ volatile("pause");
    }
}

#endif
