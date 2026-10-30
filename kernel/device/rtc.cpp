// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <device/rtc.hh>
#include <cpu/io.hh>

namespace device
{
    namespace
    {
        uint8_t reg(uint8_t index)
        {
            cpu::outb(0x70, static_cast<uint8_t>(index | 0x80));
            return cpu::inb(0x71);
        }

        bool ready()
        {
            for (uint32_t attempt = 0; attempt < 100000; ++attempt)
                if ((reg(0x0A) & 0x80) == 0)
                    return true;
            return false;
        }

        uint8_t decimal(uint8_t value)
        {
            return static_cast<uint8_t>((value & 0x0F) + (value >> 4) * 10);
        }

        struct Snapshot
        {
            uint8_t year, month, day, hour, minute, second, status;
        };

        Snapshot sample()
        {
            return {reg(0x09), reg(0x08), reg(0x07), reg(0x04),
                    reg(0x02), reg(0x00), reg(0x0B)};
        }

        bool same(const Snapshot &a, const Snapshot &b)
        {
            return a.year == b.year && a.month == b.month && a.day == b.day &&
                   a.hour == b.hour && a.minute == b.minute &&
                   a.second == b.second && a.status == b.status;
        }
    }

    bool rtc_read(RtcDateTime *result)
    {
        if (result == nullptr)
            return false;
        struct RestoreNmi
        {
            ~RestoreNmi() { cpu::outb(0x70, 0x00); }
        } restore_nmi;
        Snapshot previous{};
        for (uint32_t attempt = 0; attempt < 8; ++attempt)
        {
            if (!ready()) return false;
            const Snapshot current = sample();
            if (attempt != 0 && same(previous, current))
            {
                const bool binary = (current.status & 0x04) != 0;
                const bool hour24 = (current.status & 0x02) != 0;
                const bool pm = (current.hour & 0x80) != 0;
                const uint8_t raw_hour = current.hour & 0x7F;
                const uint32_t year = binary ? current.year : decimal(current.year);
                result->year = (year < 70 ? 2000 : 1900) + year;
                result->month = binary ? current.month : decimal(current.month);
                result->day = binary ? current.day : decimal(current.day);
                result->hour = binary ? raw_hour : decimal(raw_hour);
                result->minute = binary ? current.minute : decimal(current.minute);
                result->second = binary ? current.second : decimal(current.second);
                if (!hour24)
                    result->hour = (result->hour % 12) + (pm ? 12 : 0);
                return result->month >= 1 && result->month <= 12 &&
                       result->day >= 1 && result->day <= 31 &&
                       result->hour <= 23 && result->minute <= 59 &&
                       result->second <= 59;
            }
            previous = current;
        }
        return false;
    }
}
