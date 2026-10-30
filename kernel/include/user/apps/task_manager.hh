// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_USER_TASK_MANAGER_HH
#define PCKOS_USER_TASK_MANAGER_HH

#include <stdint.h>
#include <user/apps/app_thread.hh>
#include <user/syscall.hh>

namespace user::apps
{
    class TaskManager
    {
    public:
        TaskManager();
        void draw(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
        bool consume_dirty();
        static void thread_entry(void *argument);

    private:
        struct Row
        {
            user::ThreadInfo thread;
            uint32_t cpu_percent;
        };

        struct ProcessRow
        {
            user::ProcessInfo process;
            uint32_t cpu_percent;
        };

        void refresh();

        SpinLock lock_;
        user::SystemInfo system_;
        ProcessRow processes_[8];
        uint32_t process_count_;
        uint32_t previous_process_ticks_[8];
        Row rows_[16];
        uint32_t row_count_;
        uint32_t previous_ticks_;
        uint32_t previous_cpu_ticks_[16];
        volatile uint32_t dirty_;
    };
}

#endif
