// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include "internal.hh"
#include <device/pc_speaker.hh>

namespace kernel::syscalls
{
    void handle_audio(Registers *registers)
    {
        switch (registers->eax)
        {
            case abi::syscall_pc_speaker:
            {
                const uint32_t frequency = registers->ebx;
                const uint32_t duration_ms = registers->ecx;
                if (frequency < 20 || frequency > 20000 ||
                    duration_ms == 0 || duration_ms > 2000)
                {
                    registers->eax = static_cast<uint32_t>(ERROR_INVALID_ARGUMENT);
                    return;
                }
                device::play_note(frequency, duration_ms);
                registers->eax = 0;
                return;
            }

            default:
                registers->eax = static_cast<uint32_t>(ERROR_NOT_IMPLEMENTED);
                return;
        }
    }
}
