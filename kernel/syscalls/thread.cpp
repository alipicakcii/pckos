// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include "internal.hh"
#include <abi/thread.hh>
#include <kernel/task/thread.hh>
#include <kernel/task/user_mode.hh>

namespace kernel::syscalls
{
    static bool copy_thread_name(uint32_t user_name, char *destination,
                          uint32_t capacity)
    {
        if (user_name == 0 || capacity == 0 ||
            !active_virtual_memory->is_user_range(user_name, 1, false))
            return false;
        for (uint32_t i = 0; i + 1 < capacity; ++i)
        {
            if (!active_virtual_memory->is_user_range(user_name + i, 1, false))
                return false;
            destination[i] = reinterpret_cast<const char *>(user_name)[i];
            if (destination[i] == '\0')
                return true;
        }
        destination[capacity - 1] = '\0';
        return true;
    }

    void handle_thread(Registers *registers)
    {
        switch (registers->eax)
        {
            case abi::syscall_thread_yield:
                thread_yield();
                registers->eax = 0;
                return;

            case abi::syscall_thread_sleep:
                thread_sleep(registers->ebx);
                registers->eax = 0;
                return;

            case abi::syscall_thread_id:
                registers->eax = thread_current_id();
                return;

            case abi::syscall_thread_count:
                registers->eax = thread_active_count();
                return;

            case abi::syscall_thread_capacity:
                registers->eax = thread_slot_count();
                return;

            case abi::syscall_thread_switch_count:
                registers->eax = thread_switch_count();
                return;

            case abi::syscall_thread_create:
            {
                char name[24]{};
                if (!copy_thread_name(registers->edi, name, sizeof(name)))
                {
                    registers->eax = static_cast<uint32_t>(ERROR_BAD_ADDRESS);
                    return;
                }
                registers->eax = static_cast<uint32_t>(user_mode_create_thread(
                    registers->ebx, registers->ecx, registers->edx, name));
                return;
            }

            case abi::syscall_thread_exit:
                user_mode_thread_exit();

            case abi::syscall_thread_info:
            {
                if (!active_virtual_memory->is_user_range(registers->ecx,
                        sizeof(abi::ThreadInfo), true))
                {
                    registers->eax = static_cast<uint32_t>(ERROR_BAD_ADDRESS);
                    return;
                }
                ThreadInfo source{};
                if (!thread_get_info(registers->ebx, &source))
                {
                    registers->eax = static_cast<uint32_t>(ERROR_NO_ENTRY);
                    return;
                }
                auto *destination = reinterpret_cast<abi::ThreadInfo *>(registers->ecx);
                destination->id = source.id;
                destination->process_id = source.process_id;
                destination->state = source.state;
                destination->flags = source.flags;
                destination->cpu_ticks = source.cpu_ticks;
                destination->stack_bytes = source.stack_bytes;
                destination->cpu_id = source.cpu_id;
                for (uint32_t i = 0; i < sizeof(source.name); ++i)
                    destination->name[i] = source.name[i];
                registers->eax = 0;
                return;
            }

            case abi::syscall_thread_set_name:
            {
                char name[24]{};
                if (!copy_thread_name(registers->ebx, name, sizeof(name)))
                {
                    registers->eax = static_cast<uint32_t>(ERROR_BAD_ADDRESS);
                    return;
                }
                thread_set_current_name(name);
                registers->eax = 0;
                return;
            }

            default:
                registers->eax = static_cast<uint32_t>(ERROR_NOT_IMPLEMENTED);
                return;
        }
    }
}
