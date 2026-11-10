// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_USER_SYSTEM_POWER_HH
#define PCKOS_USER_SYSTEM_POWER_HH

#include <user/syscall/call.hh>

namespace user
{
    inline int32_t shutdown()
    {
        return call(syscall_power_off);
    }
}

#endif
