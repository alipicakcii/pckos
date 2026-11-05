// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_DEVICE_PC_SPEKAKER_HH
#define PCKOS_DEVICE_PC_SPEKAKER_HH

#include <stdint.h>

namespace device
{

    struct Note
    {
        uint32_t freq;
        uint32_t duration;
    };


    void play_note(uint32_t frequency, uint32_t duration_ms);
    void stop();
}

#endif
