// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <device/ata_pio.hh>
#include <device/input/ps2_keyboard.hh>
#include <device/input/ps2_mouse.hh>
#include <device/serial_device.hh>
#include <device/video/frame_buffer.hh>
#include <fs/pckfs.hh>
#include <interrupts/descriptortables.hh>
#include <interrupts/interrupts.hh>
#include <interrupts/isr.hh>
#include <kernel/panic.hh>
#include <kernel/user_mode.hh>
#include <kernel/thread.hh>
#include <library/kprint.hh>
#include <library/pck_rtti.hh>
#include <mem/multiboot.hh>
#include <mem/vmm.hh>
#include <mem/new.hh>


extern "C" void _init();


extern "C" void kernel_main(mem::multiboot_info_t *multiboot_info,
                            [[maybe_unused]] uint32_t multiboot_magic)
{
    cpu::disable_interrupt();
    device::serial_initialize();
    kernel::descriptor_tables_initialize();

    mem::Vmm virtual_memory;
    virtual_memory.initialize_paging(multiboot_info->mem_upper);

    // Dosya kapsamındaki C++ kurucuları, sayfalama kurulurken hazırlanan heap’e ihtiyaç duyar.
    _init();


    device::keyboard_initialize();
    const bool mouse_ready = device::mouse_initialize();

    device::FrameBuffer *framebuffer = new device::FrameBuffer{multiboot_info};
    kernel::set_panic_framebuffer(framebuffer);
    framebuffer->initialize(virtual_memory);

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

    kernel::timer_initialize(100);
    _kprintf("PIT zamanlayici: 100 Hz\n");
    _kprintf("Klavye: PS/2 IRQ1\n");
    _kprintf("Fare: PS/2 %s\n", mouse_ready ? "IRQ12 hazir" : "kullanilamiyor");

    if (kernel::user_mode_initialize(virtual_memory, framebuffer) &&
        kernel::thread_system_initialize(kernel::user_mode_kernel_stack_top()))
    {
        _kprintf("Zamanlayici: Round Robin, PIT 100 Hz\n");
        _kprintf("Kullanici alani masaustu baslatiliyor. Sistem gunlugu seri porttadir.\n");
        const int32_t desktop_status = kernel::user_mode_run_desktop();
        _kprintf("Kullanici alani masaustu sonlandi: %d\n", desktop_status);
    }
    else
        _kprintf("Kullanici alani baslatilamadi.\n");

   


    cpu::enable_interrupt();
    for (;;)
        cpu::cpu_halt();
}
