// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef ARCH_I386_ISR__H
#define ARCH_I386_ISR__H

#include <stdint.h>

#define IRQ0 32
#define IRQ1 33
#define IRQ2 34
#define IRQ3 35
#define IRQ4 36
#define IRQ5 37
#define IRQ6 38
#define IRQ7 39
#define IRQ8 40
#define IRQ9 41
#define IRQ10 42
#define IRQ11 43
#define IRQ12 44
#define IRQ13 45
#define IRQ14 46
#define IRQ15 47
namespace kernel
{

    typedef struct Registers
    {
        uint32_t gs;
        uint32_t fs;
        uint32_t es;
        uint32_t ds;
        uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax; // pusha tarafından yığına eklenir
        uint32_t interruptNumber, errorCode;             // varsa
        uint32_t eip, cs, eflags, userEsp, ss;           // işlemci tarafından yığına eklenir
    } Registers;

    typedef void (*IsrFunction)(Registers *);

    extern IsrFunction g_interrupt_handlers[];
    void interrupt_register(uint8_t n, IsrFunction handler);
    void timer_initialize(uint32_t frequency);
    uint32_t timer_get_ticks();
    void user_mode_handle_exception(Registers *registers);
} // namespace kernel

#endif
