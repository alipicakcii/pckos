// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_KERNEL_BOOT_SCREEN_HH
#define PCKOS_KERNEL_BOOT_SCREEN_HH

#include <stdint.h>

namespace device { class FrameBuffer; }

namespace kernel::boot
{
    void show_loading_screen(device::FrameBuffer &framebuffer,
                             const char *message, uint32_t progress,
                             uint32_t ready = 0, uint32_t total = 0);
    void show_smp_progress(uint32_t ready, uint32_t total, void *context);
    void show_console(device::FrameBuffer &framebuffer);
}

#endif
