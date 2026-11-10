// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_USER_APP_THREAD_HH
#define PCKOS_USER_APP_THREAD_HH

#include <user/thread.hh>
#include <user/lib/lock.hh>
#include <stdint.h>

namespace user::apps
{
    struct AppEvent
    {
        uint32_t type;
        int32_t value0;
        int32_t value1;
        uint32_t value2;
        uint32_t value3;
    };

    template <uint32_t Capacity>
    class AppEventQueue
    {
        static_assert(Capacity > 1 && (Capacity & (Capacity - 1)) == 0,
                      "event queue size must be a power of two");

    public:
        AppEventQueue() : head_(0), tail_(0) {}

        bool push(const AppEvent &event)
        {
            const uint32_t head = head_;
            const uint32_t next = (head + 1) & (Capacity - 1);
            if (next == tail_)
                return false;
            events_[head] = event;
            __asm__ volatile("" ::: "memory");
            head_ = next;
            return true;
        }

        bool pop(AppEvent *event)
        {
            const uint32_t tail = tail_;
            if (tail == head_ || event == nullptr)
                return false;
            *event = events_[tail];
            __asm__ volatile("" ::: "memory");
            tail_ = (tail + 1) & (Capacity - 1);
            return true;
        }

    private:
        volatile uint32_t head_;
        volatile uint32_t tail_;
        AppEvent events_[Capacity];
    };

    using user::lib::SpinLock;
    using user::lib::LockGuard;

}

#endif
