// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_KERNEL_GRAPHICS_HH
#define PCKOS_KERNEL_GRAPHICS_HH

#include <stdint.h>

namespace kernel
{
    bool syscall_graphics_frame_active();
    void syscall_window_set_visible(uint32_t process_id, bool visible);
    void syscall_window_raise(uint32_t process_id);
    void syscall_window_remove(uint32_t process_id);
    uint32_t syscall_window_at(uint32_t x, uint32_t y);
}

#endif
