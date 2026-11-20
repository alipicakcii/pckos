// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <user/io/serial_dev.hh>
#include <user/syscall/call.hh>

namespace user::io::serialdev
{
    int32_t write(const char *string)
    {
        return call(sysycall_serial_dev_write,
                    reinterpret_cast<uint32_t>(string));
    }
}
