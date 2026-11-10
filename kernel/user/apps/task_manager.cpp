// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <user/task/process.hh>
#include <user/system/info.hh>
#include <user/task/thread.hh>
#include <user/apps/task_manager.hh>
#include <user/gui/drawing.hh>
#include <user/memory/new.hh>

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

        uint32_t cpu_percent(uint32_t busy, uint32_t elapsed, uint32_t processors)
        {
            if (elapsed == 0) return 0;
            if (processors == 0) processors = 1;
            while (elapsed > UINT32_MAX / 100)
            {
                elapsed >>= 1;
                busy >>= 1;
            }
            const uint32_t whole = busy / elapsed;
            if (whole >= processors) return 100;
            return (whole * 100 + ((busy % elapsed) * 100) / elapsed) / processors;
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
        : lock_{}, system_{}, processes_(nullptr), process_count_(0),
          process_capacity_(0), previous_process_ids_(nullptr),
          previous_process_ticks_(nullptr), rows_(nullptr), row_count_(0),
          row_capacity_(0), scroll_offset_(0), visible_rows_(0), previous_ticks_(0),
          total_cpu_percent_(0), baseline_ready_(false),
          previous_cpu_ticks_(nullptr), dirty_(0)
    {
        refresh();
    }

    bool TaskManager::ensure_process_capacity(uint32_t capacity)
    {
        if (capacity <= process_capacity_) return true;
        uint32_t next = process_capacity_ == 0 ? 8 : process_capacity_;
        while (next < capacity)
        {
            if (next > UINT32_MAX / 2) return false;
            next *= 2;
        }
        auto *processes = new (std::nothrow) ProcessRow[next];
        auto *ids = new (std::nothrow) uint32_t[next];
        auto *ticks = new (std::nothrow) uint32_t[next];
        if (processes == nullptr || ids == nullptr || ticks == nullptr)
        {
            delete[] processes;
            delete[] ids;
            delete[] ticks;
            return false;
        }
        bool already_large_enough = false;
        {
            LockGuard guard(lock_);
            if (capacity <= process_capacity_)
                already_large_enough = true;
            else for (uint32_t i = 0; i < process_capacity_; ++i)
            {
                processes[i] = processes_[i];
                ids[i] = previous_process_ids_[i];
                ticks[i] = previous_process_ticks_[i];
            }
        }
        if (already_large_enough)
        {
            delete[] processes;
            delete[] ids;
            delete[] ticks;
            return true;
        }
        ProcessRow *old_processes;
        uint32_t *old_ids;
        uint32_t *old_ticks;
        {
            LockGuard guard(lock_);
            old_processes = processes_;
            old_ids = previous_process_ids_;
            old_ticks = previous_process_ticks_;
            processes_ = processes;
            previous_process_ids_ = ids;
            previous_process_ticks_ = ticks;
            process_capacity_ = next;
        }
        delete[] old_processes;
        delete[] old_ids;
        delete[] old_ticks;
        return true;
    }

    bool TaskManager::ensure_thread_capacity(uint32_t capacity)
    {
        if (capacity <= row_capacity_) return true;
        uint32_t next = row_capacity_ == 0 ? 16 : row_capacity_;
        while (next < capacity)
        {
            if (next > UINT32_MAX / 2) return false;
            next *= 2;
        }
        auto *rows = new (std::nothrow) Row[next];
        auto *ticks = new (std::nothrow) uint32_t[next];
        if (rows == nullptr || ticks == nullptr)
        {
            delete[] rows;
            delete[] ticks;
            return false;
        }
        bool already_large_enough = false;
        {
            LockGuard guard(lock_);
            if (capacity <= row_capacity_)
                already_large_enough = true;
            else for (uint32_t i = 0; i < row_capacity_; ++i)
            {
                rows[i] = rows_[i];
                ticks[i] = previous_cpu_ticks_[i];
            }
        }
        if (already_large_enough)
        {
            delete[] rows;
            delete[] ticks;
            return true;
        }
        Row *old_rows;
        uint32_t *old_ticks;
        {
            LockGuard guard(lock_);
            old_rows = rows_;
            old_ticks = previous_cpu_ticks_;
            rows_ = rows;
            previous_cpu_ticks_ = ticks;
            row_capacity_ = next;
        }
        delete[] old_rows;
        delete[] old_ticks;
        return true;
    }

    void TaskManager::refresh()
    {
        user::SystemInfo system{};
        if (user::get_system_info(&system) < 0)
            return;

        const uint32_t thread_capacity = user::thread_capacity();
        uint32_t count = 0;
        uint32_t total_cpu_ticks = 0;
        const uint32_t elapsed = system.ticks - previous_ticks_;
        const uint32_t process_count = user::process_count();
        if (!ensure_process_capacity(process_count) ||
            !ensure_thread_capacity(thread_capacity))
            return;
        auto *processes = new (std::nothrow) ProcessRow[process_count]{};
        auto *rows = new (std::nothrow) Row[thread_capacity]{};
        if ((process_count != 0 && processes == nullptr) ||
            (thread_capacity != 0 && rows == nullptr))
        {
            delete[] processes;
            delete[] rows;
            return;
        }
        uint32_t process_rows = 0;
        for (uint32_t index = 0; index < process_count; ++index)
        {
            user::ProcessInfo info{};
            if (user::process_get_info(index, &info) < 0)
                continue;
            const uint32_t delta = !baseline_ready_ ? 0
                : info.id == previous_process_ids_[process_rows] &&
                  info.cpu_ticks >= previous_process_ticks_[process_rows]
                    ? info.cpu_ticks - previous_process_ticks_[process_rows]
                    : info.cpu_ticks;
            processes[process_rows].process = info;
            processes[process_rows].cpu_percent = cpu_percent(delta, elapsed, system.logical_cpu_count);
            if (processes[process_rows].cpu_percent > 100) processes[process_rows].cpu_percent = 100;
            previous_process_ticks_[process_rows] = info.cpu_ticks;
            previous_process_ids_[process_rows] = info.id;
            ++process_rows;
        }
        for (uint32_t id = 0; id < thread_capacity; ++id)
        {
            user::ThreadInfo info{};
            if (user::thread_get_info(id, &info) < 0)
                continue;
            const uint32_t delta = !baseline_ready_ ? 0
                : info.cpu_ticks >= previous_cpu_ticks_[id]
                    ? info.cpu_ticks - previous_cpu_ticks_[id]
                    : info.cpu_ticks;
            if ((info.flags & 1U) == 0)
                total_cpu_ticks += delta;
            rows[count].thread = info;
            rows[count].cpu_percent = cpu_percent(delta, elapsed, system.logical_cpu_count);
            if (rows[count].cpu_percent > 100)
                rows[count].cpu_percent = 100;
            previous_cpu_ticks_[id] = info.cpu_ticks;
            ++count;
        }
        const uint32_t total_cpu_delta = baseline_ready_ ? total_cpu_ticks : 0;
        previous_ticks_ = system.ticks;
        baseline_ready_ = true;
        const uint32_t total_cpu_percent = cpu_percent(
            total_cpu_delta, elapsed, system.logical_cpu_count);

        {
            LockGuard guard(lock_);
            system_ = system;
            total_cpu_percent_ = total_cpu_percent;
            process_count_ = process_rows;
            for (uint32_t i = 0; i < process_rows; ++i)
                processes_[i] = processes[i];
            row_count_ = count;
            for (uint32_t i = 0; i < count; ++i)
                rows_[i] = rows[i];
            dirty_ = 1;
        }
        delete[] processes;
        delete[] rows;
    }

    bool TaskManager::consume_dirty()
    {
        LockGuard guard(lock_);
        const bool dirty = dirty_ != 0;
        dirty_ = 0;
        return dirty;
    }

    void TaskManager::scroll(int32_t rows)
    {
        LockGuard guard(lock_);
        const uint32_t maximum = row_count_ > visible_rows_
            ? row_count_ - visible_rows_ : 0;
        if (rows < 0)
        {
            const uint32_t amount = static_cast<uint32_t>(-rows);
            scroll_offset_ = amount > scroll_offset_ ? 0 : scroll_offset_ - amount;
        }
        else
        {
            const uint32_t amount = static_cast<uint32_t>(rows);
            scroll_offset_ = amount > maximum - (scroll_offset_ < maximum
                ? scroll_offset_ : maximum)
                ? maximum : scroll_offset_ + amount;
        }
        dirty_ = 1;
    }

    void TaskManager::scroll_to_top()
    {
        LockGuard guard(lock_);
        scroll_offset_ = 0;
        dirty_ = 1;
    }

    void TaskManager::scroll_to_bottom()
    {
        LockGuard guard(lock_);
        scroll_offset_ = row_count_ > visible_rows_
            ? row_count_ - visible_rows_ : 0;
        dirty_ = 1;
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
        uint32_t process_count = 0;
        uint32_t count = 0;
        uint32_t total_cpu_percent = 0;
        uint32_t scroll_offset = 0;
        {
            LockGuard guard(lock_);
            system = system_;
            total_cpu_percent = total_cpu_percent_;
            process_count = process_count_;
            count = row_count_;
            scroll_offset = scroll_offset_;
        }
        auto *processes = new (std::nothrow) ProcessRow[process_count];
        auto *rows = new (std::nothrow) Row[count];
        if ((process_count != 0 && processes == nullptr) ||
            (count != 0 && rows == nullptr))
        {
            delete[] processes;
            delete[] rows;
            return;
        }
        {
            LockGuard guard(lock_);
            if (process_count > process_capacity_ || count > row_capacity_)
            {
                process_count = 0;
                count = 0;
            }
            else
            {
                for (uint32_t i = 0; i < process_count; ++i)
                    processes[i] = processes_[i];
                for (uint32_t i = 0; i < count; ++i)
                    rows[i] = rows_[i];
            }
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
        append(processor, sizeof(processor), &at, " | ");
        append_number(processor, sizeof(processor), &at, system.logical_cpu_count);
        append(processor, sizeof(processor), &at, " islemci | i386");
        user::gui::text(x + 8, y + 27, processor, user::gui::COLOR_TEXT);

        char counts[96] = "Surec: ";
        at = 7;
        append_number(counts, sizeof(counts), &at, process_count);
        append(counts, sizeof(counts), &at, " | Thread: ");
        append_number(counts, sizeof(counts), &at, count);
        append(counts, sizeof(counts), &at, " | Zamanlayici gecisi: ");
        append_number(counts, sizeof(counts), &at, system.switch_count);
        append(counts, sizeof(counts), &at, " | CPU yuk: ");
        append_number(counts, sizeof(counts), &at,
                      total_cpu_percent);
        append(counts, sizeof(counts), &at, "%");
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
        user::gui::frame(x + 6, thread_table_y, width > 12 ? width - 12 : width,
                         20, 0xFFB0B0B0, true);
        user::gui::text(x + 10, thread_table_y + 2, "PID", user::gui::COLOR_TEXT);
        user::gui::text(x + 48, thread_table_y + 2, "TID", user::gui::COLOR_TEXT);
        user::gui::text(x + 88, thread_table_y + 2, "Thread", user::gui::COLOR_TEXT);
        user::gui::text(x + 270, thread_table_y + 2, "Durum", user::gui::COLOR_TEXT);
        user::gui::text(x + 382, thread_table_y + 2, "CPU%", user::gui::COLOR_TEXT);
        user::gui::text(x + 426, thread_table_y + 2, "CPU", user::gui::COLOR_TEXT);
        user::gui::text(x + 486, thread_table_y + 2, "Yigin", user::gui::COLOR_TEXT);

        const uint32_t thread_rows_y = thread_table_y + 21;
        const uint32_t visible = height > thread_rows_y - y
            ? (height - (thread_rows_y - y)) / 20 : 0;
        const uint32_t maximum_offset = count > visible ? count - visible : 0;
        if (scroll_offset > maximum_offset)
            scroll_offset = maximum_offset;
        {
            LockGuard guard(lock_);
            visible_rows_ = visible;
            scroll_offset_ = scroll_offset;
        }
        char thread_title[80] = "Thread'ler ";
        uint32_t title_at = 11;
        append_number(thread_title, sizeof(thread_title), &title_at,
                      count == 0 ? 0 : scroll_offset + 1);
        append(thread_title, sizeof(thread_title), &title_at, "-");
        append_number(thread_title, sizeof(thread_title), &title_at,
                      scroll_offset + (count - scroll_offset < visible
                          ? count - scroll_offset : visible));
        append(thread_title, sizeof(thread_title), &title_at, "/");
        append_number(thread_title, sizeof(thread_title), &title_at, count);
        append(thread_title, sizeof(thread_title), &title_at,
               "  Oklar/PgUp/PgDn/Home/End");
        user::gui::text(x + 8, thread_table_y - 16, thread_title,
                        user::gui::COLOR_TEXT);
        for (uint32_t i = 0; i < visible && scroll_offset + i < count; ++i)
        {
            const uint32_t row_y = thread_rows_y + i * 20;
            if ((i & 1U) == 0)
                user::gui::fill(x + 7, row_y, width > 14 ? width - 14 : width,
                                20, 0xFFD8D8D8);
            char number[12];
            uint32_t number_at = 0;
            number[0] = '\0';
            append_number(number, sizeof(number), &number_at,
                          rows[scroll_offset + i].thread.process_id);
            user::gui::text(x + 10, row_y + 2, number, user::gui::COLOR_TEXT);
            number_at = 0;
            number[0] = '\0';
            append_number(number, sizeof(number), &number_at,
                          rows[scroll_offset + i].thread.id);
            user::gui::text(x + 48, row_y + 2, number, user::gui::COLOR_TEXT);
            user::gui::text(x + 88, row_y + 2, rows[scroll_offset + i].thread.name,
                            user::gui::COLOR_TEXT);
            user::gui::text(x + 270, row_y + 2,
                            state_name(rows[scroll_offset + i].thread.state), user::gui::COLOR_TEXT);
            char cpu[8];
            uint32_t cpu_at = 0;
            cpu[0] = '\0';
            append_number(cpu, sizeof(cpu), &cpu_at,
                          rows[scroll_offset + i].cpu_percent);
            append(cpu, sizeof(cpu), &cpu_at, "%");
            user::gui::text(x + 382, row_y + 2, cpu, user::gui::COLOR_TEXT);
            char processor[12];
            uint32_t processor_at = 0;
            processor[0] = '\0';
            if (rows[scroll_offset + i].thread.cpu_id == UINT32_MAX)
                append(processor, sizeof(processor), &processor_at, "-");
            else
                append_number(processor, sizeof(processor), &processor_at,
                              rows[scroll_offset + i].thread.cpu_id);
            user::gui::text(x + 426, row_y + 2, processor, user::gui::COLOR_TEXT);
            char stack[16];
            uint32_t stack_at = 0;
            stack[0] = '\0';
            append_number(stack, sizeof(stack), &stack_at,
                          rows[scroll_offset + i].thread.stack_bytes / 1024);
            append(stack, sizeof(stack), &stack_at, " KB");
            user::gui::text(x + 486, row_y + 2, stack, user::gui::COLOR_TEXT);
        }
        delete[] processes;
        delete[] rows;
    }
}
