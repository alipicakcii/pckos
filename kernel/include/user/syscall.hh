// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_USER_SYSCALL_HH
#define PCKOS_USER_SYSCALL_HH

#include <stdint.h>

namespace user
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
        syscall_gfx_begin_frame = 17,
        syscall_gfx_end_frame = 18,
        syscall_gfx_push_clip = 19,
        syscall_gfx_pop_clip = 20
    };

    enum : uint32_t
    {
        stdin_descriptor = 0,
        stdout_descriptor = 1,
        stderr_descriptor = 2,
        directory_type = 2
    };

    struct DirectoryEntry
    {
        uint32_t inode;
        uint32_t type;
        uint32_t size;
        char name[48];
    };

    struct InputEvent
    {
        uint32_t type; // 1 = klavye, 2 = fare, 3 = zaman aşımı
        int32_t value0; // tuş karakteri veya farenin X eksenindeki hareketi
        int32_t value1; // farenin Y eksenindeki hareketi
        uint32_t buttons;
    };

    struct GraphicsInfo
    {
        uint32_t width;
        uint32_t height;
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

    struct SystemInfo
    {
        uint32_t total_pages;
        uint32_t free_pages;
        uint32_t kernel_heap_used;
        uint32_t ticks;
        uint32_t switch_count;
        uint32_t cpu_family;
        uint32_t cpu_model;
        uint32_t logical_cpu_count;
        char cpu_vendor[13];
        char cpu_brand[49];
    };

    struct DateTime
    {
        uint32_t year, month, day, hour, minute, second;
    };

    using ThreadEntry = void (*)(void *argument);

    // i386 int 0x80 ABI: çağrı numarası EAX’te, bağımsız değişkenler EBX/ECX/EDX/ESI/EDI yazmaçlarındadır.
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

    inline int32_t write(int descriptor, const void *buffer, uint32_t length)
    {
        return call(syscall_write, static_cast<uint32_t>(descriptor),
                    reinterpret_cast<uint32_t>(buffer), length);
    }

    inline int32_t read(int descriptor, void *buffer, uint32_t length)
    {
        return call(syscall_read, static_cast<uint32_t>(descriptor),
                    reinterpret_cast<uint32_t>(buffer), length);
    }

    inline int32_t get_ticks()
    {
        return call(syscall_get_ticks);
    }

    inline int32_t get_datetime(DateTime *value)
    {
        if (value == nullptr) return -22;
        return call(syscall_get_datetime, reinterpret_cast<uint32_t>(value));
    }

    inline int32_t get_heap_usage()
    {
        return call(syscall_get_heap_usage);
    }

    inline void *heap_alloc(uint32_t size, uint32_t alignment = 16)
    {
        const int32_t result = call(syscall_heap_alloc, size, alignment);
        return result < 0 ? nullptr : reinterpret_cast<void *>(result);
    }

    inline int32_t heap_free(void *address)
    {
        return call(syscall_heap_free, reinterpret_cast<uint32_t>(address));
    }

    inline int32_t open(const char *path, uint32_t flags = 0)
    {
        return call(syscall_open, reinterpret_cast<uint32_t>(path), flags);
    }

    inline int32_t close(int descriptor)
    {
        return call(syscall_close, static_cast<uint32_t>(descriptor));
    }

    inline int32_t read_directory(uint32_t directory, uint32_t index,
                                  DirectoryEntry *entry)
    {
        return call(syscall_readdir, directory, index,
                    reinterpret_cast<uint32_t>(entry));
    }

    inline int32_t root_inode()
    {
        return call(syscall_get_root);
    }

    inline int32_t wait_event(InputEvent *event, uint32_t timeout_ticks = 0)
    {
        return call(syscall_wait_event, reinterpret_cast<uint32_t>(event), timeout_ticks);
    }

    inline void thread_yield()
    {
        (void)call(syscall_thread_yield);
    }

    inline void thread_sleep(uint32_t ticks)
    {
        (void)call(syscall_thread_sleep, ticks);
    }

    inline uint32_t thread_id()
    {
        return static_cast<uint32_t>(call(syscall_thread_id));
    }

    inline uint32_t thread_count()
    {
        return static_cast<uint32_t>(call(syscall_thread_count));
    }

    inline uint32_t thread_switch_count()
    {
        return static_cast<uint32_t>(call(syscall_thread_switch_count));
    }

    [[noreturn]] inline void thread_start_trampoline(ThreadEntry entry,
                                                      void *argument)
    {
        if (entry != nullptr)
            entry(argument);
        (void)call(syscall_thread_exit);
        for (;;) (void)call(syscall_thread_yield);
    }

    inline int32_t thread_create(ThreadEntry entry, void *argument,
                                 const char *name)
    {
        if (entry == nullptr || name == nullptr)
            return -22;
        return call(syscall_thread_create,
                    reinterpret_cast<uint32_t>(&thread_start_trampoline),
                    reinterpret_cast<uint32_t>(entry),
                    reinterpret_cast<uint32_t>(argument),
                    0, reinterpret_cast<uint32_t>(name));
    }

    [[noreturn]] inline void thread_exit()
    {
        (void)call(syscall_thread_exit);
        for (;;) (void)call(syscall_thread_yield);
    }

    inline int32_t thread_get_info(uint32_t id, ThreadInfo *info)
    {
        return call(syscall_thread_info, id,
                    reinterpret_cast<uint32_t>(info));
    }

    inline uint32_t process_count()
    {
        return static_cast<uint32_t>(call(syscall_process_count));
    }

    inline int32_t process_get_info(uint32_t index, ProcessInfo *info)
    {
        return call(syscall_process_info, index,
                    reinterpret_cast<uint32_t>(info));
    }

    inline int32_t process_spawn(const char *path, const char *name)
    {
        if (path == nullptr || name == nullptr)
            return -22;
        return call(syscall_process_spawn, reinterpret_cast<uint32_t>(path),
                    reinterpret_cast<uint32_t>(name));
    }

    inline int32_t process_minimize()
    {
        return call(syscall_process_minimize);
    }

    // Çağıran process’i sorgulamak için 0 gönderin.
    inline bool process_is_minimized(uint32_t process_id = 0)
    {
        return call(syscall_process_is_minimized, process_id) != 0;
    }

    inline int32_t process_focus(uint32_t process_id)
    {
        return call(syscall_process_focus, process_id);
    }

    inline uint32_t process_focused_id()
    {
        return static_cast<uint32_t>(call(syscall_process_focused_id));
    }

    inline int32_t get_system_info(SystemInfo *info)
    {
        return call(syscall_system_info, reinterpret_cast<uint32_t>(info));
    }

    inline int32_t thread_set_name(const char *name)
    {
        return call(syscall_thread_set_name,
                    reinterpret_cast<uint32_t>(name));
    }

    inline int32_t graphics_clear(uint32_t color)
    {
        return call(syscall_gfx_clear, color);
    }

    inline int32_t graphics_rect(uint32_t x, uint32_t y, uint32_t width,
                                 uint32_t height, uint32_t color)
    {
        return call(syscall_gfx_rect, x, y, width, height, color);
    }

    inline int32_t graphics_text(uint32_t x, uint32_t y, const char *text,
                                 uint32_t length, uint32_t color)
    {
        return call(syscall_gfx_text, x, y, reinterpret_cast<uint32_t>(text),
                    length, color);
    }

    inline int32_t graphics_get_info(GraphicsInfo *info)
    {
        return call(syscall_get_screen_info,
                    reinterpret_cast<uint32_t>(info));
    }

    inline int32_t graphics_cursor(uint32_t x, uint32_t y, bool visible)
    {
        return call(syscall_gfx_cursor, x, y, visible ? 1U : 0U);
    }

    inline int32_t graphics_get_cursor(int32_t *x, int32_t *y)
    {
        if (x == nullptr || y == nullptr)
            return -22;
        return call(syscall_gfx_get_cursor, reinterpret_cast<uint32_t>(x),
                    reinterpret_cast<uint32_t>(y));
    }

    inline int32_t graphics_register_window(uint32_t x, uint32_t y,
                                            uint32_t width, uint32_t height)
    {
        return call(syscall_gfx_register_window, x, y, width, height);
    }

    inline int32_t graphics_move_window(uint32_t x, uint32_t y)
    {
        return call(syscall_gfx_move_window, x, y);
    }

    inline int32_t graphics_bitmap(uint32_t x, uint32_t y,
                                   const uint32_t *argb_pixels,
                                   uint32_t width, uint32_t height)
    {
        return call(syscall_gfx_bitmap, x, y,
                    reinterpret_cast<uint32_t>(argb_pixels), width, height);
    }

    // Kare yalnızca end_frame sonrasında görünür olur; kırpma durumu process başına iç içe tutulur.
    inline int32_t graphics_begin_frame()
    {
        return call(syscall_gfx_begin_frame);
    }

    inline int32_t graphics_end_frame()
    {
        return call(syscall_gfx_end_frame);
    }

    inline int32_t graphics_push_clip(uint32_t x, uint32_t y,
                                      uint32_t width, uint32_t height)
    {
        return call(syscall_gfx_push_clip, x, y, width, height);
    }

    inline int32_t graphics_pop_clip()
    {
        return call(syscall_gfx_pop_clip);
    }

    inline int32_t get_kernel_log(char *buffer, uint32_t capacity)
    {
        return call(syscall_get_kernel_log,
                    reinterpret_cast<uint32_t>(buffer), capacity);
    }

    [[noreturn]] inline void exit(int32_t status)
    {
        (void)call(syscall_exit, static_cast<uint32_t>(status));
        for (;;)
            __asm__ volatile("pause");
    }
}

#endif
