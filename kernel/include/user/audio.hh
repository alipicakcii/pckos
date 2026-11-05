// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_USER_AUDIO_HH
#define PCKOS_USER_AUDIO_HH

#include <user/syscall.hh>

namespace user
{
    inline int32_t pc_speaker_play(uint32_t frequency, uint32_t duration_ms)
    {
        return call(syscall_pc_speaker, frequency, duration_ms);
    }
}

#endif
