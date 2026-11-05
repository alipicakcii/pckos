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
}

#endif
