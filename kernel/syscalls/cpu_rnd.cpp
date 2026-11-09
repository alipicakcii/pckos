// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include "internal.hh"
#include <library/rnd.hh>

namespace kernel::syscalls
{
    void handle_cpu_rnd(Registers *registers)
    {
        switch (registers->eax)
        {
        case abi::syscall_cpu_rnd:
        {
            const uint32_t max = registers->ebx;

            registers->eax = lib::rand(max);
            return;
        }

        default:
            registers->eax = static_cast<uint32_t>(ERROR_NOT_IMPLEMENTED);
            return;
        }
    }
}
