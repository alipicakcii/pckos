// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_KERNEL_SYSCALL_HH
#define PCKOS_KERNEL_SYSCALL_HH

#include <stdint.h>
#include <mem/vmm.hh>

namespace device { class FrameBuffer; }

namespace kernel
{
    void syscall_initialize(mem::Vmm &virtual_memory,
                            device::FrameBuffer *framebuffer);
    void syscall_reset_process();
    bool syscall_graphics_frame_active();
    void syscall_window_set_visible(uint32_t process_id, bool visible);
    void syscall_window_raise(uint32_t process_id);
    void syscall_window_remove(uint32_t process_id);
    uint32_t syscall_window_at(uint32_t x, uint32_t y);
}

#endif
