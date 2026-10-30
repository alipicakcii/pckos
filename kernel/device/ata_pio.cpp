// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <device/ata_pio.hh>
#include <cpu/io.hh>

namespace device
{
    namespace
    {
        constexpr uint32_t SECTOR_SIZE = 512;
        constexpr uint8_t STATUS_ERR = 0x01;
        constexpr uint8_t STATUS_DRQ = 0x08;
        constexpr uint8_t STATUS_DF = 0x20;
        constexpr uint8_t STATUS_BSY = 0x80;
        constexpr uint32_t WAIT_LIMIT = 1000000;

        uint16_t inw(uint16_t port)
        {
            uint16_t value;
            asm volatile("inw %1, %0" : "=a"(value) : "Nd"(port));
            return value;
        }

        void outw(uint16_t port, uint16_t value)
        {
            asm volatile("outw %0, %1" : : "a"(value), "Nd"(port));
        }
    }

    AtaPioDisk::AtaPioDisk()
        : io_base_(0x1F0), control_port_(0x3F6), drive_select_(0xE0),
          sector_count_(0), ready_(false)
    {
    }

    void AtaPioDisk::io_delay() const
    {
        cpu::inb(control_port_);
        cpu::inb(control_port_);
        cpu::inb(control_port_);
        cpu::inb(control_port_);
    }

    bool AtaPioDisk::wait_ready(bool data_request) const
    {
        for (uint32_t attempt = 0; attempt < WAIT_LIMIT; ++attempt)
        {
            const uint8_t status = cpu::inb(io_base_ + 7);
            if (status == 0 || status == 0xFF || (status & (STATUS_ERR | STATUS_DF)))
                return false;
            if ((status & STATUS_BSY) == 0 &&
                (!data_request || (status & STATUS_DRQ) != 0))
                return true;
        }
        return false;
    }

    bool AtaPioDisk::initialize()
    {
        ready_ = false;
        sector_count_ = 0;

        cpu::outb(io_base_ + 6, drive_select_);
        io_delay();
        cpu::outb(io_base_ + 2, 0);
        cpu::outb(io_base_ + 3, 0);
        cpu::outb(io_base_ + 4, 0);
        cpu::outb(io_base_ + 5, 0);
        cpu::outb(io_base_ + 7, 0xEC); // ATA aygıtını tanımla (IDENTIFY DEVICE)

        uint8_t status = cpu::inb(io_base_ + 7);
        if (status == 0 || status == 0xFF)
            return false;

        uint32_t attempts = 0;
        while ((status & STATUS_BSY) != 0 && attempts++ < WAIT_LIMIT)
            status = cpu::inb(io_base_ + 7);
        if (attempts >= WAIT_LIMIT || status == 0 || status == 0xFF ||
            (status & STATUS_ERR) != 0)
            return false;

        if ((cpu::inb(io_base_ + 4) != 0) || (cpu::inb(io_base_ + 5) != 0))
            return false; // Bu bir ATAPI aygıtı; ATA sabit diski değil.
        if ((status & STATUS_DRQ) == 0)
            return false;

        uint16_t identify[256];
        for (uint32_t i = 0; i < 256; ++i)
            identify[i] = inw(io_base_);

        sector_count_ = static_cast<uint32_t>(identify[60]) |
                        (static_cast<uint32_t>(identify[61]) << 16);
        ready_ = sector_count_ != 0;
        return ready_;
    }

    bool AtaPioDisk::read_sector(uint32_t lba, uint8_t *buffer) const
    {
        if (!ready_ || buffer == nullptr || lba >= sector_count_ || lba >= 0x10000000)
            return false;
        if (!wait_ready(false))
            return false;

        cpu::outb(io_base_ + 6, static_cast<uint8_t>(drive_select_ | ((lba >> 24) & 0x0F)));
        io_delay();
        cpu::outb(io_base_ + 2, 1);
        cpu::outb(io_base_ + 3, static_cast<uint8_t>(lba));
        cpu::outb(io_base_ + 4, static_cast<uint8_t>(lba >> 8));
        cpu::outb(io_base_ + 5, static_cast<uint8_t>(lba >> 16));
        cpu::outb(io_base_ + 7, 0x20); // Sektörleri oku (READ SECTORS)
        if (!wait_ready(true))
            return false;

        for (uint32_t i = 0; i < SECTOR_SIZE / 2; ++i)
        {
            const uint16_t word = inw(io_base_);
            buffer[i * 2] = static_cast<uint8_t>(word);
            buffer[i * 2 + 1] = static_cast<uint8_t>(word >> 8);
        }
        io_delay();
        return wait_ready(false);
    }

    bool AtaPioDisk::write_sector(uint32_t lba, const uint8_t *buffer) const
    {
        if (!ready_ || buffer == nullptr || lba >= sector_count_ || lba >= 0x10000000)
            return false;
        if (!wait_ready(false))
            return false;

        cpu::outb(io_base_ + 6, static_cast<uint8_t>(drive_select_ | ((lba >> 24) & 0x0F)));
        io_delay();
        cpu::outb(io_base_ + 2, 1);
        cpu::outb(io_base_ + 3, static_cast<uint8_t>(lba));
        cpu::outb(io_base_ + 4, static_cast<uint8_t>(lba >> 8));
        cpu::outb(io_base_ + 5, static_cast<uint8_t>(lba >> 16));
        cpu::outb(io_base_ + 7, 0x30); // Sektörleri yaz (WRITE SECTORS)
        if (!wait_ready(true))
            return false;

        for (uint32_t i = 0; i < SECTOR_SIZE / 2; ++i)
        {
            const uint16_t word = static_cast<uint16_t>(buffer[i * 2]) |
                (static_cast<uint16_t>(buffer[i * 2 + 1]) << 8);
            outw(io_base_, word);
        }
        io_delay();
        if (!wait_ready(false))
            return false;

        cpu::outb(io_base_ + 7, 0xE7); // Önbelleği diske aktar (FLUSH CACHE)
        return wait_ready(false);
    }
}
