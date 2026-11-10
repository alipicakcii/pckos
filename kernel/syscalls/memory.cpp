// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include "internal.hh"
#include <kernel/task/user_mode.hh>
#include <mem/alloc.hh>

namespace kernel::syscalls
{
    void handle_memory(Registers *registers)
    {
        switch (registers->eax)
        {
            case abi::syscall_get_heap_usage:
                registers->eax = mem::MemAlloc::get_kernel_heap_used();
                return;

            case abi::syscall_heap_alloc:
                registers->eax = static_cast<uint32_t>(user_mode_heap_alloc(
                    registers->ebx, registers->ecx));
                return;

            case abi::syscall_heap_free:
                registers->eax = static_cast<uint32_t>(user_mode_heap_free(registers->ebx));
                return;

            default:
                registers->eax = static_cast<uint32_t>(ERROR_NOT_IMPLEMENTED);
                return;
        }
    }
}
