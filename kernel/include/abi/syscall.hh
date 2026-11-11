// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_ABI_SYSCALL_HH
#define PCKOS_ABI_SYSCALL_HH

#include <stdint.h>

namespace abi
{
    enum SyscallNumber : uint32_t
    {
        syscall_exit = 0,
        syscall_write = 1,
        syscall_read = 2,
        syscall_get_ticks = 3,
        syscall_get_heap_usage = 4,
        syscall_open = 5,
        syscall_close = 6,
        syscall_readdir = 7,
        syscall_get_root = 8,
        syscall_wait_event = 9,
        syscall_gfx_clear = 10,
        syscall_gfx_rect = 11,
        syscall_gfx_text = 12,
        syscall_get_screen_info = 13,
        syscall_gfx_cursor = 14,
        syscall_gfx_bitmap = 15,
        syscall_get_kernel_log = 16,
        syscall_gfx_begin_frame = 17,
        syscall_gfx_end_frame = 18,
        syscall_gfx_push_clip = 19,
        syscall_gfx_pop_clip = 20,
        syscall_thread_yield = 21,
        syscall_thread_sleep = 22,
        syscall_thread_id = 23,
        syscall_thread_count = 24,
        syscall_thread_switch_count = 25,
        syscall_thread_create = 26,
        syscall_thread_exit = 27,
        syscall_thread_info = 28,
        syscall_system_info = 29,
        syscall_thread_set_name = 30,
        syscall_process_count = 31,
        syscall_process_info = 32,
        syscall_process_spawn = 33,
        syscall_process_minimize = 34,
        syscall_process_is_minimized = 35,
        syscall_process_focus = 36,
        syscall_gfx_get_cursor = 37,
        syscall_gfx_register_window = 38,
        syscall_gfx_move_window = 39,
        syscall_process_focused_id = 40,
        syscall_get_datetime = 41,
        syscall_heap_alloc = 42,
        syscall_heap_free = 43,
        syscall_terminal_attach = 44,
        syscall_terminal_read = 45,
        syscall_terminal_send = 46,
        syscall_read_kernel_log = 47,
        syscall_pc_speaker = 48,
        syscall_cpu_rnd = 49,
        syscall_terminal_interrupt = 50,
        syscall_thread_capacity = 51,
        syscall_power_off = 52,
        syscall_file_replace = 53,
        syscall_file_create = 54,
        syscall_directory_create = 55,
        sysycall_serial_dev_write = 56
    };
}

#endif
