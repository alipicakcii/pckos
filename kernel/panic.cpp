// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <device/video/frame_buffer.hh>
#include <kernel/panic.hh>

namespace kernel
{
    static device::FrameBuffer *panic_framebuffer = nullptr;

    void set_panic_framebuffer(device::FrameBuffer *framebuffer)
    {
        panic_framebuffer = framebuffer;
    }
}

// çekirdek hata çıktıları
extern "C" void kernel_panic_screen(const char *message)
{
    if (kernel::panic_framebuffer == nullptr)
        return;

    kernel::panic_framebuffer->abort_frame();
    kernel::panic_framebuffer->select_process(0);
    kernel::panic_framebuffer->reset_clip();
    kernel::panic_framebuffer->clear(0xFF300000);
    kernel::panic_framebuffer->draw_string(16, 24, "PCKOS KERNEL PANIC", 0xFFFFFFFF);
    kernel::panic_framebuffer->draw_string(16, 54, "System halted:", 0xFFFFCCCC);
    kernel::panic_framebuffer->draw_string(16, 80,
        message != nullptr ? message : "Unknown error", 0xFFFFFFFF);
}
