// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <library/kernel_log.hh>
#include <device/serial_device.hh>
#include <stdint.h>

namespace lib
{
    namespace
    {
        char log_buffer[KERNEL_LOG_CAPACITY];
        size_t log_start = 0;
        size_t log_size = 0;

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

        device::serial_write_char(character);
        restore_interrupts(flags);
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
