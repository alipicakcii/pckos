// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <interrupts/isr.hh>
#include <library/kprint.hh>
#include <cpu/io.hh>
#include <interrupts/interrupts.hh>
#include <kernel/thread.hh>
#include <library/kernel_panic.hh>

using namespace cpu;
namespace kernel
{
    IsrFunction g_interrupt_handlers[256];
    static volatile uint32_t g_timer_ticks = 0;

    void timer_initialize(uint32_t frequency)
    {
        if (frequency == 0)
            return;

        uint32_t divisor = 1193180 / frequency;
        if (divisor == 0)
            divisor = 1;
        if (divisor > 0xFFFF)
            divisor = 0xFFFF;

        outb(0x43, 0x36);
        outb(0x40, static_cast<uint8_t>(divisor & 0xFF));
        outb(0x40, static_cast<uint8_t>((divisor >> 8) & 0xFF));

        uint8_t mask = inb(0x21);
        outb(0x21, static_cast<uint8_t>(mask & ~0x01));
    }

    uint32_t timer_get_ticks()
    {
        return g_timer_ticks;
    }

    void interrupt_register(uint8_t n, IsrFunction handler)
    {
        g_interrupt_handlers[n] = handler;
    }

    extern "C" Registers *handle_isr(Registers *regs)
    {
		if (regs == nullptr)
			return regs;

		const uint8_t int_no = regs->interruptNumber & 0xFF;
		if ((regs->cs & 3U) == 3U && int_no < 32)
		{
			user_mode_handle_exception(regs);
			return regs;
		}

        if (g_interrupt_handlers[int_no] != 0)
        {
            IsrFunction handler = g_interrupt_handlers[int_no];
            handler(regs);
        }
        else if (int_no == 129)
            return thread_schedule(regs, false);
        else if (int_no < 32)
        {
            _kprintf("[ISR] vector=%d error=0x%x eip=0x%x cs=0x%x esp=0x%x\n",
                     int_no, regs->errorCode, regs->eip, regs->cs, regs->esp);
            lib::panic("Unhandled kernel exception");
        }
        else
            _kprintf("isr_unhandled interrupt: %d\n", int_no);
        return regs;
    }

    extern "C" Registers *handle_irq(Registers *regs)
    {
		if (regs == nullptr)
			return regs;

        if (regs->interruptNumber >= 40)
        {
            outb(0xA0, 0x20);
        }
        outb(0x20, 0x20);

        if (regs->interruptNumber == IRQ0)
        {
            ++g_timer_ticks;
            return thread_schedule(regs, true);
        }

        if (g_interrupt_handlers[regs->interruptNumber] != 0)
        {
            IsrFunction handler = g_interrupt_handlers[regs->interruptNumber];
            handler(regs);
        }
        else
            _kprintf("unhandled IRQ: %d\n", regs->interruptNumber);
        return regs;
    }

} // namespace kernel
