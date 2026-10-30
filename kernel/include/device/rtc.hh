// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_DEVICE_RTC_HH
#define PCKOS_DEVICE_RTC_HH

#include <stdint.h>

namespace device
{
    struct RtcDateTime
    {
        uint32_t year, month, day, hour, minute, second;
    };

    bool rtc_read(RtcDateTime *result);
}

#endif
