// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_FIRMWARE_ACPI_HH
#define PCKOS_FIRMWARE_ACPI_HH

namespace firmware
{
    bool initialize_power_off();

    // ACPI S5 kapanışını başlatır; ACPI güç bilgisi yoksa false döndürür.
    bool power_off();
}

#endif
