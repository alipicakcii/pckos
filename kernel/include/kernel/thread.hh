// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_KERNEL_THREAD_HH
#define PCKOS_KERNEL_THREAD_HH

#include <stdint.h>
#include <interrupts/isr.hh>

namespace kernel
{
    using ThreadEntry = void (*)(void *argument);

    enum ThreadState : uint32_t
    {
        thread_state_unused = 0,
        thread_state_ready = 1,
        thread_state_running = 2,
        thread_state_sleeping = 3,
        thread_state_exited = 4
    };

    enum ThreadFlags : uint32_t
    {
        thread_flag_idle = 1,
        thread_flag_user = 2
    };

    struct ThreadInfo
    {
        uint32_t id;
        uint32_t process_id;
        uint32_t state;
        uint32_t flags;
        uint32_t cpu_ticks;
        uint32_t stack_bytes;
        char name[24];
    };

    struct ProcessInfo
    {
        uint32_t id;
        uint32_t parent_id;
        uint32_t main_thread_id;
        uint32_t thread_count;
        uint32_t cpu_ticks;
        char name[24];
    };

    // Bir tik, bir PIT periyodudur (100 Hz ayarında 10 ms).
    bool thread_system_initialize(uint32_t ring0_stack_top);
    int32_t thread_create(ThreadEntry entry, void *argument);
    int32_t thread_create_user(uint32_t entry, uint32_t user_stack_pointer,
                               uint32_t user_stack_base, uint32_t user_stack_pages,
                               uint32_t cr3, uint32_t process_id,
                               const char *name);
    void thread_yield();
    void thread_sleep(uint32_t ticks);
    void thread_set_waiting_for_input(bool waiting);
    uint32_t thread_current_id();
    uint32_t thread_active_count();
    uint32_t thread_switch_count();
    uint32_t thread_tick_count();
    bool thread_get_info(uint32_t id, ThreadInfo *info);
    uint32_t process_start(const char *name);
    uint32_t process_create(const char *name, uint32_t parent_id);
    void process_set_main_thread(uint32_t process_id, uint32_t thread_id);
    void process_finish(uint32_t process_id);
    uint32_t process_active_count();
    bool process_get_info(uint32_t index, ProcessInfo *info);
    uint32_t thread_current_process_id();
    uint32_t process_main_thread_id(uint32_t process_id);
    void process_set_input_focus(uint32_t process_id);
    uint32_t process_input_focus();
    bool process_minimize(uint32_t process_id);
    bool process_is_minimized(uint32_t process_id);
    bool process_focus(uint32_t caller_id, uint32_t target_id);
    uint32_t thread_current_user_stack_base();
    uint32_t thread_current_user_stack_pages();
    bool thread_current_is_user();
    void thread_set_current_name(const char *name);
    void thread_set_current_user(uint32_t process_id, const char *name,
                                 uint32_t stack_base,
                                 uint32_t stack_pages);
    void thread_set_current_kernel(const char *name);
    void thread_terminate_users();

    // Yalnızca IF bayrağı kapalıyken kesme dağıtıcısından çağrılır.
    Registers *thread_schedule(Registers *interrupted, bool from_pit);
    void thread_set_current_ring0_stack(uint32_t stack_top);
    [[noreturn]] void thread_exit();
}

#endif
