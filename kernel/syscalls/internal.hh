// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_SYSCALLS_INTERNAL_HH
#define PCKOS_SYSCALLS_INTERNAL_HH

#include <abi/syscall.hh>
#include <interrupts/isr.hh>
#include <mem/vmm.hh>

namespace device { class FrameBuffer; }

namespace kernel::syscalls
{
    constexpr uint32_t MAX_SYSCALL_TRANSFER = 4096;
    constexpr uint32_t MAX_PCKFS_REPLACE = 8192;
    constexpr uint32_t MAX_USER_PATH = 128;

    constexpr int32_t ERROR_NO_ENTRY = -2;
    constexpr int32_t ERROR_PERMISSION = -13;
    constexpr int32_t ERROR_BAD_DESCRIPTOR = -9;
    constexpr int32_t ERROR_NO_MEMORY = -12;
    constexpr int32_t ERROR_BAD_ADDRESS = -14;
    constexpr int32_t ERROR_NO_DEVICE = -19;
    constexpr int32_t ERROR_IS_DIRECTORY = -21;
    constexpr int32_t ERROR_INVALID_ARGUMENT = -22;
    constexpr int32_t ERROR_TOO_MANY_FILES = -24;
    constexpr int32_t ERROR_IO = -5;
    constexpr int32_t ERROR_NOT_IMPLEMENTED = -38;
    constexpr int32_t ERROR_SERIAL_DEV_WRITE = -40;

    extern mem::Vmm *active_virtual_memory;

    bool copy_user_string(uint32_t address, char *destination, uint32_t capacity);
    int32_t read_file(uint32_t descriptor, uint32_t user_buffer, uint32_t length);
    int32_t read_input(uint32_t user_buffer, uint32_t length);
    void reset_files();
    void initialize_graphics(device::FrameBuffer *framebuffer);
    void reset_graphics();
    uint32_t graphics_mouse_target(int32_t delta_x, int32_t delta_y);

    void handle_io(Registers *registers);
    void handle_filesystem(Registers *registers);
    void handle_thread(Registers *registers);
    void handle_process(Registers *registers);
    void handle_system(Registers *registers);
    void handle_memory(Registers *registers);
    void handle_input(Registers *registers);
    void handle_graphics(Registers *registers);
    void handle_audio(Registers *registers);
    void handle_cpu_rnd(Registers *registers);
    void handle_serialdev_write(Registers *registers);
}

#endif
