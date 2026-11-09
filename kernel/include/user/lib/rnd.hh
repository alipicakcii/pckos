// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_USER_RND_HH
#define PCKOS_USER_RND_HH

#include <user/syscall.hh>

namespace user::lib
{
    // 1...max arasında sayı; geçersiz sınır, destek yokluğu veya üretim hatasında 0.
    inline uint32_t rand(uint32_t max)
    {
        return static_cast<uint32_t>(call(syscall_cpu_rnd, max));
    }

}

#endif
