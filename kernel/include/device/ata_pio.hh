// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_DEVICE_ATA_PIO_HH
#define PCKOS_DEVICE_ATA_PIO_HH

#include <stdint.h>

namespace device
{
    class AtaPioDisk
    {
    public:
        AtaPioDisk();

        bool initialize();
        bool read_sector(uint32_t lba, uint8_t *buffer) const;
        bool write_sector(uint32_t lba, const uint8_t *buffer) const;
        bool is_ready() const { return ready_; }
        uint32_t sector_count() const { return sector_count_; }

    private:
        bool wait_ready(bool data_request) const;
        void io_delay() const;

        uint16_t io_base_;
        uint16_t control_port_;
        uint8_t drive_select_;
        uint32_t sector_count_;
        bool ready_;
    };
}

#endif
