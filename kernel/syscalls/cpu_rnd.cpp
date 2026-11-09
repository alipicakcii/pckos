// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include "internal.hh"
#include <library/rnd.hh>
#include <library/kprint.hh>

namespace kernel::syscalls
{
    void handle_cpu_rnd(Registers *registers)
    {
        switch (registers->eax)
        {
        case abi::syscall_cpu_rnd:
        {
            const uint32_t max = registers->ebx;

            if (max < 0)
            {
                registers->eax = static_cast<uint32_t>(ERROR_INVALID_ARGUMENT);
                return;
            }
            uint32_t rnd = lib::rand(max);
            registers->eax = rnd;
            return;
        }

        default:
            registers->eax = static_cast<uint32_t>(ERROR_NOT_IMPLEMENTED);
            return;
        }
    }
}