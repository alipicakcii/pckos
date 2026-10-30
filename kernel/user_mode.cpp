// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <kernel/user_mode.hh>
#include <kernel/syscall.hh>
#include <kernel/thread.hh>
#include <cpu/get_cpu.hh>
#include <device/ata_pio.hh>
#include <fs/pckfs.hh>
#include <interrupts/descriptortables.hh>
#include <interrupts/isr.hh>
#include <library/kernel_panic.hh>
#include <library/kprint.hh>
#include <library/mem.hh>
#include <mem/vmm.hh>

extern "C"
{
    void user_mode_enter(uint32_t entry, uint32_t stack_pointer,
                         uint32_t page_directory);
    void user_mode_exit_trampoline();
    extern const uint8_t pckos_user_demo_start[];
    extern const uint8_t pckos_user_demo_end[];
    extern const uint8_t pckos_user_desktop_start[];
    extern const uint8_t pckos_user_desktop_end[];
    uint32_t pckos_user_saved_kernel_esp = 0;
    uint32_t pckos_user_saved_kernel_eflags = 0;
    uint32_t pckos_user_saved_kernel_cr3 = 0;
}

namespace kernel
{
    namespace
    {
        constexpr uint32_t USER_CODE_ADDRESS = USER_OFFSET;
        constexpr uint32_t USER_CODE_MAX_PAGES = 64;
        constexpr uint32_t USER_STACK_PAGES = 32;
        constexpr uint32_t USER_CODE_MAX_SIZE = USER_CODE_MAX_PAGES * PAGESIZE_4K;
        constexpr uint32_t USER_STACK_ADDRESS = USER_CODE_ADDRESS +
            (USER_CODE_MAX_PAGES + 1) * PAGESIZE_4K;
        constexpr uint32_t USER_STACK_TOP = USER_STACK_ADDRESS +
            USER_STACK_PAGES * PAGESIZE_4K - 16;
        constexpr uint32_t USER_THREAD_STACK_PAGES = 8;
        constexpr uint32_t USER_THREAD_STACK_BYTES =
            USER_THREAD_STACK_PAGES * PAGESIZE_4K;
        constexpr uint32_t USER_THREAD_STACK_COUNT = 14;
        constexpr uint32_t USER_THREAD_STACK_BASE = USER_STACK_ADDRESS +
            USER_STACK_PAGES * PAGESIZE_4K + PAGESIZE_4K;
        constexpr uint32_t USER_PROCESS_COUNT = 8;
        constexpr uint32_t RING0_INTERRUPT_STACK_SIZE = 16 * 1024;
        constexpr uint32_t USER_PAGE_DIRECTORY_INDEX = USER_CODE_ADDRESS >> 22;
        constexpr int32_t ERROR_NO_ENTRY = -2;
        constexpr int32_t ERROR_NO_MEMORY = -12;
        constexpr int32_t ERROR_BAD_ADDRESS = -14;
        constexpr int32_t ERROR_IS_DIRECTORY = -21;
        constexpr int32_t ERROR_INVALID_ARGUMENT = -22;

        struct UserProcessSpace
        {
            bool reserved;
            uint32_t process_id;
            uint32_t page_directory_address;
            uint32_t code_pages;
            bool thread_stacks[USER_THREAD_STACK_COUNT];
        };

        uint8_t ring0_interrupt_stack[RING0_INTERRUPT_STACK_SIZE]
            __attribute__((aligned(16)));
        mem::Vmm *active_virtual_memory = nullptr;
        volatile bool user_task_active = false;
        int32_t user_task_exit_status = 0;
        UserProcessSpace process_spaces[USER_PROCESS_COUNT]{};
        uint8_t spawn_image[USER_CODE_MAX_SIZE];

        UserProcessSpace *reserve_process_space()
        {
            for (uint32_t i = 0; i < USER_PROCESS_COUNT; ++i)
            {
                if (process_spaces[i].reserved)
                    continue;
                UserProcessSpace &space = process_spaces[i];
                memset(&space, 0, sizeof(space));
                space.reserved = true;
                space.page_directory_address = KERN_PD_AREA_BEGIN + i * PAGESIZE_4K;
                return &space;
            }
            return nullptr;
        }

        UserProcessSpace *find_process_space(uint32_t process_id)
        {
            for (uint32_t i = 0; i < USER_PROCESS_COUNT; ++i)
                if (process_spaces[i].reserved &&
                    process_spaces[i].process_id == process_id)
                    return &process_spaces[i];
            return nullptr;
        }

        void prepare_user_page_directory(UserProcessSpace &space)
        {
            auto *directory = reinterpret_cast<uint32_t *>(space.page_directory_address);
            uint32_t *kernel_directory = active_virtual_memory->get_kernel_page_directory();
            memset(directory, 0, PAGESIZE_4K);
            for (uint32_t index = 0; index < 1023; ++index)
                directory[index] = kernel_directory[index] & ~(PG_OWNED | PG_USER);
            directory[1023] = space.page_directory_address | PG_PRESENT | PG_WRITE;
        }

        bool map_user_page(UserProcessSpace &space, uint32_t virtual_address,
                           uint32_t flags, uint32_t *physical_frame)
        {
            const uint32_t old_cr3 = cpu::read_cr3();
            if (old_cr3 != space.page_directory_address)
                mem::change_pd(space.page_directory_address);

            const uint32_t frame = active_virtual_memory->acquire_page_frame_4k();
            if (frame == static_cast<uint32_t>(-1))
            {
                if (old_cr3 != space.page_directory_address)
                    mem::change_pd(old_cr3);
                return false;
            }

            mem::VirtualAddress virtual_page(virtual_address);
            mem::PhysicalAddress physical_page(frame);
            if (!active_virtual_memory->add_page_to_pd(virtual_page, physical_page,
                                                        flags))
            {
                active_virtual_memory->release_page_frame_4k(frame);
                if (old_cr3 != space.page_directory_address)
                    mem::change_pd(old_cr3);
                return false;
            }
            memset(reinterpret_cast<void *>(virtual_address), 0, PAGESIZE_4K);
            if (physical_frame != nullptr)
                *physical_frame = frame;
            if (old_cr3 != space.page_directory_address)
                mem::change_pd(old_cr3);
            return true;
        }

        void unmap_user_page(UserProcessSpace &space, uint32_t virtual_address)
        {
            const uint32_t old_cr3 = cpu::read_cr3();
            if (old_cr3 != space.page_directory_address)
                mem::change_pd(space.page_directory_address);
            mem::VirtualAddress virtual_page(virtual_address);
            uint32_t physical_frame = 0;
            if (active_virtual_memory->remove_page_from_pd(virtual_page, &physical_frame))
                active_virtual_memory->release_page_frame_4k(physical_frame);
            if (old_cr3 != space.page_directory_address)
                mem::change_pd(old_cr3);
        }

        void release_process_thread_stacks(UserProcessSpace &space)
        {
            for (uint32_t slot = 0; slot < USER_THREAD_STACK_COUNT; ++slot)
            {
                if (!space.thread_stacks[slot])
                    continue;
                const uint32_t base = USER_THREAD_STACK_BASE +
                                      slot * USER_THREAD_STACK_BYTES;
                for (uint32_t page = 0; page < USER_THREAD_STACK_PAGES; ++page)
                    unmap_user_page(space, base + page * PAGESIZE_4K);
                space.thread_stacks[slot] = false;
            }
        }

        void cleanup_process_space(UserProcessSpace &space, bool restore_kernel)
        {
            if (!space.reserved)
                return;
            const uint32_t old_cr3 = cpu::read_cr3();
            if (old_cr3 != space.page_directory_address)
                mem::change_pd(space.page_directory_address);

            release_process_thread_stacks(space);
            for (uint32_t page = 0; page < USER_STACK_PAGES; ++page)
                unmap_user_page(space, USER_STACK_ADDRESS + page * PAGESIZE_4K);
            for (uint32_t page = 0; page < space.code_pages; ++page)
                unmap_user_page(space, USER_CODE_ADDRESS + page * PAGESIZE_4K);

            auto *directory = reinterpret_cast<uint32_t *>(space.page_directory_address);
            const uint32_t table_entry = directory[USER_PAGE_DIRECTORY_INDEX];
            directory[USER_PAGE_DIRECTORY_INDEX] = 0;
            const uint32_t return_cr3 = restore_kernel
                ? KERN_PAGE_DIRECTORY : old_cr3;
            if (return_cr3 != space.page_directory_address)
                mem::change_pd(return_cr3);
            if ((table_entry & (PG_PRESENT | PG_4MB)) == PG_PRESENT)
                active_virtual_memory->release_page_frame_4k(table_entry & 0xFFFFF000);
            memset(reinterpret_cast<void *>(space.page_directory_address), 0,
                   PAGESIZE_4K);
            memset(&space, 0, sizeof(space));
        }

        bool load_flat_image(UserProcessSpace &space, const uint8_t *image,
                             uint32_t image_size)
        {
            if (image == nullptr || image_size == 0 ||
                image_size > USER_CODE_MAX_SIZE)
                return false;
            prepare_user_page_directory(space);
            space.code_pages = (image_size + PAGESIZE_4K - 1) / PAGESIZE_4K;
            for (uint32_t page = 0; page < space.code_pages; ++page)
            {
                if (!map_user_page(space, USER_CODE_ADDRESS + page * PAGESIZE_4K,
                                   PG_USER | PG_WRITE, nullptr))
                    return false;
            }
            for (uint32_t page = 0; page < USER_STACK_PAGES; ++page)
            {
                if (!map_user_page(space, USER_STACK_ADDRESS + page * PAGESIZE_4K,
                                   PG_USER | PG_WRITE, nullptr))
                    return false;
            }

            const uint32_t old_cr3 = cpu::read_cr3();
            if (old_cr3 != space.page_directory_address)
                mem::change_pd(space.page_directory_address);
            memcpy(reinterpret_cast<void *>(USER_CODE_ADDRESS), image, image_size);
            bool valid = active_virtual_memory->is_user_range(
                USER_CODE_ADDRESS, image_size, true) &&
                active_virtual_memory->is_user_range(USER_STACK_ADDRESS,
                    USER_STACK_PAGES * PAGESIZE_4K, true);
            for (uint32_t page = 0; valid && page < space.code_pages; ++page)
            {
                mem::VirtualAddress code_address(USER_CODE_ADDRESS + page * PAGESIZE_4K);
                valid = active_virtual_memory->set_page_permissions(code_address,
                                                                     true, false);
            }
            if (old_cr3 != space.page_directory_address)
                mem::change_pd(old_cr3);
            return valid;
        }

        void exit_current_process(int32_t status)
        {
            const uint32_t process_id = thread_current_process_id();
            UserProcessSpace *space = find_process_space(process_id);
            process_finish(process_id);
            if (space != nullptr)
                cleanup_process_space(*space, true);
            (void)status;
            thread_exit();
        }
    }

    bool user_mode_initialize(mem::Vmm &virtual_memory,
                              device::FrameBuffer *framebuffer)
    {
        active_virtual_memory = &virtual_memory;
        const uint32_t ring0_stack_top = reinterpret_cast<uint32_t>(ring0_interrupt_stack) +
                                         sizeof(ring0_interrupt_stack);
        set_kernel_stack(ring0_stack_top);
        thread_set_current_ring0_stack(ring0_stack_top);
        syscall_initialize(virtual_memory, framebuffer);
        return true;
    }

    int32_t user_mode_run_program(const uint8_t *image, uint32_t image_size)
    {
        if (active_virtual_memory == nullptr || image == nullptr || image_size == 0 ||
            image_size > USER_CODE_MAX_SIZE || user_task_active)
            return ERROR_INVALID_ARGUMENT;

        const uint32_t kernel_cr3 = cpu::read_cr3();
        UserProcessSpace *space = reserve_process_space();
        if (space == nullptr)
            return ERROR_NO_MEMORY;
        if (!load_flat_image(*space, image, image_size))
        {
            cleanup_process_space(*space, false);
            return ERROR_NO_MEMORY;
        }

        syscall_reset_process();
        user_task_exit_status = 0;
        const uint32_t process_id = process_start("Masaustu");
        if (process_id == 0)
        {
            cleanup_process_space(*space, false);
            return ERROR_NO_MEMORY;
        }
        space->process_id = process_id;
        user_task_active = true;
        thread_set_current_user(process_id, "Masaustu", USER_STACK_ADDRESS,
                                USER_STACK_PAGES);
        user_mode_enter(USER_CODE_ADDRESS, USER_STACK_TOP,
                        space->page_directory_address);
        user_task_active = false;
        process_finish(process_id);
        thread_set_current_kernel("Kernel ana");
        syscall_reset_process();
        cleanup_process_space(*space, false);
        (void)kernel_cr3;
        return user_task_exit_status;
    }

    int32_t user_mode_spawn_process(const char *path, const char *name)
    {
        if (!user_task_active || !thread_current_is_user() || path == nullptr ||
            name == nullptr || !fs::pckfs_is_mounted())
            return ERROR_INVALID_ARGUMENT;
        fs::PckfsEntry entry{};
        if (!fs::pckfs_find(path, &entry))
            return ERROR_NO_ENTRY;
        if (entry.type == fs::PckfsNodeType::directory)
            return ERROR_IS_DIRECTORY;
        if (entry.size == 0 || entry.size > sizeof(spawn_image))
            return ERROR_INVALID_ARGUMENT;
        uint32_t bytes_read = 0;
        if (!fs::pckfs_read_file(entry.inode, spawn_image,
                                 sizeof(spawn_image), &bytes_read) ||
            bytes_read != entry.size)
            return ERROR_NO_ENTRY;

        UserProcessSpace *space = reserve_process_space();
        if (space == nullptr)
            return ERROR_NO_MEMORY;
        if (!load_flat_image(*space, spawn_image, bytes_read))
        {
            cleanup_process_space(*space, false);
            return ERROR_NO_MEMORY;
        }

        const uint32_t parent_id = thread_current_process_id();
        const uint32_t process_id = process_create(name, parent_id);
        if (process_id == 0)
        {
            cleanup_process_space(*space, false);
            return ERROR_NO_MEMORY;
        }
        space->process_id = process_id;
        const int32_t thread_id = thread_create_user(USER_CODE_ADDRESS,
            USER_STACK_TOP, USER_STACK_ADDRESS, USER_STACK_PAGES,
            space->page_directory_address, process_id, name);
        if (thread_id < 0)
        {
            process_finish(process_id);
            cleanup_process_space(*space, false);
            return thread_id;
        }
        process_set_main_thread(process_id, static_cast<uint32_t>(thread_id));
        process_set_input_focus(process_id);
        return static_cast<int32_t>(process_id);
    }

    int32_t user_mode_run_demo()
    {
        const uint32_t image_size =
            static_cast<uint32_t>(reinterpret_cast<uintptr_t>(pckos_user_demo_end) -
                                  reinterpret_cast<uintptr_t>(pckos_user_demo_start));
        return user_mode_run_program(pckos_user_demo_start, image_size);
    }

    int32_t user_mode_run_desktop()
    {
        const uint32_t image_size =
            static_cast<uint32_t>(reinterpret_cast<uintptr_t>(pckos_user_desktop_end) -
                                  reinterpret_cast<uintptr_t>(pckos_user_desktop_start));
        return user_mode_run_program(pckos_user_desktop_start, image_size);
    }

    bool user_mode_is_active()
    {
        return user_task_active;
    }

    uint32_t user_mode_kernel_stack_top()
    {
        return reinterpret_cast<uint32_t>(ring0_interrupt_stack) +
               sizeof(ring0_interrupt_stack);
    }

    void user_mode_exit(Registers *registers, int32_t status)
    {
        if (registers == nullptr || !user_task_active)
            return;
        if (thread_current_id() != 0)
        {
            exit_current_process(status);
            return;
        }

        user_task_exit_status = status;
        user_task_active = false;
        thread_terminate_users();
        UserProcessSpace *space = find_process_space(thread_current_process_id());
        for (uint32_t i = 0; i < USER_PROCESS_COUNT; ++i)
        {
            UserProcessSpace &child = process_spaces[i];
            if (!child.reserved || child.process_id == 0 ||
                child.process_id == thread_current_process_id())
                continue;
            process_finish(child.process_id);
            cleanup_process_space(child, false);
        }
        registers->eax = static_cast<uint32_t>(status);
        registers->eip = reinterpret_cast<uint32_t>(user_mode_exit_trampoline);
        registers->cs = 0x08;
        registers->eflags = 0x02;
        if (space != nullptr)
            process_finish(space->process_id);
    }

    void user_mode_handle_exception(Registers *registers)
    {
        if (registers == nullptr || !user_task_active)
        {
            lib::panic("Unexpected exception from Ring 3");
            return;
        }

        _kprintf("[Ring3] Hata: vector=%d error=0x%x eip=0x%x\n",
                 registers->interruptNumber, registers->errorCode, registers->eip);
        const uint32_t process_id = thread_current_process_id();
        if (thread_current_is_user() && thread_current_id() != 0 && process_id != 0)
        {
            if (process_main_thread_id(process_id) == thread_current_id())
                exit_current_process(-128 -
                    static_cast<int32_t>(registers->interruptNumber & 0xFF));
            user_mode_thread_exit();
        }
        const int32_t status = -128 -
            static_cast<int32_t>(registers->interruptNumber & 0xFF);
        user_mode_exit(registers, status);
    }

    int32_t user_mode_create_thread(uint32_t trampoline, uint32_t entry,
                                    uint32_t argument, const char *name)
    {
        const uint32_t process_id = thread_current_process_id();
        UserProcessSpace *space = find_process_space(process_id);
        if (!user_task_active || !thread_current_is_user() || space == nullptr ||
            !active_virtual_memory->is_user_range(trampoline, 1, false) ||
            !active_virtual_memory->is_user_range(entry, 1, false))
            return ERROR_BAD_ADDRESS;

        uint32_t stack_slot = 0;
        while (stack_slot < USER_THREAD_STACK_COUNT && space->thread_stacks[stack_slot])
            ++stack_slot;
        if (stack_slot == USER_THREAD_STACK_COUNT)
            return ERROR_NO_MEMORY;

        const uint32_t stack_base = USER_THREAD_STACK_BASE +
                                    stack_slot * USER_THREAD_STACK_BYTES;
        uint32_t mapped_pages = 0;
        for (; mapped_pages < USER_THREAD_STACK_PAGES; ++mapped_pages)
        {
            if (!map_user_page(*space, stack_base + mapped_pages * PAGESIZE_4K,
                               PG_USER | PG_WRITE, nullptr))
            {
                for (uint32_t page = 0; page < mapped_pages; ++page)
                    unmap_user_page(*space, stack_base + page * PAGESIZE_4K);
                return ERROR_NO_MEMORY;
            }
        }

        const uint32_t old_cr3 = cpu::read_cr3();
        if (old_cr3 != space->page_directory_address)
            mem::change_pd(space->page_directory_address);
        uint32_t user_stack = stack_base + USER_THREAD_STACK_BYTES - 16;
        auto *arguments = reinterpret_cast<uint32_t *>(user_stack - 12);
        arguments[0] = 0;
        arguments[1] = entry;
        arguments[2] = argument;
        user_stack -= 12;
        if (old_cr3 != space->page_directory_address)
            mem::change_pd(old_cr3);

        space->thread_stacks[stack_slot] = true;
        const int32_t id = thread_create_user(trampoline, user_stack, stack_base,
            USER_THREAD_STACK_PAGES, space->page_directory_address, process_id, name);
        if (id < 0)
        {
            for (uint32_t page = 0; page < USER_THREAD_STACK_PAGES; ++page)
                unmap_user_page(*space, stack_base + page * PAGESIZE_4K);
            space->thread_stacks[stack_slot] = false;
            return id;
        }
        return id;
    }

    [[noreturn]] void user_mode_thread_exit()
    {
        const uint32_t process_id = thread_current_process_id();
        if (process_main_thread_id(process_id) == thread_current_id())
        {
            exit_current_process(0);
            __builtin_unreachable();
        }
        UserProcessSpace *space = find_process_space(process_id);
        const uint32_t base = thread_current_user_stack_base();
        if (space != nullptr && base >= USER_THREAD_STACK_BASE)
        {
            const uint32_t offset = base - USER_THREAD_STACK_BASE;
            if (offset % USER_THREAD_STACK_BYTES == 0)
            {
                const uint32_t slot = offset / USER_THREAD_STACK_BYTES;
                if (slot < USER_THREAD_STACK_COUNT && space->thread_stacks[slot])
                {
                    for (uint32_t page = 0; page < USER_THREAD_STACK_PAGES; ++page)
                        unmap_user_page(*space, base + page * PAGESIZE_4K);
                    space->thread_stacks[slot] = false;
                }
            }
        }
        thread_exit();
    }
}
