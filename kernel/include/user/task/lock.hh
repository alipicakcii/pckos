// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_USER_LIB_LOCK_HH
#define PCKOS_USER_LIB_LOCK_HH

#include <stdint.h>
#include <user/task/thread.hh>

namespace user::lib
{
    class SpinLock
    {
    public:
        constexpr SpinLock() : value_(0) {}

        void lock()
        {
            for (;;)
            {
                uint32_t previous = 1;
                __asm__ volatile("xchgl %0, %1"
                    : "+r"(previous), "+m"(value_) :: "memory");
                if (previous == 0)
                    return;
                user::thread_yield();
            }
        }

        void unlock()
        {
            __asm__ volatile("" ::: "memory");
            value_ = 0;
        }

    private:
        volatile uint32_t value_;
    };

    class LockGuard
    {
    public:
        explicit LockGuard(SpinLock &lock) : lock_(lock) { lock_.lock(); }
        ~LockGuard() { lock_.unlock(); }
        LockGuard(const LockGuard &) = delete;
        LockGuard &operator=(const LockGuard &) = delete;

    private:
        SpinLock &lock_;
    };
}

#endif
