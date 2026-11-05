// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_USER_INPUT_HH
#define PCKOS_USER_INPUT_HH

#include <abi/input.hh>
#include <user/syscall.hh>

namespace user
{
    using abi::InputEvent;

    inline int32_t wait_event(InputEvent *event, uint32_t timeout_ticks = 0)
    {
        return call(syscall_wait_event, reinterpret_cast<uint32_t>(event), timeout_ticks);
    }
}

#endif
