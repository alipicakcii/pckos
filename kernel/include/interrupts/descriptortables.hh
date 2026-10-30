// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef ARCH_I386_INTERRUPTS_H
#define ARCH_I386_INTERRUPTS_H

#include <stdint.h>

namespace kernel
{

    void descriptor_tables_initialize();
    void set_kernel_stack(uint32_t stack_top);

    struct GdtEntry
    {
        uint16_t limit_low;
        uint16_t base_low;
        uint8_t base_middle;
        uint8_t access;
        uint8_t granularity;
        uint8_t base_high;
    } __attribute__((packed));

    typedef struct GdtEntry GdtEntry;

    struct GdtPointer
    {
        uint16_t limit;
        uint32_t base;
    } __attribute__((packed));

    typedef struct GdtPointer GdtPointer;

    struct IdtEntry
    {
        uint16_t base_lo;
        uint16_t sel;
        uint8_t always0;
        uint8_t flags;
        uint16_t base_hi;
    } __attribute__((packed));

    typedef struct IdtEntry IdtEntry;

    struct IdtPointer
    {
        uint16_t limit;
        uint32_t base;
    } __attribute__((packed));

    typedef struct IdtPointer IdtPointer;

    struct Tss
    {
        uint16_t previous_task, __previous_task_unused;
        uint32_t esp0;
        uint16_t ss0, __ss0_unused;
        uint32_t esp1;
        uint16_t ss1, __ss1_unused;
        uint32_t esp2;
        uint16_t ss2, __ss2_unused;
        uint32_t cr3;
        uint32_t eip, eflags, eax, ecx, edx, ebx, esp, ebp, esi, edi;
        uint16_t es, __es_unused;
        uint16_t cs, __cs_unused;
        uint16_t ss, __ss_unused;
        uint16_t ds, __ds_unused;
        uint16_t fs, __fs_unused;
        uint16_t gs, __gs_unused;
        uint16_t ldt_selector, __ldt_sel_unused;
        uint16_t debug_flag, io_map;
    } __attribute__((packed));


    extern "C" void isr0();
    extern "C" void isr1();
    extern "C" void isr2();
    extern "C" void isr3();
    extern "C" void isr4();
    extern "C" void isr5();
    extern "C" void isr6();
    extern "C" void isr7();
    extern "C" void isr8();
    extern "C" void isr9();
    extern "C" void isr10();
    extern "C" void isr11();
    extern "C" void isr12();
    extern "C" void isr13();
    extern "C" void isr14();
    extern "C" void isr15();
    extern "C" void isr16();
    extern "C" void isr17();
    extern "C" void isr18();
    extern "C" void isr19();
    extern "C" void isr20();
    extern "C" void isr21();
    extern "C" void isr22();
    extern "C" void isr23();
    extern "C" void isr24();
    extern "C" void isr25();
    extern "C" void isr26();
    extern "C" void isr27();
    extern "C" void isr28();
    extern "C" void isr29();
    extern "C" void isr30();
    extern "C" void isr31();
    extern "C" void irq0();
    extern "C" void irq1();
    extern "C" void irq2();
    extern "C" void irq3();
    extern "C" void irq4();
    extern "C" void irq5();
    extern "C" void irq6();
    extern "C" void irq7();
    extern "C" void irq8();
    extern "C" void irq9();
    extern "C" void irq10();
    extern "C" void irq11();
    extern "C" void irq12();
    extern "C" void irq13();
    extern "C" void irq14();
    extern "C" void irq15();
    extern "C" void isr128();
    extern "C" void isr129();

} // namespace kernel

#endif
