// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_KERNEL_PANIC_SCREEN_HH
#define PCKOS_KERNEL_PANIC_SCREEN_HH
#include <interrupts/interrupts.hh>
#include <library/kprint.hh>

namespace device
{
    class FrameBuffer;
}


namespace kernel
{
    void set_panic_framebuffer(device::FrameBuffer *framebuffer);
}

extern "C" void kernel_panic_screen(const char *message);

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
