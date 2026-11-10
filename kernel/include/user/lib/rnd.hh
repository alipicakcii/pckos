// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_USER_RND_HH
#define PCKOS_USER_RND_HH

#include <stdint.h>
#include <user/lib/lock.hh>

namespace user::lib::random
{
    enum class Source : uint32_t
    {
        hardware,
        software
    };

    class Random
    {
    public:
        explicit Random(uint32_t seed = 0x6D2B79F5U);
        void seed(uint32_t value);
        uint32_t next(uint32_t max, Source source = Source::hardware);

    private:
        uint32_t next_software(uint32_t max);
        uint32_t next_hardware(uint32_t max);

        SpinLock lock_;
        uint32_t state_;
    };
}

#endif
