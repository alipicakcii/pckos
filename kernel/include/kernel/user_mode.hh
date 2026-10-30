// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_KERNEL_USER_MODE_HH
#define PCKOS_KERNEL_USER_MODE_HH

#include <stdint.h>

namespace mem { class Vmm; }
namespace device { class FrameBuffer; }

namespace kernel
{
    struct Registers;

    bool user_mode_initialize(mem::Vmm &virtual_memory,
                              device::FrameBuffer *framebuffer);
    int32_t user_mode_run_program(const uint8_t *image, uint32_t image_size);
    int32_t user_mode_spawn_process(const char *path, const char *name);
    int32_t user_mode_heap_alloc(uint32_t size, uint32_t alignment);
    int32_t user_mode_heap_free(uint32_t address);
    int32_t user_mode_run_demo();
    int32_t user_mode_run_desktop();
    bool user_mode_is_active();
    uint32_t user_mode_kernel_stack_top();
    void user_mode_exit(Registers *registers, int32_t status);
    void user_mode_handle_exception(Registers *registers);
    int32_t user_mode_create_thread(uint32_t trampoline, uint32_t entry,
                                    uint32_t argument, const char *name);
    [[noreturn]] void user_mode_thread_exit();
}

#endif
