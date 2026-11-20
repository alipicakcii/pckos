// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <user/io/stream.hh>
#include <user/syscall/call.hh>

namespace user
{
    int32_t write(int descriptor, const void *buffer, uint32_t length)
    {
        return call(syscall_write, static_cast<uint32_t>(descriptor),
                    reinterpret_cast<uint32_t>(buffer), length);
    }

    int32_t read(int descriptor, void *buffer, uint32_t length)
    {
        return call(syscall_read, static_cast<uint32_t>(descriptor),
                    reinterpret_cast<uint32_t>(buffer), length);
    }

    int32_t terminal_attach()
    {
        return call(syscall_terminal_attach);
    }

    int32_t terminal_read(void *buffer, uint32_t length)
    {
        return call(syscall_terminal_read,
                    reinterpret_cast<uint32_t>(buffer), length);
    }

    int32_t terminal_interrupt(uint32_t process_id)
    {
        return call(syscall_terminal_interrupt, process_id);
    }

    int32_t terminal_send(uint32_t process_id, const void *buffer,
                          uint32_t length)
    {
        return call(syscall_terminal_send, process_id,
                    reinterpret_cast<uint32_t>(buffer), length);
    }
}
