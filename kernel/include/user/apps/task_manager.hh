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
        void scroll(int32_t rows);
        void scroll_to_top();
        void scroll_to_bottom();
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
        uint32_t previous_process_ids_[8];
        uint32_t previous_process_ticks_[8];
        Row rows_[abi::MAX_THREADS];
        uint32_t row_count_;
        uint32_t scroll_offset_;
        uint32_t visible_rows_;
        uint32_t previous_ticks_;
        uint32_t total_cpu_percent_;
        bool baseline_ready_;
        uint32_t previous_cpu_ticks_[abi::MAX_THREADS];
        volatile uint32_t dirty_;
    };
}

#endif
