// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include "firmware/smbios.hh"
#include <device/ata_pio.hh>
#include <device/input/ps2_keyboard.hh>
#include <device/input/ps2_mouse.hh>
#include <device/video/frame_buffer.hh>
#include <fs/pckfs.hh>
#include <interrupts/descriptortables.hh>
#include <interrupts/interrupts.hh>
#include <interrupts/isr.hh>
#include <kernel/core/panic.hh>
#include <kernel/task/user_mode.hh>
#include <kernel/task/thread.hh>
#include <firmware/smp.hh>  
#include <firmware/acpi.hh>
#include <library/kprint.hh>
#include <library/kernel_log.hh>
#include <device/rtc.hh>
#include <mem/alloc.hh>
#include <cpu/get_cpu.hh>
#include <library/pck_rtti.hh>
#include <mem/multiboot.hh>
#include <mem/vmm.hh>
#include <mem/new.hh>



extern "C" void _init();

namespace
{
    constexpr uint32_t LOADER_BACKGROUND = 0xFF06110B;
    constexpr uint32_t LOADER_PANEL = 0xFF0D1D13;
    constexpr uint32_t LOADER_BORDER = 0xFF31583A;
    constexpr uint32_t LOADER_TRACK = 0xFF192D1F;
    constexpr uint32_t LOADER_ACCENT = 0xFF9DE85B;
    constexpr uint32_t LOADER_TEXT = 0xFFE8F2E3;
    constexpr uint32_t LOADER_MUTED = 0xFF9CAE9A;
    bool loader_drawn = false;
    uint32_t loader_x = 0, loader_y = 0, loader_width = 0;

    void fill_rectangle(device::FrameBuffer &framebuffer, uint32_t x,
                        uint32_t y, uint32_t width, uint32_t height,
                        uint32_t color)
    {
        const uint32_t right = x + width < framebuffer.get_width()
            ? x + width : framebuffer.get_width();
        const uint32_t bottom = y + height < framebuffer.get_height()
            ? y + height : framebuffer.get_height();
        for (uint32_t row = y; row < bottom; ++row)
            for (uint32_t column = x; column < right; ++column)
                framebuffer.draw_pixel(column, row, color);
    }

    void draw_centered(device::FrameBuffer &framebuffer, uint32_t y,
                       const char *text, uint32_t color)
    {
        uint32_t length = 0;
        while (text[length] != '\0') ++length;
        const uint32_t text_width = length * 8;
        const uint32_t x = text_width < framebuffer.get_width()
            ? (framebuffer.get_width() - text_width) / 2 : 0;
        framebuffer.draw_string(x, y, text, color);
    }

    void draw_number(device::FrameBuffer &framebuffer, uint32_t x,
                     uint32_t y, uint32_t number, uint32_t color)
    {
        char digits[11];
        uint32_t count = 0;
        do
        {
            digits[count++] = static_cast<char>('0' + number % 10);
            number /= 10;
        } while (number != 0 && count < sizeof(digits));
        while (count != 0)
            framebuffer.draw_char(x, y, digits[--count], color), x += 8;
    }

    void show_loading_screen(device::FrameBuffer &framebuffer,
                             const char *message, uint32_t progress,
                             uint32_t ready = 0, uint32_t total = 0)
    {
        const uint32_t screen_width = framebuffer.get_width();
        const uint32_t screen_height = framebuffer.get_height();
        if (screen_width < 240 || screen_height < 160)
            return;
        framebuffer.select_process(1);
        const bool frame_started = framebuffer.begin_frame();
        framebuffer.reset_clip();

        if (!loader_drawn)
        {
            loader_width = screen_width > 700 ? 620 : screen_width - 32;
            const uint32_t panel_height = 224;
            loader_x = (screen_width - loader_width) / 2;
            loader_y = (screen_height - panel_height) / 2;
            framebuffer.clear(LOADER_BACKGROUND);
            fill_rectangle(framebuffer, loader_x, loader_y,
                           loader_width, panel_height, LOADER_PANEL);
            fill_rectangle(framebuffer, loader_x, loader_y,
                           loader_width, 2, LOADER_BORDER);
            fill_rectangle(framebuffer, loader_x, loader_y + panel_height - 2,
                           loader_width, 2, LOADER_BORDER);
            fill_rectangle(framebuffer, loader_x, loader_y,
                           2, panel_height, LOADER_BORDER);
            fill_rectangle(framebuffer, loader_x + loader_width - 2, loader_y,
                           2, panel_height, LOADER_BORDER);
            fill_rectangle(framebuffer, loader_x + 28, loader_y + 28,
                           48, 48, LOADER_BORDER);
            fill_rectangle(framebuffer, loader_x + 32, loader_y + 32,
                           40, 40, LOADER_BACKGROUND);
            framebuffer.draw_char_scaled(loader_x + 42, loader_y + 36,
                                          'P', LOADER_TEXT, 2);
            fill_rectangle(framebuffer, loader_x + 64, loader_y + 62,
                           7, 7, LOADER_ACCENT);
            framebuffer.draw_string(loader_x + 92, loader_y + 32,
                                    "PCKOS", LOADER_TEXT);
            framebuffer.draw_string(loader_x + 92, loader_y + 52,
                                    "SISTEM BASLATILIYOR", LOADER_MUTED);
            fill_rectangle(framebuffer, loader_x + 28, loader_y + 94,
                           loader_width - 56, 1, LOADER_BORDER);
            draw_centered(framebuffer, loader_y + 112,
                          "PCKOS loading...", LOADER_ACCENT);
            fill_rectangle(framebuffer, loader_x + 32, loader_y + 158,
                           loader_width - 64, 14, LOADER_TRACK);
            loader_drawn = true;
        }

        const uint32_t bar_x = loader_x + 32;
        const uint32_t bar_y = loader_y + 158;
        const uint32_t bar_width = loader_width - 64;
        fill_rectangle(framebuffer, loader_x + 28, loader_y + 132,
                       loader_width - 56, 18, LOADER_PANEL);
        framebuffer.draw_string(loader_x + 32, loader_y + 132,
                                message, LOADER_TEXT);
        fill_rectangle(framebuffer, bar_x, bar_y, bar_width, 14, LOADER_TRACK);
        if (progress > 100) progress = 100;
        fill_rectangle(framebuffer, bar_x, bar_y,
                       (bar_width * progress) / 100, 14, LOADER_ACCENT);
        fill_rectangle(framebuffer, loader_x + 28, loader_y + 184,
                       loader_width - 56, 18, LOADER_PANEL);
        if (total != 0)
        {
            framebuffer.draw_string(loader_x + 32, loader_y + 184,
                                    "Islemciler:", LOADER_MUTED);
            draw_number(framebuffer, loader_x + 112, loader_y + 184,
                        ready, LOADER_TEXT);
            framebuffer.draw_char(loader_x + 128, loader_y + 184, '/', LOADER_MUTED);
            draw_number(framebuffer, loader_x + 144, loader_y + 184,
                        total, LOADER_MUTED);
        }
        else
            framebuffer.draw_string(loader_x + 32, loader_y + 184,
                                    "Lutfen bekleyin", LOADER_MUTED);
        if (frame_started)
            framebuffer.end_frame();
    }

    void show_smp_progress(uint32_t ready, uint32_t total, void *context)
    {
        auto *framebuffer = static_cast<device::FrameBuffer *>(context);
        const uint32_t safe_total = total == 0 ? 1 : total;
        const uint32_t progress = 24 + (ready * 66) / safe_total;
        show_loading_screen(*framebuffer, "SMP islemcileri baslatiliyor...",
                            progress, ready, total);
    }

    void show_boot_console(device::FrameBuffer &framebuffer)
    {
        char log[lib::KERNEL_LOG_CAPACITY];
        const uint32_t size = lib::kernel_log_snapshot(log, sizeof(log));
        framebuffer.select_process(1);
        framebuffer.begin_frame();
        framebuffer.clear(0xFF101820);
        uint32_t x = 8, y = 8;
        for (uint32_t i = 0; i < size; ++i)
        {
            if (log[i] == '\r') continue;
            if (log[i] == '\n' || x + 8 > framebuffer.get_width())
            {
                x = 8;
                y += 18;
                if (y + 16 > framebuffer.get_height())
                {
                    framebuffer.clear(0xFF101820);
                    y = 8;
                }
                if (log[i] == '\n') continue;
            }
            framebuffer.draw_char(x, y, log[i], 0xFFC0FFC0);
            x += 8;
        }
        framebuffer.end_frame();
    }

    void log_cpu()
    {
        uint32_t a, b, c, d;
        asm volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(0), "c"(0));
        char vendor[13]{};
        const uint32_t words[] = {b, d, c};
        for (uint32_t i = 0; i < 12; ++i)
            vendor[i] = static_cast<char>(words[i / 4] >> ((i % 4) * 8));
        _kprintf("CPU uretici: %s | etkin cekirdek: 1\n", vendor);
        asm volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(0x80000000U), "c"(0));
        if (a >= 0x80000004U)
        {
            char brand[49]{};
            for (uint32_t leaf = 0; leaf < 3; ++leaf)
            {
                asm volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d)
                             : "a"(0x80000002U + leaf), "c"(0));
                const uint32_t part[] = {a, b, c, d};
                for (uint32_t i = 0; i < 16; ++i)
                    brand[leaf * 16 + i] = static_cast<char>(part[i / 4] >> ((i % 4) * 8));
            }
            _kprintf("CPU model: %s\n", brand);
        }
    }
}




extern "C" void kernel_main(mem::multiboot_info_t *multiboot_info,
                            [[maybe_unused]] uint32_t multiboot_magic)
{


    cpu::disable_interrupt();
    _kprintf("\n[PCKOS boot] Multiboot magic=0x%x flags=0x%x\n",
             multiboot_magic, multiboot_info->flags);
    _kprintf("Sistem konsolu: framebuffer | mimari: i386, 32 bit\n");
    kernel::descriptor_tables_initialize();
    _kprintf("GDT / IDT / TSS: kuruldu | cekirdek yetki seviyesi: 0\n");
    log_cpu();



    const kernel::smp::CpuTopology cpu_topology = kernel::smp::discover_cpus();
    _kprintf("Islemci topolojisi: %u etkin islemci | ACPI MADT: %s | LAPIC: %s\n",
             cpu_topology.count, cpu_topology.acpi_found ? "var" : "yok",
             cpu_topology.local_apic_found ? "var" : "yok");
    const bool acpi_power_ready = firmware::initialize_power_off();
    _kprintf("ACPI sistem kapatma: %s\n", acpi_power_ready ? "hazir" : "desteklenmiyor");

    mem::Vmm virtual_memory;
    virtual_memory.initialize_paging(multiboot_info->mem_upper);

    // _init fonksiyonu. C++ sınıflarından oluşturulan
    // Global nesnelerin Yapıcılarının derleyici tarafından çağrılması içindir.
    _init();
    _kprintf("C++ global kurucular: tamamlandi\n");
    _kprintf("Sayfalama: 4 KiB | CR3=0x%x\n", cpu::read_cr3());
    _kprintf("Fiziksel sayfa: toplam=%u bos=%u\n",
             virtual_memory.get_total_page_count(), virtual_memory.get_free_page_count());


    device::keyboard_initialize();
    const bool mouse_ready = device::mouse_initialize();

    device::FrameBuffer *framebuffer = new device::FrameBuffer{multiboot_info};
    kernel::set_panic_framebuffer(framebuffer);
    framebuffer->initialize(virtual_memory);
    _kprintf("Framebuffer: %u x %u, pitch=%u bayt | RAM birlestirme\n",
             framebuffer->get_width(), framebuffer->get_height(), framebuffer->get_pitch());
    show_loading_screen(*framebuffer, "Cekirdek hizmetleri hazirlaniyor...", 8);



    const uint32_t memory_mb = multiboot_info->mem_upper / 1024 + 1;
    device::AtaPioDisk system_disk;
    const bool disk_ready = system_disk.initialize();
    const bool pckfs_ready = disk_ready && fs::pckfs_mount(&system_disk);
    _kprintf("\n[PckOS acilis]\n");
    _kprintf("Mimari: i386\n");
    _kprintf("Bellek: %d MiB\n", memory_mb);
_kprintf("Goruntu alani: %d x %d\n", framebuffer->get_width(), framebuffer->get_height());
    _kprintf("IDE disk: %s (%d sektor)\n", disk_ready ? "hazir" : "bulunamadi",
             disk_ready ? system_disk.sector_count() : 0);
    _kprintf("PCKFS kok: %s (%d oge)\n", pckfs_ready ? "baglandi" : "bulunamadi",
             fs::pckfs_inode_count());

    _kprintf("Disk erisimi: ATA PIO | PCKFS v1: yazilabilir kok\n");
    show_loading_screen(*framebuffer, "PCKFS hazirlaniyor...", 18);
    device::RtcDateTime now{};
    if (device::rtc_read(&now))
        _kprintf("CMOS RTC: %u-%u-%u %u:%u:%u\n", now.year, now.month, now.day,
                 now.hour, now.minute, now.second);
    else
        _kprintf("CMOS RTC: okunamadi\n");
    show_loading_screen(*framebuffer, "Kullanici alani hazirlaniyor...", 22);

    kernel::timer_initialize(100);
    _kprintf("PIT zamanlayici: 100 Hz\n");
    _kprintf("Klavye: PS/2 IRQ1\n");
    _kprintf("Fare: PS/2 %s\n", mouse_ready ? "IRQ12 hazir" : "kullanilamiyor");
  
    if (kernel::user_mode_initialize(virtual_memory, framebuffer) &&
        kernel::thread_system_initialize(kernel::user_mode_kernel_stack_top()))
    {
        show_loading_screen(*framebuffer, "SMP islemcileri baslatiliyor...", 24,
                            1, cpu_topology.count);
        const uint32_t online_cpus = kernel::smp::initialize(
            cpu_topology, virtual_memory, show_smp_progress, framebuffer);
        _kprintf("SMP: %u islemci etkin | Round Robin, 100 Hz\n", online_cpus);
        _kprintf("Sistem cagrilari: int 0x80 | kullanici yetki seviyesi: 3\n");
        _kprintf("Uygulama girisi: 0x40000000 | ayri surec adres alanlari\n");
        _kprintf("Kullanici heap: 32 MiB/surec | ana yigin: 128 KiB\n");
        _kprintf("Standart akislar: stdin=0 stdout=1 stderr=2 | terminal baglantisi\n");
        _kprintf("Cekirdek heap kullanimi: %u bayt\n", mem::MemAlloc::get_kernel_heap_used());
        _kprintf("Masaustu baslatiliyor. Sistem konsolu: ESC | gunluk: dmesg\n");
        show_loading_screen(*framebuffer, "Masaustu baslatiliyor...", 96,
                            online_cpus, cpu_topology.count);

    
        const int32_t desktop_status = kernel::user_mode_run_desktop();
        _kprintf("Kullanici alani masaustu sonlandi: %d\n", desktop_status);
    }
    else
        _kprintf("Kullanici alani baslatilamadi.\n");

   

        


    show_boot_console(*framebuffer);
    cpu::enable_interrupt();
    for (;;)
        cpu::cpu_halt();
}
