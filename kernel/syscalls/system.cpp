// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include "internal.hh"
#include <abi/system.hh>
#include <device/rtc.hh>
#include <kernel/thread.hh>
#include <library/kernel_log.hh>
#include <mem/alloc.hh>

namespace kernel::syscalls
{
    static void read_cpuid(uint32_t leaf, uint32_t subleaf,
                    uint32_t &eax, uint32_t &ebx,
                    uint32_t &ecx, uint32_t &edx)
    {
        __asm__ volatile("cpuid"
            : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
            : "a"(leaf), "c"(subleaf));
    }

    void handle_system(Registers *registers)
    {
        switch (registers->eax)
        {
            case abi::syscall_get_ticks:
                registers->eax = timer_get_ticks();
                return;

            case abi::syscall_get_datetime:
            {
                if (!active_virtual_memory->is_user_range(registers->ebx,
                        sizeof(abi::DateTime), true))
                {
                    registers->eax = static_cast<uint32_t>(ERROR_BAD_ADDRESS);
                    return;
                }
                device::RtcDateTime value{};
                if (!device::rtc_read(&value))
                {
                    registers->eax = static_cast<uint32_t>(ERROR_IO);
                    return;
                }
                auto *destination = reinterpret_cast<abi::DateTime *>(registers->ebx);
                destination->year = value.year;
                destination->month = value.month;
                destination->day = value.day;
                destination->hour = value.hour;
                destination->minute = value.minute;
                destination->second = value.second;
                registers->eax = 0;
                return;
            }

            case abi::syscall_system_info:
            {
                if (!active_virtual_memory->is_user_range(registers->ebx,
                        sizeof(abi::SystemInfo), true))
                {
                    registers->eax = static_cast<uint32_t>(ERROR_BAD_ADDRESS);
                    return;
                }
                abi::SystemInfo info{};
                info.total_pages = active_virtual_memory->get_total_page_count();
                info.free_pages = active_virtual_memory->get_free_page_count();
                info.kernel_heap_used = mem::MemAlloc::get_kernel_heap_used();
                info.ticks = thread_tick_count();
                info.switch_count = thread_switch_count();
                info.logical_cpu_count = 1;
                uint32_t eax = 0, ebx = 0, ecx = 0, edx = 0;
                read_cpuid(0, 0, eax, ebx, ecx, edx);
                const uint32_t maximum_leaf = eax;
                const uint32_t vendor_words[3] = {ebx, edx, ecx};
                auto *vendor = reinterpret_cast<uint8_t *>(info.cpu_vendor);
                for (uint32_t word = 0; word < 3; ++word)
                    for (uint32_t byte = 0; byte < 4; ++byte)
                        vendor[word * 4 + byte] =
                            static_cast<uint8_t>(vendor_words[word] >> (byte * 8));
                info.cpu_vendor[12] = '\0';
                if (maximum_leaf >= 1)
                {
                    read_cpuid(1, 0, eax, ebx, ecx, edx);
                    const uint32_t base_family = (eax >> 8) & 0xF;
                    const uint32_t base_model = (eax >> 4) & 0xF;
                    const uint32_t ext_family = (eax >> 20) & 0xFF;
                    const uint32_t ext_model = (eax >> 16) & 0xF;
                    info.cpu_family = base_family == 0xF
                        ? base_family + ext_family : base_family;
                    info.cpu_model = (base_family == 0x6 || base_family == 0xF)
                        ? (ext_model << 4) | base_model : base_model;
                }
                uint32_t extended_max = 0;
                read_cpuid(0x80000000U, 0, extended_max, ebx, ecx, edx);
                if (extended_max >= 0x80000004U)
                {
                    auto *brand = reinterpret_cast<uint8_t *>(info.cpu_brand);
                    for (uint32_t leaf = 0; leaf < 3; ++leaf)
                    {
                        uint32_t words[4]{};
                        read_cpuid(0x80000002U + leaf, 0,
                                   words[0], words[1], words[2], words[3]);
                        for (uint32_t word = 0; word < 4; ++word)
                            for (uint32_t byte = 0; byte < 4; ++byte)
                                brand[leaf * 16 + word * 4 + byte] =
                                    static_cast<uint8_t>(words[word] >> (byte * 8));
                    }
                    info.cpu_brand[48] = '\0';
                }
                auto *source = reinterpret_cast<const uint8_t *>(&info);
                auto *destination = reinterpret_cast<uint8_t *>(registers->ebx);
                for (uint32_t i = 0; i < sizeof(info); ++i)
                    destination[i] = source[i];
                registers->eax = 0;
                return;
            }

            case abi::syscall_read_kernel_log:
            {
                const uint32_t capacity = registers->ecx;
                if (capacity == 0 || capacity > MAX_SYSCALL_TRANSFER)
                    registers->eax = ERROR_INVALID_ARGUMENT;
                else if (!active_virtual_memory->is_user_range(registers->ebx, capacity, true) ||
                         !active_virtual_memory->is_user_range(registers->edx, sizeof(uint32_t), true))
                    registers->eax = ERROR_BAD_ADDRESS;
                else
                {
                    uint32_t cursor = *reinterpret_cast<uint32_t *>(registers->edx);
                    registers->eax = lib::kernel_log_read(
                        reinterpret_cast<char *>(registers->ebx), capacity, &cursor);
                    *reinterpret_cast<uint32_t *>(registers->edx) = cursor;
                }
                return;
            }

            case abi::syscall_get_kernel_log:
            {
                const uint32_t capacity = registers->ecx;
                if (capacity == 0 || capacity > MAX_SYSCALL_TRANSFER)
                {
                    registers->eax = static_cast<uint32_t>(ERROR_INVALID_ARGUMENT);
                    return;
                }
                if (!active_virtual_memory->is_user_range(registers->ebx,
                        capacity, true))
                {
                    registers->eax = static_cast<uint32_t>(ERROR_BAD_ADDRESS);
                    return;
                }
                registers->eax = static_cast<uint32_t>(lib::kernel_log_snapshot(
                    reinterpret_cast<char *>(registers->ebx), capacity));
                return;
            }

            default:
                registers->eax = static_cast<uint32_t>(ERROR_NOT_IMPLEMENTED);
                return;
        }
    }
}
