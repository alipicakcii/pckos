// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <kernel/syscalls/syscall.hh>
#include <kernel/task/user_mode.hh>
#include "internal.hh"


namespace kernel
{
    namespace
    {
        void dispatch_syscall(Registers *registers)
        {
            if (registers == nullptr)
                return;
            if ((registers->cs & 3U) != 3U || !user_mode_is_active() ||
                syscalls::active_virtual_memory == nullptr)
            {
                registers->eax = static_cast<uint32_t>(syscalls::ERROR_NOT_IMPLEMENTED);
                return;
            }

            switch (registers->eax)
            {
                case abi::syscall_write:
                case abi::syscall_read:
                case abi::syscall_terminal_attach:
                case abi::syscall_terminal_read:
                case abi::syscall_terminal_send:
                case abi::syscall_terminal_interrupt:
                    syscalls::handle_io(registers);
                    return;

                case abi::syscall_open:
                case abi::syscall_close:
                case abi::syscall_readdir:
                case abi::syscall_get_root:
                    syscalls::handle_filesystem(registers);
                    return;

                case abi::syscall_thread_yield:
                case abi::syscall_thread_sleep:
                case abi::syscall_thread_id:
                case abi::syscall_thread_count:
                case abi::syscall_thread_switch_count:
                case abi::syscall_thread_create:
                case abi::syscall_thread_exit:
                case abi::syscall_thread_info:
                case abi::syscall_thread_set_name:
                    syscalls::handle_thread(registers);
                    return;

                case abi::syscall_exit:
                case abi::syscall_process_count:
                case abi::syscall_process_info:
                case abi::syscall_process_spawn:
                case abi::syscall_process_minimize:
                case abi::syscall_process_is_minimized:
                case abi::syscall_process_focus:
                case abi::syscall_process_focused_id:
                    syscalls::handle_process(registers);
                    return;

                case abi::syscall_get_ticks:
                case abi::syscall_get_datetime:
                case abi::syscall_system_info:
                case abi::syscall_read_kernel_log:
                case abi::syscall_get_kernel_log:
                    syscalls::handle_system(registers);
                    return;

                case abi::syscall_get_heap_usage:
                case abi::syscall_heap_alloc:
                case abi::syscall_heap_free:
                    syscalls::handle_memory(registers);
                    return;

                case abi::syscall_wait_event:
                    syscalls::handle_input(registers);
                    return;

                case abi::syscall_gfx_clear:
                case abi::syscall_gfx_rect:
                case abi::syscall_gfx_text:
                case abi::syscall_get_screen_info:
                case abi::syscall_gfx_cursor:
                case abi::syscall_gfx_bitmap:
                case abi::syscall_gfx_begin_frame:
                case abi::syscall_gfx_end_frame:
                case abi::syscall_gfx_push_clip:
                case abi::syscall_gfx_pop_clip:
                case abi::syscall_gfx_get_cursor:
                case abi::syscall_gfx_register_window:
                case abi::syscall_gfx_move_window:
                    syscalls::handle_graphics(registers);
                    return;

                case abi::syscall_pc_speaker:
                    syscalls::handle_audio(registers);
                    return;

                case abi::syscall_cpu_rnd:
                    syscalls::handle_cpu_rnd(registers);
                    return;
                default:
                    registers->eax = static_cast<uint32_t>(syscalls::ERROR_NOT_IMPLEMENTED);
                    return;
            }
        }
    }

    void syscall_initialize(mem::Vmm &virtual_memory,
                            device::FrameBuffer *framebuffer)
    {
        syscalls::active_virtual_memory = &virtual_memory;
        syscalls::initialize_graphics(framebuffer);
        syscall_reset_process();
        interrupt_register(0x80, dispatch_syscall);
    }

    void syscall_reset_process()
    {
        syscalls::reset_graphics();
        syscalls::reset_files();
    }
}
