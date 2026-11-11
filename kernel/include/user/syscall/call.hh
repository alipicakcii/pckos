// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_USER_SYSCALL_HH
#define PCKOS_USER_SYSCALL_HH

#include <abi/syscall.hh>

namespace user
{
    using abi::SyscallNumber;
    using abi::syscall_exit;
    using abi::syscall_write;
    using abi::syscall_read;
    using abi::syscall_get_ticks;
    using abi::syscall_get_heap_usage;
    using abi::syscall_open;
    using abi::syscall_close;
    using abi::syscall_readdir;
    using abi::syscall_get_root;
    using abi::syscall_wait_event;
    using abi::syscall_gfx_clear;
    using abi::syscall_gfx_rect;
    using abi::syscall_gfx_text;
    using abi::syscall_get_screen_info;
    using abi::syscall_gfx_cursor;
    using abi::syscall_gfx_bitmap;
    using abi::syscall_get_kernel_log;
    using abi::syscall_gfx_begin_frame;
    using abi::syscall_gfx_end_frame;
    using abi::syscall_gfx_push_clip;
    using abi::syscall_gfx_pop_clip;
    using abi::syscall_thread_yield;
    using abi::syscall_thread_sleep;
    using abi::syscall_thread_id;
    using abi::syscall_thread_count;
    using abi::syscall_thread_capacity;
    using abi::syscall_power_off;
    using abi::syscall_thread_switch_count;
    using abi::syscall_thread_create;
    using abi::syscall_thread_exit;
    using abi::syscall_thread_info;
    using abi::syscall_system_info;
    using abi::syscall_thread_set_name;
    using abi::syscall_process_count;
    using abi::syscall_process_info;
    using abi::syscall_process_spawn;
    using abi::syscall_process_minimize;
    using abi::syscall_process_is_minimized;
    using abi::syscall_process_focus;
    using abi::syscall_gfx_get_cursor;
    using abi::syscall_gfx_register_window;
    using abi::syscall_gfx_move_window;
    using abi::syscall_process_focused_id;
    using abi::syscall_get_datetime;
    using abi::syscall_heap_alloc;
    using abi::syscall_heap_free;
    using abi::syscall_terminal_attach;
    using abi::syscall_terminal_read;
    using abi::syscall_terminal_send;
    using abi::syscall_read_kernel_log;
    using abi::syscall_pc_speaker;
    using abi::syscall_cpu_rnd;
    using abi::syscall_terminal_interrupt;
    using abi::syscall_file_replace;
    using abi::syscall_file_create;

    // i386 int 0x80 ABI: çağrı numarası EAX’te,
    // bağımsız değişkenler EBX/ECX/EDX/ESI/EDI yazmaçlarındadır.
    // not: bu fonskiyon çekirdekde derlenmez kullanıcı alanından
    //  bir kod caggirdi zaman inline oldugu icin o kodla birliktde aynı dosyada derlenir
    inline int32_t call(uint32_t number, uint32_t arg0 = 0,
                        uint32_t arg1 = 0, uint32_t arg2 = 0,
                        uint32_t arg3 = 0, uint32_t arg4 = 0)
    {
        register uint32_t result __asm__("eax") = number;
        register uint32_t first __asm__("ebx") = arg0;
        register uint32_t second __asm__("ecx") = arg1;
        register uint32_t third __asm__("edx") = arg2;
        register uint32_t fourth __asm__("esi") = arg3;
        register uint32_t fifth __asm__("edi") = arg4;
        __asm__ volatile("int $0x80"
            : "+a"(result)
            : "b"(first), "c"(second), "d"(third), "S"(fourth), "D"(fifth)
            : "memory", "cc");
        return static_cast<int32_t>(result);
    }
}

#endif
