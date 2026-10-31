// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <library/kernel_log.hh>
#include <stdint.h>

namespace lib
{
    namespace
    {
        char log_buffer[KERNEL_LOG_CAPACITY];
        size_t log_start = 0;
        size_t log_size = 0;
        uint32_t log_sequence = 0;

        uint32_t disable_interrupts_and_save_flags()
        {
            uint32_t flags;
            asm volatile("pushfl; popl %0; cli" : "=r"(flags) : : "memory");
            return flags;
        }

        void restore_interrupts(uint32_t flags)
        {
            if ((flags & (1U << 9)) != 0)
                asm volatile("sti" : : : "memory");
        }
    }

    void kernel_log_write_char(char character)
    {
        const uint32_t flags = disable_interrupts_and_save_flags();

        if (log_size < KERNEL_LOG_CAPACITY)
        {
            const size_t index = (log_start + log_size) % KERNEL_LOG_CAPACITY;
            log_buffer[index] = character;
            ++log_size;
        }
        else
        {
            log_buffer[log_start] = character;
            log_start = (log_start + 1) % KERNEL_LOG_CAPACITY;
        }

        ++log_sequence;
        restore_interrupts(flags);
    }

    size_t kernel_log_read(char *destination, size_t capacity, uint32_t *cursor)
    {
        if (destination == nullptr || cursor == nullptr || capacity == 0) return 0;
        const uint32_t flags = disable_interrupts_and_save_flags();
        uint32_t pending = log_sequence - *cursor;
        if (pending > log_size) pending = static_cast<uint32_t>(log_size);
        const size_t copied = pending < capacity ? pending : capacity;
        const size_t start = (log_start + log_size - pending) % KERNEL_LOG_CAPACITY;
        for (size_t i = 0; i < copied; ++i)
            destination[i] = log_buffer[(start + i) % KERNEL_LOG_CAPACITY];
        *cursor = log_sequence - pending + static_cast<uint32_t>(copied);
        restore_interrupts(flags);
        return copied;
    }

    size_t kernel_log_snapshot(char *destination, size_t capacity)
    {
        if (destination == nullptr || capacity == 0)
            return 0;

        const uint32_t flags = disable_interrupts_and_save_flags();
        const size_t available = capacity - 1;
        const size_t copied = log_size < available ? log_size : available;
        const size_t skipped = log_size - copied;

        for (size_t i = 0; i < copied; ++i)
        {
            const size_t index = (log_start + skipped + i) % KERNEL_LOG_CAPACITY;
            destination[i] = log_buffer[index];
        }
        destination[copied] = '\0';

        restore_interrupts(flags);
        return copied;
    }
}
