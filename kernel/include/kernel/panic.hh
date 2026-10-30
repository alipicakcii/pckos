// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_KERNEL_PANIC_SCREEN_HH
#define PCKOS_KERNEL_PANIC_SCREEN_HH
#include <interrupts/interrupts.hh>

namespace device
{
    class FrameBuffer;
}


namespace kernel
{
    void set_panic_framebuffer(device::FrameBuffer *framebuffer);
}

extern "C" void kernel_panic_screen(const char *message);

#endif
