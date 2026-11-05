// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#ifndef PCKOS_USER_GRAPHICS_HH
#define PCKOS_USER_GRAPHICS_HH

#include <abi/graphics.hh>
#include <user/syscall.hh>

namespace user
{
    using abi::GraphicsInfo;

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
}

#endif
