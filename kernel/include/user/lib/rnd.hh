// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_USER_RND_HH
#define PCKOS_USER_RND_HH

#include <user/syscall.hh>

namespace user::lib
{
    inline int32_t rand(uint32_t max)
    {
        return call(syscall_cpu_rnd, max);
    }

    
}

#endif
