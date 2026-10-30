// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <kernel/thread.hh>
#include <cpu/get_cpu.hh>
#include <interrupts/interrupts.hh>
#include <interrupts/descriptortables.hh>
#include <library/mem.hh>
#include <mem/vmm.hh>
#include <kernel/syscall.hh>

namespace kernel
{
    namespace
    {
        constexpr uint32_t MAX_THREADS = 16;
        constexpr uint32_t MAX_PROCESSES = 8;
        constexpr uint32_t STACK_SIZE = 8192;
        constexpr uint32_t NO_THREAD = UINT32_MAX;

        enum class State : uint8_t { unused, ready, running, sleeping, exited };

        struct Process
        {
            uint32_t id;
            uint32_t parent_id;
            uint32_t main_thread_id;
            bool active;
            bool minimized;
            char name[24];
        };

        struct Thread
        {
            Registers *saved_context;
            ThreadEntry entry;
            void *argument;
            uint32_t id;
            uint32_t process_id;
            uint32_t cr3;
            uint32_t ring0_stack_top;
            uint32_t wake_tick;
            uint32_t cpu_ticks;
            uint32_t user_stack_base;
            uint32_t user_stack_pages;
            State state;
            bool idle;
            bool user;
            bool waiting_for_input;
            char name[24];
            alignas(16) uint8_t stack[STACK_SIZE];
        };

        Thread threads[MAX_THREADS]{};
        Process processes[MAX_PROCESSES]{};
        volatile uint32_t current_thread = NO_THREAD;
        volatile uint32_t pit_ticks = 0;
        bool initialized = false;
        volatile uint32_t switch_count = 0;
        uint32_t next_process_id = 1;
        volatile uint32_t focused_process_id = 0;

        extern "C" [[noreturn]] void thread_first_run();

        void idle_thread(void *)
        {
            for (;;) cpu::cpu_halt();
        }

        bool tick_reached(uint32_t now, uint32_t deadline)
        {
            return static_cast<int32_t>(now - deadline) >= 0;
        }

        void set_name(Thread &thread, const char *name)
        {
            uint32_t i = 0;
            if (name != nullptr)
                while (i + 1 < sizeof(thread.name) && name[i] != '\0')
                {
                    thread.name[i] = name[i];
                    ++i;
                }
            if (i == 0)
            {
                const char *fallback = thread.user ? "Kullanici thread" : "Kernel thread";
                while (fallback[i] != '\0' && i + 1 < sizeof(thread.name))
                {
                    thread.name[i] = fallback[i];
                    ++i;
                }
            }
            thread.name[i] = '\0';
        }

        uint32_t find_next(uint32_t after)
        {
            for (uint32_t step = 1; step <= MAX_THREADS; ++step)
            {
                const uint32_t candidate = (after + step) % MAX_THREADS;
                const State state = threads[candidate].state;
                if ((state == State::ready || state == State::running) &&
                    !threads[candidate].idle)
                    return candidate;
            }
            // Schedule the idle context only when every regular thread blocks/exits.
            for (uint32_t step = 1; step <= MAX_THREADS; ++step)
            {
                const uint32_t candidate = (after + step) % MAX_THREADS;
                const State state = threads[candidate].state;
                if ((state == State::ready || state == State::running) &&
                    threads[candidate].idle)
                    return candidate;
            }
            return NO_THREAD;
        }

        [[noreturn]] void run_thread()
        {
            const uint32_t id = current_thread;
            ThreadEntry entry = threads[id].entry;
            void *argument = threads[id].argument;
            if (entry != nullptr)
                entry(argument);
            thread_exit();
        }
    }

    [[noreturn]] void thread_exit();

    bool thread_system_initialize(uint32_t ring0_stack_top)
    {
        if (initialized || ring0_stack_top == 0)
            return false;
        for (uint32_t i = 0; i < MAX_THREADS; ++i)
            threads[i].state = State::unused;

        Thread &bootstrap = threads[0];
        bootstrap.id = 0;
        bootstrap.cr3 = cpu::read_cr3();
        bootstrap.ring0_stack_top = ring0_stack_top;
        bootstrap.state = State::running;
        bootstrap.user = false;
        set_name(bootstrap, "Kernel ana");
        current_thread = 0;
        initialized = true;
        const int32_t idle_id = thread_create(idle_thread, nullptr);
        if (idle_id < 0) return false;
        threads[idle_id].idle = true;
        set_name(threads[idle_id], "Bos bekleme");
        return true;
    }

    int32_t thread_create(ThreadEntry entry, void *argument)
    {
        if (!initialized || entry == nullptr)
            return -22;
        uint32_t flags;
        __asm__ volatile("pushfl; popl %0" : "=r"(flags));
        cpu::disable_interrupt();
        uint32_t slot = 1;
        while (slot < MAX_THREADS && threads[slot].state != State::unused &&
               threads[slot].state != State::exited)
            ++slot;
        if (slot == MAX_THREADS)
        {
            if ((flags & 0x200U) != 0) cpu::enable_interrupt();
            return -12;
        }

        Thread &thread = threads[slot];
        thread.saved_context = nullptr;
        thread.entry = entry;
        thread.argument = argument;
        thread.id = slot;
        thread.process_id = 0;
        thread.cr3 = cpu::read_cr3();
        thread.ring0_stack_top = reinterpret_cast<uint32_t>(thread.stack) + STACK_SIZE;
        thread.wake_tick = 0;
        thread.state = State::ready;
        thread.idle = false;
        thread.user = false;
        thread.waiting_for_input = false;
        thread.cpu_ticks = 0;
        thread.user_stack_base = 0;
        thread.user_stack_pages = 0;
        set_name(thread, "Kernel thread");

        // Build the same saved frame the IRQ assembly restores. The final iret
        // enters thread_first_run on this thread's own stack at Ring 0.
        uint32_t top = thread.ring0_stack_top;
        top -= sizeof(Registers);
        thread.saved_context = reinterpret_cast<Registers *>(top);
        memset(thread.saved_context, 0, sizeof(Registers));
        // RESTORE_REGS reloads these selectors before iret. Null data selectors
        // are legal to load but fault on the first kernel data access.
        thread.saved_context->gs = 0x10;
        thread.saved_context->fs = 0x10;
        thread.saved_context->es = 0x10;
        thread.saved_context->ds = 0x10;
        thread.saved_context->interruptNumber = IRQ0;
        thread.saved_context->eip = reinterpret_cast<uint32_t>(thread_first_run);
        thread.saved_context->cs = 0x08;
        thread.saved_context->eflags = 0x202;
        if ((flags & 0x200U) != 0) cpu::enable_interrupt();
        return static_cast<int32_t>(slot);
    }

    int32_t thread_create_user(uint32_t entry, uint32_t user_stack_pointer,
                               uint32_t user_stack_base, uint32_t user_stack_pages,
                               uint32_t cr3, uint32_t process_id,
                               const char *name)
    {
        if (!initialized || entry == 0 || user_stack_pointer == 0 ||
            user_stack_base == 0 || user_stack_pages == 0 || cr3 == 0)
            return -22;
        uint32_t flags;
        __asm__ volatile("pushfl; popl %0" : "=r"(flags));
        cpu::disable_interrupt();
        uint32_t slot = 2;
        while (slot < MAX_THREADS && threads[slot].state != State::unused &&
               threads[slot].state != State::exited)
            ++slot;
        if (slot == MAX_THREADS)
        {
            if ((flags & 0x200U) != 0) cpu::enable_interrupt();
            return -12;
        }

        Thread &thread = threads[slot];
        thread.saved_context = reinterpret_cast<Registers *>(
            reinterpret_cast<uint32_t>(thread.stack) + STACK_SIZE - sizeof(Registers));
        memset(thread.saved_context, 0, sizeof(Registers));
        thread.saved_context->gs = 0x23;
        thread.saved_context->fs = 0x23;
        thread.saved_context->es = 0x23;
        thread.saved_context->ds = 0x23;
        thread.saved_context->eip = entry;
        thread.saved_context->cs = 0x1B;
        thread.saved_context->eflags = 0x202;
        thread.saved_context->userEsp = user_stack_pointer;
        thread.saved_context->ss = 0x23;
        thread.entry = nullptr;
        thread.argument = nullptr;
        thread.id = slot;
        thread.process_id = process_id;
        thread.cr3 = cr3;
        thread.ring0_stack_top = reinterpret_cast<uint32_t>(thread.stack) + STACK_SIZE;
        thread.wake_tick = 0;
        thread.cpu_ticks = 0;
        thread.user_stack_base = user_stack_base;
        thread.user_stack_pages = user_stack_pages;
        thread.state = State::ready;
        thread.idle = false;
        thread.user = true;
        thread.waiting_for_input = false;
        set_name(thread, name);
        if ((flags & 0x200U) != 0) cpu::enable_interrupt();
        return static_cast<int32_t>(slot);
    }

    Registers *thread_schedule(Registers *interrupted, bool from_pit)
    {
        if (!initialized || interrupted == nullptr)
            return interrupted;

        if (from_pit)
        {
            ++pit_ticks;
            if (current_thread < MAX_THREADS &&
                threads[current_thread].state == State::running &&
                !threads[current_thread].idle &&
                !threads[current_thread].waiting_for_input)
                ++threads[current_thread].cpu_ticks;
        }
        for (uint32_t i = 0; i < MAX_THREADS; ++i)
            if (threads[i].state == State::sleeping &&
                tick_reached(pit_ticks, threads[i].wake_tick))
                threads[i].state = State::ready;

        // A GUI frame has a shared clip stack and is presented atomically.
        if (syscall_graphics_frame_active())
            return interrupted;

        const uint32_t previous = current_thread;
        if (previous >= MAX_THREADS)
            return interrupted;
        Thread &old = threads[previous];
        old.saved_context = interrupted;
        old.cr3 = cpu::read_cr3();
        if (old.state == State::running)
            old.state = State::ready;

        const uint32_t next = find_next(previous);
        if (next == NO_THREAD)
        {
            old.state = State::running;
            return interrupted;
        }
        Thread &selected = threads[next];
        selected.state = State::running;
        current_thread = next;
        if (next != previous) ++switch_count;

        if (selected.cr3 != old.cr3)
            mem::change_pd(selected.cr3);
        if (selected.ring0_stack_top != 0)
            set_kernel_stack(selected.ring0_stack_top);
        return selected.saved_context != nullptr ? selected.saved_context : interrupted;
    }

    void thread_yield()
    {
        if (initialized)
            __asm__ volatile("int $0x81" ::: "memory", "cc");
    }

    void thread_sleep(uint32_t ticks)
    {
        if (!initialized || current_thread == NO_THREAD)
            return;
        if (ticks == 0)
        {
            thread_yield();
            return;
        }
        Thread &thread = threads[current_thread];
        thread.wake_tick = pit_ticks + ticks;
        thread.state = State::sleeping;
        thread_yield();
    }

    void thread_set_waiting_for_input(bool waiting)
    {
        if (current_thread < MAX_THREADS)
            threads[current_thread].waiting_for_input = waiting;
    }

    uint32_t thread_current_id()
    {
        return initialized ? current_thread : NO_THREAD;
    }

    uint32_t thread_active_count()
    {
        uint32_t count = 0;
        for (uint32_t i = 0; i < MAX_THREADS; ++i)
            if (threads[i].state != State::unused && threads[i].state != State::exited)
                ++count;
        return count;
    }

    uint32_t thread_switch_count()
    {
        return switch_count;
    }

    uint32_t thread_tick_count()
    {
        return pit_ticks;
    }

    bool thread_get_info(uint32_t id, ThreadInfo *info)
    {
        if (info == nullptr || id >= MAX_THREADS ||
            threads[id].state == State::unused || threads[id].state == State::exited)
            return false;
        const Thread &thread = threads[id];
        info->id = thread.id;
        info->process_id = thread.process_id;
        info->state = static_cast<uint32_t>(thread.state);
        info->flags = (thread.idle ? static_cast<uint32_t>(thread_flag_idle) : 0U) |
                      (thread.user ? static_cast<uint32_t>(thread_flag_user) : 0U);
        info->cpu_ticks = thread.cpu_ticks;
        info->stack_bytes = thread.user
            ? thread.user_stack_pages * 4096 + STACK_SIZE
            : STACK_SIZE;
        for (uint32_t i = 0; i < sizeof(info->name); ++i)
            info->name[i] = thread.name[i];
        return true;
    }

    uint32_t process_start(const char *name)
    {
        if (!initialized || current_thread >= MAX_THREADS)
            return 0;
        const uint32_t id = process_create(name, 0);
        if (id == 0)
            return 0;
        Process *created = nullptr;
        for (uint32_t i = 0; i < MAX_PROCESSES; ++i)
            if (processes[i].active && processes[i].id == id)
                created = &processes[i];
        if (created == nullptr)
            return 0;
        created->main_thread_id = current_thread;
        threads[current_thread].process_id = id;
        focused_process_id = id;
        return id;
    }

    uint32_t process_create(const char *name, uint32_t parent_id)
    {
        if (!initialized)
            return 0;
        uint32_t slot = 0;
        while (slot < MAX_PROCESSES && processes[slot].active)
            ++slot;
        if (slot == MAX_PROCESSES)
            return 0;

        uint32_t id = next_process_id++;
        if (id == 0)
            id = next_process_id++;
        Process &process = processes[slot];
        process.id = id;
        process.parent_id = parent_id;
        process.main_thread_id = UINT32_MAX;
        process.active = true;
        process.minimized = false;
        uint32_t i = 0;
        if (name != nullptr)
            while (i + 1 < sizeof(process.name) && name[i] != '\0')
            {
                process.name[i] = name[i];
                ++i;
            }
        process.name[i] = '\0';
        return id;
    }

    void process_set_main_thread(uint32_t process_id, uint32_t thread_id)
    {
        for (uint32_t i = 0; i < MAX_PROCESSES; ++i)
            if (processes[i].active && processes[i].id == process_id)
            {
                processes[i].main_thread_id = thread_id;
                return;
            }
    }

    void process_finish(uint32_t process_id)
    {
        if (process_id == 0)
            return;
        syscall_window_remove(process_id);
        for (uint32_t i = 0; i < MAX_THREADS; ++i)
            if (i != current_thread && threads[i].process_id == process_id &&
                threads[i].user)
                threads[i].state = State::exited;
        for (uint32_t i = 0; i < MAX_PROCESSES; ++i)
            if (processes[i].active && processes[i].id == process_id)
            {
                if (focused_process_id == process_id)
                    focused_process_id = processes[i].parent_id;
                processes[i].active = false;
            }
    }

    uint32_t process_active_count()
    {
        uint32_t count = 0;
        for (uint32_t i = 0; i < MAX_PROCESSES; ++i)
            if (processes[i].active)
                ++count;
        return count;
    }

    bool process_get_info(uint32_t index, ProcessInfo *info)
    {
        if (info == nullptr)
            return false;
        uint32_t active_index = 0;
        for (uint32_t i = 0; i < MAX_PROCESSES; ++i)
        {
            const Process &process = processes[i];
            if (!process.active)
                continue;
            if (active_index++ != index)
                continue;
            info->id = process.id;
            info->parent_id = process.parent_id;
            info->main_thread_id = process.main_thread_id;
            info->thread_count = 0;
            info->cpu_ticks = 0;
            for (uint32_t thread_id = 0; thread_id < MAX_THREADS; ++thread_id)
            {
                const Thread &thread = threads[thread_id];
                if (thread.process_id != process.id ||
                    thread.state == State::unused || thread.state == State::exited)
                    continue;
                ++info->thread_count;
                info->cpu_ticks += thread.cpu_ticks;
            }
            for (uint32_t n = 0; n < sizeof(info->name); ++n)
                info->name[n] = process.name[n];
            return true;
        }
        return false;
    }

    uint32_t thread_current_process_id()
    {
        return current_thread < MAX_THREADS ? threads[current_thread].process_id : 0;
    }

    uint32_t process_main_thread_id(uint32_t process_id)
    {
        for (uint32_t i = 0; i < MAX_PROCESSES; ++i)
            if (processes[i].active && processes[i].id == process_id)
                return processes[i].main_thread_id;
        return UINT32_MAX;
    }

    void process_set_input_focus(uint32_t process_id)
    {
        if (process_id == 0)
        {
            focused_process_id = 0;
            return;
        }
        for (uint32_t i = 0; i < MAX_PROCESSES; ++i)
            if (processes[i].active && processes[i].id == process_id)
            {
                focused_process_id = process_id;
                return;
            }
    }

    uint32_t process_input_focus()
    {
        return focused_process_id;
    }

    bool process_minimize(uint32_t process_id)
    {
        if (process_id == 0)
            return false;
        for (uint32_t i = 0; i < MAX_PROCESSES; ++i)
            if (processes[i].active && processes[i].id == process_id)
            {
                processes[i].minimized = true;
                syscall_window_set_visible(process_id, false);
                focused_process_id = processes[i].parent_id;
                return true;
            }
        return false;
    }

    bool process_is_minimized(uint32_t process_id)
    {
        for (uint32_t i = 0; i < MAX_PROCESSES; ++i)
            if (processes[i].active && processes[i].id == process_id)
                return processes[i].minimized;
        return false;
    }

    bool process_focus(uint32_t caller_id, uint32_t target_id)
    {
        for (uint32_t i = 0; i < MAX_PROCESSES; ++i)
            if (processes[i].active && processes[i].id == target_id &&
                (processes[i].parent_id == caller_id || target_id == caller_id))
            {
                processes[i].minimized = false;
                syscall_window_set_visible(target_id, true);
                syscall_window_raise(target_id);
                focused_process_id = target_id;
                return true;
            }
        return false;
    }

    uint32_t thread_current_user_stack_base()
    {
        return current_thread < MAX_THREADS ? threads[current_thread].user_stack_base : 0;
    }

    uint32_t thread_current_user_stack_pages()
    {
        return current_thread < MAX_THREADS ? threads[current_thread].user_stack_pages : 0;
    }

    bool thread_current_is_user()
    {
        return current_thread < MAX_THREADS && threads[current_thread].user;
    }

    void thread_set_current_name(const char *name)
    {
        if (current_thread < MAX_THREADS)
            set_name(threads[current_thread], name);
    }

    void thread_set_current_user(uint32_t process_id, const char *name,
                                 uint32_t stack_base,
                                 uint32_t stack_pages)
    {
        if (current_thread >= MAX_THREADS)
            return;
        Thread &thread = threads[current_thread];
        thread.user = true;
        thread.process_id = process_id;
        thread.user_stack_base = stack_base;
        thread.user_stack_pages = stack_pages;
        set_name(thread, name);
    }

    void thread_set_current_kernel(const char *name)
    {
        if (current_thread >= MAX_THREADS)
            return;
        Thread &thread = threads[current_thread];
        thread.user = false;
        thread.process_id = 0;
        thread.user_stack_base = 0;
        thread.user_stack_pages = 0;
        set_name(thread, name);
    }

    void thread_terminate_users()
    {
        for (uint32_t i = 1; i < MAX_THREADS; ++i)
            if (threads[i].user)
                threads[i].state = State::exited;
    }

    void thread_set_current_ring0_stack(uint32_t stack_top)
    {
        if (!initialized || current_thread >= MAX_THREADS || stack_top == 0)
            return;
        threads[current_thread].ring0_stack_top = stack_top;
        set_kernel_stack(stack_top);
    }

    [[noreturn]] void thread_exit()
    {
        if (initialized && current_thread < MAX_THREADS && current_thread != 0)
            threads[current_thread].state = State::exited;
        thread_yield();
        for (;;) cpu::cpu_halt();
    }

    extern "C" [[noreturn]] void thread_first_run()
    {
        run_thread();
    }
}
