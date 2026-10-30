// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <user/apps/task_manager.hh>
#include <user/gui.hh>

namespace user::apps
{
    namespace
    {
        void append(char *destination, uint32_t capacity, uint32_t *at,
                    const char *value)
        {
            while (*value != '\0' && *at + 1 < capacity)
                destination[(*at)++] = *value++;
            destination[*at] = '\0';
        }

        void append_number(char *destination, uint32_t capacity, uint32_t *at,
                           uint32_t value)
        {
            char reverse[10];
            uint32_t count = 0;
            do
            {
                reverse[count++] = static_cast<char>('0' + value % 10);
                value /= 10;
            } while (value != 0 && count < sizeof(reverse));
            while (count != 0 && *at + 1 < capacity)
                destination[(*at)++] = reverse[--count];
            destination[*at] = '\0';
        }

        const char *state_name(uint32_t state)
        {
            switch (state)
            {
                case 1: return "Hazir";
                case 2: return "Calisiyor";
                case 3: return "Uyuyor";
                default: return "Bilinmiyor";
            }
        }
    }

    TaskManager::TaskManager()
        : lock_{}, system_{}, processes_{}, process_count_(0),
          previous_process_ticks_{}, rows_{}, row_count_(0), previous_ticks_(0),
          previous_cpu_ticks_{}, dirty_(0)
    {
        refresh();
    }

    void TaskManager::refresh()
    {
        user::SystemInfo system{};
        if (user::get_system_info(&system) < 0)
            return;

        Row rows[16]{};
        uint32_t count = 0;
        const uint32_t elapsed = system.ticks - previous_ticks_;
        ProcessRow processes[8]{};
        const uint32_t process_count = user::process_count() < 8
            ? user::process_count() : 8;
        for (uint32_t index = 0; index < process_count; ++index)
        {
            user::ProcessInfo info{};
            if (user::process_get_info(index, &info) < 0)
                continue;
            const uint32_t delta = info.cpu_ticks >= previous_process_ticks_[index]
                ? info.cpu_ticks - previous_process_ticks_[index] : info.cpu_ticks;
            processes[index].process = info;
            processes[index].cpu_percent = elapsed == 0 ? 0 :
                (delta >= elapsed ? 100 : (delta * 100) / elapsed);
            previous_process_ticks_[index] = info.cpu_ticks;
        }
        for (uint32_t id = 0; id < 16; ++id)
        {
            user::ThreadInfo info{};
            if (user::thread_get_info(id, &info) < 0)
                continue;
            const uint32_t delta = info.cpu_ticks >= previous_cpu_ticks_[id]
                ? info.cpu_ticks - previous_cpu_ticks_[id] : info.cpu_ticks;
            rows[count].thread = info;
            rows[count].cpu_percent = elapsed == 0 ? 0 :
                (delta >= elapsed ? 100 : (delta * 100) / elapsed);
            if (rows[count].cpu_percent > 100)
                rows[count].cpu_percent = 100;
            previous_cpu_ticks_[id] = info.cpu_ticks;
            ++count;
        }
        previous_ticks_ = system.ticks;

        LockGuard guard(lock_);
        system_ = system;
        process_count_ = process_count;
        for (uint32_t i = 0; i < process_count; ++i)
            processes_[i] = processes[i];
        row_count_ = count;
        for (uint32_t i = 0; i < count; ++i)
            rows_[i] = rows[i];
        dirty_ = 1;
    }

    bool TaskManager::consume_dirty()
    {
        LockGuard guard(lock_);
        const bool dirty = dirty_ != 0;
        dirty_ = 0;
        return dirty;
    }

    void TaskManager::thread_entry(void *argument)
    {
        auto *manager = static_cast<TaskManager *>(argument);
        if (manager == nullptr)
            user::thread_exit();
        for (;;)
        {
            user::thread_sleep(100);
            manager->refresh();
        }
    }

    void TaskManager::draw(uint32_t x, uint32_t y, uint32_t width,
                           uint32_t height)
    {
        user::SystemInfo system{};
        ProcessRow processes[8]{};
        uint32_t process_count = 0;
        Row rows[16]{};
        uint32_t count = 0;
        {
            LockGuard guard(lock_);
            system = system_;
            process_count = process_count_;
            for (uint32_t i = 0; i < process_count; ++i)
                processes[i] = processes_[i];
            count = row_count_;
            for (uint32_t i = 0; i < count; ++i)
                rows[i] = rows_[i];
        }

        user::gui::fill(x, y, width, height, 0xFFC0C0C0);
        char memory[80] = "Bellek: ";
        uint32_t at = 8;
        append_number(memory, sizeof(memory), &at, system.total_pages / 256);
        append(memory, sizeof(memory), &at, " MiB toplam, ");
        append_number(memory, sizeof(memory), &at, system.free_pages / 256);
        append(memory, sizeof(memory), &at, " MiB bos");
        user::gui::text(x + 8, y + 7, memory, user::gui::COLOR_TEXT);

        char processor[96] = "CPU: ";
        at = 5;
        const char *brand = system.cpu_brand[0] != '\0'
            ? system.cpu_brand : system.cpu_vendor;
        append(processor, sizeof(processor), &at, brand);
        append(processor, sizeof(processor), &at, " | 1 cekirdek | i386");
        user::gui::text(x + 8, y + 27, processor, user::gui::COLOR_TEXT);

        char counts[96] = "Surec: ";
        at = 7;
        append_number(counts, sizeof(counts), &at, process_count);
        append(counts, sizeof(counts), &at, " | Thread: ");
        append_number(counts, sizeof(counts), &at, count);
        append(counts, sizeof(counts), &at, " | Zamanlayici gecisi: ");
        append_number(counts, sizeof(counts), &at, system.switch_count);
        user::gui::text(x + 8, y + 47, counts, user::gui::COLOR_TEXT);

        const uint32_t process_table_y = y + 74;
        user::gui::text(x + 8, process_table_y - 16, "Uygulamalar",
                        user::gui::COLOR_TEXT);
        user::gui::frame(x + 6, process_table_y, width > 12 ? width - 12 : width,
                         20, 0xFFB0B0B0, true);
        user::gui::text(x + 10, process_table_y + 2, "PID", user::gui::COLOR_TEXT);
        user::gui::text(x + 48, process_table_y + 2, "Uygulama", user::gui::COLOR_TEXT);
        user::gui::text(x + 224, process_table_y + 2, "Ana TID", user::gui::COLOR_TEXT);
        user::gui::text(x + 312, process_table_y + 2, "Thread", user::gui::COLOR_TEXT);
        user::gui::text(x + 402, process_table_y + 2, "CPU", user::gui::COLOR_TEXT);

        const uint32_t visible_processes = height > 116
            ? (height - 116) / 18 : 0;
        for (uint32_t i = 0; i < process_count && i < visible_processes; ++i)
        {
            const uint32_t row_y = process_table_y + 21 + i * 18;
            if ((i & 1U) == 0)
                user::gui::fill(x + 7, row_y, width > 14 ? width - 14 : width,
                                18, 0xFFD8D8D8);
            char number[12];
            uint32_t number_at = 0;
            number[0] = '\0';
            append_number(number, sizeof(number), &number_at, processes[i].process.id);
            user::gui::text(x + 10, row_y + 1, number, user::gui::COLOR_TEXT);
            user::gui::text(x + 48, row_y + 1, processes[i].process.name,
                            user::gui::COLOR_TEXT);
            number_at = 0;
            number[0] = '\0';
            append_number(number, sizeof(number), &number_at,
                          processes[i].process.main_thread_id);
            user::gui::text(x + 224, row_y + 1, number, user::gui::COLOR_TEXT);
            number_at = 0;
            number[0] = '\0';
            append_number(number, sizeof(number), &number_at,
                          processes[i].process.thread_count);
            user::gui::text(x + 312, row_y + 1, number, user::gui::COLOR_TEXT);
            char cpu[8];
            uint32_t cpu_at = 0;
            cpu[0] = '\0';
            append_number(cpu, sizeof(cpu), &cpu_at, processes[i].cpu_percent);
            append(cpu, sizeof(cpu), &cpu_at, "%");
            user::gui::text(x + 402, row_y + 1, cpu, user::gui::COLOR_TEXT);
        }

        const uint32_t thread_table_y = process_table_y + 22 +
            (process_count < visible_processes ? process_count : visible_processes) * 18 + 18;
        user::gui::text(x + 8, thread_table_y - 16, "Thread'ler",
                        user::gui::COLOR_TEXT);
        user::gui::frame(x + 6, thread_table_y, width > 12 ? width - 12 : width,
                         20, 0xFFB0B0B0, true);
        user::gui::text(x + 10, thread_table_y + 2, "PID", user::gui::COLOR_TEXT);
        user::gui::text(x + 48, thread_table_y + 2, "TID", user::gui::COLOR_TEXT);
        user::gui::text(x + 88, thread_table_y + 2, "Thread", user::gui::COLOR_TEXT);
        user::gui::text(x + 270, thread_table_y + 2, "Durum", user::gui::COLOR_TEXT);
        user::gui::text(x + 382, thread_table_y + 2, "CPU", user::gui::COLOR_TEXT);
        user::gui::text(x + 446, thread_table_y + 2, "Yigin", user::gui::COLOR_TEXT);

        const uint32_t thread_rows_y = thread_table_y + 21;
        const uint32_t visible = height > thread_rows_y - y
            ? (height - (thread_rows_y - y)) / 20 : 0;
        for (uint32_t i = 0; i < count && i < visible; ++i)
        {
            const uint32_t row_y = thread_rows_y + i * 20;
            if ((i & 1U) == 0)
                user::gui::fill(x + 7, row_y, width > 14 ? width - 14 : width,
                                20, 0xFFD8D8D8);
            char number[12];
            uint32_t number_at = 0;
            number[0] = '\0';
            append_number(number, sizeof(number), &number_at,
                          rows[i].thread.process_id);
            user::gui::text(x + 10, row_y + 2, number, user::gui::COLOR_TEXT);
            number_at = 0;
            number[0] = '\0';
            append_number(number, sizeof(number), &number_at, rows[i].thread.id);
            user::gui::text(x + 48, row_y + 2, number, user::gui::COLOR_TEXT);
            user::gui::text(x + 88, row_y + 2, rows[i].thread.name,
                            user::gui::COLOR_TEXT);
            user::gui::text(x + 270, row_y + 2,
                            state_name(rows[i].thread.state), user::gui::COLOR_TEXT);
            char cpu[8];
            uint32_t cpu_at = 0;
            cpu[0] = '\0';
            append_number(cpu, sizeof(cpu), &cpu_at, rows[i].cpu_percent);
            append(cpu, sizeof(cpu), &cpu_at, "%");
            user::gui::text(x + 382, row_y + 2, cpu, user::gui::COLOR_TEXT);
            char stack[16];
            uint32_t stack_at = 0;
            stack[0] = '\0';
            append_number(stack, sizeof(stack), &stack_at,
                          rows[i].thread.stack_bytes / 1024);
            append(stack, sizeof(stack), &stack_at, " KB");
            user::gui::text(x + 446, row_y + 2, stack, user::gui::COLOR_TEXT);
        }
    }
}
