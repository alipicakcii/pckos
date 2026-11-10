// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include "internal.hh"
#include <abi/io.hh>
#include <kernel/task/user_mode.hh>

namespace kernel::syscalls
{
    void handle_io(Registers *registers)
    {
        switch (registers->eax)
        {
            case abi::syscall_write:
            {
                if (registers->ebx != abi::stdout_descriptor &&
                    registers->ebx != abi::stderr_descriptor)
                {
                    registers->eax = static_cast<uint32_t>(ERROR_BAD_DESCRIPTOR);
                    return;
                }
                const uint32_t length = registers->edx;
                if (length > MAX_SYSCALL_TRANSFER)
                {
                    registers->eax = static_cast<uint32_t>(ERROR_INVALID_ARGUMENT);
                    return;
                }
                if (length != 0 && !active_virtual_memory->is_user_range(
                        registers->ecx, length, false))
                {
                    registers->eax = static_cast<uint32_t>(ERROR_BAD_ADDRESS);
                    return;
                }
                const char *buffer = reinterpret_cast<const char *>(registers->ecx);
                registers->eax = static_cast<uint32_t>(
                    user_mode_stream_write(buffer, length));
                return;
            }

            case abi::syscall_read:
                if (registers->ebx == abi::stdin_descriptor)
                    registers->eax = static_cast<uint32_t>(read_input(
                        registers->ecx, registers->edx));
                else if (registers->ebx >= 3)
                    registers->eax = static_cast<uint32_t>(read_file(
                        registers->ebx, registers->ecx, registers->edx));
                else
                    registers->eax = static_cast<uint32_t>(ERROR_BAD_DESCRIPTOR);
                return;

            case abi::syscall_terminal_interrupt:
                registers->eax = user_mode_terminal_interrupt(registers->ebx);
                return;

            case abi::syscall_terminal_attach:
                registers->eax = user_mode_terminal_attach();
                return;

            case abi::syscall_terminal_read:
                if (registers->ecx > MAX_SYSCALL_TRANSFER)
                    registers->eax = ERROR_INVALID_ARGUMENT;
                else if (registers->ecx != 0 && !active_virtual_memory->is_user_range(
                        registers->ebx, registers->ecx, true))
                    registers->eax = ERROR_BAD_ADDRESS;
                else
                    registers->eax = user_mode_terminal_read(
                        reinterpret_cast<char *>(registers->ebx), registers->ecx);
                return;

            case abi::syscall_terminal_send:
                if (registers->edx > MAX_SYSCALL_TRANSFER)
                    registers->eax = ERROR_INVALID_ARGUMENT;
                else if (registers->edx != 0 && !active_virtual_memory->is_user_range(
                        registers->ecx, registers->edx, false))
                    registers->eax = ERROR_BAD_ADDRESS;
                else
                    registers->eax = user_mode_terminal_send(registers->ebx,
                        reinterpret_cast<const char *>(registers->ecx), registers->edx);
                return;

            default:
                registers->eax = static_cast<uint32_t>(ERROR_NOT_IMPLEMENTED);
                return;
        }
    }
}
