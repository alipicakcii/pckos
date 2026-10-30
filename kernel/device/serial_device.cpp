// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <device/serial_device.hh>
#include <cpu/io.hh>

namespace device
{
    namespace
    {
        constexpr uint16_t COM1 = 0x3F8;
        constexpr uint16_t LINE_STATUS = COM1 + 5;
        constexpr uint8_t DATA_READY = 0x01;
        constexpr uint8_t TRANSMITTER_EMPTY = 0x20;
    }

    void serial_initialize()
    {
        cpu::outb(COM1 + 1, 0x00); // UART kesmelerini devre dışı bırak.
        cpu::outb(COM1 + 3, 0x80); // Bölücü yazmacını etkinleştir.
        cpu::outb(COM1 + 0, 0x03); // 38400 baud hızını ayarla.
        cpu::outb(COM1 + 1, 0x00);
        cpu::outb(COM1 + 3, 0x03); // 8 veri biti, eşlik yok, 1 dur biti.
        cpu::outb(COM1 + 2, 0xC7); // FIFO kuyruğunu etkinleştir ve temizle.
        cpu::outb(COM1 + 4, 0x0B);
    }

    void serial_write_char(char character)
    {
        while ((cpu::inb(LINE_STATUS) & TRANSMITTER_EMPTY) == 0)
            ;
        cpu::outb(COM1, static_cast<uint8_t>(character));
    }

    void serial_write_text(const char *text)
    {
        if (text == nullptr)
            return;
        while (*text)
            serial_write_char(*text++);
    }

    bool serial_data_ready()
    {
        return (cpu::inb(LINE_STATUS) & DATA_READY) != 0;
    }

    char serial_read_char()
    {
        while (!serial_data_ready())
            ;
        return static_cast<char>(cpu::inb(COM1));
    }
}
