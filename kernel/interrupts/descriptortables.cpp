// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <interrupts/descriptortables.hh>
#include <library/mem.hh>
#include <cpu/io.hh>
#include <interrupts/isr.hh>
#include <library/kprint.hh>
#include <library/kernel_panic.hh>
#include <interrupts/interrupts.hh>
using namespace cpu;

namespace kernel
{
    extern "C" void flush_gdt(uint32_t);
    extern "C" void flush_idt(uint32_t);
    extern "C" void flush_tss();

    static void gdt_initialize();
    static void idt_initialize();
    static void set_gdt_entry(int32_t num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran);
    static void set_idt_entry(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags);

    GdtEntry g_gdt_entries[7];
    GdtPointer g_gdt_pointer;

    IdtEntry g_idt_entries[256];
    IdtPointer g_idt_pointer;
    Tss tss;
    void cpu_zero_div_error(Registers *regs)
    {
        (void)(regs);
        lib::panic("Divide by zero");
    }

    void descriptor_tables_initialize()
    {
        gdt_initialize();

        idt_initialize();

        memset((uint8_t *)&g_interrupt_handlers, 0, sizeof(IsrFunction) * 256);
        interrupt_register(0, (IsrFunction)cpu_zero_div_error);
    }

    static void gdt_initialize()
    {
        g_gdt_pointer.limit = (sizeof(GdtEntry) * 7) - 1;
        g_gdt_pointer.base = (uint32_t)&g_gdt_entries;

        /*
          CS segmenti bitleri = 0-1 bitleri CPL bitleri, 2. bit 0 ise GDT 1 ise LDT, 2-15 index : 0000000000000   0   00
                                                                                                    INDEX         TI  CPL
        */
        /*
          Data Segmentleri bitleri = 0-1 bitleri RPL bitleri, 2. bit 0 ise GDT 1 ise LDT, 2-15 index : 0000000000000   0  00
                                                                                                         INDEX         TI  RPL
        */
        set_gdt_entry(0, 0, 0, 0, 0);                // 0x00 Null segment
        set_gdt_entry(1, 0, 0xFFFFFFFF, 0x9A, 0xCF); // 0x08 Code segment
        set_gdt_entry(2, 0, 0xFFFFFFFF, 0x92, 0xCF); // 0x10 Data segment
        set_gdt_entry(3, 0, 0xFFFFFFFF, 0xFA, 0xCF); // 0x18 User mode code segment
        set_gdt_entry(4, 0, 0xFFFFFFFF, 0xF2, 0xCF); // 0x20 User mode data segment

        // tss
        memset((uint8_t *)&tss, 0, sizeof(tss));
        tss.debug_flag = 0x00;
        tss.io_map     = sizeof(Tss);
        tss.esp0       = 0;
        tss.ss0        = 0x10;

        tss.cs   = 0x1B;
        tss.ss = tss.ds = tss.es = tss.fs = tss.gs = 0x23;
        uint32_t tss_base = (uint32_t) &tss;
        uint32_t tss_limit = sizeof(tss) - 1;


    

        set_gdt_entry(5, tss_base, tss_limit, 0x89, 0x00);
        set_gdt_entry(6, 0, 0xFFFFFFFF, 0x80, 0xCF); // Thread Local Storage pointer segment

        flush_gdt((uint32_t)&g_gdt_pointer);

        flush_tss();
    }

    void set_kernel_stack(uint32_t stack_top)
    {
        tss.esp0 = stack_top;
        tss.ss0 = 0x10;
    }

    static void set_gdt_entry(int32_t num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran)
    {
        g_gdt_entries[num].base_low = (base & 0xFFFF);
        g_gdt_entries[num].base_middle = (base >> 16) & 0xFF;
        g_gdt_entries[num].base_high = (base >> 24) & 0xFF;

        g_gdt_entries[num].limit_low = (limit & 0xFFFF);
        g_gdt_entries[num].granularity = (limit >> 16) & 0x0F;

        g_gdt_entries[num].granularity |= gran & 0xF0;
        g_gdt_entries[num].access = access;
    }

    static void idt_initialize()
    {
        g_idt_pointer.limit = sizeof(IdtEntry) * 256 - 1;
        g_idt_pointer.base = (uint32_t)&g_idt_entries;

        memset((uint8_t *)&g_idt_entries, 0, sizeof(IdtEntry) * 256);

        // Remap the irq table.
        outb(0x20, 0x11);
        outb(0xa0, 0x11);
        outb(0x21, 0x20);
        outb(0xa1, 0x28);
        outb(0x21, 0x04);
        outb(0xa1, 0x02);
        outb(0x21, 0x01);
        outb(0xa1, 0x01);
        // IRQ0 (PIT) and IRQ1 (keyboard) are enabled explicitly by their drivers.
        outb(0x21, 0xFC);
        outb(0xa1, 0xFF);

        // outb(0x21, 0xFF); // Master PIC'e IRQ0'ın devre dışı bırakılması
        // outb(0xA1, 0xFF); // Slave PIC'e IRQ0'ın devre dışı bırakılması

        set_idt_entry(0, (uint32_t)isr0, 0x08, 0x8E);
        set_idt_entry(1, (uint32_t)isr1, 0x08, 0x8E);
        set_idt_entry(2, (uint32_t)isr2, 0x08, 0x8E);
        set_idt_entry(3, (uint32_t)isr3, 0x08, 0x8E);
        set_idt_entry(4, (uint32_t)isr4, 0x08, 0x8E);
        set_idt_entry(5, (uint32_t)isr5, 0x08, 0x8E);
        set_idt_entry(6, (uint32_t)isr6, 0x08, 0x8E);
        set_idt_entry(7, (uint32_t)isr7, 0x08, 0x8E);
        set_idt_entry(8, (uint32_t)isr8, 0x08, 0x8E);
        set_idt_entry(9, (uint32_t)isr9, 0x08, 0x8E);
        set_idt_entry(10, (uint32_t)isr10, 0x08, 0x8E);
        set_idt_entry(11, (uint32_t)isr11, 0x08, 0x8E);
        set_idt_entry(12, (uint32_t)isr12, 0x08, 0x8E);
        set_idt_entry(13, (uint32_t)isr13, 0x08, 0x8E);
        set_idt_entry(14, (uint32_t)isr14, 0x08, 0x8E);
        set_idt_entry(15, (uint32_t)isr15, 0x08, 0x8E);
        set_idt_entry(16, (uint32_t)isr16, 0x08, 0x8E);
        set_idt_entry(17, (uint32_t)isr17, 0x08, 0x8E);
        set_idt_entry(18, (uint32_t)isr18, 0x08, 0x8E);
        set_idt_entry(19, (uint32_t)isr19, 0x08, 0x8E);
        set_idt_entry(20, (uint32_t)isr20, 0x08, 0x8E);
        set_idt_entry(21, (uint32_t)isr21, 0x08, 0x8E);
        set_idt_entry(22, (uint32_t)isr22, 0x08, 0x8E);
        set_idt_entry(23, (uint32_t)isr23, 0x08, 0x8E);
        set_idt_entry(24, (uint32_t)isr24, 0x08, 0x8E);
        set_idt_entry(25, (uint32_t)isr25, 0x08, 0x8E);
        set_idt_entry(26, (uint32_t)isr26, 0x08, 0x8E);
        set_idt_entry(27, (uint32_t)isr27, 0x08, 0x8E);
        set_idt_entry(28, (uint32_t)isr28, 0x08, 0x8E);
        set_idt_entry(29, (uint32_t)isr29, 0x08, 0x8E);
        set_idt_entry(30, (uint32_t)isr30, 0x08, 0x8E);
        set_idt_entry(31, (uint32_t)isr31, 0x08, 0x8E);

        set_idt_entry(32, (uint32_t)irq0, 0x08, 0x8E);
        set_idt_entry(33, (uint32_t)irq1, 0x08, 0x8E);
        set_idt_entry(34, (uint32_t)irq2, 0x08, 0x8E);
        set_idt_entry(35, (uint32_t)irq3, 0x08, 0x8E);
        set_idt_entry(36, (uint32_t)irq4, 0x08, 0x8E);
        set_idt_entry(37, (uint32_t)irq5, 0x08, 0x8E);
        set_idt_entry(38, (uint32_t)irq6, 0x08, 0x8E);
        set_idt_entry(39, (uint32_t)irq7, 0x08, 0x8E);
        set_idt_entry(40, (uint32_t)irq8, 0x08, 0x8E);
        set_idt_entry(41, (uint32_t)irq9, 0x08, 0x8E);
        set_idt_entry(42, (uint32_t)irq10, 0x08, 0x8E);
        set_idt_entry(43, (uint32_t)irq11, 0x08, 0x8E);
        set_idt_entry(44, (uint32_t)irq12, 0x08, 0x8E);
        set_idt_entry(45, (uint32_t)irq13, 0x08, 0x8E);
        set_idt_entry(46, (uint32_t)irq14, 0x08, 0x8E);
        set_idt_entry(47, (uint32_t)irq15, 0x08, 0x8E);
        set_idt_entry(128, (uint32_t)isr128, 0x08, 0xEE);
        set_idt_entry(129, (uint32_t)isr129, 0x08, 0x8E);

        flush_idt((uint32_t)&g_idt_pointer);
    }

    static void set_idt_entry(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags)
    {
        g_idt_entries[num].base_lo = base & 0xFFFF;
        g_idt_entries[num].base_hi = (base >> 16) & 0xFFFF;

        g_idt_entries[num].sel = sel;
        g_idt_entries[num].always0 = 0;
        g_idt_entries[num].flags = flags;
    }
} // namespace kernel
