// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_USER_AUDIO_HH
#define PCKOS_USER_AUDIO_HH

#include <user/syscall/call.hh>

namespace user
{
    int32_t pc_speaker_play(uint32_t frequency, uint32_t duration_ms);
}

#endif
