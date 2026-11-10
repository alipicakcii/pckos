// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_USER_THREAD_HH
#define PCKOS_USER_THREAD_HH

#include <abi/thread.hh>
#include <user/syscall/call.hh>

namespace user
{
    using abi::ThreadInfo;
    using abi::ThreadEntry;

    inline void thread_yield()
    {
        (void)call(syscall_thread_yield);
    }

    inline void thread_sleep(uint32_t ticks)
    {
        (void)call(syscall_thread_sleep, ticks);
    }

    inline uint32_t thread_id()
    {
        return static_cast<uint32_t>(call(syscall_thread_id));
    }

    inline uint32_t thread_count()
    {
        return static_cast<uint32_t>(call(syscall_thread_count));
    }

    inline uint32_t thread_capacity()
    {
        return static_cast<uint32_t>(call(syscall_thread_capacity));
    }

    inline uint32_t thread_switch_count()
    {
        return static_cast<uint32_t>(call(syscall_thread_switch_count));
    }

    [[noreturn]] inline void thread_start_trampoline(ThreadEntry entry,
                                                      void *argument)
    {
        if (entry != nullptr)
            entry(argument);
        (void)call(syscall_thread_exit);
        for (;;) (void)call(syscall_thread_yield);
    }

    inline int32_t thread_create(ThreadEntry entry, void *argument,
                                 const char *name)
    {
        if (entry == nullptr || name == nullptr)
            return -22;
        return call(syscall_thread_create,
                    reinterpret_cast<uint32_t>(&thread_start_trampoline),
                    reinterpret_cast<uint32_t>(entry),
                    reinterpret_cast<uint32_t>(argument),
                    0, reinterpret_cast<uint32_t>(name));
    }

    [[noreturn]] inline void thread_exit()
    {
        (void)call(syscall_thread_exit);
        for (;;) (void)call(syscall_thread_yield);
    }

    inline int32_t thread_get_info(uint32_t id, ThreadInfo *info)
    {
        return call(syscall_thread_info, id,
                    reinterpret_cast<uint32_t>(info));
    }

    inline int32_t thread_set_name(const char *name)
    {
        return call(syscall_thread_set_name,
                    reinterpret_cast<uint32_t>(name));
    }
}

#endif
