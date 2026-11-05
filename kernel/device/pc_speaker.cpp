// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <device/pc_speaker.hh>
#include <cpu/io.hh>
#include <kernel/thread.hh>

#define PIT_FREQ 1193180

namespace device
{
    void play_note(uint32_t frequency, uint32_t duration_ms)
    {


        
        if (frequency == 0)
            return;

        // PIT'in frekansı 1.193180 MHz (sabit)
        // İstenen frekansa göre "reload" değeri hesaplanır
        // Bu değer, PIT'in kaç sayımda bir tetikleneceğini belirler
        uint16_t reload_value = PIT_FREQ / frequency;

        // PIT'i konfigüre ediyoruz:
        // 0x43 portuna kontrol komutu gönderiyoruz:
        // 0xB6 =
        //   Bit 7-6: 10 (channel 2)
        //   Bit 5-4: 11 (low+high byte yazılacak)
        //   Bit 3-1: 110 (Mode 3 - Square Wave Generator)
        //   Bit 0  : 0  (binary mode)
        cpu::outb(0x43, 0xB6);

        // 0x42 portuna reload_value'ın önce düşük, sonra yüksek baytı gönderilir
        cpu::outb(0x42, (uint8_t)(reload_value & 0xFF));        // Low byte
        cpu::outb(0x42, (uint8_t)((reload_value >> 8) & 0xFF)); // High byte

        // 0x61 portu üzerinden speaker kontrolü yapılır
        // Bit 0: Speaker çıkışını aktif eder
        // Bit 1: PIT Channel 2 sinyalini speaker'a yönlendirir
        uint8_t tmp = cpu::inb(0x61);
        if ((tmp & 0x03) != 0x03) // Bit 0 ve 1 zaten set değilse
        {
            cpu::outb(0x61, tmp | 0x03); // Bit 0 ve 1'i aktif et (speaker'ı aç)
        }

        // PIT tikleriyle bekle; işlemci hızına bağlı döngü kullanma.
        kernel::thread_sleep((duration_ms + 9) / 10);

        // Speaker'ı kapat
        stop();
    }

    void stop()
    {
        uint8_t tmp = cpu::inb(0x61) & 0xFC; // Bit 0 ve 1'i temizle
        cpu::outb(0x61, tmp);
    }

}
