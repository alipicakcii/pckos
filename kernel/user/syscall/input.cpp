// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <user/input/events.hh>
#include <user/syscall/call.hh>

namespace user
{
    int32_t wait_event(InputEvent *event, uint32_t timeout_ticks)
    {
        return call(syscall_wait_event,
                    reinterpret_cast<uint32_t>(event), timeout_ticks);
    }
}
