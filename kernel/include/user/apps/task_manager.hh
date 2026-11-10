// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_USER_TASK_MANAGER_HH
#define PCKOS_USER_TASK_MANAGER_HH

#include <user/task/process.hh>
#include <user/system/info.hh>
#include <user/task/thread.hh>
#include <stdint.h>
#include <user/apps/app_thread.hh>

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
        Row rows_[abi::MAX_THREADS];
        uint32_t row_count_;
        uint32_t previous_ticks_;
        uint32_t previous_cpu_ticks_[abi::MAX_THREADS];
        volatile uint32_t dirty_;
    };
}

#endif
