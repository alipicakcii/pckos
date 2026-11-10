// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include "internal.hh"
#include <abi/process.hh>
#include <kernel/task/thread.hh>
#include <kernel/task/user_mode.hh>

namespace kernel::syscalls
{
    void handle_process(Registers *registers)
    {
        switch (registers->eax)
        {
            case abi::syscall_exit:
                user_mode_exit(registers, static_cast<int32_t>(registers->ebx));
                return;

            case abi::syscall_process_count:
                registers->eax = process_active_count();
                return;

            case abi::syscall_process_info:
            {
                if (!active_virtual_memory->is_user_range(registers->ecx,
                        sizeof(abi::ProcessInfo), true))
                {
                    registers->eax = static_cast<uint32_t>(ERROR_BAD_ADDRESS);
                    return;
                }
                ProcessInfo source{};
                if (!process_get_info(registers->ebx, &source))
                {
                    registers->eax = static_cast<uint32_t>(ERROR_NO_ENTRY);
                    return;
                }
                auto *destination = reinterpret_cast<abi::ProcessInfo *>(registers->ecx);
                destination->id = source.id;
                destination->parent_id = source.parent_id;
                destination->main_thread_id = source.main_thread_id;
                destination->thread_count = source.thread_count;
                destination->cpu_ticks = source.cpu_ticks;
                for (uint32_t i = 0; i < sizeof(source.name); ++i)
                    destination->name[i] = source.name[i];
                registers->eax = 0;
                return;
            }

            case abi::syscall_process_spawn:
            {
                char path[MAX_USER_PATH];
                char name[24];
                if (!copy_user_string(registers->ebx, path, sizeof(path)) ||
                    !copy_user_string(registers->ecx, name, sizeof(name)))
                {
                    registers->eax = static_cast<uint32_t>(ERROR_BAD_ADDRESS);
                    return;
                }
                registers->eax = static_cast<uint32_t>(user_mode_spawn_process(
                    path, name));
                return;
            }

            case abi::syscall_process_minimize:
                registers->eax = process_minimize(thread_current_process_id())
                    ? 0 : static_cast<uint32_t>(ERROR_NO_ENTRY);
                return;

            case abi::syscall_process_is_minimized:
                registers->eax = process_is_minimized(registers->ebx != 0
                    ? registers->ebx : thread_current_process_id()) ? 1 : 0;
                return;

            case abi::syscall_process_focus:
                registers->eax = process_focus(thread_current_process_id(),
                                               registers->ebx)
                    ? 0 : static_cast<uint32_t>(ERROR_PERMISSION);
                return;

            case abi::syscall_process_focused_id:
                registers->eax = process_input_focus();
                return;

            default:
                registers->eax = static_cast<uint32_t>(ERROR_NOT_IMPLEMENTED);
                return;
        }
    }
}
