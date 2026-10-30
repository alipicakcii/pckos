// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef LIB_KERNEL_PANIC_H
#define LIB_KERNEL_PANIC_H
#include <stdint.h>
#include <library/kprint.hh>
#include <interrupts/interrupts.hh>
#include <kernel/panic.hh>


namespace lib
{
	// önemli hata çekirdek hata bildirimi
	inline void  panic(const char * msg)
	{
		cpu::disable_interrupt();
		kernel_panic_screen(msg);
		_kprintf("\n[kernel::panic]=%s", msg);
		for (;;)
			cpu::cpu_halt();
	}
}

#endif
