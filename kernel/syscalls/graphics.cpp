// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include "internal.hh"
#include <abi/graphics.hh>
#include <device/video/frame_buffer.hh>
#include <kernel/thread.hh>
#include <kernel/graphics.hh>

namespace kernel::syscalls
{
    namespace
    {
        constexpr uint32_t MAX_GFX_TEXT = 512;
        constexpr uint32_t MAX_GFX_RECT_PIXELS = 1024 * 1024;
        constexpr uint32_t CURSOR_WIDTH = 12;
        constexpr uint32_t CURSOR_HEIGHT = 16;
        device::FrameBuffer *active_framebuffer = nullptr;
        uint32_t cursor_x = 0;
        uint32_t cursor_y = 0;
        bool cursor_visible = false;
        bool cursor_drawn = false;
        struct ClipRect { uint32_t x, y, right, bottom; };
        constexpr uint32_t MAX_CLIP_DEPTH = 16;
        ClipRect clip_stack[MAX_CLIP_DEPTH + 1]{};
        uint32_t clip_depth = 0;
        uint32_t clip_overflow = 0;
    }

    static void reset_graphics_clip()
    {
        clip_depth = 0;
        clip_overflow = 0;
        if (active_framebuffer == nullptr) return;
        clip_stack[0] = {0, 0, active_framebuffer->get_width(),
                              active_framebuffer->get_height()};
        active_framebuffer->reset_clip();
    }

    static int32_t push_graphics_clip(uint32_t x, uint32_t y,
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

    static int32_t pop_graphics_clip()
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

    static void hide_graphics_cursor()
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

    static void paint_graphics_cursor()
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

    static void set_graphics_cursor(uint32_t x, uint32_t y, bool visible)
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

    static int32_t get_graphics_info(uint32_t user_info)
    {
        if (active_framebuffer == nullptr)
            return ERROR_NO_DEVICE;
        if (!active_virtual_memory->is_user_range(user_info,
                sizeof(abi::GraphicsInfo), true))
            return ERROR_BAD_ADDRESS;

        const abi::GraphicsInfo info{
            active_framebuffer->get_width(),
            active_framebuffer->get_height()
        };
        const auto *source = reinterpret_cast<const uint8_t *>(&info);
        auto *destination = reinterpret_cast<uint8_t *>(user_info);
        for (uint32_t i = 0; i < sizeof(info); ++i)
            destination[i] = source[i];
        return 0;
    }

    static int32_t draw_rectangle(const Registers *registers)
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

    static int32_t draw_text(const Registers *registers)
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

    static int32_t draw_user_bitmap(const Registers *registers)
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

    void initialize_graphics(device::FrameBuffer *framebuffer)
    {
        active_framebuffer = framebuffer;
    }

    void reset_graphics()
    {
        if (active_framebuffer != nullptr && active_framebuffer->frame_active())
            active_framebuffer->end_frame();
        set_graphics_cursor(0, 0, false);
        reset_graphics_clip();
    }

    uint32_t graphics_mouse_target(int32_t delta_x, int32_t delta_y)
    {
        if (active_framebuffer == nullptr)
            return thread_current_process_id();
        int32_t x = static_cast<int32_t>(cursor_x) + delta_x;
        int32_t y = static_cast<int32_t>(cursor_y) - delta_y;
        if (x < 0) x = 0;
        if (y < 0) y = 0;
        if (x >= static_cast<int32_t>(active_framebuffer->get_width()))
            x = static_cast<int32_t>(active_framebuffer->get_width()) - 1;
        if (y >= static_cast<int32_t>(active_framebuffer->get_height()))
            y = static_cast<int32_t>(active_framebuffer->get_height()) - 1;
        return syscall_window_at(static_cast<uint32_t>(x), static_cast<uint32_t>(y));
    }

    void handle_graphics(Registers *registers)
    {
        if (active_framebuffer != nullptr)
            active_framebuffer->select_process(thread_current_process_id());

        switch (registers->eax)
        {
            case abi::syscall_gfx_clear:
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

            case abi::syscall_gfx_rect:
                registers->eax = static_cast<uint32_t>(draw_rectangle(registers));
                return;

            case abi::syscall_gfx_text:
                registers->eax = static_cast<uint32_t>(draw_text(registers));
                return;

            case abi::syscall_get_screen_info:
                registers->eax = static_cast<uint32_t>(get_graphics_info(
                    registers->ebx));
                return;

            case abi::syscall_gfx_cursor:
                if (active_framebuffer == nullptr)
                    registers->eax = static_cast<uint32_t>(ERROR_NO_DEVICE);
                else
                {
                    set_graphics_cursor(registers->ebx, registers->ecx,
                                        registers->edx != 0);
                    registers->eax = 0;
                }
                return;

            case abi::syscall_gfx_bitmap:
                registers->eax = static_cast<uint32_t>(draw_user_bitmap(registers));
                return;

            case abi::syscall_gfx_begin_frame:
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

            case abi::syscall_gfx_end_frame:
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

            case abi::syscall_gfx_push_clip:
                registers->eax = static_cast<uint32_t>(push_graphics_clip(
                    registers->ebx, registers->ecx, registers->edx, registers->esi));
                return;

            case abi::syscall_gfx_pop_clip:
                registers->eax = static_cast<uint32_t>(pop_graphics_clip());
                return;

            case abi::syscall_gfx_get_cursor:
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

            case abi::syscall_gfx_register_window:
                registers->eax = active_framebuffer != nullptr &&
                    active_framebuffer->register_window(thread_current_process_id(),
                        registers->ebx, registers->ecx,
                        registers->edx, registers->esi)
                    ? 0 : static_cast<uint32_t>(ERROR_NO_MEMORY);
                if (registers->eax == 0)
                    process_set_input_focus(thread_current_process_id());
                return;

            case abi::syscall_gfx_move_window:
                registers->eax = active_framebuffer != nullptr &&
                    active_framebuffer->frame_active() &&
                    active_framebuffer->move_window(thread_current_process_id(),
                        registers->ebx, registers->ecx)
                    ? 0 : static_cast<uint32_t>(ERROR_INVALID_ARGUMENT);
                return;

            default:
                registers->eax = static_cast<uint32_t>(ERROR_NOT_IMPLEMENTED);
                return;
        }
    }
}

namespace kernel
{
    bool syscall_graphics_frame_active()
    {
        return syscalls::active_framebuffer != nullptr && syscalls::active_framebuffer->frame_active();
    }

    void syscall_window_set_visible(uint32_t process_id, bool visible)
    {
        if (syscalls::active_framebuffer == nullptr) return;
        syscalls::hide_graphics_cursor();
        (void)syscalls::active_framebuffer->set_window_visible(process_id, visible);
        syscalls::paint_graphics_cursor();
    }

    void syscall_window_raise(uint32_t process_id)
    {
        if (syscalls::active_framebuffer == nullptr) return;
        syscalls::hide_graphics_cursor();
        (void)syscalls::active_framebuffer->raise_window(process_id);
        syscalls::paint_graphics_cursor();
    }

    void syscall_window_remove(uint32_t process_id)
    {
        if (syscalls::active_framebuffer == nullptr) return;
        syscalls::hide_graphics_cursor();
        if (syscalls::active_framebuffer->frame_active())
            syscalls::active_framebuffer->abort_frame();
        syscalls::active_framebuffer->unregister_window(process_id);
        syscalls::paint_graphics_cursor();
    }

    uint32_t syscall_window_at(uint32_t x, uint32_t y)
    {
        return syscalls::active_framebuffer != nullptr
            ? syscalls::active_framebuffer->window_at(x, y) : 1;
    }

}
