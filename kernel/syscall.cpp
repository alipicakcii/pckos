// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <kernel/syscall.hh>
#include <kernel/user_mode.hh>
#include <kernel/thread.hh>
#include <interrupts/isr.hh>
#include <interrupts/interrupts.hh>
#include <device/input/ps2_keyboard.hh>
#include <device/input/ps2_mouse.hh>
#include <device/rtc.hh>
#include <device/video/frame_buffer.hh>
#include <fs/pckfs.hh>
#include <library/kernel_log.hh>
#include <mem/alloc.hh>
#include <user/syscall.hh>

namespace kernel
{
    namespace
    {
        constexpr uint32_t MAX_SYSCALL_TRANSFER = 4096;
        constexpr uint32_t MAX_USER_PATH = 128;
        constexpr uint32_t MAX_OPEN_FILES = 8;
        constexpr uint32_t MAX_GFX_TEXT = 512;
        constexpr uint32_t MAX_GFX_RECT_PIXELS = 1024 * 1024;
        constexpr uint32_t CURSOR_WIDTH = 12;
        constexpr uint32_t CURSOR_HEIGHT = 16;

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

        struct OpenFile
        {
            uint16_t inode;
            uint32_t offset;
            bool used;
        };

        mem::Vmm *active_virtual_memory = nullptr;
        device::FrameBuffer *active_framebuffer = nullptr;
        OpenFile open_files[MAX_OPEN_FILES]{};
        uint32_t cursor_x = 0;
        uint32_t cursor_y = 0;
        bool cursor_visible = false;
        bool cursor_drawn = false;
        device::MouseEvent pending_mouse{};
        uint32_t pending_mouse_process = 0;
        uint32_t pending_mouse_buttons = 0;
        uint8_t last_mouse_buttons = 0;
        struct ClipRect { uint32_t x, y, right, bottom; };
        constexpr uint32_t MAX_CLIP_DEPTH = 16;
        ClipRect clip_stack[MAX_CLIP_DEPTH + 1]{};
        uint32_t clip_depth = 0;
        uint32_t clip_overflow = 0;

        void reset_graphics_clip()
        {
            clip_depth = 0;
            clip_overflow = 0;
            if (active_framebuffer == nullptr) return;
            clip_stack[0] = {0, 0, active_framebuffer->get_width(),
                                  active_framebuffer->get_height()};
            active_framebuffer->reset_clip();
        }

        void read_cpuid(uint32_t leaf, uint32_t subleaf,
                        uint32_t &eax, uint32_t &ebx,
                        uint32_t &ecx, uint32_t &edx)
        {
            __asm__ volatile("cpuid"
                : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                : "a"(leaf), "c"(subleaf));
        }

        bool copy_thread_name(uint32_t user_name, char *destination,
                              uint32_t capacity)
        {
            if (user_name == 0 || capacity == 0 ||
                !active_virtual_memory->is_user_range(user_name, 1, false))
                return false;
            for (uint32_t i = 0; i + 1 < capacity; ++i)
            {
                if (!active_virtual_memory->is_user_range(user_name + i, 1, false))
                    return false;
                destination[i] = reinterpret_cast<const char *>(user_name)[i];
                if (destination[i] == '\0')
                    return true;
            }
            destination[capacity - 1] = '\0';
            return true;
        }

        int32_t push_graphics_clip(uint32_t x, uint32_t y,
                                  uint32_t width, uint32_t height)
        {
            if (active_framebuffer == nullptr) return ERROR_NO_DEVICE;
            // Taşma durumunda işlem güvenli biçimde reddedilir; eşleşen pop çağrıları önceki kırpma alanını geri yükler.
            if (clip_depth == MAX_CLIP_DEPTH || clip_overflow != 0)
            {
                ++clip_overflow;
                active_framebuffer->set_clip(0, 0, 0, 0);
                return ERROR_INVALID_ARGUMENT;
            }
            const ClipRect parent = clip_stack[clip_depth];
            const uint64_t right = static_cast<uint64_t>(x) + width;
            const uint64_t bottom = static_cast<uint64_t>(y) + height;
            ClipRect clip{x > parent.x ? x : parent.x,
                          y > parent.y ? y : parent.y,
                          right < parent.right ? static_cast<uint32_t>(right) : parent.right,
                          bottom < parent.bottom ? static_cast<uint32_t>(bottom) : parent.bottom};
            if (clip.x > clip.right) clip.x = clip.right;
            if (clip.y > clip.bottom) clip.y = clip.bottom;
            clip_stack[++clip_depth] = clip;
            active_framebuffer->set_clip(clip.x, clip.y, clip.right - clip.x,
                                         clip.bottom - clip.y);
            return 0;
        }

        int32_t pop_graphics_clip()
        {
            if (active_framebuffer == nullptr) return ERROR_NO_DEVICE;
            if (clip_overflow != 0)
            {
                if (--clip_overflow != 0) return 0;
            }
            else if (clip_depth != 0) --clip_depth;
            else return ERROR_INVALID_ARGUMENT;
            const ClipRect &clip = clip_stack[clip_depth];
            active_framebuffer->set_clip(clip.x, clip.y, clip.right - clip.x,
                                         clip.bottom - clip.y);
            return 0;
        }

        void hide_graphics_cursor()
        {
            if (!cursor_drawn || active_framebuffer == nullptr ||
                active_framebuffer->frame_active())
                return;
            for (uint32_t y = 0; y < CURSOR_HEIGHT; ++y)
                for (uint32_t x = 0; x < CURSOR_WIDTH; ++x)
                    active_framebuffer->draw_hardware_pixel(cursor_x + x, cursor_y + y,
                        active_framebuffer->get_presented_pixel(cursor_x + x, cursor_y + y));
            cursor_drawn = false;
        }

        void paint_graphics_cursor()
        {
            if (!cursor_visible || active_framebuffer == nullptr ||
                active_framebuffer->frame_active())
                return;

            const uint32_t width = active_framebuffer->get_width();
            const uint32_t height = active_framebuffer->get_height();
            cursor_x = width != 0 ? (cursor_x < width ? cursor_x : width - 1) : 0;
            cursor_y = height != 0 ? (cursor_y < height ? cursor_y : height - 1) : 0;

            static const uint16_t arrow[CURSOR_HEIGHT] = {
                0x800, 0xC00, 0xE00, 0xF00, 0xF80, 0xFC0, 0xFE0, 0xFF0,
                0xFF8, 0xF80, 0xDC0, 0x8E0, 0x060, 0x070, 0x030, 0x000
            };
            for (uint32_t y = 0; y < CURSOR_HEIGHT; ++y)
                for (uint32_t x = 0; x < CURSOR_WIDTH; ++x)
                {
                    const bool filled = ((arrow[y] >> (CURSOR_WIDTH - 1 - x)) & 1U) != 0;
                    bool edge = false;
                    if (!filled)
                        for (int32_t dy = -1; dy <= 1 && !edge; ++dy)
                            for (int32_t dx = -1; dx <= 1; ++dx)
                            {
                                const int32_t ny = static_cast<int32_t>(y) + dy;
                                const int32_t nx = static_cast<int32_t>(x) + dx;
                                if (ny >= 0 && ny < static_cast<int32_t>(CURSOR_HEIGHT) &&
                                    nx >= 0 && nx < static_cast<int32_t>(CURSOR_WIDTH) &&
                                    ((arrow[ny] >> (CURSOR_WIDTH - 1 - nx)) & 1U) != 0)
                                {
                                    edge = true;
                                    break;
                                }
                            }
                    if (filled || edge)
                        active_framebuffer->draw_hardware_pixel(cursor_x + x, cursor_y + y,
                            filled ? 0xFFFFFFFF : 0xFF000000);
                }
            cursor_drawn = true;
        }

        void set_graphics_cursor(uint32_t x, uint32_t y, bool visible)
        {
            if (active_framebuffer != nullptr && active_framebuffer->frame_active())
                return; // Donanım imlecini yalnızca tamamlanmış kareler arasında hareket ettir.
            if (x == cursor_x && y == cursor_y && visible == cursor_visible)
                return;
            hide_graphics_cursor();
            cursor_x = x;
            cursor_y = y;
            cursor_visible = visible;
            paint_graphics_cursor();
        }

        bool copy_user_string(uint32_t address, char *destination, uint32_t capacity)
        {
            if (destination == nullptr || capacity == 0 ||
                active_virtual_memory == nullptr)
                return false;

            for (uint32_t i = 0; i < capacity; ++i)
            {
                if (address > UINT32_MAX - i ||
                    !active_virtual_memory->is_user_range(address + i, 1, false))
                    return false;
                const char character = *reinterpret_cast<const char *>(address + i);
                destination[i] = character;
                if (character == '\0')
                    return i != 0;
            }
            destination[capacity - 1] = '\0';
            return false;
        }

        int32_t open_file(const Registers *registers)
        {
            if (!fs::pckfs_is_mounted())
                return ERROR_NO_DEVICE;
            if (registers->ecx != 0)
                return ERROR_INVALID_ARGUMENT;

            char path[MAX_USER_PATH];
            if (!copy_user_string(registers->ebx, path, sizeof(path)))
                return ERROR_BAD_ADDRESS;

            fs::PckfsEntry entry;
            if (!fs::pckfs_find(path, &entry))
                return ERROR_NO_ENTRY;
            if (entry.type == fs::PckfsNodeType::directory)
                return ERROR_IS_DIRECTORY;

            for (uint32_t i = 0; i < MAX_OPEN_FILES; ++i)
            {
                if (open_files[i].used)
                    continue;
                open_files[i].inode = entry.inode;
                open_files[i].offset = 0;
                open_files[i].used = true;
                return static_cast<int32_t>(i + 3);
            }
            return ERROR_TOO_MANY_FILES;
        }

        int32_t read_file(uint32_t descriptor, uint32_t user_buffer,
                          uint32_t length)
        {
            const uint32_t slot = descriptor - 3;
            if (slot >= MAX_OPEN_FILES || !open_files[slot].used)
                return ERROR_BAD_DESCRIPTOR;
            if (length > MAX_SYSCALL_TRANSFER)
                return ERROR_INVALID_ARGUMENT;
            if (length != 0 && !active_virtual_memory->is_user_range(
                    user_buffer, length, true))
                return ERROR_BAD_ADDRESS;

            uint32_t bytes_read = 0;
            uint8_t *buffer = reinterpret_cast<uint8_t *>(user_buffer);
            if (!fs::pckfs_read_file_range(open_files[slot].inode,
                    open_files[slot].offset, buffer, length, &bytes_read))
                return ERROR_IO;
            open_files[slot].offset += bytes_read;
            return static_cast<int32_t>(bytes_read);
        }

        int32_t read_input(uint32_t user_buffer, uint32_t length)
        {
            if (length > MAX_SYSCALL_TRANSFER)
                return ERROR_INVALID_ARGUMENT;
            if (length != 0 && !active_virtual_memory->is_user_range(
                    user_buffer, length, true))
                return ERROR_BAD_ADDRESS;

            char *buffer = reinterpret_cast<char *>(user_buffer);
            uint32_t read_count = 0;
            while (read_count < length)
            {
                const int character = device::keyboard_read_char();
                if (character < 0)
                    break;
                buffer[read_count++] = static_cast<char>(character);
            }
            return static_cast<int32_t>(read_count);
        }

        int32_t read_directory(const Registers *registers)
        {
            if (!fs::pckfs_is_mounted())
                return ERROR_NO_DEVICE;
            if (registers->ebx > UINT16_MAX)
                return ERROR_INVALID_ARGUMENT;
            if (!active_virtual_memory->is_user_range(registers->edx,
                    sizeof(user::DirectoryEntry), true))
                return ERROR_BAD_ADDRESS;

            fs::PckfsEntry entry;
            if (!fs::pckfs_get_child(static_cast<uint16_t>(registers->ebx),
                                     registers->ecx, &entry))
                return 0;

            user::DirectoryEntry output{};
            output.inode = entry.inode;
            output.type = static_cast<uint32_t>(entry.type);
            output.size = entry.size;
            for (uint32_t i = 0; i < sizeof(output.name); ++i)
                output.name[i] = entry.name[i];

            const auto *source = reinterpret_cast<const uint8_t *>(&output);
            auto *destination = reinterpret_cast<uint8_t *>(registers->edx);
            for (uint32_t i = 0; i < sizeof(output); ++i)
                destination[i] = source[i];
            return 1;
        }

        int32_t get_graphics_info(uint32_t user_info)
        {
            if (active_framebuffer == nullptr)
                return ERROR_NO_DEVICE;
            if (!active_virtual_memory->is_user_range(user_info,
                    sizeof(user::GraphicsInfo), true))
                return ERROR_BAD_ADDRESS;

            const user::GraphicsInfo info{
                active_framebuffer->get_width(),
                active_framebuffer->get_height()
            };
            const auto *source = reinterpret_cast<const uint8_t *>(&info);
            auto *destination = reinterpret_cast<uint8_t *>(user_info);
            for (uint32_t i = 0; i < sizeof(info); ++i)
                destination[i] = source[i];
            return 0;
        }

        int32_t wait_for_input(uint32_t user_event, uint32_t timeout_ticks)
        {
            if (!active_virtual_memory->is_user_range(user_event,
                    sizeof(user::InputEvent), true))
                return ERROR_BAD_ADDRESS;

            auto *event = reinterpret_cast<user::InputEvent *>(user_event);
            const uint32_t started = timer_get_ticks();
            for (;;)
            {
                const uint32_t current_process = thread_current_process_id();
                if (pending_mouse_process == current_process)
                {
                    event->type = 2;
                    event->value0 = pending_mouse.delta_x;
                    event->value1 = pending_mouse.delta_y;
                    event->buttons = pending_mouse_buttons;
                    pending_mouse_process = 0;
                    return 1;
                }
                const bool input_focused = thread_current_process_id() ==
                                           process_input_focus();
                const int character = input_focused
                    ? device::keyboard_read_char() : -1;
                if (character >= 0)
                {
                    event->type = 1;
                    event->value0 = character;
                    event->value1 = 0;
                    event->buttons = 0;
                    return 1;
                }

                device::MouseEvent mouse;
                if (input_focused && device::mouse_read_event(&mouse))
                {
                    const bool pressed = (mouse.buttons & 1U) != 0 &&
                                         (last_mouse_buttons & 1U) == 0;
                    last_mouse_buttons = mouse.buttons;
                    if (pressed && active_framebuffer != nullptr)
                    {
                        int32_t x = static_cast<int32_t>(cursor_x) + mouse.delta_x;
                        int32_t y = static_cast<int32_t>(cursor_y) - mouse.delta_y;
                        if (x < 0) x = 0;
                        if (y < 0) y = 0;
                        if (x >= static_cast<int32_t>(active_framebuffer->get_width()))
                            x = static_cast<int32_t>(active_framebuffer->get_width()) - 1;
                        if (y >= static_cast<int32_t>(active_framebuffer->get_height()))
                            y = static_cast<int32_t>(active_framebuffer->get_height()) - 1;
                        const uint32_t target = syscall_window_at(
                            static_cast<uint32_t>(x), static_cast<uint32_t>(y));
                        if (target != current_process)
                        {
                            pending_mouse = mouse;
                            pending_mouse_process = target;
                            pending_mouse_buttons = mouse.buttons | 0x100U;
                            process_set_input_focus(target);
                            syscall_window_raise(target);
                            continue;
                        }
                    }
                    event->type = 2;
                    event->value0 = mouse.delta_x;
                    event->value1 = mouse.delta_y;
                    event->buttons = mouse.buttons | (pressed ? 0x100U : 0U);
                    return 1;
                }

                if (timeout_ticks != 0 && timer_get_ticks() - started >= timeout_ticks)
                {
                    *event = {3, 0, 0, 0};
                    return 1;
                }
                thread_set_waiting_for_input(true);
                cpu::enable_interrupt();
                cpu::cpu_halt();
                cpu::disable_interrupt();
                thread_set_waiting_for_input(false);
            }
        }

        int32_t draw_rectangle(const Registers *registers)
        {
            if (active_framebuffer == nullptr)
                return ERROR_NO_DEVICE;

            const uint32_t x = registers->ebx;
            const uint32_t y = registers->ecx;
            const uint32_t width = registers->edx;
            const uint32_t height = registers->esi;
            const uint32_t color = registers->edi;
            if (width == 0 || height == 0)
                return 0;
            if (static_cast<uint64_t>(width) * height > MAX_GFX_RECT_PIXELS)
                return ERROR_INVALID_ARGUMENT;

            const uint32_t screen_width = active_framebuffer->get_width();
            const uint32_t screen_height = active_framebuffer->get_height();
            if (x >= screen_width || y >= screen_height)
                return 0;

            const uint64_t end_x = static_cast<uint64_t>(x) + width;
            const uint64_t end_y = static_cast<uint64_t>(y) + height;
            const ClipRect &clip = clip_stack[clip_depth];
            if (clip_overflow != 0) return 0;
            const uint32_t left = x > clip.x ? x : clip.x;
            const uint32_t top = y > clip.y ? y : clip.y;
            const uint32_t right = end_x < clip.right ? static_cast<uint32_t>(end_x) : clip.right;
            const uint32_t bottom = end_y < clip.bottom ? static_cast<uint32_t>(end_y) : clip.bottom;
            if (left >= right || top >= bottom) return 0;
            hide_graphics_cursor();
            for (uint32_t row = top; row < bottom; ++row)
                for (uint32_t column = left; column < right; ++column)
                    active_framebuffer->draw_pixel(column, row, color);
            paint_graphics_cursor();
            return static_cast<int32_t>((right - left) * (bottom - top));
        }

        int32_t draw_text(const Registers *registers)
        {
            if (active_framebuffer == nullptr)
                return ERROR_NO_DEVICE;
            const uint32_t length = registers->esi;
            if (length > MAX_GFX_TEXT)
                return ERROR_INVALID_ARGUMENT;
            if (length != 0 && !active_virtual_memory->is_user_range(
                    registers->edx, length, false))
                return ERROR_BAD_ADDRESS;

            const char *text = reinterpret_cast<const char *>(registers->edx);
            uint32_t x = registers->ebx;
            uint32_t y = registers->ecx;
            const uint32_t origin_x = x;
            const uint32_t color = registers->edi;
            const uint32_t screen_width = active_framebuffer->get_width();
            const uint32_t screen_height = active_framebuffer->get_height();

            hide_graphics_cursor();
            for (uint32_t i = 0; i < length; ++i)
            {
                if (text[i] == '\n')
                {
                    x = origin_x;
                    if (y > UINT32_MAX - 18)
                        break;
                    y += 18;
                    continue;
                }
                if (x < screen_width && y < screen_height)
                    active_framebuffer->draw_char(x, y, text[i], color);
                if (x > UINT32_MAX - 8)
                    break;
                x += 8;
            }
            paint_graphics_cursor();
            return static_cast<int32_t>(length);
        }

        int32_t draw_user_bitmap(const Registers *registers)
        {
            if (active_framebuffer == nullptr)
                return ERROR_NO_DEVICE;
            const uint32_t width = registers->esi;
            const uint32_t height = registers->edi;
            const uint64_t pixel_count = static_cast<uint64_t>(width) * height;
            if (width == 0 || height == 0 || pixel_count > MAX_GFX_RECT_PIXELS)
                return ERROR_INVALID_ARGUMENT;
            const uint64_t byte_count = pixel_count * sizeof(uint32_t);
            if (byte_count > UINT32_MAX || !active_virtual_memory->is_user_range(
                    registers->edx, static_cast<uint32_t>(byte_count), false))
                return ERROR_BAD_ADDRESS;

            const uint32_t screen_width = active_framebuffer->get_width();
            const uint32_t screen_height = active_framebuffer->get_height();
            if (registers->ebx >= screen_width || registers->ecx >= screen_height)
                return 0;
            const uint64_t end_x = static_cast<uint64_t>(registers->ebx) + width;
            const uint64_t end_y = static_cast<uint64_t>(registers->ecx) + height;
            const uint32_t draw_width = static_cast<uint32_t>(
                (end_x < screen_width ? end_x : screen_width) - registers->ebx);
            const uint32_t draw_height = static_cast<uint32_t>(
                (end_y < screen_height ? end_y : screen_height) - registers->ecx);
            const auto *pixels = reinterpret_cast<const uint32_t *>(registers->edx);

            hide_graphics_cursor();
            for (uint32_t row = 0; row < draw_height; ++row)
                for (uint32_t column = 0; column < draw_width; ++column)
                {
                    const uint32_t source = pixels[row * width + column];
                    const uint32_t alpha = source >> 24;
                    if (alpha == 0)
                        continue;
                    if (alpha == 255)
                    {
                        active_framebuffer->draw_pixel(registers->ebx + column,
                            registers->ecx + row, source);
                        continue;
                    }
                    const uint32_t background = active_framebuffer->get_pixel(
                        registers->ebx + column, registers->ecx + row);
                    const uint32_t inverse_alpha = 255 - alpha;
                    const uint32_t red = (((source >> 16) & 0xFF) * alpha +
                        ((background >> 16) & 0xFF) * inverse_alpha) / 255;
                    const uint32_t green = (((source >> 8) & 0xFF) * alpha +
                        ((background >> 8) & 0xFF) * inverse_alpha) / 255;
                    const uint32_t blue = ((source & 0xFF) * alpha +
                        (background & 0xFF) * inverse_alpha) / 255;
                    active_framebuffer->draw_pixel(registers->ebx + column,
                        registers->ecx + row,
                        0xFF000000 | (red << 16) | (green << 8) | blue);
                }
            paint_graphics_cursor();
            return static_cast<int32_t>(draw_width * draw_height);
        }

        void dispatch_syscall(Registers *registers)
        {
            if (registers == nullptr)
                return;
            if ((registers->cs & 3U) != 3U || !user_mode_is_active() ||
                active_virtual_memory == nullptr)
            {
                registers->eax = static_cast<uint32_t>(ERROR_NOT_IMPLEMENTED);
                return;
            }

            const uint32_t syscall_number = registers->eax;
            if (active_framebuffer != nullptr &&
                ((syscall_number >= user::syscall_gfx_clear &&
                  syscall_number <= user::syscall_gfx_bitmap) ||
                 (syscall_number >= user::syscall_gfx_begin_frame &&
                  syscall_number <= user::syscall_gfx_pop_clip) ||
                 (syscall_number >= user::syscall_gfx_get_cursor &&
                  syscall_number <= user::syscall_gfx_move_window)))
                active_framebuffer->select_process(thread_current_process_id());

            switch (registers->eax)
            {
                case user::syscall_exit:
                    user_mode_exit(registers, static_cast<int32_t>(registers->ebx));
                    return;

                case user::syscall_write:
                {
                    if (registers->ebx != user::stdout_descriptor &&
                        registers->ebx != user::stderr_descriptor)
                    {
                        registers->eax = static_cast<uint32_t>(ERROR_BAD_DESCRIPTOR);
                        return;
                    }
                    const uint32_t length = registers->edx;
                    if (length > MAX_SYSCALL_TRANSFER)
                    {
                        registers->eax = static_cast<uint32_t>(ERROR_INVALID_ARGUMENT);
                        return;
                    }
                    if (length != 0 && !active_virtual_memory->is_user_range(
                            registers->ecx, length, false))
                    {
                        registers->eax = static_cast<uint32_t>(ERROR_BAD_ADDRESS);
                        return;
                    }
                    const char *buffer = reinterpret_cast<const char *>(registers->ecx);
                    for (uint32_t i = 0; i < length; ++i)
                        lib::kernel_log_write_char(buffer[i]);
                    registers->eax = length;
                    return;
                }

                case user::syscall_read:
                    if (registers->ebx == user::stdin_descriptor)
                        registers->eax = static_cast<uint32_t>(read_input(
                            registers->ecx, registers->edx));
                    else if (registers->ebx >= 3)
                        registers->eax = static_cast<uint32_t>(read_file(
                            registers->ebx, registers->ecx, registers->edx));
                    else
                        registers->eax = static_cast<uint32_t>(ERROR_BAD_DESCRIPTOR);
                    return;

                case user::syscall_get_ticks:
                    registers->eax = timer_get_ticks();
                    return;
                case user::syscall_get_datetime:
                {
                    if (!active_virtual_memory->is_user_range(registers->ebx,
                            sizeof(user::DateTime), true))
                    {
                        registers->eax = static_cast<uint32_t>(ERROR_BAD_ADDRESS);
                        return;
                    }
                    device::RtcDateTime value{};
                    if (!device::rtc_read(&value))
                    {
                        registers->eax = static_cast<uint32_t>(ERROR_IO);
                        return;
                    }
                    auto *destination = reinterpret_cast<user::DateTime *>(registers->ebx);
                    destination->year = value.year;
                    destination->month = value.month;
                    destination->day = value.day;
                    destination->hour = value.hour;
                    destination->minute = value.minute;
                    destination->second = value.second;
                    registers->eax = 0;
                    return;
                }
                case user::syscall_thread_yield:
                    thread_yield();
                    registers->eax = 0;
                    return;
                case user::syscall_thread_sleep:
                    thread_sleep(registers->ebx);
                    registers->eax = 0;
                    return;
                case user::syscall_thread_id:
                    registers->eax = thread_current_id();
                    return;
                case user::syscall_thread_count:
                    registers->eax = thread_active_count();
                    return;
                case user::syscall_thread_switch_count:
                    registers->eax = thread_switch_count();
                    return;
                case user::syscall_thread_create:
                {
                    char name[24]{};
                    if (!copy_thread_name(registers->edi, name, sizeof(name)))
                    {
                        registers->eax = static_cast<uint32_t>(ERROR_BAD_ADDRESS);
                        return;
                    }
                    registers->eax = static_cast<uint32_t>(user_mode_create_thread(
                        registers->ebx, registers->ecx, registers->edx, name));
                    return;
                }
                case user::syscall_thread_exit:
                    user_mode_thread_exit();
                case user::syscall_thread_info:
                {
                    if (!active_virtual_memory->is_user_range(registers->ecx,
                            sizeof(user::ThreadInfo), true))
                    {
                        registers->eax = static_cast<uint32_t>(ERROR_BAD_ADDRESS);
                        return;
                    }
                    ThreadInfo source{};
                    if (!thread_get_info(registers->ebx, &source))
                    {
                        registers->eax = static_cast<uint32_t>(ERROR_NO_ENTRY);
                        return;
                    }
                    auto *destination = reinterpret_cast<user::ThreadInfo *>(registers->ecx);
                    destination->id = source.id;
                    destination->process_id = source.process_id;
                    destination->state = source.state;
                    destination->flags = source.flags;
                    destination->cpu_ticks = source.cpu_ticks;
                    destination->stack_bytes = source.stack_bytes;
                    for (uint32_t i = 0; i < sizeof(source.name); ++i)
                        destination->name[i] = source.name[i];
                    registers->eax = 0;
                    return;
                }
                case user::syscall_process_count:
                    registers->eax = process_active_count();
                    return;
                case user::syscall_process_info:
                {
                    if (!active_virtual_memory->is_user_range(registers->ecx,
                            sizeof(user::ProcessInfo), true))
                    {
                        registers->eax = static_cast<uint32_t>(ERROR_BAD_ADDRESS);
                        return;
                    }
                    ProcessInfo source{};
                    if (!process_get_info(registers->ebx, &source))
                    {
                        registers->eax = static_cast<uint32_t>(ERROR_NO_ENTRY);
                        return;
                    }
                    auto *destination = reinterpret_cast<user::ProcessInfo *>(registers->ecx);
                    destination->id = source.id;
                    destination->parent_id = source.parent_id;
                    destination->main_thread_id = source.main_thread_id;
                    destination->thread_count = source.thread_count;
                    destination->cpu_ticks = source.cpu_ticks;
                    for (uint32_t i = 0; i < sizeof(source.name); ++i)
                        destination->name[i] = source.name[i];
                    registers->eax = 0;
                    return;
                }
                case user::syscall_process_spawn:
                {
                    char path[MAX_USER_PATH];
                    char name[24];
                    if (!copy_user_string(registers->ebx, path, sizeof(path)) ||
                        !copy_user_string(registers->ecx, name, sizeof(name)))
                    {
                        registers->eax = static_cast<uint32_t>(ERROR_BAD_ADDRESS);
                        return;
                    }
                    registers->eax = static_cast<uint32_t>(user_mode_spawn_process(
                        path, name));
                    return;
                }
                case user::syscall_process_minimize:
                    registers->eax = process_minimize(thread_current_process_id())
                        ? 0 : static_cast<uint32_t>(ERROR_NO_ENTRY);
                    return;
                case user::syscall_process_is_minimized:
                    registers->eax = process_is_minimized(registers->ebx != 0
                        ? registers->ebx : thread_current_process_id()) ? 1 : 0;
                    return;
                case user::syscall_process_focus:
                    registers->eax = process_focus(thread_current_process_id(),
                                                   registers->ebx)
                        ? 0 : static_cast<uint32_t>(ERROR_PERMISSION);
                    return;
                case user::syscall_process_focused_id:
                    registers->eax = process_input_focus();
                    return;
                case user::syscall_thread_set_name:
                {
                    char name[24]{};
                    if (!copy_thread_name(registers->ebx, name, sizeof(name)))
                    {
                        registers->eax = static_cast<uint32_t>(ERROR_BAD_ADDRESS);
                        return;
                    }
                    thread_set_current_name(name);
                    registers->eax = 0;
                    return;
                }
                case user::syscall_system_info:
                {
                    if (!active_virtual_memory->is_user_range(registers->ebx,
                            sizeof(user::SystemInfo), true))
                    {
                        registers->eax = static_cast<uint32_t>(ERROR_BAD_ADDRESS);
                        return;
                    }
                    user::SystemInfo info{};
                    info.total_pages = active_virtual_memory->get_total_page_count();
                    info.free_pages = active_virtual_memory->get_free_page_count();
                    info.kernel_heap_used = mem::MemAlloc::get_kernel_heap_used();
                    info.ticks = thread_tick_count();
                    info.switch_count = thread_switch_count();
                    info.logical_cpu_count = 1;
                    uint32_t eax = 0, ebx = 0, ecx = 0, edx = 0;
                    read_cpuid(0, 0, eax, ebx, ecx, edx);
                    const uint32_t maximum_leaf = eax;
                    const uint32_t vendor_words[3] = {ebx, edx, ecx};
                    auto *vendor = reinterpret_cast<uint8_t *>(info.cpu_vendor);
                    for (uint32_t word = 0; word < 3; ++word)
                        for (uint32_t byte = 0; byte < 4; ++byte)
                            vendor[word * 4 + byte] =
                                static_cast<uint8_t>(vendor_words[word] >> (byte * 8));
                    info.cpu_vendor[12] = '\0';
                    if (maximum_leaf >= 1)
                    {
                        read_cpuid(1, 0, eax, ebx, ecx, edx);
                        const uint32_t base_family = (eax >> 8) & 0xF;
                        const uint32_t base_model = (eax >> 4) & 0xF;
                        const uint32_t ext_family = (eax >> 20) & 0xFF;
                        const uint32_t ext_model = (eax >> 16) & 0xF;
                        info.cpu_family = base_family == 0xF
                            ? base_family + ext_family : base_family;
                        info.cpu_model = (base_family == 0x6 || base_family == 0xF)
                            ? (ext_model << 4) | base_model : base_model;
                    }
                    uint32_t extended_max = 0;
                    read_cpuid(0x80000000U, 0, extended_max, ebx, ecx, edx);
                    if (extended_max >= 0x80000004U)
                    {
                        auto *brand = reinterpret_cast<uint8_t *>(info.cpu_brand);
                        for (uint32_t leaf = 0; leaf < 3; ++leaf)
                        {
                            uint32_t words[4]{};
                            read_cpuid(0x80000002U + leaf, 0,
                                       words[0], words[1], words[2], words[3]);
                            for (uint32_t word = 0; word < 4; ++word)
                                for (uint32_t byte = 0; byte < 4; ++byte)
                                    brand[leaf * 16 + word * 4 + byte] =
                                        static_cast<uint8_t>(words[word] >> (byte * 8));
                        }
                        info.cpu_brand[48] = '\0';
                    }
                    auto *source = reinterpret_cast<const uint8_t *>(&info);
                    auto *destination = reinterpret_cast<uint8_t *>(registers->ebx);
                    for (uint32_t i = 0; i < sizeof(info); ++i)
                        destination[i] = source[i];
                    registers->eax = 0;
                    return;
                }

                case user::syscall_get_heap_usage:
                    registers->eax = mem::MemAlloc::get_kernel_heap_used();
                    return;

                case user::syscall_heap_alloc:
                    registers->eax = static_cast<uint32_t>(user_mode_heap_alloc(
                        registers->ebx, registers->ecx));
                    return;

                case user::syscall_heap_free:
                    registers->eax = static_cast<uint32_t>(user_mode_heap_free(registers->ebx));
                    return;

                case user::syscall_open:
                    registers->eax = static_cast<uint32_t>(open_file(registers));
                    return;

                case user::syscall_close:
                {
                    if (registers->ebx < 3 || registers->ebx >= 3 + MAX_OPEN_FILES ||
                        !open_files[registers->ebx - 3].used)
                    {
                        registers->eax = static_cast<uint32_t>(ERROR_BAD_DESCRIPTOR);
                        return;
                    }
                    open_files[registers->ebx - 3] = {};
                    registers->eax = 0;
                    return;
                }

                case user::syscall_readdir:
                    registers->eax = static_cast<uint32_t>(read_directory(registers));
                    return;

                case user::syscall_get_root:
                    registers->eax = fs::pckfs_is_mounted()
                        ? fs::pckfs_root_inode()
                        : static_cast<uint32_t>(ERROR_NO_DEVICE);
                    return;

                case user::syscall_wait_event:
                    registers->eax = static_cast<uint32_t>(wait_for_input(registers->ebx, registers->ecx));
                    return;

                case user::syscall_gfx_clear:
                    if (active_framebuffer == nullptr)
                        registers->eax = static_cast<uint32_t>(ERROR_NO_DEVICE);
                    else
                    {
                        hide_graphics_cursor();
                        active_framebuffer->clear(registers->ebx);
                        paint_graphics_cursor();
                        registers->eax = 0;
                    }
                    return;

                case user::syscall_gfx_rect:
                    registers->eax = static_cast<uint32_t>(draw_rectangle(registers));
                    return;

                case user::syscall_gfx_text:
                    registers->eax = static_cast<uint32_t>(draw_text(registers));
                    return;

                case user::syscall_get_screen_info:
                    registers->eax = static_cast<uint32_t>(get_graphics_info(
                        registers->ebx));
                    return;

                case user::syscall_gfx_cursor:
                    if (active_framebuffer == nullptr)
                        registers->eax = static_cast<uint32_t>(ERROR_NO_DEVICE);
                    else
                    {
                        set_graphics_cursor(registers->ebx, registers->ecx,
                                            registers->edx != 0);
                        registers->eax = 0;
                    }
                    return;

                case user::syscall_gfx_get_cursor:
                    if (!active_virtual_memory->is_user_range(registers->ebx,
                            sizeof(int32_t), true) ||
                        !active_virtual_memory->is_user_range(registers->ecx,
                            sizeof(int32_t), true))
                    {
                        registers->eax = static_cast<uint32_t>(ERROR_BAD_ADDRESS);
                        return;
                    }
                    *reinterpret_cast<int32_t *>(registers->ebx) =
                        static_cast<int32_t>(cursor_x);
                    *reinterpret_cast<int32_t *>(registers->ecx) =
                        static_cast<int32_t>(cursor_y);
                    registers->eax = 0;
                    return;

                case user::syscall_gfx_register_window:
                    registers->eax = active_framebuffer != nullptr &&
                        active_framebuffer->register_window(thread_current_process_id(),
                            registers->ebx, registers->ecx,
                            registers->edx, registers->esi)
                        ? 0 : static_cast<uint32_t>(ERROR_NO_MEMORY);
                    return;

                case user::syscall_gfx_move_window:
                    registers->eax = active_framebuffer != nullptr &&
                        active_framebuffer->frame_active() &&
                        active_framebuffer->move_window(thread_current_process_id(),
                            registers->ebx, registers->ecx)
                        ? 0 : static_cast<uint32_t>(ERROR_INVALID_ARGUMENT);
                    return;

                case user::syscall_gfx_bitmap:
                    registers->eax = static_cast<uint32_t>(draw_user_bitmap(registers));
                    return;

                case user::syscall_gfx_begin_frame:
                    if (active_framebuffer == nullptr)
                        registers->eax = static_cast<uint32_t>(ERROR_NO_DEVICE);
                    else if (active_framebuffer->frame_active())
                        registers->eax = static_cast<uint32_t>(ERROR_INVALID_ARGUMENT);
                    else
                    {
                        reset_graphics_clip();
                        registers->eax = active_framebuffer->begin_frame() ? 0 :
                            static_cast<uint32_t>(ERROR_NO_MEMORY);
                    }
                    return;

                case user::syscall_gfx_end_frame:
                    if (active_framebuffer == nullptr)
                        registers->eax = static_cast<uint32_t>(ERROR_NO_DEVICE);
                    else if (!active_framebuffer->frame_active())
                        registers->eax = static_cast<uint32_t>(ERROR_INVALID_ARGUMENT);
                    else
                    {
                        active_framebuffer->end_frame();
                        reset_graphics_clip();
                        // Katmanı çizim çağrıları arasında silmeden yeniden uygula.
                        paint_graphics_cursor();
                        registers->eax = 0;
                    }
                    return;

                case user::syscall_gfx_push_clip:
                    registers->eax = static_cast<uint32_t>(push_graphics_clip(
                        registers->ebx, registers->ecx, registers->edx, registers->esi));
                    return;

                case user::syscall_gfx_pop_clip:
                    registers->eax = static_cast<uint32_t>(pop_graphics_clip());
                    return;

                case user::syscall_get_kernel_log:
                {
                    const uint32_t capacity = registers->ecx;
                    if (capacity == 0 || capacity > MAX_SYSCALL_TRANSFER)
                    {
                        registers->eax = static_cast<uint32_t>(ERROR_INVALID_ARGUMENT);
                        return;
                    }
                    if (!active_virtual_memory->is_user_range(registers->ebx,
                            capacity, true))
                    {
                        registers->eax = static_cast<uint32_t>(ERROR_BAD_ADDRESS);
                        return;
                    }
                    registers->eax = static_cast<uint32_t>(lib::kernel_log_snapshot(
                        reinterpret_cast<char *>(registers->ebx), capacity));
                    return;
                }

                default:
                    registers->eax = static_cast<uint32_t>(ERROR_NOT_IMPLEMENTED);
                    return;
            }
        }
    }

    void syscall_initialize(mem::Vmm &virtual_memory,
                            device::FrameBuffer *framebuffer)
    {
        active_virtual_memory = &virtual_memory;
        active_framebuffer = framebuffer;
        syscall_reset_process();
        interrupt_register(0x80, dispatch_syscall);
    }

    bool syscall_graphics_frame_active()
    {
        return active_framebuffer != nullptr && active_framebuffer->frame_active();
    }

    void syscall_window_set_visible(uint32_t process_id, bool visible)
    {
        if (active_framebuffer == nullptr) return;
        hide_graphics_cursor();
        (void)active_framebuffer->set_window_visible(process_id, visible);
        paint_graphics_cursor();
    }

    void syscall_window_raise(uint32_t process_id)
    {
        if (active_framebuffer == nullptr) return;
        hide_graphics_cursor();
        (void)active_framebuffer->raise_window(process_id);
        paint_graphics_cursor();
    }

    void syscall_window_remove(uint32_t process_id)
    {
        if (active_framebuffer == nullptr) return;
        hide_graphics_cursor();
        if (active_framebuffer->frame_active())
            active_framebuffer->abort_frame();
        active_framebuffer->unregister_window(process_id);
        paint_graphics_cursor();
    }

    uint32_t syscall_window_at(uint32_t x, uint32_t y)
    {
        return active_framebuffer != nullptr
            ? active_framebuffer->window_at(x, y) : 1;
    }

    void syscall_reset_process()
    {
        if (active_framebuffer != nullptr && active_framebuffer->frame_active())
            active_framebuffer->end_frame();
        set_graphics_cursor(0, 0, false);
        reset_graphics_clip();
        for (uint32_t i = 0; i < MAX_OPEN_FILES; ++i)
            open_files[i] = {};
    }
}
