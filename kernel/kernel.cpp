// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
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
    void show_boot_console(device::FrameBuffer &framebuffer)
    {
        char log[lib::KERNEL_LOG_CAPACITY];
        const uint32_t size = lib::kernel_log_snapshot(log, sizeof(log));
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
    show_boot_console(*framebuffer);



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

    _kprintf("Disk erisimi: ATA PIO | PCKFS v1: salt okunur kok\n");
    device::RtcDateTime now{};
    if (device::rtc_read(&now))
        _kprintf("CMOS RTC: %u-%u-%u %u:%u:%u\n", now.year, now.month, now.day,
                 now.hour, now.minute, now.second);
    else
        _kprintf("CMOS RTC: okunamadi\n");
    show_boot_console(*framebuffer);

    kernel::timer_initialize(100);
    _kprintf("PIT zamanlayici: 100 Hz\n");
    _kprintf("Klavye: PS/2 IRQ1\n");
    _kprintf("Fare: PS/2 %s\n", mouse_ready ? "IRQ12 hazir" : "kullanilamiyor");
  
    if (kernel::user_mode_initialize(virtual_memory, framebuffer) &&
        kernel::thread_system_initialize(kernel::user_mode_kernel_stack_top()))
    {
        const uint32_t online_cpus = kernel::smp::initialize(cpu_topology, virtual_memory);
        _kprintf("SMP: %u islemci etkin | Round Robin, 100 Hz\n", online_cpus);
        _kprintf("Sistem cagrilari: int 0x80 | kullanici yetki seviyesi: 3\n");
        _kprintf("Uygulama girisi: 0x40000000 | ayri surec adres alanlari\n");
        _kprintf("Kullanici heap: 32 MiB/surec | ana yigin: 128 KiB\n");
        _kprintf("Standart akislar: stdin=0 stdout=1 stderr=2 | terminal baglantisi\n");
        _kprintf("Cekirdek heap kullanimi: %u bayt\n", mem::MemAlloc::get_kernel_heap_used());
        _kprintf("Masaustu baslatiliyor. Sistem konsolu: ESC | gunluk: dmesg\n");
        show_boot_console(*framebuffer);       

    
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
