// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <user/device/audio.hh>
#include <user/syscall/call.hh>

namespace user
{
    int32_t pc_speaker_play(uint32_t frequency, uint32_t duration_ms)
    {
        return call(syscall_pc_speaker, frequency, duration_ms);
    }
}
