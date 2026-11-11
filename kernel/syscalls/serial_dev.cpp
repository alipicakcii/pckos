// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include "internal.hh"
#include <device/serial_device.hh>

namespace kernel::syscalls
{
    void handle_serialdev_write(Registers *registers)
    {
        switch (registers->eax)
        {
        case abi::sysycall_serial_dev_write:
        {
            const char *string = reinterpret_cast<const char *>(registers->ebx);
            if (string == nullptr)
            {
                registers->eax = static_cast<uint32_t>(ERROR_SERIAL_DEV_WRITE);
                return;
            }

            device::serial_write_text(string);
            registers->eax = 0;
            return;
        }

        default:
            registers->eax = static_cast<uint32_t>(ERROR_NOT_IMPLEMENTED);
            return;
        }
    }
}
